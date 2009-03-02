#CC = cc -g
#CC = cc -Ac -g
#CC = cc -Xc -g
#CC = acc -g
#CC = CC -g
CC = gcc
#CC = gcc -g -Wall -ansi -pedantic
#CC = gcc -g -Wall -W -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings -Waggregate-return -Wmissing-prototypes -Wstrict-prototypes
#CC = g++ -g -Wall

RM = rm -f

LEX = lex # could really be flex
#LEX = flex

#LEXLIB = -ll
LEXLIB = -lfl

all : xlockgen
	guibuild.pl

lex.yy.c : xlockgen.lex
	$(LEX) xlockgen.lex

xlockgen : lex.yy.c
	$(CC) lex.yy.c -o $@ $(LEXLIB)

clean :
	$(RM) core *~ *% *.bak *.orig *.rej make.log MakeOut *.patch

distclean : clean
	$(RM) xlockgen lex.yy.c
