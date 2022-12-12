/* xscreensaver, Copyright © 1991-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_XINPUT_H__
#define __XSCREENSAVER_XINPUT_H__

extern Bool init_xinput (Display *dpy, int *opcode_ret);
extern Bool xinput_event_to_xlib (int evtype, XIDeviceEvent *in, XEvent *out);
extern void print_xinput_event (Display *, XEvent *, const char *desc);

#endif /* __XSCREENSAVER_XINPUT_H__ */
