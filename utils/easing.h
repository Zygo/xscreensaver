/* Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 * Easing functions.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __EASING_H__
#define __EASING_H__

typedef enum { 
  EASE_NONE,
  EASE_IN_SINE,   EASE_OUT_SINE,     EASE_IN_OUT_SINE,
  EASE_IN_QUAD,    EASE_OUT_QUAD,    EASE_IN_OUT_QUAD,
  EASE_IN_CUBIC,   EASE_OUT_CUBIC,   EASE_IN_OUT_CUBIC,
  EASE_IN_QUART,   EASE_OUT_QUART,   EASE_IN_OUT_QUART,
  EASE_IN_QUINT,   EASE_OUT_QUINT,   EASE_IN_OUT_QUINT,
  EASE_IN_EXPO,    EASE_OUT_EXPO,    EASE_IN_OUT_EXPO,
  EASE_IN_CIRC,    EASE_OUT_CIRC,    EASE_IN_OUT_CIRC,
  EASE_IN_BACK,    EASE_OUT_BACK,    EASE_IN_OUT_BACK,
  EASE_IN_ELASTIC, EASE_OUT_ELASTIC, EASE_IN_OUT_ELASTIC,
  EASE_IN_BOUNCE,  EASE_OUT_BOUNCE,  EASE_IN_OUT_BOUNCE
} easing_function;

extern double ease (easing_function, double);

#endif /* __EASING_H__ */
