/* xscreensaver, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __CLIENTMSG_H__
#define __CLIENTMSG_H__

extern Window find_screensaver_window (Display *, char **version);
extern void clientmessage_response (Display *, XEvent *, Bool ok,
                                    const char *msg);

#endif /* __CLIENTMSG_H__ */
