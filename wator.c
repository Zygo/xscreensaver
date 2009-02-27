#ifndef lint
static char sccsid[] = "@(#)wator.c	2.7 95/02/21 xlockmore";
#endif
/*-
 * wator.c - Dewdney's Wa-Tor, water torus simulation for xlock,
 *           the X Window System lockscreen.
 *
 * Copyright (c) 1994 by David Bagley.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 12-Dec-94: Coded from A.K. Dewdney's "The Armchair Universe, Computer 
 *            Recreations from the Pages of Scientific American Magazine"
 *            W.H. Freedman and Company, New York, 1988 also used life.c
 *            as a guide.
 */

#include "xlock.h"
#include "bitmaps/fish-0.xbm"
#include "bitmaps/fish-1.xbm"
#include "bitmaps/fish-2.xbm"
#include "bitmaps/fish-3.xbm"
#include "bitmaps/fish-4.xbm"
#include "bitmaps/fish-5.xbm"
#include "bitmaps/fish-6.xbm"
#include "bitmaps/fish-7.xbm"
#include "bitmaps/shark-0.xbm"
#include "bitmaps/shark-1.xbm"
#include "bitmaps/shark-2.xbm"
#include "bitmaps/shark-3.xbm"
#include "bitmaps/shark-4.xbm"
#include "bitmaps/shark-5.xbm"
#include "bitmaps/shark-6.xbm"
#include "bitmaps/shark-7.xbm"

#define	MAXROWS	155
#define MAXCOLS	144
#define FISH 0
#define SHARK 1
#define KINDS 2
#define ORIENTS 4
#define REFLECTS 2
#define BITMAPS (ORIENTS*REFLECTS*KINDS)
#define KINDBITMAPS (ORIENTS*REFLECTS)

static XImage logo[BITMAPS] = {
  {0, 0, 0, XYBitmap, (char *) fish0_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) fish1_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) fish2_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) fish3_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) fish4_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) fish5_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) fish6_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) fish7_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) shark0_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) shark1_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) shark2_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) shark3_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) shark4_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) shark5_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) shark6_bits, LSBFirst, 8, LSBFirst, 8, 1},
  {0, 0, 0, XYBitmap, (char *) shark7_bits, LSBFirst, 8, LSBFirst, 8, 1},
};

/* Fish and shark data */
typedef struct {
  char kind, age, food, direction;
  unsigned long color;
  int col, row;
} cellstruct;

/* Doublely linked list */
typedef struct _CellList
{
  cellstruct info;
  struct _CellList *previous, *next;
} CellList;

typedef struct {
  int initialized;
  int nkind[KINDS];  /* Number of fish and sharks */
  int breed[KINDS];  /* Breeding time of fish and sharks */
  int sstarve;       /* Time the sharks starve if they dont find a fish */
  int kind;          /* Currently working on fish or sharks? */
  int xs, ys;        /* Size of fish and sharks */
  int xb, yb;        /* Bitmap offset for fish and sharks */
  int pixelmode;
  int generation;
  int ncols, nrows;
  int width, height;
  CellList *currkind, *babykind, *lastkind[KINDS+1], *firstkind[KINDS+1];
  CellList *arr[MAXCOLS][MAXROWS];  /* 0=empty or pts to a fish or shark */
} watorstruct;

static watorstruct wators[MAXSCREENS];
static int icon_width, icon_height;

static void
drawcell(win, col, row, color, bitmap)
  Window win;
  int col, row;
  long unsigned int color;
  int bitmap;
{
  watorstruct *wp = &wators[screen];

  XSetForeground(dsp, Scr[screen].gc, color);
  if (wp->pixelmode)
    XFillRectangle(dsp, win, Scr[screen].gc,
      wp->xb + wp->xs * col, wp->yb + wp->ys * row, wp->xs, wp->ys);
  else
    XPutImage(dsp, win, Scr[screen].gc, &logo[bitmap], 0, 0,
      wp->xb + wp->xs * col, wp->yb + wp->ys * row, icon_width, icon_height);
}


static void
erasecell(win, col, row)
  Window win;
  int col, row;
{
  watorstruct *wp = &wators[screen];

  XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
  XFillRectangle(dsp, win, Scr[screen].gc,
    wp->xb + wp->xs * col, wp->yb + wp->ys * row, wp->xs, wp->ys);
}

