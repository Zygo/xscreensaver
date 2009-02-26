/* bubbles.c - frying pan / soft drink in a glass simulation */

/*$Id: bubbles.c,v 1.1 1996/09/08 01:35:40 jwz Exp $*/

/*
 *  Copyright (C) 1995-1996 James Macnicol
 *
 *   This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 *   This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details. 
 */

/*
 * I got my original inspiration for this by looking at the bottom of a 
 * frying pan while something was cooking and watching the little bubbles
 * coming off the bottom of the pan as the oil was boiling joining together
 * to form bigger bubbles and finally to *pop* and disappear.  I had some
 * time on my hands so I wrote this little xscreensaver module to imitate
 * it.  Now that it's done it reminds me more of the bubbles you get in
 * a glass of fizzy soft drink.....
 *
 * The problem seemed to be that the position/size etc. of all the bubbles
 * on the screen had to be remembered and searched through to find when
 * bubbles hit each other and combined.  To do this more efficiently, the
 * window/screen is divided up into a square mesh of side length mesh_length
 * and separate lists of bubbles contained in each cell of the mesh are
 * kept.  Only the cells in the immediate vicinity of the bubble in question
 * are searched.  This should make things more efficient although the whole
 * thing seems to use up too much CPU, but then I'm using an ancient PC so
 * perhaps it's not surprising .
 * (Six months after I wrote the above I now have a Pentium with PCI graphics 
 * and things are _much_ nicer.)
 *
 * Author:           James Macnicol 
 * Internet E-mail : J.Macnicol@student.anu.edu.au
 */

#include "bubbles.h"

#ifdef BUBBLES_IO
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#endif /* BUBBLES_IO */

#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "screenhack.h"
#include "../utils/yarandom.h"

#ifdef HAVE_XPM
#include <X11/xpm.h>
#endif

/* 
 * Public variables 
 */

#ifndef NO_DEFAULT_BUBBLE
extern int num_default_bubbles;
extern char **default_bubbles[];
#endif /* NO_DEFAULT_BUBBLE */

char *progclass = "Bubbles";

char *defaults [] = {
  "*background: black",
  "*foreground: white",
  "*simple:     false",
  "*broken:     false",
  "*delay:      2000",
#ifdef BUBBLES_IO
  "*file:       (default)",
  "*directory:  (default)",
#endif /* BUBBLES_IO */
  "*quiet:      false", 
  "*nodelay:    false",
  "*3D:     false",
  "*geometry:   400x300",
  0
};

XrmOptionDescRec options [] = {
  { "-simple",          ".simple",      XrmoptionNoArg, "true" },
#ifdef HAVE_XPM
  { "-broken",          ".broken",      XrmoptionNoArg, "true" },
#endif /* HAVE_XPM */
  { "-quiet",           ".quiet",       XrmoptionNoArg, "true" },
  { "-nodelay",         ".nodelay",     XrmoptionNoArg, "true" },
  { "-3D",          ".3D",      XrmoptionNoArg, "true" },
#ifdef BUBBLES_IO
  { "-file",            ".file",        XrmoptionSepArg, 0 },
  { "-directory",       ".directory",   XrmoptionSepArg, 0 },
#endif /* BUBBLES_IO */
  { "-delay",           ".delay",       XrmoptionSepArg, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));

/* 
 * Private variables 
 */

static Bubble **mesh;
static int mesh_length;
static int mesh_width;
static int mesh_height;
static int mesh_cells;

static int **adjacent_list;

static int screen_width;
static int screen_height;
static int screen_depth;
static unsigned int default_fg_pixel, default_bg_pixel;
/* 
 * I know it's not elegant to save this stuff in global variables
 * but we need it for the signal handler.
 */
static Display *defdsp;
static Window defwin;
static Colormap defcmap;

/* For simple mode only */
static int bubble_min_radius;
static int bubble_max_radius;
static long *bubble_areas;
static GC draw_gc, erase_gc;

#ifdef HAVE_XPM
static int num_bubble_pixmaps;
static Bubble_Step **step_pixmaps;
#ifdef BUBBLES_IO
static char *pixmap_file;
#endif /* BUBBLES_IO */
static int use_default_bubble;
#endif /* HAVE_XPM */

/* Options stuff */
#ifdef HAVE_XPM
static Bool simple = False;
#else
static Bool simple = True;
#endif
static Bool broken = False;
static Bool quiet = False;
static Bool threed = False;
static int delay;

/* 
 * To prevent forward references, some stuff is up here 
 */

static long
calc_bubble_area(r)
     int r;
/* Calculate the area of a bubble of radius r */
{
#ifdef DEBUG
  printf("%d %g\n", r,
	 10.0 * PI * (double)r * (double)r * (double)r);
#endif /* DEBUG */
  if (threed)
    return (long)(10.0 * PI * (double)r * (double)r * (double)r);
  else
    return (long)(10.0 * PI * (double)r * (double)r);
}

static void *
xmalloc(size)
     size_t size;
/* Safe malloc */
{
  void *ret;

  if ((ret = malloc(size)) == NULL) {
    fprintf(stderr, "%s: out of memory\n", progname);
    exit(1);
  }
  return ret;
}

#ifdef DEBUG
static void 
die_bad_bubble(bb)
     Bubble *bb;
/* This is for use with GDB */
{
  fprintf(stderr, "Bad bubble detected at 0x%x!\n", (int)bb);
  exit(1);
}
#endif

static int
null_bubble(bb)
     Bubble *bb;
/* Returns true if the pointer passed is NULL.  If not then this checks to
see if the bubble is valid (i.e. the (x,y) position is valid and the magic
number is set correctly.  This only a sanity check for debugging and is
turned off if DEBUG isn't set. */
{
  if (bb == (Bubble *)NULL)
    return 1;
#ifdef DEBUG
  if ((bb->cell_index < 0) || (bb->cell_index > mesh_cells)) {
    fprintf(stderr, "cell_index = %d\n", bb->cell_index);
    die_bad_bubble(bb);
  }
  if (bb->magic != BUBBLE_MAGIC)  {
    fprintf(stderr, "Magic = %d\n", bb->magic);
    die_bad_bubble(bb);
  }
  if (simple) {
    if ((bb->x < 0) || (bb->x > screen_width) ||
	(bb->y < 0) || (bb->y > screen_height) ||
	(bb->radius < bubble_min_radius) || (bb->radius >
					     bubble_max_radius)) {
      fprintf(stderr,
	      "radius = %d, x = %d, y = %d, magic = %d, \
cell index = %d\n", bb->radius, bb->x, bb->y, bb->magic, bb->cell_index);
      die_bad_bubble(bb);  
    }
#ifdef HAVE_XPM
  } else {
    if ((bb->x < 0) || (bb->x > screen_width) ||
	(bb->y < 0) || (bb->y > screen_height) ||
	(bb->radius < step_pixmaps[0]->radius) || 
	(bb->radius > step_pixmaps[num_bubble_pixmaps-1]->radius)) {
      fprintf(stderr,
	      "radius = %d, x = %d, y = %d, magic = %d, \
cell index = %d\n", bb->radius, bb->x, bb->y, bb->magic, bb->cell_index);
      die_bad_bubble(bb);  
    }
#endif /* HAVE_XPM */
  }
#endif /* DEBUG */
  return 0;
}

