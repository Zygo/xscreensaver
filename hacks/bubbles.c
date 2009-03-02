/* bubbles.c - frying pan / soft drink in a glass simulation */

/*$Id: bubbles.c,v 1.30 2008/07/31 19:27:48 jwz Exp $*/

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <limits.h>

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

#include "screenhack.h"
#include "yarandom.h"
#include "bubbles.h"
#include "xpm-pixmap.h"

#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
# define FANCY_BUBBLES
#endif

/* 
 * Public variables 
 */

static const char *bubbles_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*fpsSolid:		true",
  "*simple:		false",
  "*broken:		false",
  "*delay:		10000",
  "*quiet:		false", 
  "*mode:		float",
  "*trails:		false",
  "*3D:			false",
  0
};

static XrmOptionDescRec bubbles_options [] = {
  { "-simple",          ".simple",      XrmoptionNoArg, "true" },
#ifdef FANCY_BUBBLES
  { "-broken",          ".broken",      XrmoptionNoArg, "true" },
#endif
  { "-quiet",           ".quiet",       XrmoptionNoArg, "true" },
  { "-3D",          ".3D",      XrmoptionNoArg, "true" },
  { "-delay",           ".delay",       XrmoptionSepArg, 0 },
  { "-mode",            ".mode",        XrmoptionSepArg, 0 },
  { "-drop",            ".mode",        XrmoptionNoArg, "drop" },
  { "-rise",            ".mode",        XrmoptionNoArg, "rise" },
  { "-trails",          ".trails",      XrmoptionNoArg, "true" },
  { 0, 0, 0, 0 }
};

/* 
 * Private variables 
 */

struct state {
  Display *dpy;
  Window window;

  Bubble **mesh;
  int mesh_length;
  int mesh_width;
  int mesh_height;
  int mesh_cells;

  int **adjacent_list;

  int screen_width;
  int screen_height;
  int screen_depth;
  unsigned int default_fg_pixel, default_bg_pixel;

  int bubble_min_radius;	/* For simple mode only */
  int bubble_max_radius;
  long *bubble_areas;
  int *bubble_droppages;
  GC draw_gc, erase_gc;

#ifdef FANCY_BUBBLES
  int num_bubble_pixmaps;
  Bubble_Step **step_pixmaps;
#endif

  Bool simple;
  Bool broken;
  Bool quiet;
  Bool threed;
  Bool drop;
  Bool trails;
  int drop_dir;
  int delay;
};

static int drop_bubble( struct state *st, Bubble *bb );

/* 
 * To prevent forward references, some stuff is up here 
 */

