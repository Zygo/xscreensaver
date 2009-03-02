# Generated automatically from Makefile.in by configure.
# $Id : Makefile.in 4.04 1997/07/10 $
#
# xlockmore Makefile.in for autoconf (UNIX/VMS X11 support)
############################################################

# 

srcdir = .
top_srcdir = ..

N=
O = .o
#O = .obj
C = .c
# C = .cc
S = $(N) $(N)
#S = ,


# please define
# C as the C source code extension
# O as the object extension
# S as the separator for object code

####################################################################
# List of object files
XALOCKOBJS = multireq$(O)$(S)xalock$(O)

# List of source files
XALOCKSRCS = $(VPATH)multireq$(C) $(VPATH)xalock$(C)

# default target
all : 

# this tells GNU make not to export variables into the environment
# But other makes do not understand its significance, so it must
# not be the first target in the file. So it is here, before
# any variables are created, but after the default target
.NOEXPORT :

SHELL = /bin/sh

srcdir = .
top_srcdir = ..
#UTILDIR = $(top_srcdir)/xlock/

prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
mandir = ${prefix}/man/man1
xapploaddir = /usr/X11R6/lib/X11/app-defaults

CONFIGDIR = $(srcdir)/config/

INSTALL = /usr/bin/ginstall -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
INSTPGMFLAGS = -s -o root -g bin -m 4111

#CC = cc -g
#CC = cc -Ac -g
#CC = cc -Xc -g
#CC = acc -g
#CC = CC -g
#CC = gcc -g -Wall -ansi -pedantic
#CC = gcc -g -Wall -W -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings -Waggregate-return -Wmissing-prototypes -Wstrict-prototypes
#CC = g++ -g -Wall
CC = gcc

LINT = lint
#LINT = alint

#DEPEND = makedepend
DEPEND = makedepend
DEPEND_FLAGS = 
DEPEND_DEFINES = 
 
LN_S = ln -s
RM = rm -f
RM_S = $(RM)
ECHO = echo

#BLN_S = set file/enter=[]
#RM = delete/noconfirm/nolog
#RM_S = set file/remove/nolog
#ECHO = write sys$output

DEFS = -DHAVE_CONFIG_H $(DEFINES)
XALOCKINC = -I/usr/X11R6/include  -I/usr/X11R6/include -I/usr/X11R6/include -I.. -I$(top_srcdir)
# -I$(UTILDIR)
CFLAGS = -g -O2
#CFLAGS = -O
#CFLAGS = -g
XALOCKLDFLAGS = 
XLIBS = /usr/X11R6/lib
XALOCKLIBS = -L/usr/X11R6/lib  -L/usr/X11R6/lib -L/usr/X11R6/lib -lXmu -lXaw -lXt  -lX11
#If you have purify, and want to use it, uncomment this definition or
# run the make as `make PURIFY=purify'
# or run configure with the --with-purify argument.
PURIFY = 

xalock : $(XALOCKOBJS)
	$(PURIFY) $(CC) -o $@ $(XALOCKOBJS) $(XALOCKLDFLAGS) $(XALOCKLIBS)
	@ $(ECHO) "$@ BUILD COMPLETE"
	@ $(ECHO) ""

.SUFFIXES : $(C) $(O)

$(C)$(O) :
	$(CC) -c $(CPPFLAGS) $(DEFS) $(XALOCKINC) $(CFLAGS) $<

install : 

install_xalock : install-program install-man install-ad
	@ $(ECHO) "$@ COMPLETE"
	@ $(ECHO) ""

install-program : xalock
	$(top_srcdir)/mkinstalldirs $(bindir)
	$(INSTALL_PROGRAM) xalock $(bindir)

install-man :

install-ad :

uninstall : 

uninstall_xalock : uninstall-program uninstall-man uninstall-ad

uninstall-program :
	$(RM) $(bindir)/xalock

uninstall-man :

uninstall-ad :

lint :
	$(LINT) -ax -DLINT $(DEFINES) $(XALOCKINC) $(XALOCKSRCS)

xrdb :

man :

html :

hlp :

clean :
	$(RM) *.o core *~ *% *.bak *.orig *.rej make.log MakeOut xalock *.patch

distclean : clean
	$(RM) Makefile

clean.all : distclean

# Adds all current dependencies to Makefile
depend :
	$(DEPEND) -s '# DO NOT DELETE: updated by make depend'              \
	$(DEPEND_FLAGS) --                                                  \
	$(XALOCKINC) $(DEFS) $(DEPEND_DEFINES) $(CFLAGS) --                 \
	$(XALOCKSRCS)

# Adds some dependencies to Makefile.in -- not totally accurate, but pretty
# close.  This excludes dependencies on files in /usr/include, etc.  It tries
# to include only dependencies on files which are themselves a part of this
# package. -w 0
distdepend :
	@echo updating dependencies in `pwd`/Makefile.in... ;                \
	$(DEPEND) -w 0 -f -                                                 \
	-s '# DO NOT DELETE: updated by make distdepend' $(DEPEND_FLAGS) -- \
	$(XALOCKINC) $(DEFS) $(DEPEND_DEFINES) $(CFLAGS) --                 \
	$(XALOCKSRCS) |                                                     \
	(                                                                   \
	  awk '/^#.*Id : Makefile.in/,/^# DO .*distdepend/' < Makefile.in ; \
	  sed -e 's@ \./@ @g;s@ /[^ ]*@@g;/^.*:$$/d' ;                      \
	  echo ''                                                           \
	) > /tmp/distdepend.$$$$ &&                                         \
	mv Makefile.in Makefile.in.bak &&                                   \
	mv /tmp/distdepend.$$$$ Makefile.in

##############################################################################
#
# DO NOT DELETE: updated by make distdepend

multireq.o: multireq.h
xalock.o: ../config.h
xalock.o: multireq.h
xalock.o: xalock.h
xalock.o: modes.h