#ifdef DEBUG
static void 
print_bubble_list(bb)
     Bubble *bb;
/* Print list of where all the bubbles are.  For debugging purposes only. */
{
  if (! null_bubble(bb)) {
    printf("  (%d, %d) %d\n", bb->x, bb->y, bb->radius);
    print_bubble_list(bb->next);
  }
}
#endif /* DEBUG */

static void 
add_bubble_to_list(list, bb)
     Bubble **list;
     Bubble *bb;
/* Take a pointer to a list of bubbles and stick bb at the head of the
 list. */
{
  Bubble *head = *list;

  if (null_bubble(head)) {
    bb->prev = (Bubble *)NULL;
    bb->next = (Bubble *)NULL;
  } else {
    bb->next = head;
    bb->prev = (Bubble *)NULL;
    head->prev = bb;
  }
  *list = bb;
}


/* 
 * Mesh stuff 
 */


static void 
init_mesh()
/* Setup the mesh of bubbles */
{
  int i;

  mesh = (Bubble **)xmalloc(mesh_cells * sizeof(Bubble *));
  for (i = 0; i < mesh_cells; i++)
    mesh[i] = (Bubble *)NULL;
}

static int
cell_to_mesh(x, y)
     int x;
     int y;
/* convert cell coordinates to mesh index */
{
#ifdef DEBUG
  if ((x < 0) || (y < 0)) {
    fprintf(stderr, "cell_to_mesh: x = %d, y = %d\n", x, y);
    exit(1);
  }
#endif
  return ((mesh_width * y) + x);
}

static void 
mesh_to_cell(mi, cx, cy)
     int mi;
     int *cx;
     int *cy;
/* convert mesh index into cell coordinates */
{
  *cx = mi % mesh_width;
  *cy = mi / mesh_width;
}

static int
pixel_to_mesh(x, y)
     int x;
     int y;
/* convert screen coordinates into mesh index */
{
  return cell_to_mesh((x / mesh_length), (y / mesh_length));
}

static int
verify_mesh_index(x, y)
     int x;
     int y;
/* check to see if (x,y) is in the mesh */
{
  if ((x < 0) || (y < 0) || (x >= mesh_width) || (y >= mesh_height))
    return (-1);
  return (cell_to_mesh(x, y));
}

#ifdef DEBUG
static void 
print_adjacents(adj)
    int *adj;
/* Print a list of the cells calculated above.  For debugging only. */
{
  int i;

  printf("(");
  for (i = 0; i < 8; i++)
    printf("%d ", adj[i]);
  printf(")\n");
}
#endif /* DEBUG */

static void 
add_to_mesh(bb)
     Bubble *bb;
/* Add the given bubble to the mesh by sticking it on the front of the
list.  bb is already allocated so no need to malloc() anything, just
adjust pointers. */
{
#ifdef DEBUG
  if (null_bubble(bb)) {
    fprintf(stderr, "Bad bubble passed to add_to_mesh()!\n");
    exit(1);
  }
#endif /* DEBUG */

  add_bubble_to_list(&mesh[bb->cell_index], bb);
}

#ifdef DEBUG
static void 
print_mesh()
/* Print the contents of the mesh */
{
  int i;

  for (i = 0; i < mesh_cells; i++) {
    if (! null_bubble(mesh[i])) {
      printf("Mesh cell %d\n", i);
      print_bubble_list(mesh[i]);
    }
  }
}

static void 
valid_mesh()
/* Check to see if the mesh is Okay.  For debugging only. */
{
  int i;
  Bubble *b;

  for (i = 0; i < mesh_cells; i++) {
    b = mesh[i];
    while (! null_bubble(b))
      b = b->next;
  }
}

static int
total_bubbles()
/* Count how many bubbles there are in total.  For debugging only. */
{
  int rv = 0;
  int i;
  Bubble *b;

  for (i = 0; i < mesh_cells; i++) {
    b = mesh[i];
    while (! null_bubble(b)) {
      rv++;
      b = b->next;
    } 
  }

  return rv;
}
#endif /* DEBUG */

static void 
calculate_adjacent_list()
/* Calculate the list of cells adjacent to a particular cell for use
   later. */
{
  int i; 
  int ix, iy;

  adjacent_list = (int **)xmalloc(mesh_cells * sizeof(int *));
  for (i = 0; i < mesh_cells; i++) {
    adjacent_list[i] = (int *)xmalloc(9 * sizeof(int));
    mesh_to_cell(i, &ix, &iy);
    adjacent_list[i][0] = verify_mesh_index(--ix, --iy);
    adjacent_list[i][1] = verify_mesh_index(++ix, iy);
    adjacent_list[i][2] = verify_mesh_index(++ix, iy);
    adjacent_list[i][3] = verify_mesh_index(ix, ++iy);
    adjacent_list[i][4] = verify_mesh_index(ix, ++iy);
    adjacent_list[i][5] = verify_mesh_index(--ix, iy);
    adjacent_list[i][6] = verify_mesh_index(--ix, iy);
    adjacent_list[i][7] = verify_mesh_index(ix, --iy);
    adjacent_list[i][8] = i;
  }
}

static void
adjust_areas()
/* Adjust areas of bubbles so we don't get overflow in weighted_mean() */
{
  double maxvalue;
  long maxarea;
  long factor;
  int i;

#ifdef HAVE_XPM
  if (simple)
    maxarea = bubble_areas[bubble_max_radius+1];
  else
    maxarea = step_pixmaps[num_bubble_pixmaps]->area;
#else
  maxarea = bubble_areas[bubble_max_radius+1];
#endif /* HAVE_XPM */
  maxvalue = (double)screen_width * 2.0 * (double)maxarea;
  factor = (long)ceil(maxvalue / (double)LONG_MAX);
  if (factor > 1) {
    /* Overflow will occur in weighted_mean().  We must divide areas
       each by factor so it will never do so. */
#ifdef HAVE_XPM
    if (simple) {
      for (i = bubble_min_radius; i <= bubble_max_radius+1; i++) {
	bubble_areas[i] /= factor;
	if (bubble_areas[i] == 0)
	  bubble_areas[i] = 1;
      }
    } else {
      for (i = 0; i <= num_bubble_pixmaps; i++) {
#ifdef DEBUG
	printf("area = %ld", step_pixmaps[i]->area);
#endif /* DEBUG */
	step_pixmaps[i]->area /= factor;
	if (step_pixmaps[i]->area == 0)
	  step_pixmaps[i]->area = 1;
#ifdef DEBUG
	printf("-> %ld\n", step_pixmaps[i]->area);
#endif /* DEBUG */
      }
    }
#else
    for (i = bubble_min_radius; i <= bubble_max_radius+1; i++) {
      bubble_areas[i] /= factor;
      if (bubble_areas[i] == 0)
	bubble_areas[i] = 1;
    }
#endif /* HAVE_XPM */
  }
#ifdef DEBUG
  printf("maxarea = %ld\n", maxarea);
  printf("maxvalue = %g\n", maxvalue);
  printf("LONG_MAX = %ld\n", LONG_MAX);
  printf("factor = %ld\n", factor);
#endif /* DEBUG */
}

