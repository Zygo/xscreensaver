/* hopfanimations.c --- Definition of the animations used in the Hopf
   fibration. */
/* Copyright (c) 2025 Carsten Steger <carsten@mirsanmir.org>.
 *
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
 *
 * REVISION HISTORY:
 * C. Steger - 26/02/06: Initial version
 */

#ifdef USE_GL

#include "hopfanimations.h"


#define ANIM_SO_NAME(f) anim_ ## f ## _s
#define ANIM_SO_DEF(f) static animation_single_obj ANIM_SO_NAME(f)[]
#define ANIM_SO_NUM(f) sizeof(ANIM_SO_NAME(f))/sizeof(*ANIM_SO_NAME(f))
#define ANIM_MO_NAME(f) anim_ ## f ## _m
#define ANIM_MO_REF(f) &ANIM_MO_NAME(f)
#define ANIM_MO_DEF(f) static animation_multi_obj ANIM_MO_NAME(f)
#define ANIM_PH_NAME(f) anim_ ## f ## _p
#define ANIM_PH_DEF(f) static animation_multi_obj *ANIM_PH_NAME(f)[]
#define ANIM_PH_NUM(f) sizeof(ANIM_PH_NAME(f))/sizeof(*ANIM_PH_NAME(f))
#define ANIM_PS_NAME(f) anim_ ## f
#define ANIM_PS_REF(f) &ANIM_PS_NAME(f)
#define ANIM_PS_DEF(f) static animation_phases ANIM_PS_NAME(f)
#define ANIMS_M_NAME(f) anims_ ## f ## _m
#define ANIMS_M_DEF(f) static animation_phases *ANIMS_M_NAME(f)[]
#define ANIMS_M_NUM(f) sizeof(ANIMS_M_NAME(f))/sizeof(*ANIMS_M_NAME(f))
#define ANIMS_NAME(f) anims_ ## f
#define ANIMS_REF(f) &ANIMS_NAME(f)
#define ANIMS_DEF(f) static animations ANIMS_NAME(f)



/*****************************************************************************
 * The set of all transformations from a single point.
 *****************************************************************************/

/* The set of possible animations for a single point. */

