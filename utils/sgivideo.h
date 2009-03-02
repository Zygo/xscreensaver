/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __SGIVIDEO_H__
# ifdef HAVE_SGI_VIDEO

Bool grab_video_frame(Screen *screen, Visual *visual, Drawable dest);

# endif /* HAVE_SGI_VIDEO */
#endif /* __SGIVIDEO_H__ */
