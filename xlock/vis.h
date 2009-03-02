extern Bool setupColormap(ModeInfo * mi, int *colors, Bool * truecolor,
  unsigned long *redmask, unsigned long *bluemask, unsigned long *greenmask);
extern int  visualClassFromName(char *name);
extern char *nameOfVisualClass(int visualclass);

extern Bool fixedColors(ModeInfo * mi);

#ifdef USE_GL
#include <GL/gl.h>
#include <GL/glx.h>

extern GLXContext *init_GL(ModeInfo * mi);
extern void FreeAllGL(ModeInfo * mi);

#endif

#ifdef OPENGL_MESA_INCLUDES
/* Allow OPEN GL using MESA includes */
#undef MESA
#endif
