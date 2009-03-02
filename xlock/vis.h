#ifdef __cplusplus
  extern "C" {
#endif
extern void defaultVisualInfo(Display * display, int screen);
extern Bool setupColormap(ModeInfo * mi, int *colors, Bool * truecolor,
  unsigned long *redmask, unsigned long *bluemask, unsigned long *greenmask);
extern int  visualClassFromName(char *name);
extern char *nameOfVisualClass(int visualclass);
extern Bool has_writable_cells(ModeInfo * mi);
extern Bool fixedColors(ModeInfo * mi);
#ifdef __cplusplus
  }
#endif

#ifdef USE_GL
#include <GL/gl.h>
#include <GL/glx.h>

#ifdef __cplusplus
  extern "C" {
#endif
extern GLXContext *init_GL(ModeInfo * mi);
extern void FreeAllGL(ModeInfo * mi);
#ifdef __cplusplus
  }
#endif

#endif

#ifdef OPENGL_MESA_INCLUDES
/* Allow OPEN GL using MESA includes */
#undef MESA
#endif
