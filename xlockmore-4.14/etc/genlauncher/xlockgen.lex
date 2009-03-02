%{
#include <stdio.h>
#include <string.h>
#include "lmode.h"
char *Begin="\"";
char *Sep="\",";
char *End="\"";
%}

%%
"$%LISTLIBSX"	{int i;
		int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);
		for (i=0;i<numprocs;i++)
			{if (LockProcs[i].define!=NULL) printf("%s\n",LockProcs[i].define);
			if (i!=numprocs-1) 
			printf("%s%s%s\n",Begin,LockProcs[i].cmdline_arg,Sep);
			else
			printf("%s%s%s\n",Begin,LockProcs[i].cmdline_arg,Sep);
			/*printf("%s%s%s\n",Begin,LockProcs[i].cmdline_arg,End);*/
			if (LockProcs[i].define!=NULL) printf("#endif\n");
			}
		}
"$%LISTMOTIF"	{int i;
		int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);
		for (i=0;i<numprocs;i++)
			{if (LockProcs[i].define!=NULL) printf("%s\n",LockProcs[i].define);
			if (i!=numprocs-1)
			{
        		printf("{\"%s\",",LockProcs[i].cmdline_arg);
        		printf("\"%s\"},\n",LockProcs[i].desc);
			}
			else
			{
        		printf("{\"%s\",",LockProcs[i].cmdline_arg);
        		printf("\"%s\"},\n",LockProcs[i].desc);
			}
			if (LockProcs[i].define!=NULL) printf("#endif\n");
		 }
		}
"$%LISTTCL"	{
	int i;
		int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);
		for (i=0;i<numprocs;i++)
			{if (i!=numprocs-1)
			printf("%s\\\n",LockProcs[i].cmdline_arg);
			else
			printf("%s\n",LockProcs[i].cmdline_arg);
			}
	}
"$%LISTJAVA" {
		int i;
		int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);
		for (i=0;i<numprocs;i++)
			{
			printf("lst.addItem(\"%s\");\n",LockProcs[i].cmdline_arg);
			}
		}
%%
main(int argc,char *argv[])
{
yylex();
}

