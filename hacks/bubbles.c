/* bubbles.c - frying pan / soft drink in a glass simulation */

/*$Id: bubbles.c,v 1.17 2000/07/19 06:38:42 jwz Exp $*/

/*
 *  Copyright (C) 1995-1996 James Macnicol
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
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
 * Internet E-mail : j-macnicol@adfa.edu.au
 */

#include <math.h>
#include "screenhack.h"
#include "bubbles.h"

#include <limits.h>

#include <stdio.h>
#include <string.h>

#ifndef VMS
# include <sys/wait.h>
#else /* VMS */
# if __DECC_VER >= 50200000
#  include <sys/wait.h>
# endif
#endif /* VMS */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include "yarandom.h"

#ifdef HAVE_XPM
# include <X11/xpm.h>
#endif

/* 
 * Public variables 
 */

extern void init_default_bubbles(void);
extern int num_default_bubbles;
extern char **default_bubbles[];
static int drop_bubble( Bubble *bb );

char *progclass = "Bubbles";

char *defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*simple:		false",
  "*broken:		false",
  "*delay:		800",
  "*quiet:		false", 
  "*nodelay:		false",
  "*drop:		false",
  "*trails:		false",
  "*3D:			false",
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
  { "-delay",           ".delay",       XrmoptionSepArg, 0 },
  { "-drop",            ".drop",        XrmoptionNoArg, "true" },
  { "-rise",            ".rise",        XrmoptionNoArg, "true" },
  { "-trails",          ".trails",      XrmoptionNoArg, "true" },
  { 0, 0, 0, 0 }
};

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
static Visual *defvisual;

/* For simple mode only */
static int bubble_min_radius;
static int bubble_max_radius;
static long *bubble_areas;
static int *bubble_droppages;
static GC draw_gc, erase_gc;

#ifdef HAVE_XPM
static int num_bubble_pixmaps;
static Bubble_Step **step_pixmaps;
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
static Bool drop = False;
static Bool trails = False;
static int drop_dir;
static int delay;

/* 
 * To prevent forward references, some stuff is up here 
 */

static long
calc_bubble_area(int r)
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
xmalloc(size_t size)
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
die_bad_bubble(Bubble *bb)
/* This is for use with GDB */
{
  fprintf(stderr, "Bad bubble detected at 0x%x!\n", (int)bb);
  exit(1);
}
#endif

static int
null_bubble(Bubble *bb)
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
	      "radius = %d, x = %d, y = %d, magic = %d, cell index = %d\n",
	      bb->radius, bb->x, bb->y, bb->magic, bb->cell_index);
      die_bad_bubble(bb);  
    }
#ifdef HAVE_XPM
  } else {
    if ((bb->x < 0) || (bb->x > screen_width) ||
	(bb->y < 0) || (bb->y > screen_height) ||
	(bb->radius < step_pixmaps[0]->radius) || 
	(bb->radius > step_pixmaps[num_bubble_pixmaps-1]->radius)) {
      fprintf(stderr,
	      "radius = %d, x = %d, y = %d, magic = %d, cell index = %d\n",
	      bb->radius, bb->x, bb->y, bb->magic, bb->cell_index);
      die_bad_bubble(bb);  
    }
#endif /* HAVE_XPM */
  }
#endif /* DEBUG */
  return 0;
}

#ifdef DEBUG
static void 
print_bubble_list(Bubble *bb)
/* Print list of where all the bubbles are.  For debugging purposes only. */
{
  if (! null_bubble(bb)) {
    printf("  (%d, %d) %d\n", bb->x, bb->y, bb->radius);
    print_bubble_list(bb->next);
  }
}
#endif /* DEBUG */

static void 
add_bubble_to_list(Bubble **list, Bubble *bb)
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
init_mesh (void)
/* Setup the mesh of bubbles */
{
  int i;

  mesh = (Bubble **)xmalloc(mesh_cells * sizeof(Bubble *));
  for (i = 0; i < mesh_cells; i++)
    mesh[i] = (Bubble *)NULL;
}

