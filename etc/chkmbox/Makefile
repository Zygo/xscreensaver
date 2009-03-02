#
# Makefile for chkmbox
#

CC=	gcc
LN=	gcc

CFLAGS=	-c -O2 -Wall

XMBOX_OBJS=	chkmbox.o imapsocket.o

all:	chkmbox

.c.o:
	$(CC) $(CFLAGS) $<

chkmbox.o:	imapsocket.h

chkmbox:	$(XMBOX_OBJS)
	$(LN) -o $@ $(XMBOX_OBJS)

