/* xscreensaver, Copyright (c) 1992, 1993, 1994, 1997, 2001, 2003, 2004, 2005
 *  Jamie Zawinski <jwz@jwz.org>
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

   If it is from a file, then it will be returned in `filename_return'.
   filename_return may be NULL; also, NULL may be returned (e.g., if
   it's a screenshot or video capture.)  You are responsible for
   freeing this string.

   The size and position of the image is returned in `geometry_return'.
   The image will generally have been scaled up to fit the window, but
   if a loaded file had a different aspect ratio than the window, it
   will have been centered, and the returned coords will describe that.

   Many colors may be allocated from the window's colormap.
 */
extern void load_random_image (Screen *screen,
                               Window top_level_window,
                               Drawable target_window_or_pixmap, 
                               char **filename_return,
                               XRectangle *geometry_return);

/* Like the above, but loads the image in the background and runs the
   given callback once it has been loaded.  Copy `name' if you want
   to keep it.
 */
extern void fork_load_random_image (Screen *screen, Window window,
                                    Drawable drawable,
                                    void (*callback) (Screen *, Window,
                                                      Drawable,
                                                      const char *name,
                                                      XRectangle *geometry,
                                                      void *closure),
                                    void *closure);


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

#endif /* __GRABSCREEN_H__ */
