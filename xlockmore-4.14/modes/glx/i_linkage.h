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
} spherestruct;


#ifdef __cplusplus
extern "C" {
#endif
  void invert_draw(spherestruct *gp);
#ifdef __cplusplus
}
#endif
