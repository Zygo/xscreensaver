/* exec.c --- executes a program in *this* pid, without an intervening process.
 * xscreensaver, Copyright (c) 1991-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_EXEC_H__
#define __XSCREENSAVER_EXEC_H__

extern void exec_command (const char *shell, const char *command,
                          int nice_level);

extern int on_path_p (const char *program);

#endif /* __XSCREENSAVER_EXEC_H__ */