static long
calc_bubble_area(struct state *st, int r)
/* Calculate the area of a bubble of radius r */
{
#ifdef DEBUG
  printf("%d %g\n", r,
	 10.0 * PI * (double)r * (double)r * (double)r);
#endif /* DEBUG */
  if (st->threed)
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
  if ((bb->cell_index < 0) || (bb->cell_index > st->mesh_cells)) {
    fprintf(stderr, "cell_index = %d\n", bb->cell_index);
    die_bad_bubble(bb);
  }
  if (bb->magic != BUBBLE_MAGIC)  {
    fprintf(stderr, "Magic = %d\n", bb->magic);
    die_bad_bubble(bb);
  }
  if (st->simple) {
    if ((bb->x < 0) || (bb->x > st->screen_width) ||
	(bb->y < 0) || (bb->y > st->screen_height) ||
	(bb->radius < st->bubble_min_radius) || (bb->radius >
					     st->bubble_max_radius)) {
      fprintf(stderr,
	      "radius = %d, x = %d, y = %d, magic = %d, cell index = %d\n",
	      bb->radius, bb->x, bb->y, bb->magic, bb->cell_index);
      die_bad_bubble(bb);  
    }
#ifdef FANCY_BUBBLES
  } else {
    if ((bb->x < 0) || (bb->x > st->screen_width) ||
	(bb->y < 0) || (bb->y > st->screen_height) ||
	(bb->radius < st->step_pixmaps[0]->radius) || 
	(bb->radius > st->step_pixmaps[st->num_bubble_pixmaps-1]->radius)) {
      fprintf(stderr,
	      "radius = %d, x = %d, y = %d, magic = %d, cell index = %d\n",
	      bb->radius, bb->x, bb->y, bb->magic, bb->cell_index);
      die_bad_bubble(bb);  
    }
#endif
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
init_mesh (struct state *st)
/* Setup the mesh of bubbles */
{
  int i;

  st->mesh = (Bubble **)xmalloc(st->mesh_cells * sizeof(Bubble *));
  for (i = 0; i < st->mesh_cells; i++)
    st->mesh[i] = (Bubble *)NULL;
}

static int
cell_to_mesh(struct state *st, int x, int y)
/* convert cell coordinates to mesh index */
{
#ifdef DEBUG
  if ((x < 0) || (y < 0)) {
    fprintf(stderr, "cell_to_mesh: x = %d, y = %d\n", x, y);
    exit(1);
  }
#endif
  return ((st->mesh_width * y) + x);
}

static void 
mesh_to_cell(struct state *st, int mi, int *cx, int *cy)
/* convert mesh index into cell coordinates */
{
  *cx = mi % st->mesh_width;
  *cy = mi / st->mesh_width;
}

static int
pixel_to_mesh(struct state *st, int x, int y)
/* convert screen coordinates into mesh index */
{
  return cell_to_mesh(st, (x / st->mesh_length), (y / st->mesh_length));
}

static int
verify_mesh_index(struct state *st, int x, int y)
/* check to see if (x,y) is in the mesh */
{
  if ((x < 0) || (y < 0) || (x >= st->mesh_width) || (y >= st->mesh_height))
    return (-1);
  return (cell_to_mesh(st, x, y));
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
add_to_mesh(struct state *st, Bubble *bb)
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

  add_bubble_to_list(&st->mesh[bb->cell_index], bb);
}

#ifdef DEBUG
static void 
print_mesh (struct state *st)
/* Print the contents of the mesh */
{
  int i;

  for (i = 0; i < st->mesh_cells; i++) {
    if (! null_bubble(st->mesh[i])) {
      printf("Mesh cell %d\n", i);
      print_bubble_list(st->mesh[i]);
    }
  }
}

static void 
valid_mesh (struct state *st)
/* Check to see if the mesh is Okay.  For debugging only. */
{
  int i;
  Bubble *b;

  for (i = 0; i < st->mesh_cells; i++) {
    b = st->mesh[i];
    while (! null_bubble(b))
      b = b->next;
  }
}

static int
total_bubbles (struct state *st)
/* Count how many bubbles there are in total.  For debugging only. */
{
  int rv = 0;
  int i;
  Bubble *b;

  for (i = 0; i < st->mesh_cells; i++) {
    b = st->mesh[i];
    while (! null_bubble(b)) {
      rv++;
      b = b->next;
    } 
  }

  return rv;
}
#endif /* DEBUG */

static void 
calculate_adjacent_list (struct state *st)
/* Calculate the list of cells adjacent to a particular cell for use
   later. */
{
  int i; 
  int ix, iy;

  st->adjacent_list = (int **)xmalloc(st->mesh_cells * sizeof(int *));
  for (i = 0; i < st->mesh_cells; i++) {
    st->adjacent_list[i] = (int *)xmalloc(9 * sizeof(int));
    mesh_to_cell(st, i, &ix, &iy);
    st->adjacent_list[i][0] = verify_mesh_index(st, --ix, --iy);
    st->adjacent_list[i][1] = verify_mesh_index(st, ++ix, iy);
    st->adjacent_list[i][2] = verify_mesh_index(st, ++ix, iy);
    st->adjacent_list[i][3] = verify_mesh_index(st, ix, ++iy);
    st->adjacent_list[i][4] = verify_mesh_index(st, ix, ++iy);
    st->adjacent_list[i][5] = verify_mesh_index(st, --ix, iy);
    st->adjacent_list[i][6] = verify_mesh_index(st, --ix, iy);
    st->adjacent_list[i][7] = verify_mesh_index(st, ix, --iy);
    st->adjacent_list[i][8] = i;
  }
}

static void
adjust_areas (struct state *st)
/* Adjust areas of bubbles so we don't get overflow in weighted_mean() */
{
  double maxvalue;
  long maxarea;
  long factor;
  int i;

#ifdef FANCY_BUBBLES
  if (st->simple)
    maxarea = st->bubble_areas[st->bubble_max_radius+1];
  else
    maxarea = st->step_pixmaps[st->num_bubble_pixmaps]->area;
#else /* !FANCY_BUBBLES */
  maxarea = st->bubble_areas[st->bubble_max_radius+1];
#endif /* !FANCY_BUBBLES */
  maxvalue = (double)st->screen_width * 2.0 * (double)maxarea;
  factor = (long)ceil(maxvalue / (double)LONG_MAX);
  if (factor > 1) {
    /* Overflow will occur in weighted_mean().  We must divide areas
       each by factor so it will never do so. */
#ifdef FANCY_BUBBLES
    if (st->simple) {
      for (i = st->bubble_min_radius; i <= st->bubble_max_radius+1; i++) {
	st->bubble_areas[i] /= factor;
	if (st->bubble_areas[i] == 0)
	  st->bubble_areas[i] = 1;
      }
    } else {
      for (i = 0; i <= st->num_bubble_pixmaps; i++) {
#ifdef DEBUG
	printf("area = %ld", st->step_pixmaps[i]->area);
#endif /* DEBUG */
	st->step_pixmaps[i]->area /= factor;
	if (st->step_pixmaps[i]->area == 0)
	  st->step_pixmaps[i]->area = 1;
#ifdef DEBUG
	printf("-> %ld\n", st->step_pixmaps[i]->area);
#endif /* DEBUG */
      }
    }
#else /* !FANCY_BUBBLES */
    for (i = st->bubble_min_radius; i <= st->bubble_max_radius+1; i++) {
      st->bubble_areas[i] /= factor;
      if (st->bubble_areas[i] == 0)
	st->bubble_areas[i] = 1;
    }
#endif /* !FANCY_BUBBLES */
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
new_bubble (struct state *st)
/* Add a new bubble at some random position on the screen of the smallest
size. */
{
  Bubble *rv = (Bubble *)xmalloc(sizeof(Bubble));

  /* Can't use null_bubble() here since magic number hasn't been set */
  if (rv == (Bubble *)NULL) {
    fprintf(stderr, "Ran out of memory!\n");
    exit(1);
  }

  if (st->simple) {
    rv->radius = st->bubble_min_radius;
    rv->area = st->bubble_areas[st->bubble_min_radius];
#ifdef FANCY_BUBBLES
  } else {
    rv->step = 0;
    rv->radius = st->step_pixmaps[0]->radius;
    rv->area = st->step_pixmaps[0]->area;
#endif /* FANCY_BUBBLES */
  }
  rv->visible = 0;
  rv->magic = BUBBLE_MAGIC;
  rv->x = random() % st->screen_width;
  rv->y = random() % st->screen_height;
  rv->cell_index = pixel_to_mesh(st, rv->x, rv->y);

  return rv;
}

static void 
show_bubble(struct state *st, Bubble *bb)
/* paint the bubble on the screen */
{
  if (null_bubble(bb)) {
    fprintf(stderr, "NULL bubble passed to show_bubble\n");
    exit(1);
  }

  if (! bb->visible) {
    bb->visible = 1;

    if (st->simple) {
      XDrawArc(st->dpy, st->window, st->draw_gc, (bb->x - bb->radius),
	       (bb->y - bb->radius), bb->radius*2, bb->radius*2, 0,
	       360*64);  
    } else {
#ifdef FANCY_BUBBLES
      XSetClipOrigin(st->dpy, st->step_pixmaps[bb->step]->draw_gc, 
		     (bb->x - bb->radius),
		     (bb->y - bb->radius));
      
      XCopyArea(st->dpy, st->step_pixmaps[bb->step]->ball, st->window, 
		st->step_pixmaps[bb->step]->draw_gc,
		0, 0, (bb->radius * 2), 
		(bb->radius * 2),  
		(bb->x - bb->radius),
		(bb->y - bb->radius));
#endif /* FANCY_BUBBLES */
    }
  }
}

static void 
hide_bubble(struct state *st, Bubble *bb)
/* erase the bubble */
{
  if (null_bubble(bb)) {
    fprintf(stderr, "NULL bubble passed to hide_bubble\n");
    exit(1);
  }

  if (bb->visible) {
    bb->visible = 0;

    if (st->simple) {
      XDrawArc(st->dpy, st->window, st->erase_gc, (bb->x - bb->radius),
	       (bb->y - bb->radius), bb->radius*2, bb->radius*2, 0,
	       360*64);
    } else {
#ifdef FANCY_BUBBLES
      if (! st->broken) {
	XSetClipOrigin(st->dpy, st->step_pixmaps[bb->step]->erase_gc, 
		       (bb->x - bb->radius), (bb->y - bb->radius));
	
	XFillRectangle(st->dpy, st->window, st->step_pixmaps[bb->step]->erase_gc,
		       (bb->x - bb->radius),
		       (bb->y - bb->radius),
		       (bb->radius * 2),
		       (bb->radius * 2));
      }
#endif /* FANCY_BUBBLES */
    }
  }
}

static void 
delete_bubble_in_mesh(struct state *st, Bubble *bb, int keep_bubble)
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
    bb->next = st->mesh[bb->cell_index];
  } else if ((null_bubble(bb->prev)) &&
	     (!null_bubble(bb->next))) {
    bb->next->prev = (Bubble *)NULL;
    st->mesh[bb->cell_index] = bb->next;
    bb->next = st->mesh[bb->cell_index];
  } else {
    /* Only item on list */
    st->mesh[bb->cell_index] = (Bubble *)NULL;
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
get_closest_bubble(struct state *st, Bubble *bb)
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
    if ((bb->cell_index < 0) || (bb->cell_index >= st->mesh_cells)) {
      fprintf(stderr, "bb->cell_index = %d\n", bb->cell_index);
      exit(1);
    }
#endif /* DEBUG */
/*    printf("%d,", bb->cell_index); */
    if (st->adjacent_list[bb->cell_index][i] != -1) {
      tmp = st->mesh[st->adjacent_list[bb->cell_index][i]];
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
ldr_barf (struct state *st)
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
bubble_eat(struct state *st, Bubble *diner, Bubble *food)
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

  hide_bubble(st, diner);
  hide_bubble(st, food);
  diner->x = weighted_mean(diner->x, food->x, diner->area, food->area);
  diner->y = weighted_mean(diner->y, food->y, diner->area, food->area);
  newmi = pixel_to_mesh(st, diner->x, diner->y);
  diner->area += food->area;
  delete_bubble_in_mesh(st, food, DELETE_BUBBLE);

  if (st->drop) {
	if ((st->simple) && (diner->area > st->bubble_areas[st->bubble_max_radius])) {
	  diner->area = st->bubble_areas[st->bubble_max_radius];
	}
#ifdef FANCY_BUBBLES
	if ((! st->simple) && (diner->area > st->step_pixmaps[st->num_bubble_pixmaps]->area)) {
	  diner->area = st->step_pixmaps[st->num_bubble_pixmaps]->area;
	}
#endif /* FANCY_BUBBLES */
  }
  else {
	if ((st->simple) && (diner->area > st->bubble_areas[st->bubble_max_radius])) {
	  delete_bubble_in_mesh(st, diner, DELETE_BUBBLE);
	  return 0;
	}
#ifdef FANCY_BUBBLES
	if ((! st->simple) && (diner->area > 
					   st->step_pixmaps[st->num_bubble_pixmaps]->area)) {
	  delete_bubble_in_mesh(st, diner, DELETE_BUBBLE);
	  return 0;
	}
#endif /* FANCY_BUBBLES */
  }

  if (st->simple) {
    if (diner->area > st->bubble_areas[diner->radius + 1]) {
      /* Move the bubble to a new radius */
      i = diner->radius;
      while ((i < st->bubble_max_radius - 1) && (diner->area > st->bubble_areas[i+1]))
		++i;
      diner->radius = i;
    }
    show_bubble(st, diner);
#ifdef FANCY_BUBBLES
  } else {
    if (diner->area > st->step_pixmaps[diner->step+1]->area) {
      i = diner->step;
      while ((i < st->num_bubble_pixmaps - 1) && (diner->area > st->step_pixmaps[i+1]->area))
		++i;
      diner->step = i;
      diner->radius = st->step_pixmaps[diner->step]->radius;
    }
    show_bubble(st, diner);
#endif /* FANCY_BUBBLES */
  }

  /* Now adjust locations and cells if need be */
  if (newmi != diner->cell_index) {
    delete_bubble_in_mesh(st, diner, KEEP_BUBBLE);
    diner->cell_index = newmi;
    add_to_mesh(st, diner);
  }

  return 1;
}

static int
merge_bubbles(struct state *st, Bubble *b1, Bubble *b2)
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
    hide_bubble(st, b1);
    delete_bubble_in_mesh(st, b1, DELETE_BUBBLE);
    return 0;
  }

  if (b1size > b2size) {
    switch (bubble_eat(st, b1, b2)) {
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
    switch (bubble_eat(st, b2, b1)) {
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
      switch (bubble_eat(st, b1, b2)) {
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
      switch (bubble_eat(st, b2, b1)) {
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
insert_new_bubble(struct state *st, Bubble *tmp)
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
  touch = get_closest_bubble(st, nextbub);
  if (null_bubble(touch))
	return;

  while (1) {

	/* Merge all touching bubbles */
	while (! null_bubble(touch)) {
	  switch (merge_bubbles(st, nextbub, touch)) {
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
	  touch = get_closest_bubble(st, nextbub);
	}
	
	if (null_bubble(nextbub))
	  break;

	/* Shift bubble down.  Break if we run off the screen. */
	if (st->drop) {
	  if (drop_bubble( st, nextbub ) == -1)
		break;
	}

	/* Check to see if there are any other bubbles still in the area
	   and if we need to do this all over again for them. */
	touch = get_closest_bubble(st, nextbub);
	if (null_bubble(touch)) {
	  /* We also continue every so often if we're dropping and the bubble is at max size */
	  if (st->drop) {
		if (st->simple) {
		  if ((nextbub->area >= st->bubble_areas[st->bubble_max_radius - 1]) && (random() % 2 == 0))
			continue;
		}
#ifdef FANCY_BUBBLES
		else {
		  if ((nextbub->step >= st->num_bubble_pixmaps - 1) && (random() % 2 == 0))
			continue;
		}
#endif /* FANCY_BUBBLES */
      }
	  break;
	}

  }
}


static void
leave_trail(struct state *st,  Bubble *bb ) 
{
  Bubble *tmp;

  tmp = new_bubble(st);
  tmp->x = bb->x;
  tmp->y = bb->y - ((bb->radius + 10) * st->drop_dir);
  tmp->cell_index = pixel_to_mesh(st, tmp->x, tmp->y);
  add_to_mesh(st, tmp);
  insert_new_bubble(st, tmp);
  show_bubble( st, tmp );	
}


static int
drop_bubble( struct state *st, Bubble *bb )
{
  int newmi;

  hide_bubble( st, bb );

  if (st->simple)
	(bb->y) += (st->bubble_droppages[bb->radius] * st->drop_dir);
#ifdef FANCY_BUBBLES
  else
	(bb->y) += (st->step_pixmaps[bb->step]->droppage * st->drop_dir);
#endif /* FANCY_BUBBLES */
  if ((bb->y < 0) || (bb->y > st->screen_height)) {
	delete_bubble_in_mesh( st, bb, DELETE_BUBBLE );
	return -1;
  }

  show_bubble( st, bb );

  /* Now adjust locations and cells if need be */
  newmi = pixel_to_mesh(st, bb->x, bb->y);
  if (newmi != bb->cell_index) {
    delete_bubble_in_mesh(st, bb, KEEP_BUBBLE);
    bb->cell_index = newmi;
    add_to_mesh(st, bb);
  }

  if (st->trails) {
	if (st->simple) {
	  if ((bb->area >= st->bubble_areas[st->bubble_max_radius - 1]) && (random() % 2 == 0)) 
		leave_trail( st, bb );
	}
#ifdef FANCY_BUBBLES
	else { 
	  if ((bb->step >= st->num_bubble_pixmaps - 1) && (random() % 2 == 0))
		leave_trail( st, bb );
	}
#endif /* FANCY_BUBBLES */
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

#ifdef FANCY_BUBBLES

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
make_pixmap_array(struct state *st, Bubble_Step *list)
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

  st->num_bubble_pixmaps = 1;
  while(tmp->next != (Bubble_Step *)NULL) {
    tmp = tmp->next;
    ++st->num_bubble_pixmaps;
  }

  if (st->num_bubble_pixmaps < 2) {
    fprintf(stderr, "Must be at least two bubbles in file\n");
    exit(1);
  }

  st->step_pixmaps = (Bubble_Step **)xmalloc((st->num_bubble_pixmaps + 1) * 
					 sizeof(Bubble_Step *));

  /* Copy them blindly into the array for sorting. */
  ind = 0;
  tmp = list;
  do {
    st->step_pixmaps[ind++] = tmp;
    tmp = tmp->next;
  } while(tmp != (Bubble_Step *)NULL);

  /* We make another bubble beyond the ones with pixmaps so that the final
     bubble hangs around and doesn't pop immediately.  It's radius and area
     are found by extrapolating from the largest two bubbles with pixmaps. */

  st->step_pixmaps[st->num_bubble_pixmaps] = 
    (Bubble_Step *)xmalloc(sizeof(Bubble_Step));
  st->step_pixmaps[st->num_bubble_pixmaps]->radius = INT_MAX;

  pixmap_sort(st->step_pixmaps, (st->num_bubble_pixmaps + 1));

#ifdef DEBUG
  if (st->step_pixmaps[st->num_bubble_pixmaps]->radius != INT_MAX) {
    fprintf(stderr, "pixmap_sort() screwed up make_pixmap_array\n");
  }
#endif /* DEBUG */

  st->step_pixmaps[st->num_bubble_pixmaps]->radius = 
    extrapolate(st->step_pixmaps[st->num_bubble_pixmaps-2]->radius,
		st->step_pixmaps[st->num_bubble_pixmaps-1]->radius);
  st->step_pixmaps[st->num_bubble_pixmaps]->area = 
    calc_bubble_area(st, st->step_pixmaps[st->num_bubble_pixmaps]->radius);
  

#ifdef DEBUG
  /* Now check for correct order */
  for (ind = 0; ind < st->num_bubble_pixmaps; ind++) {
    if (prevrad > 0) {
      if (st->step_pixmaps[ind]->radius < prevrad) {
	fprintf(stderr, "Pixmaps not in ascending order of radius\n");
	exit(1);
      }
    }
    prevrad = st->step_pixmaps[ind]->radius;
  }
#endif /* DEBUG */

  /* Now populate the droppage values */
  for (ind = 0; ind < st->num_bubble_pixmaps; ind++)
	  st->step_pixmaps[ind]->droppage = MAX_DROPPAGE * ind / st->num_bubble_pixmaps;
}

static void
make_pixmap_from_default(struct state *st, char **pixmap_data, Bubble_Step *bl)
/* Read pixmap data which has been compiled into the program and a pointer
 to which has been passed. 

 This is virtually copied verbatim from make_pixmap_from_file() above and
changes made to either should be propagated onwards! */
{
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

#ifdef FANCY_BUBBLES
  {
    int w, h;
    bl->ball = xpm_data_to_pixmap (st->dpy, st->window, (char **) pixmap_data,
                                   &w, &h, &bl->shape_mask);
    bl->radius = MAX(w, h) / 2;
    bl->area = calc_bubble_area(st, bl->radius);
  }
#endif /* FANCY_BUBBLES */

  gcv.foreground = st->default_fg_pixel;
  gcv.function = GXcopy;
  bl->draw_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  XSetClipMask(st->dpy, bl->draw_gc, bl->shape_mask);
  
  gcv.foreground = st->default_bg_pixel;
  gcv.function = GXcopy;
  bl->erase_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  XSetClipMask(st->dpy, bl->erase_gc, bl->shape_mask);
}

static void 
default_to_pixmaps (struct state *st)
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
    make_pixmap_from_default(st, pixpt, newpix);
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
  make_pixmap_array(st, pixmap_list);
}

#endif /* FANCY_BUBBLES */


/* 
 * Main stuff 
 */


static void 
get_resources(struct state *st)
/* Get the appropriate X resources and warn about any inconsistencies. */
{
  Bool rise;
  XWindowAttributes xgwa;
  Colormap cmap;
  char *s;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  cmap = xgwa.colormap;

  st->threed = get_boolean_resource(st->dpy, "3D", "Boolean");
  st->quiet = get_boolean_resource(st->dpy, "quiet", "Boolean");
  st->simple = get_boolean_resource(st->dpy, "simple", "Boolean");
  /* Forbid rendered bubbles on monochrome displays */
  if ((mono_p) && (! st->simple)) {
    if (! st->quiet)
      fprintf(stderr,
	      "Rendered bubbles not supported on monochrome displays\n");
    st->simple = True;
  }
  st->delay = get_integer_resource(st->dpy, "delay", "Integer");

  s = get_string_resource (st->dpy, "mode", "Mode");
  rise = False;
  if (!s || !*s || !strcasecmp (s, "float"))
    ;
  else if (!strcasecmp (s, "rise"))
    rise = True;
  else if (!strcasecmp (s, "drop"))
    st->drop = True;
  else
    fprintf (stderr, "%s: bogus mode: \"%s\"\n", progname, s);

  st->trails = get_boolean_resource(st->dpy, "trails", "Boolean");
  st->drop_dir = (st->drop ? 1 : -1);
  if (st->drop || rise)
	st->drop = 1;

  st->default_fg_pixel = get_pixel_resource (st->dpy,
					 cmap, "foreground", "Foreground");
  st->default_bg_pixel = get_pixel_resource (st->dpy,
					 cmap, "background", "Background");

  if (st->simple) {
    /* This is easy */
    st->broken = get_boolean_resource(st->dpy, "broken", "Boolean");
    if (st->broken)
      if (! st->quiet)
	fprintf(stderr, "-broken not available in simple mode\n");
  } else {
#ifndef FANCY_BUBBLES
    st->simple = 1;
#else  /* FANCY_BUBBLES */
    st->broken = get_boolean_resource(st->dpy, "broken", "Boolean");
#endif /* FANCY_BUBBLES */
  }
}

static void *
bubbles_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  int i;

  st->dpy = dpy;
  st->window = window;

  get_resources(st);

  XGetWindowAttributes (st->dpy, st->window, &xgwa);

#ifdef DEBUG
  printf("sizof(int) on this platform is %d\n", sizeof(int));
  printf("sizof(long) on this platform is %d\n", sizeof(long));
#endif /* DEBUG */

  st->screen_width = xgwa.width;
  st->screen_height = xgwa.height;
  st->screen_depth = xgwa.depth;

  if (st->simple) {
    /* These are pretty much plucked out of the air */
    st->bubble_min_radius = (int)(0.006*(double)(MIN(st->screen_width, 
						 st->screen_height)));
    st->bubble_max_radius = (int)(0.045*(double)(MIN(st->screen_width,
						 st->screen_height)));
    /* Some trivial values */
    if (st->bubble_min_radius < 1)
      st->bubble_min_radius = 1;
    if (st->bubble_max_radius <= st->bubble_min_radius)
      st->bubble_max_radius = st->bubble_min_radius + 1;

    st->mesh_length = (2 * st->bubble_max_radius) + 3;

    /* store area of each bubble of certain radius as number of 1/10s of
       a pixel area.  PI is defined in <math.h> */
    st->bubble_areas = (long *)xmalloc((st->bubble_max_radius + 2) * sizeof(int));
    for (i = 0; i < st->bubble_min_radius; i++)
      st->bubble_areas[i] = 0;
    for (i = st->bubble_min_radius; i <= (st->bubble_max_radius+1); i++)
      st->bubble_areas[i] = calc_bubble_area(st, i);

	/* Now populate the droppage values */
    st->bubble_droppages = (int *)xmalloc((st->bubble_max_radius + 2) * sizeof(int));
    for (i = 0; i < st->bubble_min_radius; i++)
      st->bubble_droppages[i] = 0;
    for (i = st->bubble_min_radius; i <= (st->bubble_max_radius+1); i++)
      st->bubble_droppages[i] = MAX_DROPPAGE * (i - st->bubble_min_radius) / (st->bubble_max_radius - st->bubble_min_radius);

    st->mesh_length = (2 * st->bubble_max_radius) + 3;
  } else {
#ifndef FANCY_BUBBLES
    fprintf(stderr,
	    "Bug: simple mode code not set but FANCY_BUBBLES not defined\n");
    exit(1);
#else  /* FANCY_BUBBLES */
    /* Make sure all #ifdef sort of things have been taken care of in
       get_resources(). */
    default_to_pixmaps(st);

    /* Set mesh length */
    st->mesh_length = (2 * st->step_pixmaps[st->num_bubble_pixmaps-1]->radius) + 3;
#endif /* FANCY_BUBBLES */

    /* Am I missing something in here??? */
  }

  st->mesh_width = (st->screen_width / st->mesh_length) + 1;
  st->mesh_height = (st->screen_height / st->mesh_length) + 1;
  st->mesh_cells = st->mesh_width * st->mesh_height;
  init_mesh(st);

  calculate_adjacent_list(st);

  adjust_areas(st);

  /* Graphics contexts for simple mode */
  if (st->simple) {
    gcv.foreground = st->default_fg_pixel;
    st->draw_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
    gcv.foreground = st->default_bg_pixel;
    st->erase_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  }

  XClearWindow (st->dpy, st->window);

# ifndef FANCY_BUBBLES
  st->simple = True;
# endif

  return st;
}

static unsigned long
bubbles_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  for (i = 0; i < 5; i++)
    {
      Bubble *tmp = new_bubble(st);
      add_to_mesh(st, tmp);
      insert_new_bubble(st, tmp);
    }
  return st->delay;
}


static void
bubbles_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
bubbles_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
bubbles_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}

XSCREENSAVER_MODULE ("Bubbles", bubbles)
