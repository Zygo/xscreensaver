/* xscreensaver, Copyright (c) 1993, 1994, 1995, 1996, 1997, 1998
 *  by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* The MIT-SHM (Shared Memory) extension is pretty tricky to use.
   This file contains the common boiler-plate for creating a shared
   XImage structure, and for making sure that the shared memory segments
   get allocated and shut down cleanly.
 */

#ifndef __XSCREENSAVER_XSHM_H__

#ifdef HAVE_XSHM_EXTENSION

# include <sys/ipc.h>
# include <sys/shm.h>
# include <X11/extensions/XShm.h>

extern XImage *create_xshm_image (Display *dpy, Visual *visual,
				  unsigned int depth,
				  int format, char *data,
				  XShmSegmentInfo *shm_info,
				  unsigned int width, unsigned int height);

#endif /* HAVE_XSHM_EXTENSION */

#endif /* __XSCREENSAVER_XSHM_H__ */