/* 
 * Bubbles stuff 
 */

static Bubble *
new_bubble()
/* Add a new bubble at some random position on the screen of the smallest
size. */
{
  Bubble *rv = (Bubble *)xmalloc(sizeof(Bubble));

  /* Can't use null_bubble() here since magic number hasn't been set */
  if (rv == (Bubble *)NULL) {
    fprintf(stderr, "Ran out of memory!\n");
    exit(1);
  }

  if (simple) {
    rv->radius = bubble_min_radius;
    rv->area = bubble_areas[bubble_min_radius];
#ifdef HAVE_XPM
  } else {
    rv->step = 0;
    rv->radius = step_pixmaps[0]->radius;
    rv->area = step_pixmaps[0]->area;
#endif /* HAVE_XPM */
  }
  rv->visible = 0;
  rv->magic = BUBBLE_MAGIC;
  rv->x = ya_random() % screen_width;
  rv->y = ya_random() % screen_height;
  rv->cell_index = pixel_to_mesh(rv->x, rv->y);

  return rv;
}

static void 
show_bubble(bb)
     Bubble *bb;
/* paint the bubble on the screen */
{
  if (null_bubble(bb)) {
    fprintf(stderr, "NULL bubble passed to show_bubble\n");
    exit(1);
  }

  if (! bb->visible) {
    bb->visible = 1;

    if (simple) {
      XDrawArc(defdsp, defwin, draw_gc, (bb->x - bb->radius),
	       (bb->y - bb->radius), bb->radius*2, bb->radius*2, 0,
	       360*64);  
    } else {
#ifdef HAVE_XPM
      XSetClipOrigin(defdsp, step_pixmaps[bb->step]->draw_gc, 
		     (bb->x - bb->radius),
		     (bb->y - bb->radius));
      
      XCopyArea(defdsp, step_pixmaps[bb->step]->ball, defwin, 
		step_pixmaps[bb->step]->draw_gc,
		0, 0, (bb->radius * 2), 
		(bb->radius * 2),  
		(bb->x - bb->radius),
		(bb->y - bb->radius));
#endif /* HAVE_XPM */
    }
  }
}

static void 
hide_bubble(bb)
     Bubble *bb;
/* erase the bubble */
{
  if (null_bubble(bb)) {
    fprintf(stderr, "NULL bubble passed to hide_bubble\n");
    exit(1);
  }

  if (bb->visible) {
    bb->visible = 0;

    if (simple) {
      XDrawArc(defdsp, defwin, erase_gc, (bb->x - bb->radius),
	       (bb->y - bb->radius), bb->radius*2, bb->radius*2, 0,
	       360*64);
    } else {
#ifdef HAVE_XPM
      if (! broken) {
	XSetClipOrigin(defdsp, step_pixmaps[bb->step]->erase_gc, 
		       (bb->x - bb->radius), (bb->y - bb->radius));
	
	XFillRectangle(defdsp, defwin, step_pixmaps[bb->step]->erase_gc,
		       (bb->x - bb->radius),
		       (bb->y - bb->radius),
		       (bb->radius * 2),
		       (bb->radius * 2));
      }
#endif /* HAVE_XPM */
    }
  }
}

static void 
delete_bubble_in_mesh(bb, keep_bubble)
     Bubble *bb;
     int keep_bubble;
/* Delete an individual bubble, adjusting list of bubbles around it.
   If keep_bubble is true then the bubble isn't actually deleted.  We
   use this to allow bubbles to change mesh cells without reallocating,
   (it needs this when two bubbles collide and the centre position is
   recalculated, and this may stray over a mesh boundary). */
{
  if ((!null_bubble(bb->prev)) && (!null_bubble(bb->next))) {
    bb->prev->next = bb->next;
    bb->next->prev = bb->prev;
  } else if ((!null_bubble(bb->prev)) &&
	     (null_bubble(bb->next))) {
    bb->prev->next = (Bubble *)NULL;
    bb->next = mesh[bb->cell_index];
  } else if ((null_bubble(bb->prev)) &&
	     (!null_bubble(bb->next))) {
    bb->next->prev = (Bubble *)NULL;
    mesh[bb->cell_index] = bb->next;
    bb->next = mesh[bb->cell_index];
  } else {
    /* Only item on list */
    mesh[bb->cell_index] = (Bubble *)NULL;
  }		 
  if (! keep_bubble)
    free(bb);
}

static unsigned long 
ulongsqrint(x)
     int x;
/* Saves ugly inline code */
{
  return ((unsigned long)x * (unsigned long)x);
}

static Bubble *
get_closest_bubble(bb)
     Bubble *bb;
/* Find the closest bubble touching the this bubble, NULL if none are
   touching. */
{
  Bubble *rv = (Bubble *)NULL;
  Bubble *tmp;
  unsigned long separation2, touchdist2;
  int dx, dy;
  unsigned long closest2 = ULONG_MAX;
  int i;

#ifdef DEBUG 
  if (null_bubble(bb)) {
    fprintf(stderr, "NULL pointer 0x%x passed to get_closest_bubble()!", 
	    (int)bb);
    exit(1);
  }
#endif /* DEBUG */

  for (i = 0; i < 9; i++) {
    /* There is a bug here where bb->cell_index is negaitve.. */
#ifdef DEBUG
    if ((bb->cell_index < 0) || (bb->cell_index >= mesh_cells)) {
      fprintf(stderr, "bb->cell_index = %d\n", bb->cell_index);
      exit(1);
    }
#endif /* DEBUG */
/*    printf("%d,", bb->cell_index); */
    if (adjacent_list[bb->cell_index][i] != -1) {
      tmp = mesh[adjacent_list[bb->cell_index][i]];
      while (! null_bubble(tmp)) {
	if (tmp != bb) {
	  dx = tmp->x - bb->x;
	  dy = tmp->y - bb->y;
	  separation2 = ulongsqrint(dx) + ulongsqrint(dy);
	  /* Add extra leeway so circles _never_ overlap */
	  touchdist2 = ulongsqrint(tmp->radius + bb->radius + 2);
	  if ((separation2 <= touchdist2) && (separation2 <
					      closest2)) {
	    rv = tmp;
	    closest2 = separation2;
	  }
	}
	tmp = tmp->next;
      }
    }
  }

  return rv;
}

#ifdef DEBUG
static void
ldr_barf()
{
}
#endif /* DEBUG */

static long
long_div_round(num, dem)
     long num;
     long dem;
{
  long divvie, moddo;

#ifdef DEBUG
  if ((num < 0) || (dem < 0)) {
    fprintf(stderr, "long_div_round: %ld, %ld\n", num, dem);
    ldr_barf();
    exit(1);
  }
#endif /* DEBUG */

  divvie = num / dem;
  moddo = num % dem;
  if (moddo > (dem / 2))
    ++divvie;

#ifdef DEBUG
  if ((divvie < 0) || (moddo < 0)) {
    fprintf(stderr, "long_div_round: %ld, %ld\n", divvie, moddo);
    ldr_barf();
    exit(1);
  }
#endif /* DEBUG */

  return divvie;
}