static void init_kindlist(kind)
int kind;
{
  watorstruct *wp = &wators[screen];

  /* Waste some space at the beginning and end of list
     so we do not have to complicated checks against falling off the ends. */
  wp->lastkind[kind] = (CellList *) malloc (sizeof (CellList));
  wp->firstkind[kind] = (CellList *) malloc (sizeof (CellList));
  wp->firstkind[kind]->previous = wp->lastkind[kind]->next = NULL;
  wp->firstkind[kind]->next = wp->lastkind[kind]->previous = NULL;
  wp->firstkind[kind]->next = wp->lastkind[kind];
  wp->lastkind[kind]->previous = wp->firstkind[kind];
}

static void addto_kindlist(kind, info)
int kind;
cellstruct info;
{
  watorstruct *wp = &wators[screen];

  wp->currkind = (CellList *) malloc (sizeof (CellList));
  wp->lastkind[kind]->previous->next = wp->currkind;
  wp->currkind->previous = wp->lastkind[kind]->previous;
  wp->currkind->next = wp->lastkind[kind];
  wp->lastkind[kind]->previous = wp->currkind;
  wp->currkind->info = info;
}

static void removefrom_kindlist(ptr)
CellList *ptr;
{
  watorstruct *wp = &wators[screen];

  ptr->previous->next = ptr->next;
  ptr->next->previous = ptr->previous;
  wp->arr[ptr->info.col][ptr->info.row] = 0;
  free(ptr);
}

static void dupin_kindlist()
{
  watorstruct *wp = &wators[screen];
  CellList *temp;
 
  temp = (CellList *) malloc (sizeof (CellList));
  temp->previous = wp->babykind;
  temp->next = wp->babykind->next;
  wp->babykind->next = temp;
  temp->next->previous = temp;
  temp->info = wp->babykind->info;
  wp->babykind = temp;
}

/*  new fish at end of list, this rotates who goes first, young fish go last *
 *  this most likely will not change the feel to any real degree             */
static void cutfrom_kindlist()
{
  watorstruct *wp = &wators[screen];
 
  wp->babykind=wp->currkind;
  wp->currkind=wp->currkind->previous;
  wp->currkind->next = wp->babykind->next;
  wp->babykind->next->previous = wp->currkind;
  wp->babykind->next = wp->lastkind[KINDS];
  wp->babykind->previous = wp->lastkind[KINDS]->previous;
  wp->babykind->previous->next = wp->babykind;
  wp->babykind->next->previous = wp->babykind;
}

static void reattach_kindlist(kind)
int kind;
{
  watorstruct *wp = &wators[screen];

  wp->currkind = wp->lastkind[kind]->previous; 
  wp->currkind->next = wp->firstkind[KINDS]->next; 
  wp->currkind->next->previous = wp->currkind;
  wp->lastkind[kind]->previous = wp->lastkind[KINDS]->previous; 
  wp->lastkind[KINDS]->previous->next = wp->lastkind[kind];
  wp->lastkind[KINDS]->previous = wp->firstkind[KINDS]; 
  wp->firstkind[KINDS]->next = wp->lastkind[KINDS]; 
}

static void flush_kindlist(kind)
int kind;
{
  watorstruct *wp = &wators[screen];

  while (wp->lastkind[kind]->previous != wp->firstkind[kind]) {
    wp->currkind = wp->lastkind[kind]->previous;
    wp->currkind->previous->next = wp->lastkind[kind];
    wp->lastkind[kind]->previous = wp->currkind->previous;
    wp->arr[wp->currkind->info.col][wp->currkind->info.row] = 0;
    free(wp->currkind);
  }
}