/* Rotate a point on the equator around the z axis. */
ANIM_SO_DEF(single_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_rot_z) = {
  ANIM_SO_NUM(single_point_rot_z),
  ANIM_SO_NAME(single_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_rot_z) = {
  ANIM_MO_REF(single_point_rot_z)
};

ANIM_PS_DEF(single_point_rot_z) = {
  ANIM_PH_NUM(single_point_rot_z),
  ANIM_PH_NAME(single_point_rot_z)
};

/* Move a point on the equator along a Hopf torus. */
ANIM_SO_DEF(single_point_move_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    4,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_move_hopf_torus) = {
  ANIM_SO_NUM(single_point_move_hopf_torus),
  ANIM_SO_NAME(single_point_move_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_move_hopf_torus) = {
  ANIM_MO_REF(single_point_move_hopf_torus)
};

ANIM_PS_DEF(single_point_move_hopf_torus) = {
  ANIM_PH_NUM(single_point_move_hopf_torus),
  ANIM_PH_NAME(single_point_move_hopf_torus)
};

/* Move a point on the equator along a Hopf spiral. */
ANIM_SO_DEF(single_point_move_hopf_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 -4.0f*M_PI_F,         EASING_CUBIC, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_move_hopf_spiral) = {
  ANIM_SO_NUM(single_point_move_hopf_spiral),
  ANIM_SO_NAME(single_point_move_hopf_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_move_hopf_spiral) = {
  ANIM_MO_REF(single_point_move_hopf_spiral)
};

ANIM_PS_DEF(single_point_move_hopf_spiral) = {
  ANIM_PH_NUM(single_point_move_hopf_spiral),
  ANIM_PH_NAME(single_point_move_hopf_spiral)
};

/* Split a single point on the equator into two points, rotate the two
   points around the z axis, and merge the two points again. */

/* Phase 1: Split a single point on the equator into two points on the
   equator by moving the second point on the equator. */
ANIM_SO_DEF(single_point_to_double_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_ACCEL, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_double_point_rot_z) = {
  ANIM_SO_NUM(single_point_to_double_point_rot_z),
  ANIM_SO_NAME(single_point_to_double_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

/* Phase 2: Rotate the two points around the z axis. */
ANIM_SO_DEF(double_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              M_PI_F,               EASING_LIN,   /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_point_rot_z) = {
  ANIM_SO_NUM(double_point_rot_z),
  ANIM_SO_NAME(double_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

/* Phase 3: Merge the two points on the equator into a single point on the
   equator. */
ANIM_SO_DEF(double_point_to_single_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               2.0f*M_PI_F,          EASING_DECEL, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_point_to_single_point_rot_z) = {
  ANIM_SO_NUM(double_point_to_single_point_rot_z),
  ANIM_SO_NAME(double_point_to_single_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(double_point_rot_z) = {
  ANIM_MO_REF(single_point_to_double_point_rot_z),
  ANIM_MO_REF(double_point_rot_z),
  ANIM_MO_REF(double_point_to_single_point_rot_z),
};

ANIM_PS_DEF(double_point_rot_z) = {
  ANIM_PH_NUM(double_point_rot_z),
  ANIM_PH_NAME(double_point_rot_z)
};

/* Split a single point on the equator into two points, move the two
   points along a Hopf torus, and merge the two points again. */

/* Phase 1: Split a single point on the equator into two points on the
   equator by moving the second point on a meridian. */
ANIM_SO_DEF(single_point_to_double_point_move_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, -1.0f, 0.0f }, 0.0f, M_PI_F,        EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_double_point_move_hopf_torus) = {
  ANIM_SO_NUM(single_point_to_double_point_move_hopf_torus),
  ANIM_SO_NAME(single_point_to_double_point_move_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

/* Phase 2: Move the two points along the Hopf torus. */
ANIM_SO_DEF(double_point_move_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    4,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              M_PI_F,               EASING_CUBIC, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    4,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_point_move_hopf_torus) = {
  ANIM_SO_NUM(double_point_move_hopf_torus),
  ANIM_SO_NAME(double_point_move_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 3: Merge the two points on the equator into a single point on the
   equator by moving the second point along a meridian. */
ANIM_SO_DEF(double_point_to_single_point_move_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, -1.0f, 0.0f }, M_PI_F, 2.0f*M_PI_F, EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_point_to_single_point_move_hopf_torus) = {
  ANIM_SO_NUM(double_point_to_single_point_move_hopf_torus),
  ANIM_SO_NAME(double_point_to_single_point_move_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(double_point_move_hopf_torus) = {
  ANIM_MO_REF(single_point_to_double_point_move_hopf_torus),
  ANIM_MO_REF(double_point_move_hopf_torus),
  ANIM_MO_REF(double_point_to_single_point_move_hopf_torus),
};

ANIM_PS_DEF(double_point_move_hopf_torus) = {
  ANIM_PH_NUM(double_point_move_hopf_torus),
  ANIM_PH_NAME(double_point_move_hopf_torus)
};

/* Split a single point on the equator into two points, move the two
   points along a Hopf spiral, and merge the two points again. */

/* Phase 1: Split a single point on the equator into two points on the
   equator by moving the second point along a Hopf spiral. */
ANIM_SO_DEF(single_point_to_double_point_move_hopf_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 -2.0f*M_PI_F,         EASING_ACCEL, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_double_point_move_hopf_spiral) = {
  ANIM_SO_NUM(single_point_to_double_point_move_hopf_spiral),
  ANIM_SO_NAME(single_point_to_double_point_move_hopf_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

/* Phase 2: Move the two points along the Hopf spiral. */
ANIM_SO_DEF(double_point_move_hopf_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 -4.0f*M_PI_F,         EASING_CUBIC, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    2.0f*M_PI_F,          -2.0f*M_PI_F,         EASING_LIN,   /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_point_move_hopf_spiral) = {
  ANIM_SO_NUM(double_point_move_hopf_spiral),
  ANIM_SO_NAME(double_point_move_hopf_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 3: Merge the two points on the equator into a single point on the
   equator by moving the second point along a Hopf spiral. */
ANIM_SO_DEF(double_point_to_single_point_move_hopf_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    2.0f*M_PI_F,          0.0f,                 EASING_DECEL, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_point_to_single_point_move_hopf_spiral) = {
  ANIM_SO_NUM(double_point_to_single_point_move_hopf_spiral),
  ANIM_SO_NAME(double_point_to_single_point_move_hopf_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(double_point_move_hopf_spiral) = {
  ANIM_MO_REF(single_point_to_double_point_move_hopf_spiral),
  ANIM_MO_REF(double_point_move_hopf_spiral),
  ANIM_MO_REF(double_point_to_single_point_move_hopf_spiral),
};

ANIM_PS_DEF(double_point_move_hopf_spiral) = {
  ANIM_PH_NUM(double_point_move_hopf_spiral),
  ANIM_PH_NAME(double_point_move_hopf_spiral)
};

ANIMS_M_DEF(single_point) = {
  ANIM_PS_REF(single_point_rot_z),
  ANIM_PS_REF(single_point_move_hopf_torus),
  ANIM_PS_REF(single_point_move_hopf_spiral),
  ANIM_PS_REF(double_point_rot_z),
  ANIM_PS_REF(double_point_move_hopf_torus),
  ANIM_PS_REF(double_point_move_hopf_spiral)
};

ANIMS_DEF(single_point) = {
  ANIMS_M_NUM(single_point),
  ANIMS_M_NAME(single_point)
};



/* The set of possible animations for the transition from a single point
   to a single torus. */

/* Expand a point on the equator to a torus on the equator. */
ANIM_SO_DEF(single_point_to_single_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_single_torus_rot_z) = {
  ANIM_SO_NUM(single_point_to_single_torus_rot_z),
  ANIM_SO_NAME(single_point_to_single_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_single_torus_rot_z) = {
  ANIM_MO_REF(single_point_to_single_torus_rot_z)
};

ANIM_PS_DEF(single_point_to_single_torus_rot_z) = {
  ANIM_PH_NUM(single_point_to_single_torus_rot_z),
  ANIM_PH_NAME(single_point_to_single_torus_rot_z)
};

/* Expand a point on the equator to a torus on the equator and rotate around
   a random axis. */
ANIM_SO_DEF(single_point_to_single_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_single_torus_rot_rnd) = {
  ANIM_SO_NUM(single_point_to_single_torus_rot_rnd),
  ANIM_SO_NAME(single_point_to_single_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_single_torus_rot_rnd) = {
  ANIM_MO_REF(single_point_to_single_torus_rot_rnd)
};

ANIM_PS_DEF(single_point_to_single_torus_rot_rnd) = {
  ANIM_PH_NUM(single_point_to_single_torus_rot_rnd),
  ANIM_PH_NAME(single_point_to_single_torus_rot_rnd)
};

ANIMS_M_DEF(single_point_to_single_torus) = {
  ANIM_PS_REF(single_point_to_single_torus_rot_z),
  ANIM_PS_REF(single_point_to_single_torus_rot_rnd)
};

ANIMS_DEF(single_point_to_single_torus) = {
  ANIMS_M_NUM(single_point_to_single_torus),
  ANIMS_M_NAME(single_point_to_single_torus)
};



/* The set of possible animations for the transition from a single point
   to a double torus. */

/* Expand the point on the equator to two tori, rotate them around the
   z axis, and move them to latitude ±45°. */
ANIM_SO_DEF(single_point_to_double_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 -2.0f*M_PI_F,         EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_double_torus_rot_z) = {
  ANIM_SO_NUM(single_point_to_double_torus_rot_z),
  ANIM_SO_NAME(single_point_to_double_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_double_torus_rot_z) = {
  ANIM_MO_REF(single_point_to_double_torus_rot_z)
};

ANIM_PS_DEF(single_point_to_double_torus_rot_z) = {
  ANIM_PH_NUM(single_point_to_double_torus_rot_z),
  ANIM_PH_NAME(single_point_to_double_torus_rot_z)
};

/* Expand the point on the equator to two tori, rotate them around a random
   axis, and move them to latitude ±45°. */
ANIM_SO_DEF(single_point_to_double_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 -2.0f*M_PI_F,         EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_double_torus_rot_rnd) = {
  ANIM_SO_NUM(single_point_to_double_torus_rot_rnd),
  ANIM_SO_NAME(single_point_to_double_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_double_torus_rot_rnd) = {
  ANIM_MO_REF(single_point_to_double_torus_rot_rnd)
};

ANIM_PS_DEF(single_point_to_double_torus_rot_rnd) = {
  ANIM_PH_NUM(single_point_to_double_torus_rot_rnd),
  ANIM_PH_NAME(single_point_to_double_torus_rot_rnd)
};

ANIMS_M_DEF(single_point_to_double_torus) = {
  ANIM_PS_REF(single_point_to_double_torus_rot_z),
  ANIM_PS_REF(single_point_to_double_torus_rot_rnd)
};

ANIMS_DEF(single_point_to_double_torus) = {
  ANIMS_M_NUM(single_point_to_double_torus),
  ANIMS_M_NAME(single_point_to_double_torus)
};



/* The set of possible animations for the transition from a single point
   to a triple torus. */

/* Expand the point on the equator to three tori, rotate two of them around
   the z axis, and move them to latitude ±45°. */
ANIM_SO_DEF(single_point_to_triple_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 -2.0f*M_PI_F,         EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_triple_torus_rot_z) = {
  ANIM_SO_NUM(single_point_to_triple_torus_rot_z),
  ANIM_SO_NAME(single_point_to_triple_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_triple_torus_rot_z) = {
  ANIM_MO_REF(single_point_to_triple_torus_rot_z)
};

ANIM_PS_DEF(single_point_to_triple_torus_rot_z) = {
  ANIM_PH_NUM(single_point_to_triple_torus_rot_z),
  ANIM_PH_NAME(single_point_to_triple_torus_rot_z)
};

/* Expand the point on the equator to three tori, rotate them around a
   random axis, and move them to latitude ±45°. */
ANIM_SO_DEF(single_point_to_triple_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 -2.0f*M_PI_F,         EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_triple_torus_rot_rnd) = {
  ANIM_SO_NUM(single_point_to_triple_torus_rot_rnd),
  ANIM_SO_NAME(single_point_to_triple_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_triple_torus_rot_rnd) = {
  ANIM_MO_REF(single_point_to_triple_torus_rot_rnd)
};

ANIM_PS_DEF(single_point_to_triple_torus_rot_rnd) = {
  ANIM_PH_NUM(single_point_to_triple_torus_rot_rnd),
  ANIM_PH_NAME(single_point_to_triple_torus_rot_rnd)
};

ANIMS_M_DEF(single_point_to_triple_torus) = {
  ANIM_PS_REF(single_point_to_triple_torus_rot_z),
  ANIM_PS_REF(single_point_to_triple_torus_rot_rnd)
};

ANIMS_DEF(single_point_to_triple_torus) = {
  ANIMS_M_NUM(single_point_to_triple_torus),
  ANIMS_M_NAME(single_point_to_triple_torus)
};



/* The set of possible animations for the transition from a single point
   to a single Seifert surface. */

/* Expand a point on the equator to a Seifert surface on the equator. */
ANIM_SO_DEF(single_point_to_single_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_single_seifert_rot_z) = {
  ANIM_SO_NUM(single_point_to_single_seifert_rot_z),
  ANIM_SO_NAME(single_point_to_single_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_point_to_single_seifert_rot_z) = {
  ANIM_MO_REF(single_point_to_single_seifert_rot_z)
};

ANIM_PS_DEF(single_point_to_single_seifert_rot_z) = {
  ANIM_PH_NUM(single_point_to_single_seifert_rot_z),
  ANIM_PH_NAME(single_point_to_single_seifert_rot_z)
};

/* Expand a point on the equator to a Seifert surface on the equator and
   rotate around a random axis. */
ANIM_SO_DEF(single_point_to_single_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_single_seifert_rot_rnd) = {
  ANIM_SO_NUM(single_point_to_single_seifert_rot_rnd),
  ANIM_SO_NAME(single_point_to_single_seifert_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_single_seifert_rot_rnd) = {
  ANIM_MO_REF(single_point_to_single_seifert_rot_rnd)
};

ANIM_PS_DEF(single_point_to_single_seifert_rot_rnd) = {
  ANIM_PH_NUM(single_point_to_single_seifert_rot_rnd),
  ANIM_PH_NAME(single_point_to_single_seifert_rot_rnd)
};

ANIMS_M_DEF(single_point_to_single_seifert) = {
  ANIM_PS_REF(single_point_to_single_seifert_rot_z),
  ANIM_PS_REF(single_point_to_single_seifert_rot_rnd)
};

ANIMS_DEF(single_point_to_single_seifert) = {
  ANIMS_M_NUM(single_point_to_single_seifert),
  ANIMS_M_NAME(single_point_to_single_seifert)
};



/* The set of possible animations for the transition from a single point
   to a triple Seifert surface. */

/* Expand the point at the equator to a Seifert surface on the without
   rotating it.  Expand the point at the equator to two Seifert surfaces at
   latitude ±45° and rotate them around the z axis. */
ANIM_SO_DEF(single_point_to_triple_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/24.0f,        EASING_CUBIC, /* offset */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/24.0f,        EASING_CUBIC, /* offset */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_triple_seifert_rot_z) = {
  ANIM_SO_NUM(single_point_to_triple_seifert_rot_z),
  ANIM_SO_NAME(single_point_to_triple_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_triple_seifert_rot_z) = {
  ANIM_MO_REF(single_point_to_triple_seifert_rot_z)
};

ANIM_PS_DEF(single_point_to_triple_seifert_rot_z) = {
  ANIM_PH_NUM(single_point_to_triple_seifert_rot_z),
  ANIM_PH_NAME(single_point_to_triple_seifert_rot_z)
};

/* Move the single point at the equator to the south pole along a Hopf
   spiral, then successively expand three Seifert surfaces from the single
   point at the south pole and move them to latitudes 70°, 0°, and -70°,
   respectively, then decrease the point density by a factor of two and move
   the Seifert surfaces at latitude ±70° to latidude ±45°, all the while
   rotating all Seifert surfaces around the z axis in opposite directions. */

/* Phase 1: Move the single point at the equator to the south pole along a
   Hopf Spiral. */
ANIM_SO_DEF(single_point_to_triple_seifert_rot_z_move_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    -2.0f,                -2.0f,                EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
};

ANIM_MO_DEF(single_point_to_triple_seifert_rot_z_move_spiral) = {
  ANIM_SO_NUM(single_point_to_triple_seifert_rot_z_move_spiral),
  ANIM_SO_NAME(single_point_to_triple_seifert_rot_z_move_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  120                                                         /* num_steps */
};

/* Phase 2: Expand a Seifert surface from the south pole and move it to
   latitude 70°. */
ANIM_SO_DEF(single_point_to_triple_seifert_rot_z_move_north_1) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F,               M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_ACCEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_triple_seifert_rot_z_move_north_1) = {
  ANIM_SO_NUM(single_point_to_triple_seifert_rot_z_move_north_1),
  ANIM_SO_NAME(single_point_to_triple_seifert_rot_z_move_north_1),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 3: Expand a Seifert surface from the south pole and move it to
   latitude 0°. */
ANIM_SO_DEF(single_point_to_triple_seifert_rot_z_move_north_2) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_ACCEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_triple_seifert_rot_z_move_north_2) = {
  ANIM_SO_NUM(single_point_to_triple_seifert_rot_z_move_north_2),
  ANIM_SO_NAME(single_point_to_triple_seifert_rot_z_move_north_2),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 4: Expand a Seifert surface from the south pole and move it to
   latitude -70°. */
ANIM_SO_DEF(single_point_to_triple_seifert_rot_z_move_north_3) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F,               8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_ACCEL  /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_triple_seifert_rot_z_move_north_3) = {
  ANIM_SO_NUM(single_point_to_triple_seifert_rot_z_move_north_3),
  ANIM_SO_NAME(single_point_to_triple_seifert_rot_z_move_north_3),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 5: Rotate the three Seifert surfaces for one revolution. */
ANIM_SO_DEF(single_point_to_triple_seifert_rot_z_linear) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     8.0f*M_PI_F/9.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_LIN    /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_triple_seifert_rot_z_linear) = {
  ANIM_SO_NUM(single_point_to_triple_seifert_rot_z_linear),
  ANIM_SO_NAME(single_point_to_triple_seifert_rot_z_linear),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 1: Rotate the three Seifert surfaces for one revolution, move the
   Seifert surfaces at latitudes ±70° to latitudes ±45°, and decrease the
   point density by a factor of two. */
ANIM_SO_DEF(single_point_to_triple_seifert_rot_z_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_DECEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_DECEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_DECEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_DECEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_DECEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_DECEL  /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_triple_seifert_rot_z_loosen) = {
  ANIM_SO_NUM(single_point_to_triple_seifert_rot_z_loosen),
  ANIM_SO_NAME(single_point_to_triple_seifert_rot_z_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_triple_seifert_move_north) = {
  ANIM_MO_REF(single_point_to_triple_seifert_rot_z_move_spiral),
  ANIM_MO_REF(single_point_to_triple_seifert_rot_z_move_north_1),
  ANIM_MO_REF(single_point_to_triple_seifert_rot_z_move_north_2),
  ANIM_MO_REF(single_point_to_triple_seifert_rot_z_move_north_3),
  ANIM_MO_REF(single_point_to_triple_seifert_rot_z_linear),
  ANIM_MO_REF(single_point_to_triple_seifert_rot_z_loosen)
};

ANIM_PS_DEF(single_point_to_triple_seifert_move_north) = {
  ANIM_PH_NUM(single_point_to_triple_seifert_move_north),
  ANIM_PH_NAME(single_point_to_triple_seifert_move_north)
};

ANIMS_M_DEF(single_point_to_triple_seifert) = {
  ANIM_PS_REF(single_point_to_triple_seifert_rot_z),
  ANIM_PS_REF(single_point_to_triple_seifert_move_north)
};

ANIMS_DEF(single_point_to_triple_seifert) = {
  ANIMS_M_NUM(single_point_to_triple_seifert),
  ANIMS_M_NAME(single_point_to_triple_seifert)
};



/* The set of possible animations for the transition from a single point
   to a single Hopf torus. */

/* Expand a point on the equator to a Hopf torus on the equator and rotate
   around a random axis. */
ANIM_SO_DEF(single_point_to_single_hopf_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_single_hopf_torus_rot_rnd) = {
  ANIM_SO_NUM(single_point_to_single_hopf_torus_rot_rnd),
  ANIM_SO_NAME(single_point_to_single_hopf_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_point_to_single_hopf_torus_rot_rnd) = {
  ANIM_MO_REF(single_point_to_single_hopf_torus_rot_rnd)
};

ANIM_PS_DEF(single_point_to_single_hopf_torus_rot_rnd) = {
  ANIM_PH_NUM(single_point_to_single_hopf_torus_rot_rnd),
  ANIM_PH_NAME(single_point_to_single_hopf_torus_rot_rnd)
};

/* Expand the point to a loose Hopf torus and increase the point densify of
   the Hopf torus by a factor of five. */

/* Phase 1: Expand the point on the equator to a loose Hopf torus. */
ANIM_SO_DEF(single_point_to_single_loose_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_point_to_single_loose_hopf_torus) = {
  ANIM_SO_NUM(single_point_to_single_loose_hopf_torus),
  ANIM_SO_NAME(single_point_to_single_loose_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

/* Phase 2: Increase the density of the Hopf torus. */
ANIM_SO_DEF(single_hopf_torus_to_single_dense_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -2.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/60.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/60.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 2.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_to_single_dense_hopf_torus) = {
  ANIM_SO_NUM(single_hopf_torus_to_single_dense_hopf_torus),
  ANIM_SO_NAME(single_hopf_torus_to_single_dense_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

ANIM_PH_DEF(single_point_to_single_hopf_torus_densify) = {
  ANIM_MO_REF(single_point_to_single_loose_hopf_torus),
  ANIM_MO_REF(single_hopf_torus_to_single_dense_hopf_torus)
};

ANIM_PS_DEF(single_point_to_single_hopf_torus_densify) = {
  ANIM_PH_NUM(single_point_to_single_hopf_torus_densify),
  ANIM_PH_NAME(single_point_to_single_hopf_torus_densify)
};

ANIMS_M_DEF(single_point_to_single_hopf_torus) = {
  ANIM_PS_REF(single_point_to_single_hopf_torus_rot_rnd),
  ANIM_PS_REF(single_point_to_single_hopf_torus_densify)
};

ANIMS_DEF(single_point_to_single_hopf_torus) = {
  ANIMS_M_NUM(single_point_to_single_hopf_torus),
  ANIMS_M_NAME(single_point_to_single_hopf_torus)
};



/* The set of possible animations for the transition from a single point to
   a single Hopf spiral. */

/* Expand a point on the equator to a Hopf spiral and rotate around a random
   axis with a certain probability. */
ANIM_SO_DEF(single_hopf_spiral_to_single_point_expand_sector) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    0.0f,                 2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_single_point_expand_sector) = {
  ANIM_SO_NUM(single_hopf_spiral_to_single_point_expand_sector),
  ANIM_SO_NAME(single_hopf_spiral_to_single_point_expand_sector),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_single_point_expand_sector) = {
  ANIM_MO_REF(single_hopf_spiral_to_single_point_expand_sector)
};

ANIM_PS_DEF(single_hopf_spiral_to_single_point_expand_sector) = {
  ANIM_PH_NUM(single_hopf_spiral_to_single_point_expand_sector),
  ANIM_PH_NAME(single_hopf_spiral_to_single_point_expand_sector)
};

ANIMS_M_DEF(single_point_to_single_hopf_spiral) = {
  ANIM_PS_REF(single_hopf_spiral_to_single_point_expand_sector)
};

ANIMS_DEF(single_point_to_single_hopf_spiral) = {
  ANIMS_M_NUM(single_point_to_single_hopf_spiral),
  ANIMS_M_NAME(single_point_to_single_hopf_spiral)
};



/*****************************************************************************
 * The set of all transformations from a single torus.
 *****************************************************************************/

/* The set of possible animations for the transition from a single torus
   to a single point. */

/* Shrink a torus on the equator to a point on the equator. */
ANIM_SO_DEF(single_torus_to_single_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_point_rot_z) = {
  ANIM_SO_NUM(single_torus_to_single_point_rot_z),
  ANIM_SO_NAME(single_torus_to_single_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_single_point_rot_z) = {
  ANIM_MO_REF(single_torus_to_single_point_rot_z)
};

ANIM_PS_DEF(single_torus_to_single_point_rot_z) = {
  ANIM_PH_NUM(single_torus_to_single_point_rot_z),
  ANIM_PH_NAME(single_torus_to_single_point_rot_z)
};

/* Shrink a torus on the equator to a point on the equator and rotate around
   a random axis. */
ANIM_SO_DEF(single_torus_to_single_point_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_point_rot_rnd) = {
  ANIM_SO_NUM(single_torus_to_single_point_rot_rnd),
  ANIM_SO_NAME(single_torus_to_single_point_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_single_point_rot_rnd) = {
  ANIM_MO_REF(single_torus_to_single_point_rot_rnd)
};

ANIM_PS_DEF(single_torus_to_single_point_rot_rnd) = {
  ANIM_PH_NUM(single_torus_to_single_point_rot_rnd),
  ANIM_PH_NAME(single_torus_to_single_point_rot_rnd)
};

ANIMS_M_DEF(single_torus_to_single_point) = {
  ANIM_PS_REF(single_torus_to_single_point_rot_z),
  ANIM_PS_REF(single_torus_to_single_point_rot_rnd)
};

ANIMS_DEF(single_torus_to_single_point) = {
  ANIMS_M_NUM(single_torus_to_single_point),
  ANIMS_M_NAME(single_torus_to_single_point)
};



/* The set of possible animations for a single torus. */

/* Rotate a torus on the equator around the x axis of the total space. */
ANIM_SO_DEF(single_torus_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_rot_x) = {
  ANIM_SO_NUM(single_torus_rot_x),
  ANIM_SO_NAME(single_torus_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_rot_x) = {
  ANIM_MO_REF(single_torus_rot_x)
};

ANIM_PS_DEF(single_torus_rot_x) = {
  ANIM_PH_NUM(single_torus_rot_x),
  ANIM_PH_NAME(single_torus_rot_x)
};

/* Rotate a torus on the equator around the z axis. */
ANIM_SO_DEF(single_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_rot_z) = {
  ANIM_SO_NUM(single_torus_rot_z),
  ANIM_SO_NAME(single_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_rot_z) = {
  ANIM_MO_REF(single_torus_rot_z)
};

ANIM_PS_DEF(single_torus_rot_z) = {
  ANIM_PH_NUM(single_torus_rot_z),
  ANIM_PH_NAME(single_torus_rot_z)
};

/* Rotate a torus on the equator around a random axis. */
ANIM_SO_DEF(single_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_rot_rnd) = {
  ANIM_SO_NUM(single_torus_rot_rnd),
  ANIM_SO_NAME(single_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_rot_rnd) = {
  ANIM_MO_REF(single_torus_rot_rnd)
};

ANIM_PS_DEF(single_torus_rot_rnd) = {
  ANIM_PH_NUM(single_torus_rot_rnd),
  ANIM_PH_NAME(single_torus_rot_rnd)
};

/* Move a torus on the equator up and down to latitude ±80° while rotating
   it around the z axis and, with a certain probability, around a random
   axis. */
ANIM_SO_DEF(single_torus_move) = {
  {
    GEN_TORUS,                                                /* generator */
    1.0f*M_PI_F/18.0f,    17.0f*M_PI_F/18.0f,   EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_move) = {
  ANIM_SO_NUM(single_torus_move),
  ANIM_SO_NAME(single_torus_move),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_move) = {
  ANIM_MO_REF(single_torus_move)
};

ANIM_PS_DEF(single_torus_move) = {
  ANIM_PH_NUM(single_torus_move),
  ANIM_PH_NAME(single_torus_move)
};

/* Rotate a torus on the equator to a meridian, yielding a parabolic ring
   cyclide, and increase its density by a factor of three.  Then, rotate
   it around the x axis of the total space.  Finally, rotate the torus back
   to the equator and decrease its density by a factor of three. */

/* Phase 1: Rotate the torus by 90° to a meridian and increase its point
   density by a factor of three. */
ANIM_SO_DEF(single_torus_to_meridian_torus_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/36.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/36.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_meridian_torus_densify) = {
  ANIM_SO_NUM(single_torus_to_meridian_torus_densify),
  ANIM_SO_NAME(single_torus_to_meridian_torus_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

/* Phase 2: Rotate the torus by 360° around the x axis of the total space. */
ANIM_SO_DEF(single_meridian_torus_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_meridian_torus_rot_x) = {
  ANIM_SO_NUM(single_meridian_torus_rot_x),
  ANIM_SO_NAME(single_meridian_torus_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 3: Rotate the meridian torus by 90° to the equator and decrease its
   point density by a factor of three. */
ANIM_SO_DEF(single_meridian_torus_to_torus_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 2.0f*M_PI_F/2.0f,
                                                EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/36.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 2.0f*M_PI_F/2.0f,
                                                EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/36.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 2.0f*M_PI_F/2.0f,
                                                EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_meridian_torus_to_torus_loosen) = {
  ANIM_SO_NUM(single_meridian_torus_to_torus_loosen),
  ANIM_SO_NAME(single_meridian_torus_to_torus_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_torus_meridian) = {
  ANIM_MO_REF(single_torus_to_meridian_torus_densify),
  ANIM_MO_REF(single_meridian_torus_rot_x),
  ANIM_MO_REF(single_meridian_torus_to_torus_loosen)
};

ANIM_PS_DEF(single_torus_meridian) = {
  ANIM_PH_NUM(single_torus_meridian),
  ANIM_PH_NAME(single_torus_meridian)
};

ANIMS_M_DEF(single_torus) = {
  ANIM_PS_REF(single_torus_rot_x),
  ANIM_PS_REF(single_torus_rot_z),
  ANIM_PS_REF(single_torus_rot_rnd),
  ANIM_PS_REF(single_torus_move),
  ANIM_PS_REF(single_torus_meridian)
};

ANIMS_DEF(single_torus) = {
  ANIMS_M_NUM(single_torus),
  ANIMS_M_NAME(single_torus)
};



/* The set of possible animations for the transition from a single torus
   to a double torus. */

/* Split the torus at the equator into a dense torus, then split the torus
   into two tori, rotate them around the z axis, and move them to latitude
   ±45°. */

/* Phase 1: Increase the point density by a factor of two. */
ANIM_SO_DEF(single_torus_to_single_dense_torus_densify_two) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/48.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/48.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_dense_torus_densify_two) = {
  ANIM_SO_NUM(single_torus_to_single_dense_torus_densify_two),
  ANIM_SO_NAME(single_torus_to_single_dense_torus_densify_two),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  30                                                          /* num_steps */
};

/* Phase 2: Split the torus into two tori, rotate them around the z axis,
   and move them to latitude ±45°. */
ANIM_SO_DEF(single_dense_torus_to_double_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/48.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_torus_to_double_torus_rot_z) = {
  ANIM_SO_NUM(single_dense_torus_to_double_torus_rot_z),
  ANIM_SO_NAME(single_dense_torus_to_double_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_double_torus_rot_z) = {
  ANIM_MO_REF(single_torus_to_single_dense_torus_densify_two),
  ANIM_MO_REF(single_dense_torus_to_double_torus_rot_z)
};

ANIM_PS_DEF(single_torus_to_double_torus_rot_z) = {
  ANIM_PH_NUM(single_torus_to_double_torus_rot_z),
  ANIM_PH_NAME(single_torus_to_double_torus_rot_z)
};

/* Split the torus at the equator into two tori, rotate them around a random
   axis, and move them to latitude ±45°. */
ANIM_SO_DEF(single_torus_to_double_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_double_torus_rot_rnd) = {
  ANIM_SO_NUM(single_torus_to_double_torus_rot_rnd),
  ANIM_SO_NAME(single_torus_to_double_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_double_torus_rot_rnd) = {
  ANIM_MO_REF(single_torus_to_double_torus_rot_rnd)
};

ANIM_PS_DEF(single_torus_to_double_torus_rot_rnd) = {
  ANIM_PH_NUM(single_torus_to_double_torus_rot_rnd),
  ANIM_PH_NAME(single_torus_to_double_torus_rot_rnd)
};

ANIMS_M_DEF(single_torus_to_double_torus) = {
  ANIM_PS_REF(single_torus_to_double_torus_rot_z),
  ANIM_PS_REF(single_torus_to_double_torus_rot_rnd)
};

ANIMS_DEF(single_torus_to_double_torus) = {
  ANIMS_M_NUM(single_torus_to_double_torus),
  ANIMS_M_NAME(single_torus_to_double_torus)
};



/* The set of possible animations for the transition from a single torus
   to a triple torus. */

/* Increase the density of the torus at the equator by a factor of three,
   split the dense torus into three tori, move two of the tori to latitude
   ±45°, and rotate all three tori around the z axis. */

/* Phase 1: Increase the point density by a factor of three. */
ANIM_SO_DEF(single_torus_to_single_dense_torus_densify_three) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/36.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/36.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_dense_torus_densify_three) = {
  ANIM_SO_NUM(single_torus_to_single_dense_torus_densify_three),
  ANIM_SO_NAME(single_torus_to_single_dense_torus_densify_three),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  30                                                          /* num_steps */
};

/* Phase 2: Split the torus into three tori, rotate them around the z axis,
   and move them to latitudes ±45° and 0°. */
ANIM_SO_DEF(single_dense_torus_to_triple_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/36.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/36.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_torus_to_triple_torus_rot_z) = {
  ANIM_SO_NUM(single_dense_torus_to_triple_torus_rot_z),
  ANIM_SO_NAME(single_dense_torus_to_triple_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_triple_torus_rot_z) = {
  ANIM_MO_REF(single_torus_to_single_dense_torus_densify_three),
  ANIM_MO_REF(single_dense_torus_to_triple_torus_rot_z)
};

ANIM_PS_DEF(single_torus_to_triple_torus_rot_z) = {
  ANIM_PH_NUM(single_torus_to_triple_torus_rot_z),
  ANIM_PH_NAME(single_torus_to_triple_torus_rot_z)
};

/* Split the torus at the equator into three tori, rotate them around a
   random axis, and move them to latitudes ±45° and 0°. */
ANIM_SO_DEF(single_torus_to_triple_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_triple_torus_rot_rnd) = {
  ANIM_SO_NUM(single_torus_to_triple_torus_rot_rnd),
  ANIM_SO_NAME(single_torus_to_triple_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_triple_torus_rot_rnd) = {
  ANIM_MO_REF(single_torus_to_triple_torus_rot_rnd)
};

ANIM_PS_DEF(single_torus_to_triple_torus_rot_rnd) = {
  ANIM_PH_NUM(single_torus_to_triple_torus_rot_rnd),
  ANIM_PH_NAME(single_torus_to_triple_torus_rot_rnd)
};

ANIMS_M_DEF(single_torus_to_triple_torus) = {
  ANIM_PS_REF(single_torus_to_triple_torus_rot_z),
  ANIM_PS_REF(single_torus_to_triple_torus_rot_rnd)
};

ANIMS_DEF(single_torus_to_triple_torus) = {
  ANIMS_M_NUM(single_torus_to_triple_torus),
  ANIMS_M_NAME(single_torus_to_triple_torus)
};



/* The set of possible animations for the transition from a single torus
   to a single Seifert surface. */

/* Shrink the torus on the equator to a Seifert surface on the equator. */
ANIM_SO_DEF(single_torus_to_single_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_seifert_rot_z) = {
  ANIM_SO_NUM(single_torus_to_single_seifert_rot_z),
  ANIM_SO_NAME(single_torus_to_single_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_torus_to_single_seifert_rot_z) = {
  ANIM_MO_REF(single_torus_to_single_seifert_rot_z)
};

ANIM_PS_DEF(single_torus_to_single_seifert_rot_z) = {
  ANIM_PH_NUM(single_torus_to_single_seifert_rot_z),
  ANIM_PH_NAME(single_torus_to_single_seifert_rot_z)
};

/* Shrink the torus on the equator to a Seifert surface on the equator and
   rotate it around a random axis. */
ANIM_SO_DEF(single_torus_to_single_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_seifert_rot_rnd) = {
  ANIM_SO_NUM(single_torus_to_single_seifert_rot_rnd),
  ANIM_SO_NAME(single_torus_to_single_seifert_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_single_seifert_rot_rnd) = {
  ANIM_MO_REF(single_torus_to_single_seifert_rot_rnd)
};

ANIM_PS_DEF(single_torus_to_single_seifert_rot_rnd) = {
  ANIM_PH_NUM(single_torus_to_single_seifert_rot_rnd),
  ANIM_PH_NAME(single_torus_to_single_seifert_rot_rnd)
};

/* Flip half of the torus on the equator around a suitable axis on the
   equator to form a Seifert surface on the equator. */
ANIM_SO_DEF(single_torus_to_single_seifert_flip) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { -0.99785892f, 0.06540313f, 0.0f },
                          0.0f, M_PI_F,         EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_seifert_flip) = {
  ANIM_SO_NUM(single_torus_to_single_seifert_flip),
  ANIM_SO_NAME(single_torus_to_single_seifert_flip),
  0.25f,                                        EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_torus_to_single_seifert_flip) = {
  ANIM_MO_REF(single_torus_to_single_seifert_flip)
};

ANIM_PS_DEF(single_torus_to_single_seifert_flip) = {
  ANIM_PH_NUM(single_torus_to_single_seifert_flip),
  ANIM_PH_NAME(single_torus_to_single_seifert_flip)
};

ANIMS_M_DEF(single_torus_to_single_seifert) = {
  ANIM_PS_REF(single_torus_to_single_seifert_rot_z),
  ANIM_PS_REF(single_torus_to_single_seifert_rot_rnd),
  ANIM_PS_REF(single_torus_to_single_seifert_flip)
};

ANIMS_DEF(single_torus_to_single_seifert) = {
  ANIMS_M_NUM(single_torus_to_single_seifert),
  ANIMS_M_NAME(single_torus_to_single_seifert)
};



/* The set of possible animations for the transition from a single torus
   to a triple Seifert surface. */

/* Increase the density of the torus at the equator by a factor of three,
   split the dense torus into three Seifert surfaces of sector 120°,
   increase their sectors to 180°, and move two of the Seifert surfaces to
   latitude ±45°, while rotating all three Seifert surfaces around the
   z axis. */

/* Phase 1: Increase the point density by a factor of three.  Already
   defined above. */

/* Phase 2: Split the dense torus into three Seifert surfaces of sector
   120°, increase their sectors to 180°, and move two of the Seifert
   surfaces to latitude ±45°, while rotating all three Seifert surfaces
   around the z axis. */
ANIM_SO_DEF(single_dense_torus_to_triple_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    4.0f*M_PI_F/3.0f,     -M_PI_F,              EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    2.0f*M_PI_F/3.0f,     -M_PI_F,              EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_torus_to_triple_seifert_rot_z) = {
  ANIM_SO_NUM(single_dense_torus_to_triple_seifert_rot_z),
  ANIM_SO_NAME(single_dense_torus_to_triple_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_triple_seifert_rot_z) = {
  ANIM_MO_REF(single_torus_to_single_dense_torus_densify_three),
  ANIM_MO_REF(single_dense_torus_to_triple_seifert_rot_z)
};

ANIM_PS_DEF(single_torus_to_triple_seifert_rot_z) = {
  ANIM_PH_NUM(single_torus_to_triple_seifert_rot_z),
  ANIM_PH_NAME(single_torus_to_triple_seifert_rot_z)
};

/* Split the torus at the equator into three Seifert surfaces, rotate them
   around a random axis, shrink their sectors from 360° to 180°, and move
   two of the Seifert surfaces from the equator to latitude ±45°. */
ANIM_SO_DEF(single_torus_to_triple_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_triple_seifert_rot_rnd) = {
  ANIM_SO_NUM(single_torus_to_triple_seifert_rot_rnd),
  ANIM_SO_NAME(single_torus_to_triple_seifert_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_triple_seifert_rot_rnd) = {
  ANIM_MO_REF(single_torus_to_triple_seifert_rot_rnd)
};

ANIM_PS_DEF(single_torus_to_triple_seifert_rot_rnd) = {
  ANIM_PH_NUM(single_torus_to_triple_seifert_rot_rnd),
  ANIM_PH_NAME(single_torus_to_triple_seifert_rot_rnd)
};

ANIMS_M_DEF(single_torus_to_triple_seifert) = {
  ANIM_PS_REF(single_torus_to_triple_seifert_rot_z),
  ANIM_PS_REF(single_torus_to_triple_seifert_rot_rnd)
};

ANIMS_DEF(single_torus_to_triple_seifert) = {
  ANIMS_M_NUM(single_torus_to_triple_seifert),
  ANIMS_M_NAME(single_torus_to_triple_seifert)
};



/* The set of possible animations for the transition from a single torus
   to a single Hopf torus. */

/* Fill the torus on the equator to a torus on the equator of five times
   the point density and then deform it to the Hopf torus. */

/* Phase 1: Increase the density of the torus on the equator. */
ANIM_SO_DEF(single_torus_to_single_dense_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -2.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/60.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/60.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 2.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_dense_torus) = {
  ANIM_SO_NUM(single_torus_to_single_dense_torus),
  ANIM_SO_NAME(single_torus_to_single_dense_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Deform the torus on the equator to a Hopf torus. */
ANIM_SO_DEF(single_dense_torus_to_single_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_torus_to_single_hopf_torus) = {
  ANIM_SO_NUM(single_dense_torus_to_single_hopf_torus),
  ANIM_SO_NAME(single_dense_torus_to_single_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_torus_to_single_hopf_torus_densify) = {
  ANIM_MO_REF(single_torus_to_single_dense_torus),
  ANIM_MO_REF(single_dense_torus_to_single_hopf_torus)
};

ANIM_PS_DEF(single_torus_to_single_hopf_torus_densify) = {
  ANIM_PH_NUM(single_torus_to_single_hopf_torus_densify),
  ANIM_PH_NAME(single_torus_to_single_hopf_torus_densify)
};

ANIMS_M_DEF(single_torus_to_single_hopf_torus) = {
  ANIM_PS_REF(single_torus_to_single_hopf_torus_densify)
};

ANIMS_DEF(single_torus_to_single_hopf_torus) = {
  ANIMS_M_NUM(single_torus_to_single_hopf_torus),
  ANIMS_M_NAME(single_torus_to_single_hopf_torus)
};



/* The set of possible animations for the transition from a single torus to
   a single Hopf spiral. */

/* Increase the point density of a torus on the equator (a collapsed Hopf
   spiral) by a factor of three, increase its winding number to two and its
   height to a Hopf spiral.. */

/* Phase 1: Increase the density of the torus by a factor of three. */
ANIM_SO_DEF(single_torus_to_single_dense_unwound_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/36.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/36.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_dense_unwound_torus) = {
  ANIM_SO_NUM(single_torus_to_single_dense_unwound_torus),
  ANIM_SO_NAME(single_torus_to_single_dense_unwound_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Wind the collapsed spiral and expand it. */
ANIM_SO_DEF(single_dense_unwound_torus_to_single_hopf_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    1.0f,                 2.0f,                 EASING_CUBIC, /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_unwound_torus_to_single_hopf_spiral) = {
  ANIM_SO_NUM(single_dense_unwound_torus_to_single_hopf_spiral),
  ANIM_SO_NAME(single_dense_unwound_torus_to_single_hopf_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_single_hopf_spiral_wind) = {
  ANIM_MO_REF(single_torus_to_single_dense_unwound_torus),
  ANIM_MO_REF(single_dense_unwound_torus_to_single_hopf_spiral)
};

ANIM_PS_DEF(single_torus_to_single_hopf_spiral_wind) = {
  ANIM_PH_NUM(single_torus_to_single_hopf_spiral_wind),
  ANIM_PH_NAME(single_torus_to_single_hopf_spiral_wind)
};

/* Decrease the winding number of a Hopf spiral on the equator to three and
   expand its height. */
ANIM_SO_DEF(single_torus_to_single_hopf_spiral_unwind) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    3.0f,                 2.0f,                 EASING_CUBIC, /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_torus_to_single_hopf_spiral_unwind) = {
  ANIM_SO_NUM(single_torus_to_single_hopf_spiral_unwind),
  ANIM_SO_NAME(single_torus_to_single_hopf_spiral_unwind),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_torus_to_single_hopf_spiral_unwind) = {
  ANIM_MO_REF(single_torus_to_single_hopf_spiral_unwind)
};

ANIM_PS_DEF(single_torus_to_single_hopf_spiral_unwind) = {
  ANIM_PH_NUM(single_torus_to_single_hopf_spiral_unwind),
  ANIM_PH_NAME(single_torus_to_single_hopf_spiral_unwind)
};

ANIMS_M_DEF(single_torus_to_single_hopf_spiral) = {
  ANIM_PS_REF(single_torus_to_single_hopf_spiral_wind),
  ANIM_PS_REF(single_torus_to_single_hopf_spiral_unwind)
};

ANIMS_DEF(single_torus_to_single_hopf_spiral) = {
  ANIMS_M_NUM(single_torus_to_single_hopf_spiral),
  ANIMS_M_NAME(single_torus_to_single_hopf_spiral)
};



/*****************************************************************************
 * The set of all transformations from a double torus.
 *****************************************************************************/

/* The set of possible animations for the transition from a double torus
   to a single point. */

/* Rotate two tori at latitude ±45° around the z axis, shrink them to a
   single point, and move them to the equator. */
ANIM_SO_DEF(double_torus_to_single_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    -2.0f*M_PI_F,         0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_single_point_rot_z) = {
  ANIM_SO_NUM(double_torus_to_single_point_rot_z),
  ANIM_SO_NAME(double_torus_to_single_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_to_single_point_rot_z) = {
  ANIM_MO_REF(double_torus_to_single_point_rot_z)
};

ANIM_PS_DEF(double_torus_to_single_point_rot_z) = {
  ANIM_PH_NUM(double_torus_to_single_point_rot_z),
  ANIM_PH_NAME(double_torus_to_single_point_rot_z)
};

/* Rotate two tori at latitude ±45° around a random axis, shrink them to a
   single point, and move them to the equator. */
ANIM_SO_DEF(double_torus_to_single_point_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    -2.0f*M_PI_F,         0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_single_point_rot_rnd) = {
  ANIM_SO_NUM(double_torus_to_single_point_rot_rnd),
  ANIM_SO_NAME(double_torus_to_single_point_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_to_single_point_rot_rnd) = {
  ANIM_MO_REF(double_torus_to_single_point_rot_rnd)
};

ANIM_PS_DEF(double_torus_to_single_point_rot_rnd) = {
  ANIM_PH_NUM(double_torus_to_single_point_rot_rnd),
  ANIM_PH_NAME(double_torus_to_single_point_rot_rnd)
};

ANIMS_M_DEF(double_torus_to_single_point) = {
  ANIM_PS_REF(double_torus_to_single_point_rot_z),
  ANIM_PS_REF(double_torus_to_single_point_rot_rnd)
};

ANIMS_DEF(double_torus_to_single_point) = {
  ANIMS_M_NUM(double_torus_to_single_point),
  ANIMS_M_NAME(double_torus_to_single_point)
};



/* The set of possible animations for the transition from a double torus
   to a single torus. */

/* Rotate two tori at latitude ±45° around the z axis and merge them into
   a dense torus at the equator, then reduce the point density by a factor
   of two. */

/* Phase 1: Rotate two tori at latitude ±45° around the z axis and merge
   them into a dense torus at the equator. */
ANIM_SO_DEF(double_torus_to_single_dense_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/48.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/48.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_single_dense_torus_rot_z) = {
  ANIM_SO_NUM(double_torus_to_single_dense_torus_rot_z),
  ANIM_SO_NAME(double_torus_to_single_dense_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 2: Reduce the point density by a factor of two. */
ANIM_SO_DEF(single_dense_torus_to_single_torus_loosen_two) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/48.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_torus_to_single_torus_loosen_two) = {
  ANIM_SO_NUM(single_dense_torus_to_single_torus_loosen_two),
  ANIM_SO_NAME(single_dense_torus_to_single_torus_loosen_two),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  30                                                          /* num_steps */
};

ANIM_PH_DEF(double_torus_to_single_torus_rot_z) = {
  ANIM_MO_REF(double_torus_to_single_dense_torus_rot_z),
  ANIM_MO_REF(single_dense_torus_to_single_torus_loosen_two)
};

ANIM_PS_DEF(double_torus_to_single_torus_rot_z) = {
  ANIM_PH_NUM(double_torus_to_single_torus_rot_z),
  ANIM_PH_NAME(double_torus_to_single_torus_rot_z)
};

/* Rotate two tori at latitude ±45° around a random axis and move them to
   the equator. */
ANIM_SO_DEF(double_torus_to_single_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_single_torus_rot_rnd) = {
  ANIM_SO_NUM(double_torus_to_single_torus_rot_rnd),
  ANIM_SO_NAME(double_torus_to_single_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_to_single_torus_rot_rnd) = {
  ANIM_MO_REF(double_torus_to_single_torus_rot_rnd)
};

ANIM_PS_DEF(double_torus_to_single_torus_rot_rnd) = {
  ANIM_PH_NUM(double_torus_to_single_torus_rot_rnd),
  ANIM_PH_NAME(double_torus_to_single_torus_rot_rnd)
};

ANIMS_M_DEF(double_torus_to_single_torus) = {
  ANIM_PS_REF(double_torus_to_single_torus_rot_z),
  ANIM_PS_REF(double_torus_to_single_torus_rot_rnd)
};

ANIMS_DEF(double_torus_to_single_torus) = {
  ANIMS_M_NUM(double_torus_to_single_torus),
  ANIMS_M_NAME(double_torus_to_single_torus)
};



/* The set of possible animations for a double torus. */

/* Rotate two tori at latitude ±45° around the x axis of the total space
   while rotating them around the z axis in opposite directions. */
ANIM_SO_DEF(double_torus_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_rot_x) = {
  ANIM_SO_NUM(double_torus_rot_x),
  ANIM_SO_NAME(double_torus_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_rot_x) = {
  ANIM_MO_REF(double_torus_rot_x)
};

ANIM_PS_DEF(double_torus_rot_x) = {
  ANIM_PH_NUM(double_torus_rot_x),
  ANIM_PH_NAME(double_torus_rot_x)
};

/* Rotate two tori at latitude ±45° by 90° around the y axis, then rotate
   them by 360° around the x axis of the total space, and then once more by
   90° around the y axis. */

/* Phase 1: Rotate the tori around the y axis by 90°. */
ANIM_SO_DEF(double_torus_rot_y_p1) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 1.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 1.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_rot_y_p1) = {
  ANIM_SO_NUM(double_torus_rot_y_p1),
  ANIM_SO_NAME(double_torus_rot_y_p1),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

/* Phase 2: Rotate the two rotated tori by 360° around the x axis of the
   total space. */
ANIM_SO_DEF(double_torus_rot_y_p2) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 1.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 1.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_rot_y_p2) = {
  ANIM_SO_NUM(double_torus_rot_y_p2),
  ANIM_SO_NAME(double_torus_rot_y_p2),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  180                                                         /* num_steps */
};

/* Phase 3: Rotate the tori around the y axis by an additional 90°. */
ANIM_SO_DEF(double_torus_rot_y_p3) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 1.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 1.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_rot_y_p3) = {
  ANIM_SO_NUM(double_torus_rot_y_p3),
  ANIM_SO_NAME(double_torus_rot_y_p3),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(double_torus_rot_y) = {
  ANIM_MO_REF(double_torus_rot_y_p1),
  ANIM_MO_REF(double_torus_rot_y_p2),
  ANIM_MO_REF(double_torus_rot_y_p3)
};

ANIM_PS_DEF(double_torus_rot_y) = {
  ANIM_PH_NUM(double_torus_rot_y),
  ANIM_PH_NAME(double_torus_rot_y)
};

/* Move two tori at latitudes ±45° to latitude ±70° and increase their point
   density by a factor of two, rotate the tori around a random axis, and
   move them back to ±45° while decreasing their point density by a factor
   of two. */

/* Phase 1: Move two tori at latitude ±45° to latitude ±70° and increase
   their point density by a factor of two. */
ANIM_SO_DEF(double_torus_rot_rnd_move_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/48.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/48.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/48.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/48.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_rot_rnd_move_densify) = {
  ANIM_SO_NUM(double_torus_rot_rnd_move_densify),
  ANIM_SO_NAME(double_torus_rot_rnd_move_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Rotate the two tori by 360° around a random axis. */
ANIM_SO_DEF(double_torus_rot_rnd_rot) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         M_PI_F/48.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     8.0f*M_PI_F/9.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         M_PI_F/48.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_rot_rnd_rot) = {
  ANIM_SO_NUM(double_torus_rot_rnd_rot),
  ANIM_SO_NAME(double_torus_rot_rnd_rot),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 3: Move two tori at latitude ±70° to latitude ±45° and decrease
   their point density by a factor of two. */
ANIM_SO_DEF(double_torus_rot_rnd_move_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/48.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/48.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_rot_rnd_move_loosen) = {
  ANIM_SO_NUM(double_torus_rot_rnd_move_loosen),
  ANIM_SO_NAME(double_torus_rot_rnd_move_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

ANIM_PH_DEF(double_torus_rot_rnd) = {
  ANIM_MO_REF(double_torus_rot_rnd_move_densify),
  ANIM_MO_REF(double_torus_rot_rnd_rot),
  ANIM_MO_REF(double_torus_rot_rnd_move_loosen)
};

ANIM_PS_DEF(double_torus_rot_rnd) = {
  ANIM_PH_NUM(double_torus_rot_rnd),
  ANIM_PH_NAME(double_torus_rot_rnd)
};

/* Move two tori at latitude ±45° to latitude ±5°, to latitude ±85° and
   back to latitude ±45° while rotating them around the z axis and, with
   a certain probability, around a random axis. */
ANIM_SO_DEF(double_torus_move) = {
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/18.0f,    M_PI_F/18.0f,         EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    10.0f*M_PI_F/18.0f,   17.0f*M_PI_F/18.0f,   EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_move) = {
  ANIM_SO_NUM(double_torus_move),
  ANIM_SO_NAME(double_torus_move),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_move) = {
  ANIM_MO_REF(double_torus_move)
};

ANIM_PS_DEF(double_torus_move) = {
  ANIM_PH_NUM(double_torus_move),
  ANIM_PH_NAME(double_torus_move)
};

ANIMS_M_DEF(double_torus) = {
  ANIM_PS_REF(double_torus_rot_x),
  ANIM_PS_REF(double_torus_rot_y),
  ANIM_PS_REF(double_torus_rot_rnd),
  ANIM_PS_REF(double_torus_move)
};

ANIMS_DEF(double_torus) = {
  ANIMS_M_NUM(double_torus),
  ANIMS_M_NAME(double_torus)
};



/* The set of possible animations for the transition from a double torus
   to a triple torus. */

/* Rotate the three tori around the z axis, split the torus at the equator
   into two tori, and move them to latitude ±45°. */
ANIM_SO_DEF(double_torus_to_triple_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/12.0f,         M_PI_F/12.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_triple_torus_rot_z) = {
  ANIM_SO_NUM(double_torus_to_triple_torus_rot_z),
  ANIM_SO_NAME(double_torus_to_triple_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_to_triple_torus_rot_z) = {
  ANIM_MO_REF(double_torus_to_triple_torus_rot_z)
};

ANIM_PS_DEF(double_torus_to_triple_torus_rot_z) = {
  ANIM_PH_NUM(double_torus_to_triple_torus_rot_z),
  ANIM_PH_NAME(double_torus_to_triple_torus_rot_z)
};

/* Rotate two tori at around a random axis, split the tori at latitude ±45°
   into two tori, and move them to the equator. */
ANIM_SO_DEF(double_torus_to_triple_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_triple_torus_rot_rnd) = {
  ANIM_SO_NUM(double_torus_to_triple_torus_rot_rnd),
  ANIM_SO_NAME(double_torus_to_triple_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_to_triple_torus_rot_rnd) = {
  ANIM_MO_REF(double_torus_to_triple_torus_rot_rnd)
};

ANIM_PS_DEF(double_torus_to_triple_torus_rot_rnd) = {
  ANIM_PH_NUM(double_torus_to_triple_torus_rot_rnd),
  ANIM_PH_NAME(double_torus_to_triple_torus_rot_rnd)
};

ANIMS_M_DEF(double_torus_to_triple_torus) = {
  ANIM_PS_REF(double_torus_to_triple_torus_rot_z),
  ANIM_PS_REF(double_torus_to_triple_torus_rot_rnd)
};

ANIMS_DEF(double_torus_to_triple_torus) = {
  ANIMS_M_NUM(double_torus_to_triple_torus),
  ANIMS_M_NAME(double_torus_to_triple_torus)
};



/* The set of possible animations for the transition from a double torus
   to a single Seifert surface. */

/* Shrink the two tori from 360° sectors to 90° sectors, join them at the
   equator, and reduce the point density by a factor of two. */

/* Phase 1: Shrink the two tori from 360° sectors to 90° sectors and join
   them at the equator. */
ANIM_SO_DEF(double_torus_to_single_dense_seifert_two_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/2.0f,         -49.0f*M_PI_F/96.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F/2.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/2.0f,         -51.0f*M_PI_F/96.0f,  EASING_CUBIC, /* offset */
    -2.0f*M_PI_F,         -M_PI_F/2.0f,         EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_single_dense_seifert_two_rot_z) = {
  ANIM_SO_NUM(double_torus_to_single_dense_seifert_two_rot_z),
  ANIM_SO_NAME(double_torus_to_single_dense_seifert_two_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 2: Reduce the point density by a factor of two. */
ANIM_SO_DEF(single_dense_seifert_two_to_single_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    95.0f*M_PI_F/96.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    97.0f*M_PI_F/96.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_seifert_two_to_single_seifert_rot_z) = {
  ANIM_SO_NUM(single_dense_seifert_two_to_single_seifert_rot_z),
  ANIM_SO_NAME(single_dense_seifert_two_to_single_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  30                                                          /* num_steps */
};

ANIM_PH_DEF(double_torus_to_single_seifert_rot_z) = {
  ANIM_MO_REF(double_torus_to_single_dense_seifert_two_rot_z),
  ANIM_MO_REF(single_dense_seifert_two_to_single_seifert_rot_z)
};

ANIM_PS_DEF(double_torus_to_single_seifert_rot_z) = {
  ANIM_PH_NUM(double_torus_to_single_seifert_rot_z),
  ANIM_PH_NAME(double_torus_to_single_seifert_rot_z)
};

/* Shrink the two tori from 360° sectors to 180° sectors, reduce their
   point point density by a factor of two, interleave them at the
   equator, and rotate around a random axis. */
ANIM_SO_DEF(double_torus_to_single_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    13.0f*M_PI_F/12.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    13.0f*M_PI_F/12.0f,   25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_single_seifert_rot_rnd) = {
  ANIM_SO_NUM(double_torus_to_single_seifert_rot_rnd),
  ANIM_SO_NAME(double_torus_to_single_seifert_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_to_single_seifert_rot_rnd) = {
  ANIM_MO_REF(double_torus_to_single_seifert_rot_rnd)
};

ANIM_PS_DEF(double_torus_to_single_seifert_rot_rnd) = {
  ANIM_PH_NUM(double_torus_to_single_seifert_rot_rnd),
  ANIM_PH_NAME(double_torus_to_single_seifert_rot_rnd)
};

ANIMS_M_DEF(double_torus_to_single_seifert) = {
  ANIM_PS_REF(double_torus_to_single_seifert_rot_z),
  ANIM_PS_REF(double_torus_to_single_seifert_rot_rnd)
};

ANIMS_DEF(double_torus_to_single_seifert) = {
  ANIMS_M_NUM(double_torus_to_single_seifert),
  ANIMS_M_NAME(double_torus_to_single_seifert)
};



/* The set of possible animations for the transition from a double torus
   to a triple Seifert surface. */

/* Rotate the two tori around the z axis, split off two tori from the tori
   at latitide ±45°, decrease their sector from 360° to 180°, and move them
   to the equator. */
ANIM_SO_DEF(double_torus_to_triple_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, M_PI_F, 0.0f,         EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, M_PI_F/2.0f, 0.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, M_PI_F/2.0f, 0.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, M_PI_F, 0.0f,        EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_triple_seifert_rot_z) = {
  ANIM_SO_NUM(double_torus_to_triple_seifert_rot_z),
  ANIM_SO_NAME(double_torus_to_triple_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_to_triple_seifert_rot_z) = {
  ANIM_MO_REF(double_torus_to_triple_seifert_rot_z)
};

ANIM_PS_DEF(double_torus_to_triple_seifert_rot_z) = {
  ANIM_PH_NUM(double_torus_to_triple_seifert_rot_z),
  ANIM_PH_NAME(double_torus_to_triple_seifert_rot_z)
};

/* Split off two tori from the tori at latitude ±45°, decrease their sector
   from 360° to 180°, move them to the equator, and rotate all Seifert
   surfaces around a random axis with a certain probability. */
ANIM_SO_DEF(double_torus_to_triple_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_triple_seifert_rot_rnd) = {
  ANIM_SO_NUM(double_torus_to_triple_seifert_rot_rnd),
  ANIM_SO_NAME(double_torus_to_triple_seifert_rot_rnd),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(double_torus_to_triple_seifert_rot_rnd) = {
  ANIM_MO_REF(double_torus_to_triple_seifert_rot_rnd)
};

ANIM_PS_DEF(double_torus_to_triple_seifert_rot_rnd) = {
  ANIM_PH_NUM(double_torus_to_triple_seifert_rot_rnd),
  ANIM_PH_NAME(double_torus_to_triple_seifert_rot_rnd)
};

ANIMS_M_DEF(double_torus_to_triple_seifert) = {
  ANIM_PS_REF(double_torus_to_triple_seifert_rot_z),
  ANIM_PS_REF(double_torus_to_triple_seifert_rot_rnd)
};

ANIMS_DEF(double_torus_to_triple_seifert) = {
  ANIMS_M_NUM(double_torus_to_triple_seifert),
  ANIMS_M_NAME(double_torus_to_triple_seifert)
};



/* The set of possible animations for the transition from a double torus
   to a Hopf torus. */

/* Decrease the density of the two tori by a factor of two, deform them to
   an interleaved Hopf torus, and increase the density of the Hopf torus by
   a factor of five. */

/* Phase 1: Decrease the density of the tori at ±45°. */
ANIM_SO_DEF(double_torus_to_loose_double_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/12.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/12.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/12.0f,         M_PI_F/12.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_loose_double_torus) = {
  ANIM_SO_NUM(double_torus_to_loose_double_torus),
  ANIM_SO_NAME(double_torus_to_loose_double_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Deform the two to loose tori an interleaved Hopf torus. */
ANIM_SO_DEF(loose_double_torus_to_loose_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/12.0f,         M_PI_F/12.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(loose_double_torus_to_loose_hopf_torus) = {
  ANIM_SO_NUM(loose_double_torus_to_loose_hopf_torus),
  ANIM_SO_NAME(loose_double_torus_to_loose_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

/* Phase 3: Increase the density of the Hopf torus by a factor of five. */
ANIM_SO_DEF(loose_hopf_torus_to_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -2.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/60.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/60.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 2.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(loose_hopf_torus_to_hopf_torus) = {
  ANIM_SO_NUM(loose_hopf_torus_to_hopf_torus),
  ANIM_SO_NAME(loose_hopf_torus_to_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

ANIM_PH_DEF(double_torus_to_single_hopf_torus_densify) = {
  ANIM_MO_REF(double_torus_to_loose_double_torus),
  ANIM_MO_REF(loose_double_torus_to_loose_hopf_torus),
  ANIM_MO_REF(loose_hopf_torus_to_hopf_torus)
};

ANIM_PS_DEF(double_torus_to_single_hopf_torus_densify) = {
  ANIM_PH_NUM(double_torus_to_single_hopf_torus_densify),
  ANIM_PH_NAME(double_torus_to_single_hopf_torus_densify)
};

ANIMS_M_DEF(double_torus_to_single_hopf_torus) = {
  ANIM_PS_REF(double_torus_to_single_hopf_torus_densify)
};

ANIMS_DEF(double_torus_to_single_hopf_torus) = {
  ANIMS_M_NUM(double_torus_to_single_hopf_torus),
  ANIMS_M_NAME(double_torus_to_single_hopf_torus)
};



/* The set of possible animations for the transition from a double torus
   to a single Hopf spiral. */

/* Deform two tori at latitiude ±45° to two Hopf spirals, merge them, and
   increase the point density by a factor of three halves. */

/* Phase 1: Deform two tori at latitiude ±45° to two Hopf spirals and merge
   them. */
ANIM_SO_DEF(double_torus_to_single_loose_hopf_spiral_merge) = {
  {
    GEN_SPIRAL,                                               /* generator */
    -M_PI_F/4.0f,         0.0f,                 EASING_CUBIC, /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    M_PI_F/4.0f,          0.0f,                 EASING_CUBIC, /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(double_torus_to_single_loose_hopf_spiral_merge) = {
  ANIM_SO_NUM(double_torus_to_single_loose_hopf_spiral_merge),
  ANIM_SO_NAME(double_torus_to_single_loose_hopf_spiral_merge),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 2: Increase the density of the Hopf spiral by a factor of three
   halves. */
ANIM_SO_DEF(single_loose_hopf_spiral_to_single_hopf_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -23.0f*M_PI_F/24.0f,  -35.0f*M_PI_F/36.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -23.0f*M_PI_F/24.0f,  -34.0f*M_PI_F/36.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_loose_hopf_spiral_to_single_hopf_spiral) = {
  ANIM_SO_NUM(single_loose_hopf_spiral_to_single_hopf_spiral),
  ANIM_SO_NAME(single_loose_hopf_spiral_to_single_hopf_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

ANIM_PH_DEF(double_torus_to_single_hopf_spiral_merge) = {
  ANIM_MO_REF(double_torus_to_single_loose_hopf_spiral_merge),
  ANIM_MO_REF(single_loose_hopf_spiral_to_single_hopf_spiral)
};

ANIM_PS_DEF(double_torus_to_single_hopf_spiral_merge) = {
  ANIM_PH_NUM(double_torus_to_single_hopf_spiral_merge),
  ANIM_PH_NAME(double_torus_to_single_hopf_spiral_merge)
};

ANIMS_M_DEF(double_torus_to_single_hopf_spiral) = {
  ANIM_PS_REF(double_torus_to_single_hopf_spiral_merge)
};

ANIMS_DEF(double_torus_to_single_hopf_spiral) = {
  ANIMS_M_NUM(double_torus_to_single_hopf_spiral),
  ANIMS_M_NAME(double_torus_to_single_hopf_spiral)
};



/*****************************************************************************
 * The set of all transformations from a triple torus.
 *****************************************************************************/

/* The set of possible animations for the transition from a triple torus
   to a single point. */

/* Rotate two tori at latitude ±45° around the z axis, shrink them to a
   single point, and move them to the equator.  Shrink the torus on the
   equator to a single point without rotating it. */
ANIM_SO_DEF(triple_torus_to_single_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    -2.0f*M_PI_F,         0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_single_point_rot_z) = {
  ANIM_SO_NUM(triple_torus_to_single_point_rot_z),
  ANIM_SO_NAME(triple_torus_to_single_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_single_point_rot_z) = {
  ANIM_MO_REF(triple_torus_to_single_point_rot_z)
};

ANIM_PS_DEF(triple_torus_to_single_point_rot_z) = {
  ANIM_PH_NUM(triple_torus_to_single_point_rot_z),
  ANIM_PH_NAME(triple_torus_to_single_point_rot_z)
};

/* Rotate three tori around a random axis, move the two tori at latitude
   ±45° to the equator, and shrink all three tori to a single point. */
ANIM_SO_DEF(triple_torus_to_single_point_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    -2.0f*M_PI_F,         0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_single_point_rot_rnd) = {
  ANIM_SO_NUM(triple_torus_to_single_point_rot_rnd),
  ANIM_SO_NAME(triple_torus_to_single_point_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_single_point_rot_rnd) = {
  ANIM_MO_REF(triple_torus_to_single_point_rot_rnd)
};

ANIM_PS_DEF(triple_torus_to_single_point_rot_rnd) = {
  ANIM_PH_NUM(triple_torus_to_single_point_rot_rnd),
  ANIM_PH_NAME(triple_torus_to_single_point_rot_rnd)
};

ANIMS_M_DEF(triple_torus_to_single_point) = {
  ANIM_PS_REF(triple_torus_to_single_point_rot_z),
  ANIM_PS_REF(triple_torus_to_single_point_rot_rnd)
};

ANIMS_DEF(triple_torus_to_single_point) = {
  ANIMS_M_NUM(triple_torus_to_single_point),
  ANIMS_M_NAME(triple_torus_to_single_point)
};



/* The set of possible animations for the transition from a triple torus
   to a single torus. */

/* Rotate two tori at latitude ±45° around the z axis and interleave them
   into a dense torus at the equator, then reduce the point density by a
   factor of three. */

/* Phase 1: Rotate two tori at latitude ±45° around the z axis and merge
   them into a dense torus at the equator. */
ANIM_SO_DEF(triple_torus_to_single_dense_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/36.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/36.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_single_dense_torus_rot_z) = {
  ANIM_SO_NUM(triple_torus_to_single_dense_torus_rot_z),
  ANIM_SO_NAME(triple_torus_to_single_dense_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 2: Reduce the point density by a factor of three. */
ANIM_SO_DEF(single_dense_torus_to_single_torus_loosen_three) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/36.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/36.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_torus_to_single_torus_loosen_three) = {
  ANIM_SO_NUM(single_dense_torus_to_single_torus_loosen_three),
  ANIM_SO_NAME(single_dense_torus_to_single_torus_loosen_three),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  30                                                          /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_single_torus_rot_z) = {
  ANIM_MO_REF(triple_torus_to_single_dense_torus_rot_z),
  ANIM_MO_REF(single_dense_torus_to_single_torus_loosen_three)
};

ANIM_PS_DEF(triple_torus_to_single_torus_rot_z) = {
  ANIM_PH_NUM(triple_torus_to_single_torus_rot_z),
  ANIM_PH_NAME(triple_torus_to_single_torus_rot_z)
};

/* Rotate three tori around a random axis and move the tori at latitudes
   ±45° to the equator. */
ANIM_SO_DEF(triple_torus_to_single_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_single_torus_rot_rnd) = {
  ANIM_SO_NUM(triple_torus_to_single_torus_rot_rnd),
  ANIM_SO_NAME(triple_torus_to_single_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_single_torus_rot_rnd) = {
  ANIM_MO_REF(triple_torus_to_single_torus_rot_rnd)
};

ANIM_PS_DEF(triple_torus_to_single_torus_rot_rnd) = {
  ANIM_PH_NUM(triple_torus_to_single_torus_rot_rnd),
  ANIM_PH_NAME(triple_torus_to_single_torus_rot_rnd)
};

ANIMS_M_DEF(triple_torus_to_single_torus) = {
  ANIM_PS_REF(triple_torus_to_single_torus_rot_z),
  ANIM_PS_REF(triple_torus_to_single_torus_rot_rnd)
};

ANIMS_DEF(triple_torus_to_single_torus) = {
  ANIMS_M_NUM(triple_torus_to_single_torus),
  ANIMS_M_NAME(triple_torus_to_single_torus)
};



/* The set of possible animations for the transition from a triple torus
   to a double torus. */

/* Rotate the three tori around the z axis, split the torus at the equator
   into two tori, and move them to latitude ±45°. */
ANIM_SO_DEF(triple_torus_to_double_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/12.0f,         M_PI_F/12.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_double_torus_rot_z) = {
  ANIM_SO_NUM(triple_torus_to_double_torus_rot_z),
  ANIM_SO_NAME(triple_torus_to_double_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_double_torus_rot_z) = {
  ANIM_MO_REF(triple_torus_to_double_torus_rot_z)
};

ANIM_PS_DEF(triple_torus_to_double_torus_rot_z) = {
  ANIM_PH_NUM(triple_torus_to_double_torus_rot_z),
  ANIM_PH_NAME(triple_torus_to_double_torus_rot_z)
};

/* Rotate three tori at around a random axis, split the torus at the
   equator into two tori, and move them to latitude ±45°. */
ANIM_SO_DEF(triple_torus_to_double_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_double_torus_rot_rnd) = {
  ANIM_SO_NUM(triple_torus_to_double_torus_rot_rnd),
  ANIM_SO_NAME(triple_torus_to_double_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_double_torus_rot_rnd) = {
  ANIM_MO_REF(triple_torus_to_double_torus_rot_rnd)
};

ANIM_PS_DEF(triple_torus_to_double_torus_rot_rnd) = {
  ANIM_PH_NUM(triple_torus_to_double_torus_rot_rnd),
  ANIM_PH_NAME(triple_torus_to_double_torus_rot_rnd)
};

ANIMS_M_DEF(triple_torus_to_double_torus) = {
  ANIM_PS_REF(triple_torus_to_double_torus_rot_z),
  ANIM_PS_REF(triple_torus_to_double_torus_rot_rnd)
};

ANIMS_DEF(triple_torus_to_double_torus) = {
  ANIMS_M_NUM(triple_torus_to_double_torus),
  ANIMS_M_NAME(triple_torus_to_double_torus)
};



/* The set of possible animations for a triple torus. */

/* Rotate three tori at latitudes ±45° and 0° around the x axis of the
   total space while rotating them around the z axis in different
   directions. */
ANIM_SO_DEF(triple_torus_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_rot_x) = {
  ANIM_SO_NUM(triple_torus_rot_x),
  ANIM_SO_NAME(triple_torus_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_rot_x) = {
  ANIM_MO_REF(triple_torus_rot_x)
};

ANIM_PS_DEF(triple_torus_rot_x) = {
  ANIM_PH_NUM(triple_torus_rot_x),
  ANIM_PH_NAME(triple_torus_rot_x)
};

/* Rotate the three tori to a a vertical orientation, yielding a parabolic
   ring cyclide and two interlocking tori, and increase their density by a
   factor of three.  Then, rotate them around the x axis of the total space.
   Next, move the two tori not on the equator between latitudes 1° and ±89°,
   showing that two solid interlocking tori fill up the complete S³.
   Finally, rotate the tori back to 0° and ±45° and decrease their density
   by a factor of three. */

/* Phase 1: Rotate the tori by 90° to a vertical position, increase their
   point density by a factor of three, and rotate them around the x axis
   of the total space by 90°. */
ANIM_SO_DEF(triple_torus_to_vertical_triple_torus_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/36.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/36.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/36.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/36.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/36.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/36.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_vertical_triple_torus_densify) = {
  ANIM_SO_NUM(triple_torus_to_vertical_triple_torus_densify),
  ANIM_SO_NAME(triple_torus_to_vertical_triple_torus_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { -1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,     EASING_CUBIC, /* rot_space */
  120                                                         /* num_steps */
};

/* Phase 2: Rotate the three tori by 360° around the x axis of the total
   space. */
ANIM_SO_DEF(vertical_triple_torus_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(vertical_triple_torus_rot_x) = {
  ANIM_SO_NUM(vertical_triple_torus_rot_x),
  ANIM_SO_NAME(vertical_triple_torus_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 5.0f*M_PI_F/2.0f,
                                                EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 3: Move the two tori at latitides ±45° between latitudes 1° and
   ±89°. */
ANIM_SO_DEF(vertical_triple_torus_move_z) = {
  {
    GEN_TORUS,                                                /* generator */
    179.0f*M_PI_F/180.0f, 91.0f*M_PI_F/180.0f,  EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    1.0f*M_PI_F/180.0f,   89.0f*M_PI_F/180.0f,  EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(vertical_triple_torus_move_z) = {
  ANIM_SO_NUM(vertical_triple_torus_move_z),
  ANIM_SO_NAME(vertical_triple_torus_move_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 4: Rotate the tori back to 0° and ±45° and decrease their density
   by a factor of three. */
ANIM_SO_DEF(vertical_triple_torus_to_triple_torus_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/36.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/36.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/36.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/36.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/36.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/36.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F,  EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(vertical_triple_torus_to_triple_torus_loosen) = {
  ANIM_SO_NUM(vertical_triple_torus_to_triple_torus_loosen),
  ANIM_SO_NAME(vertical_triple_torus_to_triple_torus_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,     EASING_CUBIC, /* rot_space */
  120                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_three_sphere) = {
  ANIM_MO_REF(triple_torus_to_vertical_triple_torus_densify),
  ANIM_MO_REF(vertical_triple_torus_rot_x),
  ANIM_MO_REF(vertical_triple_torus_move_z),
  ANIM_MO_REF(vertical_triple_torus_to_triple_torus_loosen)
};

ANIM_PS_DEF(triple_torus_three_sphere) = {
  ANIM_PH_NUM(triple_torus_three_sphere),
  ANIM_PH_NAME(triple_torus_three_sphere)
};

/* Move the two tori at latitudes ±45° to latitude ±70° and increase the
   point density of all three tori by a factor of two, rotate the tori
   around a random axis, move the two tori at latitude ±70° back to ±45°,
   and decrease the point density of all three tori by a factor of two. */

/* Phase 1: Move two tori at latitude ±45° to latitude ±70° and increase
   the point density of all three tori by a factor of two. */
ANIM_SO_DEF(triple_torus_rot_rnd_move_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/48.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/48.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/48.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/48.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F/48.0f,        EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/48.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_rot_rnd_move_densify) = {
  ANIM_SO_NUM(triple_torus_rot_rnd_move_densify),
  ANIM_SO_NAME(triple_torus_rot_rnd_move_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Rotate the two tori by 360° around a random axis. */
ANIM_SO_DEF(triple_torus_rot_rnd_rot) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         M_PI_F/48.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         M_PI_F/48.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     8.0f*M_PI_F/9.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         M_PI_F/48.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_rot_rnd_rot) = {
  ANIM_SO_NUM(triple_torus_rot_rnd_rot),
  ANIM_SO_NAME(triple_torus_rot_rnd_rot),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 3: Move the two tori at latitude ±70° to latitude ±45° and
   decrease the point density of all three tori by a factor of two. */
ANIM_SO_DEF(triple_torus_rot_rnd_move_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/48.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/48.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/48.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_rot_rnd_move_loosen) = {
  ANIM_SO_NUM(triple_torus_rot_rnd_move_loosen),
  ANIM_SO_NAME(triple_torus_rot_rnd_move_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

ANIM_PH_DEF(triple_torus_rot_rnd) = {
  ANIM_MO_REF(triple_torus_rot_rnd_move_densify),
  ANIM_MO_REF(triple_torus_rot_rnd_rot),
  ANIM_MO_REF(triple_torus_rot_rnd_move_loosen)
};

ANIM_PS_DEF(triple_torus_rot_rnd) = {
  ANIM_PH_NUM(triple_torus_rot_rnd),
  ANIM_PH_NAME(triple_torus_rot_rnd)
};

/* Move the two tori at latitude ±45° to latitude ±5°, to latitude ±85° and
   back to latitude ±45° while rotating all three tori around the z axis
   and, with a certain probability, around a random axis. */
ANIM_SO_DEF(triple_torus_move) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/18.0f,         8.0f*M_PI_F/18.0f,    EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    17.0f*M_PI_F/18.0f,   10.0f*M_PI_F/18.0f,   EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_move) = {
  ANIM_SO_NUM(triple_torus_move),
  ANIM_SO_NAME(triple_torus_move),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_move) = {
  ANIM_MO_REF(triple_torus_move)
};

ANIM_PS_DEF(triple_torus_move) = {
  ANIM_PH_NUM(triple_torus_move),
  ANIM_PH_NAME(triple_torus_move)
};

ANIMS_M_DEF(triple_torus) = {
  ANIM_PS_REF(triple_torus_rot_x),
  ANIM_PS_REF(triple_torus_three_sphere),
  ANIM_PS_REF(triple_torus_rot_rnd),
  ANIM_PS_REF(triple_torus_move)
};

ANIMS_DEF(triple_torus) = {
  ANIMS_M_NUM(triple_torus),
  ANIMS_M_NAME(triple_torus)
};



/* The set of possible animations for the transition from a triple torus
   to a single Seifert surface. */

/* Shrink the three tori from 360° sectors to 60° sectors, join them at the
   equator, and reduce the point density by a factor of three. */

/* Phase 1: Shrink the three tori from 360° sectors to 60° sectors and join
   them at the equator. */
ANIM_SO_DEF(triple_torus_to_single_dense_seifert_three_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -3.0f*M_PI_F/2.0f,    -25.0f*M_PI_F/72.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F/3.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -3.0f*M_PI_F/2.0f,    -49.0f*M_PI_F/72.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F/3.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -3.0f*M_PI_F/2.0f,    -73.0f*M_PI_F/72.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F/3.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_single_dense_seifert_three_rot_z) = {
  ANIM_SO_NUM(triple_torus_to_single_dense_seifert_three_rot_z),
  ANIM_SO_NAME(triple_torus_to_single_dense_seifert_three_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 2: Reduce the point density by a factor of three. */
ANIM_SO_DEF(single_dense_seifert_three_to_single_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    71.0f*M_PI_F/72.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    73.0f*M_PI_F/72.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_seifert_three_to_single_seifert_rot_z) = {
  ANIM_SO_NUM(single_dense_seifert_three_to_single_seifert_rot_z),
  ANIM_SO_NAME(single_dense_seifert_three_to_single_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  30                                                          /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_single_seifert_rot_z) = {
  ANIM_MO_REF(triple_torus_to_single_dense_seifert_three_rot_z),
  ANIM_MO_REF(single_dense_seifert_three_to_single_seifert_rot_z)
};

ANIM_PS_DEF(triple_torus_to_single_seifert_rot_z) = {
  ANIM_PH_NUM(triple_torus_to_single_seifert_rot_z),
  ANIM_PH_NAME(triple_torus_to_single_seifert_rot_z)
};

/* Shrink the three tori from 360° sectors to 180° sectors, reduce their
   point point density by a factor of three, interleave them at the
   equator, and rotate around a random axis. */
ANIM_SO_DEF(triple_torus_to_single_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    13.0f*M_PI_F/12.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    14.0f*M_PI_F/12.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    13.0f*M_PI_F/12.0f,   25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    14.0f*M_PI_F/12.0f,   25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    13.0f*M_PI_F/12.0f,   26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    14.0f*M_PI_F/12.0f,   26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_single_seifert_rot_rnd) = {
  ANIM_SO_NUM(triple_torus_to_single_seifert_rot_rnd),
  ANIM_SO_NAME(triple_torus_to_single_seifert_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_single_seifert_rot_rnd) = {
  ANIM_MO_REF(triple_torus_to_single_seifert_rot_rnd)
};

ANIM_PS_DEF(triple_torus_to_single_seifert_rot_rnd) = {
  ANIM_PH_NUM(triple_torus_to_single_seifert_rot_rnd),
  ANIM_PH_NAME(triple_torus_to_single_seifert_rot_rnd)
};

ANIMS_M_DEF(triple_torus_to_single_seifert) = {
  ANIM_PS_REF(triple_torus_to_single_seifert_rot_z),
  ANIM_PS_REF(triple_torus_to_single_seifert_rot_rnd)
};

ANIMS_DEF(triple_torus_to_single_seifert) = {
  ANIMS_M_NUM(triple_torus_to_single_seifert),
  ANIMS_M_NAME(triple_torus_to_single_seifert)
};



/* The set of possible animations for the transition from a triple torus
   to a triple Seifert surface. */

/* Rotate the three tori around the z axis and decrease their sector from
   360° to 180°. */
ANIM_SO_DEF(triple_torus_to_triple_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, -M_PI_F/2.0f, 2.0f*M_PI_F,
                                                EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, -M_PI_F/2.0f, 2.0f*M_PI_F,
                                                EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_triple_seifert_rot_z) = {
  ANIM_SO_NUM(triple_torus_to_triple_seifert_rot_z),
  ANIM_SO_NAME(triple_torus_to_triple_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_triple_seifert_rot_z) = {
  ANIM_MO_REF(triple_torus_to_triple_seifert_rot_z)
};

ANIM_PS_DEF(triple_torus_to_triple_seifert_rot_z) = {
  ANIM_PH_NUM(triple_torus_to_triple_seifert_rot_z),
  ANIM_PH_NAME(triple_torus_to_triple_seifert_rot_z)
};

/* Decrease the sector of the tori from 360° to 180° and rotate them around
   a random axis with a certain probability. */
ANIM_SO_DEF(triple_torus_to_triple_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_triple_seifert_rot_rnd) = {
  ANIM_SO_NUM(triple_torus_to_triple_seifert_rot_rnd),
  ANIM_SO_NAME(triple_torus_to_triple_seifert_rot_rnd),
  0.75f,                                        EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_triple_seifert_rot_rnd) = {
  ANIM_MO_REF(triple_torus_to_triple_seifert_rot_rnd)
};

ANIM_PS_DEF(triple_torus_to_triple_seifert_rot_rnd) = {
  ANIM_PH_NUM(triple_torus_to_triple_seifert_rot_rnd),
  ANIM_PH_NAME(triple_torus_to_triple_seifert_rot_rnd)
};

ANIMS_M_DEF(triple_torus_to_triple_seifert) = {
  ANIM_PS_REF(triple_torus_to_triple_seifert_rot_z),
  ANIM_PS_REF(triple_torus_to_triple_seifert_rot_rnd)
};

ANIMS_DEF(triple_torus_to_triple_seifert) = {
  ANIMS_M_NUM(triple_torus_to_triple_seifert),
  ANIMS_M_NAME(triple_torus_to_triple_seifert)
};



/* The set of possible animations for the transition from a triple torus
   to a Hopf torus. */

/* Transform the three tori to a Hopf torus on the equator while increasing
   the point density of the two tori at latitude ±45° by a factor of two. */
ANIM_SO_DEF(triple_torus_to_single_hopf_torus_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -2.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 -1.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 1.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 2.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_single_hopf_torus_densify) = {
  ANIM_SO_NUM(triple_torus_to_single_hopf_torus_densify),
  ANIM_SO_NAME(triple_torus_to_single_hopf_torus_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  120                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_single_hopf_torus_densify) = {
  ANIM_MO_REF(triple_torus_to_single_hopf_torus_densify)
};

ANIM_PS_DEF(triple_torus_to_single_hopf_torus_densify) = {
  ANIM_PH_NUM(triple_torus_to_single_hopf_torus_densify),
  ANIM_PH_NAME(triple_torus_to_single_hopf_torus_densify)
};

ANIMS_M_DEF(triple_torus_to_single_hopf_torus) = {
  ANIM_PS_REF(triple_torus_to_single_hopf_torus_densify)
};

ANIMS_DEF(triple_torus_to_single_hopf_torus) = {
  ANIMS_M_NUM(triple_torus_to_single_hopf_torus),
  ANIMS_M_NAME(triple_torus_to_single_hopf_torus)
};



/* The set of possible animations for the transition from a triple torus
   to a single Hopf spiral. */

/* Deform the three tori to three Hopf spirals and merge them. */
ANIM_SO_DEF(triple_torus_to_single_hopf_spiral_merge) = {
  {
    GEN_SPIRAL,                                               /* generator */
    -M_PI_F/4.0f,         0.0f,                 EASING_CUBIC, /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 -M_PI_F,              EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F/2.0f,         -M_PI_F/3.0f,         EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    M_PI_F/4.0f,          0.0f,                 EASING_CUBIC, /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              M_PI_F/3.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_torus_to_single_hopf_spiral_merge) = {
  ANIM_SO_NUM(triple_torus_to_single_hopf_spiral_merge),
  ANIM_SO_NAME(triple_torus_to_single_hopf_spiral_merge),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_torus_to_single_hopf_spiral_merge) = {
  ANIM_MO_REF(triple_torus_to_single_hopf_spiral_merge)
};

ANIM_PS_DEF(triple_torus_to_single_hopf_spiral_merge) = {
  ANIM_PH_NUM(triple_torus_to_single_hopf_spiral_merge),
  ANIM_PH_NAME(triple_torus_to_single_hopf_spiral_merge)
};

ANIMS_M_DEF(triple_torus_to_single_hopf_spiral) = {
  ANIM_PS_REF(triple_torus_to_single_hopf_spiral_merge)
};

ANIMS_DEF(triple_torus_to_single_hopf_spiral) = {
  ANIMS_M_NUM(triple_torus_to_single_hopf_spiral),
  ANIMS_M_NAME(triple_torus_to_single_hopf_spiral)
};



/*****************************************************************************
 * The set of all transformations from a single Seifert surface.
 *****************************************************************************/

/* The set of possible animations for the transition from a single Seifert
   surface to a single point. */

/* Shrink a Seifert surface on the equator to a point on the equator. */
ANIM_SO_DEF(single_seifert_to_single_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_point_rot_z) = {
  ANIM_SO_NUM(single_seifert_to_single_point_rot_z),
  ANIM_SO_NAME(single_seifert_to_single_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_single_point_rot_z) = {
  ANIM_MO_REF(single_seifert_to_single_point_rot_z)
};

ANIM_PS_DEF(single_seifert_to_single_point_rot_z) = {
  ANIM_PH_NUM(single_seifert_to_single_point_rot_z),
  ANIM_PH_NAME(single_seifert_to_single_point_rot_z)
};

/* Shrink a Seifert surface  on the equator to a point on the equator and
   rotate around a random axis. */
ANIM_SO_DEF(single_seifert_to_single_point_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_point_rot_rnd) = {
  ANIM_SO_NUM(single_seifert_to_single_point_rot_rnd),
  ANIM_SO_NAME(single_seifert_to_single_point_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_single_point_rot_rnd) = {
  ANIM_MO_REF(single_seifert_to_single_point_rot_rnd)
};

ANIM_PS_DEF(single_seifert_to_single_point_rot_rnd) = {
  ANIM_PH_NUM(single_seifert_to_single_point_rot_rnd),
  ANIM_PH_NAME(single_seifert_to_single_point_rot_rnd)
};

ANIMS_M_DEF(single_seifert_to_single_point) = {
  ANIM_PS_REF(single_seifert_to_single_point_rot_z),
  ANIM_PS_REF(single_seifert_to_single_point_rot_rnd)
};

ANIMS_DEF(single_seifert_to_single_point) = {
  ANIMS_M_NUM(single_seifert_to_single_point),
  ANIMS_M_NAME(single_seifert_to_single_point)
};



/* The set of possible animations for the transition from a single Seifert
   surface to a single torus. */

/* Expand the Seifert surface on the equator to a torus on the equator. */
ANIM_SO_DEF(single_seifert_to_single_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_torus_rot_z) = {
  ANIM_SO_NUM(single_seifert_to_single_torus_rot_z),
  ANIM_SO_NAME(single_seifert_to_single_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_single_torus_rot_z) = {
  ANIM_MO_REF(single_seifert_to_single_torus_rot_z)
};

ANIM_PS_DEF(single_seifert_to_single_torus_rot_z) = {
  ANIM_PH_NUM(single_seifert_to_single_torus_rot_z),
  ANIM_PH_NAME(single_seifert_to_single_torus_rot_z)
};

/* Expand the Seifert surface on the equator to a torus on the equator and
   rotate it around a random axis. */
ANIM_SO_DEF(single_seifert_to_single_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_torus_rot_rnd) = {
  ANIM_SO_NUM(single_seifert_to_single_torus_rot_rnd),
  ANIM_SO_NAME(single_seifert_to_single_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_single_torus_rot_rnd) = {
  ANIM_MO_REF(single_seifert_to_single_torus_rot_rnd)
};

ANIM_PS_DEF(single_seifert_to_single_torus_rot_rnd) = {
  ANIM_PH_NUM(single_seifert_to_single_torus_rot_rnd),
  ANIM_PH_NAME(single_seifert_to_single_torus_rot_rnd)
};

/* Flip half of a Seifert surface on the equator around a suitable axis on
   the equator to form a torus on the equator. */
ANIM_SO_DEF(single_seifert_to_single_torus_flip) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.99785892f, -0.06540313f, 0.0f },
                          M_PI_F, 0.0f,         EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_torus_flip) = {
  ANIM_SO_NUM(single_seifert_to_single_torus_flip),
  ANIM_SO_NAME(single_seifert_to_single_torus_flip),
  0.25f,                                        EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_single_torus_flip) = {
  ANIM_MO_REF(single_seifert_to_single_torus_flip)
};

ANIM_PS_DEF(single_seifert_to_single_torus_flip) = {
  ANIM_PH_NUM(single_seifert_to_single_torus_flip),
  ANIM_PH_NAME(single_seifert_to_single_torus_flip)
};

ANIMS_M_DEF(single_seifert_to_single_torus) = {
  ANIM_PS_REF(single_seifert_to_single_torus_rot_z),
  ANIM_PS_REF(single_seifert_to_single_torus_rot_rnd),
  ANIM_PS_REF(single_seifert_to_single_torus_flip)
};

ANIMS_DEF(single_seifert_to_single_torus) = {
  ANIMS_M_NUM(single_seifert_to_single_torus),
  ANIMS_M_NAME(single_seifert_to_single_torus)
};



/* The set of possible animations for the transition from a single Seifert
   surface to a double torus. */

/* Increase the point density of the two Seifert surfaces by a factor of
   two, expand them from 90° sectors to 360° sectors, and split them at
   the equator. */

/* Phase 1: Increase the point density by a factor of two. */
ANIM_SO_DEF(single_seifert_to_single_dense_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               95.0f*M_PI_F/96.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               97.0f*M_PI_F/96.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_dense_seifert_rot_z) = {
  ANIM_SO_NUM(single_seifert_to_single_dense_seifert_rot_z),
  ANIM_SO_NAME(single_seifert_to_single_dense_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  30                                                          /* num_steps */
};

/* Phase 2: Expand the two Seifert surfaces from 90° sectors to 360°
   sectors and split them at the equator. */
ANIM_SO_DEF(single_dense_seifert_to_double_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -49.0f*M_PI_F/96.0f,  -M_PI_F/2.0f,         EASING_CUBIC, /* offset */
    M_PI_F/2.0f,          2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -51.0f*M_PI_F/96.0f,  -M_PI_F/2.0f,         EASING_CUBIC, /* offset */
    -M_PI_F/2.0f,         -2.0f*M_PI_F,         EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_seifert_to_double_torus_rot_z) = {
  ANIM_SO_NUM(single_dense_seifert_to_double_torus_rot_z),
  ANIM_SO_NAME(single_dense_seifert_to_double_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_double_torus_rot_z) = {
  ANIM_MO_REF(single_seifert_to_single_dense_seifert_rot_z),
  ANIM_MO_REF(single_dense_seifert_to_double_torus_rot_z)
};

ANIM_PS_DEF(single_seifert_to_double_torus_rot_z) = {
  ANIM_PH_NUM(single_seifert_to_double_torus_rot_z),
  ANIM_PH_NAME(single_seifert_to_double_torus_rot_z)
};

/* Split the two Seifert surfaces at the equator, expand them from 180°
   sectors to 360° sectors, increase their point density by a factor of two,
   and rotate around a random axis. */
ANIM_SO_DEF(single_seifert_to_double_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               13.0f*M_PI_F/12.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   13.0f*M_PI_F/12.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_double_torus_rot_rnd) = {
  ANIM_SO_NUM(single_seifert_to_double_torus_rot_rnd),
  ANIM_SO_NAME(single_seifert_to_double_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_double_torus_rot_rnd) = {
  ANIM_MO_REF(single_seifert_to_double_torus_rot_rnd)
};

ANIM_PS_DEF(single_seifert_to_double_torus_rot_rnd) = {
  ANIM_PH_NUM(single_seifert_to_double_torus_rot_rnd),
  ANIM_PH_NAME(single_seifert_to_double_torus_rot_rnd)
};

ANIMS_M_DEF(single_seifert_to_double_torus) = {
  ANIM_PS_REF(single_seifert_to_double_torus_rot_z),
  ANIM_PS_REF(single_seifert_to_double_torus_rot_rnd)
};

ANIMS_DEF(single_seifert_to_double_torus) = {
  ANIMS_M_NUM(single_seifert_to_double_torus),
  ANIMS_M_NAME(single_seifert_to_double_torus)
};



/* The set of possible animations for the transition from a single Seifert
   surface to a triple torus. */

/* Increase the point density of the three Seifert surfaces by a factor of
   three, expand them from 60° sectors to 360° sectors, and split them at
   the equator. */

/* Phase 1: Increase the point density by a factor of three. */
ANIM_SO_DEF(single_seifert_to_single_dense_seifert_three_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               71.0f*M_PI_F/72.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               73.0f*M_PI_F/72.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_dense_seifert_three_rot_z) = {
  ANIM_SO_NUM(single_seifert_to_single_dense_seifert_three_rot_z),
  ANIM_SO_NAME(single_seifert_to_single_dense_seifert_three_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  30                                                          /* num_steps */
};

/* Phase 2: Expand the three Seifert surfaces from 60° sectors to 360°
   sectors and split them at the equator. */
ANIM_SO_DEF(single_dense_seifert_three_to_triple_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -25.0f*M_PI_F/72.0f,  -3.0f*M_PI_F/2.0f,    EASING_CUBIC, /* offset */
    M_PI_F/3.0f,          2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -49.0f*M_PI_F/72.0f,  -3.0f*M_PI_F/2.0f,    EASING_CUBIC, /* offset */
    M_PI_F/3.0f,          2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -73.0f*M_PI_F/72.0f,  -3.0f*M_PI_F/2.0f,    EASING_CUBIC, /* offset */
    M_PI_F/3.0f,          2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_seifert_three_to_triple_torus_rot_z) = {
  ANIM_SO_NUM(single_dense_seifert_three_to_triple_torus_rot_z),
  ANIM_SO_NAME(single_dense_seifert_three_to_triple_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_triple_torus_rot_z) = {
  ANIM_MO_REF(single_seifert_to_single_dense_seifert_three_rot_z),
  ANIM_MO_REF(single_dense_seifert_three_to_triple_torus_rot_z)
};

ANIM_PS_DEF(single_seifert_to_triple_torus_rot_z) = {
  ANIM_PH_NUM(single_seifert_to_triple_torus_rot_z),
  ANIM_PH_NAME(single_seifert_to_triple_torus_rot_z)
};

/* Split the three Seifert surfaces at the equator, expand them from 180°
   sectors to 360° sectors, increase their point density by a factor of
   three, and rotate around a random axis. */
ANIM_SO_DEF(single_seifert_to_triple_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               13.0f*M_PI_F/12.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               14.0f*M_PI_F/12.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   13.0f*M_PI_F/12.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   14.0f*M_PI_F/12.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   13.0f*M_PI_F/12.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   14.0f*M_PI_F/12.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_triple_torus_rot_rnd) = {
  ANIM_SO_NUM(single_seifert_to_triple_torus_rot_rnd),
  ANIM_SO_NAME(single_seifert_to_triple_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_triple_torus_rot_rnd) = {
  ANIM_MO_REF(single_seifert_to_triple_torus_rot_rnd)
};

ANIM_PS_DEF(single_seifert_to_triple_torus_rot_rnd) = {
  ANIM_PH_NUM(single_seifert_to_triple_torus_rot_rnd),
  ANIM_PH_NAME(single_seifert_to_triple_torus_rot_rnd)
};

ANIMS_M_DEF(single_seifert_to_triple_torus) = {
  ANIM_PS_REF(single_seifert_to_triple_torus_rot_z),
  ANIM_PS_REF(single_seifert_to_triple_torus_rot_rnd)
};

ANIMS_DEF(single_seifert_to_triple_torus) = {
  ANIMS_M_NUM(single_seifert_to_triple_torus),
  ANIMS_M_NAME(single_seifert_to_triple_torus)
};



/* The set of possible animations for a single Seifert surface. */

/* Rotate a Seifert surface on the x axis of the total space. */
ANIM_SO_DEF(single_seifert_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_rot_x) = {
  ANIM_SO_NUM(single_seifert_rot_x),
  ANIM_SO_NAME(single_seifert_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_rot_x) = {
  ANIM_MO_REF(single_seifert_rot_x)
};

ANIM_PS_DEF(single_seifert_rot_x) = {
  ANIM_PH_NUM(single_seifert_rot_x),
  ANIM_PH_NAME(single_seifert_rot_x)
};

/* Rotate a Seifert surface on the equator around the z axis. */
ANIM_SO_DEF(single_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_rot_z) = {
  ANIM_SO_NUM(single_seifert_rot_z),
  ANIM_SO_NAME(single_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_rot_z) = {
  ANIM_MO_REF(single_seifert_rot_z)
};

ANIM_PS_DEF(single_seifert_rot_z) = {
  ANIM_PH_NUM(single_seifert_rot_z),
  ANIM_PH_NAME(single_seifert_rot_z)
};

/* Rotate a Seifert surface on the equator around a random axis. */
ANIM_SO_DEF(single_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_rot_rnd) = {
  ANIM_SO_NUM(single_seifert_rot_rnd),
  ANIM_SO_NAME(single_seifert_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_rot_rnd) = {
  ANIM_MO_REF(single_seifert_rot_rnd)
};

ANIM_PS_DEF(single_seifert_rot_rnd) = {
  ANIM_PH_NUM(single_seifert_rot_rnd),
  ANIM_PH_NAME(single_seifert_rot_rnd)
};

/* Move a Seifert surface on the equator up and down to latitude ±80° while
   rotating it around the z axis and, with a certain probability, around a
   random axis. */
ANIM_SO_DEF(single_seifert_move) = {
  {
    GEN_TORUS,                                                /* generator */
    1.0f*M_PI_F/18.0f,    17.0f*M_PI_F/18.0f,   EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_move) = {
  ANIM_SO_NUM(single_seifert_move),
  ANIM_SO_NAME(single_seifert_move),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_move) = {
  ANIM_MO_REF(single_seifert_move)
};

ANIM_PS_DEF(single_seifert_move) = {
  ANIM_PH_NUM(single_seifert_move),
  ANIM_PH_NAME(single_seifert_move)
};

ANIMS_M_DEF(single_seifert) = {
  ANIM_PS_REF(single_seifert_rot_x),
  ANIM_PS_REF(single_seifert_rot_z),
  ANIM_PS_REF(single_seifert_rot_rnd),
  ANIM_PS_REF(single_seifert_move)
};

ANIMS_DEF(single_seifert) = {
  ANIMS_M_NUM(single_seifert),
  ANIMS_M_NAME(single_seifert)
};



/* The set of possible animations for the transition from a single Seifert
   surface to a triple Seifert surface. */

/* Increase the point density of the three Seifert surfaces by a factor of
   three, expand them from 60° sectors to 180° sectors, and split them at
   the equator. */

/* Phase 1: Increase the point density by a factor of three.  Already
   defined above. */

/* Phase 2: Expand the three Seifert surfaces from 60° sectors to 360°
   sectors and split them at the equator. */
ANIM_SO_DEF(single_dense_seifert_three_to_triple_seifert_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -25.0f*M_PI_F/72.0f,  -M_PI_F,              EASING_CUBIC, /* offset */
    M_PI_F/3.0f,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -49.0f*M_PI_F/72.0f,  -M_PI_F,              EASING_CUBIC, /* offset */
    M_PI_F/3.0f,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -73.0f*M_PI_F/72.0f,  -M_PI_F,              EASING_CUBIC, /* offset */
    M_PI_F/3.0f,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_seifert_three_to_triple_seifert_rot_z) = {
  ANIM_SO_NUM(single_dense_seifert_three_to_triple_seifert_rot_z),
  ANIM_SO_NAME(single_dense_seifert_three_to_triple_seifert_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_triple_seifert_rot_z) = {
  ANIM_MO_REF(single_seifert_to_single_dense_seifert_three_rot_z),
  ANIM_MO_REF(single_dense_seifert_three_to_triple_seifert_rot_z)
};

ANIM_PS_DEF(single_seifert_to_triple_seifert_rot_z) = {
  ANIM_PH_NUM(single_seifert_to_triple_seifert_rot_z),
  ANIM_PH_NAME(single_seifert_to_triple_seifert_rot_z)
};

/* Split the three Seifert surfaces at the equator, increase their point
   density by a factor of three, and rotate around a random axis. */
ANIM_SO_DEF(single_seifert_to_triple_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_triple_seifert_rot_rnd) = {
  ANIM_SO_NUM(single_seifert_to_triple_seifert_rot_rnd),
  ANIM_SO_NAME(single_seifert_to_triple_seifert_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_triple_seifert_rot_rnd) = {
  ANIM_MO_REF(single_seifert_to_triple_seifert_rot_rnd)
};

ANIM_PS_DEF(single_seifert_to_triple_seifert_rot_rnd) = {
  ANIM_PH_NUM(single_seifert_to_triple_seifert_rot_rnd),
  ANIM_PH_NAME(single_seifert_to_triple_seifert_rot_rnd)
};

ANIMS_M_DEF(single_seifert_to_triple_seifert) = {
  ANIM_PS_REF(single_seifert_to_triple_seifert_rot_z),
  ANIM_PS_REF(single_seifert_to_triple_seifert_rot_rnd)
};

ANIMS_DEF(single_seifert_to_triple_seifert) = {
  ANIMS_M_NUM(single_seifert_to_triple_seifert),
  ANIMS_M_NAME(single_seifert_to_triple_seifert)
};



/* The set of possible animations for the transition from a single Seifert
   surface to a single Hopf torus. */

/* Expand the Seifert surface on the equator, fill it to five times the
   density, and deform it to a Hopf torus. */
ANIM_SO_DEF(single_seifert_to_single_hopf_torus_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               61.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               62.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               63.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               64.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_hopf_torus_densify) = {
  ANIM_SO_NUM(single_seifert_to_single_hopf_torus_densify),
  ANIM_SO_NAME(single_seifert_to_single_hopf_torus_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_single_hopf_torus_densify) = {
  ANIM_MO_REF(single_seifert_to_single_hopf_torus_densify)
};

ANIM_PS_DEF(single_seifert_to_single_hopf_torus_densify) = {
  ANIM_PH_NUM(single_seifert_to_single_hopf_torus_densify),
  ANIM_PH_NAME(single_seifert_to_single_hopf_torus_densify)
};

ANIMS_M_DEF(single_seifert_to_single_hopf_torus) = {
  ANIM_PS_REF(single_seifert_to_single_hopf_torus_densify)
};

ANIMS_DEF(single_seifert_to_single_hopf_torus) = {
  ANIMS_M_NUM(single_seifert_to_single_hopf_torus),
  ANIMS_M_NAME(single_seifert_to_single_hopf_torus)
};



/* The set of possible animations for the transition from a Seifert surface
   to a single single Hopf spiral. */

/* Decrease the winding number of a Hopf spiral to one and collapse its
   height to a Seifert surface on the equator, and decrease the point
   density by a factor of three. */
ANIM_SO_DEF(single_seifert_to_single_hopf_spiral_wind_hor) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    1.0f,                 2.0f,                 EASING_CUBIC, /* r */
    M_PI_F/24.0f,         -M_PI_F,              EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    1.0f,                 2.0f,                 EASING_CUBIC, /* r */
    M_PI_F/24.0f,         -35.0f*M_PI_F/36.0f,  EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    1.0f,                 2.0f,                 EASING_CUBIC, /* r */
    M_PI_F/24.0f,         -34.0f*M_PI_F/36.0f , EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_hopf_spiral_wind_hor) = {
  ANIM_SO_NUM(single_seifert_to_single_hopf_spiral_wind_hor),
  ANIM_SO_NAME(single_seifert_to_single_hopf_spiral_wind_hor),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_single_hopf_spiral_wind_hor) = {
  ANIM_MO_REF(single_seifert_to_single_hopf_spiral_wind_hor)
};

ANIM_PS_DEF(single_seifert_to_single_hopf_spiral_wind_hor) = {
  ANIM_PH_NUM(single_seifert_to_single_hopf_spiral_wind_hor),
  ANIM_PH_NAME(single_seifert_to_single_hopf_spiral_wind_hor)
};

/* Increase the point density of a Seifert surface on the equator by a
   factor of three and rotate it to a vertical position, then wind the
   Seifert surface to a Hopf spiral. */

/* Phase 1: Increase the density of the Seifert surface and rotate it to a
   vertical position. */
ANIM_SO_DEF(single_seifert_to_single_dense_seifert_vert) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -2.0f*M_PI_F,         -M_PI_F,              EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -2.0f*M_PI_F,         -35.0f*M_PI_F/36.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -2.0f*M_PI_F,         -34.0f*M_PI_F/36.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_seifert_to_single_dense_seifert_vert) = {
  ANIM_SO_NUM(single_seifert_to_single_dense_seifert_vert),
  ANIM_SO_NAME(single_seifert_to_single_dense_seifert_vert),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  120                                                         /* num_steps */
};

/* Phase 2: Wind the Hopf spiral and expand it. */
ANIM_SO_DEF(single_dense_seifert_to_single_hopf_spiral_wind) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    0.0f,                 2.0f,                 EASING_CUBIC, /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_seifert_to_single_hopf_spiral_wind) = {
  ANIM_SO_NUM(single_dense_seifert_to_single_hopf_spiral_wind),
  ANIM_SO_NAME(single_dense_seifert_to_single_hopf_spiral_wind),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_seifert_to_single_hopf_spiral_wind_vert) = {
  ANIM_MO_REF(single_seifert_to_single_dense_seifert_vert),
  ANIM_MO_REF(single_dense_seifert_to_single_hopf_spiral_wind)
};

ANIM_PS_DEF(single_seifert_to_single_hopf_spiral_wind_vert) = {
  ANIM_PH_NUM(single_seifert_to_single_hopf_spiral_wind_vert),
  ANIM_PH_NAME(single_seifert_to_single_hopf_spiral_wind_vert)
};

ANIMS_M_DEF(single_seifert_to_single_hopf_spiral) = {
  ANIM_PS_REF(single_seifert_to_single_hopf_spiral_wind_hor),
  ANIM_PS_REF(single_seifert_to_single_hopf_spiral_wind_vert)
};

ANIMS_DEF(single_seifert_to_single_hopf_spiral) = {
  ANIMS_M_NUM(single_seifert_to_single_hopf_spiral),
  ANIMS_M_NAME(single_seifert_to_single_hopf_spiral)
};



/*****************************************************************************
 * The set of all transformations from a triple Seifert surface.
 *****************************************************************************/

/* The set of possible animations for the transition from a triple Seifert
   surface to a single point. */

/* Rotate two Seifert surfaces at latitude ±45° around the z axis, shrink
   them to a single point, and move them to the equator.  Shrink the Seifert
   surface on the equator to a single point without rotating it. */
ANIM_SO_DEF(triple_seifert_to_single_point_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/24.0f,        0.0f,                 EASING_CUBIC, /* offset */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/24.0f,        0.0f,                 EASING_CUBIC, /* offset */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_point_rot_z) = {
  ANIM_SO_NUM(triple_seifert_to_single_point_rot_z),
  ANIM_SO_NAME(triple_seifert_to_single_point_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_single_point_rot_z) = {
  ANIM_MO_REF(triple_seifert_to_single_point_rot_z)
};

ANIM_PS_DEF(triple_seifert_to_single_point_rot_z) = {
  ANIM_PH_NUM(triple_seifert_to_single_point_rot_z),
  ANIM_PH_NAME(triple_seifert_to_single_point_rot_z)
};

/* Rotate the three Seifert surfaces around the z axis in opposite
   directions, move the Seifert surfaces at latitudes ±45° to latitudes
   ±70°, and increase their point density by a factor of two, then move them
   successively to the south pole while continuing to rotate, creating a
   single point at the south pole, then move the point on the south pole to
   its canonical position on the equator along a Hopf spiral. */

/* Phase 1: Rotate the three Seifert surfaces for one revolution, move the
   Seifert surfaces at latitudes ±45° to latitudes ±70°, and increase the
   point density by a factor of two. */
ANIM_SO_DEF(triple_seifert_to_single_point_rot_z_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_ACCEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_ACCEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_ACCEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_ACCEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_ACCEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_ACCEL  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_point_rot_z_densify) = {
  ANIM_SO_NUM(triple_seifert_to_single_point_rot_z_densify),
  ANIM_SO_NAME(triple_seifert_to_single_point_rot_z_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 2: Rotate the three Seifert surfaces for one revolution. */
ANIM_SO_DEF(triple_seifert_to_single_point_rot_z_linear) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     8.0f*M_PI_F/9.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_LIN    /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_point_rot_z_linear) = {
  ANIM_SO_NUM(triple_seifert_to_single_point_rot_z_linear),
  ANIM_SO_NAME(triple_seifert_to_single_point_rot_z_linear),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 3: Move the Seifert surface at latitude -70° to the south pole. */
ANIM_SO_DEF(triple_seifert_to_single_point_rot_z_move_south_1) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     M_PI_F,               EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_DECEL  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_point_rot_z_move_south_1) = {
  ANIM_SO_NUM(triple_seifert_to_single_point_rot_z_move_south_1),
  ANIM_SO_NAME(triple_seifert_to_single_point_rot_z_move_south_1),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 4: Move the Seifert surface at latitude 0° to the south pole. */
ANIM_SO_DEF(triple_seifert_to_single_point_rot_z_move_south_2) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_LIN    /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_DECEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_point_rot_z_move_south_2) = {
  ANIM_SO_NUM(triple_seifert_to_single_point_rot_z_move_south_2),
  ANIM_SO_NAME(triple_seifert_to_single_point_rot_z_move_south_2),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 5: Move the Seifert surface at latitude 70° to the south pole. */
ANIM_SO_DEF(triple_seifert_to_single_point_rot_z_move_south_3) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F,               EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_DECEL  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_point_rot_z_move_south_3) = {
  ANIM_SO_NUM(triple_seifert_to_single_point_rot_z_move_south_3),
  ANIM_SO_NAME(triple_seifert_to_single_point_rot_z_move_south_3),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 6: Move the single point at the south pole to the equator along a
   Hopf Spiral. */
ANIM_SO_DEF(triple_seifert_to_single_point_rot_z_move_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* offset */
    0.0f,                 0.0f,                 EASING_NONE,  /* sector */
    0,                    1,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
};

ANIM_MO_DEF(triple_seifert_to_single_point_rot_z_move_spiral) = {
  ANIM_SO_NUM(triple_seifert_to_single_point_rot_z_move_spiral),
  ANIM_SO_NAME(triple_seifert_to_single_point_rot_z_move_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  120                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_single_point_move_south) = {
  ANIM_MO_REF(triple_seifert_to_single_point_rot_z_densify),
  ANIM_MO_REF(triple_seifert_to_single_point_rot_z_linear),
  ANIM_MO_REF(triple_seifert_to_single_point_rot_z_move_south_1),
  ANIM_MO_REF(triple_seifert_to_single_point_rot_z_move_south_2),
  ANIM_MO_REF(triple_seifert_to_single_point_rot_z_move_south_3),
  ANIM_MO_REF(triple_seifert_to_single_point_rot_z_move_spiral)
};

ANIM_PS_DEF(triple_seifert_to_single_point_move_south) = {
  ANIM_PH_NUM(triple_seifert_to_single_point_move_south),
  ANIM_PH_NAME(triple_seifert_to_single_point_move_south)
};

ANIMS_M_DEF(triple_seifert_to_single_point) = {
  ANIM_PS_REF(triple_seifert_to_single_point_rot_z),
  ANIM_PS_REF(triple_seifert_to_single_point_move_south)
};

ANIMS_DEF(triple_seifert_to_single_point) = {
  ANIMS_M_NUM(triple_seifert_to_single_point),
  ANIMS_M_NAME(triple_seifert_to_single_point)
};



/* The set of possible animations for the transition from a triple Seifert
   surface to a single torus. */

/* Rotate three Seifert surfaces around the z axis, reduce their sectors
   from 180° to 120°, move the two Seifert surfaces at latitude ±45° to the
   equator, and merge all three Seifert surfaces into a dense torus at the
   equator, then reduce the point density by a factor of three. */

/* Phase 1: Rotate three Seifert surfaces around the z axis, reduce their
   sectors from 180° to 120°, move the two Seifert surfaces at latitude
   ±45° to the equator, and merge all three Seifert surfaces into a dense
   torus at the equator. */
ANIM_SO_DEF(triple_seifert_to_single_dense_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              4.0f*M_PI_F/3.0f,     EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_dense_torus_rot_z) = {
  ANIM_SO_NUM(triple_seifert_to_single_dense_torus_rot_z),
  ANIM_SO_NAME(triple_seifert_to_single_dense_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 2: Reduce the point density by a factor of three.  Already defined
   above. */

ANIM_PH_DEF(triple_seifert_to_single_torus_rot_z) = {
  ANIM_MO_REF(triple_seifert_to_single_dense_torus_rot_z),
  ANIM_MO_REF(single_dense_torus_to_single_torus_loosen_three)
};

ANIM_PS_DEF(triple_seifert_to_single_torus_rot_z) = {
  ANIM_PH_NUM(triple_seifert_to_single_torus_rot_z),
  ANIM_PH_NAME(triple_seifert_to_single_torus_rot_z)
};

/* Rotate three Seifert surfaces around a random axis, expand their sectors
   from 180° to 360°, and move the Seifert surfaces at latitude ±45° to the
   equator. */
ANIM_SO_DEF(triple_seifert_to_single_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_torus_rot_rnd) = {
  ANIM_SO_NUM(triple_seifert_to_single_torus_rot_rnd),
  ANIM_SO_NAME(triple_seifert_to_single_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_single_torus_rot_rnd) = {
  ANIM_MO_REF(triple_seifert_to_single_torus_rot_rnd)
};

ANIM_PS_DEF(triple_seifert_to_single_torus_rot_rnd) = {
  ANIM_PH_NUM(triple_seifert_to_single_torus_rot_rnd),
  ANIM_PH_NAME(triple_seifert_to_single_torus_rot_rnd)
};

ANIMS_M_DEF(triple_seifert_to_single_torus) = {
  ANIM_PS_REF(triple_seifert_to_single_torus_rot_z),
  ANIM_PS_REF(triple_seifert_to_single_torus_rot_rnd)
};

ANIMS_DEF(triple_seifert_to_single_torus) = {
  ANIMS_M_NUM(triple_seifert_to_single_torus),
  ANIMS_M_NAME(triple_seifert_to_single_torus)
};



/* The set of possible animations for the transition from a triple Seifert
   surface to a double torus. */

/* Rotate the three Seifert surfaces around the z axis, split the Seifert
   surface at the equator into two Seifert surfaces, increase their sector
   from 180° to 360°, and move them to latitude ±45°. */
ANIM_SO_DEF(triple_seifert_to_double_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, M_PI_F,         EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, M_PI_F/2.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, M_PI_F,        EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_double_torus_rot_z) = {
  ANIM_SO_NUM(triple_seifert_to_double_torus_rot_z),
  ANIM_SO_NAME(triple_seifert_to_double_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_double_torus_rot_z) = {
  ANIM_MO_REF(triple_seifert_to_double_torus_rot_z)
};

ANIM_PS_DEF(triple_seifert_to_double_torus_rot_z) = {
  ANIM_PH_NUM(triple_seifert_to_double_torus_rot_z),
  ANIM_PH_NAME(triple_seifert_to_double_torus_rot_z)
};

/* Split the Seifert surface at the equator into two Seifert surfaces,
   increase their sector from 180° to 360°, move them to latitude ±45°, and
   rotate all Seifert surfaces around a random axis with a certain
   probability. */
ANIM_SO_DEF(triple_seifert_to_double_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_double_torus_rot_rnd) = {
  ANIM_SO_NUM(triple_seifert_to_double_torus_rot_rnd),
  ANIM_SO_NAME(triple_seifert_to_double_torus_rot_rnd),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_double_torus_rot_rnd) = {
  ANIM_MO_REF(triple_seifert_to_double_torus_rot_rnd)
};

ANIM_PS_DEF(triple_seifert_to_double_torus_rot_rnd) = {
  ANIM_PH_NUM(triple_seifert_to_double_torus_rot_rnd),
  ANIM_PH_NAME(triple_seifert_to_double_torus_rot_rnd)
};

ANIMS_M_DEF(triple_seifert_to_double_torus) = {
  ANIM_PS_REF(triple_seifert_to_double_torus_rot_z),
  ANIM_PS_REF(triple_seifert_to_double_torus_rot_rnd)
};

ANIMS_DEF(triple_seifert_to_double_torus) = {
  ANIMS_M_NUM(triple_seifert_to_double_torus),
  ANIMS_M_NAME(triple_seifert_to_double_torus)
};



/* The set of possible animations for the transition from a triple Seifert
   surface to a triple torus. */

/* Rotate the three Seifert surfaces around the z axis and increase their
   sector from 180° to 360°. */
ANIM_SO_DEF(triple_seifert_to_triple_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 3.0f*M_PI_F/2.0f,
                                                EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               0.0f,                 EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 3.0f*M_PI_F/2.0f,
                                                EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_triple_torus_rot_z) = {
  ANIM_SO_NUM(triple_seifert_to_triple_torus_rot_z),
  ANIM_SO_NAME(triple_seifert_to_triple_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_triple_torus_rot_z) = {
  ANIM_MO_REF(triple_seifert_to_triple_torus_rot_z)
};

ANIM_PS_DEF(triple_seifert_to_triple_torus_rot_z) = {
  ANIM_PH_NUM(triple_seifert_to_triple_torus_rot_z),
  ANIM_PH_NAME(triple_seifert_to_triple_torus_rot_z)
};

/* Increase the sector of the Seifert surfaces from 180° to 360° and rotate
   them around a random axis with a certain probability. */
ANIM_SO_DEF(triple_seifert_to_triple_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE , /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_triple_torus_rot_rnd) = {
  ANIM_SO_NUM(triple_seifert_to_triple_torus_rot_rnd),
  ANIM_SO_NAME(triple_seifert_to_triple_torus_rot_rnd),
  0.75f,                                        EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_triple_torus_rot_rnd) = {
  ANIM_MO_REF(triple_seifert_to_triple_torus_rot_rnd)
};

ANIM_PS_DEF(triple_seifert_to_triple_torus_rot_rnd) = {
  ANIM_PH_NUM(triple_seifert_to_triple_torus_rot_rnd),
  ANIM_PH_NAME(triple_seifert_to_triple_torus_rot_rnd)
};

ANIMS_M_DEF(triple_seifert_to_triple_torus) = {
  ANIM_PS_REF(triple_seifert_to_triple_torus_rot_z),
  ANIM_PS_REF(triple_seifert_to_triple_torus_rot_rnd)
};

ANIMS_DEF(triple_seifert_to_triple_torus) = {
  ANIMS_M_NUM(triple_seifert_to_triple_torus),
  ANIMS_M_NAME(triple_seifert_to_triple_torus)
};



/* The set of possible animations for the transition from a triple Seifert
   surface to a single Seifert surface. */

/* Shrink the three Seifert surfaces from 180° sectors to 60° sectors, join
   them at the equator, and reduce the point density by a factor of three. */

/* Phase 1: Shrink the three Seifert surfaces from 180° sectors to 60°
   sectors and join them at the equator. */
ANIM_SO_DEF(triple_seifert_to_single_dense_seifert_three_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -25.0f*M_PI_F/72.0f,  EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F/3.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -49.0f*M_PI_F/72.0f,  EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F/3.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -73.0f*M_PI_F/72.0f,  EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F/3.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_dense_seifert_three_rot_z) = {
  ANIM_SO_NUM(triple_seifert_to_single_dense_seifert_three_rot_z),
  ANIM_SO_NAME(triple_seifert_to_single_dense_seifert_three_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 2: Decrease the point density by a factor of three.  Already
   defined above. */

ANIM_PH_DEF(triple_seifert_to_single_seifert_rot_z) = {
  ANIM_MO_REF(triple_seifert_to_single_dense_seifert_three_rot_z),
  ANIM_MO_REF(single_dense_seifert_three_to_single_seifert_rot_z)
};

ANIM_PS_DEF(triple_seifert_to_single_seifert_rot_z) = {
  ANIM_PH_NUM(triple_seifert_to_single_seifert_rot_z),
  ANIM_PH_NAME(triple_seifert_to_single_seifert_rot_z)
};

/* Reduce the point density of the three Seifert surfaces by a factor of
   three, interleave them at the equator, and rotate around a random axis. */
ANIM_SO_DEF(triple_seifert_to_single_seifert_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   25.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    25.0f*M_PI_F/24.0f,   26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    26.0f*M_PI_F/24.0f,   26.0f*M_PI_F/24.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    8,                                  /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_seifert_rot_rnd) = {
  ANIM_SO_NUM(triple_seifert_to_single_seifert_rot_rnd),
  ANIM_SO_NAME(triple_seifert_to_single_seifert_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_single_seifert_rot_rnd) = {
  ANIM_MO_REF(triple_seifert_to_single_seifert_rot_rnd)
};

ANIM_PS_DEF(triple_seifert_to_single_seifert_rot_rnd) = {
  ANIM_PH_NUM(triple_seifert_to_single_seifert_rot_rnd),
  ANIM_PH_NAME(triple_seifert_to_single_seifert_rot_rnd)
};

ANIMS_M_DEF(triple_seifert_to_single_seifert) = {
  ANIM_PS_REF(triple_seifert_to_single_seifert_rot_z),
  ANIM_PS_REF(triple_seifert_to_single_seifert_rot_rnd)
};

ANIMS_DEF(triple_seifert_to_single_seifert) = {
  ANIMS_M_NUM(triple_seifert_to_single_seifert),
  ANIMS_M_NAME(triple_seifert_to_single_seifert)
};



/* The set of possible animations for a triple Seifert surface. */

/* Rotate three Seifert surfaces at latitudes ±45° and 0° around the x axis
   of the total space while rotating them around the z axis in different
   directions. */
ANIM_SO_DEF(triple_seifert_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_rot_x) = {
  ANIM_SO_NUM(triple_seifert_rot_x),
  ANIM_SO_NAME(triple_seifert_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_rot_x) = {
  ANIM_MO_REF(triple_seifert_rot_x)
};

ANIM_PS_DEF(triple_seifert_rot_x) = {
  ANIM_PH_NUM(triple_seifert_rot_x),
  ANIM_PH_NAME(triple_seifert_rot_x)
};

/* Rotate the three Seifert surfaces to a a vertical orientation, yielding
   half of a parabolic ring cyclide and two interlocking Seifert surfaces,
   and increase their density by a factor of two.  Then, rotate them around
   the x axis of the total space.  Next, move the two Seifert surfaces not
   on the equator between latitudes 1° and ±89°.  Finally, rotate the
   Seifert surfaces back to 0° and ±45° and decrease their density by a
   factor of two. */

/* Phase 1: Rotate the Seifert surfaces by 90° to a vertical position,
   increase their point density by a factor of two, and rotate them around
   the x axis of the total space by 90°. */
ANIM_SO_DEF(triple_seifert_to_vertical_triple_seifert_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_vertical_triple_seifert_densify) = {
  ANIM_SO_NUM(triple_seifert_to_vertical_triple_seifert_densify),
  ANIM_SO_NAME(triple_seifert_to_vertical_triple_seifert_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { -1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,     EASING_CUBIC, /* rot_space */
  120                                                         /* num_steps */
};

/* Phase 2: Rotate the three Seifert surfaces by 360° around the x axis of
   the total space. */
ANIM_SO_DEF(vertical_triple_seifert_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(vertical_triple_seifert_rot_x) = {
  ANIM_SO_NUM(vertical_triple_seifert_rot_x),
  ANIM_SO_NAME(vertical_triple_seifert_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 5.0f*M_PI_F/2.0f,
                                                EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 3: Move the two Seifert surfaces at latitides ±45° between
   latitudes 1° and ±89°. */
ANIM_SO_DEF(vertical_triple_seifert_move_z) = {
  {
    GEN_TORUS,                                                /* generator */
    179.0f*M_PI_F/180.0f, 91.0f*M_PI_F/180.0f,  EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    1.0f*M_PI_F/180.0f,   89.0f*M_PI_F/180.0f,  EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(vertical_triple_seifert_move_z) = {
  ANIM_SO_NUM(vertical_triple_seifert_move_z),
  ANIM_SO_NAME(vertical_triple_seifert_move_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, M_PI_F/2.0f,
                                                EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 4: Rotate the Seifert surfaces back to 0° and ±45° and decrease
   their density by a factor of two. */
ANIM_SO_DEF(vertical_triple_seifert_to_triple_seifert_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,   EASING_CUBIC  /* rot_axis */
  },
};

ANIM_MO_DEF(vertical_triple_seifert_to_triple_seifert_loosen) = {
  ANIM_SO_NUM(vertical_triple_seifert_to_triple_seifert_loosen),
  ANIM_SO_NAME(vertical_triple_seifert_to_triple_seifert_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { -1.0f, 0.0f, 0.0f }, M_PI_F/2.0f, 0.0f,     EASING_CUBIC, /* rot_space */
  120                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_three_sphere) = {
  ANIM_MO_REF(triple_seifert_to_vertical_triple_seifert_densify),
  ANIM_MO_REF(vertical_triple_seifert_rot_x),
  ANIM_MO_REF(vertical_triple_seifert_move_z),
  ANIM_MO_REF(vertical_triple_seifert_to_triple_seifert_loosen)
};

ANIM_PS_DEF(triple_seifert_three_sphere) = {
  ANIM_PH_NUM(triple_seifert_three_sphere),
  ANIM_PH_NAME(triple_seifert_three_sphere)
};

/* Move the two Seifert surfaces at latitudes ±45° to latitude ±70° and
   increase the point density of all three Seifert surfaces by a factor of
   two, rotate the Seifert surfaces around the z axis and, with a certain
   probability, around a random axis, move the two Seifert surfaces at
   latitude ±70° back to ±45°, and decrease the point density of all three
   Seifert surfaces by a factor of two. */

/* Phase 1: Move two Seifert surfaces at latitude ±45° to latitude ±70° and
   increase the point density of all three Seifert surfaces by a factor of
   two. */
ANIM_SO_DEF(triple_seifert_rot_rnd_move_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/9.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     8.0f*M_PI_F/9.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_rot_rnd_move_densify) = {
  ANIM_SO_NUM(triple_seifert_rot_rnd_move_densify),
  ANIM_SO_NAME(triple_seifert_rot_rnd_move_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Rotate the two tori by 360° around the z axis and, with a
   certain probability, around a random axis. */
ANIM_SO_DEF(triple_seifert_rot_rnd_rot) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/9.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     8.0f*M_PI_F/9.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    48,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_rot_rnd_rot) = {
  ANIM_SO_NUM(triple_seifert_rot_rnd_rot),
  ANIM_SO_NAME(triple_seifert_rot_rnd_rot),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 3: Move the two Seifert surfaces at latitude ±70° to latitude ±45°
   and decrease the point density of all three Seifert surfaces by a factor
   of two. */
ANIM_SO_DEF(triple_seifert_rot_rnd_move_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/9.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    8.0f*M_PI_F/9.0f,     3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    49.0f*M_PI_F/48.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_rot_rnd_move_loosen) = {
  ANIM_SO_NUM(triple_seifert_rot_rnd_move_loosen),
  ANIM_SO_NAME(triple_seifert_rot_rnd_move_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

ANIM_PH_DEF(triple_seifert_rot_rnd) = {
  ANIM_MO_REF(triple_seifert_rot_rnd_move_densify),
  ANIM_MO_REF(triple_seifert_rot_rnd_rot),
  ANIM_MO_REF(triple_seifert_rot_rnd_move_loosen)
};

ANIM_PS_DEF(triple_seifert_rot_rnd) = {
  ANIM_PH_NUM(triple_seifert_rot_rnd),
  ANIM_PH_NAME(triple_seifert_rot_rnd)
};

/* Move the two Seifert surfaces at latitude ±45° to latitude ±5°, to
   latitude ±85° and back to latitude ±45° while rotating all three Seifert
   surfaces around the z axis and, with a certain probability, around a
   random axis. */
ANIM_SO_DEF(triple_seifert_move) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/18.0f,         8.0f*M_PI_F/18.0f,    EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    17.0f*M_PI_F/18.0f,   10.0f*M_PI_F/18.0f,   EASING_SIN,   /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, -1.0f }, 0.0f, 2.0f*M_PI_F,   EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_move) = {
  ANIM_SO_NUM(triple_seifert_move),
  ANIM_SO_NAME(triple_seifert_move),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_move) = {
  ANIM_MO_REF(triple_seifert_move)
};

ANIM_PS_DEF(triple_seifert_move) = {
  ANIM_PH_NUM(triple_seifert_move),
  ANIM_PH_NAME(triple_seifert_move)
};

ANIMS_M_DEF(triple_seifert) = {
  ANIM_PS_REF(triple_seifert_rot_x),
  ANIM_PS_REF(triple_seifert_three_sphere),
  ANIM_PS_REF(triple_seifert_rot_rnd),
  ANIM_PS_REF(triple_seifert_move)
};

ANIMS_DEF(triple_seifert) = {
  ANIMS_M_NUM(triple_seifert),
  ANIMS_M_NAME(triple_seifert)
};



/* The set of possible animations for the transition from a triple Seifert
   surface to a single Hopf torus. */

/* Expand the sectors of the three Seifert surfaces from 180° to 360°,
   transform them to a Hopf torus on the equator while increasing the point
   density of the two Seifert surfaces at latitude ±45° by a factor of two. */
ANIM_SO_DEF(triple_seifert_to_single_hopf_torus_densify) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               58.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               59.0f*M_PI_F/60.0f,   EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/2.0f,          EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F/60.0f,         EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* p */
    0.0f,                 M_PI_F/8.0f,          EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               2.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    M_PI_F,               2.0f*M_PI_F,          EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_hopf_torus_densify) = {
  ANIM_SO_NUM(triple_seifert_to_single_hopf_torus_densify),
  ANIM_SO_NAME(triple_seifert_to_single_hopf_torus_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_single_hopf_torus_densify) = {
  ANIM_MO_REF(triple_seifert_to_single_hopf_torus_densify)
};

ANIM_PS_DEF(triple_seifert_to_single_hopf_torus_densify) = {
  ANIM_PH_NUM(triple_seifert_to_single_hopf_torus_densify),
  ANIM_PH_NAME(triple_seifert_to_single_hopf_torus_densify)
};

ANIMS_M_DEF(triple_seifert_to_single_hopf_torus) = {
  ANIM_PS_REF(triple_seifert_to_single_hopf_torus_densify)
};

ANIMS_DEF(triple_seifert_to_single_hopf_torus) = {
  ANIMS_M_NUM(triple_seifert_to_single_hopf_torus),
  ANIMS_M_NAME(triple_seifert_to_single_hopf_torus)
};



/* The set of possible animations for the transition from a triple Seifert
   surface to a single Hopf spiral. */

/* Deform the three Seifert surfaces to three Hopf spirals and merge them. */
ANIM_SO_DEF(triple_seifert_to_single_hopf_spiral_merge) = {
  {
    GEN_SPIRAL,                                               /* generator */
    -M_PI_F/4.0f,         0.0f,                 EASING_CUBIC, /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         -M_PI_F,              EASING_CUBIC, /* offset */
    M_PI_F/2.0f,          2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    M_PI_F/48.0f,         -M_PI_F/3.0f,         EASING_CUBIC, /* offset */
    M_PI_F/2.0f,          2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    M_PI_F/4.0f,          0.0f,                 EASING_CUBIC, /* p */
    0.0f,                 1.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -47.0f*M_PI_F/48.0f,  M_PI_F/3.0f,          EASING_CUBIC, /* offset */
    M_PI_F/2.0f,          2.0f*M_PI_F/3.0f,     EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(triple_seifert_to_single_hopf_spiral_merge) = {
  ANIM_SO_NUM(triple_seifert_to_single_hopf_spiral_merge),
  ANIM_SO_NAME(triple_seifert_to_single_hopf_spiral_merge),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(triple_seifert_to_single_hopf_spiral_merge) = {
  ANIM_MO_REF(triple_seifert_to_single_hopf_spiral_merge)
};

ANIM_PS_DEF(triple_seifert_to_single_hopf_spiral_merge) = {
  ANIM_PH_NUM(triple_seifert_to_single_hopf_spiral_merge),
  ANIM_PH_NAME(triple_seifert_to_single_hopf_spiral_merge)
};

ANIMS_M_DEF(triple_seifert_to_single_hopf_spiral) = {
  ANIM_PS_REF(triple_seifert_to_single_hopf_spiral_merge)
};

ANIMS_DEF(triple_seifert_to_single_hopf_spiral) = {
  ANIMS_M_NUM(triple_seifert_to_single_hopf_spiral),
  ANIMS_M_NAME(triple_seifert_to_single_hopf_spiral)
};



/*****************************************************************************
 * The set of all transformations from a single Hopf torus.
 *****************************************************************************/

/* The set of possible animations for the transition from a single Hopf
   torus to a single point. */

/* Shrink a Hopf torus on the equator to a point on the equator and rotate
   around a random axis. */
ANIM_SO_DEF(single_hopf_torus_to_single_point_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_to_single_point_rot_rnd) = {
  ANIM_SO_NUM(single_hopf_torus_to_single_point_rot_rnd),
  ANIM_SO_NAME(single_hopf_torus_to_single_point_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_to_single_point_rot_rnd) = {
  ANIM_MO_REF(single_hopf_torus_to_single_point_rot_rnd)
};

ANIM_PS_DEF(single_hopf_torus_to_single_point_rot_rnd) = {
  ANIM_PH_NUM(single_hopf_torus_to_single_point_rot_rnd),
  ANIM_PH_NAME(single_hopf_torus_to_single_point_rot_rnd)
};

/* Reduce the Hopf torus to a Hopf torus of one fifth the point density and
   then deform it to a point on the equator. */

/* Phase 1: Decrease the density of the Hopf torus. */
ANIM_SO_DEF(single_dense_hopf_torus_to_single_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -2.0f*M_PI_F/60.0f,   0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/60.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/60.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    2.0f*M_PI_F/60.0f,    0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_hopf_torus_to_single_hopf_torus) = {
  ANIM_SO_NUM(single_dense_hopf_torus_to_single_hopf_torus),
  ANIM_SO_NAME(single_dense_hopf_torus_to_single_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Shrink the loose Hopf torus to a point on the equator. */
ANIM_SO_DEF(single_loose_hopf_torus_to_single_point) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_loose_hopf_torus_to_single_point) = {
  ANIM_SO_NUM(single_loose_hopf_torus_to_single_point),
  ANIM_SO_NAME(single_loose_hopf_torus_to_single_point),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_to_single_point_loosen) = {
  ANIM_MO_REF(single_dense_hopf_torus_to_single_hopf_torus),
  ANIM_MO_REF(single_loose_hopf_torus_to_single_point)
};

ANIM_PS_DEF(single_hopf_torus_to_single_point_loosen) = {
  ANIM_PH_NUM(single_hopf_torus_to_single_point_loosen),
  ANIM_PH_NAME(single_hopf_torus_to_single_point_loosen)
};

ANIMS_M_DEF(single_hopf_torus_to_single_point) = {
  ANIM_PS_REF(single_hopf_torus_to_single_point_rot_rnd),
  ANIM_PS_REF(single_hopf_torus_to_single_point_loosen)
};

ANIMS_DEF(single_hopf_torus_to_single_point) = {
  ANIMS_M_NUM(single_hopf_torus_to_single_point),
  ANIMS_M_NAME(single_hopf_torus_to_single_point)
};



/* The set of possible animations for the transition from a single Hopf
   torus to a single torus. */

/* Reduce the Hopf torus to a Hopf torus of one fifth the point density and
   then deform it to a torus on the equator. */

/* Phase 1: Decrease the density of the Hopf torus.  Already defined above. */

/* Phase 2: Deform the Hopf torus to a torus on the equator. */
ANIM_SO_DEF(single_hopf_torus_to_single_loose_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_to_single_loose_torus) = {
  ANIM_SO_NUM(single_hopf_torus_to_single_loose_torus),
  ANIM_SO_NAME(single_hopf_torus_to_single_loose_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_to_single_torus_loosen) = {
  ANIM_MO_REF(single_dense_hopf_torus_to_single_hopf_torus),
  ANIM_MO_REF(single_hopf_torus_to_single_loose_torus)
};

ANIM_PS_DEF(single_hopf_torus_to_single_torus_loosen) = {
  ANIM_PH_NUM(single_hopf_torus_to_single_torus_loosen),
  ANIM_PH_NAME(single_hopf_torus_to_single_torus_loosen)
};

ANIMS_M_DEF(single_hopf_torus_to_single_torus) = {
  ANIM_PS_REF(single_hopf_torus_to_single_torus_loosen)
};

ANIMS_DEF(single_hopf_torus_to_single_torus) = {
  ANIMS_M_NUM(single_hopf_torus_to_single_torus),
  ANIMS_M_NAME(single_hopf_torus_to_single_torus)
};



/* The set of possible animations for the transition from a Hopf torus
   to a double torus. */

/* Decrease the density of the Hopf torus by a factor of five, disentangle
   and deform the Hopf torus to two loose tori at latitude ±45°, and
   increase the density of the two tori by a factor of two. */

/* Phase 1: Decrease the density of the Hopf torus by a factor of five. */
ANIM_SO_DEF(hopf_torus_to_loose_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -2.0f*M_PI_F/60.0f,   0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/60.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/60.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    2.0f*M_PI_F/60.0f,    0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(hopf_torus_to_loose_hopf_torus) = {
  ANIM_SO_NUM(hopf_torus_to_loose_hopf_torus),
  ANIM_SO_NAME(hopf_torus_to_loose_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Deform the interleaved Hopf torus to two loose tori. */
ANIM_SO_DEF(loose_hopf_torus_to_loose_double_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/12.0f,         M_PI_F/12.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(loose_hopf_torus_to_loose_double_torus) = {
  ANIM_SO_NUM(loose_hopf_torus_to_loose_double_torus),
  ANIM_SO_NAME(loose_hopf_torus_to_loose_double_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

/* Phase 3: Increase the density of the tori at ±45°. */
ANIM_SO_DEF(loose_double_torus_to_double_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/4.0f,          M_PI_F/4.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F/12.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/12.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    3.0f*M_PI_F/4.0f,     3.0f*M_PI_F/4.0f,     EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/12.0f,         M_PI_F/12.0f,         EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    12,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(loose_double_torus_to_double_torus) = {
  ANIM_SO_NUM(loose_double_torus_to_double_torus),
  ANIM_SO_NAME(loose_double_torus_to_double_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_to_double_torus_loosen) = {
  ANIM_MO_REF(hopf_torus_to_loose_hopf_torus),
  ANIM_MO_REF(loose_hopf_torus_to_loose_double_torus),
  ANIM_MO_REF(loose_double_torus_to_double_torus)
};

ANIM_PS_DEF(single_hopf_torus_to_double_torus_loosen) = {
  ANIM_PH_NUM(single_hopf_torus_to_double_torus_loosen),
  ANIM_PH_NAME(single_hopf_torus_to_double_torus_loosen)
};

ANIMS_M_DEF(single_hopf_torus_to_double_torus) = {
  ANIM_PS_REF(single_hopf_torus_to_double_torus_loosen)
};

ANIMS_DEF(single_hopf_torus_to_double_torus) = {
  ANIMS_M_NUM(single_hopf_torus_to_double_torus),
  ANIMS_M_NAME(single_hopf_torus_to_double_torus)
};



/* The set of possible animations for the transition from a single Hopf
   torus to a triple torus. */

/* Transform Hopf torus on the equator into three tori while decreasing the
   point density of the two tori at latitude ±45° by a factor of two. */
ANIM_SO_DEF(single_hopf_torus_to_triple_torus_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -2.0f*M_PI_F/60.0f,   0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -1.0f*M_PI_F/60.0f,   0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    1.0f*M_PI_F/60.0f,    0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    2.0f*M_PI_F/60.0f,    0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_to_triple_torus_loosen) = {
  ANIM_SO_NUM(single_hopf_torus_to_triple_torus_loosen),
  ANIM_SO_NAME(single_hopf_torus_to_triple_torus_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  120                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_to_triple_torus_loosen) = {
  ANIM_MO_REF(single_hopf_torus_to_triple_torus_loosen)
};

ANIM_PS_DEF(single_hopf_torus_to_triple_torus_loosen) = {
  ANIM_PH_NUM(single_hopf_torus_to_triple_torus_loosen),
  ANIM_PH_NAME(single_hopf_torus_to_triple_torus_loosen)
};

ANIMS_M_DEF(single_hopf_torus_to_triple_torus) = {
  ANIM_PS_REF(single_hopf_torus_to_triple_torus_loosen)
};

ANIMS_DEF(single_hopf_torus_to_triple_torus) = {
  ANIMS_M_NUM(single_hopf_torus_to_triple_torus),
  ANIMS_M_NAME(single_hopf_torus_to_triple_torus)
};



/* The set of possible animations for the transition from a single Hopf
   torus to a single Seifert surface. */

/* Shrink the Hopf torus, decrease its density to one fifth, and deform it
   to a Seifert surface on the equator. */
ANIM_SO_DEF(single_hopf_torus_to_single_seifert_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    61.0f*M_PI_F/60.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    62.0f*M_PI_F/60.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    63.0f*M_PI_F/60.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    64.0f*M_PI_F/60.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_to_single_seifert_loosen) = {
  ANIM_SO_NUM(single_hopf_torus_to_single_seifert_loosen),
  ANIM_SO_NAME(single_hopf_torus_to_single_seifert_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_to_single_seifert_loosen) = {
  ANIM_MO_REF(single_hopf_torus_to_single_seifert_loosen)
};

ANIM_PS_DEF(single_hopf_torus_to_single_seifert_loosen) = {
  ANIM_PH_NUM(single_hopf_torus_to_single_seifert_loosen),
  ANIM_PH_NAME(single_hopf_torus_to_single_seifert_loosen)
};

ANIMS_M_DEF(single_hopf_torus_to_single_seifert) = {
  ANIM_PS_REF(single_hopf_torus_to_single_seifert_loosen)
};

ANIMS_DEF(single_hopf_torus_to_single_seifert) = {
  ANIMS_M_NUM(single_hopf_torus_to_single_seifert),
  ANIMS_M_NAME(single_hopf_torus_to_single_seifert)
};



/* The set of possible animations for the transition from a single Hopf
   torus to a triple Seifert surface. */

/* Transform the Hopf torus on the equator into three Seifert surfaces while
   decreasing the point density of the two Seifert surfaces at latitude ±45°
   by a factor of two. */
ANIM_SO_DEF(single_hopf_torus_to_triple_seifert_loosen) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    58.0f*M_PI_F/60.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/4.0f,          EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    59.0f*M_PI_F/60.0f,   M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/2.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/60.0f,         M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          3.0f*M_PI_F/4.0f,     EASING_CUBIC, /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    2.0f*M_PI_F/60.0f,    M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_to_triple_seifert_loosen) = {
  ANIM_SO_NUM(single_hopf_torus_to_triple_seifert_loosen),
  ANIM_SO_NAME(single_hopf_torus_to_triple_seifert_loosen),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_to_triple_seifert_loosen) = {
  ANIM_MO_REF(single_hopf_torus_to_triple_seifert_loosen)
};

ANIM_PS_DEF(single_hopf_torus_to_triple_seifert_loosen) = {
  ANIM_PH_NUM(single_hopf_torus_to_triple_seifert_loosen),
  ANIM_PH_NAME(single_hopf_torus_to_triple_seifert_loosen)
};

ANIMS_M_DEF(single_hopf_torus_to_triple_seifert) = {
  ANIM_PS_REF(single_hopf_torus_to_triple_seifert_loosen)
};

ANIMS_DEF(single_hopf_torus_to_triple_seifert) = {
  ANIMS_M_NUM(single_hopf_torus_to_triple_seifert),
  ANIMS_M_NAME(single_hopf_torus_to_triple_seifert)
};



/* The set of possible animations for a single Hopf torus. */

/* Rotate a Hopf torus around the x axis of the total space. */
ANIM_SO_DEF(single_hopf_torus_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_rot_x) = {
  ANIM_SO_NUM(single_hopf_torus_rot_x),
  ANIM_SO_NAME(single_hopf_torus_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_rot_x) = {
  ANIM_MO_REF(single_hopf_torus_rot_x)
};

ANIM_PS_DEF(single_hopf_torus_rot_x) = {
  ANIM_PH_NUM(single_hopf_torus_rot_x),
  ANIM_PH_NAME(single_hopf_torus_rot_x)
};

/* Rotate a Hopf torus around the z axis. */
ANIM_SO_DEF(single_hopf_torus_rot_z) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_rot_z) = {
  ANIM_SO_NUM(single_hopf_torus_rot_z),
  ANIM_SO_NAME(single_hopf_torus_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_rot_z) = {
  ANIM_MO_REF(single_hopf_torus_rot_z)
};

ANIM_PS_DEF(single_hopf_torus_rot_z) = {
  ANIM_PH_NUM(single_hopf_torus_rot_z),
  ANIM_PH_NAME(single_hopf_torus_rot_z)
};

/* Rotate a Hopf torus around a random axis. */
ANIM_SO_DEF(single_hopf_torus_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_rot_rnd) = {
  ANIM_SO_NUM(single_hopf_torus_rot_rnd),
  ANIM_SO_NAME(single_hopf_torus_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_rot_rnd) = {
  ANIM_MO_REF(single_hopf_torus_rot_rnd)
};

ANIM_PS_DEF(single_hopf_torus_rot_rnd) = {
  ANIM_PH_NUM(single_hopf_torus_rot_rnd),
  ANIM_PH_NAME(single_hopf_torus_rot_rnd)
};

/* Wave a Hopf torus around a random axis, i.e., animate the parameter q,
   and rotate it with a certain probability around a random axis. */
ANIM_SO_DEF(single_hopf_torus_wave) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    -M_PI_F/8.0f,         M_PI_F/8.0f,          EASING_COS,   /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_wave) = {
  ANIM_SO_NUM(single_hopf_torus_wave),
  ANIM_SO_NAME(single_hopf_torus_wave),
  0.25f,                                        EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_wave) = {
  ANIM_MO_REF(single_hopf_torus_wave)
};

ANIM_PS_DEF(single_hopf_torus_wave) = {
  ANIM_PH_NUM(single_hopf_torus_wave),
  ANIM_PH_NAME(single_hopf_torus_wave)
};

/* Move a Hopf torus on the equator up and down to latitude ±60° while
   rotating it around the z axis and, with a certain probability, around
   a random axis. */
ANIM_SO_DEF(single_hopf_torus_move) = {
  {
    GEN_TORUS,                                                /* generator */
    1.0f*M_PI_F/6.0f,     5.0f*M_PI_F/6.0f,     EASING_SIN,   /* p */
    M_PI_F/24.0f,         M_PI_F/8.0f,          EASING_COS,   /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_move) = {
  ANIM_SO_NUM(single_hopf_torus_move),
  ANIM_SO_NAME(single_hopf_torus_move),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_torus_move) = {
  ANIM_MO_REF(single_hopf_torus_move)
};

ANIM_PS_DEF(single_hopf_torus_move) = {
  ANIM_PH_NUM(single_hopf_torus_move),
  ANIM_PH_NAME(single_hopf_torus_move)
};

/* Deform the Hopf torus into a Hopf flower, rotate the Hopf flower around
   the x axis of the total space, and deform the Hopf flower back to the
   Hopf torus. */

/* Phase 1: Deform the Hopf torus into a Hopf flower. */
ANIM_SO_DEF(single_hopf_torus_to_single_hopf_flower) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    0.0f,                 -0.5f,                EASING_CUBIC, /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_to_single_hopf_flower) = {
  ANIM_SO_NUM(single_hopf_torus_to_single_hopf_flower),
  ANIM_SO_NAME(single_hopf_torus_to_single_hopf_flower),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

/* Phase 2: Rotate the Hopf flower around the x axis of the total space. */
ANIM_SO_DEF(single_hopf_flower_rot_x) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    -0.5f,                -0.5f,                EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_flower_rot_x) = {
  ANIM_SO_NUM(single_hopf_flower_rot_x),
  ANIM_SO_NAME(single_hopf_flower_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 3: Deform the Hopf flower into a Hopf torus. */
ANIM_SO_DEF(single_hopf_flower_to_single_hopf_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    -0.5f,                0.0f,                 EASING_CUBIC, /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_flower_to_single_hopf_torus) = {
  ANIM_SO_NUM(single_hopf_flower_to_single_hopf_torus),
  ANIM_SO_NAME(single_hopf_flower_to_single_hopf_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

ANIM_PH_DEF(single_hopf_flower_rot_x) = {
  ANIM_MO_REF(single_hopf_torus_to_single_hopf_flower),
  ANIM_MO_REF(single_hopf_flower_rot_x),
  ANIM_MO_REF(single_hopf_flower_to_single_hopf_torus)
};

ANIM_PS_DEF(single_hopf_flower_rot_x) = {
  ANIM_PH_NUM(single_hopf_flower_rot_x),
  ANIM_PH_NAME(single_hopf_flower_rot_x)
};

/* Deform the Hopf torus into a Hopf flower, rotate the Hopf flower around
   the a random axis, and deform the Hopf flower back to the Hopf torus. */

/* Phase 1: Deform the Hopf torus into a Hopf flower.  Already defined
   above. */

/* Phase 2: Rotate a Hopf flower around a random axis. */
ANIM_SO_DEF(single_hopf_flower_rot_rnd) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          M_PI_F/8.0f,          EASING_NONE,  /* q */
    -0.5f,                -0.5f,                EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_flower_rot_rnd) = {
  ANIM_SO_NUM(single_hopf_flower_rot_rnd),
  ANIM_SO_NAME(single_hopf_flower_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

/* Phase 3: Deform the Hopf flower into a Hopf torus.  Already defined
   above. */

ANIM_PH_DEF(single_hopf_flower_rot_rnd) = {
  ANIM_MO_REF(single_hopf_torus_to_single_hopf_flower),
  ANIM_MO_REF(single_hopf_flower_rot_rnd),
  ANIM_MO_REF(single_hopf_flower_to_single_hopf_torus)
};

ANIM_PS_DEF(single_hopf_flower_rot_rnd) = {
  ANIM_PH_NUM(single_hopf_flower_rot_rnd),
  ANIM_PH_NAME(single_hopf_flower_rot_rnd)
};

/* Deform the Hopf torus into a Hopf flower, move the Hopf flower on the
   equator up and down to latitude ±60° while rotating it around the z axis
   and, with a certain probability, around a random axis, and deform the
   Hopf flower back to the Hopf torus. */

/* Phase 1: Deform the Hopf torus into a Hopf flower.  Already defined
   above. */

/* Phase 2: Move the Hopf flower on the equator up and down to latitude ±60°
   while rotating it around the z axis and, with a certain probability,
   around a random axis. */
ANIM_SO_DEF(single_hopf_flower_move) = {
  {
    GEN_TORUS,                                                /* generator */
    1.0f*M_PI_F/6.0f,     5.0f*M_PI_F/6.0f,     EASING_SIN,   /* p */
    M_PI_F/16.0f,         M_PI_F/8.0f,          EASING_COS,   /* q */
    -0.5f,                -0.5f,                EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    120,                                /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_flower_move) = {
  ANIM_SO_NUM(single_hopf_flower_move),
  ANIM_SO_NAME(single_hopf_flower_move),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

/* Phase 3: Deform the Hopf flower into a Hopf torus.  Already defined
   above. */

ANIM_PH_DEF(single_hopf_flower_move) = {
  ANIM_MO_REF(single_hopf_torus_to_single_hopf_flower),
  ANIM_MO_REF(single_hopf_flower_move),
  ANIM_MO_REF(single_hopf_flower_to_single_hopf_torus)
};

ANIM_PS_DEF(single_hopf_flower_move) = {
  ANIM_PH_NUM(single_hopf_flower_move),
  ANIM_PH_NAME(single_hopf_flower_move)
};

ANIMS_M_DEF(single_hopf_torus) = {
  ANIM_PS_REF(single_hopf_torus_rot_x),
  ANIM_PS_REF(single_hopf_torus_rot_z),
  ANIM_PS_REF(single_hopf_torus_rot_rnd),
  ANIM_PS_REF(single_hopf_torus_move),
  ANIM_PS_REF(single_hopf_torus_wave),
  ANIM_PS_REF(single_hopf_flower_rot_x),
  ANIM_PS_REF(single_hopf_flower_rot_rnd),
  ANIM_PS_REF(single_hopf_flower_move)
};

ANIMS_DEF(single_hopf_torus) = {
  ANIMS_M_NUM(single_hopf_torus),
  ANIMS_M_NAME(single_hopf_torus)
};



/* The set of possible animations for the transition from a single Hopf
   torus to a single Hopf spiral. */

/* Reduce the Hopf torus to a Hopf torus of three fifths the point density
   and deform it to a point on the equator, then deform the torus to a Hopf
   spiral. */

/* Phase 1: Decrease the density of the Hopf torus and deform it to a torus
   on the equator. */
ANIM_SO_DEF(single_hopf_torus_to_single_mid_density_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    1.0f*M_PI_F/60.0f,    1.0f*M_PI_F/36.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    2.0f*M_PI_F/60.0f,    1.0f*M_PI_F/36.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    3.0f*M_PI_F/60.0f,    2.0f*M_PI_F/36.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    M_PI_F/8.0f,          0.0f,                 EASING_CUBIC, /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    4.0f*M_PI_F/60.0f,    2.0f*M_PI_F/36.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    4,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_torus_to_single_mid_density_torus) = {
  ANIM_SO_NUM(single_hopf_torus_to_single_mid_density_torus),
  ANIM_SO_NAME(single_hopf_torus_to_single_mid_density_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  90                                                          /* num_steps */
};

/* Phase 2: Wind the collapsed spiral and expand it.  Already defined above. */

ANIM_PH_DEF(single_hopf_torus_to_single_hopf_spiral_wind) = {
  ANIM_MO_REF(single_hopf_torus_to_single_mid_density_torus),
  ANIM_MO_REF(single_dense_unwound_torus_to_single_hopf_spiral)
};

ANIM_PS_DEF(single_hopf_torus_to_single_hopf_spiral_wind) = {
  ANIM_PH_NUM(single_hopf_torus_to_single_hopf_spiral_wind),
  ANIM_PH_NAME(single_hopf_torus_to_single_hopf_spiral_wind)
};

ANIMS_M_DEF(single_hopf_torus_to_single_hopf_spiral) = {
  ANIM_PS_REF(single_hopf_torus_to_single_hopf_spiral_wind)
};

ANIMS_DEF(single_hopf_torus_to_single_hopf_spiral) = {
  ANIMS_M_NUM(single_hopf_torus_to_single_hopf_spiral),
  ANIMS_M_NAME(single_hopf_torus_to_single_hopf_spiral)
};



/*****************************************************************************
 * The set of all transformations from a single Hopf spiral.
 *****************************************************************************/

/* The set of possible animations for the transition from a single Hopf
   spiral to a single point. */

/* Shrink a Hopf spiral to a point on the equator and rotate around a random
   axis with a certain probability. */
ANIM_SO_DEF(single_hopf_spiral_to_single_point_shrink_sector) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          0.0f,                 EASING_CUBIC, /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_single_point_shrink_sector) = {
  ANIM_SO_NUM(single_hopf_spiral_to_single_point_shrink_sector),
  ANIM_SO_NAME(single_hopf_spiral_to_single_point_shrink_sector),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_single_point_shrink_sector) = {
  ANIM_MO_REF(single_hopf_spiral_to_single_point_shrink_sector)
};

ANIM_PS_DEF(single_hopf_spiral_to_single_point_shrink_sector) = {
  ANIM_PH_NUM(single_hopf_spiral_to_single_point_shrink_sector),
  ANIM_PH_NAME(single_hopf_spiral_to_single_point_shrink_sector)
};

ANIMS_M_DEF(single_hopf_spiral_to_single_point) = {
  ANIM_PS_REF(single_hopf_spiral_to_single_point_shrink_sector)
};

ANIMS_DEF(single_hopf_spiral_to_single_point) = {
  ANIMS_M_NUM(single_hopf_spiral_to_single_point),
  ANIMS_M_NAME(single_hopf_spiral_to_single_point)
};



/* The set of possible animations for the transition from a single Hopf
   spiral to a single torus. */

/* Decrease the winding number of a Hopf spiral to one and collapse its
   height to a torus on the equator, and then decrease the point density
   of the torus by a factor of three. */

/* Phase 1: Unwind the spiral and collapse it to a dense torus. */
ANIM_SO_DEF(single_hopf_spiral_to_single_dense_torus_unwind) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_single_dense_torus_unwind) = {
  ANIM_SO_NUM(single_hopf_spiral_to_single_dense_torus_unwind),
  ANIM_SO_NAME(single_hopf_spiral_to_single_dense_torus_unwind),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

/* Phase 2: Decrease the density of the torus by a factor of three. */
ANIM_SO_DEF(single_dense_unwound_torus_to_single_torus) = {
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    M_PI_F/36.0f,         0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    0.0f,                 0.0f,                 EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_TORUS,                                                /* generator */
    M_PI_F/2.0f,          M_PI_F/2.0f,          EASING_NONE,  /* p */
    0.0f,                 0.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F/36.0f,        0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_unwound_torus_to_single_torus) = {
  ANIM_SO_NUM(single_dense_unwound_torus_to_single_torus),
  ANIM_SO_NAME(single_dense_unwound_torus_to_single_torus),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_single_torus_unwind) = {
  ANIM_MO_REF(single_hopf_spiral_to_single_dense_torus_unwind),
  ANIM_MO_REF(single_dense_unwound_torus_to_single_torus)
};

ANIM_PS_DEF(single_hopf_spiral_to_single_torus_unwind) = {
  ANIM_PH_NUM(single_hopf_spiral_to_single_torus_unwind),
  ANIM_PH_NAME(single_hopf_spiral_to_single_torus_unwind)
};

/* Increase the winding number of a Hopf spiral to three and collapse its
   height to a torus on the equator. */
ANIM_SO_DEF(single_hopf_spiral_to_single_torus_wind) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 3.0f,                 EASING_CUBIC, /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_single_torus_wind) = {
  ANIM_SO_NUM(single_hopf_spiral_to_single_torus_wind),
  ANIM_SO_NAME(single_hopf_spiral_to_single_torus_wind),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_single_torus_wind) = {
  ANIM_MO_REF(single_hopf_spiral_to_single_torus_wind)
};

ANIM_PS_DEF(single_hopf_spiral_to_single_torus_wind) = {
  ANIM_PH_NUM(single_hopf_spiral_to_single_torus_wind),
  ANIM_PH_NAME(single_hopf_spiral_to_single_torus_wind)
};

ANIMS_M_DEF(single_hopf_spiral_to_single_torus) = {
  ANIM_PS_REF(single_hopf_spiral_to_single_torus_unwind),
  ANIM_PS_REF(single_hopf_spiral_to_single_torus_wind)
};

ANIMS_DEF(single_hopf_spiral_to_single_torus) = {
  ANIMS_M_NUM(single_hopf_spiral_to_single_torus),
  ANIMS_M_NAME(single_hopf_spiral_to_single_torus)
};



/* The set of possible animations for the transition from a single Hopf
   spiral to a double torus. */

/* Decrease the point density of a Hopf spiral by a factor of two thirds,
   split the spiral into two parts, and deform the parts to two tori at
   latitiude ±45°. */

/* Phase 1: Decrease the density of the Hopf spiral by a factor of two
   thirds. */
ANIM_SO_DEF(single_hopf_spiral_to_single_loose_hopf_spiral) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -35.0f*M_PI_F/36.0f,  -23.0f*M_PI_F/24.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -34.0f*M_PI_F/36.0f,  -23.0f*M_PI_F/24.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_single_loose_hopf_spiral) = {
  ANIM_SO_NUM(single_hopf_spiral_to_single_loose_hopf_spiral),
  ANIM_SO_NAME(single_hopf_spiral_to_single_loose_hopf_spiral),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  60                                                          /* num_steps */
};

/* Phase 2: Split the spiral into two parts and deform it to two tori at
   latitiude ±45°. */
ANIM_SO_DEF(single_loose_hopf_spiral_to_double_torus_split) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 -M_PI_F/4.0f,         EASING_CUBIC, /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -2.0f*M_PI_F,         EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 M_PI_F/4.0f,          EASING_CUBIC, /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    0.0f,                 M_PI_F,               EASING_CUBIC, /* offset */
    M_PI_F,               M_PI_F,               EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_loose_hopf_spiral_to_double_torus_split) = {
  ANIM_SO_NUM(single_loose_hopf_spiral_to_double_torus_split),
  ANIM_SO_NAME(single_loose_hopf_spiral_to_double_torus_split),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_double_torus_split) = {
  ANIM_MO_REF(single_hopf_spiral_to_single_loose_hopf_spiral),
  ANIM_MO_REF(single_loose_hopf_spiral_to_double_torus_split)
};

ANIM_PS_DEF(single_hopf_spiral_to_double_torus_split) = {
  ANIM_PH_NUM(single_hopf_spiral_to_double_torus_split),
  ANIM_PH_NAME(single_hopf_spiral_to_double_torus_split)
};

ANIMS_M_DEF(single_hopf_spiral_to_double_torus) = {
  ANIM_PS_REF(single_hopf_spiral_to_double_torus_split)
};

ANIMS_DEF(single_hopf_spiral_to_double_torus) = {
  ANIMS_M_NUM(single_hopf_spiral_to_double_torus),
  ANIMS_M_NAME(single_hopf_spiral_to_double_torus)
};



/* The set of possible animations for the transition from a single Hopf
   spiral to a triple torus. */

/* Split the Hopf spiral into three parts and Deform them to three tori. */
ANIM_SO_DEF(single_hopf_spiral_to_triple_torus_split) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 -M_PI_F/4.0f,         EASING_CUBIC, /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -2.0f*M_PI_F,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F/3.0f,         -M_PI_F/2.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 M_PI_F/4.0f,          EASING_CUBIC, /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    M_PI_F/3.0f,          M_PI_F,               EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_triple_torus_split) = {
  ANIM_SO_NUM(single_hopf_spiral_to_triple_torus_split),
  ANIM_SO_NAME(single_hopf_spiral_to_triple_torus_split),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_triple_torus_split) = {
  ANIM_MO_REF(single_hopf_spiral_to_triple_torus_split)
};

ANIM_PS_DEF(single_hopf_spiral_to_triple_torus_split) = {
  ANIM_PH_NUM(single_hopf_spiral_to_triple_torus_split),
  ANIM_PH_NAME(single_hopf_spiral_to_triple_torus_split)
};

ANIMS_M_DEF(single_hopf_spiral_to_triple_torus) = {
  ANIM_PS_REF(single_hopf_spiral_to_triple_torus_split)
};

ANIMS_DEF(single_hopf_spiral_to_triple_torus) = {
  ANIMS_M_NUM(single_hopf_spiral_to_triple_torus),
  ANIMS_M_NAME(single_hopf_spiral_to_triple_torus)
};



/* The set of possible animations for the transition from a single Hopf
   spiral to a single Seifert surface. */

/* Decrease the winding number of a Hopf spiral to one and collapse its
   height to a Seifert surface on the equator, and decrease the point
   density by a factor of three. */
ANIM_SO_DEF(single_hopf_spiral_to_single_seifert_unwind_hor) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -M_PI_F,              M_PI_F/24.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -35.0f*M_PI_F/36.0f,  M_PI_F/24.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -34.0f*M_PI_F/36.0f,  M_PI_F/24.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          M_PI_F,               EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_single_seifert_unwind_hor) = {
  ANIM_SO_NUM(single_hopf_spiral_to_single_seifert_unwind_hor),
  ANIM_SO_NAME(single_hopf_spiral_to_single_seifert_unwind_hor),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_single_seifert_unwind_hor) = {
  ANIM_MO_REF(single_hopf_spiral_to_single_seifert_unwind_hor)
};

ANIM_PS_DEF(single_hopf_spiral_to_single_seifert_unwind_hor) = {
  ANIM_PH_NUM(single_hopf_spiral_to_single_seifert_unwind_hor),
  ANIM_PH_NAME(single_hopf_spiral_to_single_seifert_unwind_hor)
};

/* Decrease the winding number of a Hopf spiral to zero to create a vertical
   Seifert surface, then decrease the point density by a factor of three and
   rotate it to the equator. */

/* Phase 1: Unwind the spiral and collapse it to a dense vertical Seifert
   surface. */
ANIM_SO_DEF(single_hopf_spiral_to_single_dense_seifert_unwind) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 0.0f,                 EASING_CUBIC, /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_single_dense_seifert_unwind) = {
  ANIM_SO_NUM(single_hopf_spiral_to_single_dense_seifert_unwind),
  ANIM_SO_NAME(single_hopf_spiral_to_single_dense_seifert_unwind),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  180                                                         /* num_steps */
};

/* Phase 2: Decrease the density of the vertical Seifert surface and rotate
   it to its canonical position. */
ANIM_SO_DEF(single_dense_seifert_vert_to_single_seifert) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -2.0f*M_PI_F,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -35.0f*M_PI_F/36.0f,  -2.0f*M_PI_F,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    0.0f,                 0.0f,                 EASING_NONE,  /* r */
    -34.0f*M_PI_F/36.0f,  -2.0f*M_PI_F,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 1.0f, 0.0f, 0.0f }, 0.0f, M_PI_F/2.0f,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_dense_seifert_vert_to_single_seifert) = {
  ANIM_SO_NUM(single_dense_seifert_vert_to_single_seifert),
  ANIM_SO_NAME(single_dense_seifert_vert_to_single_seifert),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  120                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_single_seifert_unwind_vert) = {
  ANIM_MO_REF(single_hopf_spiral_to_single_dense_seifert_unwind),
  ANIM_MO_REF(single_dense_seifert_vert_to_single_seifert)
};

ANIM_PS_DEF(single_hopf_spiral_to_single_seifert_unwind_vert) = {
  ANIM_PH_NUM(single_hopf_spiral_to_single_seifert_unwind_vert),
  ANIM_PH_NAME(single_hopf_spiral_to_single_seifert_unwind_vert)
};

ANIMS_M_DEF(single_hopf_spiral_to_single_seifert) = {
  ANIM_PS_REF(single_hopf_spiral_to_single_seifert_unwind_hor),
  ANIM_PS_REF(single_hopf_spiral_to_single_seifert_unwind_vert)
};

ANIMS_DEF(single_hopf_spiral_to_single_seifert) = {
  ANIMS_M_NUM(single_hopf_spiral_to_single_seifert),
  ANIMS_M_NAME(single_hopf_spiral_to_single_seifert)
};



/* The set of possible animations for the transition from a single Hopf
   spiral to a triple Seifert surface. */

/* Deform the three Seifert surfaces to three Hopf spirals and merge them. */
ANIM_SO_DEF(single_hopf_spiral_to_triple_seifert_split) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 -M_PI_F/4.0f,         EASING_CUBIC, /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -95.0f*M_PI_F/48.0f,  EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F/3.0f,         M_PI_F/48.0f,         EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 M_PI_F/4.0f,          EASING_CUBIC, /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    M_PI_F/3.0f,          49.0f*M_PI_F/48.0f,   EASING_CUBIC, /* offset */
    2.0f*M_PI_F/3.0f,     M_PI_F/2.0f,          EASING_CUBIC, /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_triple_seifert_split) = {
  ANIM_SO_NUM(single_hopf_spiral_to_triple_seifert_split),
  ANIM_SO_NAME(single_hopf_spiral_to_triple_seifert_split),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_to_triple_seifert_split) = {
  ANIM_MO_REF(single_hopf_spiral_to_triple_seifert_split)
};

ANIM_PS_DEF(single_hopf_spiral_to_triple_seifert_split) = {
  ANIM_PH_NUM(single_hopf_spiral_to_triple_seifert_split),
  ANIM_PH_NAME(single_hopf_spiral_to_triple_seifert_split)
};

ANIMS_M_DEF(single_hopf_spiral_to_triple_seifert) = {
  ANIM_PS_REF(single_hopf_spiral_to_triple_seifert_split)
};

ANIMS_DEF(single_hopf_spiral_to_triple_seifert) = {
  ANIMS_M_NUM(single_hopf_spiral_to_triple_seifert),
  ANIMS_M_NAME(single_hopf_spiral_to_triple_seifert)
};



/* The set of possible animations for the transition from a single Hopf
   spiral to a single Hopf torus. */

/* Decrease the winding number of a Hopf spiral to one and collapse its
   height to a torus on the equator, and increase the point density
   of the torus by a factor of five thirds, then deform the torus to a
   Hopf torus. */

/* Phase 1: Unwind the spiral and collapse it to a dense torus. */
ANIM_SO_DEF(single_hopf_spiral_to_single_torus_densify) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -M_PI_F,              0.0f,                 EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -35.0f*M_PI_F/36.0f,  1.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -35.0f*M_PI_F/36.0f,  2.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -34.0f*M_PI_F/36.0f,  3.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  },
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 0.0f,                 EASING_CUBIC, /* q */
    2.0f,                 1.0f,                 EASING_CUBIC, /* r */
    -34.0f*M_PI_F/36.0f,  4.0f*M_PI_F/60.0f,    EASING_CUBIC, /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    24,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_to_single_torus_densify) = {
  ANIM_SO_NUM(single_hopf_spiral_to_single_torus_densify),
  ANIM_SO_NAME(single_hopf_spiral_to_single_torus_densify),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  240                                                         /* num_steps */
};

/* Phase 2: Deform the torus on the equator to a Hopf torus.  Already
   defined above. */

ANIM_PH_DEF(single_hopf_spiral_to_single_hopf_torus_unwind) = {
  ANIM_MO_REF(single_hopf_spiral_to_single_torus_densify),
  ANIM_MO_REF(single_dense_torus_to_single_hopf_torus)
};

ANIM_PS_DEF(single_hopf_spiral_to_single_hopf_torus_unwind) = {
  ANIM_PH_NUM(single_hopf_spiral_to_single_hopf_torus_unwind),
  ANIM_PH_NAME(single_hopf_spiral_to_single_hopf_torus_unwind)
};

ANIMS_M_DEF(single_hopf_spiral_to_single_hopf_torus) = {
  ANIM_PS_REF(single_hopf_spiral_to_single_hopf_torus_unwind)
};

ANIMS_DEF(single_hopf_spiral_to_single_hopf_torus) = {
  ANIMS_M_NUM(single_hopf_spiral_to_single_hopf_torus),
  ANIMS_M_NAME(single_hopf_spiral_to_single_hopf_torus)
};



/* The set of possible animations for a single Hopf spiral. */

/* Rotate a Hopf torus around the x axis of the total space. */
ANIM_SO_DEF(single_hopf_spiral_rot_x) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_rot_x) = {
  ANIM_SO_NUM(single_hopf_spiral_rot_x),
  ANIM_SO_NAME(single_hopf_spiral_rot_x),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 1.0f, 0.0f, 0.0f }, 0.0f, 2.0f*M_PI_F,      EASING_CUBIC, /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_rot_x) = {
  ANIM_MO_REF(single_hopf_spiral_rot_x)
};

ANIM_PS_DEF(single_hopf_spiral_rot_x) = {
  ANIM_PH_NUM(single_hopf_spiral_rot_x),
  ANIM_PH_NAME(single_hopf_spiral_rot_x)
};

/* Rotate a Hopf torus around the z axis. */
ANIM_SO_DEF(single_hopf_spiral_rot_z) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 1.0f }, 0.0f, 2.0f*M_PI_F,    EASING_CUBIC  /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_rot_z) = {
  ANIM_SO_NUM(single_hopf_spiral_rot_z),
  ANIM_SO_NAME(single_hopf_spiral_rot_z),
  0.0f,                                         EASING_NONE,  /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_rot_z) = {
  ANIM_MO_REF(single_hopf_spiral_rot_z)
};

ANIM_PS_DEF(single_hopf_spiral_rot_z) = {
  ANIM_PH_NUM(single_hopf_spiral_rot_z),
  ANIM_PH_NAME(single_hopf_spiral_rot_z)
};

/* Rotate a Hopf spiral around a random axis. */
ANIM_SO_DEF(single_hopf_spiral_rot_rnd) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    2.0f,                 2.0f,                 EASING_NONE,  /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_rot_rnd) = {
  ANIM_SO_NUM(single_hopf_spiral_rot_rnd),
  ANIM_SO_NAME(single_hopf_spiral_rot_rnd),
  1.0f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_rot_rnd) = {
  ANIM_MO_REF(single_hopf_spiral_rot_rnd)
};

ANIM_PS_DEF(single_hopf_spiral_rot_rnd) = {
  ANIM_PH_NUM(single_hopf_spiral_rot_rnd),
  ANIM_PH_NAME(single_hopf_spiral_rot_rnd)
};

/* Animate the winding number of a Hopf spiral and rotate it with a certain
   probability around a random axis. */
ANIM_SO_DEF(single_hopf_spiral_winding) = {
  {
    GEN_SPIRAL,                                               /* generator */
    0.0f,                 0.0f,                 EASING_NONE,  /* p */
    1.0f,                 1.0f,                 EASING_NONE,  /* q */
    1.0f,                 3.0f,                 EASING_SIN,   /* r */
    -M_PI_F,              -M_PI_F,              EASING_NONE,  /* offset */
    2.0f*M_PI_F,          2.0f*M_PI_F,          EASING_NONE,  /* sector */
    0,                    72,                                 /* n, num */
    { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,           EASING_NONE   /* rot_axis */
  }
};

ANIM_MO_DEF(single_hopf_spiral_winding) = {
  ANIM_SO_NUM(single_hopf_spiral_winding),
  ANIM_SO_NAME(single_hopf_spiral_winding),
  0.5f,                                         EASING_CUBIC, /* rotate_prob */
  { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f,             EASING_NONE,  /* rot_space */
  360                                                         /* num_steps */
};

ANIM_PH_DEF(single_hopf_spiral_winding) = {
  ANIM_MO_REF(single_hopf_spiral_winding)
};

ANIM_PS_DEF(single_hopf_spiral_winding) = {
  ANIM_PH_NUM(single_hopf_spiral_winding),
  ANIM_PH_NAME(single_hopf_spiral_winding)
};

ANIMS_M_DEF(single_hopf_spiral) = {
  ANIM_PS_REF(single_hopf_spiral_rot_x),
  ANIM_PS_REF(single_hopf_spiral_rot_z),
  ANIM_PS_REF(single_hopf_spiral_rot_rnd),
  ANIM_PS_REF(single_hopf_spiral_winding)
};

ANIMS_DEF(single_hopf_spiral) = {
  ANIMS_M_NUM(single_hopf_spiral),
  ANIMS_M_NAME(single_hopf_spiral)
};



/*****************************************************************************
 * The set of all possible animations.
 *****************************************************************************/
animations *hopf_animations[NUM_ANIM_STATES][NUM_ANIM_STATES] = {
  {
    ANIMS_REF(single_point),
    ANIMS_REF(single_point_to_single_torus),
    ANIMS_REF(single_point_to_double_torus),
    ANIMS_REF(single_point_to_triple_torus),
    ANIMS_REF(single_point_to_single_seifert),
    ANIMS_REF(single_point_to_triple_seifert),
    ANIMS_REF(single_point_to_single_hopf_torus),
    ANIMS_REF(single_point_to_single_hopf_spiral)
  },
  {
    ANIMS_REF(single_torus_to_single_point),
    ANIMS_REF(single_torus),
    ANIMS_REF(single_torus_to_double_torus),
    ANIMS_REF(single_torus_to_triple_torus),
    ANIMS_REF(single_torus_to_single_seifert),
    ANIMS_REF(single_torus_to_triple_seifert),
    ANIMS_REF(single_torus_to_single_hopf_torus),
    ANIMS_REF(single_torus_to_single_hopf_spiral)
  },
  {
    ANIMS_REF(double_torus_to_single_point),
    ANIMS_REF(double_torus_to_single_torus),
    ANIMS_REF(double_torus),
    ANIMS_REF(double_torus_to_triple_torus),
    ANIMS_REF(double_torus_to_single_seifert),
    ANIMS_REF(double_torus_to_triple_seifert),
    ANIMS_REF(double_torus_to_single_hopf_torus),
    ANIMS_REF(double_torus_to_single_hopf_spiral)
  },
  {
    ANIMS_REF(triple_torus_to_single_point),
    ANIMS_REF(triple_torus_to_single_torus),
    ANIMS_REF(triple_torus_to_double_torus),
    ANIMS_REF(triple_torus),
    ANIMS_REF(triple_torus_to_single_seifert),
    ANIMS_REF(triple_torus_to_triple_seifert),
    ANIMS_REF(triple_torus_to_single_hopf_torus),
    ANIMS_REF(triple_torus_to_single_hopf_spiral)
  },
  {
    ANIMS_REF(single_seifert_to_single_point),
    ANIMS_REF(single_seifert_to_single_torus),
    ANIMS_REF(single_seifert_to_double_torus),
    ANIMS_REF(single_seifert_to_triple_torus),
    ANIMS_REF(single_seifert),
    ANIMS_REF(single_seifert_to_triple_seifert),
    ANIMS_REF(single_seifert_to_single_hopf_torus),
    ANIMS_REF(single_seifert_to_single_hopf_spiral)
  },
  {
    ANIMS_REF(triple_seifert_to_single_point),
    ANIMS_REF(triple_seifert_to_single_torus),
    ANIMS_REF(triple_seifert_to_double_torus),
    ANIMS_REF(triple_seifert_to_triple_torus),
    ANIMS_REF(triple_seifert_to_single_seifert),
    ANIMS_REF(triple_seifert),
    ANIMS_REF(triple_seifert_to_single_hopf_torus),
    ANIMS_REF(triple_seifert_to_single_hopf_spiral),
  },
  {
    ANIMS_REF(single_hopf_torus_to_single_point),
    ANIMS_REF(single_hopf_torus_to_single_torus),
    ANIMS_REF(single_hopf_torus_to_double_torus),
    ANIMS_REF(single_hopf_torus_to_triple_torus),
    ANIMS_REF(single_hopf_torus_to_single_seifert),
    ANIMS_REF(single_hopf_torus_to_triple_seifert),
    ANIMS_REF(single_hopf_torus),
    ANIMS_REF(single_hopf_torus_to_single_hopf_spiral)
  },
  {
    ANIMS_REF(single_hopf_spiral_to_single_point),
    ANIMS_REF(single_hopf_spiral_to_single_torus),
    ANIMS_REF(single_hopf_spiral_to_double_torus),
    ANIMS_REF(single_hopf_spiral_to_triple_torus),
    ANIMS_REF(single_hopf_spiral_to_single_seifert),
    ANIMS_REF(single_hopf_spiral_to_triple_seifert),
    ANIMS_REF(single_hopf_spiral_to_single_hopf_torus),
    ANIMS_REF(single_hopf_spiral)
  }
};

#endif /* USE_GL */