static int
weighted_mean(n1, n2, w1, w2)
     int n1;
     int n2;
     long w1;
     long w2;
/* Mean of n1 and n2 respectively weighted by weights w1 and w2. */
{
#ifdef DEBUG
  if ((w1 <= 0) || (w2 <= 0)) {
    fprintf(stderr, "Bad weights passed to weighted_mean() - \
(%d, %d, %ld, %ld)!\n", n1, n2, w1, w2);
    exit(1);
  }
#endif /* DEBUG */
  return ((int)long_div_round((long)n1 * w1 + (long)n2 * w2,
                           w1 + w2));
}

static int
bubble_eat(diner, food)
     Bubble *diner;
     Bubble *food;
/* The diner eats the food.  Returns true (1) if the diner still exists */
{ 
  int i;
  int newmi;

#ifdef DEBUG
  if ((null_bubble(diner)) || (null_bubble(food))) {
    fprintf(stderr, "Bad bubbles passed to bubble_eat()!\n");
    exit(1);
  }
#endif /* DEBUG */

  /* We hide the diner even in the case that it doesn't grow so that
     if the food overlaps its boundary it is replaced. This could
     probably be solved by letting bubbles eat others which are close
     but not quite touching.  It's probably worth it, too, since we
     would then not have to redraw bubbles which don't change in
     size. */

  hide_bubble(diner);
  hide_bubble(food);
  diner->x = weighted_mean(diner->x, food->x, diner->area, food->area);
  diner->y = weighted_mean(diner->y, food->y, diner->area, food->area);
  newmi = pixel_to_mesh(diner->x, diner->y);
  diner->area += food->area;
  delete_bubble_in_mesh(food, DELETE_BUBBLE);

  if ((simple) && (diner->area > bubble_areas[bubble_max_radius])) {
    delete_bubble_in_mesh(diner, DELETE_BUBBLE);
    return 0;
  }
#ifdef HAVE_XPM
  if ((! simple) && (diner->area > 
		     step_pixmaps[num_bubble_pixmaps]->area)) {
    delete_bubble_in_mesh(diner, DELETE_BUBBLE);
    return 0;
  }
#endif /* HAVE_XPM */

  if (simple) {
    if (diner->area > bubble_areas[diner->radius + 1]) {
      /* Move the bubble to a new radius */
      i = diner->radius;
      while (diner->area > bubble_areas[i+1])
	++i;
      diner->radius = i;
    }
    show_bubble(diner);
#ifdef HAVE_XPM
  } else {
    if (diner->area > step_pixmaps[diner->step+1]->area) {
      i = diner->step;
      while (diner->area > step_pixmaps[i+1]->area)
	++i;
      diner->step = i;
      diner->radius = step_pixmaps[diner->step]->radius;
    }
    show_bubble(diner);
#endif /* HAVE_XPM */
  }

  /* Now adjust locations and cells if need be */
  if (newmi != diner->cell_index) {
    delete_bubble_in_mesh(diner, KEEP_BUBBLE);
    diner->cell_index = newmi;
    add_to_mesh(diner);
  }

  return 1;
}

static int
merge_bubbles(b1, b2)
     Bubble *b1;
     Bubble *b2;
/* These two bubbles merge into one.  If the first one wins out return
1 else return 2.  If there is no winner (it explodes) then return 0 */
{
  int b1size, b2size;

  b1size = b1->area;
  b2size = b2->area;

#ifdef DEBUG
  if ((null_bubble(b1) || null_bubble(b2))) {
    fprintf(stderr, "NULL bubble passed to merge_bubbles()!\n");
    exit(1);
  }
#endif /* DEBUG */

  if (b1 == b2) {
    hide_bubble(b1);
    delete_bubble_in_mesh(b1, DELETE_BUBBLE);
    return 0;
  }

  if (b1size > b2size) {
    switch (bubble_eat(b1, b2)) {
    case 0:
      return 0;
      break;
    case 1:
      return 1;
      break;
    default:
      break;
    }
  } else if (b1size < b2size) {
    switch (bubble_eat(b2, b1)) {
    case 0:
      return 0;
      break;
    case 1:
      return 2;
      break;
    default:
      break;
    }
  } else {
    if ((ya_random() % 2) == 0) {
      switch (bubble_eat(b1, b2)) {
      case 0:
	return 0;
	break;
      case 1:
	return 1;
	break;
      default:
	break;
      }
    } else {
      switch (bubble_eat(b2, b1)) {
      case 0:
	return 0;
	break;
      case 1:
	return 2;
	break;
      default:
	break;
      }
    }
  }
  fprintf(stderr, "An error occurred in merge_bubbles()\n");
  exit(1);
}

static void 
insert_new_bubble(tmp)
     Bubble *tmp;
/* Calculates which bubbles are eaten when a new bubble tmp is
   inserted.  This is called recursively in case when a bubble grows
   it eats others.  Careful to pick out disappearing bubbles. */
{
  Bubble *nextbub;
  Bubble *touch;

#ifdef DEBUG
  if (null_bubble(tmp)) {
    fprintf(stderr, "Bad bubble passed to insert_new_bubble()!\n");
    exit(1);
  }
#endif /* DEBUG */
  
  nextbub = tmp;
  touch = get_closest_bubble(nextbub);
  while (! null_bubble(touch)) {
    switch (merge_bubbles(nextbub, touch)) {
    case 2:
      /* touch ate nextbub and survived */
      nextbub = touch;
      break;
    case 1:
      /* nextbub ate touch and survived */
      break;
    case 0:
      /* somebody ate someone else but they exploded */
      nextbub = (Bubble *)NULL;
      break;
    default:
      /* something went wrong */
      fprintf(stderr, "Error occurred in insert_new_bubble()\n");
      exit(1);
    }
    /* Check to see if there are any other bubbles still in the area
       and if we need to do this all over again for them. */
    if (! null_bubble(nextbub))
      touch = get_closest_bubble(nextbub);
    else
      touch = (Bubble *)NULL;
  }
}

#ifdef DEBUG
static int
get_length_of_bubble_list(bb)
     Bubble *bb;
{
  Bubble *tmp = bb;
  int rv = 0;

  while (! null_bubble(tmp)) {
    rv++;
    tmp = tmp->next;
  }

  return rv;
}
#endif /* DEBUG */

/*
 * Pixmap stuff used regardless of whether file I/O is available.  Must
 * still check for XPM, though!
 */

#ifdef HAVE_XPM

