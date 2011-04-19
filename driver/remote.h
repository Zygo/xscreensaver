/* xscreensaver-command, Copyright (c) 1991-1998
 *  by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef _XSCREENSAVER_REMOTE_H_
#define _XSCREENSAVER_REMOTE_H_

extern int xscreensaver_command (Display *dpy, Atom command, long arg,
				 Bool verbose_p, char **error_ret);

extern void server_xscreensaver_version (Display *dpy,
					 char **version_ret,
					 char **user_ret,
					 char **host_ret);

#endif /* _XSCREENSAVER_REMOTE_H_ */
