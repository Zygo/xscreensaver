/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)

   For the latest version, source code, and links to more of my stuff, see:
   http://www.eblong.com/zarf/stonerview.html

   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
*/

/* Ported away from GLUT (so that it can do `-root' and work with xscreensaver)
   by Jamie Zawinski <jwz@jwz.org>, 22-Jan-2001.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <GL/gl.h>

#include "yarandom.h"
#include "usleep.h"
#include "stonerview-move.h"

extern int init_view(int *argc, char *argv[]);
extern void win_draw(void);


int main(int argc, char *argv[])
{

  /* Randomize -- only need to do this here because this program
     doesn't use the `screenhack.h' or `lockmore.h' APIs. */
# undef ya_rand_init
  ya_rand_init (0);

  if (!init_view(&argc, argv))
    return -1;
  if (!init_move())
    return -1;

  while (1)
    {
      win_draw();
      move_increment();
      usleep (20000);
    }

  return 0;
}