static void 
free_pixmaps()
/* Free resources associated with XPM */
{
  int i;

#ifdef DEBUG
  if (simple) {
    fprintf(stderr, "free_pixmaps() called in simple mode\n");
    exit(1);
  }
  printf("free_pixmaps()\n");
#endif /* DEBUG */

  for(i = 0; i < (num_bubble_pixmaps - 1); i++) {
    XFreePixmap(defdsp, step_pixmaps[i]->ball);
    XFreePixmap(defdsp, step_pixmaps[i]->shape_mask);
    XFreeGC(defdsp, step_pixmaps[i]->draw_gc);
    XFreeGC(defdsp, step_pixmaps[i]->erase_gc);
    XFreeColors(defdsp, defcmap, step_pixmaps[i]->xpmattrs.pixels, 
		step_pixmaps[i]->xpmattrs.npixels, 0);
    XpmFreeAttributes(&step_pixmaps[i]->xpmattrs);
  }
}

static void 
onintr(a)
     int a;
/* This gets called when SIGINT or SIGTERM is received */
{
  free_pixmaps();
  exit(0);
}

#ifdef DEBUG
static void
onsegv(a)
     int a;
/* Called when SEGV detected.   Hmmmmm.... */
{
  fflush(stdout);
  fprintf(stderr, "SEGV detected! : %d\n", a);
  exit(1);
}
#endif /* DEBUG */


/*
 * Pixmaps without file I/O (but do have XPM)
 */

static void 
pixmap_sort(head, numelems)
     Bubble_Step **head;
     int numelems;
/* Couldn't get qsort to work right with this so I wrote my own.  This puts
the numelems length array with first element at head into order of radius.
*/
{
  Bubble_Step tmp;
  Bubble_Step *least = 0;
  int minradius = INT_MAX;
  int i;

  for (i = 0; i < numelems; i++) {
    if (head[i]->radius < minradius) {
      least = head[i];
      minradius = head[i]->radius;
    }
  }
  if (*head != least) {
    memcpy(&tmp, least, sizeof(Bubble_Step));
    memcpy(least, *head, sizeof(Bubble_Step));
    memcpy(*head, &tmp, sizeof(Bubble_Step));
  }

  if (numelems > 2)
    pixmap_sort(&head[1], numelems-1);
}

static int
extrapolate(i1, i2)
     int i1;
     int i2;
{
  return (i2 + (i2 - i1));
}

static void 
make_pixmap_array(list)
     Bubble_Step *list;
/* From a linked list of bubbles construct the array step_pixmaps */
{
  Bubble_Step *tmp = list;
  int ind;
#ifdef DEBUG
  int prevrad = -1;
#endif
  
  if (list == (Bubble_Step *)NULL) {
    fprintf(stderr, "NULL list passed to make_pixmap_array\n");
    exit(1);
  }

  num_bubble_pixmaps = 1;
  while(tmp->next != (Bubble_Step *)NULL) {
    tmp = tmp->next;
    ++num_bubble_pixmaps;
  }

  if (num_bubble_pixmaps < 2) {
    fprintf(stderr, "Must be at least two bubbles in file\n");
    exit(1);
  }

  step_pixmaps = (Bubble_Step **)xmalloc((num_bubble_pixmaps + 1) * 
					 sizeof(Bubble_Step *));

  /* Copy them blindly into the array for sorting. */
  ind = 0;
  tmp = list;
  do {
    step_pixmaps[ind++] = tmp;
    tmp = tmp->next;
  } while(tmp != (Bubble_Step *)NULL);

  /* We make another bubble beyond the ones with pixmaps so that the final
     bubble hangs around and doesn't pop immediately.  It's radius and area
     are found by extrapolating from the largest two bubbles with pixmaps. */

  step_pixmaps[num_bubble_pixmaps] = 
    (Bubble_Step *)xmalloc(sizeof(Bubble_Step));
  step_pixmaps[num_bubble_pixmaps]->radius = INT_MAX;

  pixmap_sort(step_pixmaps, (num_bubble_pixmaps + 1));

#ifdef DEBUG
  if (step_pixmaps[num_bubble_pixmaps]->radius != INT_MAX) {
    fprintf(stderr, "pixmap_sort() screwed up make_pixmap_array\n");
  }
#endif /* DEBUG */

  step_pixmaps[num_bubble_pixmaps]->radius = 
    extrapolate(step_pixmaps[num_bubble_pixmaps-2]->radius,
		step_pixmaps[num_bubble_pixmaps-1]->radius);
  step_pixmaps[num_bubble_pixmaps]->area = 
    calc_bubble_area(step_pixmaps[num_bubble_pixmaps]->radius);
  

#ifdef DEBUG
  /* Now check for correct order */
  for (ind = 0; ind < num_bubble_pixmaps; ind++) {
    if (prevrad > 0) {
      if (step_pixmaps[ind]->radius < prevrad) {
	fprintf(stderr, "Pixmaps not in ascending order of radius\n");
	exit(1);
      }
    }
    prevrad = step_pixmaps[ind]->radius;
  }
#endif /* DEBUG */
}

#ifndef NO_DEFAULT_BUBBLE
static void
make_pixmap_from_default(pixmap_data, bl)
     char **pixmap_data;
     Bubble_Step *bl;
/* Read pixmap data which has been compiled into the program and a pointer
 to which has been passed. 

 This is virtually copied verbatim from make_pixmap_from_file() above and
changes made to either should be propagated onwards! */
{
  int result;
  XGCValues gcv;

#ifdef DEBUG
  if (pixmap_data == (char **)0) {
    fprintf(stderr, "make_pixmap_from_default(): NULL passed\n");
    exit(1);
  }
#endif

  if (bl == (Bubble_Step *)NULL) {
    fprintf(stderr, "NULL pointer passed to make_pixmap()\n");
    exit(1);
  }

  bl->xpmattrs.closeness = 40000;
  bl->xpmattrs.valuemask = XpmColormap | XpmCloseness;
  bl->xpmattrs.colormap = defcmap;

  /* This is the only line which is different from make_pixmap_from_file() */
  result = XpmCreatePixmapFromData(defdsp, defwin, pixmap_data, &bl->ball, 
				   &bl->shape_mask, &bl->xpmattrs);

  switch(result) {
  case XpmColorError:
    fprintf(stderr, "xpm: color substitution performed\n");
    /* fall through */
  case XpmSuccess:
    bl->radius = MAX(bl->xpmattrs.width, bl->xpmattrs.height) / 2;
    bl->area = calc_bubble_area(bl->radius);
    break;
  case XpmColorFailed:
    fprintf(stderr, "xpm: color allocation failed\n");
    exit(1);
  case XpmNoMemory:
    fprintf(stderr, "xpm: out of memory\n");
    exit(1);
  default:
    fprintf(stderr, "xpm: unknown error code %d\n", result);
    exit(1);
  }
  
  gcv.plane_mask = AllPlanes;
  gcv.foreground = default_fg_pixel;
  gcv.function = GXcopy;
  bl->draw_gc = XCreateGC (defdsp, defwin, GCForeground, &gcv);
  XSetClipMask(defdsp, bl->draw_gc, bl->shape_mask);
  
  gcv.foreground = default_bg_pixel;
  gcv.function = GXcopy;
  bl->erase_gc = XCreateGC (defdsp, defwin, GCForeground, &gcv);
  XSetClipMask(defdsp, bl->erase_gc, bl->shape_mask);  
}

