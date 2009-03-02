/*****************************************************************************
*  (c) Copyright 1996,1997 Metapath Software Corporation
*
*  Permission to use, copy, modify, and distribute this software and its
*  documentation for any purpose and without fee is hereby granted,
*  provided that the above copyright notice appear in all copies and that
*  both that copyright notice and this permission notice appear in
*  supporting documentation.
*
*  This file is provided AS IS with no warranties of any kind.  The author
*  shall have no liability with respect to the infringement of copyrights,
*  trade secrets or any patents by this file or any part thereof.  In no
*  event will the author be liable for any lost revenue or profits or
*  other special, indirect and consequential damages.
*
*  Author:  David A. Hansen
*  Created: 28-MAR-1996
*
*  Change History:
*  22-JUL-96   D.Hansen    Fixed some bugs and added length check.
*  01-AUG-96   D.Hansen    Added Usage dump on SIGHUP
*  17-JUL-97   D.Hansen    Removed dependencies for use with xlock
*
*  Description:
*     This module replaces the standard malloc/free routines with more
*     enhanced/robust version to aid in catching memory bugs.
*
*****************************************************************************/

/*-
   It's still a little crude, but it works.  So the next thing I need to do is
   add some more refinement.  First on my list is to figure out a way to
   translate the caller's address into something useful, like a symbol.
   Currently you have to be in gdb and use something like:

   gdb> x <addr>

   Oh, and don't forget to link with debugging, LDFLAGS=-g

   To build, I just put memcheck.c in the xlock subdirectory and hand edited

   the Makefile and ../mode/Makefile to include it.  You'll probably want to
   create a debug subdirectory and figure out a way to eloquently get
   configure to build with it.

   Also, there is no comparison/growth detection utility.  Maybe that's
   something you can add.  Basically, you send HUP signals to the process
   every so often and it will dump the memory users and amount they have
   allocated.  After running and HUPing for a while, a script could analyze
   the output and determine which caller addresses are continuing to consume
   memory.

   Also, the method for determining the caller's address is probably specific
   to Intel machines since I use a trick that is based on the way the frame is
   stacked.  It may work on Sun with a little playing around with the
   reference variable, like the last variable in the parameter list instead of
   the first, or maybe by changing the reference to add 1 long word instead of
   subtract 1 long word.  If you look at the variable caller_addr, you'll see
   what I'm talking about.  It all depends on things like whether the stack
   grows up or down, etc.  I would try it first without changing anything and
   use gdb or dbx or whatever to see if the addresses translate into
   appropriate symbols.  In any case, it will always be very machine
   dependent.  Not much one can do about that.

   Finally, what we probably want to do in the long run is create a script
   that runs xlock randomly through all the modes on a given interval and at
   the same interval issues SIGHUPs to the process to take a snapshot of the
   used memory.  Then after letting it iterate through all the modes several
   times, another script can post-process the output looking for memory growth
   and bad memory users.  (gdb> handle SIGHUP print pass nostop) (ifndebug
   around the SIGHUP handler in xlock.c).

   I've also thought about adding another signal catcher to snapshot a stable
   allocation.  In other words, once xlock is started, issue a SIGUSR1 or
   something, and then all subsequent SIGHUPs would print deviations from the
   initial SIGUSR1.  Of course, that would produce a different list for every
   mode, but it would delete the common mallocs done at process
   initialization.  Sometimes it's also hard to tell if a library is doing a
   one time permanent malloc and just reusing it later.  I think MesaGL does
   this. */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#ifdef ULONG
#undef ULONG
#endif
#if LINT			/* Lint complains so give it what it expects */
#define ULONG unsigned int
#else
#define ULONG unsigned long
#endif

extern char *ProgramName;
extern pid_t ProgramPID;
extern void *sbrk(int incr);

typedef struct mem_struct {
	struct mem_hdr {
		ULONG       check_mark;
		ULONG       chunk_size;
		ULONG       used_length;
		struct mem_struct *next;
		void       *caller;
	} hdr;

	/* allocate space for the marker at the end of the data */
	/* but return the address of data */
	unsigned char data[2];

} mem_type;


