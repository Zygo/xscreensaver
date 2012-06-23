/* xscreensaver, Copyright (c) 1992-2012 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __GRABSCREEN_H__
#define __GRABSCREEN_H__

/* This will write an image onto the given Drawable.
   The Drawable (arg 3) may be a Window or a Pixmap.

   The Window must be the top-level window.  The image *may or may not*
   be written to the window, though it will definitely be written to
   the drawable.  It's fine for args 2 and 3 to be the same window, or
   for arg 2 to be a Window, and arg 3 to be a Pixmap.

   The loaded image might be from a file, or from a screen shot of the
   desktop, or from the system's video input, depending on user
   preferences.

   When the callback is called, the image data will have been loaded
   into the given drawable.  Copy `name' if you want to keep it.

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


/* Whether one should use GCSubwindowMode when drawing on this window
   (assuming a screen image has been grabbed onto it.)  Yes, this is a
   total kludge. */
extern Bool use_subwindow_mode_p(Screen *screen, Window window);

/* Whether the given window is:
   - the real root window;
   - the virtual root window;
   - a direct child of the root window;
   - a direct child of the window manager's decorations.
 */
extern Bool top_level_window_p(Screen *screen, Window window);


/* Don't call this: this is for the "xscreensaver-getimage" program only. */
extern void grab_screen_image_internal (Screen *, Window);

/* Don't use these: this is how "xscreensaver-getimage" and "grabclient.c"
   pass the file name around. */
#define XA_XSCREENSAVER_IMAGE_FILENAME "_SCREENSAVER_IMAGE_FILENAME"
#define XA_XSCREENSAVER_IMAGE_GEOMETRY "_SCREENSAVER_IMAGE_GEOMETRY"

/* For debugging: turn on verbosity. */
extern void grabscreen_verbose (void);

#ifdef HAVE_COCOA
/* Don't use these: internal interface of grabclient.c. */
extern Bool osx_grab_desktop_image (Screen *, Window, Drawable,
                                    XRectangle *geom_ret);
extern Bool osx_load_image_file (Screen *, Window, Drawable,
                                 const char *filename, XRectangle *geom_ret);
#endif /* HAVE_COCOA */

#endif /* __GRABSCREEN_H__ */
