#include "kernel/types.h"
#include <user/user.h>

int
main(int argc, char *argv[])
{
   char data[20];
   int p[2];

   if (pipe(p) < 0) {
      printf("pipe failed.");
      exit(1);
   }

   if (fork() == 0) {
      read(p[0], data, sizeof(data));
      printf("%d: received %s\n", getpid(), data);
      write(p[1], "pong", 5);
      exit(0);
   } else {
      write(p[1], "ping", 5);
      wait(0);
      // read here is not blocking.
      read(p[0], data, sizeof(data));
      printf("%d: received %s\n", getpid(), data);
      exit(0);
   }
}