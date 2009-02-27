# @(#)Makefile	1.9 91/09/27 XLOCK
# Makefile for xlock on a Sun.

C_SRCS=xlock.c hsbramp.c usleep.c resource.c \
	hopalong.c qix.c life.c image.c blank.c \
	swarm.c rotor.c pyro.c flame.c worm.c
HDRS=xlock.h
IMAGES=lifeicon.bit sunlogo.bit
SRCS=${C_SRCS} ${HDRS} ${IMAGES} xlock.man XLock.ad
OBJS=${C_SRCS:.c=.o}
COPT=-O
OWDIR=/usr/openwin
CFLAGS=${COPT} -I${OWDIR}/include
LDFLAGS=-L${OWDIR}/lib
LIBS=${LDFLAGS} -lX11 -lm

xlock: ${OBJS}
	cc -o $@ ${OBJS} ${LIBS}

debug: xlock
debug:=COPT=-g -DDEBUG

resource.o:=DEFINES=-DDEF_FILESEARCHPATH=\"$(LIBDIR)/%T/%N%S\"
pyro.o:=DEFINES=-DSTARSIZE=2

tar:
	tar cvf xlock.tar ${SRCS} Makefile Makefile.imake Imakefile README

.KEEP_STATE:
