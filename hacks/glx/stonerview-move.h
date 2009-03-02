/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)

   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
*/

typedef struct elem_struct {
  GLfloat pos[3];
  GLfloat vervec[2];
  GLfloat col[4];
} elem_t;

extern elem_t elist[];

extern int init_move(void);
extern void final_move(void);
extern void move_increment(void);
