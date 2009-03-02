/* xscreensaver, Copyright (c) 1997, 1998 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains code for grabbing a frame from one of the video inputs
   on an SGI.  It returns it on a Drawable where it can be hacked at will.
   This code checks all available video devices for the first one with a
   non-blank signal.

   It checks the deviced whose number comes from the `videoDevice' resource
   first, then the default video device, then all the devices in order.

   The intensity of the video signal is increased by the value of the
   `videoGain' resource (a float) defaulting to 2.2, since NTSC video tends
   to appear kind of dim on computer screens.

   The video image is expanded to fit the window (while preserving the aspect
   ratio.)  This is done by simply replicating pixels, not dithering.  That
   turns out to look good enough most of the time.

   If the target window is not TrueColor, the grabbed image will be quantized
   to fit.  This also is done without dithering, but in this case, dithering
   would help a lot, because it looks like crap.  So use TrueColor if you care.
 */

#include "utils.h"
#include "sgivideo.h"
#include "resources.h"
#include "visual.h"

#ifdef HAVE_SGI_VIDEO	/* whole file */

#include "usleep.h"

#include <X11/Xutil.h>

#ifdef DEBUG
extern char *progname;
#endif /* DEBUG */


# include <dmedia/vl.h>

static Bool dark_image_p(unsigned long *image, int width, int height);
static Bool install_video_frame(unsigned long *image, int width, int height,
				Screen *screen, Visual *visual, Drawable dest);

#ifdef DEBUG
static void
describe_input(const char *prefix, VLServer server, int camera)
{
  VLDevList dl;
  int i, j;

  if (camera == VL_ANY)
    {
      printf("%s: %s VL_ANY\n", progname, prefix);
      return;
    }

  vlGetDeviceList(server, &dl);
  for (i = 0; i < dl.numDevices; i++)
    {
      VLDevice *d = &dl.devices[i];
      for (j = 0; j < d->numNodes; j++)
	if (d->nodes[j].number == camera)
	  {
	    printf("%s: %s %d, \"%s\"\n", progname, prefix,
		   d->nodes[j].number,
		   d->nodes[j].name);
	    return;
	  }
    }

  /* else... */
  printf("%s: %s %d (???)\n", progname, prefix, camera);
}
#endif /* DEBUG */


static Bool
grab_frame_1(Screen *screen, Visual *visual, Drawable dest, int camera)
{
  Bool status = False;
  int width = 0;
  int height = 0;
  VLServer server = 0;
  VLNode input = -1;
  VLNode output = -1;
  VLPath path = 0;
  VLBuffer buffer = 0;
  VLControlValue ctl;
  VLInfoPtr info;
  VLTransferDescriptor trans;
  unsigned long *image = 0;

  server = vlOpenVideo (NULL);
  if (!server)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to open video server\n", progname);
#endif /* DEBUG */
      goto DONE;
    }

#ifdef DEBUG
  describe_input("trying device", server, camera);
#endif /* DEBUG */

  input  = vlGetNode (server, VL_SRC, VL_VIDEO, camera);
  output = vlGetNode (server, VL_DRN, VL_MEM, VL_ANY);

  if (input == -1 || output == -1)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to get video I/O nodes: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

  path = vlCreatePath (server, VL_ANY, input, output);
  if (path == -1)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to get video path: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

  if (vlSetupPaths (server, (VLPathList) &path, 1, VL_SHARE, VL_SHARE) == -1)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to set up video path: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

  ctl.intVal = VL_CAPTURE_INTERLEAVED;
  if (vlSetControl (server, path, output, VL_CAP_TYPE, &ctl) == -1)
    {
#ifdef DEBUG
      fprintf (stderr,
	       "%s: unable to set video capture type to interleaved: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

  ctl.intVal = VL_PACKING_RGBA_8;
  if (vlSetControl (server, path, output, VL_PACKING, &ctl) == -1)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to set video packing to RGB8: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

  buffer = vlCreateBuffer (server, path, output, 3);
  if (!buffer)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to create video buffer: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

  vlRegisterBuffer (server, path, output, buffer);

  memset(&trans, 0, sizeof(trans));
  trans.trigger = VLTriggerImmediate;
  trans.mode = VL_TRANSFER_MODE_DISCRETE;
  trans.delay = 0;
  trans.count = 1;
  if (vlBeginTransfer (server, path, 1, &trans) == -1)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to begin video transfer: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }
    
  
  /* try to get a frame; don't try more than a zillion times.
     I strongly suspect this isn't the best way to do this...
   */
  {
    int i;
    for (i = 0; i < 1000; i++)
      {
	info = vlGetLatestValid (server, buffer);
	if (info) break;
	usleep(10000);	/* 1/100th second (a bit more than half a field) */
      }
  }

  if (!info)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to get video info: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

  image = vlGetActiveRegion (server, buffer, info);
  if (!image)
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to grab video frame: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

  if (vlGetControl (server, path, input, VL_SIZE, &ctl) != -1)
    {
      width  = ctl.xyVal.x;
      height = ctl.xyVal.y;
    }
  else
    {
#ifdef DEBUG
      fprintf (stderr, "%s: unable to get video image size: %d\n",
	       progname, vlErrno);
#endif /* DEBUG */
      goto DONE;
    }

#ifdef DEBUG
  describe_input("read device", server, camera);
#endif /* DEBUG */

  if (dark_image_p(image, width, height))
    goto DONE;

  status = install_video_frame(image, width, height, screen, visual, dest);

 DONE:

  /* I think `image' is freed as a result of destroying buffer. */

  if (buffer)
    vlDestroyBuffer(server, buffer);
  if (path)
    vlDestroyPath(server, path);
  if (server)
    vlCloseVideo (server);

  return status;
}


