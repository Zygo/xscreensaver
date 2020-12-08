/* xscreensaver, Copyright (c) 1993-2017 Jamie Zawinski <jwz@jwz.org>
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
   https://www.x.org/releases/current/doc/xextproto/shm.html

   (This document seems not to ever remain available on the web in one place
   for very long; you can search for it by the title, "MIT-SHM -- The MIT
   Shared Memory Extension".)

   To monitor the system's shared memory segments, run "ipcs -m".
  */

#include "utils.h"

/* #define DEBUG */

#include <errno.h>		/* for perror() */

#ifdef HAVE_JWXYZ
# include "jwxyz.h"
#else
# include <X11/Xutil.h>		/* for XDestroyImage() */
#endif

#include "xshm.h"
#include "resources.h"		/* for get_string_resource() */
#include "thread_util.h"        /* for thread_malloc() */

#ifdef DEBUG
# include <X11/Xmu/Error.h>
#endif

extern char *progname;


/* The documentation for the XSHM extension implies that if the server
   supports XSHM but is not the local machine, the XShm calls will return
   False; but this turns out not to be the case.  Instead, the server
   throws a BadAccess error.  So, we need to catch X errors around all
   of our XSHM calls, sigh.
 */

#ifdef HAVE_XSHM_EXTENSION

static Bool shm_got_x_error = False;
XErrorHandler old_handler = 0;
static int
shm_ehandler (Display *dpy, XErrorEvent *error)
{
  shm_got_x_error = True;

#ifdef DEBUG
  fprintf (stderr, "\n%s: ignoring X error from XSHM:\n", progname);
  XmuPrintDefaultErrorMessage (dpy, error, stderr);
  fprintf (stderr, "\n");
#endif

  return 0;
}


#define CATCH_X_ERROR(DPY) do {				\
  XSync((DPY), False); 					\
  shm_got_x_error = False;				\
  if (old_handler != shm_ehandler)			\
    old_handler = XSetErrorHandler (shm_ehandler);	\
} while(0)

#define UNCATCH_X_ERROR(DPY) do {			\
  XSync((DPY), False); 					\
  if (old_handler)					\
    XSetErrorHandler (old_handler);			\
  old_handler = 0;					\
} while(0)

#endif /* HAVE_XSHM_EXTENSION */


static void
print_error (int err)
{
  fprintf(stderr, "%s: %s\n", progname, strerror(err));
}

static XImage *
create_fallback (Display *dpy, Visual *visual,
                 unsigned int depth,
                 int format, XShmSegmentInfo *shm_info,
                 unsigned int width, unsigned int height)
{
  XImage *image = XCreateImage (dpy, visual, depth, format, 0, NULL,
                                width, height, BitmapPad(dpy), 0);
  shm_info->shmid = -1;

  if (!image) {
    print_error (ENOMEM);
  } else {
    /* Sometimes the XImage data needs to be aligned, such as for SIMD (SSE2
       in Fireworkx), or multithreading (AnalogTV).
     */
    int error = thread_malloc ((void **)&image->data, dpy,
                               image->height * image->bytes_per_line);
    if (error) {
      print_error (error);
      XDestroyImage (image);
      image = NULL;
    } else {
      memset (image->data, 0, image->height * image->bytes_per_line);
    }
  }

  return image;
}


