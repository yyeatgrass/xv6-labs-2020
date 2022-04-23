#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"


int
parseNextLine(char **plines, char *line, int maxLen)
{

   char *start = *plines;
   if (*start == '\0') {
      return 0;
   }

   char *p = start;
   while (*p != '\n' && *p != '\0') {
      p += 1;
   }

   memcpy(line, start, p - start);
   line[p - start] = '\0';
   *plines = p + 1;

   return p - start;
}


int
main(int argc, char *argv[])
{
   char *prog = argv[1];
   char *xArgv[MAXARG];
   char lines[512];
   char line[128];
   char *lp = lines;

   for (int i = 0; i < argc - 1; i++) {
      xArgv[i] = argv[i + 1];
   }
   read(0, lines, sizeof(lines));
   while (parseNextLine(&lp, line, 512) > 0) {
      if (fork() == 0) {
         xArgv[argc - 1] = line;
         exec(prog, xArgv);
      }
      wait(0);
   }
   exit(0);
}