static Bool
dark_image_p(unsigned long *image, int width, int height)
{
  double max = 0.02;
  double total = 0.0;
  int i;
  int pixels = (width * height);
#ifdef DEBUG
  int mr = 0, mg = 0, mb = 0;
#endif /* DEBUG */
  for (i = pixels-1; i >= 0; i--)
    {
      unsigned long pixel = image[i];
      unsigned int r = (pixel & 0x0000FF);
      unsigned int g = (pixel & 0x00FF00) >> 8;
      unsigned int b = (pixel & 0xFF0000) >> 16;
#ifdef DEBUG
      if (r > mr) mr = r;
      if (g > mg) mg = g;
      if (b > mb) mb = b;
#endif /* DEBUG */
      total += ((r * (0.3086 / 0xFF)) +       /* gamma 1.0 intensity values */
		(g * (0.6094 / 0xFF)) +
		(b * (0.0820 / 0xFF)));
    }
  total /= pixels;
#ifdef DEBUG
  fprintf(stderr, "%s: %sdark %f (max rgb: %d %d %d)\n", progname,
	  (total < max ? "" : "not "),
	  total, mr, mg, mb);
#endif /* DEBUG */
  return (total < max);
}


Bool
grab_video_frame(Screen *screen, Visual *visual, Drawable dest)
{
  char *def_camera = get_string_resource("videoDevice", "Integer");
  if (def_camera && *def_camera)
    {
      int cam;
      char c;
      int ok = (1 == sscanf(def_camera, " %d %c", &cam, &c));
      free (def_camera);
      if (ok && grab_frame_1(screen, visual, dest, cam))
	return True;
    }

  if (grab_frame_1(screen, visual, dest, VL_ANY))
    return True;
  else
    {
      int i;
      VLServer server = vlOpenVideo (NULL);

      if (!server) return False;

      for (i = 0; i < 5; i++)	/* if we get all black images, retry up to
				   five times. */
	{
	  VLDevList dl;
	  int j;

          j = vlGetDeviceList(server, &dl);
          vlCloseVideo(server);
          if (j < 0) return False;

	  for (j = 0; j < dl.numDevices; j++)
	    {
	      VLDevice *d = &dl.devices[j];
	      int k;
	      for (k = 0; k < d->numNodes; k++)
		if (d->nodes[k].type == VL_SRC &&
		    d->nodes[k].kind == VL_VIDEO)
		  if (grab_frame_1(screen, visual, dest, d->nodes[k].number))
		    return True;
	      /* nothing yet?  go around and try again... */
	    }
	}
    }
#ifdef DEBUG
  fprintf (stderr, "%s: images on all video feeds are too dark.\n",
	   progname);
#endif /* DEBUG */
  return False;
}


