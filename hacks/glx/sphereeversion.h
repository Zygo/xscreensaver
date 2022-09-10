/* Copyright (c) 2021-2022 Carsten Steger <carsten@mirsanmir.org>. */

/*
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 */

/* This header makes outsidein be a mode of sphereeversion rather than
   a standalone hack, since they are so similar in effect (if not
   implementation).
 */

#ifndef __SPHEREEVERSION_H__
#define __SPHEREEVERSION_H__


#define M_PI_F 3.14159265359f


/* For some strange reason, the color buffer must be initialized
   and used on macOS. Otherwise one- and two-sided lighting will
   not work. */
#if (defined(HAVE_COCOA) || defined(__APPLE__)) && !defined(HAVE_IPHONE)
#define VERTEXATTRIBARRAY_WORKAROUND
#endif


#define EVERSION_ANALYTIC          0
#define EVERSION_CORRUGATIONS      1
#define NUM_EVERSIONS              2

#define DISP_SURFACE               0
#define DISP_TRANSPARENT           1
#define NUM_DISPLAY_MODES          2

#define APPEARANCE_SOLID           0
#define APPEARANCE_PARALLEL_BANDS  1
#define APPEARANCE_MERIDIAN_BANDS  2
#define NUM_APPEARANCES            3

#define COLORS_TWOSIDED            0
#define COLORS_PARALLEL            1
#define COLORS_MERIDIAN            2
#define COLORS_EARTH               3
#define NUM_COLORS                 4

#define DISP_PERSPECTIVE           0
#define DISP_ORTHOGRAPHIC          1
#define NUM_DISP_MODES             2

/* Animation states */
#define ANIM_DEFORM 0
#define ANIM_TURN   1

/* Angle of a single turn step */
#define TURN_STEP 1.0


#ifdef USE_GL

#include "glsl-utils.h"
#include "gltrackball.h"


typedef struct {
  GLint WindH, WindW;
  GLXContext *glx_context;
  /* Options */
  int eversion_method;
  int display_mode[2];
  Bool random_display_mode;
  int appearance[2];
  Bool random_appearance;
  int colors[2];
  Bool random_colors;
  int projection;
  /* The next two parameters are used for the analytic eversion only */
  Bool graticule[2];
  Bool random_graticule;
  /* The next two parameters are used for the corrugations eversion only */
  int num_hemispheres;
  int strip_step;
  /* 3D rotation angles */
  float alpha, beta, delta;
  /* Animation state */
  int anim_state;
  /* Deformation parameter for the analytic eversion */
  float tau;
  /* Deformation parameter for the corrugations eversion */
  float time;
  int defdir;
  /* Turning parameters */
  int turn_step;
  int num_turn;
  float qs[4], qe[4];
  /* Two global shape parameters of the analytic sphere eversion */
  float eta_min, beta_max;
  /* The order of the analytic sphere eversion */
  int g;
  Bool random_g;
  /* The viewing offset in 3d */
  float offset3d[3];
  /* The 3d coordinates of the surface and the corresponding normal vectors */
  float *sp;
  float *sn;
  /* The precomputed colors of the surface */
  float *colf[2];
  float *colb[2];
  /* Aspect ratio of the current window */
  float aspect;
  /* Trackball states */
  trackball_state *trackball;
  Bool button_pressed;
  /* A random factor to modify the rotation speeds */
  float speed_scale;
#ifdef HAVE_GLSL
  /* The precomputed texture coordinates of the sphere */
  float *tex;
  /* The textures of earth by day (index 0), earth by night (index 1), and
     the water mask (index 2) */
  GLuint tex_names[3];
  /* Indices for uniform variables and attributes */
  GLuint *solid_indices, *parallel_indices;
  GLuint *meridian_indices, *line_indices;
  Bool use_shaders, use_mipmaps, buffers_initialized;
  GLuint poly_shader_program;
  GLint poly_pos_index, poly_normal_index;
  GLint poly_colorf_index, poly_colorb_index, poly_vertex_tex_index;
  GLint poly_mv_index, poly_proj_index;
  GLint poly_glbl_ambient_index, poly_lt_ambient_index;
  GLint poly_lt_diffuse_index, poly_lt_specular_index;
  GLint poly_lt_direction_index, poly_lt_halfvect_index;
  GLint poly_front_ambient_index, poly_back_ambient_index;
  GLint poly_front_diffuse_index, poly_back_diffuse_index;
  GLint poly_specular_index, poly_shininess_index;
  GLint poly_tex_samp_f_index, poly_tex_samp_b_index;
  GLint poly_tex_samp_w_index, poly_bool_textures_index;
  GLuint line_shader_program;
  GLint line_pos_index, line_color_index;
  GLint line_mv_index, line_proj_index;
  GLint max_tex_size;
  GLuint color_textures[2];
  GLuint blend_shader_program;
  GLint blend_vertex_p_index, blend_vertex_t_index;
  GLint blend_t_index, blend_sampler0_index, blend_sampler1_index;
  GLuint vertex_pos_buffer, vertex_normal_buffer;
  GLuint vertex_colorf_buffer[2], vertex_colorb_buffer[2];
  GLuint vertex_tex_coord_buffer;
  GLuint solid_indices_buffer, parallel_indices_buffer;
  GLuint meridian_indices_buffer, line_indices_buffer;
  GLint num_solid_strips, num_solid_triangles;
  GLint num_parallel_strips, num_parallel_triangles;
  GLint num_meridian_strips, num_meridian_triangles;
  GLint num_lines;
#endif /* HAVE_GLSL */
} sphereeversionstruct;

extern sphereeversionstruct *sphereeversion;


extern char *eversion_method;
extern char *mode;
extern char *appear;
extern char *color_mode;
extern char *graticule;
extern char *proj;
extern float speed_x;
extern float speed_y;
extern float speed_z;
extern float deform_speed;
extern char *surface_order;
extern char *lunes;
extern char *hemispheres;


extern void rotatex(float m[3][3], float phi);
extern void rotatey(float m[3][3], float phi);
extern void rotatez(float m[3][3], float phi);
extern void rotateall(float al, float be, float de, float m[3][3]);
extern void mult_rotmat(float m[3][3], float n[3][3], float o[3][3]);
extern void quat_to_angles(float q[4], float *alpha, float *beta,
                           float *delta);
extern void angles_to_quat(float alpha, float beta, float delta, float p[4]);
extern void quat_slerp(float t, float qs[4], float qe[4], float q[4]);
extern void color(sphereeversionstruct *se, float angle, const float mat[3][3],
                  float colf[4], float colb[4]);
extern void setup_xpm_texture(ModeInfo *mi, const unsigned char *data,
                              unsigned long size);
extern void gen_textures(ModeInfo *mi);
extern void init_glsl(ModeInfo *mi);


extern void init_sphereeversion_analytic(ModeInfo *mi);
extern void init_sphereeversion_corrugations(ModeInfo *mi);
extern void display_sphereeversion_analytic(ModeInfo *mi);
extern void display_sphereeversion_corrugations(ModeInfo *mi);

#endif /* USE_GL */


#endif /* __SPHEREEVERSION_H__ */
