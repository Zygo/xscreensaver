/* xshadertoy, Copyright © 2025-2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Run arbitrary GLSL programs that use the shadertoy.com API.
 * Usage:
 *
 *   xshadertoy --program FILE.glsl
 *
 *   xshadertoy --program-common FILE.glsl	\
 *		--program0 FILE0.glsl		\
 *		--program1 FILE1.glsl		\
 *		--program2 FILE2.glsl		\
 *		--program3 FILE3.glsl		\
 *		--program4 FILE4.glsl		\
 *		--speed 2.0			\
 *		--scale 0.5			\
 *		--automouse			\
 *
 *   It is not possible to have this program load the code directly from
 *   shadertoy.com due to their scraping countermeasures, so you must
 *   download the GLSL files manually.  The "xshadertoy-download.pl"
 *   script makes that a little easier, but not much easier.
 *
 *   Does not support texture or other input assets; the programs must
 *   be self-contained.
 *
 *   Multi-pass rendering is supported, but only in the default order:
 *   e.g. program3 gets program2's output on iChannel2 and can't bind
 *   iChannel2 to a different pass instead.  Likewise, self-access to
 *   previously-generated frames is not supported.
 *
 *
 * LICENSING:
 *
 *   The default license on shadertoy.com is "CC BY-NC-SA", and so the vast
 *   majority of uploads use that.  However, that license, by prohibiting
 *   commercial use, is not an open source license, and is more restrictive
 *   than the MIT/BSD-style license used by the rest of XScreenSaver.
 *
 *   For a Shadertoy program to be distributed with or within XScreenSaver,
 *   it must be licensed under a compatible license, such as: MIT, BSD,
 *   CC-BY, CC-BY-SA or CC0.
 *
 *   Also: The Shadertoy community has a strong culture of remixing, so
 *   when evaluating the license terms of a program, any programs that
 *   it was derived from must also have compatible terms.
 *
 *
 * CONTROL FLOW:
 *
 *   The naming convention for Shadertoy-based XScreenSaver modules is
 *   that the GLSL files have the same basename as the hack itself,
 *   plus an optional suffix ("HACK-[c0-4].glsl").  Each gets its own
 *   config/HACK.xml and .man as usual.
 *
 *   X11:
 *     /usr/libexec/xscreensaver/HACK is a bash script that invokes
 *     "xshadertoy" with the GLSL code on stdin.  It uses "exec -a" so
 *     that argv0 / progname = "HACK" and progclass = "XShadertoy".
 *     Makefile.in rules build these via xshadertoy-compile.pl.
 *
 *   macOS Cocoa:
 *     The HACK.saver target compiles xshadertoy.c and XScreenSaverView.c
 *     and includes the .glsl files as assets.
 *
 *     As usual, XScreenSaverView compiles to class XScreenSaverHACKView,
 *     but it is also compiled with -DUSE_SHADERTOY=1 which causes it
 *     to use "xshadertoy_xscreensaver_function_table" instead of
 *     "HACK_xscreensaver_function_table".  It then changes progclass
 *     from "XShadertoy" back to "HACK".
 *
 *     This file, xshadertoy.c, searches in $BUNDLE_RESPATH (which is
 *     "HACK.saver/Contents/Resources/") for the .glsl files and loads
 *     them as normal files.
 *
 *   iOS Cocoa:
 *     Same as macOS except that instead of -DUSE_SHADERTOY, the
 *     ios-function-table.m maps "xshadertoy_xscreensaver_function_table".
 *
 *   Android:
 *     The .glsl files are copied into xscreensaver/assets/glsl/ in the
 *     APK by check-configs.pl.  Similarly to iOS, function-table.h maps
 *     "xshadertoy_xscreensaver_function_table".  This file loads them
 *     with android_read_asset_file() instead of with fopen().
 */

#define DEFAULTS	"*delay:		20000	\n" \
			"*showFPS:		False	\n" \
			"*prefersGLSL:		True	\n" \
			"*forceSingleSample:	True	\n" \

# define release_xshadertoy 0

#include "xlockmore.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#include "doubletime.h"
#include "glsl-utils.h"
#include "rotator.h"

#define DEF_SPEED "1.0"
#define DEF_AUTOMOUSE "False"
#define DEF_AUTOMOUSE_SPEED "1.0"
#define DEF_SCALE "1.0"
#define DEF_SHADER_FILE0 "(none)"
#define DEF_SHADER_FILE1 "(none)"
#define DEF_SHADER_FILE2 "(none)"
#define DEF_SHADER_FILE3 "(none)"
#define DEF_SHADER_FILE4 "(none)"
#define DEF_SHADER_FILE_COMMON "(none)"

static GLfloat speed;
static GLfloat scale;
static Bool automouse_p;
static GLfloat automouse_speed;
static char *shader_file0 = 0;
static char *shader_file1 = 0;
static char *shader_file2 = 0;
static char *shader_file3 = 0;
static char *shader_file4 = 0;
static char *shader_file_common = 0;

