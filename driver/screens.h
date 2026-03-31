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

#ifndef __XSCREENSAVER_SCREENS_H__
#define __XSCREENSAVER_SCREENS_H__

typedef struct _monitor monitor;

typedef enum { S_SANE, S_ENCLOSED, S_DUPLICATE, S_OVERLAP, 
               S_OFFSCREEN, S_DISABLED } monitor_sanity;

struct _monitor {
  int id;
  char *desc;
  Screen *screen;
  int x, y, width, height;
  monitor_sanity sanity;	/* I'm not crazy you're the one who's crazy */
  int enemy;			/* which monitor it overlaps or duplicates */
  char *err;			/* msg to print at appropriate later time;
                                   exists only on monitor #0. */
};

extern monitor **scan_monitors (Display *);
extern Bool monitor_layouts_differ_p (monitor **a, monitor **b);
extern void free_monitors (monitor **monitors);
extern void describe_monitor_layout (monitor **monitors);
extern void check_monitor_sanity (monitor **monitors);

#endif /* __XSCREENSAVER_SCREENS_H__ */
