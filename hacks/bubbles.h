/* bubbles.h - definitions for bubbles screensaver */

/* $Id: bubbles.h,v 1.6 2006/02/25 20:11:57 jwz Exp $ */

#ifndef _BUBBLES_H_
#define _BUBBLES_H_

#ifdef HAVE_JWXYZ
# include "jwxyz.h"
# define HAVE_XPM
#else
# include <X11/Xlib.h>
#endif

/***************************************************************************
 *   Options you might like to change to affect the program's behaviour    *
 ***************************************************************************/

/*
 *   Uncommenting the following will enable support for reading bubbles from 
 * files (using the -file and -directory options to bubbles).  This is
 * disabled by default since such operations are inherently non-portable
 * and we want the program to compile on as many systems as possible.  
 *
 *   If you uncomment this and you figure out how to get it working, please
 * let me (J.Macnicol@student.anu.edu.au) know.  Diffs against the standard
 * distribution would be appreciated.  Possible sources of problems are
 * dirent and possibly the use of tmpnam().
 */

/* #define BUBBLES_IO */

/*
 *   The following only makes sense if BUBBLES_IO above is defined.
 * 
 *   Uncomment the following if you always want to use the -file or
 * -directory options on the command line and never to use a default bubble
 * compiled into the program.  This way you would save memory and disk space
 * since if you do use -file or -directory only one bubble will be loaded
 * into memory at any one time (and remember the default bubble is really
 * uncompressed, unlike bubbles in files which can be compressed).  This
 * is disabled by default only so people running the program for the first
 * time with no knowldege of the command line options don't get error
 * messages ;)
 *
 * NOTE: You will still need to have a bubbles_default.c file, else the
 * build sequence will fail.  Well constructed bubbles_default.c files
 * have #ifdef's which simply exclude everything else in the file at
 * compile time.  The bubblestodefault script does this.
 */

/* #define NO_DEFAULT_BUBBLE */

/*
 * This turns on any debugging messages and sanity checks.  Hopefully you
 * won't need this :)  It slows things down a bit, too.
 *
 * NOTE: If you uncomment this you will get some messages about unused
 * functions when you compile.  You can ignore these - they refer to 
 * convenient checking routines which simply aren't called but are left
 * in case someone wants to use them.
 */

/* #define DEBUG */

/***************************************************************************
 *      Things you might need to change to get things working right        *
 ***************************************************************************/

/*
 *  Name of the gzip binary.  You shouldn't need to change this unless it's
 * not in your PATH when the program is run, in which case you will need to
 * substitute the full path here.  Keep the double quotes else things won't
 * compile!
 */

#define GZIP               "gzip"

/*
 *  Likewise for the Bourne shell.
 */

#define BOURNESH           "sh"

/*
 * The name of the directory entry structure is different under Linux
 * (under which this code is being developed) than other systems.  The case
 * alternate form here is that given in Kernighan & Ritchie's C book (which
 * must be authoratitive, no?) 
 *
 * 04/07/96 : People will have to hack this to get it working on some
 * systems.  I believe it doesn't work on SGI, for example.
 */

#ifdef _POSIX_SOURCE
#define STRUCT_DIRENT      struct dirent
#else
#define STRUCT_DIRENT      Dirent
#endif

/* 
 * The naming of fields in struct dirent also seems to differ from system to
 * system.  This may have to be extended to make things truly portable.
 * What we want here is the name field from a dirent struct pointed to
 * by "dp". 
 *
 * 04/07/96 : See above.  This may need to be changed too.
 */

#ifdef _POSIX_SOURCE
#define DIRENT_NAME       dp->d_name
#else
#define DIRENT_NAME       dp->name
#endif

/* I don't know why this isn't defined. */
#ifdef linux
/* apparently it is defined in recent linuxes.  who knows. */
/*extern char *tempnam(char *, char *);*/
#endif

/****************************************************************************
 *      Buffer lengths and things you probably won't need to touch          *
 ****************************************************************************/

/* Maximum length of a full path name we can deal with */
#define PATH_BUF_SIZE      1024

/* Size of string passed to shell as command */
#define COMMAND_BUF_SIZE   2500

/* Size increments for read_line() buffers */
#define READ_LINE_BUF_SIZE 24

/* Maximum amount to drop a bubble */
#define MAX_DROPPAGE 20

/****************************************************************************
 *                        End of options                                    *
 ****************************************************************************/

/* Some machines define M_PI and not PI.  If they don't define either, use
own own.  Really, the accuracy of this is _not_ very important. */
#ifndef PI
# define PI  M_PI
# ifndef M_PI
#  define M_PI 3.1415926535
# endif
#endif

/* for delete_bubble_in_mesh() */
#define DELETE_BUBBLE      0
#define KEEP_BUBBLE        1

/* Status codes for read_line */
#define LINE_READ          0
#define EOF_REACHED        1
#define IO_ERROR           2

/* 
 * Magic number for Bubble struct, in case it's trashed when debugging code
 * (which happened to me often.... :(  
 */

#define BUBBLE_MAGIC       5674

/* Useful macros */
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

/* How we represent bubbles */
struct bub {
  int radius;
  int step;  /* for rendered bubbles */
  long area;
  int x;
  int y;
  int magic;
  int cell_index;
  int visible;
  struct bub *next;
  struct bub *prev;
};

typedef struct bub Bubble;

/*
 * How we represent pixmaps of rendered bubbles.  Because the range of radii
 * available may not be continuous, we call each a step (for the lack of a
 * better name...)
 */

#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
struct bub_step {
  int radius;
  long area;
  int droppage;
  Pixmap ball, shape_mask;
  GC draw_gc, erase_gc;
  struct bub_step *next;
};

typedef struct bub_step Bubble_Step;
#endif /* HAVE_XPM || HAVE_GDK_PIXBUF */

/* Make sure default bubble isn't compiled when we don't have XPM
Disable file I/O code too. */
#if !defined(HAVE_XPM) && !defined(HAVE_GDK_PIXBUF)
# define NO_DEFAULT_BUBBLE
# undef BUBBLES_IO
#endif /* !HAVE_XPM && !HAVE_GDK_PIXBUF */

/* Make sure default bubble is compiled in when we have XPM and no file I/O */
#if defined(HAVE_XPM) || defined(HAVE_GDK_PIXBUF)
# ifndef BUBBLES_IO
#  undef NO_DEFAULT_BUBBLE
# endif /* BUBBLES_IO */
#endif /* HAVE_XPM || HAVE_GDK_PIXBUF */

extern void init_default_bubbles(void);
extern int num_default_bubbles;
extern char **default_bubbles[];

#endif /* _BUBBLES_H_ */
