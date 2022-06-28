// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

struct bucket {
  struct spinlock lock;
  struct buf head;
};

#define NUMOFBACHEBUCKETS 13
struct bucket bufbuckets[NUMOFBACHEBUCKETS];

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  memset(bufbuckets, 0, NUMOFBACHEBUCKETS*sizeof(struct bucket));
  for (int i = 0; i < NUMOFBACHEBUCKETS; i++) {
    initlock(&bufbuckets[i].lock, "bcachebucket");
    bufbuckets[i].head.next = &bufbuckets[i].head;
    bufbuckets[i].head.prev = &bufbuckets[i].head;
  }

  int i = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++, i++) {
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint64 bucketno = blockno % NUMOFBACHEBUCKETS;
  struct bucket *buc = &bufbuckets[bucketno];
  acquire(&buc->lock);

  for (b = buc->head.next; b != &buc->head; b = b->next) {
    // printf("head %p, b %p \n", &buc->head, b);
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&buc->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      buc->head.next->prev = b;
      b->next = buc->head.next;
      buc->head.next = b;
      b->prev = &buc->head;
      release(&bcache.lock);
      release(&buc->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->next->prev = b->prev;
    b->prev->next = b->next;
    acquire(&tickslock);
    b->lastusedtime = ticks;
    release(&tickslock);
  }
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


