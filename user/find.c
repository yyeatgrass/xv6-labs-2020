#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  return ++p;
}

void
find(char* path, char* name)
{
   char buf[512], *p;
   int fd;
   struct dirent de;
   struct stat st;

   if (strcmp(fmtname(path), name) == 0) {
      printf("%s\n", path);
   }

   if ((fd = open(path, 0)) < 0) {
      printf("cannot open %s\n", path);
      return;
   }

   if (fstat(fd, &st) < 0) {;
      printf("cannot stat %s\n", path);
      close(fd);
      return;
   }

   if (st.type == T_FILE || st.type == T_DEVICE) {
      close(fd);
      return;
   }

   strcpy(buf, path);
   p = buf+strlen(buf);
   *p++ = '/';
   while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) {
         continue;
      }
      if (strcmp(de.name, ".") == 0 ||
          strcmp(de.name, "..") == 0) {
         continue;
      }
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, name);
   }
   close(fd);
   return;
}

int
main(int argc, char *argv[])
{
   find(argv[1], argv[2]);
   exit(0);
}