XImage *
create_xshm_image (Display *dpy, Visual *visual,
		   unsigned int depth,
		   int format, XShmSegmentInfo *shm_info,
		   unsigned int width, unsigned int height)
{
#ifndef HAVE_XSHM_EXTENSION

  return create_fallback (dpy, visual, depth, format, shm_info, width, height);

#else /* HAVE_XSHM_EXTENSION */

  Status status;
  XImage *image = 0;
  if (!get_boolean_resource(dpy, "useSHM", "Boolean") ||
      !XShmQueryExtension (dpy)) {
    return create_fallback (dpy, visual, depth, format, shm_info,
                            width, height);
  }

  CATCH_X_ERROR(dpy);
  image = XShmCreateImage(dpy, visual, depth,
                          format, NULL, shm_info, width, height);
  UNCATCH_X_ERROR(dpy);
  if (shm_got_x_error)
    return create_fallback (dpy, visual, depth, format, shm_info,
                            width, height);

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

      CATCH_X_ERROR(dpy);
      status = XShmAttach(dpy, shm_info);
      UNCATCH_X_ERROR(dpy);
      if (shm_got_x_error)
	status = False;

      if (!status)
	{
	  fprintf (stderr, "%s: XShmAttach failed!\n", progname);
	  XDestroyImage (image);
	  XSync(dpy, False);
	  shmdt (shm_info->shmaddr);
	  image = 0;
	}
#ifdef DEBUG
      else
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

  if (!image) {
    return create_fallback (dpy, visual, depth, format, shm_info,
                            width, height);
  }

  return image;

#endif /* HAVE_XSHM_EXTENSION */
}


Bool
put_xshm_image (Display *dpy, Drawable d, GC gc, XImage *image,
                int src_x, int src_y, int dest_x, int dest_y,
                unsigned int width, unsigned int height,
                XShmSegmentInfo *shm_info)
{
#ifdef HAVE_XSHM_EXTENSION
  assert (shm_info); /* Don't just s/XShmPutImage/put_xshm_image/. */
  if (shm_info->shmid != -1) {
    /* XShmPutImage is asynchronous; the contents of the XImage must not be
       modified until the server has placed the pixels on the screen and the
       client has received an XShmCompletionEvent. Breaking this rule can cause
       tearing. That said, put_xshm_image doesn't provide a send_event
       parameter, so we're always breaking this rule. Not that it seems to
       matter; everything (so far) looks fine without it.

       ####: Add a send_event parameter. And fake it for XPutImage.
     */
    return XShmPutImage (dpy, d, gc, image, src_x, src_y, dest_x, dest_y,
                         width, height, False);
  }
#endif /* HAVE_XSHM_EXTENSION */

  return XPutImage (dpy, d, gc, image, src_x, src_y, dest_x, dest_y,
                    width, height);
}


Bool
get_xshm_image (Display *dpy, Drawable d, XImage *image, int x, int y,
                unsigned long plane_mask, XShmSegmentInfo *shm_info)
{
#ifdef HAVE_XSHM_EXTENSION
  if (shm_info->shmid != -1) {
    return XShmGetImage (dpy, d, image, x, y, plane_mask);
  }
#endif /* HAVE_XSHM_EXTENSION */
  return XGetSubImage (dpy, d, x, y, image->width, image->height, plane_mask,
                       image->format, image, 0, 0) != NULL;
}


void
destroy_xshm_image (Display *dpy, XImage *image, XShmSegmentInfo *shm_info)
{
#ifdef HAVE_XSHM_EXTENSION
  Status status;

  if (shm_info->shmid == -1) {
#endif /* HAVE_XSHM_EXTENSION */

    /* Don't let XDestroyImage free image->data. */
    thread_free (image->data);
    image->data = NULL;
    XDestroyImage (image);
    return;

#ifdef HAVE_XSHM_EXTENSION
  }

  CATCH_X_ERROR(dpy);
  status = XShmDetach (dpy, shm_info);
  UNCATCH_X_ERROR(dpy);
  if (shm_got_x_error)
    status = False;
  if (!status)
    fprintf (stderr, "%s: XShmDetach failed!\n", progname);
# ifdef DEBUG
  else
    fprintf (stderr, "%s: XShmDetach(dpy, shm_info) ==> True\n", progname);
# endif

  XDestroyImage (image);
  XSync(dpy, False);

  status = shmdt (shm_info->shmaddr);

  if (status != 0)
    {
      char buf[1024];
      sprintf (buf, "%s: shmdt(0x%lx) failed", progname,
               (unsigned long) shm_info->shmaddr);
      perror(buf);
    }
# ifdef DEBUG
  else
    fprintf (stderr, "%s: shmdt(shm_info->shmaddr) ==> 0\n", progname);
# endif

  XSync(dpy, False);

#endif /* HAVE_XSHM_EXTENSION */
}
