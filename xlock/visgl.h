#ifndef __XLOCK_VISGL_H__
#define __XLOCK_VISGL_H__

#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)visgl.h	5.01 01/02/19 xlockmore" */

#endif

/*-
 * GL Visual stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 19-02-01: Started log. :)
 */

#ifdef USE_GL
#include <GL/gl.h>
#include <GL/glx.h>

#ifdef __cplusplus
  extern "C" {
#endif
extern GLXContext *init_GL(ModeInfo * mi);
extern void FreeAllGL(ModeInfo * mi);
extern void do_fps (ModeInfo *mi);
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

extern XVisualInfo *
getGLVisual(Display * display, int screen, XVisualInfo * wantVis, int monochrome);

#ifdef __cplusplus
  }
#endif

#endif

#ifdef OPENGL_MESA_INCLUDES
/* Allow OPEN GL using MESA includes */
#undef MESA
#endif

#endif /* __XLOCK_VISGL_H__ */