static int
cell_to_mesh(int x, int y)
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
mesh_to_cell(int mi, int *cx, int *cy)
/* convert mesh index into cell coordinates */
{
  *cx = mi % mesh_width;
  *cy = mi / mesh_width;
}

static int
pixel_to_mesh(int x, int y)
/* convert screen coordinates into mesh index */
{
  return cell_to_mesh((x / mesh_length), (y / mesh_length));
}

static int
verify_mesh_index(int x, int y)
/* check to see if (x,y) is in the mesh */
{
  if ((x < 0) || (y < 0) || (x >= mesh_width) || (y >= mesh_height))
    return (-1);
  return (cell_to_mesh(x, y));
}

#ifdef DEBUG
static void 
print_adjacents(int *adj)
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
add_to_mesh(Bubble *bb)
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
print_mesh (void)
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
valid_mesh (void)
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
total_bubbles (void)
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
calculate_adjacent_list (void)
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
adjust_areas (void)
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
new_bubble (void)
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
  rv->x = random() % screen_width;
  rv->y = random() % screen_height;
  rv->cell_index = pixel_to_mesh(rv->x, rv->y);

  return rv;
}

static void 
show_bubble(Bubble *bb)
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
hide_bubble(Bubble *bb)
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
delete_bubble_in_mesh(Bubble *bb, int keep_bubble)
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
ulongsqrint(int x)
/* Saves ugly inline code */
{
  return ((unsigned long)x * (unsigned long)x);
}

static Bubble *
get_closest_bubble(Bubble *bb)
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
ldr_barf (void)
{
}
#endif /* DEBUG */

static long
long_div_round(long num, long dem)
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
weighted_mean(int n1, int n2, long w1, long w2)
/* Mean of n1 and n2 respectively weighted by weights w1 and w2. */
{
#ifdef DEBUG
  if ((w1 <= 0) || (w2 <= 0)) {
    fprintf(stderr,
	    "Bad weights passed to weighted_mean() - (%d, %d, %ld, %ld)!\n",
	    n1, n2, w1, w2);
    exit(1);
  }
#endif /* DEBUG */
  return ((int)long_div_round((long)n1 * w1 + (long)n2 * w2,
                           w1 + w2));
}