static void 
default_to_pixmaps(void)
/* Make pixmaps out of default ball data stored in bubbles_default.c */
{
  int i;
  Bubble_Step *pixmap_list = (Bubble_Step *)NULL;
  Bubble_Step *newpix, *tmppix;
  char **pixpt;

  /* Make sure pixmaps are freed when program is terminated */
  /* This is when I hit ^C */
  if (signal(SIGINT, SIG_IGN) != SIG_IGN)
    signal(SIGINT, onintr);
  /* xscreensaver sends SIGTERM */
  if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
    signal(SIGTERM, onintr);
#ifdef DEBUG
  if (signal(SIGSEGV, SIG_IGN) != SIG_IGN) {
    printf("Setting signal handler for SIGSEGV\n");
    signal(SIGSEGV, onsegv);
  } else {
    printf("Didn't set signal hanlder for SIGSEGV\n");
  }
#endif /* DEBUG */

  for (i = 0; i < num_default_bubbles; i++) {
    pixpt = default_bubbles[i];
    newpix = (Bubble_Step *)xmalloc(sizeof(Bubble_Step));
    make_pixmap_from_default(pixpt, newpix);
    /* Now add to list */
    if (pixmap_list == (Bubble_Step *)NULL) {
      pixmap_list = newpix;
    } else {
      tmppix = pixmap_list;
      while (tmppix->next != (Bubble_Step *)NULL)
	tmppix = tmppix->next;
      tmppix->next = newpix;
    }
    newpix->next = (Bubble_Step *)NULL;
  }
  
  /* Finally construct step_pixmaps[] */
  make_pixmap_array(pixmap_list);
}

#endif /* NO_DEFAULT_BUBBLE */

#endif /* HAVE_XPM */

/* 
 * File I/O stuff
 */

#ifdef BUBBLES_IO

static DIR *
my_opendir(name)
     char *name;
/* Like opendir() but checks for things so we don't have to do it multiple
times in the code. */
{
  DIR *rv;

  if (name == (char *)NULL) {
    fprintf(stderr, "NULL directory name\n");
    return (DIR *)NULL;
  }
  
  if ((rv = opendir(name)) == NULL) {
    perror(name);
    return (DIR *)NULL;
  }

  return rv;
}

static int
regular_file(name)
     char *name;
/* Check to see if we can use the named file.  This was broken under Linux
1.3.45 but seems to be okay under 1.3.54.  The parameter "name" was being
trashed if the file didn't exist.  Yeah, I know 1.3.x are development
kernels....
*/
{
  int fd;

  if ((fd = open(name, O_RDONLY)) == -1) {
    perror(name);
    return 0;
  } else {
    close(fd);
    return 1;
  }
}

static char *
get_random_name(dir)
     char *dir;
/* Pick an appropriate file at random out of the files in the directory dir */
{
  STRUCT_DIRENT *dp;
  DIR *dfd;
  int numentries = 0;
  int entnum;
  int x;
  char buf[PATH_BUF_SIZE];
  char *rv;

  if ((dfd = my_opendir(dir)) == (DIR *)NULL)
    return (char *)NULL;

  while ((dp = readdir(dfd)) != NULL) {
    if ((strcmp(DIRENT_NAME, ".") == 0) || (strcmp(DIRENT_NAME, "..") == 0))
      continue;
    if ((strlen(dir)+strlen(DIRENT_NAME)+2) > 1024) {
      fprintf(stderr, "name %s/%s too long\n", dir, DIRENT_NAME);
      continue;
    }
    if (sprintf(buf, "%s/%s", dir, DIRENT_NAME) > (PATH_BUF_SIZE-1)) {
      fprintf(stderr, "path buffer overflowed in get_random_name()\n");
      continue;
    }
    if (regular_file(buf))
      ++numentries;
  }
  closedir(dfd);
  if (numentries == 0) {
    fprintf(stderr, "No suitable files found in %s\n", dir);
    return (char *)NULL;
  }
  entnum = ya_random() % numentries;
  x = 0;

  if ((dfd = my_opendir(dir)) == (DIR *)NULL)
    return (char *)NULL;
  while ((dp = readdir(dfd)) != NULL) {
    if ((strcmp(DIRENT_NAME, ".") == 0) || (strcmp(DIRENT_NAME, "..") == 0))
      continue;
    if ((strlen(dir)+strlen(DIRENT_NAME)+2) > 1024) {
      /* We warned about this previously */
      continue;
    }
    if (sprintf(buf, "%s/%s", dir, DIRENT_NAME) > (PATH_BUF_SIZE-1)) {
      fprintf(stderr, "path buffer overflowed in get_random_name()\n");
      continue;
    }
    if (regular_file(buf)) {
      if (x == entnum) {
	rv = (char *)xmalloc(1024 * sizeof(char));
	strcpy(rv, buf);
	closedir(dfd);
	return rv;
      }
      ++x;
    }
  }
  /* We've screwed up if we reach here - someone must have deleted all the
     files while we were counting them... */
  fprintf(stderr, "get_random_name(): Oops!\n");
  exit(1);
}

static int
read_line(fd, buf, bufsize)
     int fd;
     char **buf;
     int bufsize;
/* A line is read from fd until a '\n' is found or EOF is reached.  (*buf)
is initially of length bufsize and is extended by bufsize chars if need
be (for as many times as it takes). */
{
  char x;
  int pos = 0;
  int size = bufsize;
  int rv;
  char *newbuf;

  while (1) {
    rv = read(fd, &x, 1);
    if (rv == -1) {
      perror("read_line(): ");
      return IO_ERROR;
    } else if (rv == 0) {
      (*buf)[pos] = '\0';
      return EOF_REACHED;
    } else if (x == '\n') {
      (*buf)[pos] = '\0';
      return LINE_READ;
    } else {
      (*buf)[pos++] = x;
      if (pos == (size - 1)) {
	/* We've come to the end of the space */
	newbuf = (char *)xmalloc((size+bufsize) * sizeof(char));
	strncpy(newbuf, *buf, (size - 1));
	free(*buf);
	*buf = newbuf;
	size += bufsize;
      }
    }
  }
}

static int
create_temp_file(name)
     char **name;
/* Create a temporary file in /tmp and return a filedescriptor to it */
{
  int rv;
 
  if (*name != (char *)NULL)
    free(*name);

  if ((*name = tempnam("/tmp", "abxdfes")) == (char *)NULL) {
    fprintf(stderr, "Couldn't make new temporary file\n");
    exit(1);
  }
/*   printf("Temp file created : %s\n", *name); */
  if ((rv = creat(*name, 0644)) == -1) {
    fprintf(stderr, "Couldn't open temporary file\n");
    exit(1);
  }
  
  return rv;
}


#ifdef BUBBLES_IO
static void 
make_pixmap_from_file(fname, bl)
     char *fname;
     Bubble_Step *bl;
