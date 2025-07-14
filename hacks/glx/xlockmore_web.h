/* xlockmore_web.h - Minimal xlockmore.h for Web Builds
 *
 * This provides the minimal definitions needed by xscreensaver hacks
 * when building for the web, avoiding the need for the full X11 headers.
 */

#ifndef XLOCKMORE_WEB_H
#define XLOCKMORE_WEB_H

#include "jwxyz.h"
#include <math.h>

// Essential macros and definitions
#define ENTRYPOINT
#define XSCREENSAVER_MODULE(name, func)

// Common xscreensaver types and macros
// ModeInfo is already defined in xscreensaver_web.c

// Common function declarations
extern void do_fps(ModeInfo *mi);
extern void screenhack_event_helper(void *display, void *window, void *event);
extern GLXContext *init_GL(ModeInfo *mi);

// Common utility functions
extern double frand(double max);
// random() is already defined in stdlib.h

// Missing types and macros
// XrmOptionDescRec is already defined in jwxyz.h

typedef struct {
    void *var;
    char *name;
    char *desc;
    char *def;
    int type;
} argtype;

typedef struct {
    int count;
    XrmOptionDescRec *opts;
    int vars_count;
    argtype *vars;
    char *desc;
} ModeSpecOpt;

// Type constants
#define t_Bool 1
#define t_Float 2

// Utility macros
#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))

// Missing ModeInfo macros
#define MI_IS_WIREFRAME(mi) 0
#define MI_WINDOW(mi) ((mi)->window)
#define MI_DISPLAY(mi) ((mi)->display)
#define MI_VISUAL(mi) ((mi)->visual)
#define MI_COLORMAP(mi) ((mi)->colormap)

// Common constants
#define XK_Up        0xFF52
#define XK_Down      0xFF54
#define XK_Left      0xFF51
#define XK_Right     0xFF53
#define XK_Prior     0xFF55
#define XK_Next      0xFF56

#endif /* XLOCKMORE_WEB_H */ 