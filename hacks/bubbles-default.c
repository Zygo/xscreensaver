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

#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include "bubbles.h"
#include "yarandom.h"

#ifndef NO_DEFAULT_BUBBLE

# define BLOOD 0
# include "images/bubbles/blood1.xpm"
# include "images/bubbles/blood2.xpm"
# include "images/bubbles/blood3.xpm"
# include "images/bubbles/blood4.xpm"
# include "images/bubbles/blood5.xpm"
# include "images/bubbles/blood6.xpm"
# include "images/bubbles/blood7.xpm"
# include "images/bubbles/blood8.xpm"
# include "images/bubbles/blood9.xpm"
# include "images/bubbles/blood10.xpm"
# include "images/bubbles/blood11.xpm"

# define BLUE 1
# include "images/bubbles/blue1.xpm"
# include "images/bubbles/blue2.xpm"
# include "images/bubbles/blue3.xpm"
# include "images/bubbles/blue4.xpm"
# include "images/bubbles/blue5.xpm"
# include "images/bubbles/blue6.xpm"
# include "images/bubbles/blue7.xpm"
# include "images/bubbles/blue8.xpm"
# include "images/bubbles/blue9.xpm"
# include "images/bubbles/blue10.xpm"
# include "images/bubbles/blue11.xpm"

# define GLASS 2
# include "images/bubbles/glass1.xpm"
# include "images/bubbles/glass2.xpm"
# include "images/bubbles/glass3.xpm"
# include "images/bubbles/glass4.xpm"
# include "images/bubbles/glass5.xpm"
# include "images/bubbles/glass6.xpm"
# include "images/bubbles/glass7.xpm"
# include "images/bubbles/glass8.xpm"
# include "images/bubbles/glass9.xpm"
# include "images/bubbles/glass10.xpm"
# include "images/bubbles/glass11.xpm"

# define JADE 3
# include "images/bubbles/jade1.xpm"
# include "images/bubbles/jade2.xpm"
# include "images/bubbles/jade3.xpm"
# include "images/bubbles/jade4.xpm"
# include "images/bubbles/jade5.xpm"
# include "images/bubbles/jade6.xpm"
# include "images/bubbles/jade7.xpm"
# include "images/bubbles/jade8.xpm"
# include "images/bubbles/jade9.xpm"
# include "images/bubbles/jade10.xpm"
# include "images/bubbles/jade11.xpm"

# define END 4


char **default_bubbles[50];
int num_default_bubbles;

void init_default_bubbles(void)
{
  int i = 0;
  switch (random() % END) {
  case BLOOD:
    default_bubbles[i++] = blood1;
    default_bubbles[i++] = blood2;
    default_bubbles[i++] = blood3;
    default_bubbles[i++] = blood4;
    default_bubbles[i++] = blood5;
    default_bubbles[i++] = blood6;
    default_bubbles[i++] = blood7;
    default_bubbles[i++] = blood8;
    default_bubbles[i++] = blood9;
    default_bubbles[i++] = blood10;
    default_bubbles[i++] = blood11;
    break;

  case BLUE:
    default_bubbles[i++] = blue1;
    default_bubbles[i++] = blue2;
    default_bubbles[i++] = blue3;
    default_bubbles[i++] = blue4;
    default_bubbles[i++] = blue5;
    default_bubbles[i++] = blue6;
    default_bubbles[i++] = blue7;
    default_bubbles[i++] = blue8;
    default_bubbles[i++] = blue9;
    default_bubbles[i++] = blue10;
    default_bubbles[i++] = blue11;
    break;

  case GLASS:
    default_bubbles[i++] = glass1;
    default_bubbles[i++] = glass2;
    default_bubbles[i++] = glass3;
    default_bubbles[i++] = glass4;
    default_bubbles[i++] = glass5;
    default_bubbles[i++] = glass6;
    default_bubbles[i++] = glass7;
    default_bubbles[i++] = glass8;
    default_bubbles[i++] = glass9;
    default_bubbles[i++] = glass10;
    default_bubbles[i++] = glass11;
    break;

  case JADE:
    default_bubbles[i++] = jade1;
    default_bubbles[i++] = jade2;
    default_bubbles[i++] = jade3;
    default_bubbles[i++] = jade4;
    default_bubbles[i++] = jade5;
    default_bubbles[i++] = jade6;
    default_bubbles[i++] = jade7;
    default_bubbles[i++] = jade8;
    default_bubbles[i++] = jade9;
    default_bubbles[i++] = jade10;
    default_bubbles[i++] = jade11;
    break;

  default:
    abort();
    break;
  }

  default_bubbles[i] = 0;
  num_default_bubbles = i;
}

#endif /* NO_DEFAULT_BUBBLE */
