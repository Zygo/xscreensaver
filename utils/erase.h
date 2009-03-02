/* erase.c: Erase the screen in various more or less interesting ways.
 * (c) 1997 by Johannes Keukelaar <johannes@nada.kth.se>
 * Permission to use in any way granted. Provided "as is" without expressed
 * or implied warranty. NO WARRANTY, NO EXPRESSION OF SUITABILITY FOR ANY
 * PURPOSE. (I.e.: Use in any way, but at your own risk!)
 */

#ifndef __XSCREENSAVER_ERASE_H__
#define __XSCREENSAVER_ERASE_H__

extern void erase_window(Display *dpy, Window window, GC gc,
			 int width, int height, int mode, int delay);
extern void erase_full_window(Display *dpy, Window window);

#endif /* __XSCREENSAVER_ERASE_H__ */
