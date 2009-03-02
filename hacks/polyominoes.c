/* -*- Mode: C; tab-width: 4 -*- */
/* polyominoes --- Shows attempts to place polyominoes into a rectangle */

#if 0
static const char sccsid[] = "@(#)polyominoes.c 5.01 2000/12/18 xlockmore";
#endif

/*
 * Copyright (c) 2000 by Stephen Montgomery-Smith <stephen@math.missouri.edu>
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
 * Revision History:
 * 07-Jan-2001: Made improvement to the search algorithm for the puzzles
 *              involving identical polyominoes (via the variable 
 *              reason_to_not_attach).  By Stephen Montgomery-Smith.
 * 20-Dec-2000: Added more puzzles at David Bagley's request.
 * 27-Nov-2000: Adapted from euler2d.c Copyright (c) 2000 by Stephen
 *              Montgomery-Smith
 * 05-Jun-2000: Adapted from flow.c Copyright (c) 1996 by Tim Auckland
 * 18-Jul-1996: Adapted from swarm.c Copyright (c) 1991 by Patrick J. Naughton.
 * 31-Aug-1990: Adapted from xswarm by Jeff Butterworth. (butterwo@ncsc.org)
 */

#ifdef STANDALONE
#define MODE_polyominoes
#define PROGCLASS "Polyominoes"
#define HACK_INIT init_polyominoes
#define HACK_DRAW draw_polyominoes
#define polyominoes_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
"*cycles: 2000 \n" \
"*ncolors: 64 \n"
#define SMOOTH_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#include "erase.h"
#include <X11/Xutil.h>
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_polyominoes
#define DEF_IDENTICAL "False"
#define DEF_PLAIN "False"

static Bool identical;
static Bool plain;

static XrmOptionDescRec opts[] =
{
  {(char *) "-identical", (char *) ".polyominoes.identical", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+identical", (char *) ".polyominoes.identical", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-plain", (char *) ".polyominoes.plain", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+plain", (char *) ".polyominoes.plain", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
  {(caddr_t *) &identical, (char *) "identical", (char *) "Identical", (char *) DEF_IDENTICAL, t_Bool},
  {(caddr_t *) & plain, (char *) "plain", (char *) "Plain", (char *) DEF_PLAIN, t_Bool}
};
static OptionStruct desc[] =
{
  {(char *) "-/+identical", (char *) "turn on/off puzzles where the polyomino pieces are identical"},
  {(char *) "-/+plain", (char *) "turn on/off plain pieces"}
};

ModeSpecOpt polyominoes_opts =
{sizeof opts / sizeof opts[0], opts,
 sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   polyominoes_description = {
  "polyominoes", "init_polyominoes", "draw_polyominoes", "release_polyominoes",
  "refresh_polyominoes", "init_polyominoes", (char *) NULL, &polyominoes_opts,
  6000, 1, 8192, 1, 64, 1.0, "",
  "Shows attempts to place polyominoes into a rectangle", 0, NULL
};

#endif

/* Each polyomino is described by several quantities:
   len:             the number of squares in the polyomino;
   point:           the list of points;
   tranform_len:    the number of items in transform_list;
   transform_list:  a list of transformations that need to be done in order
                    to get all rotations and reflections (see the function
                    transform below);
   max_white:       the maximum number of white squares covered if polyomino
                    is placed on a chess board;
   color:           it's display color;
   attached:        whether it is currently attached to the rectangle;
   attach_point:    point on rectangle where attached;
   point_no:        the number of the point where it is attached;
   transform_index: which element of transform_list is currently being used.
*/

typedef struct {int x,y;} point_type;

typedef struct {int len; point_type *point;
                int transform_len, transform_list[8], max_white;
		unsigned long color;
		int attached;
		point_type attach_point;
		int point_no, transform_index;} polyomino_type;


typedef struct _polyominoesstruct{
  int wait;

  int width, height;
  unsigned border_color;
  int mono;

  polyomino_type *polyomino;
  int nr_polyominoes;
  Bool identical, use3D;
  int *attach_list;
  int nr_attached;

/* The array that tells where the polyominoes are attached. */
  int *array, *changed_array;

/* These specify the dimensions of how things appear on the screen. */
  int box, x_margin, y_margin;

/* These booleans decide in which way to try to attach the polyominoes. */
  int left_right, top_bottom;

/* Bitmaps for display purposes. */
  int use_bitmaps;
  XImage *bitmaps[256];

/* Structures used for display purposes if there is not enough memory
   to allocate bitmaps (or if the screen is small). */
  XSegment *lines;
  XRectangle *rectangles;

/* A procedure that may be used to see if certain configurations
   are permissible. */
  int (*check_ok)(struct _polyominoesstruct *sp);

/* Tells program that solutions are invariant under 180 degree
   rotation. */
  int rot180;

/* This is a variable used in the case that all the polyominoes are identical
   that will further prune the search tree.  Essentially it will be
   int reason_to_not_attach[nr_polynominoes][nr_polynominoes];
   Let me first explain the effect it is trying to overcome.  Often
   in the search process, the computer will first try to fit shapes into
   a region (call it A), and then into another region (call it B) that is
   essentially disjoint from A.  But it may be that it just is not possible
   to fit shapes into region B.  So it fits something into A, and then
   tries to fit something into B.  Failing it fits something else into A,
   and then tried again to fit something into B.  Thus the program is trying
   again and again to fit something into B, when it should have figured out
   the first time that it was impossible.

   To overcome this, everytime we try to attach a piece, we collect the reasons
   why it cannot be attached (a boolean for each piece that got in the way).
   If we see that a piece cannot be attached, we detach the other pieces until
   we have detached at least one piece for which the boolean reason_to_not_attach
   is set.
*/
  int *reason_to_not_attach;

  int counter;
} polyominoesstruct;

#define ARRAY(x,y)         (sp->array[(x)*sp->height+(y)])
#define CHANGED_ARRAY(x,y) (sp->changed_array[(x)*sp->height+(y)])
#define ARRAY_P(p)         (sp->array[(p).x*sp->height+(p).y])
#define CHANGED_ARRAY_P(p) (sp->changed_array[(p).x*sp->height+(p).y])
#define ARR(x,y) (((x)<0||(x)>=sp->width||(y)<0||(y)>=sp->height)?-2:ARRAY(x,y))

#define REASON_TO_NOT_ATTACH(x,y) (sp->reason_to_not_attach[(x)*sp->nr_polyominoes+(y)])

#define ROUND8(n) ((((n)+7)/8)*8)

/* Defines to index the bitmaps.  A set bit indicates that an edge or
   corner is required. */
#define LEFT (1<<0)
#define RIGHT (1<<1)
#define UP (1<<2)
#define DOWN (1<<3)
#define LEFT_UP (1<<4)
#define LEFT_DOWN (1<<5)
#define RIGHT_UP (1<<6)
#define RIGHT_DOWN (1<<7)
#define IS_LEFT(n) ((n) & LEFT)
#define IS_RIGHT(n) ((n) & RIGHT)
#define IS_UP(n) ((n) & UP)
#define IS_DOWN(n) ((n) & DOWN)
#define IS_LEFT_UP(n) ((n) & LEFT_UP)
#define IS_LEFT_DOWN(n) ((n) & LEFT_DOWN)
#define IS_RIGHT_UP(n) ((n) & RIGHT_UP)
#define IS_RIGHT_DOWN(n) ((n) & RIGHT_DOWN)

/* Defines to access the bitmaps. */
#define BITNO(x,y) ((x)+(y)*ROUND8(sp->box))
#define SETBIT(n,x,y) {data[BITNO(x,y)/8] |= 1<<(BITNO(x,y)%8);}
#define RESBIT(n,x,y) {data[BITNO(x,y)/8] &= ~(1<<(BITNO(x,y)%8));}
#define TWOTHIRDSBIT(n,x,y) {if ((x+y-1)%3) SETBIT(n,x,y) else RESBIT(n,x,y)}
#define HALFBIT(n,x,y) {if ((x-y)%2) SETBIT(n,x,y) else RESBIT(n,x,y)}
#define THIRDBIT(n,x,y) {if (!((x-y-1)%3)) SETBIT(n,x,y) else RESBIT(n,x,y)}
#define THREEQUARTERSBIT(n,x,y) \
  {if ((y%2)||((x+2+y/2+1)%2)) SETBIT(n,x,y) else RESBIT(n,x,y)}
#define NOTHALFBIT(n,x,y) {if ((x-y)%2) RESBIT(n,x,y) else SETBIT(n,x,y)}

/* Parameters for bitmaps. */
#define G ((sp->box/45)+1)         /* 1/2 of gap between polyominoes. */
#define T ((sp->box<=12)?1:(G*2))  /* Thickness of walls of polyominoes. */
#define R ((sp->box<=12)?1:(G*6))  /* Amount of rounding. */
#define RT ((sp->box<=12)?1:(G*3)) /* Thickness of wall of rounded parts. 
                                      Here 3 is an approximation to 2 sqrt(2). */
#define RR 0   /* Roof ridge thickness */

#if 0
/* A list of those bitmaps we need to create to display any pentomino. */
/* (not used right now because it does not seem to work for hexonimoes.) */

static int bitmaps_needed[] =
{
 LEFT_UP|LEFT_DOWN|RIGHT_UP|RIGHT_DOWN,

 LEFT|RIGHT_UP|RIGHT_DOWN,
 RIGHT|LEFT_UP|LEFT_DOWN,
 UP|LEFT_DOWN|RIGHT_DOWN,
 DOWN|LEFT_UP|RIGHT_UP,
 LEFT|RIGHT_UP,
 RIGHT|LEFT_UP,
 UP|LEFT_DOWN,
 DOWN|LEFT_UP,
 LEFT|RIGHT_DOWN,
 RIGHT|LEFT_DOWN,
 UP|RIGHT_DOWN,
 DOWN|RIGHT_UP,

/* These needed for hexonimoes*/
 LEFT,
 RIGHT,
 UP,
 DOWN,
 LEFT_DOWN|RIGHT_UP|RIGHT_DOWN,
 LEFT_UP|RIGHT_UP|RIGHT_DOWN,
 LEFT_UP|LEFT_DOWN|RIGHT_DOWN,
 LEFT_UP|LEFT_DOWN|RIGHT_UP,

 LEFT|UP|RIGHT_DOWN,
 LEFT|DOWN|RIGHT_UP,
 RIGHT|UP|LEFT_DOWN,
 RIGHT|DOWN|LEFT_UP,
 LEFT|UP,
 LEFT|DOWN,
 RIGHT|UP,
 RIGHT|DOWN,

 LEFT|RIGHT,
 UP|DOWN,

 RIGHT|UP|DOWN,
 LEFT|UP|DOWN,
 LEFT|RIGHT|DOWN,
 LEFT|RIGHT|UP,

 -1
};

static int bitmap_needed(int n) {
  int i;

  for (i=0;bitmaps_needed[i]!=-1;i++)
    if (n == bitmaps_needed[i])
      return 1;
  return 0;
}

#endif

/* Some debugging routines.

static void print_board(polyominoesstruct *sp) {
  int x,y;
  for (y=0;y<sp->height;y++) {
    for (x=0;x<sp->width;x++)
      if (ARRAY(x,y) == -1)
        fprintf(stderr," ");
      else
        fprintf(stderr,"%c",'a'+ARRAY(x,y));
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n");
}

static void print_points(point_type *point, int len) {
  int i;

  for (i=0;i<len;i++)
    fprintf(stderr,"(%d %d) ",point[i].x,point[i].y);
}

static void print_polyomino(polyomino_type poly) {
  int i;

  print_points(poly.point,poly.len);
  fprintf(stderr,"\n");
  for (i=0;i<poly.transform_len;i++)
    fprintf(stderr,"%d ",poly.transform_list[i]);
  fprintf(stderr,"\n");
}
*/

static polyominoesstruct *polyominoeses = (polyominoesstruct *) NULL;

static
void random_permutation(int n, int a[]) {
  int i,j,k,r;

  for (i=0;i<n;i++) a[i] = -1;
  for (i=0;i<n;i++) {
    r=NRAND(n-i);
    k=0;
    while(a[k]!=-1) k++;
    for (j=0;j<r;j++) {
      k++;
      while(a[k]!=-1) k++;
    }
    a[k]=i;
  }
}


/************************************************************
These are the routines for manipulating the polyominoes and
attaching and detaching them from the rectangle.
************************************************************/

static void transform(point_type in, point_type offset, int transform_no, 
                      point_type attach_point, point_type *out) {
  switch (transform_no) {
    case 0: out->x=in.x-offset.x+attach_point.x;
            out->y=in.y-offset.y+attach_point.y;
            break;
    case 1: out->x=-(in.y-offset.y)+attach_point.x;
            out->y=in.x-offset.x+attach_point.y;
            break;
    case 2: out->x=-(in.x-offset.x)+attach_point.x;
            out->y=-(in.y-offset.y)+attach_point.y;
            break;
    case 3: out->x=in.y-offset.y+attach_point.x;
            out->y=-(in.x-offset.x)+attach_point.y;
            break;
    case 4: out->x=-(in.x-offset.x)+attach_point.x;
            out->y=in.y-offset.y+attach_point.y;
            break;
    case 5: out->x=in.y-offset.y+attach_point.x;
            out->y=in.x-offset.x+attach_point.y;
            break;
    case 6: out->x=in.x-offset.x+attach_point.x;
            out->y=-(in.y-offset.y)+attach_point.y;
            break;
    case 7: out->x=-(in.y-offset.y)+attach_point.x;
            out->y=-(in.x-offset.x)+attach_point.y;
            break;
  }
}

static int first_poly_no(polyominoesstruct *sp) {
  int poly_no;

  poly_no = 0;
  while(poly_no<sp->nr_polyominoes && sp->polyomino[poly_no].attached)
    poly_no++;
  return poly_no;
}

static void next_poly_no(polyominoesstruct *sp, int *poly_no) {

  if (sp->identical) {
    *poly_no = sp->nr_polyominoes;
  } else {
    do {
      (*poly_no)++;
    } while (*poly_no<sp->nr_polyominoes && sp->polyomino[*poly_no].attached);
  }
}

/* check_all_regions_multiple_of looks for connected regions of 
   blank spaces, and returns 0 if it finds a connected region containing
   a number of blanks that is not a multiple of n.
*/

static void count_adjacent_blanks(polyominoesstruct *sp, int *count, int x, int y, int blank_mark) {

  if (ARRAY(x,y) == -1) {
    (*count)++;
    ARRAY(x,y) = blank_mark;
    if (x>=1) count_adjacent_blanks(sp, count,x-1,y,blank_mark);
    if (x<sp->width-1) count_adjacent_blanks(sp, count,x+1,y,blank_mark);
    if (y>=1) count_adjacent_blanks(sp, count,x,y-1,blank_mark);
    if (y<sp->height-1) count_adjacent_blanks(sp, count,x,y+1,blank_mark);
  }
}

static int check_all_regions_multiple_of(polyominoesstruct *sp, int n) {
  int x,y,count,good = 1;

  for (x=0;x<sp->width && good;x++) for (y=0;y<sp->height && good;y++) {
    count = 0;
    count_adjacent_blanks(sp, &count,x,y,-2);
    good = count%n == 0;
  }

  for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++)
    if (ARRAY(x,y) == -2)
      ARRAY(x,y) = -1;

  return good;
}

static int check_all_regions_positive_combination_of(polyominoesstruct *sp, int m, int n) {
  int x,y,count,good = 1;

  for (x=0;x<sp->width && good;x++) for (y=0;y<sp->height && good;y++) {
    count = 0;
    count_adjacent_blanks(sp, &count,x,y,-2);
    good = 0;
    for (;count>=0 && !good;count-=m)
      good = count%n == 0;
  }

  for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++)
    if (ARRAY(x,y) == -2)
      ARRAY(x,y) = -1;

  return good;
}

static int find_smallest_blank_component(polyominoesstruct *sp) {
  int x,y,size,smallest_size,blank_mark,smallest_mark;

  smallest_mark = blank_mark = -10;
  smallest_size = 1000000000;
  for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++) if (ARRAY(x,y) == -1) {
    size = 0;
    count_adjacent_blanks(sp, &size,x,y,blank_mark);
    if (size<smallest_size) {
      smallest_mark = blank_mark;
      smallest_size = size;
    }
    blank_mark--;
  }

  return smallest_mark;
}

/* "Chess board" check - see init_max_whites_list above for more details.
*/
/* If the rectangle is alternatively covered by white and black
   squares (chess board style), then this gives the list of
   the maximal possible whites covered by each polyomino.  It
   is used by the function whites_ok which makes sure that the
   array has a valid number of white or black remaining blanks left.
*/

static int whites_ok(polyominoesstruct *sp) {
  int x,y,poly_no,whites=0,blacks=0,max_white=0,min_white=0;

  for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++) {
    if (ARRAY(x,y) == -1 && (x+y)%2) whites++;
    if (ARRAY(x,y) == -1 && (x+y+1)%2) blacks++;
  }
  for (poly_no=0;poly_no<sp->nr_polyominoes;poly_no++) if (!sp->polyomino[poly_no].attached) {
    max_white += sp->polyomino[poly_no].max_white;
    min_white += sp->polyomino[poly_no].len - sp->polyomino[poly_no].max_white;
  }
  return (min_white <= blacks && min_white <= whites
          && blacks <= max_white && whites <= max_white);
}

/* This routine looks at the point (x,y) and sees how many polyominoes
   and all their various transforms may be attached there.
*/

static int
score_point(polyominoesstruct *sp, int x, int y, int min_score_so_far) {
  int poly_no, point_no, transform_index, i, attachable;
  point_type attach_point, target_point;
  int score = 0;

  if (x>=1 && x<sp->width-1 && y>=1 && y<sp->height-1 &&
      ARRAY(x-1,y-1)<0 && ARRAY(x-1,y)<0 && ARRAY(x-1,y+1)<0 && 
      ARRAY(x+1,y-1)<0 && ARRAY(x+1,y)<0 && ARRAY(x+1,y+1)<0 && 
      ARRAY(x,y-1)<0 && ARRAY(x,y+1)<0)
    return 10000;

  attach_point.x = x;
  attach_point.y = y;
  for (poly_no=first_poly_no(sp);poly_no<sp->nr_polyominoes;next_poly_no(sp,&poly_no))
  if (!sp->polyomino[poly_no].attached) {
    for (point_no=0;point_no<sp->polyomino[poly_no].len;point_no++)
    for (transform_index=0;transform_index<sp->polyomino[poly_no].transform_len;transform_index++) {
      attachable = 1;
      for (i=0;i<sp->polyomino[poly_no].len;i++) {
        transform(sp->polyomino[poly_no].point[i],
                  sp->polyomino[poly_no].point[point_no],
                  sp->polyomino[poly_no].transform_list[transform_index], 
                  attach_point, &target_point);
        if ( ! ((target_point.x>=0) && (target_point.x<sp->width) 
                  && (target_point.y>=0) && (target_point.y<sp->height)
                  && (ARRAY_P(target_point)<0))) {
          attachable = 0;
          break;
        }
      }
      if (attachable) {
        score++;
        if (score>=min_score_so_far)
          return score;
      }
    }
  }

  return score;
}

static void find_blank(polyominoesstruct *sp, point_type *point) {
  int score, worst_score;
  int x, y;
  int blank_mark;

  blank_mark = find_smallest_blank_component(sp);

  worst_score = 1000000;
  for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++) if (ARRAY(x,y)==blank_mark) {
    score = 100*score_point(sp,x,y,worst_score);
    if (score>0) {
      if (sp->left_right) score += 10*x;
      else                score += 10*(sp->width-1-x);
      if (sp->top_bottom) score += y;
      else                score += (sp->height-1-y);
    }
    if (score<worst_score) {
      point->x = x;
      point->y = y;
      worst_score = score;
    }
  }

  for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++) 
    if (ARRAY(x,y)<0) ARRAY(x,y) = -1;
}

/* Detaches the most recently attached polyomino. */

static
void detach(polyominoesstruct *sp, int *poly_no, int *point_no, int *transform_index, point_type *attach_point, int rot180) {
  int i;
  point_type target_point;

  if (sp->nr_attached == 0) return;
  sp->nr_attached--;
  *poly_no = sp->attach_list[sp->nr_attached];
  *point_no = sp->polyomino[*poly_no].point_no;
  *transform_index = sp->polyomino[*poly_no].transform_index;
  *attach_point = sp->polyomino[*poly_no].attach_point;
  for (i=0;i<sp->polyomino[*poly_no].len;i++) {
    transform(sp->polyomino[*poly_no].point[i],
              sp->polyomino[*poly_no].point[*point_no],
              sp->polyomino[*poly_no].transform_list[*transform_index]^(rot180<<1), 
              *attach_point, &target_point);
    ARRAY_P(target_point) = -1;
    CHANGED_ARRAY_P(target_point) = 1;
  }

  sp->polyomino[*poly_no].attached = 0;
}

/* Attempts to attach a polyomino at point (x,y) at the 
   point_no-th point of that polyomino, using the transform
   transform_no.  Returns 1 if successful.
*/

static
int attach(polyominoesstruct *sp, int poly_no, int point_no, int transform_index, point_type attach_point, int rot180,
           int *reason_to_not_attach) {
  point_type target_point;
  int i;
  int attachable = 1, worst_reason_not_to_attach = 1000000000;

  if (rot180) {
    attach_point.x = sp->width-1-attach_point.x;
    attach_point.y = sp->height-1-attach_point.y;
  }

  if (sp->polyomino[poly_no].attached)
    return 0;

  for (i=0;i<sp->polyomino[poly_no].len;i++) {
    transform(sp->polyomino[poly_no].point[i],
              sp->polyomino[poly_no].point[point_no],
              sp->polyomino[poly_no].transform_list[transform_index]^(rot180<<1), 
              attach_point, &target_point);
    if ( ! ((target_point.x>=0) && (target_point.x<sp->width) 
              && (target_point.y>=0) && (target_point.y<sp->height)
              && (ARRAY_P(target_point) == -1))) {
      if (sp->identical) {
        attachable = 0;
        if ((target_point.x>=0) && (target_point.x<sp->width) 
           && (target_point.y>=0) && (target_point.y<sp->height) 
           && (ARRAY_P(target_point) >= 0)
           && (ARRAY_P(target_point)<worst_reason_not_to_attach))
          worst_reason_not_to_attach = ARRAY_P(target_point);
      }
      else
        return 0;
    }
  }

  if (sp->identical && !attachable) {
    if (worst_reason_not_to_attach < 1000000000)
      reason_to_not_attach[worst_reason_not_to_attach] = 1;
    return 0;
  }

  for (i=0;i<sp->polyomino[poly_no].len;i++) {
    transform(sp->polyomino[poly_no].point[i],
              sp->polyomino[poly_no].point[point_no],
              sp->polyomino[poly_no].transform_list[transform_index]^(rot180<<1),
              attach_point, &target_point);
    ARRAY_P(target_point) = poly_no;
    CHANGED_ARRAY_P(target_point) = 1;
  }

  sp->attach_list[sp->nr_attached] = poly_no;
  sp->nr_attached++;

  sp->polyomino[poly_no].attached = 1;
  sp->polyomino[poly_no].point_no = point_no;
  sp->polyomino[poly_no].attach_point = attach_point;
  sp->polyomino[poly_no].transform_index = transform_index;

  if (!sp->check_ok(sp)) {
    detach(sp,&poly_no,&point_no,&transform_index,&attach_point,rot180);
    return 0;
  }

  return 1;
}

static
int next_attach_try(polyominoesstruct *sp, int *poly_no, int *point_no, int *transform_index) {

  (*transform_index)++;
  if (*transform_index>=sp->polyomino[*poly_no].transform_len) {
    *transform_index = 0;
    (*point_no)++;
    if (*point_no>=sp->polyomino[*poly_no].len) {
      *point_no = 0;
      next_poly_no(sp,poly_no);
      if (*poly_no>=sp->nr_polyominoes) {
        *poly_no = first_poly_no(sp);
        return 0;
      }
    }
  }
  return 1;
}


/*******************************************************
Display routines.
*******************************************************/

static void
draw_without_bitmaps(ModeInfo * mi, polyominoesstruct *sp) {
  Display *display = MI_DISPLAY(mi);
  Window  window = MI_WINDOW(mi);
  GC gc = MI_GC(mi);
  int x,y,poly_no,nr_lines,nr_rectangles;

  XSetLineAttributes(display,gc,sp->box/10+1,LineSolid,CapRound,JoinRound);

  for (poly_no=-1;poly_no<sp->nr_polyominoes;poly_no++) {
    nr_rectangles = 0;
    for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++)
    if (CHANGED_ARRAY(x,y) && ARRAY(x,y) == poly_no) {
      sp->rectangles[nr_rectangles].x = sp->x_margin + sp->box*x;
      sp->rectangles[nr_rectangles].y = sp->y_margin + sp->box*y;
      sp->rectangles[nr_rectangles].width = sp->box;
      sp->rectangles[nr_rectangles].height = sp->box;
      nr_rectangles++;
    }
    if (poly_no == -1)
      XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
    else
      XSetForeground(display, gc, sp->polyomino[poly_no].color);
    XFillRectangles(display, window, gc, sp->rectangles, nr_rectangles);
  }

  XSetForeground(display, gc, MI_BLACK_PIXEL(mi));

  nr_lines = 0;
  for (x=0;x<sp->width-1;x++) for (y=0;y<sp->height;y++) {
    if (ARRAY(x,y) == -1 && ARRAY(x+1,y) == -1
        && (CHANGED_ARRAY(x,y) || CHANGED_ARRAY(x+1,y))) {
      sp->lines[nr_lines].x1 = sp->x_margin + sp->box*(x+1);
      sp->lines[nr_lines].y1 = sp->y_margin + sp->box*y;
      sp->lines[nr_lines].x2 = sp->x_margin + sp->box*(x+1);
      sp->lines[nr_lines].y2 = sp->y_margin + sp->box*(y+1);
      nr_lines++;
    }
  }
  XDrawSegments(display, window, gc, sp->lines, nr_lines);

  nr_lines = 0;
  for (x=0;x<sp->width;x++) for (y=0;y<sp->height-1;y++) {
    if (ARRAY(x,y) == -1 && ARRAY(x,y+1) == -1
        && (CHANGED_ARRAY(x,y) || CHANGED_ARRAY(x,y+1))) {
      sp->lines[nr_lines].x1 = sp->x_margin + sp->box*x;
      sp->lines[nr_lines].y1 = sp->y_margin + sp->box*(y+1);
      sp->lines[nr_lines].x2 = sp->x_margin + sp->box*(x+1);
      sp->lines[nr_lines].y2 = sp->y_margin + sp->box*(y+1);
      nr_lines++;
    }
  }
  XDrawSegments(display, window, gc, sp->lines, nr_lines);

  XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
  XDrawRectangle(display, window, gc, sp->x_margin, sp->y_margin, sp->box*sp->width, sp->box*sp->height);

  XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

  nr_lines = 0;
  for (x=0;x<sp->width-1;x++) for (y=0;y<sp->height;y++) {
    if (ARRAY(x+1,y) != ARRAY(x,y)) {
      sp->lines[nr_lines].x1 = sp->x_margin + sp->box*(x+1);
      sp->lines[nr_lines].y1 = sp->y_margin + sp->box*y;
      sp->lines[nr_lines].x2 = sp->x_margin + sp->box*(x+1);
      sp->lines[nr_lines].y2 = sp->y_margin + sp->box*(y+1);
      nr_lines++;
    }
  }
  XDrawSegments(display, window, gc, sp->lines, nr_lines);

  nr_lines = 0;
  for (x=0;x<sp->width;x++) for (y=0;y<sp->height-1;y++) {
    if (ARRAY(x,y+1) != ARRAY(x,y)) {
      sp->lines[nr_lines].x1 = sp->x_margin + sp->box*x;
      sp->lines[nr_lines].y1 = sp->y_margin + sp->box*(y+1);
      sp->lines[nr_lines].x2 = sp->x_margin + sp->box*(x+1);
      sp->lines[nr_lines].y2 = sp->y_margin + sp->box*(y+1);
      nr_lines++;
    }
  }
  XDrawSegments(display, window, gc, sp->lines, nr_lines);
  XSetLineAttributes(display,gc,1,LineSolid,CapRound,JoinRound);
  XFlush(display);
}

static void create_bitmaps(ModeInfo * mi, polyominoesstruct *sp) {
  int x,y,n;
  char *data;

  for (n=0;n<256;n++) {

/* Avoid duplication of identical bitmaps. */
    if (IS_LEFT_UP(n) && (IS_LEFT(n) || IS_UP(n)))
      sp->bitmaps[n] = sp->bitmaps[n & ~LEFT_UP];
    else if (IS_LEFT_DOWN(n) && (IS_LEFT(n) || IS_DOWN(n)))
      sp->bitmaps[n] = sp->bitmaps[n & ~LEFT_DOWN];
    else if (IS_RIGHT_UP(n) && (IS_RIGHT(n) || IS_UP(n)))
      sp->bitmaps[n] = sp->bitmaps[n & ~RIGHT_UP];
    else if (IS_RIGHT_DOWN(n) && (IS_RIGHT(n) || IS_DOWN(n)))
      sp->bitmaps[n] = sp->bitmaps[n & ~RIGHT_DOWN];

    else /* if (bitmap_needed(n)) */ {
      data = (char *) malloc(sizeof(char)*(sp->box*ROUND8(sp->box)/8));
      if (data == NULL) {
        sp->use_bitmaps = 0;
        return;
      }

      for (y=0;y<sp->box;y++) for (x=0;x<sp->box;x++) {
        if (!sp->use3D) {
#ifdef SMALL_BELLYBUTTON
          if (x >= sp->box / 2 && x <= sp->box / 2 + 1 &&
              y >= sp->box / 2 && y <= sp->box / 2 + 1)
            NOTHALFBIT(n,x,y)
          else
#endif
            HALFBIT(n,x,y)
        } else if ((x>=y && x<=sp->box-y-1 && IS_UP(n))
            || (x<=y && x<=sp->box-y-1 && y<sp->box/2 && !IS_LEFT(n))
            || (x>=y && x>=sp->box-y-1 && y<sp->box/2 && !IS_RIGHT(n)))
          SETBIT(n,x,y)
        else if ((x<=y && x<=sp->box-y-1 && IS_LEFT(n))
            || (x>=y && x<=sp->box-y-1 && x<sp->box/2 && !IS_UP(n))
            || (x<=y && x>=sp->box-y-1 && x<sp->box/2 && !IS_DOWN(n)))
          TWOTHIRDSBIT(n,x,y)
        else if ((x>=y && x>=sp->box-y-1 && IS_RIGHT(n))
            || (x>=y && x<=sp->box-y-1 && x>=sp->box/2 && !IS_UP(n))
            || (x<=y && x>=sp->box-y-1 && x>=sp->box/2 && !IS_DOWN(n)))
          HALFBIT(n,x,y)
        else if ((x<=y && x>=sp->box-y-1 && IS_DOWN(n))
            || (x<=y && x<=sp->box-y-1 && y>=sp->box/2 && !IS_LEFT(n))
            || (x>=y && x>=sp->box-y-1 && y>=sp->box/2 && !IS_RIGHT(n)))
          THIRDBIT(n,x,y)
      }

      if (IS_LEFT(n)) 
        for (y=0;y<sp->box;y++) for (x=G;x<G+T;x++)
          SETBIT(n,x,y)
      if (IS_RIGHT(n)) 
        for (y=0;y<sp->box;y++) for (x=G;x<G+T;x++)
          SETBIT(n,sp->box-1-x,y)
      if (IS_UP(n))
        for (x=0;x<sp->box;x++) for (y=G;y<G+T;y++)
          SETBIT(n,x,y)
      if (IS_DOWN(n))
        for (x=0;x<sp->box;x++) for (y=G;y<G+T;y++)
          SETBIT(n,x,sp->box-1-y)
      if (IS_LEFT(n)) 
        for (y=0;y<sp->box;y++) for (x=0;x<G;x++)
          RESBIT(n,x,y)
      if (IS_RIGHT(n)) 
        for (y=0;y<sp->box;y++) for (x=0;x<G;x++)
          RESBIT(n,sp->box-1-x,y)
      if (IS_UP(n))
        for (x=0;x<sp->box;x++) for (y=0;y<G;y++)
          RESBIT(n,x,y)
      if (IS_DOWN(n))
        for (x=0;x<sp->box;x++) for (y=0;y<G;y++)
          RESBIT(n,x,sp->box-1-y)

      if (IS_LEFT(n) && IS_UP(n))
        for (x=G;x<=G+R;x++)
          for (y=G;y<=R+2*G-x;y++) {
            if (x+y>R+2*G-RT)
              SETBIT(n,x,y)
            else
              RESBIT(n,x,y)
          }
      if (IS_LEFT(n) && IS_DOWN(n))
        for (x=G;x<=G+R;x++)
          for (y=G;y<=R+2*G-x;y++) {
            if (x+y>R+2*G-RT)
              SETBIT(n,x,sp->box-1-y)
            else
              RESBIT(n,x,sp->box-1-y)
          }
      if (IS_RIGHT(n) && IS_UP(n))
        for (x=G;x<=G+R;x++)
          for (y=G;y<=R+2*G-x;y++) {
            if (x+y>R+2*G-RT)
              SETBIT(n,sp->box-1-x,y)
            else
              RESBIT(n,sp->box-1-x,y)
          }
      if (IS_RIGHT(n) && IS_DOWN(n))
        for (x=G;x<=G+R;x++)
          for (y=G;y<=R+2*G-x;y++) {
            if (x+y>R+2*G-RT)
              SETBIT(n,sp->box-1-x,sp->box-1-y)
            else
              RESBIT(n,sp->box-1-x,sp->box-1-y)
          }

      if (!IS_LEFT(n) && !IS_UP(n) && IS_LEFT_UP(n)) {
        for (x=0;x<G;x++) for(y=0;y<G;y++)
          RESBIT(n,x,y)
        for (x=G;x<G+T;x++) for(y=0;y<G;y++)
          SETBIT(n,x,y)
        for (x=0;x<G+T;x++) for(y=G;y<G+T;y++)
          SETBIT(n,x,y)
      }
      if (!IS_LEFT(n) && !IS_DOWN(n) && IS_LEFT_DOWN(n)) {
        for (x=0;x<G;x++) for(y=0;y<G;y++)
          RESBIT(n,x,sp->box-1-y)
        for (x=G;x<G+T;x++) for(y=0;y<G;y++)
          SETBIT(n,x,sp->box-1-y)
        for (x=0;x<G+T;x++) for(y=G;y<G+T;y++)
          SETBIT(n,x,sp->box-1-y)
      }
      if (!IS_RIGHT(n) && !IS_UP(n) && IS_RIGHT_UP(n)) {
        for (x=0;x<G;x++) for(y=0;y<G;y++)
          RESBIT(n,sp->box-1-x,y)
        for (x=G;x<G+T;x++) for(y=0;y<G;y++)
          SETBIT(n,sp->box-1-x,y)
        for (x=0;x<G+T;x++) for(y=G;y<G+T;y++)
          SETBIT(n,sp->box-1-x,y)
      }
      if (!IS_RIGHT(n) && !IS_DOWN(n) && IS_RIGHT_DOWN(n)) {
        for (x=0;x<G;x++) for(y=0;y<G;y++)
          RESBIT(n,sp->box-1-x,sp->box-1-y)
        for (x=G;x<G+T;x++) for(y=0;y<G;y++)
          SETBIT(n,sp->box-1-x,sp->box-1-y)
        for (x=0;x<G+T;x++) for(y=G;y<G+T;y++)
          SETBIT(n,sp->box-1-x,sp->box-1-y)
      }

#ifdef LARGE_BELLYBUTTON
      if (!sp->use3D) {
        if (!IS_LEFT(n) && !IS_UP(n) && !IS_LEFT_UP(n))
          for (x=0;x<G+T;x++) for(y=0;y<G+T;y++)
            SETBIT(n,x,y)
        if (!IS_LEFT(n) && !IS_DOWN(n) && !IS_LEFT_DOWN(n))
          for (x=0;x<G+T;x++) for(y=sp->box-G-T;y<sp->box;y++)
            SETBIT(n,x,y)
        if (!IS_RIGHT(n) && !IS_UP(n) && !IS_RIGHT_UP(n))
          for (x=sp->box-G-T;x<sp->box;x++) for(y=0;y<G+T;y++)
            SETBIT(n,x,y)
        if (!IS_RIGHT(n) && !IS_DOWN(n) && !IS_RIGHT_DOWN(n))
          for (x=sp->box-G-T;x<sp->box;x++) for(y=sp->box-G-T;y<sp->box;y++)
            SETBIT(n,x,y)
      } else
#else
      if (sp->use3D)
#endif
      {
        if (!IS_LEFT(n) && !IS_UP(n) && !IS_LEFT_UP(n))
          for (x=0;x<sp->box/2-RR;x++) for(y=0;y<sp->box/2-RR;y++)
            THREEQUARTERSBIT(n,x,y)
        if (!IS_LEFT(n) && !IS_DOWN(n) && !IS_LEFT_DOWN(n))
          for (x=0;x<sp->box/2-RR;x++) for(y=sp->box/2+RR;y<sp->box;y++)
            THREEQUARTERSBIT(n,x,y)
        if (!IS_RIGHT(n) && !IS_UP(n) && !IS_RIGHT_UP(n))
          for (x=sp->box/2+RR;x<sp->box;x++) for(y=0;y<sp->box/2-RR;y++)
            THREEQUARTERSBIT(n,x,y)
        if (!IS_RIGHT(n) && !IS_DOWN(n) && !IS_RIGHT_DOWN(n))
          for (x=sp->box/2+RR;x<sp->box;x++) for(y=sp->box/2+RR;y<sp->box;y++)
            THREEQUARTERSBIT(n,x,y)
      }

      sp->bitmaps[n] = XCreateImage(MI_DISPLAY(mi), MI_VISUAL(mi), 1, XYBitmap,
                                    0, data, sp->box, sp->box, 8, 0);
      if (sp->bitmaps[n] == None) {
        free(data);
        sp->use_bitmaps = 0;
        return;
      }
      sp->bitmaps[n]->byte_order = MSBFirst;
      sp->bitmaps[n]->bitmap_unit = 8;
      sp->bitmaps[n]->bitmap_bit_order = LSBFirst;
    }
  }

  sp->use_bitmaps = 1;
}

static void draw_with_bitmaps(ModeInfo * mi, polyominoesstruct *sp) {
  Display *display = MI_DISPLAY(mi);
  Window  window = MI_WINDOW(mi);
  GC gc = MI_GC(mi);
  int x,y,t,bitmap_index;

  for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++) {
    if (ARRAY(x,y) == -1) {
      if (CHANGED_ARRAY(x,y)) {
        XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
        XFillRectangle(display,window,gc,
                       sp->x_margin + sp->box*x,
                       sp->y_margin + sp->box*y,
                       sp->box,sp->box);
      }
    }
    else {
      XSetForeground(display, gc, sp->polyomino[ARRAY(x,y)].color);
      bitmap_index = 0;
      if (ARR(x,y) != ARR(x-1,y))   bitmap_index |= LEFT;
      if (ARR(x,y) != ARR(x+1,y))   bitmap_index |= RIGHT;
      if (ARR(x,y) != ARR(x,y-1))   bitmap_index |= UP;
      if (ARR(x,y) != ARR(x,y+1))   bitmap_index |= DOWN;
      if (ARR(x,y) != ARR(x-1,y-1)) bitmap_index |= LEFT_UP;
      if (ARR(x,y) != ARR(x-1,y+1)) bitmap_index |= LEFT_DOWN;
      if (ARR(x,y) != ARR(x+1,y-1)) bitmap_index |= RIGHT_UP;
      if (ARR(x,y) != ARR(x+1,y+1)) bitmap_index |= RIGHT_DOWN;
      (void) XPutImage(display,window,gc,
                sp->bitmaps[bitmap_index],
                0,0,
                sp->x_margin + sp->box*x,
                sp->y_margin + sp->box*y,
                sp->box,sp->box);
    }
  }

  XSetForeground(display, gc, sp->border_color);
  for (t=G;t<G+T;t++)
    XDrawRectangle(display,window,gc,
                   sp->x_margin-t-1,sp->y_margin-t-1,
                   sp->box*sp->width+1+2*t, sp->box*sp->height+1+2*t);
  XFlush(display);
}


/***************************************************
Routines to initialise and close down polyominoes.
***************************************************/

static void free_bitmaps(polyominoesstruct *sp) {
  int n;
  
  for (n=0;n<256;n++)
/* Don't bother to free duplicates */
    if (IS_LEFT_UP(n) && (IS_LEFT(n) || IS_UP(n)))
      sp->bitmaps[n] = None;
    else if (IS_LEFT_DOWN(n) && (IS_LEFT(n) || IS_DOWN(n)))
      sp->bitmaps[n] = None;
    else if (IS_RIGHT_UP(n) && (IS_RIGHT(n) || IS_UP(n)))
      sp->bitmaps[n] = None;
    else if (IS_RIGHT_DOWN(n) && (IS_RIGHT(n) || IS_DOWN(n)))
      sp->bitmaps[n] = None;

    else if (sp->bitmaps[n] != None) {
        XDestroyImage(sp->bitmaps[n]);
        sp->bitmaps[n] = None;
    }
}

#define deallocate(p,t) if ((p)!=NULL) {free(p); p=(t*)NULL;}

static void free_polyominoes(polyominoesstruct *sp) {
  int n;

  for (n=0;n<sp->nr_polyominoes;n++) {
    deallocate(sp->polyomino[n].point, point_type);
  }

  deallocate(sp->polyomino, polyomino_type);
  deallocate(sp->attach_list, int);
  deallocate(sp->rectangles, XRectangle);
  deallocate(sp->lines, XSegment);
  deallocate(sp->reason_to_not_attach, int);
  deallocate(sp->array, int);
  deallocate(sp->changed_array, int);

  free_bitmaps(sp);
}

#define set_allocate(p,type,size) p = (type *) malloc(size);		\
                             if ((p)==NULL) {free_polyominoes(sp);return 0;}

#define copy_polyomino(dst,src,new_rand)				\
  (dst).len=(src).len;							\
  (dst).max_white = (src).max_white;					\
  set_allocate((dst).point,point_type,sizeof(point_type)*(src).len);		\
  (dst).len = (src).len;						\
  if (new_rand)								\
    random_permutation((src).len,perm_point);				\
  for (i=0;i<(src).len;i++)						\
    (dst).point[i] = (src).point[perm_point[i]];			\
  (dst).transform_len = (src).transform_len;				\
  if (new_rand)								\
    random_permutation((src).transform_len,perm_transform);		\
  for (i=0;i<(src).transform_len;i++)					\
    (dst).transform_list[i] = (src).transform_list[perm_transform[i]];	\
  (dst).attached = 0


/***************************************************
Puzzle specific initialization routines.
***************************************************/

static
int check_pentomino_puzzle(polyominoesstruct *sp) {
  return check_all_regions_multiple_of(sp, 5) && whites_ok(sp);
}

static
int check_hexomino_puzzle(polyominoesstruct *sp) {
  return check_all_regions_multiple_of(sp, 6) && whites_ok(sp);
}

static
int check_tetr_pentomino_puzzle(polyominoesstruct *sp) {
  return check_all_regions_positive_combination_of(sp, 5, 4) && whites_ok(sp);
}

static
int check_pent_hexomino_puzzle(polyominoesstruct *sp) {
  return check_all_regions_positive_combination_of(sp, 6, 5) && whites_ok(sp);
}

static
int check_heptomino_puzzle(polyominoesstruct *sp) {
  return check_all_regions_multiple_of(sp, 7) && whites_ok(sp);
}

static
int check_octomino_puzzle(polyominoesstruct *sp) {
  return check_all_regions_multiple_of(sp, 8) && whites_ok(sp);
}

static
int check_dekomino_puzzle(polyominoesstruct *sp) {
  return check_all_regions_multiple_of(sp, 10) && whites_ok(sp);
}

static
int check_elevenomino_puzzle(polyominoesstruct *sp) {
  return check_all_regions_multiple_of(sp, 11) && whites_ok(sp);
}

static struct {int len; point_type point[4];
               int transform_len, transform_list[8], max_white;} tetromino[5] =
{
/*
xxxx 
*/
  {4, {{0,0}, {1,0}, {2,0}, {3,0}},
   2, {0, 1, -1, -1, -1, -1, -1, -1}, 2},
/*
xxx  
  x  
*/
  {4, {{0,0}, {1,0}, {2,0}, {2,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 2},
/*
xxx  
 x   
*/
  {4, {{0,0}, {1,0}, {1,1}, {2,0}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3},
/*
xx   
 xx  
*/
  {4, {{0,0}, {1,0}, {1,1}, {2,1}},
   4, {0, 1, 4, 5, -1, -1, -1, -1}, 2},
/*
xx   
xx   
*/
  {4, {{0,0}, {0,1}, {1,0}, {1,1}},
   1, {0, -1, -1, -1, -1, -1, -1, -1}, 2}};


static struct pentomino_struct {int len; point_type point[5];
                                int transform_len, transform_list[8], max_white;}
  pentomino[12] =
{
/*
xxxxx 
*/
  {5, {{0,0}, {1,0}, {2,0}, {3,0}, {4,0}},
   2, {0, 1, -1, -1, -1, -1, -1, -1}, 3},
/*
xxxx  
   x  
*/
  {5, {{0,0}, {1,0}, {2,0}, {3,0}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxxx  
  x   
*/
  {5, {{0,0}, {1,0}, {2,0}, {2,1}, {3,0}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
  x   
xxx   
  x   
*/
  {5, {{0,0}, {1,0}, {2,-1}, {2,0}, {2,1}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3},
/*
xxx   
  xx  
*/
  {5, {{0,0}, {1,0}, {2,0}, {2,1}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxx   
 xx   
*/
  {5, {{0,0}, {1,0}, {1,1}, {2,0}, {2,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxx   
  x   
  x   
*/
  {5, {{0,0}, {1,0}, {2,0}, {2,1}, {2,2}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3},
/*
 x    
xxx   
  x   
*/
  {5, {{0,0}, {1,-1}, {1,0}, {2,0}, {2,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxx   
x x   
*/
  {5, {{0,0}, {0,1}, {1,0}, {2,0}, {2,1}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3},
/*
  x   
xxx   
x     
*/
  {5, {{0,0}, {0,1}, {1,0}, {2,-1}, {2,0}},
   4, {0, 1, 4, 5, -1, -1, -1, -1}, 3},
/*
 x    
xxx   
 x    
*/
  {5, {{0,0}, {1,-1}, {1,0}, {1,1}, {2,0}},
   1, {0, -1, -1, -1, -1, -1, -1, -1}, 4},
/*
xx    
 xx   
  x   
*/
  {5, {{0,0}, {1,0}, {1,1}, {2,1}, {2,2}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3}};


static struct hexomino_struct {int len; point_type point[6];
                               int transform_len, transform_list[8], max_white;}
  hexomino[35] =
{
/*
xxxxxx 
*/
  {6, {{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}},
   2, {0, 1, -1, -1, -1, -1, -1, -1}, 3},
/*
xxxxx  
    x  
*/
  {6, {{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {4,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxxxx  
   x   
*/
  {6, {{0,0}, {1,0}, {2,0}, {3,0}, {3,1}, {4,0}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 4},
/*
xxxxx  
  x    
*/
  {6, {{0,0}, {1,0}, {2,0}, {2,1}, {3,0}, {4,0}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3},
/*
   x   
xxxx   
   x   
*/
  {6, {{0,0}, {1,0}, {2,0}, {3,-1}, {3,0}, {3,1}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 4},
/*
xxxx   
   xx  
*/
  {6, {{0,0}, {1,0}, {2,0}, {3,0}, {3,1}, {4,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxxx   
  xx   
*/
  {6, {{0,0}, {1,0}, {2,0}, {2,1}, {3,0}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxxx   
   x   
   x   
*/
  {6, {{0,0}, {1,0}, {2,0}, {3,0}, {3,1}, {3,2}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
  x    
xxxx   
   x   
*/
  {6, {{0,0}, {1,0}, {2,-1}, {2,0}, {3,0}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxxx   
 x x   
*/
  {6, {{0,0}, {1,0}, {1,1}, {2,0}, {3,0}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 4},
/*
 x     
xxxx   
   x   
*/
  {6, {{0,0}, {1,-1}, {1,0}, {2,0}, {3,0}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 4},
/*
xxxx   
x  x   
*/
  {6, {{0,0}, {0,1}, {1,0}, {2,0}, {3,0}, {3,1}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3},
/*
   x   
xxxx   
x      
*/
  {6, {{0,0}, {0,1}, {1,0}, {2,0}, {3,-1}, {3,0}},
   4, {0, 1, 4, 5, -1, -1, -1, -1}, 3},
/*
  x    
xxxx   
  x    
*/
  {6, {{0,0}, {1,0}, {2,-1}, {2,0}, {2,1}, {3,0}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 4},
/*
xxxx   
 xx    
*/
  {6, {{0,0}, {1,0}, {1,1}, {2,0}, {2,1}, {3,0}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3},
/*
xxxx   
  x    
  x    
*/
  {6, {{0,0}, {1,0}, {2,0}, {2,1}, {2,2}, {3,0}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
 x     
xxxx   
  x    
*/
  {6, {{0,0}, {1,-1}, {1,0}, {2,0}, {2,1}, {3,0}},
   4, {0, 1, 4, 5, -1, -1, -1, -1}, 3},
/*
  xx   
xxx    
  x    
*/
  {6, {{0,0}, {1,0}, {2,-1}, {2,0}, {2,1}, {3,-1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
 xx    
xxx    
  x    
*/
  {6, {{0,0}, {1,-1}, {1,0}, {2,-1}, {2,0}, {2,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
  x    
xxx    
x x    
*/
  {6, {{0,0}, {0,1}, {1,0}, {2,-1}, {2,0}, {2,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 4},
/*
xxx    
  xxx  
*/
  {6, {{0,0}, {1,0}, {2,0}, {2,1}, {3,1}, {4,1}},
   4, {0, 1, 4, 5, -1, -1, -1, -1}, 3},
/*
xxx    
  xx   
   x   
*/
  {6, {{0,0}, {1,0}, {2,0}, {2,1}, {3,1}, {3,2}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxx    
 xxx   
*/
  {6, {{0,0}, {1,0}, {1,1}, {2,0}, {2,1}, {3,1}},
   4, {0, 1, 4, 5, -1, -1, -1, -1}, 4},
/*
xxx    
  xx   
  x    
*/
  {6, {{0,0}, {1,0}, {2,0}, {2,1}, {2,2}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 4},
/*
 x     
xxx    
  xx   
*/
  {6, {{0,0}, {1,-1}, {1,0}, {2,0}, {2,1}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 4},
/*
xxx    
x xx   
*/
  {6, {{0,0}, {0,1}, {1,0}, {2,0}, {2,1}, {3,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxx    
 xx    
  x    
*/
  {6, {{0,0}, {1,0}, {1,1}, {2,0}, {2,1}, {2,2}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 4},
/*
 x     
xxx    
 xx    
*/
  {6, {{0,0}, {1,-1}, {1,0}, {1,1}, {2,0}, {2,1}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 4},
/*
xxx    
xxx    
*/
  {6, {{0,0}, {0,1}, {1,0}, {1,1}, {2,0}, {2,1}},
   2, {0, 1, -1, -1, -1, -1, -1, -1}, 3},
/*
xxx    
  x    
  xx   
*/
  {6, {{0,0}, {1,0}, {2,0}, {2,1}, {2,2}, {3,2}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xxx    
  x    
 xx    
*/
  {6, {{0,0}, {1,0}, {1,2}, {2,0}, {2,1}, {2,2}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
 x     
xxx    
x x    
*/
  {6, {{0,0}, {0,1}, {1,-1}, {1,0}, {2,0}, {2,1}},
   4, {0, 1, 2, 3, -1, -1, -1, -1}, 3},
/*
  xx   
xxx    
x      
*/
  {6, {{0,0}, {0,1}, {1,0}, {2,-1}, {2,0}, {3,-1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
 xx    
xxx    
x      
*/
  {6, {{0,0}, {0,1}, {1,-1}, {1,0}, {2,-1}, {2,0}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3},
/*
xx     
 xx    
  xx   
*/
  {6, {{0,0}, {1,0}, {1,1}, {2,1}, {2,2}, {3,2}},
   4, {0, 1, 4, 5, -1, -1, -1, -1}, 3}};

static struct pentomino_struct one_sided_pentomino[60];

void make_one_sided_pentomino(void) {
  int i,j,t,u;

  j=0;
  for (i=0;i<18;i++) {
    one_sided_pentomino[j] = pentomino[i];
    for (t=0;t<8;t++)
      if (one_sided_pentomino[j].transform_list[t]>=4) {
        one_sided_pentomino[j].transform_len = t;
        j++;
        one_sided_pentomino[j] = pentomino[i];
        for (u=t;u<8;u++) one_sided_pentomino[j].transform_list[u-t] = one_sided_pentomino[j].transform_list[u];
        one_sided_pentomino[j].transform_len -= t;
        break;
      }
    j++;
  }
}

static struct hexomino_struct one_sided_hexomino[60];

void make_one_sided_hexomino(void) {
  int i,j,t,u;

  j=0;
  for (i=0;i<35;i++) {
    one_sided_hexomino[j] = hexomino[i];
    for (t=0;t<8;t++)
      if (one_sided_hexomino[j].transform_list[t]>=4) {
        one_sided_hexomino[j].transform_len = t;
        j++;
        one_sided_hexomino[j] = hexomino[i];
        for (u=t;u<8;u++) one_sided_hexomino[j].transform_list[u-t] = one_sided_hexomino[j].transform_list[u];
        one_sided_hexomino[j].transform_len -= t;
        break;
      }
    j++;
  }
}

/*
Find all the ways of placing all twelve pentominoes
into a rectangle whose size is 20x3, 15x4, 12x5 or 10x6.
*/

static
int set_pentomino_puzzle(polyominoesstruct *sp) {
  int perm_poly[12], perm_point[5], perm_transform[8], i, p;

  switch (NRAND(4)) {
    case 0:
      sp->width = 20;
      sp->height = 3;
      break;
    case 1:
      sp->width = 15;
      sp->height = 4;
      break;
    case 2:
      sp->width = 12;
      sp->height = 5;
      break;
    case 3:
      sp->width = 10;
      sp->height = 6;
      break;
  }

  sp->nr_polyominoes = 12;
  set_allocate(sp->polyomino,polyomino_type,12*sizeof(polyomino_type));
  random_permutation(12,perm_poly);
  for (p=0;p<12;p++) {
    copy_polyomino(sp->polyomino[p],pentomino[perm_poly[p]],1);
  }

  sp->check_ok = check_pentomino_puzzle;

  return 1;
}

/*
Many of the following puzzles are inspired by
http://www.xs4all.nl/~gp/PolyominoSolver/Polyomino.html
*/

/*
Find all the ways of placing all eighteen one-sided pentominoes
into a rectangle.
*/

static
int set_one_sided_pentomino_puzzle(polyominoesstruct *sp) {
  int perm_poly[18], perm_point[5], perm_transform[8], i, p;

  make_one_sided_pentomino();

  switch (NRAND(4)) {
    case 0:
      sp->width = 30;
      sp->height = 3;
      break;
    case 1:
      sp->width = 18;
      sp->height = 5;
      break;
    case 2:
      sp->width = 15;
      sp->height = 6;
      break;
    case 3:
      sp->width = 10;
      sp->height = 9;
      break;
  }

  sp->nr_polyominoes = 18;
  set_allocate(sp->polyomino,polyomino_type,18*sizeof(polyomino_type));
  random_permutation(18,perm_poly);
  for (p=0;p<18;p++) {
    copy_polyomino(sp->polyomino[p],one_sided_pentomino[perm_poly[p]],1);
  }

  sp->check_ok = check_pentomino_puzzle;

  return 1;
}

/*
Find all the ways of placing all sixty one-sided hexominoes
into a rectangle.
*/

static
int set_one_sided_hexomino_puzzle(polyominoesstruct *sp) {
  int perm_poly[60], perm_point[6], perm_transform[8], i, p;

  make_one_sided_hexomino();

  switch (NRAND(8)) {
    case 0:
      sp->width = 20;
      sp->height = 18;
      break;
    case 1:
      sp->width = 24;
      sp->height = 15;
      break;
    case 2:
      sp->width = 30;
      sp->height = 12;
      break;
    case 3:
      sp->width = 36;
      sp->height = 10;
      break;
    case 4:
      sp->width = 40;
      sp->height = 9;
      break;
    case 5:
      sp->width = 45;
      sp->height = 8;
      break;
    case 6:
      sp->width = 60;
      sp->height = 6;
      break;
    case 7:
      sp->width = 72;
      sp->height = 5;
      break;
  }

  sp->nr_polyominoes = 60;
  set_allocate(sp->polyomino,polyomino_type,60*sizeof(polyomino_type));
  random_permutation(60,perm_poly);
  for (p=0;p<60;p++) {
    copy_polyomino(sp->polyomino[p],one_sided_hexomino[perm_poly[p]],1);
  }

  sp->check_ok = check_hexomino_puzzle;

  return 1;
}

/*
Find all the ways of placing all five tetrominoes and all twelve
pentominoes into a rectangle.
*/

static
int set_tetr_pentomino_puzzle(polyominoesstruct *sp) {
  int perm_poly[17], perm_point[5], perm_transform[8], i, p;

  switch (NRAND(3)) {
    case 0:
      sp->width = 20;
      sp->height = 4;
      break;
    case 1:
      sp->width = 16;
      sp->height = 5;
      break;
    case 2:
      sp->width = 10;
      sp->height = 8;
      break;
  }

  sp->nr_polyominoes = 17;
  set_allocate(sp->polyomino,polyomino_type,17*sizeof(polyomino_type));
  random_permutation(17,perm_poly);
  for (p=0;p<5;p++) {
    copy_polyomino(sp->polyomino[perm_poly[p]],tetromino[p],1);
  }
  for (p=0;p<12;p++) {
    copy_polyomino(sp->polyomino[perm_poly[p+5]],pentomino[p],1);
  }

  sp->check_ok = check_tetr_pentomino_puzzle;

  return 1;
}
/*
Find all the ways of placing all twelve pentominoes and all thirty five
hexominoes into a rectangle whose size is 18x15.
*/

static
int set_pent_hexomino_puzzle(polyominoesstruct *sp) {
  int perm_poly[47], perm_point[6], perm_transform[8], i, p;

  switch (NRAND(5)) {
    case 0:
      sp->width = 54;
      sp->height = 5;
      break;
    case 1:
      sp->width = 45;
      sp->height = 6;
      break;
    case 2:
      sp->width = 30;
      sp->height = 9;
      break;
    case 3:
      sp->width = 27;
      sp->height = 10;
      break;
    case 4:
      sp->width = 18;
      sp->height = 15;
      break;
  }

  sp->nr_polyominoes = 47;
  set_allocate(sp->polyomino,polyomino_type,47*sizeof(polyomino_type));
  random_permutation(47,perm_poly);
  for (p=0;p<12;p++) {
    copy_polyomino(sp->polyomino[perm_poly[p]],pentomino[p],1);
  }
  for (p=0;p<35;p++) {
    copy_polyomino(sp->polyomino[perm_poly[p+12]],hexomino[p],1);
  }

  sp->check_ok = check_pent_hexomino_puzzle;

  return 1;
}

/*
Other puzzles:

Science News September 20, 1986 Vol 130, No 12
Science News November 14, 1987 Vol 132, Pg 310
*/

/*

 *
**** fills a 10x5 rectangle

*/

static struct {int len; point_type point[5]; 
               int transform_len, transform_list[8], max_white;} pentomino1 =
  {5, {{0,0}, {1,0}, {2,0}, {3,0}, {1,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 3};

static
int set_pentomino_puzzle1(polyominoesstruct *sp) {
  int perm_point[5], perm_transform[8], i, p;

  sp->width = 10;
  sp->height =5;

  sp->nr_polyominoes = 10;
  set_allocate(sp->polyomino,polyomino_type,10*sizeof(polyomino_type));
  for (p=0;p<10;p++) {
    copy_polyomino(sp->polyomino[p],pentomino1,1);
  }

  sp->check_ok = check_pentomino_puzzle;

  return 1;
}

/*

 *
***** fills a 24x23 rectangle

*/

static struct {int len; point_type point[6]; 
               int transform_len, transform_list[8], max_white;} hexomino1 =
  {6, {{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {1,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 4};

static
int set_hexomino_puzzle1(polyominoesstruct *sp) {
  int perm_point[6], perm_transform[8], i, p;

  sp->width = 24;
  sp->height =23;

  sp->nr_polyominoes = 92;
  set_allocate(sp->polyomino,polyomino_type,92*sizeof(polyomino_type));
  for (p=0;p<92;p++) {
    copy_polyomino(sp->polyomino[p],hexomino1,1);
  }

  sp->check_ok = check_hexomino_puzzle;

  return 1;
}

/*

 **
***** fills a 21x26 rectangle

(All solutions have 180 degree rotational symmetry)

*/

static struct {int len; point_type point[7]; 
               int transform_len, transform_list[8], max_white;} heptomino1 =
  {7, {{0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {1,1}, {2,1}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 4};

static
int set_heptomino_puzzle1(polyominoesstruct *sp) {
  int perm_point[7], perm_transform[8], i, p;

  sp->rot180 = 1;

  sp->width = 26;
  sp->height =21;

  sp->nr_polyominoes = 78;
  set_allocate(sp->polyomino,polyomino_type,78*sizeof(polyomino_type));
  for (p=0;p<78;p+=2) {
    copy_polyomino(sp->polyomino[p],heptomino1,1);
    copy_polyomino(sp->polyomino[p+1],heptomino1,0);
  }

  sp->check_ok = check_heptomino_puzzle;

  return 1;
}

/* The following puzzles from 
Polyominoes Puzzles, Patterns, Problems, and Packings Revised (2nd) Edition
by Solomon W. Golomb   Princeton University Press 1994
*/

/*

 **
***** fills a 28x19 rectangle

*/
static
int set_heptomino_puzzle2(polyominoesstruct *sp) {
  int perm_point[7], perm_transform[8], i, p;

  sp->width = 28;
  sp->height =19;

  sp->nr_polyominoes = 76;
  set_allocate(sp->polyomino,polyomino_type,76*sizeof(polyomino_type));
  for (p=0;p<76;p++) {
    copy_polyomino(sp->polyomino[p],heptomino1,1);
  }

  sp->check_ok = check_heptomino_puzzle;

  return 1;
}

/*

***
**** fills a 25x22 rectangle
****

*/

static struct {int len; point_type point[11]; 
               int transform_len, transform_list[8], max_white;} elevenomino1 =
  {11, {{0,0}, {1,0}, {2,0}, 
        {0,1}, {1,1}, {2,1}, {3,1},
        {0,2}, {1,2}, {2,2}, {3,2}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 6};

static
int set_elevenomino_puzzle1(polyominoesstruct *sp) {
  int perm_point[11], perm_transform[8], i, p;

  sp->rot180 = 1;

  sp->width = 25;
  sp->height =22;

  sp->nr_polyominoes = 50;
  set_allocate(sp->polyomino,polyomino_type,50*sizeof(polyomino_type));
  for (p=0;p<50;p+=2) {
    copy_polyomino(sp->polyomino[p],elevenomino1,1);
    copy_polyomino(sp->polyomino[p+1],elevenomino1,0);
  }

  sp->check_ok = check_elevenomino_puzzle;

  return 1;
}

/*

 *
 *
**** fills 32 x 30 rectangle
****

*/

static struct {int len; point_type point[10]; 
               int transform_len, transform_list[8], max_white;} dekomino1 =
  {10, {       {1,-1},
               {1,0}, 
        {0,1}, {1,1}, {2,1}, {3,1},
        {0,2}, {1,2}, {2,2}, {3,2}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 5};

static
int set_dekomino_puzzle1(polyominoesstruct *sp) {
  int perm_point[10], perm_transform[8], i, p;

  sp->width = 32;
  sp->height =30;

  sp->nr_polyominoes = 96;
  set_allocate(sp->polyomino,polyomino_type,96*sizeof(polyomino_type));
  for (p=0;p<96;p++) {
    copy_polyomino(sp->polyomino[p],dekomino1,1);
  }

  sp->check_ok = check_dekomino_puzzle;

  return 1;
}

/*

 *
***  fills 96 x 26 rectangle
****

*/

static struct {int len; point_type point[10]; 
               int transform_len, transform_list[8], max_white;} octomino1 =
  {8, {        {1,0}, 
        {0,1}, {1,1}, {2,1},
        {0,2}, {1,2}, {2,2}, {3,2}},
   8, {0, 1, 2, 3, 4, 5, 6, 7}, 5};

static
int set_octomino_puzzle1(polyominoesstruct *sp) {
  int perm_point[8], perm_transform[8], i, p;

  sp->width = 96;
  sp->height =26;

  sp->nr_polyominoes = 312;
  set_allocate(sp->polyomino,polyomino_type,312*sizeof(polyomino_type));
  for (p=0;p<312;p++) {
    copy_polyomino(sp->polyomino[p],octomino1,1);
  }

  sp->check_ok = check_octomino_puzzle;

  return 1;
}

/*

 *   fills 15 x 15 rectangle
****

*/

static
int set_pentomino_puzzle2(polyominoesstruct *sp) {
  int perm_point[5], perm_transform[8], i, p;

  sp->width = 15;
  sp->height =15;

  sp->nr_polyominoes = 45;
  set_allocate(sp->polyomino,polyomino_type,45*sizeof(polyomino_type));
  for (p=0;p<45;p++) {
    copy_polyomino(sp->polyomino[p],pentomino1,1);
  }

  sp->check_ok = check_pentomino_puzzle;

  return 1;
}

/*

***
**** fills a 47x33 rectangle
****

*/

static
int set_elevenomino_puzzle2(polyominoesstruct *sp) {
  int perm_point[11], perm_transform[8], i, p;

  sp->width = 47;
  sp->height =33;

  sp->nr_polyominoes = 141;
  set_allocate(sp->polyomino,polyomino_type,141*sizeof(polyomino_type));
  for (p=0;p<141;p++) {
    copy_polyomino(sp->polyomino[p],elevenomino1,1);
  }

  sp->check_ok = check_elevenomino_puzzle;

  return 1;
}

/**************************************************
The main functions.
**************************************************/

#define allocate(p,type,size) p = (type *) malloc(size); if ((p)==NULL) {free_polyominoes(sp); return;}

void
init_polyominoes(ModeInfo * mi) {
  polyominoesstruct *sp;
  int i,x,y, start;
  int box1, box2;
  int *perm;

  if (polyominoeses == NULL) {
    if ((polyominoeses
         = (polyominoesstruct *) calloc(MI_NUM_SCREENS(mi),sizeof (polyominoesstruct))) 
        == NULL)
      return;
  }
  sp = &polyominoeses[MI_SCREEN(mi)];

  free_polyominoes(sp);

  sp->rot180 = 0;
  sp->counter = 0;

  if (MI_IS_FULLRANDOM(mi)) {
    sp->identical = (Bool) (LRAND() & 1);
    sp->use3D = (Bool) (NRAND(4));
  } else {
    sp->identical = identical; 
    sp->use3D = !plain;
  }
  if (sp->identical) {
    switch (NRAND(9)) {
      case 0:
        if (!set_pentomino_puzzle1(sp))
          return;
        break;
      case 1:
        if (!set_hexomino_puzzle1(sp))
          return;
        break;
      case 2:
        if (!set_heptomino_puzzle1(sp))
          return;
        break;
      case 3:
        if (!set_heptomino_puzzle2(sp))
          return;
        break;
      case 4:
        if (!set_elevenomino_puzzle1(sp))
          return;
        break;
      case 5:
        if (!set_dekomino_puzzle1(sp))
          return;
        break;
      case 6:
        if (!set_octomino_puzzle1(sp))
          return;
        break;
      case 7:
        if (!set_pentomino_puzzle2(sp))
          return;
        break;
      case 8:
        if (!set_elevenomino_puzzle2(sp))
          return;
        break;
    }
  } else {
    switch (NRAND(5)) {
      case 0:
        if (!set_pentomino_puzzle(sp))
          return;
        break;
      case 1:
        if (!set_one_sided_pentomino_puzzle(sp))
          return;
        break;
      case 2:
        if (!set_one_sided_hexomino_puzzle(sp))
          return;
        break;
      case 3:
        if (!set_pent_hexomino_puzzle(sp))
          return;
        break;
      case 4:
        if (!set_tetr_pentomino_puzzle(sp))
          return;
        break;
    }
  }

  allocate(sp->attach_list,int,sp->nr_polyominoes*sizeof(int));
  sp->nr_attached = 0;

  if (sp->identical) {
    allocate(sp->reason_to_not_attach,int,sp->nr_polyominoes*sp->nr_polyominoes*sizeof(int));
  }

  allocate(sp->array,int,sp->width*sp->height*sizeof(int));
  allocate(sp->changed_array,int,sp->width*sp->height*sizeof(int));
  for (x=0;x<sp->width;x++) for (y=0;y<sp->height;y++) ARRAY(x,y) = -1;

  sp->left_right = NRAND(2);
  sp->top_bottom = NRAND(2);

  box1 = MI_WIDTH(mi)/(sp->width+2);
  box2 = MI_HEIGHT(mi)/(sp->height+2);
  if (box1<box2)
    sp->box = box1;
  else
    sp->box = box2;

  if (sp->box >= 12) {
    sp->box = (sp->box/12)*12;
    create_bitmaps(mi,sp);
    if (!sp->use_bitmaps)
      free_bitmaps(sp);
   }
   else
     sp->use_bitmaps = 0;

  if (!sp->use_bitmaps) {
    allocate(sp->rectangles,XRectangle,sp->width*sp->height*sizeof(XRectangle));
    allocate(sp->lines,XSegment,sp->width*sp->height*sizeof(XSegment));
  }

  allocate(perm,int,sp->nr_polyominoes*sizeof(int));
  random_permutation(sp->nr_polyominoes, perm);
  sp->mono = MI_NPIXELS(mi) < 12;
  start = NRAND(MI_NPIXELS(mi));
  for (i=0;i<sp->nr_polyominoes;i++)
    if (!sp->mono) {
      sp->polyomino[i].color = MI_PIXEL(mi,(perm[i]*MI_NPIXELS(mi) / sp->nr_polyominoes + start) % MI_NPIXELS(mi));
      if (sp->rot180) {
         sp->polyomino[i+1].color = sp->polyomino[i].color;
         i++;
      }
    }
    else
      if(sp->use_bitmaps)
        sp->polyomino[i].color = MI_WHITE_PIXEL(mi);
      else
        sp->polyomino[i].color = MI_BLACK_PIXEL(mi);
  free(perm);

  if (sp->use_bitmaps) {
    if (sp->mono)
      sp->border_color = MI_WHITE_PIXEL(mi);
    else
      sp->border_color = MI_PIXEL(mi,NRAND(MI_NPIXELS(mi)));
  }

  sp->x_margin = (MI_WIDTH(mi)-sp->box*sp->width)/2;
  sp->y_margin = (MI_HEIGHT(mi)-sp->box*sp->height)/2;

  sp->wait = 0;

  /* Clear the background. */
  MI_CLEARWINDOW(mi);
  
}

void
draw_polyominoes(ModeInfo * mi) {
  polyominoesstruct *sp;
  int poly_no,point_no,transform_index,done,another_attachment_try;
  point_type attach_point;
  int i,detach_until;

  if (polyominoeses == NULL)
    return;
  sp = &polyominoeses[MI_SCREEN(mi)];

  if (MI_CYCLES(mi) != 0) {
    if (++sp->counter > MI_CYCLES(mi)) {
#ifdef STANDALONE
      erase_full_window(MI_DISPLAY(mi), MI_WINDOW(mi));
#endif /* STANDALONE */
      init_polyominoes(mi);
      return;
    }
  }

  if (sp->box == 0) {
#ifdef STANDALONE
    erase_full_window(MI_DISPLAY(mi), MI_WINDOW(mi));
#endif /* STANDALONE */
    init_polyominoes(mi);
    return;
  }

  MI_IS_DRAWN(mi) = True;
  sp->wait--;
  if (sp->wait>0) return;

  memset(sp->changed_array,0,sp->width*sp->height*sizeof(int));

  poly_no = first_poly_no(sp);
  point_no = 0;
  transform_index = 0;
  done = 0;
  another_attachment_try = 1;
  find_blank(sp,&attach_point);
  if (sp->identical && sp->nr_attached < sp->nr_polyominoes)
    memset(&REASON_TO_NOT_ATTACH(sp->nr_attached,0),0,sp->nr_polyominoes*sizeof(int));
  while(!done) {
    if (sp->nr_attached < sp->nr_polyominoes) {
      while (!done && another_attachment_try) {
        done = attach(sp,poly_no,point_no,transform_index,attach_point,0,&REASON_TO_NOT_ATTACH(sp->nr_attached,0));
        if (done && sp->rot180) {
          poly_no = first_poly_no(sp);
          done = attach(sp,poly_no,point_no,transform_index,attach_point,1,&REASON_TO_NOT_ATTACH(sp->nr_attached-1,0));
          if (!done)
            detach(sp,&poly_no,&point_no,&transform_index,&attach_point,0);
        }
        if (!done)
          another_attachment_try = next_attach_try(sp,&poly_no,&point_no,&transform_index);
      }
    }

    if (sp->identical) {
      if (!done) {
        if (sp->nr_attached == 0)
          done = 1;
        else {
          detach_until=sp->nr_attached-1;
          if (sp->nr_attached < sp->nr_polyominoes)
            while (detach_until>0 && REASON_TO_NOT_ATTACH(sp->nr_attached,detach_until)==0)
              detach_until--;
          while (sp->nr_attached>detach_until) {
            if (sp->rot180)
              detach(sp,&poly_no,&point_no,&transform_index,&attach_point,1);
            detach(sp,&poly_no,&point_no,&transform_index,&attach_point,0);
            if (sp->nr_attached+1+sp->rot180 < sp->nr_polyominoes)
              for (i=0;i<sp->nr_polyominoes;i++)
                REASON_TO_NOT_ATTACH(sp->nr_attached,i) |= REASON_TO_NOT_ATTACH(sp->nr_attached+1+sp->rot180,i);
          }
          another_attachment_try = next_attach_try(sp,&poly_no,&point_no,&transform_index);
        }
      }
    }
    else {
      if (!done) {
        if (sp->nr_attached == 0)
          done = 1;
        else {
          if (sp->rot180)
            detach(sp,&poly_no,&point_no,&transform_index,&attach_point,1);
          detach(sp,&poly_no,&point_no,&transform_index,&attach_point,0);
        }
        another_attachment_try = next_attach_try(sp,&poly_no,&point_no,&transform_index);
      }
    }
  }

  if (sp->use_bitmaps)
    draw_with_bitmaps(mi,sp);
  else
    draw_without_bitmaps(mi,sp);

  if (sp->nr_attached == sp->nr_polyominoes)
    sp->wait = 100;
  else
    sp->wait = 0;
}

void
release_polyominoes(ModeInfo * mi) {
  int screen;
  
  if (polyominoeses != NULL) {
    for (screen=0;screen<MI_NUM_SCREENS(mi); screen++)
      free_polyominoes(&polyominoeses[screen]);
    (void) free((void *) polyominoeses);
    polyominoeses = (polyominoesstruct *) NULL;
  }
}

void
refresh_polyominoes(ModeInfo * mi) {
  MI_CLEARWINDOW(mi);
}

#endif /* MODE_polyominoes */