/* Read the pixmap in file fname into structure bl which must already
 be allocated. */
{
  int result;
  XGCValues gcv;

  if (bl == (Bubble_Step *)NULL) {
    fprintf(stderr, "NULL pointer passed to make_pixmap()\n");
    exit(1);
  }

  bl->xpmattrs.closeness = 40000;
  bl->xpmattrs.valuemask = XpmColormap | XpmCloseness;
  bl->xpmattrs.colormap = defcmap;

  result = XpmReadFileToPixmap(defdsp, defwin, fname, &bl->ball, 
			       &bl->shape_mask, &bl->xpmattrs);

  switch(result) {
  case XpmColorError:
    fprintf(stderr, "xpm: color substitution performed\n");
    /* fall through */
  case XpmSuccess:
    bl->radius = MAX(bl->xpmattrs.width, bl->xpmattrs.height) / 2;
    bl->area = calc_bubble_area(bl->radius);
    break;
  case XpmColorFailed:
    fprintf(stderr, "xpm: color allocation failed\n");
    exit(1);
  case XpmNoMemory:
    fprintf(stderr, "xpm: out of memory\n");
    exit(1);
  default:
    fprintf(stderr, "xpm: unknown error code %d\n", result);
    exit(1);
  }
  
  gcv.plane_mask = AllPlanes;
  gcv.foreground = default_fg_pixel;
  gcv.function = GXcopy;
  bl->draw_gc = XCreateGC (defdsp, defwin, GCForeground, &gcv);
  XSetClipMask(defdsp, bl->draw_gc, bl->shape_mask);
  
  gcv.foreground = default_bg_pixel;
  gcv.function = GXcopy;
  bl->erase_gc = XCreateGC (defdsp, defwin, GCForeground, &gcv);
  XSetClipMask(defdsp, bl->erase_gc, bl->shape_mask);  
}
#endif /* BUBBLES_IO */

static void 
read_file_to_pixmaps(fname)
     char *fname;
/* Read the pixmaps contained in the file fname into memory.  THESE SHOULD
BE UNCOMPRESSED AND READY TO GO! */
{
  int fd, tmpfd=0, rv;
  int inxpm = 0;
  int xpmseen = 0;
  char *buf = (char *)NULL;
  char *tmpname = (char *)NULL;
  Bubble_Step *pixmap_list = (Bubble_Step *)NULL;
  Bubble_Step *newpix, *tmppix;

  /* We first create a linked list of pixmaps before allocating
     memory for the array */

  if ((fd = open(fname, O_RDONLY)) == -1) {
    fprintf(stderr, "Couldn't open %s\n", fname);
    exit(1);
  }

  /* Make sure pixmaps are freed when program is terminated */
  /* This is when I hit ^C */
  if (signal(SIGINT, SIG_IGN) != SIG_IGN)
    signal(SIGINT, onintr);
  /* xscreensaver sends SIGTERM */
  if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
    signal(SIGTERM, onintr);
#ifdef DEBUG
  if (signal(SIGSEGV, SIGN_IGN) != SIG_IGN)
    signal(SIGSEGV, onsegv);
#endif /* DEBUG */
  
  while (1) {
    if (inxpm == 2)
      break;

    buf = (char *)malloc(READ_LINE_BUF_SIZE * sizeof(char));

    switch ((rv = read_line(fd, &buf, READ_LINE_BUF_SIZE))) {
    case IO_ERROR:
      fprintf(stderr, "An I/O error occurred\n");
      exit(1);
    case EOF_REACHED:
      if (inxpm) {
	fprintf(stderr, "EOF occurred inside an XPM block\n");
	exit(1);
      } else
	inxpm = 2;
      break;
    case LINE_READ:
      if (inxpm) {
	if (strncmp("};", buf, 2) == 0) {
	  inxpm = 0;
	  write(tmpfd, buf, strlen(buf));
	  write(tmpfd, "\n", 1);
	  close(tmpfd);
	  /* Now process the tmpfile */
	  newpix = (Bubble_Step *)xmalloc(sizeof(Bubble_Step));
	  make_pixmap_from_file(tmpname, newpix);
	  /* Now add to list */
	  if (pixmap_list == (Bubble_Step *)NULL) {
	    pixmap_list = newpix;
	  } else {
	    tmppix = pixmap_list;
	    while (tmppix->next != (Bubble_Step *)NULL)
	      tmppix = tmppix->next;
	    tmppix->next = newpix;
	  }
	  newpix->next = (Bubble_Step *)NULL;
	  unlink(tmpname);
	} else {
	  write(tmpfd, buf, strlen(buf));
	  write(tmpfd, "\n", 1);
	}
      } else {
	if (strncmp("/* XPM */", buf, 9) == 0) {
	  tmpfd = create_temp_file(&tmpname);
/* This proves XPM's performance is kinda pathetic */
#ifdef DEBUG
 	  printf("New XPM detected : %s, fd=%d\n", tmpname, tmpfd); 
#endif /* DEBUG */
	  inxpm = 1;
	  xpmseen = 1;
	}
	write(tmpfd, buf, strlen(buf));
	write(tmpfd, "\n", 1);
      }
      break;
    default:
      fprintf(stderr, "read_line returned unknown code %d\n", rv);
      exit(1);
    }

    free(buf);
  }

  close(fd);
  if (buf != (char *)NULL)
    free(buf);
  if (tmpname != (char *)NULL)
    free(tmpname);

  if (! xpmseen) {
    fprintf(stderr, "There was no XPM data in the file %s\n", fname);
    exit(1);
  }

  /* Finally construct step_pixmaps[] */
  make_pixmap_array(pixmap_list);
}

static void 
shell_exec(command)
     char *command;
/* Forks a shell to execute "command" then waits for command to finish */
{
  int pid, status, wval;

  switch(pid=fork()) {
  case 0:
    if (execlp(BOURNESH, BOURNESH, "-c", command, (char *)NULL) == -1) {
      fprintf(stderr, "Couldn't exec shell %s\n", BOURNESH);
      exit(1);
    }
    /* fall through if execlp() fails */
  case -1:
    /* Couldn't fork */
    perror(progname);
    exit(1);
  default:
    while ((wval = wait(&status)) != pid)
      if (wval == -1) {
	perror(progname);
	exit(1);
      }    
  }
}

static void 
uncompress_file(current, namebuf)
     char *current;
     char *namebuf;
/* If the file current is compressed (i.e. its name ends in .gz or .Z,
no check is made to see if it is actually a compressed file...) then a
new temporary file is created for it and it is decompressed into there,
returning the name of the file to namebuf, else current is returned in
namebuf */
{
  int fd;
  char *tname = (char *)NULL;
  char argbuf[COMMAND_BUF_SIZE];

  if (((strlen(current) >=4) && 
       (strncmp(&current[strlen(current)-3], ".gz", 3) == 0)) || 
      ((strlen(current) >=3) && 
       (strncmp(&current[strlen(current)-2], ".Z", 2) == 0))) {
    fd = create_temp_file(&tname);
    /* close immediately but don't unlink so we should have a zero length
       file in /tmp which we can append to */
    close(fd);
    if (sprintf(argbuf, "%s -dc %s > %s", GZIP, current, tname) > 
	(COMMAND_BUF_SIZE-1)) {
      fprintf(stderr, "command buffer overflowed in uncompress_file()\n");
      exit(1);
    }
    shell_exec(argbuf);
    strcpy(namebuf, tname);
  } else {
    strcpy(namebuf, current);
  }
  return;
}

#endif /* BUBBLES_IO */

/* 
 * Main stuff 
 */


static void 
get_resources(dpy)
     Display *dpy;
/* Get the appropriate X resources and warn about any inconsistencies. */
{
  Bool nodelay;
#ifdef BUBBLES_IO
#ifdef HAVE_XPM
  char *dirname;
#else
  char *foo, *bar;
#endif /* HAVE_XPM */
#endif /* BUBBLES_IO */

  threed = get_boolean_resource("3D", "Boolean");
  quiet = get_boolean_resource("quiet", "Boolean");
  simple = get_boolean_resource("simple", "Boolean");
  /* Forbid rendered bubbles on monochrome displays */
  if ((mono_p) && (! simple)) {
    if (! quiet)
      fprintf(stderr, "Rendered bubbles not supported on monochrome \
displays\n");
    simple = True;
  }
  delay = get_integer_resource("delay", "Integer");
  nodelay = get_boolean_resource("nodelay", "Boolean");
  if (nodelay)
    delay = 0;
  if (delay < 0)
    delay = 0;

  default_fg_pixel = get_pixel_resource ("foreground", "Foreground", dpy,
					 DefaultColormap(dpy, 
							 DefaultScreen(dpy)));
  default_bg_pixel = get_pixel_resource ("background", "Background", dpy,
					 DefaultColormap(dpy, 
							 DefaultScreen(dpy)));

  if (simple) {
    /* This is easy */
    broken = get_boolean_resource("broken", "Boolean");
    if (broken)
      if (! quiet)
	fprintf(stderr, "-broken not available in simple mode\n");
  } else {
#ifndef HAVE_XPM
    simple = 1;
#else
    broken = get_boolean_resource("broken", "Boolean");
#ifdef BUBBLES_IO
    pixmap_file = get_string_resource("file", "File");
    dirname = get_string_resource("directory", "Directory");    
#ifdef NO_DEFAULT_BUBBLE
    /* Must specify -file or -directory if no default bubble compiled in */
    if (strcmp(pixmap_file, "(default)") != 0) {
    } else if (strcmp(dirname, "(default)") != 0) {
      if ((pixmap_file = get_random_name(dirname)) == (char *)NULL) {
	/* Die if we can't open directory - make it consistent with -file
	   when it fails, rather than falling back to default. */
	exit(1);
      }
    } else {
      fprintf(stderr, "No default bubble compiled in - use -file or \
-directory\n");
      exit(1);
    }
#else
    if (strcmp(pixmap_file, "(default)") != 0) {
    } else if (strcmp(dirname, "(default)") != 0) {
      if ((pixmap_file = get_random_name(dirname)) == (char *)NULL) {
	exit(1);
      }
    } else {
      /* Use default bubble */
      use_default_bubble = 1;
    }
#endif /* NO_DEFAULT_BUBBLE */
#else 
    use_default_bubble = 1;
#endif /* BUBBLES_IO */
#endif /* HAVE_XPM */
  }
}

static void
init_bubbles (dpy, window)
     Display *dpy;
     Window window;
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  int i;
#ifdef BUBBLES_IO
  char uncompressed[1024];
#endif /* BUBBLES_IO */

  defdsp = dpy;
  defwin = window;

  ya_rand_init(0);

  get_resources(dpy);

  XGetWindowAttributes (dpy, window, &xgwa);

#ifdef DEBUG
  printf("sizof(int) on this platform is %d\n", sizeof(int));
  printf("sizof(long) on this platform is %d\n", sizeof(long));
#endif /* DEBUG */

  screen_width = xgwa.width;
  screen_height = xgwa.height;
  screen_depth = xgwa.depth;
  defcmap = xgwa.colormap;

  if (simple) {
    /* These are pretty much plucked out of the air */
    bubble_min_radius = (int)(0.006*(double)(MIN(screen_width, 
						 screen_height)));
    bubble_max_radius = (int)(0.045*(double)(MIN(screen_width,
						 screen_height)));
    /* Some trivial values */
    if (bubble_min_radius < 1)
      bubble_min_radius = 1;
    if (bubble_max_radius <= bubble_min_radius)
      bubble_max_radius = bubble_min_radius + 1;

    mesh_length = (2 * bubble_max_radius) + 3;

    /* store area of each bubble of certain radius as number of 1/10s of
       a pixel area.  PI is defined in <math.h> */
    bubble_areas = (long *)xmalloc((bubble_max_radius + 2) * sizeof(int));
    for (i = 0; i < bubble_min_radius; i++)
      bubble_areas[i] = 0;
    for (i = bubble_min_radius; i <= (bubble_max_radius+1); i++)
      bubble_areas[i] = calc_bubble_area(i);

    mesh_length = (2 * bubble_max_radius) + 3;
  } else {
#ifndef HAVE_XPM
    fprintf(stderr, "Bug: simple mode code not set but HAVE_XPM not \
defined\n");
    exit(1);
#else
    /* Make sure all #ifdef sort of things have been taken care of in
       get_resources(). */
    if (use_default_bubble) {
#ifdef NO_DEFAULT_BUBBLE
      fprintf(stderr, "Bug: use_default_bubble and NO_DEFAULT_BUBBLE both \
defined\n");
      exit(1);
#else
      default_to_pixmaps();
#endif /* NO_DEFAULT_BUBBLE */

      /* Set mesh length */
      mesh_length = (2 * step_pixmaps[num_bubble_pixmaps-1]->radius) + 3;
    } else {
#ifdef BUBBLES_IO
      if (! regular_file(pixmap_file)) {
	/* perror() in regular_file printed error message */
	exit(1);
      }
      uncompress_file(pixmap_file, uncompressed);
      read_file_to_pixmaps(uncompressed);
      if (strcmp(pixmap_file, uncompressed))
	unlink(uncompressed);

      mesh_length = (2 * step_pixmaps[num_bubble_pixmaps-1]->radius) + 3;
#else
      fprintf(stderr, "Bug: use_default_bubble is not defined yet I/O is not \
compiled in\n");
      exit(1);
#endif /* BUBBLES_IO */
    }
#endif /* HAVE_XPM */

    /* Am I missing something in here??? */
  }

  mesh_width = (screen_width / mesh_length) + 1;
  mesh_height = (screen_height / mesh_length) + 1;
  mesh_cells = mesh_width * mesh_height;
  init_mesh();

  calculate_adjacent_list();

  adjust_areas();

  /* Graphics contexts for simple mode */
  if (simple) {
    gcv.foreground = default_fg_pixel;
    draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
    gcv.foreground = default_bg_pixel;
    erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  }

  XClearWindow (dpy, window);
}

static void 
bubbles (dpy, window)
     Display *dpy;
     Window window;
{
  Bubble *tmp;

  tmp = new_bubble();
  add_to_mesh(tmp);
  insert_new_bubble(tmp);

  XSync (dpy, True);
}


void 
screenhack (dpy, window)
     Display *dpy;
     Window window;
{
  init_bubbles (dpy, window);
  while (1) {
    bubbles (dpy, window);
    if (delay)
      usleep(delay);
  }
}