#define USED_MARKER   ((ULONG) 0xBABECAFE)
#define FREE_MARKER   ((ULONG) 0xDEADBEEF)
#define HEAD_MARKER   ((ULONG) 0xFACEF00D)
#define EOD_MARKER    ((ULONG) 0xEC)

#define SBRK_MIN      4096
#define SPLIT_MIN     32

static mem_type *free_head = NULL;
static mem_type *malloc_head = NULL;
static int  reentrancy_check = 0;
static int  first_time = 1;
static FILE *dump_file;
static char dump_fname[256];

static struct sigaction hup_action;
static struct sigaction old_action;

static ULONG total_count;
static ULONG total_size;
static ULONG total_chunk;
static ULONG caller_count;
static ULONG caller_size;
static ULONG caller_chunk;
static time_t hup_time;
static char time_str[256];

/*-------------------------------------------------------------------------*/
static void
message(char *msgstr)
{
	if (dump_file == NULL)
		return;
	(void) time(&hup_time);
	(void) strftime(time_str, sizeof (time_str), "%d-%b-%y %H:%M:%S",
			localtime(&hup_time));
	(void) fprintf(dump_file, "%s - %s (%d): %s\n",
		       time_str, ProgramName, (int) ProgramPID, msgstr);
	(void) fflush(dump_file);
}				/* message */


