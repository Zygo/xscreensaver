/* xscreensaver, Copyright (c) 1993-1998 by Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __VISUAL_H__
#define __VISUAL_H__

extern Visual *get_visual (Screen *, const char *name, Bool, Bool);
extern Visual *get_visual_resource (Screen *, char *, char *, Bool);
extern int visual_depth (Screen *, Visual *);
/* extern int visual_pixmap_depth (Screen *, Visual *); */
extern int visual_class (Screen *, Visual *);
extern int visual_cells (Screen *, Visual *);
extern int screen_number (Screen *);
extern Visual *find_similar_visual (Screen *, Visual *old);
extern void describe_visual (FILE *f, Screen *, Visual *);
extern Visual *get_overlay_visual (Screen *, unsigned long *pixel_return);
extern Bool has_writable_cells (Screen *screen, Visual *visual);

#endif /* __VISUAL_H__ */