void
initwator(win)
  Window win;
{
  int i, row, col, kind;
  XWindowAttributes xgwa;
  watorstruct *wp = &wators[screen];
  cellstruct info;

  wp->generation = 0;
  if (!wp->initialized) { /* Genesis */
    icon_width = fish0_width;
    icon_height = fish0_height;
    wp->initialized = 1;
    /* Set up what will be a 'triply' linked list.
       doubly linked list, doubly linked to an array */ 
    (void) memset(wp->arr, 0, sizeof(wp->arr));
    for (kind = FISH; kind <= KINDS; kind++)
      init_kindlist(kind);
    for (i = 0; i < BITMAPS; i++) {
      logo[i].width = icon_width;
      logo[i].height = icon_height;
      logo[i].bytes_per_line = (fish0_width + 7) / 8;
    }
  } else /* Exterminate all  */
    for (i = FISH; i <= KINDS; i++)
      flush_kindlist(i);

  (void) XGetWindowAttributes(dsp, win, &xgwa);
  wp->width = xgwa.width;
  wp->height = xgwa.height;
  wp->pixelmode = (wp->width + wp->height < 3 * (icon_width + icon_height));
  if (wp->pixelmode) {
    wp->ncols = min(wp->width / 2, MAXCOLS);
    wp->nrows = min(wp->height / 2, MAXROWS);
  } else {
    wp->ncols = min(wp->width / icon_width, MAXCOLS);
    wp->nrows = min(wp->height / icon_height, MAXROWS);
  }

  /* Play G-d with these numbers */
  wp->nkind[FISH] = wp->ncols * wp->nrows / 3;
  wp->nkind[SHARK] = wp->nkind[FISH] / 10;
  wp->kind = FISH;
  if (!wp->nkind[SHARK])
    wp->nkind[SHARK] = 1;
  if (batchcount < 1)
    batchcount = 1;
  else if (batchcount > wp->breed[SHARK])
    batchcount = 4;
  wp->breed[FISH] = batchcount;
  wp->breed[SHARK] = 10;
  wp->sstarve = 3;

  wp->xs = wp->width / wp->ncols;
  wp->ys = wp->height / wp->nrows;
  wp->xb = (wp->width - wp->xs * wp->ncols) / 2;
  wp->yb = (wp->height - wp->ys * wp->nrows) / 2;

  XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
  XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, wp->width, wp->height);
  for (kind = FISH; kind <= SHARK; kind++) {
    i = 0;
    while (i < wp->nkind[kind]) {
      col = RAND() % wp->ncols;
      row = RAND() % wp->nrows;
      if (!wp->arr[col][row]) {
        i++;
        info.kind = kind;
        info.age = RAND() % wp->breed[kind];
        info.food = RAND() % wp->sstarve;
        info.direction = RAND() % KINDBITMAPS + kind * KINDBITMAPS;
        if (!mono && Scr[screen].npixels > 2)
          info.color = Scr[screen].pixels[RAND() % Scr[screen].npixels];
        else
          info.color = WhitePixel(dsp, screen);
        info.col = col;
        info.row = row;
        addto_kindlist(kind, info);
        wp->arr[col][row] = wp->currkind;
        drawcell(win, col, row,
                 wp->currkind->info.color,
                 wp->currkind->info.direction);
      }
    }
  }
}

