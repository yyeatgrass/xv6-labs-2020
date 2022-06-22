// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct kmem kmemlist[NCPU];

void
kinit()
{
  memset(kmemlist, 0, NCPU * sizeof(struct kmem));
  for (int i = 0; i< NCPU; i++) {
    initlock(&kmemlist[i].lock, "kmem");
  }

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  uint64 id = cpuid();
  acquire(&kmemlist[id].lock);
  r->next = kmemlist[id].freelist;
  kmemlist[id].freelist = r;
  release(&kmemlist[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();
  uint64 cpu = cpuid();
  acquire(&kmemlist[cpu].lock);
  if (kmemlist[cpu].freelist == 0) {
    for (int i = 0; i < NCPU; i++) {
      if (i == cpuid()) {
        continue;
      }
      acquire(&kmemlist[i].lock);
      if (kmemlist[i].freelist != 0 && kmemlist[i].freelist->next != 0) {
        struct run * left = kmemlist[i].freelist;
        struct run * right = kmemlist[i].freelist->next;
        while (right != 0 && right->next != 0) {
          left = left->next;
          right = right->next->next;
        }
        kmemlist[cpu].freelist = left->next;
        left->next = 0;
        release(&kmemlist[i].lock);
        break;
      }
      release(&kmemlist[i].lock);
    }
  }
  r = kmemlist[cpu].freelist;
  if (r) {
    kmemlist[cpu].freelist = r->next;
  }
  release(&kmemlist[cpu].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  pop_off();
  return (void*)r;
}