/*-------------------------------------------------------------------------*/
static void
hup_handler(int interrupt)
{
	mem_type   *t1;
	mem_type   *t2;

	if (dump_file == NULL)
		return;
	if (reentrancy_check > 1) {
		message("Memory allocation list is currently unaccessible");
		return;
	}
	message("malloc/free usage dump:");

	(void) fprintf(dump_file,
		       "Dumping on interrupt %d.\n", interrupt);
	(void) fprintf(dump_file,
		       "=================================================\n");
	(void) fprintf(dump_file,
		       "Caller      |   Number  |    Size   |    Heap   |\n");
	(void) fprintf(dump_file,
		       "------------|-----------|-----------|-----------|\n");
	total_count = 0;
	total_size = 0;
	total_chunk = 0;
	for (t1 = malloc_head; t1; t1 = t1->hdr.next) {
		total_count++;
		total_size += t1->hdr.used_length;
		total_chunk += t1->hdr.chunk_size;
		if (t1->hdr.check_mark == HEAD_MARKER) {
			t1->hdr.check_mark = USED_MARKER;
			continue;
		}
		caller_count = 1;
		caller_size = t1->hdr.used_length;
		caller_chunk = t1->hdr.chunk_size;
		for (t2 = t1->hdr.next; t2; t2 = t2->hdr.next) {
			if (t2->hdr.caller == t1->hdr.caller) {
				t2->hdr.check_mark = HEAD_MARKER;
				caller_count++;
				caller_size += t2->hdr.used_length;
				caller_chunk += t2->hdr.chunk_size;
			}
		}
#ifdef LINT
		(void) fprintf(dump_file, "0x%08X: |%10u |%10u |%10u |\n",
#else
		(void) fprintf(dump_file, "0x%08lX: |%10lu |%10lu |%10lu |\n",
#endif
			       (ULONG) t1->hdr.caller,
			       caller_count, caller_size, caller_chunk);
	}
	(void) fprintf(dump_file,
		       "------------|-----------|-----------|-----------|\n");
#ifdef LINT
	(void) fprintf(dump_file, "totals:     |%10u |%10u |%10u |\n\n",
#else
	(void) fprintf(dump_file, "totals:     |%10lu |%10lu |%10lu |\n\n",
#endif
		       total_count, total_size, total_chunk);
	(void) fflush(dump_file);
}				/* hup_handler */



/*-------------------------------------------------------------------------*/
static void *
allocate_memory(ULONG length, void *caller_addr)
{
	mem_type   *temp;
	mem_type   *cnew;
	mem_type   *last;
	ULONG       req_size;
	ULONG       incr;

	if (first_time) {
		/* install SIGHUP handler to dump usage info */
		first_time = 0;
		(void) sprintf(dump_fname,
			     "memdiag.%s-%d", ProgramName, (int) ProgramPID);
		dump_file = fopen(dump_fname, "a");

		message("malloc/free diagnostics started");

		(void) sigaction(SIGHUP, NULL, &old_action);
		if (old_action.sa_handler == SIG_DFL) {
			hup_action.sa_handler = hup_handler;
#ifdef _INCLUDE_HPUX_SOURCE
			hup_action.sa_flags = SA_RESETHAND;	/* Just gettting it to compile */
#else
			hup_action.sa_flags = SA_RESTART;
#endif
			(void) sigaction(SIGHUP, &hup_action, NULL);
			message("Installed SIGHUP handler for usage dump");
		} else {
			message("Another SIGHUP handler already installed");
		}
	}
	if (++reentrancy_check > 1) {
		message("MALLOC - reentrancy detected");
		*(ULONG *) 1 = 1L;
	}
	/* round length up to next long word boundary */
	req_size = (length + 3) & ~3;

	/* add in the mem_type overhead */
	req_size += sizeof (mem_type);

	/* check the current list of free space */
	last = NULL;
	for (temp = free_head; temp != NULL; temp = temp->hdr.next) {
		if (temp->hdr.check_mark != FREE_MARKER) {
			message("MALLOC - corrupt free list");
			*(ULONG *) 1 = 1L;
		}
		if (temp->hdr.chunk_size >= req_size)
			break;
		last = temp;
	}

	/* no free space large enough, lets sbrk some more */
	if (temp == NULL) {
		/* round up to the next page boundary */
		incr = (req_size + SBRK_MIN) & ~SBRK_MIN;
		temp = (mem_type *) sbrk(incr);
		if (temp == NULL) {
			message("MALLOC - no memory available");
			*(ULONG *) 1 = 1L;
		}
		temp->hdr.check_mark = FREE_MARKER;
		temp->hdr.chunk_size = incr;
		temp->hdr.caller = NULL;
		temp->hdr.next = NULL;
	}
	/* if space is large enough to split */
	if ((temp->hdr.chunk_size - req_size) > SPLIT_MIN) {
		cnew = (mem_type *) ((char *) temp + req_size);
		cnew->hdr.check_mark = FREE_MARKER;
		cnew->hdr.chunk_size = temp->hdr.chunk_size - req_size;
		cnew->hdr.caller = NULL;
		cnew->hdr.next = temp->hdr.next;
		temp->hdr.next = cnew;
		temp->hdr.chunk_size = req_size;
	}
	/* remove block from the free list */
	if (last == NULL)
		free_head = temp->hdr.next;
	else
		last->hdr.next = temp->hdr.next;

	/* add block to the malloc list */
	temp->hdr.next = malloc_head;
	malloc_head = temp;

	temp->hdr.caller = caller_addr;
	temp->hdr.check_mark = USED_MARKER;
	temp->hdr.used_length = length;
	temp->data[length] = EOD_MARKER;

	reentrancy_check--;

	return ((void *) temp->data);

}				/* allocate_memory */



/*-------------------------------------------------------------------------*/
void       *
malloc(ULONG length)
{
	void       *caller_addr = (void *) *((ULONG *) & length - 1);

	return (allocate_memory(length, caller_addr));

}				/* malloc */



/*-------------------------------------------------------------------------*/
void
free(void *ptr)
{
	mem_type   *cur;
	mem_type   *temp;
	mem_type   *last;

	/* Don't try to free null */
	if (ptr == NULL) {
		message("FREE - NULL pointer");
		*(ULONG *) 1 = 1L;
	}
	if (++reentrancy_check > 1) {
		message("FREE - reentrancy detected");
		*(ULONG *) 1 = 1L;
	}
	/* subtract off mem_type header */
	cur = (mem_type *) ((char *) ptr - sizeof (struct mem_hdr));

	/* check data length integrity */
	if (cur->data[cur->hdr.used_length] != EOD_MARKER) {
		message("FREE - end of data corrupted");
		*(ULONG *) 1 = 1L;
	}
	/* find the current memory in the malloc list */
	last = NULL;
	for (temp = malloc_head; temp != NULL; temp = temp->hdr.next) {
		if (temp->hdr.check_mark != USED_MARKER) {
			message("FREE - corrupt malloc list");
			*(ULONG *) 1 = 1L;
		}
		if (temp == cur)
			break;
		last = temp;
	}

	if (temp == NULL) {
		message("FREE - pointer not found");
		*(ULONG *) 1 = 1L;
	}
	/* remove block from the malloc list */
	if (last == NULL)
		malloc_head = temp->hdr.next;
	else
		last->hdr.next = temp->hdr.next;

	cur->hdr.check_mark = FREE_MARKER;

	/* add block by insertion sort to the free list */
	last = NULL;
	for (temp = free_head; temp != NULL; temp = temp->hdr.next) {
		if (temp > cur)
			break;
		last = temp;
	}
	cur->hdr.next = temp;
	if (last == NULL)
		free_head = cur;
	else
		last->hdr.next = cur;

	/* do garbage collection */

	/* forward chunk reconciliation */
	if (cur->hdr.next != NULL) {
		temp = (mem_type *) ((char *) cur + cur->hdr.chunk_size);
		if (temp == cur->hdr.next) {
			cur->hdr.next = temp->hdr.next;
			cur->hdr.chunk_size += temp->hdr.chunk_size;
			temp->hdr.check_mark = 0L;
			temp->hdr.next = NULL;
		}
	}
	/* reverse chunk reconciliation */
	if (last != NULL) {
		temp = (mem_type *) ((char *) last + last->hdr.chunk_size);
		if (temp == cur) {
			last->hdr.next = temp->hdr.next;
			last->hdr.chunk_size += temp->hdr.chunk_size;
			temp->hdr.check_mark = 0L;
			temp->hdr.next = NULL;
		}
	}
	reentrancy_check--;

}				/* free */



/*-------------------------------------------------------------------------*/
void       *
calloc(ULONG nelem, ULONG length)
{
	void       *caller_addr = (void *) *((ULONG *) & nelem - 1);
	register void *temp;

	length *= nelem;
	temp = allocate_memory(length, caller_addr);
	(void) memset(temp, 0, length);
	return (temp);

}				/* calloc */



/*-------------------------------------------------------------------------*/
void       *
realloc(void *ptr, ULONG new_length)
{
	void       *caller_addr = (void *) *((ULONG *) & ptr - 1);
	mem_type   *temp;
	ULONG       alloc_length;
	void       *cnew;

	if (new_length == 0) {
		if (ptr)
			free(ptr);
		return (NULL);
	}
	if (ptr == NULL)
		return (allocate_memory(new_length, caller_addr));

	temp = (mem_type *) ((char *) ptr - sizeof (struct mem_hdr));

	if (temp->hdr.check_mark != USED_MARKER) {
		message("REALLOC - corrupt malloc list");
		*(ULONG *) 1 = 1L;
	}
	alloc_length = temp->hdr.chunk_size - sizeof (mem_type);

	if (new_length <= alloc_length) {
		/* check data length integrity */
		if (temp->data[temp->hdr.used_length] != EOD_MARKER) {
			message("FREE - end of data corrupted");
			*(ULONG *) 1 = 1L;
		}
		/* update the info */
		temp->hdr.used_length = new_length;
		temp->data[new_length] = EOD_MARKER;
	} else {
		/* we need a new chunk */
		cnew = allocate_memory(new_length, caller_addr);
		(void) memcpy(cnew, ptr, temp->hdr.used_length);
		free(ptr);
		ptr = cnew;
	}

	return (ptr);

}				/* realloc */