static int
bubble_eat(Bubble *diner, Bubble *food)
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

  if (drop) {
	if ((simple) && (diner->area > bubble_areas[bubble_max_radius])) {
	  diner->area = bubble_areas[bubble_max_radius];
	}
#ifdef HAVE_XPM
	if ((! simple) && (diner->area > step_pixmaps[num_bubble_pixmaps]->area)) {
	  diner->area = step_pixmaps[num_bubble_pixmaps]->area;
	}
#endif /* HAVE_XPM */
  }
  else {
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
  }

  if (simple) {
    if (diner->area > bubble_areas[diner->radius + 1]) {
      /* Move the bubble to a new radius */
      i = diner->radius;
      while ((i < bubble_max_radius - 1) && (diner->area > bubble_areas[i+1]))
		++i;
      diner->radius = i;
    }
    show_bubble(diner);
#ifdef HAVE_XPM
  } else {
    if (diner->area > step_pixmaps[diner->step+1]->area) {
      i = diner->step;
      while ((i < num_bubble_pixmaps - 1) && (diner->area > step_pixmaps[i+1]->area))
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
merge_bubbles(Bubble *b1, Bubble *b2)
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
    if ((random() % 2) == 0) {
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
insert_new_bubble(Bubble *tmp)
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
  if (null_bubble(touch))
	return;

  while (1) {

	/* Merge all touching bubbles */
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
	
	  /* Check to see if any bubble survived. */
	  if (null_bubble(nextbub))
		break;

	  /* Check to see if there are any other bubbles still in the area
		 and if we need to do this all over again for them. */
	  touch = get_closest_bubble(nextbub);
	}
	
	if (null_bubble(nextbub))
	  break;

	/* Shift bubble down.  Break if we run off the screen. */
	if (drop) {
	  if (drop_bubble( nextbub ) == -1)
		break;
	}

	/* Check to see if there are any other bubbles still in the area
	   and if we need to do this all over again for them. */
	touch = get_closest_bubble(nextbub);
	if (null_bubble(touch)) {
	  /* We also continue every so often if we're dropping and the bubble is at max size */
	  if (drop) {
		if (simple) {
		  if ((nextbub->area >= bubble_areas[bubble_max_radius - 1]) && (random() % 2 == 0))
			continue;
		}
#ifdef HAVE_XPM
		else {
		  if ((nextbub->step >= num_bubble_pixmaps - 1) && (random() % 2 == 0))
			continue;
		}
#endif /* HAVE_XPM */
      }
	  break;
	}

  }
}


static void
leave_trail( Bubble *bb ) 
{
  Bubble *tmp;

  tmp = new_bubble();
  tmp->x = bb->x;
  tmp->y = bb->y - ((bb->radius + 10) * drop_dir);
  tmp->cell_index = pixel_to_mesh(tmp->x, tmp->y);
  add_to_mesh(tmp);
  insert_new_bubble(tmp);
  show_bubble( tmp );	
}


static int
drop_bubble( Bubble *bb )
{
  int newmi;

  hide_bubble( bb );

  if (simple)
	(bb->y) += (bubble_droppages[bb->radius] * drop_dir);
#ifdef HAVE_XPM
  else
	(bb->y) += (step_pixmaps[bb->step]->droppage * drop_dir);
#endif /* HAVE_XPM */
  if ((bb->y < 0) || (bb->y > screen_height)) {
	delete_bubble_in_mesh( bb, DELETE_BUBBLE );
	return -1;
  }

  show_bubble( bb );

  /* Now adjust locations and cells if need be */
  newmi = pixel_to_mesh(bb->x, bb->y);
  if (newmi != bb->cell_index) {
    delete_bubble_in_mesh(bb, KEEP_BUBBLE);
    bb->cell_index = newmi;
    add_to_mesh(bb);
  }

  if (trails) {
	if (simple) {
	  if ((bb->area >= bubble_areas[bubble_max_radius - 1]) && (random() % 2 == 0)) 
		leave_trail( bb );
	}
#ifdef HAVE_XPM
	else { 
	  if ((bb->step >= num_bubble_pixmaps - 1) && (random() % 2 == 0))
		leave_trail( bb );
	}
#endif /* HAVE_XPM */
  }

  return 0;
}


#ifdef DEBUG
static int
get_length_of_bubble_list(Bubble *bb)
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

/*
 * Pixmaps without file I/O (but do have XPM)
 */

static void 
pixmap_sort(Bubble_Step **head, int numelems)
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
extrapolate(int i1, int i2)
{
  return (i2 + (i2 - i1));
}

static void 
make_pixmap_array(Bubble_Step *list)
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

  /* Now populate the droppage values */
  for (ind = 0; ind < num_bubble_pixmaps; ind++)
	  step_pixmaps[ind]->droppage = MAX_DROPPAGE * ind / num_bubble_pixmaps;
}

static void
make_pixmap_from_default(char **pixmap_data, Bubble_Step *bl)
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

  bl->xpmattrs.valuemask = 0;

#ifdef XpmCloseness
  bl->xpmattrs.valuemask |= XpmCloseness;
  bl->xpmattrs.closeness = 40000;
#endif
#ifdef XpmVisual
  bl->xpmattrs.valuemask |= XpmVisual;
  bl->xpmattrs.visual = defvisual;
#endif
#ifdef XpmDepth
  bl->xpmattrs.valuemask |= XpmDepth;
  bl->xpmattrs.depth = screen_depth;
#endif
#ifdef XpmColormap
  bl->xpmattrs.valuemask |= XpmColormap;
  bl->xpmattrs.colormap = defcmap;
