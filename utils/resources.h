/* xscreensaver, Copyright (c) 1992-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_RESOURCES_H__
#define __XSCREENSAVER_RESOURCES_H__

extern char *get_string_resource (Display*,char*,char*);
extern Bool get_boolean_resource (Display*,char*,char*);
extern int get_integer_resource (Display*,char*,char*);
extern double get_float_resource (Display*,char*,char*);
extern unsigned int get_pixel_resource (Display*,Colormap,char*,char*);
extern unsigned int get_minutes_resource (Display*,char*,char*);
extern unsigned int get_seconds_resource (Display*,char*,char*);
extern int parse_time (const char *string, Bool seconds_default_p,
                       Bool silent_p);
extern Pixmap
xscreensaver_logo (Screen *screen, Visual *visual,
                   Drawable drawable, Colormap cmap,
                   unsigned long background_color,
                   unsigned long **pixels_ret, int *npixels_ret,
                   Pixmap *mask_ret,
                   Bool big_p);


/* A utility function for event-handler functions:
   Returns True if the event is a simple click, Space, Tab, etc.
   Returns False otherwise.
   The idea here is that most hacks interpret to clicks or basic
   keypresses as "change it up".
 */
extern Bool screenhack_event_helper (Display *, Window, XEvent *);

#endif /* __XSCREENSAVER_RESOURCES_H__ */
