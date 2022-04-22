#include "kernel/types.h"
#include <user/user.h>

void childSieve(int* pPipe);

int
main(int argc, char *argv[])
{
   int i;
   int p[2];
   int status;

   if (pipe(p) < 0) {
      printf("pipe failed.");
      exit(1);
   }

   if (fork() > 0) {
      close(p[0]);
      for (i = 2; i <= 35; i++) {
         write(p[1], &i, sizeof(i));
      }
      close(p[1]);
   } else {
      childSieve(p);
   }
   wait(&status);
   return 0;
}


void
childSieve(int* pPipe)
{
   int n;
   int prime;
   int cPipe[2];
   int childCreated = 0;
   int status;

   close(pPipe[1]);
   read(pPipe[0], &prime, sizeof(prime));
   printf("prime %d\n", prime);
   while (read(pPipe[0], &n, sizeof(n)) > 0) {
      if (n % prime != 0 ) {
         if (!childCreated) {
            if (pipe(cPipe) < 0) {
               printf("pipe failed.");
               exit(1);
            }
            if (fork() > 0) {
               childCreated = 1;
               close(cPipe[0]);
               write(cPipe[1], &n, sizeof(n));
            } else {
               childSieve(cPipe);
            }
         } else {
            write(cPipe[1], &n, sizeof(n));
         }
      }
   }
   if (childCreated) {
      close(cPipe[1]);
      wait(&status);
   }
   exit(0);
}