static Bool
install_video_frame(unsigned long *image, int width, int height,
		    Screen *screen, Visual *visual, Drawable dest)
{
  Display *dpy = DisplayOfScreen(screen);
  int x, y;
  unsigned int w, h, b, d;
  Window root;
  XGCValues gcv;
  GC gc;
  XImage *ximage = 0;
  int image_depth;
  Bool free_data = False;
  int vblank_kludge = 3;	/* lose the closed-captioning blips... */

  double gain;
  char c, *G = get_string_resource("videoGain", "Float");
  if (!G || (1 != sscanf (G, " %lf %c", &gain, &c)))
    /* default to the usual NTSC gamma value.  Is this the right thing to do?
       (Yeah, "gain" isn't quite "gamma", but it's close enough...) */
    gain = 2.2;
  if (G) free (G);

  XGetGeometry(dpy, dest, &root, &x, &y, &w, &h, &b, &d);
  
  gcv.function = GXcopy;
  gcv.foreground = BlackPixelOfScreen(screen);
  gc = XCreateGC (dpy, dest, GCFunction|GCForeground, &gcv);

  image_depth = visual_depth(screen, visual);
  if (image_depth < 24)
    image_depth = 24;  /* We'll dither */

  ximage = XCreateImage (dpy, visual, image_depth, ZPixmap, 0, (char *) image,
			 width, height, 8, 0);
  XInitImage(ximage);
  if (!ximage)
    return False;

  if (gain > 0.0)	/* Pump up the volume */
    {
      unsigned char *end = (unsigned char *) (image + (width * height));
      unsigned char *s = (unsigned char *) image;
      while (s < end)
	{
	  unsigned int r = s[1] * gain;
	  unsigned int g = s[2] * gain;
	  unsigned int b = s[3] * gain;
	  s[1] = (r > 255 ? 255 : r);
	  s[2] = (g > 255 ? 255 : g);
	  s[3] = (b > 255 ? 255 : b);
	  s += 4;
	}
    }

  /* If the drawable is not of truecolor depth, we need to convert the
     grabbed bits to match the depth by clipping off the less significant
     bit-planes of each color component.
  */
  if (d != 24 && d != 32)
    {
      int x, y;
      /* We use the same ximage->data in both images -- that's ok, because
	 since we're reading from B and writing to A, and B uses more bytes
	 per pixel than A, the write pointer won't overrun the read pointer.
      */
      XImage *ximage2 = XCreateImage (dpy, visual, d, ZPixmap, 0,
				      (char *) image,
				      width, height, 8, 0);
      XInitImage(ximage2);
      if (!ximage2)
	{
	  XDestroyImage(ximage);
	  return False;
	}

#ifdef DEBUG
      fprintf(stderr, "%s: converting from depth %d to depth %d\n",
	      progname, ximage->depth, ximage2->depth);
#endif /* DEBUG */

      for (y = 0; y < ximage->height; y++)
	for (x = 0; x < ximage->width; x++)
	  {
	    unsigned long pixel = XGetPixel(ximage, x, y);
	    unsigned int r = (pixel & 0x0000FF);
	    unsigned int g = (pixel & 0x00FF00) >> 8;
	    unsigned int b = (pixel & 0xFF0000) >> 16;

	    if (d == 8)
	      pixel = ((r >> 5) | ((g >> 5) << 3) | ((b >> 6) << 6));
	    else if (d == 12)
	      pixel = ((r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8));
	    else if (d == 16)
	      pixel = ((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
	    else
	      abort();

	    XPutPixel(ximage2, x, y, pixel);
	  }
      ximage->data = 0;
      XDestroyImage(ximage);
      ximage = ximage2;
    }

  if (width < w && height < h)      /* Stretch the image to fit the window. */
    {
      double dw = (((double) w) / ((double) width));
      double dh = (((double) h) / ((double) height));
      double d = (dw > dh ? dh : dw);
      int width2  = d * width;
      int height2 = d * height;
      int x, y;
      XImage *ximage2 = XCreateImage (dpy, visual, ximage->depth, ZPixmap,
				      0, NULL,
				      width2, height2, 8, 0);
      if (!ximage2->data)
	ximage2->data = (char *) malloc(width2 * height2 * 4);
      free_data = True;
      XInitImage(ximage2);
#ifdef DEBUG
      fprintf(stderr, "%s: stretching video image by %f (%d %d -> %d %d)\n",
	      progname, d, width, height, width2, height2);
#endif /* DEBUG */
      for (y = 0; y < height2; y++)
	{
	  int y2 = (int) (y / d);
	  for (x = 0; x < width2; x++)
	    XPutPixel(ximage2, x, y, XGetPixel(ximage, (int) (x / d), y2));
	}
      ximage->data = 0;
      XDestroyImage(ximage);
      ximage = ximage2;
      width = width2;
      height = height2;
      vblank_kludge *= d;
    }

  XFillRectangle(dpy, dest, gc, 0, 0, w, h);
  XPutImage(dpy, dest, gc, ximage, 0, vblank_kludge,
	    (w - width) / 2,
	    (h - height) / 2,
	    width, height - vblank_kludge);
  XSync(dpy, False);

  if (free_data)
    free(ximage->data);
  ximage->data = 0;
  XDestroyImage(ximage);
  XFreeGC (dpy, gc);
  return True;
}

#endif /* HAVE_SGI_VIDEO */
