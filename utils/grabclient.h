/* xscreensaver, Copyright © 1992-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __GRABCLIENT_H__
#define __GRABCLIENT_H__

/* This will write an image onto the given Drawable.
   The Drawable (arg 3) may be a Window or a Pixmap.

   The Window must be the top-level window.  The image *may or may not*
   be written to the Window, though it will definitely be written to
   the Drawable.  It's fine for the Window and Drawable to be equal,
   or for the Drawable to be a Pixmap.

   The Window and Drawable need not be the same size.
   (Smaller pixmap: Tessellimage, XAnalogTV; larger pixmap: Esper)

   The loaded image might be from a file, or from a screen shot of the
   desktop, or from the system's video input, depending on user
   preferences.

   When the callback is called, the image data will have been loaded
   into the given Drawable.  Copy `name' if you want to keep it.

   If it is from a file, then the `filename' argument will be the name
   of the file.  It may be NULL.  If you want to keep this string, copy it.

   The size and position of the image is in the `geometry' arg.
   The image will generally have been scaled up to fit the window, but
   if a loaded file had a different aspect ratio than the window, it
   will have been centered, and the returned coords will describe that.

   Many colors may be allocated from the window's colormap.
 */
extern void load_image_async (Screen *, Window, Drawable,
                              void (*callback) (Screen *, Window,
                                                Drawable,
                                                const char *name,
                                                XRectangle *geometry,
                                                void *closure),
                              void *closure);

/* A utility wrapper around load_image_async() that is simpler if you
   are only loading a single image at a time: just keep calling it
   periodically until it returns NULL.  When it does, the image has
   been loaded.
 */
typedef struct async_load_state async_load_state;
extern async_load_state *load_image_async_simple (async_load_state *,
                                                  Screen *,
                                                  Window top_level,
                                                  Drawable target, 
                                                  char **filename_ret,
                                                  XRectangle *geometry_ret);


/* Don't use these: this is how "xscreensaver-getimage" and "grabclient.c"
   pass the file name around. */
#define XA_XSCREENSAVER_IMAGE_FILENAME "_SCREENSAVER_IMAGE_FILENAME"
#define XA_XSCREENSAVER_IMAGE_TITLE    "_SCREENSAVER_IMAGE_TITLE"
#define XA_XSCREENSAVER_IMAGE_GEOMETRY "_SCREENSAVER_IMAGE_GEOMETRY"

#ifdef HAVE_JWXYZ
/* Don't use these: internal interface of grabclient.c. */
extern Bool osx_grab_desktop_image (Screen *, Window, Drawable,
                                    XRectangle *geom_ret);
extern Bool osx_load_image_file (Screen *, Window, Drawable,
                                 const char *filename, XRectangle *geom_ret);
#endif /* HAVE_JWXYZ */

#ifdef HAVE_IPHONE
extern void ios_load_random_image (void (*callback) (void *uiimage,
                                                     const char *filename,
                                                     int w, int h,
                                                     void *closure),
                                   void *closure,
                                   int width, int height);
#endif /* HAVE_IPHONE */

#ifdef HAVE_ANDROID
char *jwxyz_draw_random_image (Display *dpy, Drawable drawable, GC gc);
#endif

#endif /* __GRABCLIENT_H__ */
