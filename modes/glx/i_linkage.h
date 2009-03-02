#include <GL/gl.h>
#include <GL/glx.h>

typedef struct {
  GLfloat     view_rotx, view_roty, view_rotz;
  GLXContext *glx_context;
  GLuint      frames;
  int         numsteps;
  int         construction;
  int         time;
  char       *partlist;
  int         forwards;
  Window      window;
} spherestruct;


#ifdef __cplusplus
extern "C" {
#endif
  Bool invert_draw(spherestruct *gp);
#ifdef __cplusplus
}
#endif
