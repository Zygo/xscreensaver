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

   This code currently deals only with shared XImages, not with shared Pixmaps.
   It also doesn't use "completion events", but so far that doesn't seem to
   be a problem (and I'm not entirely clear on when they would actually be
   needed, anyway.)

   If you don't have man pages for this extension, see
   http://www.physik.uni-regensburg.de/~scs22156/sofie-0.2/mit-shm.html
   or in the R6 sources as "xc/doc/specs/Xext/mit-shm.ms".

   (This document seems not to ever remain available on the web in one place
   for very long; you can search for it by the title, "MIT-SHM -- The MIT
   Shared Memory Extension".)

   To monitor the system's shared memory segments, run "ipcs -m".
  */

#include "utils.h"

#ifdef HAVE_XSHM_EXTENSION	/* whole file */

#include <errno.h>		/* for perror() */
#include <X11/Xutil.h>		/* for XDestroyImage() */

#include "xshm.h"
#include "resources.h"		/* for get_string_resource() */


extern char *progname;

XImage *
create_xshm_image (Display *dpy, Visual *visual,
		   unsigned int depth,
		   int format, char *data,
		   XShmSegmentInfo *shm_info,
		   unsigned int width, unsigned int height)
{
  XImage *image = 0;
  if (!get_boolean_resource("useSHM", "Boolean"))
    return 0;

  if (!XShmQueryExtension (dpy))
    return 0;

  image = XShmCreateImage(dpy, visual, depth,
			      format, data, shm_info, width, height);
#ifdef DEBUG
  fprintf(stderr, "\n%s: XShmCreateImage(... %d, %d)\n", progname,
	  width, height);
#endif

  shm_info->shmid = shmget(IPC_PRIVATE,
			   image->bytes_per_line * image->height,
			   IPC_CREAT | 0777);
#ifdef DEBUG
  fprintf(stderr, "%s: shmget(IPC_PRIVATE, %d, IPC_CREAT | 0777) ==> %d\n",
	  progname, image->bytes_per_line * image->height, shm_info->shmid);
#endif

  if (shm_info->shmid == -1)
    {
      char buf[1024];
      sprintf (buf, "%s: shmget failed", progname);
      perror(buf);
      XDestroyImage (image);
      image = 0;
      XSync(dpy, False);
    }
  else
    {
      shm_info->readOnly = False;
      image->data = shm_info->shmaddr = shmat(shm_info->shmid, 0, 0);

#ifdef DEBUG
      fprintf(stderr, "%s: shmat(%d, 0, 0) ==> %d\n", progname,
	      shm_info->shmid, (int) image->data);
#endif

      if (!XShmAttach(dpy, shm_info))
	{
	  fprintf (stderr, "%s: XShmAttach failed!\n", progname);
	  XDestroyImage (image);
	  XSync(dpy, False);
	  shmdt (shm_info->shmaddr);
	  image = 0;
	}

#ifdef DEBUG
      fprintf(stderr, "%s: XShmAttach(dpy, shm_info) ==> True\n", progname);
#endif

      XSync(dpy, False);

      /* Delete the shared segment right now; the segment won't actually
	 go away until both the client and server have deleted it.  The
	 server will delete it as soon as the client disconnects, so we
	 should delete our side early in case of abnormal termination.
	 (And note that, in the context of xscreensaver, abnormal
	 termination is the rule rather than the exception, so this would
	 leak like a sieve if we didn't do this...)

	 #### Are we leaking anyway?  Perhaps because of the window of
	 opportunity between here and the XShmAttach call above, during
	 which we might be killed?  Do we need to establish a signal
	 handler for this case?
       */
      shmctl (shm_info->shmid, IPC_RMID, 0);

#ifdef DEBUG
      fprintf(stderr, "%s: shmctl(%d, IPC_RMID, 0)\n\n", progname,
	      shm_info->shmid);
#endif
    }

  return image;
}

#endif /* HAVE_XSHM_EXTENSION */