#endif


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
default_to_pixmaps (void)
/* Make pixmaps out of default ball data stored in bubbles_default.c */
{
  int i;
  Bubble_Step *pixmap_list = (Bubble_Step *)NULL;
  Bubble_Step *newpix, *tmppix;
  char **pixpt;

  init_default_bubbles();

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

#endif /* HAVE_XPM */


/* 
 * Main stuff 
 */


static void 
get_resources(Display *dpy, Window window)
/* Get the appropriate X resources and warn about any inconsistencies. */
{
  Bool nodelay, rise;
  XWindowAttributes xgwa;
  Colormap cmap;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;

  threed = get_boolean_resource("3D", "Boolean");
  quiet = get_boolean_resource("quiet", "Boolean");
  simple = get_boolean_resource("simple", "Boolean");
  /* Forbid rendered bubbles on monochrome displays */
  if ((mono_p) && (! simple)) {
    if (! quiet)
      fprintf(stderr,
	      "Rendered bubbles not supported on monochrome displays\n");
    simple = True;
  }
  delay = get_integer_resource("delay", "Integer");
  nodelay = get_boolean_resource("nodelay", "Boolean");
  if (nodelay)
    delay = 0;
  if (delay < 0)
    delay = 0;

  drop = get_boolean_resource("drop", "Boolean");
  rise = get_boolean_resource("rise", "Boolean");
  trails = get_boolean_resource("trails", "Boolean");
  if (drop && rise) {
	fprintf( stderr, "Sorry, bubbles can't both drop and rise\n" );
	exit(1);
  }
  drop_dir = (drop ? 1 : -1);
  if (drop || rise)
	drop = 1;

  default_fg_pixel = get_pixel_resource ("foreground", "Foreground", dpy,
					 cmap);
  default_bg_pixel = get_pixel_resource ("background", "Background", dpy,
					 cmap);

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
#endif /* HAVE_XPM */
  }
}

static void
init_bubbles (Display *dpy, Window window)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  int i;

  defdsp = dpy;
  defwin = window;

  get_resources(dpy, window);

  XGetWindowAttributes (dpy, window, &xgwa);

#ifdef DEBUG
  printf("sizof(int) on this platform is %d\n", sizeof(int));
  printf("sizof(long) on this platform is %d\n", sizeof(long));
#endif /* DEBUG */

  screen_width = xgwa.width;
  screen_height = xgwa.height;
  screen_depth = xgwa.depth;
  defcmap = xgwa.colormap;
  defvisual = xgwa.visual;

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

	/* Now populate the droppage values */
    bubble_droppages = (int *)xmalloc((bubble_max_radius + 2) * sizeof(int));
    for (i = 0; i < bubble_min_radius; i++)
      bubble_droppages[i] = 0;
    for (i = bubble_min_radius; i <= (bubble_max_radius+1); i++)
      bubble_droppages[i] = MAX_DROPPAGE * (i - bubble_min_radius) / (bubble_max_radius - bubble_min_radius);

    mesh_length = (2 * bubble_max_radius) + 3;
  } else {
#ifndef HAVE_XPM
    fprintf(stderr,
	    "Bug: simple mode code not set but HAVE_XPM not defined\n");
    exit(1);
#else
    /* Make sure all #ifdef sort of things have been taken care of in
       get_resources(). */
    default_to_pixmaps();

    /* Set mesh length */
    mesh_length = (2 * step_pixmaps[num_bubble_pixmaps-1]->radius) + 3;
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
bubbles (Display *dpy, Window window)
{
  Bubble *tmp;

  tmp = new_bubble();
  add_to_mesh(tmp);
  insert_new_bubble(tmp);

  XSync (dpy, False);
}


void 
screenhack (Display *dpy, Window window)
{
  init_bubbles (dpy, window);
  while (1) {
    bubbles (dpy, window);
    screenhack_handle_events (dpy);
    if (delay)
      usleep(delay);
  }
}

