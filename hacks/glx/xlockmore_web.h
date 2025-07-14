/* xlockmore_web.h - Minimal xlockmore.h for Web Builds
 *
 * This provides the minimal definitions needed by xscreensaver hacks
 * when building for the web, avoiding the need for the full X11 headers.
 */

#ifndef XLOCKMORE_WEB_H
#define XLOCKMORE_WEB_H

#include "jwxyz.h"

// Essential macros and definitions
#define ENTRYPOINT
#define XSCREENSAVER_MODULE(name, func)

// Common xscreensaver types and macros
typedef struct {
    int count;
    int cycles;
    int size;
    int ncolors;
    int fps_p;
    void *fpst;  // fps_state*
} ModeInfo;

// Common function declarations
extern void do_fps(ModeInfo *mi);
extern void screenhack_event_helper(void *display, void *window, void *event);

// Common utility functions
extern double frand(double max);
extern int random(void);

// Common constants
#define XK_Up        0xFF52
#define XK_Down      0xFF54
#define XK_Left      0xFF51
#define XK_Right     0xFF53
#define XK_Prior     0xFF55
#define XK_Next      0xFF56

#endif /* XLOCKMORE_WEB_H */ 