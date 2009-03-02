Content-Type: text/plain; charset=us-ascii; name="xlockgen.lex"
/********************************************
* xlockgen
* author charles vidal
* to help adminitration and utils for launcher(s)
* this file replace token by all modes token 
* : LISTLIBSX, LISTMOTIF,LISTTCL
* utils :
* If you want all modes of xlock: 
* xlockgen -allmodes
* to see all mode : in bash 
for i in `./xlockgen -allmodes`
do 
xlock -mode $i 
done
********************************************/
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
void usage() {
	printf("xlockgen :\n");
	printf("	-allmodes\n");
	printf("or to be used in \n");
}

int main(int argc,char *argv[])
{
if (argc>1) {
		if (!strcmp("-allmodes",argv[1])) {
				int i;
				int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);
				for (i=0;i<numprocs;i++)
					printf("%s\n",LockProcs[i].cmdline_arg);
				exit(0);
			} 
		if (!strcmp("--help",argv[1]) || 
		!strcmp("-help",argv[1]) || 
		!strcmp("-?",argv[1]) || 
		!strcmp("-h",argv[1])) {
				usage();
				exit(0);
			}
	}
	
yylex();
}
