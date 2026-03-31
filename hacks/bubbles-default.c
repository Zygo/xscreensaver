/* bubbles_default.c - pick images for bubbles.c
 * By Jamie Zawinski <jwz@jwz.org>, 20-Jan-98.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include "bubbles.h"
#include "yarandom.h"

#ifndef NO_DEFAULT_BUBBLE

# define BLOOD 0
# include "images/gen/blood1_png.h"
# include "images/gen/blood2_png.h"
# include "images/gen/blood3_png.h"
# include "images/gen/blood4_png.h"
# include "images/gen/blood5_png.h"
# include "images/gen/blood6_png.h"
# include "images/gen/blood7_png.h"
# include "images/gen/blood8_png.h"
# include "images/gen/blood9_png.h"
# include "images/gen/blood10_png.h"
# include "images/gen/blood11_png.h"

# define BLUE 1
# include "images/gen/blue1_png.h"
# include "images/gen/blue2_png.h"
# include "images/gen/blue3_png.h"
# include "images/gen/blue4_png.h"
# include "images/gen/blue5_png.h"
# include "images/gen/blue6_png.h"
# include "images/gen/blue7_png.h"
# include "images/gen/blue8_png.h"
# include "images/gen/blue9_png.h"
# include "images/gen/blue10_png.h"
# include "images/gen/blue11_png.h"

# define GLASS 2
# include "images/gen/glass1_png.h"
# include "images/gen/glass2_png.h"
# include "images/gen/glass3_png.h"
# include "images/gen/glass4_png.h"
# include "images/gen/glass5_png.h"
# include "images/gen/glass6_png.h"
# include "images/gen/glass7_png.h"
# include "images/gen/glass8_png.h"
# include "images/gen/glass9_png.h"
# include "images/gen/glass10_png.h"
# include "images/gen/glass11_png.h"

# define JADE 3
# include "images/gen/jade1_png.h"
# include "images/gen/jade2_png.h"
# include "images/gen/jade3_png.h"
# include "images/gen/jade4_png.h"
# include "images/gen/jade5_png.h"
# include "images/gen/jade6_png.h"
# include "images/gen/jade7_png.h"
# include "images/gen/jade8_png.h"
# include "images/gen/jade9_png.h"
# include "images/gen/jade10_png.h"
# include "images/gen/jade11_png.h"

# define END 4


bubble_png default_bubbles[50];
int num_default_bubbles;

void init_default_bubbles(void)
{
  int i = 0;
  switch (random() % END) {

# define DEF(N,S) default_bubbles[i].png = N; default_bubbles[i].size = S; i++

  case BLOOD:
    DEF(blood1_png, sizeof(blood1_png));
    DEF(blood2_png, sizeof(blood2_png));
    DEF(blood3_png, sizeof(blood3_png));
    DEF(blood4_png, sizeof(blood4_png));
    DEF(blood5_png, sizeof(blood5_png));
    DEF(blood6_png, sizeof(blood6_png));
    DEF(blood7_png, sizeof(blood7_png));
    DEF(blood8_png, sizeof(blood8_png));
    DEF(blood9_png, sizeof(blood9_png));
    DEF(blood10_png, sizeof(blood10_png));
    DEF(blood11_png, sizeof(blood11_png));
    break;

  case BLUE:
    DEF(blue1_png, sizeof(blue1_png));
    DEF(blue2_png, sizeof(blue2_png));
    DEF(blue3_png, sizeof(blue3_png));
    DEF(blue4_png, sizeof(blue4_png));
    DEF(blue5_png, sizeof(blue5_png));
    DEF(blue6_png, sizeof(blue6_png));
    DEF(blue7_png, sizeof(blue7_png));
    DEF(blue8_png, sizeof(blue8_png));
    DEF(blue9_png, sizeof(blue9_png));
    DEF(blue10_png, sizeof(blue10_png));
    DEF(blue11_png, sizeof(blue11_png));
    break;

  case GLASS:
    DEF(glass1_png, sizeof(glass1_png));
    DEF(glass2_png, sizeof(glass2_png));
    DEF(glass3_png, sizeof(glass3_png));
    DEF(glass4_png, sizeof(glass4_png));
    DEF(glass5_png, sizeof(glass5_png));
    DEF(glass6_png, sizeof(glass6_png));
    DEF(glass7_png, sizeof(glass7_png));
    DEF(glass8_png, sizeof(glass8_png));
    DEF(glass9_png, sizeof(glass9_png));
    DEF(glass10_png, sizeof(glass10_png));
    DEF(glass11_png, sizeof(glass11_png));
    break;

  case JADE:
    DEF(jade1_png, sizeof(jade1_png));
    DEF(jade2_png, sizeof(jade2_png));
    DEF(jade3_png, sizeof(jade3_png));
    DEF(jade4_png, sizeof(jade4_png));
    DEF(jade5_png, sizeof(jade5_png));
    DEF(jade6_png, sizeof(jade6_png));
    DEF(jade7_png, sizeof(jade7_png));
    DEF(jade8_png, sizeof(jade8_png));
    DEF(jade9_png, sizeof(jade9_png));
    DEF(jade10_png, sizeof(jade10_png));
    DEF(jade11_png, sizeof(jade11_png));
    break;

  default:
    abort();
    break;
  }

  default_bubbles[i].png = 0;
  num_default_bubbles = i;
}

#endif /* NO_DEFAULT_BUBBLE */