typedef struct {
  GLXContext *glx_context;

  GLint gl_major, gl_minor, glsl_major, glsl_minor;
  GLboolean gl_gles3;

  char *shader_program[6];		/* BufferA, B, C, D, Image, Common */

  GLuint vertex_pos_buffer;
  GLuint win_draw_fbo, win_read_fbo;
  GLuint vao;
  struct {
    GLuint poly_shader_program;
    GLuint fbo, tex, draw_fbo, read_fbo;
    GLint poly_pos;
    GLint frag_irez;
    GLint frag_itime;
    GLint frag_idelta;
    GLint frag_ifps;
    GLint frag_iframe;
    GLint frag_idate;
    GLint frag_imouse;
    GLint frag_ichan[4];
  } channels[5];

  double start_time, last_time, last_tm, midnight;
  struct tm now;
  unsigned long total_frames;
  struct { int x, y; } mouse_clicked, mouse_dragged;
  Bool button_down_p, was_button_down_p;
  rotator *rot;

} xshadertoy_configuration;

static xshadertoy_configuration *bps = NULL;

static XrmOptionDescRec opts[] = {
  {"-speed",          ".speed",          XrmoptionSepArg, 0 },
  {"-scale",          ".scale",          XrmoptionSepArg, 0 },
  {"-automouse",      ".automouse",      XrmoptionNoArg, "True"  },
  {"+automouse",      ".automouse",      XrmoptionNoArg, "False" },
  {"-automouse-speed",".automouseSpeed", XrmoptionSepArg, 0 },
  {"-program",        ".shaderChannel0", XrmoptionSepArg, 0 },
  {"-program-common", ".shaderCommon",   XrmoptionSepArg, 0 },
  {"-program0",       ".shaderChannel0", XrmoptionSepArg, 0 },
  {"-program1",       ".shaderChannel1", XrmoptionSepArg, 0 },
  {"-program2",       ".shaderChannel2", XrmoptionSepArg, 0 },
  {"-program3",       ".shaderChannel3", XrmoptionSepArg, 0 },
  {"-program4",       ".shaderChannel4", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  { &speed, "speed", "Speed", DEF_SPEED, t_Float },
  { &scale, "scale", "Scale", DEF_SCALE, t_Float },
  { &automouse_p, "automouse", "Automouse", DEF_AUTOMOUSE, t_Bool },
  { &automouse_speed, "automouseSpeed", "Speed", DEF_AUTOMOUSE_SPEED, t_Float },
  { &shader_file0, "shaderChannel0", "ShaderChannel",
      DEF_SHADER_FILE0, t_String },
  { &shader_file1, "shaderChannel1", "ShaderChannel",
      DEF_SHADER_FILE1, t_String },
  { &shader_file2, "shaderChannel2", "ShaderChannel",
      DEF_SHADER_FILE2, t_String },
  { &shader_file3, "shaderChannel3", "ShaderChannel",
      DEF_SHADER_FILE3, t_String },
  { &shader_file4, "shaderChannel4", "ShaderChannel",
      DEF_SHADER_FILE4, t_String },
  { &shader_file_common, "shaderCommon", "ShaderChannel",
      DEF_SHADER_FILE_COMMON, t_String },
};

ENTRYPOINT ModeSpecOpt xshadertoy_opts = {
  countof(opts), opts, countof(vars), vars, NULL};

#ifdef HAVE_GLSL

static const GLchar *vertex_shader = "\
	#ifdef GL_ES							\n\
	  precision highp float;					\n\
	  precision highp int;						\n\
	#endif								\n\
									\n\
	#if __VERSION__ <= 120						\n\
	  attribute vec2 a_Position;					\n\
	#else								\n\
	  in vec2 a_Position;						\n\
	#endif								\n\
									\n\
	void main() {							\n\
	  gl_Position = vec4 (a_Position.xy, 0.0, 1.0);			\n\
	}								\n\
";

/* Docs:
   https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.1.20.pdf
   https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.1.30.pdf
   https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.1.50.pdf
   https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.1.40.pdf
   https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.00.pdf
 */

static const GLchar *fragment_shader_head = "\
	#ifdef GL_ES							\n\
	  precision highp float;					\n\
	  precision highp int;						\n\
	#endif								\n\
									\n\
	#if __VERSION__ >= 130						\n\
	  out vec4 frag_color;						\n\
        #endif								\n\
									\n\
	uniform vec3  iResolution;  // viewport resolution (in pixels)	\n\
	uniform float iTime;	    // shader playback time (in secs)	\n\
	uniform float iTimeDelta;   // render time (in secs)		\n\
	uniform float iFrameRate;   // shader FPS			\n\
	uniform int   iFrame;	    // shader frame number		\n\
	uniform vec4  iDate;	    // (Y, M, D, secs since midnight)	\n\
	uniform vec4  iMouse;	    // mouse pos (see below)		\n\
									\n\
	uniform vec3  iChannelResolution[4];	// Texture sizes	\n\
									\n\
	// These are for audio:						\n\
	uniform float iChannelTime[4];					\n\
	uniform float iSampleRate;					\n\
									\n\
	// On the web version, these might be of type sampler3D or	\n\
	// something else, depending on what input was selected in	\n\
	// then menu.  Here, we only support 2D textures because we	\n\
	// only allow input from the output of the previous pass.	\n\
	//								\n\
	uniform sampler2D iChannel0;					\n\
	uniform sampler2D iChannel1;					\n\
	uniform sampler2D iChannel2;					\n\
	uniform sampler2D iChannel3;					\n\
									\n\
									\n\
	// These library functions existed in GLSL 1.2 but were		\n\
	// removed in GLSL 1.3; avoid name conflicts if programs	\n\
	// happen to use these names for their own functions.		\n\
	//								\n\
	# define noise  xshadertoy_noise				\n\
	# define noise2 xshadertoy_noise2				\n\
									\n\
									\n\
	#if __VERSION__ <= 120						\n\
									\n\
	// The ivec and uvec types were introduced in GLSL 1.3.		\n\
	#define ivec2 vec2						\n\
	#define ivec3 vec3						\n\
	#define ivec4 vec4						\n\
	#define uvec2 vec2						\n\
	#define uvec3 vec3						\n\
	#define uvec4 vec4						\n\
	#define uint  int						\n\
									\n\
	// The texture2D() function was renamed texture() in GLSL 1.3.	\n\
	// (Deprecated in GLSL 1.3, removed in GLSL 1.5.)		\n\
	//								\n\
	vec4 texture (sampler2D sampler, vec2 coord) {			\n\
	  return texture2D (sampler, coord);				\n\
	}								\n\
        vec4 texture (sampler2D sampler, vec3 coord) {			\n\
          return texture2D (sampler, coord.xy);				\n\
        }								\n\
	vec4 texture (sampler2D sampler, vec2 coord, float bias) {	\n\
	  return texture2D (sampler, coord, bias);			\n\
	}								\n\
	vec4 texture (sampler2D sampler, vec3 coord, float bias) {	\n\
	  return texture2D (sampler, coord.xy, bias);			\n\
	}								\n\
									\n\
	vec4 texelFetch(sampler2D sampler, ivec2 coord, int lod) {	\n\
	  return texture (sampler, (coord + 0.5) / iResolution.xy);	\n\
	}								\n\
	vec4 texelFetch(sampler2D sampler, ivec3 coord, int lod) {	\n\
	  return texture (sampler, (coord + 0.5) / iResolution);	\n\
	}								\n\
									\n\
	// Hyperbolic functions were added in GLSL 1.3.			\n\
	//								\n\
	float sinh(in float i) {					\n\
	  // return (exp(i) - exp(-i)) / 2;				\n\
	  float j = exp(i);						\n\
	  return (j - 1 / j) / 2;					\n\
	}								\n\
	float cosh(in float i) {					\n\
	  // return (exp(i) + exp(-i)) / 2;				\n\
	  float j = exp(i);						\n\
	  return (j + 1 / j) / 2;					\n\
	}								\n\
	float tanh(in float i) {					\n\
          // return sinh(i) / cosh(i);					\n\
	  return (2 / (1 + exp(-2 * i))) - 1;				\n\
	}								\n\
	vec2 sinh(in vec2 v) { return vec2(sinh(v.x), sinh(v.y)); }	\n\
	vec2 cosh(in vec2 v) { return vec2(cosh(v.x), cosh(v.y)); }	\n\
	vec2 tanh(in vec2 v) { return vec2(tanh(v.x), tanh(v.y)); }	\n\
									\n\
	vec3 sinh(in vec3 v) {						\n\
	  return vec3(sinh(v.x), sinh(v.y), sinh(v.z));			\n\
	}								\n\
	vec3 cosh(vec3 v) {						\n\
	  return vec3(cosh(v.x), cosh(v.y), cosh(v.z));			\n\
	}								\n\
	vec3 tanh(vec3 v) {						\n\
	  return vec3(tanh(v.x), tanh(v.y), tanh(v.z));			\n\
	}								\n\
									\n\
	vec4 sinh(in vec4 v) {						\n\
	  return vec4(sinh(v.x), sinh(v.y), sinh(v.z), sinh(v.w));	\n\
	}								\n\
	vec4 cosh(vec4 v) {						\n\
	  return vec4(cosh(v.x), cosh(v.y), cosh(v.z), cosh(v.w));	\n\
	}								\n\
	vec4 tanh(vec4 v) {						\n\
	  return vec4(tanh(v.x), tanh(v.y), tanh(v.z), tanh(v.w));	\n\
	}								\n\
									\n\
	// The modern concept of rounding was added in GLSL 1.3.	\n\
	//								\n\
	int round(float f) { return int(f + 0.5); }			\n\
	ivec2 round(vec2 v) { return ivec2(round(v.x), round(v.y)); }	\n\
	ivec3 round(vec3 v) {						\n\
	  return ivec3(round(v.x), round(v.y), round(v.z));		\n\
	}								\n\
	ivec4 round(vec4 v) {						\n\
	  return ivec4(round(v.x), round(v.y), round(v.z), round(v.w));	\n\
	}								\n\
									\n\
	// I think the type propagation rules changed?			\n\
	// This makes 'int i = max(...)' work.				\n\
	int max(int a, int b) { return a > b ? a : b; }			\n\
        // I hate that GLSL's method-selection signatures include	\n\
	// arg types but not return types, yet require them to match,	\n\
	// so these conflict:						\n\
        // int max(int a, float b) { return a > b ? a : int(b); }	\n\
        // int max(float a, int b) { return a > b ? int(a) : b; }	\n\
									\n\
	#endif   // __VERSION__ > 120					\n\
									\n\
	#line 1								\n\
";
static const char *fragment_shader_tail = "\
									\n\
	void main() {							\n\
	  vec4 col = vec4 (0.0, 0.0, 0.0, 1.0);				\n\
	  mainImage (col, gl_FragCoord.xy);				\n\
	#if __VERSION__ <= 120						\n\
	  gl_FragColor = col;						\n\
	#else								\n\
	  frag_color = col;						\n\
        #endif								\n\
	}								\n\
";

#endif /* HAVE_GLSL */


static void
gen_framebuffers (ModeInfo *mi)
{
  xshadertoy_configuration *bp = &bps[MI_SCREEN(mi)];
  GLint internal_format, format, type;
  int i;

  if (!bp->gl_gles3)
    {
      internal_format = GL_RGBA;
      format = GL_RGBA;
      type = GL_FLOAT;
    }
  else
    {
      if (bp->gl_major == 3 && bp->gl_minor >= 2)
        {
          internal_format = GL_RGBA16F;
          format = GL_RGBA;
          type = GL_HALF_FLOAT;
        }
      else
        {
          internal_format = GL_RGBA8;
          format = GL_RGBA;
          type = GL_UNSIGNED_BYTE;
        }
    }

  for (i = 0; i < countof(bp->channels); i++)
    {
      if (!bp->shader_program[i] || !*bp->shader_program[i])
        continue;

      glGenFramebuffers (1, &bp->channels[i].fbo);
      glGenTextures (1, &bp->channels[i].tex);
      glBindTexture (GL_TEXTURE_2D, bp->channels[i].tex);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D (GL_TEXTURE_2D, 0, internal_format,
                    MI_WIDTH(mi) * scale, MI_HEIGHT(mi) * scale,
                    0, format, type, 0);
    
      glBindFramebuffer (GL_FRAMEBUFFER, bp->channels[i].fbo);
      glFramebufferTexture2D (GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                              bp->channels[i].tex, 0);
      if (GL_FRAMEBUFFER_COMPLETE !=
          glCheckFramebufferStatus (GL_FRAMEBUFFER))
        {
          fprintf (stderr,
                   "%s: GLSL FBO failed for FBO %d \n", progname, i);
          exit (1);
        }

      bp->channels[i].draw_fbo = bp->channels[i].fbo;
      bp->channels[i].read_fbo = bp->channels[i].fbo;
    }
}


static void
delete_framebuffers (ModeInfo *mi)
{
  xshadertoy_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  for (i = 0; i < countof(bp->channels); i++)
    {
      if (bp->channels[i].fbo)
        glDeleteFramebuffers (1, &bp->channels[i].fbo);
      if (bp->channels[i].tex)
        glDeleteTextures (1, &bp->channels[i].tex);
    }
}


static void
init_glsl (ModeInfo *mi)
{
# ifndef HAVE_GLSL
  fprintf (stderr, "%s: GLSL is required.\n", progname);
  exit (1);
# else /* HAVE_GLSL */

  xshadertoy_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;

  if (!glsl_GetGlAndGlslVersions (&bp->gl_major, &bp->gl_minor,
                                  &bp->glsl_major, &bp->glsl_minor,
                                  &bp->gl_gles3))
    {
      fprintf (stderr, "%s: GLSL is required\n", progname);
      exit (1);
    }

  if (glsl_IsCoreProfile())
    glGenVertexArrays (1, &bp->vao);

  /* The FBOs of the output window, before channel FBOs were created. */
  glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, (GLint *) &bp->win_draw_fbo);
  glGetIntegerv (GL_READ_FRAMEBUFFER_BINDING, (GLint *) &bp->win_read_fbo);

  if (scale > 1 || scale <= 0)  /* Scale down only */
    scale = 1;

  for (i = 0; i < countof(bp->channels); i++)
    {
      const char *fn = (i == 0 ? shader_file0 :
                        i == 1 ? shader_file1 :
                        i == 2 ? shader_file2 :
                        i == 3 ? shader_file3 :
                        i == 4 ? shader_file4 : "???");
      GLuint p;
      const GLchar *vertex_shader_source[2];
      const GLchar *fragment_shader_source[6];
      const GLchar *common =
        bp->shader_program[countof(bp->shader_program) - 1];
      if (!common) common = "";

      if (!bp->shader_program[i] || !*bp->shader_program[i])
        continue;

      vertex_shader_source[0]   = glsl_GetGLSLVersionString();
      vertex_shader_source[1]   = vertex_shader;
      fragment_shader_source[0] = glsl_GetGLSLVersionString();
      fragment_shader_source[1] = fragment_shader_head;
      fragment_shader_source[2] = common;
      fragment_shader_source[3] = "\n#line 1\n";
      fragment_shader_source[4] = bp->shader_program[i];
      fragment_shader_source[5] = fragment_shader_tail;
      if (!glsl_CompileAndLinkShaders(
           countof(vertex_shader_source),   vertex_shader_source,
           countof(fragment_shader_source), fragment_shader_source,
           &bp->channels[i].poly_shader_program))
        {
          fprintf (stderr,
                   "%s: GLSL compilation of --program%d \"%s\" failed\n",
                   progname, i, fn);
          exit (1);
        }

      /* These are -1 if names are not declared in the shader program,
         or if the program does not use them. */
      p = bp->channels[i].poly_shader_program;
      bp->channels[i].poly_pos    = glGetAttribLocation  (p, "a_Position");
      bp->channels[i].frag_irez   = glGetUniformLocation (p, "iResolution");
      bp->channels[i].frag_itime  = glGetUniformLocation (p, "iTime");
      bp->channels[i].frag_idelta = glGetUniformLocation (p, "iTimeDelta");
      bp->channels[i].frag_ifps   = glGetUniformLocation (p, "iFrameRate");
      bp->channels[i].frag_iframe = glGetUniformLocation (p, "iFrame");
      bp->channels[i].frag_imouse = glGetUniformLocation (p, "iMouse");
      bp->channels[i].frag_idate  = glGetUniformLocation (p, "iDate");
      bp->channels[i].frag_ichan[0] = glGetUniformLocation (p, "iChannel0");
      bp->channels[i].frag_ichan[1] = glGetUniformLocation (p, "iChannel1");
      bp->channels[i].frag_ichan[2] = glGetUniformLocation (p, "iChannel2");
      bp->channels[i].frag_ichan[3] = glGetUniformLocation (p, "iChannel3");
    }

  gen_framebuffers (mi);

  {
    static const GLfloat verts[] = {
      -1, -1,
       1, -1,
      -1,  1,
      -1,  1,
       1, -1,
       1,  1,
    };
    glGenBuffers (1, &bp->vertex_pos_buffer);
    glBindBuffer (GL_ARRAY_BUFFER, bp->vertex_pos_buffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  }
# endif /* HAVE_GLSL */
}


ENTRYPOINT void
reshape_xshadertoy (ModeInfo *mi, int width, int height)
{
  /* Since this is GLES-only, the fixed function pipeline is not used. */
  glViewport (0, 0, (GLint) width, (GLint) height);

  /* Reallocate the framebuffers with the current window size. */
  delete_framebuffers (mi);
  gen_framebuffers (mi);
}


ENTRYPOINT Bool
xshadertoy_handle_event (ModeInfo *mi, XEvent *event)
{
  xshadertoy_configuration *bp = &bps[MI_SCREEN(mi)];
  if (event->xany.type == ButtonPress)
    {
      bp->button_down_p++;
      return True;
    }
  else if (event->xany.type == ButtonRelease)
    {
      if (bp->button_down_p) bp->button_down_p--;
      return True;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    return True;
  return False;
}


ENTRYPOINT void
init_xshadertoy (ModeInfo *mi)
{
  xshadertoy_configuration *bp;
  int i;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_xshadertoy (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  /* Pack the programs to the left, if they did --program0 --program2 */
  {
    char *p[countof(bp->channels)];
    memset (p, 0, sizeof(p));
    i = 0;
# define PUSH(S) if (S && *S && !!strcasecmp (S, DEF_SHADER_FILE0)) p[i++] = S
    PUSH (shader_file0);
    PUSH (shader_file1);
    PUSH (shader_file2);
    PUSH (shader_file3);
    PUSH (shader_file4);
# undef PUSH
    shader_file0 = p[0];
    shader_file1 = p[1];
    shader_file2 = p[2];
    shader_file3 = p[3];
    shader_file4 = p[4];
  }

# ifdef HAVE_COCOA
  /* When xshadertoy is run under a macOS Cocoa .saver bundle, we search
     in the $BUNDLE_RESPATH directory for any files whose names match
     "progname-N.glsl", etc.
   */
  {
    const char *dir = getenv ("BUNDLE_RESPATH");
    const char *ext = "glsl";
    char *name = strdup (progname);
    char *s;
    for (s = name; *s; s++) *s = tolower(*s);

    if (!dir)
      {
        fprintf (stderr, "%s: BUNDLE_RESPATH is unset\n", name);
        abort();
      }

    for (i = 0; i < countof(bp->shader_program); i++)
      {
        char *fn = (char *)
          malloc (strlen(dir) + strlen(name) + strlen(ext) + 20);
        struct stat st;

        if (i == countof(bp->shader_program) - 1)
          sprintf (fn, "%s/%s-%s.%s", dir, name, "c", ext);
        else
          sprintf (fn, "%s/%s-%d.%s", dir, name, i, ext);

        if (i == 0 && !!stat (fn, &st))
          sprintf (fn, "%s/%s.%s", dir, name, ext);

        if (!stat (fn, &st))
          switch (i) {
          case 0: shader_file0 = strdup (fn); break;
          case 1: shader_file1 = strdup (fn); break;
          case 2: shader_file2 = strdup (fn); break;
          case 3: shader_file3 = strdup (fn); break;
          case 4: shader_file4 = strdup (fn); break;
          case 5: shader_file_common = strdup (fn); break;
          default: abort(); break;
          }
        free (fn);
      }
    free (name);
  }
# elif defined(HAVE_ANDROID)
  /* Under Android, search for any files whose names match "progname-N.glsl",
     except that these pseudo-files are "assets" instead.
   */
  {
    const char *ext = "glsl";
    const char *dir = ext;
    char *name = strdup (progname);
    char *s;
    for (s = name; *s; s++) *s = tolower(*s);

    for (i = 0; i < countof(bp->shader_program); i++)
      {
        char *body;
        char *fn = (char *) malloc (strlen(name) + strlen(ext) + 20);
  
        if (i == countof(bp->shader_program) - 1)
          sprintf (fn, "%s-c.%s", name, ext);
        else
          sprintf (fn, "%s-%d.%s", name, i, ext);
  
        body = android_read_asset_file (MI_WINDOW(mi), dir, fn);
        if (i == 0 && !body)
          {
            sprintf (fn, "%s.%s", name, ext);
            body = android_read_asset_file (MI_WINDOW(mi), dir, fn);
          }
  
        if (body)
          {
            bp->shader_program[i] = body;
            switch (i) {
            case 0: shader_file0 = strdup (fn); break;
            case 1: shader_file1 = strdup (fn); break;
            case 2: shader_file2 = strdup (fn); break;
            case 3: shader_file3 = strdup (fn); break;
            case 4: shader_file4 = strdup (fn); break;
            case 5: shader_file_common = strdup (fn); break;
            default: abort(); break;
            }
          }
        free (fn);
      }
    free (name);
  }
# endif /* HAVE_ANDROID */


# ifndef HAVE_ANDROID /* X11, Cocoa and iOS read files with fopen. */
  {
    int j;
    FILE *in = 0, *stdin = 0;
    Bool nl_p = True;
    int order[] = { 5, 0, 1, 2, 3, 4 };    /* Read "common" first on stdin */

    for (j = 0; j < countof (bp->shader_program); j++)
      {
        int i = order[j];
        const char *fn = (i == 0 ? shader_file0 :
                          i == 1 ? shader_file1 :
                          i == 2 ? shader_file2 :
                          i == 3 ? shader_file3 :
                          i == 4 ? shader_file4 :
                          i == 5 ? shader_file_common : 0);

        char buf[255];
        int res;
        int len = 0;
        int size = 0;

        if (!fn || !*fn || !strcasecmp (fn, DEF_SHADER_FILE0))
          continue;
        else if (!!strcmp (fn, "-"))
          in = fopen (fn, "r");
        else
          {
            in = (stdin ? stdin : fdopen (STDIN_FILENO, "r"));
            stdin = in;
          }

        if (!in)
          {
            sprintf (buf, "%s: error reading \"%.200s\"", progname, fn);
            perror (buf);
            exit (1);
          }

        bp->shader_program[i] = NULL;

        while (fgets (buf, sizeof(buf)-1, in))
          {
            res = strlen (buf);
            if (nl_p && stdin &&
                !strcmp (buf, ".\n"))	/* "." alone on a line is EOF */
              break;
            nl_p = (res > 0 && buf[res-1] == '\n');
            if (len + res >= size)
              {
                size += sizeof(buf);
                bp->shader_program[i] =
                  (bp->shader_program[i]
                   ? realloc (bp->shader_program[i], size)
                   : malloc (size));
                if (!bp->shader_program[i]) abort();
              }
            memcpy (bp->shader_program[i] + len, buf, res);
            len += res;
            bp->shader_program[i][len] = 0;
          }

        if (!stdin)
          {
            fclose (in);
            in = 0;
          }

        if (!bp->shader_program[i] || !*bp->shader_program[i] ||
            strlen (bp->shader_program[i]) < 10)
          {
            fprintf (stderr, "%s: no program in \"%s\"\n", progname,
                     fn);
            exit (1);
          }
      }

    if (in)
      fclose (in);
  }

# endif /* !HAVE_ANDROID */

  if (!bp->shader_program[0] || !*bp->shader_program[0])
    {
      fprintf (stderr, "%s: no --program specified\n", progname);
      exit (1);
    }

  {
    double mouse_speed = 0.01 * speed * automouse_speed;
    bp->rot = make_rotator (0, 0, 0, 0, mouse_speed, False);
  }

  init_glsl (mi);

  bp->total_frames = 0;
  bp->start_time   = double_time();
  bp->last_time    = bp->start_time;
  {
    time_t t = bp->start_time;
    struct tm *tm = localtime (&t);
    bp->now = *tm;
  }
  
# ifndef HAVE_JWXYZ
  /* Put the name of this script in the window title, in case it was launched
     with "exec -a" to set argv[0] to something other than "xshadertoy".
   */
  if (!MI_WIN_IS_INROOT(mi))
    {
      char *s, *s1;
      XFetchName (MI_DISPLAY(mi), MI_WINDOW(mi), &s);
      s1 = s ? strchr (s, ':') : 0;
      if (s1)
        {
          const char *s2, *s3;
          *s1 = 0;
          s2 = s1+1;
          while (*s2 == ' ') s2++;
          s3 = strrchr (progname, '/');
          s3 = s3 ? s3+1 : progname;
          if (!!strcasecmp (progname, s))
            {
              char *s4 = (char *)
                malloc (strlen(s) + strlen(s2) + strlen(s3) + 10);
              sprintf (s4, "%s: %s: %s", s, s3, s2);
              XStoreName (MI_DISPLAY(mi), MI_WINDOW(mi), s4);
              free (s4);
            }
        }
      if (s) free (s);
    }
# endif /* HAVE_JWXYZ */

}


ENTRYPOINT void
draw_xshadertoy (ModeInfo *mi)
{
  xshadertoy_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  double now = double_time();
  int mx, my, mz, mw;
  int i;

  if (!bp->glx_context)
    return;

  now = bp->start_time + (now - bp->start_time) * speed;  /* Time warp */

  /* Shadertoy mouse input is, shall we say, idiosyncratic.  Test cases:
     https://www.shadertoy.com/view/Mss3zH
     https://www.shadertoy.com/view/MdXyzX */
  {
    int x, y;

    if (automouse_p)	/* Hold button forever and drag sinusoidally. */
      {
        double dx, dy, dz;
        get_position (bp->rot, &dx, &dy, &dz, True);
        x = dx * (MI_WIDTH(mi));
        y = dy * (MI_HEIGHT(mi));
        bp->button_down_p = True;
      }
    else
      {
        Window r, c;
        int rx, ry;
        unsigned int m;
        XQueryPointer (MI_DISPLAY (mi), MI_WINDOW (mi),
                       &r, &c, &rx, &ry, &x, &y, &m);
      }

    y = MI_HEIGHT(mi) - 1 - y;

    if (x < 1) x = 1;    /* 0 means "button not down" */
    if (y < 1) y = 1;
    if (x >= MI_WIDTH(mi))  x = MI_WIDTH(mi)  - 1;
    if (y >= MI_HEIGHT(mi)) y = MI_HEIGHT(mi) - 1;

    if (bp->button_down_p && !bp->was_button_down_p)	/* Drag beginning */
      {
        mx = x;
        my = y;
        mz = x;
        mw = y;
        bp->mouse_clicked.x = x;
        bp->mouse_clicked.y = y;
      }
    else if (bp->button_down_p)				/* Drag continuing */
      {
        mx =  x;
        my =  y;
        mz =  bp->mouse_clicked.x;
        mw = -bp->mouse_clicked.y;
        bp->mouse_dragged.x = x;
        bp->mouse_dragged.y = y;
      }
    else if (!bp->button_down_p && bp->was_button_down_p) /* Drag released */
      {
        mx =  x;
        my =  y;
        mz = -bp->mouse_clicked.x;
        mw = -bp->mouse_clicked.y;
        bp->mouse_dragged.x = x;
        bp->mouse_dragged.y = y;
      }
    else						/* Not dragging */
      {
        mx =  bp->mouse_dragged.x;
        my =  bp->mouse_dragged.y;
        mz = -bp->mouse_clicked.x;
        mw = -bp->mouse_clicked.y;
      }

    mx *= scale;
    my *= scale;
    mz *= scale;
    mw *= scale;

    bp->was_button_down_p = bp->button_down_p;
  }

  /* Shadertoy keyboard input is insane: you have to feed it a texture map
     as input that has a bit set for the ASCII character of each key that is
     down.  So we're not doing that.  https://www.shadertoy.com/view/lsXGzf */

  /* "iDate" is year, month, day and fractional-seconds-since-midnight.
     Only recompute once a second, since localtime and mktime are a lot.
     Test case: https://www.shadertoy.com/view/lsXGz8 */
  if (now >= bp->last_tm + 1)
    {
      time_t t      = now;
      struct tm *tm = localtime (&t);
      bp->now       = *tm;
      bp->last_tm   = t;
      tm->tm_hour   = tm->tm_min = tm->tm_sec = 0;
      bp->midnight  = mktime (tm);
    }

  glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

# ifdef HAVE_GLSL

  for (i = 0; i < countof(bp->channels); i++)
    {
      Bool last_p = (i == countof(bp->channels) - 1 ||
                     !bp->shader_program[i + 1]);
      int j;

      if (!bp->channels[i].poly_shader_program)
        continue;

      glUseProgram (bp->channels[i].poly_shader_program);

      /* Bind to this layer's output buffer */
      glBindFramebuffer (GL_DRAW_FRAMEBUFFER, bp->channels[i].draw_fbo);
      glBindFramebuffer (GL_READ_FRAMEBUFFER, bp->channels[i].read_fbo);

      for (j = 0; j < countof(bp->channels); j++)
        {
          if (!bp->channels[j].tex ||
              bp->channels[i].frag_ichan[j] < 0)
            continue;
          glActiveTexture (GL_TEXTURE0 + j);
          glBindTexture (GL_TEXTURE_2D, bp->channels[j].tex);
          glUniform1i (bp->channels[i].frag_ichan[j], j);
        }

      glUniform3f (bp->channels[i].frag_irez,
                   MI_WIDTH(mi) * scale,
                   MI_HEIGHT(mi) * scale,
                   1);
      glUniform1f (bp->channels[i].frag_itime,  now - bp->start_time);
      glUniform1f (bp->channels[i].frag_idelta, now - bp->last_time);
      glUniform1f (bp->channels[i].frag_ifps,
                   bp->total_frames / (now - bp->start_time));
      glUniform1i (bp->channels[i].frag_iframe, bp->total_frames);
      glUniform4f (bp->channels[i].frag_imouse, mx, my, mz, mw);
      glUniform4f (bp->channels[i].frag_idate,
                   bp->now.tm_year + 1900,
                   bp->now.tm_mon + 1,
                   bp->now.tm_mday,
                   now - bp->midnight);

      if (bp->channels[i].poly_pos < 0) abort();
      if (bp->vao)
        glBindVertexArray (bp->vao);
      glBindBuffer (GL_ARRAY_BUFFER, bp->vertex_pos_buffer);
      glVertexAttribPointer (bp->channels[i].poly_pos, 2,
                             GL_FLOAT, GL_FALSE, 0, 0);
      glEnableVertexAttribArray (bp->channels[i].poly_pos);
      glDrawArrays (GL_TRIANGLES, 0, 6);
      glBindBuffer (GL_ARRAY_BUFFER, 0);
      if (bp->vao)
        glBindVertexArray (0);

      glUseProgram (0);

      if (last_p)
        break;
    }

  glBindFramebuffer (GL_READ_FRAMEBUFFER, bp->channels[i].draw_fbo);
  glBindFramebuffer (GL_DRAW_FRAMEBUFFER, bp->win_draw_fbo);

  glBlitFramebuffer (0, 0, MI_WIDTH(mi) * scale, MI_HEIGHT(mi) * scale,
                     0, 0, MI_WIDTH(mi), MI_HEIGHT(mi),
                     GL_COLOR_BUFFER_BIT, GL_LINEAR);

  glBindFramebuffer (GL_READ_FRAMEBUFFER, bp->win_read_fbo);

# endif /* HAVE_GLSL */

  bp->last_time = now;
  bp->total_frames++;

#ifndef HAVE_MOBILE  // #### texfont not working with GLSL
  if (mi->fps_p) do_fps (mi);
#endif
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_xshadertoy (ModeInfo *mi)
{
  xshadertoy_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  if (!bp->glx_context) return;
  for (i = 0; i < countof(bp->shader_program); i++)
    if (bp->shader_program[i]) free (bp->shader_program[i]);
  if (bp->rot) free_rotator (bp->rot);
# ifdef HAVE_GLSL
  glUseProgram (0);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  for (i = 0; i < countof(bp->channels); i++)
    {
      if (bp->channels[i].poly_shader_program)
        glDeleteProgram (bp->channels[i].poly_shader_program);
    }
  delete_framebuffers (mi);
  glDeleteBuffers (1, &bp->vertex_pos_buffer);
  if (bp->vao)
    glDeleteVertexArrays (1, &bp->vao);

# endif
}

XSCREENSAVER_MODULE ("XShadertoy", xshadertoy)

#endif /* USE_GL */
