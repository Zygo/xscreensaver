/* xscreensaver, Copyright © 1993-2021 Jamie Zawinski <jwz@jwz.org>
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
extern int visual_pixmap_depth (Screen *, Visual *);
extern int visual_class (Screen *, Visual *);
extern int visual_cells (Screen *, Visual *);
extern int screen_number (Screen *);
extern Visual *find_similar_visual (Screen *, Visual *old);
extern void describe_visual (FILE *f, Screen *, Visual *, Bool private_cmap_p);
extern Visual *get_overlay_visual (Screen *, unsigned long *pixel_return);
extern Bool has_writable_cells (Screen *, Visual *);
extern Visual *id_to_visual (Screen *, int);
extern void visual_rgb_masks (Screen *screen, Visual *visual,
                              unsigned long *red_mask,
                              unsigned long *green_mask,
                              unsigned long *blue_mask);

extern Visual *get_gl_visual (Screen *);
extern void describe_gl_visual (FILE *, Screen *, Visual *, Bool priv_cmap_p);
extern Bool validate_gl_visual (FILE *, Screen *, const char *, Visual *);

#ifdef __egl_h_  /* EGL/egl.h included */
extern void get_egl_config (Display *, EGLDisplay *, EGLint vid, EGLConfig *);
#endif

#endif /* __VISUAL_H__ */
