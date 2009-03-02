/* xlockgen --- generates launcher include file */
/* @(#)xlockgen.lex     2.0 99/10/10 xlockmore"
/*
/* Copyright (c) Charles Vidal */

/*
 * to help adminitration and utils for launcher(s)
 * this file replace token by all modes token :
 * LISTMOTIF, LISTTCL, LISTGTK
 * utils :
 * If you want all modes of xlock:
 * xlockgen -allmodes
 * to see all mode : in bash
 * for i in `./xlockgen -allmodes`
 * do
 *   xlock -mode $i
 * done
 *
 * REVISION HISTORY:
 *       99/10/10: fixes from Eric Lassauge <lassauge@mail.dotcom.fr> :
 *                 - include "config.h"
 *                 - new lmode.h
 *                 - new token LISTGTK
 *                 - header
 *
 */

%{
#include <stdio.h>
#include <string.h>

/* #include "../../config.h" */
#include "lmode.h"

char *Begin="\"";
char *Sep="\",";
char *End="\"";
%}

%%
"$%LISTMOTIF" {
  int i;
  int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);

  for (i = 0; i < numprocs; i++) {
    if (LockProcs[i].define != NULL)
      printf("%s\n", LockProcs[i].define);
    if (i != numprocs - 1) {
      printf("{\"%s\",", LockProcs[i].cmdline_arg);
      printf("\"%s\"},\n", LockProcs[i].desc);
    } else {
      printf("{\"%s\",", LockProcs[i].cmdline_arg);
      printf("\"%s\"},\n", LockProcs[i].desc);
    }
    if (LockProcs[i].define != NULL)
      printf("#endif\n");
  }
}
"$%LISTGTK" {
  int i;
  int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);

  for (i = 0; i < numprocs; i++) {
    if (LockProcs[i].define != NULL)
      printf("%s\n", LockProcs[i].define);
    printf("\t{\"%s\",\n", LockProcs[i].cmdline_arg);
    printf("\t %d, %d, %d, %d, %2.2f,\n",
      LockProcs[i].def_delay,
      LockProcs[i].def_count,
      LockProcs[i].def_cycles,
      LockProcs[i].def_size,
      LockProcs[i].def_saturation);
    printf("\t \"%s\", (void *) NULL},\n", LockProcs[i].desc);
    if (LockProcs[i].define != NULL)
      printf("#endif\n");
  }
}
"$%LISTTCL" {
  int i;
  int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);

  for (i = 0; i < numprocs; i++) {
    printf("%s\\\n", LockProcs[i].cmdline_arg);
  }
}
"$%LISTJAVA" {
  int i;
  int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);

  for (i = 0; i < numprocs; i++) {
    printf("lst.addItem(\"%s\");\n", LockProcs[i].cmdline_arg);
  }
}
%%
void usage() {
  printf("xlockgen :\n");
  printf("\t-allmodes\n");
  printf("or to be used in \n");
}

int main(int argc, char *argv[])
{
  if (argc>1) {
    if (!strcmp("-allmodes", argv[1])) {
      int i;
      int numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);

      for (i = 0; i < numprocs; i++)
        printf("%s\n", LockProcs[i].cmdline_arg);
      exit(0);
    }
    if (!strcmp("--help", argv[1]) ||
        !strcmp("-help", argv[1]) ||
        !strcmp("-?", argv[1]) ||
        !strcmp("-h", argv[1])) {
      usage();
      exit(0);
    }
  }
  yylex();
}