void
drawwator(win)
  Window win;
{
  int col, row, colprevious, rowprevious, colnext, rownext;
  int i, numok;
      
  struct {
    int x, y, dir;
  } acell[4];

  watorstruct *wp = &wators[screen];

  /* Alternate updates, fish and sharks live out of phase with each other */
  wp->kind = (wp->kind + 1) % KINDS;
  {
    wp->currkind = wp->firstkind[wp->kind]->next;

    while (wp->currkind != wp->lastkind[wp->kind]) {
      col = wp->currkind->info.col;
      row = wp->currkind->info.row;
      colprevious = (!col) ? wp->ncols - 1 : col - 1;
      rowprevious = (!row) ? wp->nrows - 1 : row - 1;
      colnext = (col == wp->ncols - 1) ? 0 : col + 1;
      rownext = (row == wp->nrows - 1) ? 0 : row + 1;

      numok = 0;
      if (wp->kind == SHARK) { /* Scan for fish */
        if (wp->arr[col][rowprevious] &&
            wp->arr[col][rowprevious]->info.kind == FISH) {
          acell[numok].x = col;
          acell[numok].y = rowprevious;
          acell[numok++].dir = 0;
        }
        if (wp->arr[colnext][row] &&
            wp->arr[colnext][row]->info.kind == FISH) {
          acell[numok].x = colnext;
          acell[numok].y = row;
          acell[numok++].dir = 1;
        }
        if (wp->arr[col][rownext] &&
            wp->arr[col][rownext]->info.kind == FISH) {
          acell[numok].x = col;
          acell[numok].y = rownext;
          acell[numok++].dir = 2;
        }
        if (wp->arr[colprevious][row] &&
            wp->arr[colprevious][row]->info.kind == FISH) {
          acell[numok].x = colprevious;
          acell[numok].y = row;
          acell[numok++].dir = 3;
        }
        if (numok) { /* No thanks, I'm vegetarian */
          i = RAND() % numok;
          wp->nkind[FISH]--;
          removefrom_kindlist(wp->arr[acell[i].x][acell[i].y]);   
          wp->arr[acell[i].x][acell[i].y] = wp->currkind;
          wp->currkind->info.direction = acell[i].dir +
            ((RAND() % REFLECTS) ? 0 : ORIENTS) + wp->kind * KINDBITMAPS;
          wp->currkind->info.col = acell[i].x;
          wp->currkind->info.row = acell[i].y;
          wp->currkind->info.food = wp->sstarve;
          drawcell(win, wp->currkind->info.col, wp->currkind->info.row,
                   wp->currkind->info.color, wp->currkind->info.direction);
          if (++(wp->currkind->info.age) >= wp->breed[wp->kind]) { /* breed */
            cutfrom_kindlist(); /* This rotates out who goes first */
            wp->babykind->info.age = 0;
            dupin_kindlist();
            wp->arr[col][row] = wp->babykind;
            wp->babykind->info.col = col;
            wp->babykind->info.row = row;
            wp->babykind->info.age = -1; /* Make one a little younger */
            if (!(RAND() % 60)) /* A color mutation */
              if (++(wp->babykind->info.color) >= Scr[screen].npixels)
                wp->babykind->info.color = 0;
            wp->nkind[wp->kind]++;
          } else {
            wp->arr[col][row] = 0;
            erasecell(win, col, row);
          }
        } else {
          if (wp->currkind->info.food-- < 0) { /* Time to die, Jaws */
            /* back up one or else in void */
            wp->currkind = wp->currkind->previous;
            removefrom_kindlist(wp->arr[col][row]);   
            wp->arr[col][row] = 0;
            erasecell(win, col, row);
            wp->nkind[wp->kind]--;
            numok = -1; /* Want to escape from next if */
          }
        }
      }
      if (!numok) { /* Fish or shark search for a place to go */
        if (!wp->arr[col][rowprevious]) {
          acell[numok].x = col;
          acell[numok].y = rowprevious;
          acell[numok++].dir = 0;
        }
        if (!wp->arr[colnext][row]) {
          acell[numok].x = colnext;
          acell[numok].y = row;
          acell[numok++].dir = 1;
        }
        if (!wp->arr[col][rownext]) {
          acell[numok].x = col;
          acell[numok].y = rownext;
          acell[numok++].dir = 2;
        }
        if (!wp->arr[colprevious][row]) {
          acell[numok].x = colprevious;
          acell[numok].y = row;
          acell[numok++].dir = 3;
        }
        if (numok) { /* Found a place to go */
          i = RAND() % numok;
          wp->arr[acell[i].x][acell[i].y] = wp->currkind;
          wp->currkind->info.direction = acell[i].dir +
            ((RAND() % REFLECTS) ? 0 : ORIENTS) + wp->kind * KINDBITMAPS;
          wp->currkind->info.col = acell[i].x;
          wp->currkind->info.row = acell[i].y;
          drawcell(win,
                   wp->currkind->info.col, wp->currkind->info.row,
                   wp->currkind->info.color, wp->currkind->info.direction);
          if (++(wp->currkind->info.age) >= wp->breed[wp->kind]) { /* breed */
            cutfrom_kindlist(); /* This rotates out who goes first */
            wp->babykind->info.age = 0;
            dupin_kindlist();
            wp->arr[col][row] = wp->babykind;
            wp->babykind->info.col = col;
            wp->babykind->info.row = row;
            wp->babykind->info.age = -1; /* Make one a little younger */
            wp->nkind[wp->kind]++;
          } else {
            wp->arr[col][row] = 0;
            erasecell(win, col, row);
          }
        } else {
          /* I'll just sit here and wave my tail so you know I am alive */
          wp->currkind->info.direction =
            (wp->currkind->info.direction + ORIENTS) % KINDBITMAPS +
            wp->kind * KINDBITMAPS;
          drawcell(win, col, row, wp->currkind->info.color,
                   wp->currkind->info.direction);
        }
      }
      wp->currkind = wp->currkind->next;
    }
    reattach_kindlist(wp->kind);
  }
      
  if ((wp->nkind[FISH] >= wp->nrows * wp->ncols) ||
      (!wp->nkind[FISH] && !wp->nkind[SHARK]) ||
      wp->generation >= 32767) /* Maybe you want to change this? */ {
    initwator(win);
  }
  if (wp->kind == SHARK)
    wp->generation++;
}
