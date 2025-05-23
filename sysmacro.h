#include <stdio.h>  /* serve per la perror */
#include <stdlib.h> /* serve per la exit */
#include <string.h> /* serve per strlen */
#include <errno.h>  /* serve per errno */
#include <unistd.h> /* serve per la write */

/*
   if COND
   MSG is printed with errno ouput and exits
*/
#define EXIT_ERROR(cond, msg) \
   errno = 0;                 \
   if ((cond) && errno != 0)  \
   {                          \
      perror(msg);            \
      exit(EXIT_FAILURE);     \
   }

/*
   if (CMD) == -1
   MSG is printed with errno ouput and exits
*/
#define IFERROR(cmd, msg) EXIT_ERROR((cmd) == -1, msg);