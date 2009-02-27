#ifndef lint
static char sccsid[] = "@(#)life3d.c	2.7 95/02/21 xlockmore";
#endif
/*-
 * life3d.c - Extension to Conway's game of Life, Carter Bays' 4555 3d life,
 *            for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1994 by David Bagley.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 12-Feb-95: shooting gliders added
 * 07-Dec-94: used life.c and a DOS version of 3dlife
 * Copyright 1993 Anthony Wesley awesley@canb.auug.org.au found at
 * life.anu.edu.au /pub/complex_systems/alife/3DLIFE.ZIP
 * There is some flashing that was not in the original.  This is because 
 * the direct video memory access garbage collection was not portable.
 *
 *
 * References:
 * Dewdney, A.K., "The Armchair Universe, Computer Recreations from the
 *   Pages of Scientific American Magazine", W.H. Freedman and Company,
 *   New York, 1988
 * Bays, Carter, "The Game of Three Dimensional Life", 86/11/20
 *   with (latest?) update from 87/2/1
 */

#include <math.h>
#include "xlock.h"

#define ON 0x40
#define OFF 0

/* Don't change these numbers without changing the offset() macro below! */
#define MAXSTACKS 64
#define	MAXROWS 128
#define MAXCOLUMNS 128
#define BASESIZE ((MAXCOLUMNS*MAXROWS*MAXSTACKS)>>6)

#define RT_ANGLE 90
#define HALFRT_ANGLE 45
/* Store state of cell in top bit. Reserve low bits for count of living nbrs */
#define Set3D(x,y,z) SetMem(x,y,z,ON)
#define Reset3D(x,y,z) SetMem(x,y,z,OFF)

#define SetList3D(x,y,z) SetMem(x,y,z,ON),AddToList(x,y,z)

#define CellState3D(c) ((c)&ON)
#define CellNbrs3D(c) ((c)&0x1f) /* 26 <= 31 */

#define EyeToScreen 72.0               /* distance from eye to screen */
#define HalfScreenD 14.0               /* 1/2 the diameter of screen */
#define BUCKETSIZE 10
#define NBUCKETS ((MAXCOLUMNS+MAXROWS+MAXSTACKS)*BUCKETSIZE)
#define Distance(x1,y1,z1,x2,y2,z2) sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)+(z1-z2)*(z1-z2))

#define IP 0.0174533

#define MAXCOLORS 64
#define COLORBASE 3
#define COLORS (COLORBASE + 2)
#define COLORSTEP (MAXCOLORS/COLORBASE)   /* 33 different colors per side */

#define BLACK 0
#define RED 1
#define GREEN 2
#define BLUE 3
#define WHITE 4
#define TIMEOUT 25
#define NUMPTS  42

typedef struct _CellList {
  unsigned char x, y, z;    /* Location in world coordinates */
  char visible;           /* 1 if the cube is to be displayed */
  short dist;             /* dist from cell to eye */
  struct _CellList *next; /* pointer to next entry on linked list */
  struct _CellList *prev;
  struct _CellList *priority;
} CellList;

typedef struct {
  int initialized;
  int wireframe; /* FALSE */
  int ox, oy, oz; /* origin */
  double vx, vy, vz; /* viewpoint */
  int generation;
  long shooterTime;
  int nstacks, nrows, ncolumns;
  int memstart;
  char visible;
  unsigned char *base[BASESIZE];
  double A, B, C, F;
  int width, height;
  long int color[COLORS];
  int alt, azm;
  double dist;
  int living_min, living_max, newcell_min, newcell_max;
  CellList *ptrhead, *ptrend, eraserhead, eraserend;
  CellList *buckethead[NBUCKETS], *bucketend[NBUCKETS]; /* comfortable upper b*/
 } life3dstruct;

static life3dstruct life3ds[MAXSCREENS];

static int DrawCube();

static int patterns[][3 * NUMPTS + 1] = {
#if 0
/* Still life */
  {				/* V */
    -1, -1, -1, 0, -1, -1,
    -1, 0, -1, 0, 0, -1,

    -1, -1, 0, 0, -1, 0,
    127
  },
  {				/* CROSS */
    0, 0, -1,

    0, -1, 0,
    -1, 0, 0, 1, 0, 0,
    0, 1, 0,

    0, 0, 1,
    127
  },
  {				/* PILLAR */
    0, -1, -1,
    -1, 0, -1, 1, 0, -1,
    0, 1, -1,

    0, -1, 0,
    -1, 0, 0, 1, 0, 0,
    0, 1, 0,
    127
  },
#endif
  {                             /* BLINKER */
    0, 0, -1,

    0, -1, 0,
    -1, 0, 0, 1, 0, 0,
    0, 1, 0,

    -1, 0, 1, 1, 0, 1,
    127
  },
  {                             /* DOUBLEBLINKER */
    0, -1, -1,
    0, 1, -1,

    0, -1, 0,
    -1, 0, 0, 1, 0, 0,
    0, 1, 0,

    -1, 0, 1, 1, 0, 1,
    127
  },
  {                             /* TRIPLEBLINKER */
    -1, -1, -2,
    -1, 0, -2,

    -2, -1, -1,
    1, 0, -1,
    -1, 1, -1, 0, 1, -1,

    -1, -2, 0, 0, -2, 0,
    -2, -1, 0, 1, 0, 0,

    0, -1, 1,
    0, 0, 1,
    127
  },
  {                             /* THREEHALFSBLINKER */
    0, -1, -1,
    -1, 0, -1, 1, 0, -1,

    -1, -1, 0,
    1, 0, 0,
    -1, 1, 0, 0, 1, 0,

    0, -1, 1,
    0, 0, 1, 
    127
  },
  {                             /* PUFFER */
    0, -1, -1,
    0, 0, -1,

    0, -2, 0,
    -1, -1, 0, 1, -1, 0,
    -1, 0, 0, 1, 0, 0,
    0, 1, 0,
    127
  },
  {				/* PINWHEEL */
    -1, 1, -1, 0, 1, -1,

    -1, -1, 0, 0, -1, 0,
    -2, 0, 0, 1, 0, 0,

    -1, 0, 1, 0, 0, 1,
    -1, 1, 1, 0, 1, 1,
    127
  },
  {				/* HEART */
    -1, -1, -1,
    -1, 0, -1, 0, 0, -1,

    0, -1, 0,
    -2, 0, 0, 1, 0, 0,

    -1, -1, 1,
    -1, 1, 1, 0, 1, 1,
    127
  },
  {                             /* ARROW */
    0, -1, -1,
    0, 0, -1,

    0, -2, 0,
    1, -1, 0,
    1, 0, 0,
    0, 1, 0,

    -1, -1, 1,
    -1, 0, 1,
    127
  },
  {				/* ROTOR */
    0, -1, -1,
    0, 0, -1,

    0, -2, 0,
    -1, -1, 0, 1, -1, 0,
    -1, 0, 0, 1, 0, 0,
    0, 1, 0,

    0, -1, 1,
    0, 0, 1,
    127
  },
  {				/* BRONCO */
    0, -1, -1,
    0, 0, -1,

    0, -2, 0,
    -1, -1, 0, 1, -1, 0,
    -1, 0, 0, 1, 0, 0,
    0, 1, 0,

    -1, -1, 1,
    -1, 0, 1,
    127
  },
  {                             /* TRIPUMP */
    0, -2, -2,
    -2, -1, -2, -1, -1, -2, 0, -1, -2,

    0, -2, -1,
    -2, 0, -1,
    -2, 1, -1, -1, 1, -1,

    1, -2, 0,
    1, -1, 0,
    1, 0, 0,
    -1, 1, 0,

    0, 0, 1, 1, 0, 1,
    -1, 1, 1,
    127
  },
  {                             /* WINDSHIELDWIPER (HELICOPTER) */
    -2, -1, -2, -1, -1, -2,
    0, 0, -2,

    -1, -2, -1,
    -2, -1, -1,
    1, 0, -1,
    -1, 1, -1, 0, 1, -1, 1, 1, -1,

    0, -2, 0,
    -2, -1, 0, 1, -1, 0,

    0, -2, 1,
    0, -1, 1,
    0, 0, 1,
    127
  },
  {                             /* WALTZER */
    -2, -1, -1, -1, -1, -1, 0, -1, -1,
    -2, 0, -1,
    -1, 1, -1, 0, 1, -1,

    -2, -1, 0,
    -2, 0, 0, 1, 0, 0,
    1, 1, 0,

    -1, 0, 1, 0, 0, 1,
    0, 1, 1,
    127
  },
  {                             /* BIGWALTZER */
    0, -1, -1, 1, -1, -1,
    -1, 0, -1, 0, 0, -1,
    -1, 1, -1,

    0, -2, 0, 1, -2, 0,
    -2, 0, 0,
    -2, 1, 0,

    0, -1, 1, 1, -1, 1,
    -1, 0, 1, 0, 0, 1,
    -1, 1, 1,
    127
  }
};

#define NPATS (sizeof patterns / sizeof patterns[0])

/*--- list ---*/
/* initialise the state of all cells to OFF */
static void Init3D()
{
  life3dstruct *lp = &life3ds[screen];

  lp->ptrhead = lp->ptrend = NULL;
  lp->eraserhead.next = &lp->eraserend;
  lp->eraserend.prev = &lp->eraserhead;
}

static CellList* NewCell()
{
  return ((CellList *)malloc(sizeof(CellList)));
}

/* Function that adds the cell (assumed live) at (x,y,z) onto the search
 * list so that it is scanned in future generations
 */
static void AddToList(x,y,z)
unsigned int x, y, z;
{
  life3dstruct *lp = &life3ds[screen];
  CellList *tmp;

  tmp = NewCell();
  tmp->x = x; tmp->y = y; tmp->z = z;
  if (lp->ptrhead == NULL) {
    lp->ptrhead = lp->ptrend = tmp;
    tmp->prev = NULL;
  } else {
    lp->ptrend->next = tmp;
    tmp->prev = lp->ptrend;
    lp->ptrend = tmp;
  }
  lp->ptrend->next = NULL;
}

static void AddToEraseList(cell)
CellList *cell;
{
  life3dstruct *lp = &life3ds[screen];

  cell->next = &lp->eraserend;
  cell->prev = lp->eraserend.prev;
  lp->eraserend.prev->next = cell;
  lp->eraserend.prev = cell;
}

static void DelFromList(cell)
CellList *cell;
{
  life3dstruct *lp = &life3ds[screen];

  if (cell != lp->ptrhead)
    cell->prev->next = cell->next;
  else {
    lp->ptrhead = cell->next;
    if (lp->ptrhead != NULL)
      lp->ptrhead->prev=NULL;
  }

  if (cell != lp->ptrend)
    cell->next->prev = cell->prev;
  else {
    lp->ptrend = cell->prev;
    if (lp->ptrend != NULL)
      lp->ptrend->next = NULL;
  }

  AddToEraseList(cell);
}

static void DelFromEraseList(cell)
CellList *cell;
{
  cell->next->prev = cell->prev;
  cell->prev->next = cell->next;
  free(cell);
}

/*--- memory ---*/
/* Simulate a large array by dynamically allocating 4x4x4 size cells when
 * needed.
 */

static void MemInit()
{
  life3dstruct *lp = &life3ds[screen];
  int i;

  for (i = 0; i < BASESIZE; ++i)
    lp->base[i] = NULL;
  lp->memstart = 0;
}

static void BaseOffset(x, y, z, b, o)
unsigned int x, y, z;
int *b, *o;
{
  life3dstruct *lp = &life3ds[screen];
  int i;

  *b = ((x & 0x7c) << 7) + ((y & 0x7c) << 2) + ((z & 0x7c) >> 2);
  *o = (x & 3) + ((y & 3) << 2) + ((z & 3) << 4);

  if (lp->base[*b] == NULL) {
    lp->base[*b] = (unsigned char *)malloc(sizeof(unsigned char) * 64);
    for(i = 0; i < 64; ++i)
      lp->base[*b][i] = 0;
  }
}

static int GetMem(x, y, z)
unsigned int x, y, z;
{
  life3dstruct *lp = &life3ds[screen];
  int b, o;

  if (lp->memstart)
    MemInit();
  BaseOffset(x, y, z, &b, &o);
  return lp->base[b][o];
}

static void SetMem(x, y, z, val)
unsigned int x, y, z, val;
{
  life3dstruct *lp = &life3ds[screen];
  int b, o;

  if (lp->memstart)
    MemInit();

  BaseOffset(x, y, z, &b, &o);
  lp->base[b][o] = val;
}

static void ChangeMem(x, y, z, val)
unsigned int x, y, z, val;
{
  life3dstruct *lp = &life3ds[screen];
  int b, o;

  if (lp->memstart)
    MemInit();
  BaseOffset(x, y, z, &b, &o);
  lp->base[b][o] += val;
}

static void ClearMem()
{
  life3dstruct *lp = &life3ds[screen];
  int i, j, count;

  for (i = 0; i < BASESIZE; ++i)
    if (lp->base[i] != NULL) {
      for (count = j = 0; j < 64 && count == 0; ++j)
         if (CellState3D(lp->base[i][j]))
           ++count;
      if (!count) {
        free(lp->base[i]);
        lp->base[i] = NULL;
      }
    }
}


/* This routine increments the values stored in the 27 cells centred on (x,y,z)
 * Note that the offset() macro implements wrapping - the world is a torus.
 */
static void IncrementNbrs3D(cell)
CellList *cell;
{
  int xc, yc, zc, x, y, z;

  xc = cell->x; yc = cell->y; zc = cell->z;
  for (z = zc - 1; z != zc + 2; ++z)
    for (y = yc - 1; y != yc + 2; ++y)
      for (x = xc - 1; x != xc + 2; ++x)
	if (x != xc || y != yc || z != zc)
          ChangeMem(x, y, z, 1);
}

static void End3D()
{
  life3dstruct *lp = &life3ds[screen];
  CellList *ptr;

  while (lp->ptrhead != NULL) {
    SetMem(lp->ptrhead->x, lp->ptrhead->y, lp->ptrhead->z, OFF);
    DelFromList(lp->ptrhead);
  }
  ptr = lp->eraserhead.next;
  while (ptr != &lp->eraserend) {
    DelFromEraseList(ptr);
    ptr = lp->eraserhead.next;
  }
}

static void RunLife3D()
{
  life3dstruct *lp = &life3ds[screen];
  unsigned int x, y, z, xc, yc, zc;
  int c;
  CellList *ptr, *ptrnextcell;

  /* Step 1 - Add 1 to all neighbours of living cells. */
  ptr = lp->ptrhead;
  while (ptr != NULL) {
    IncrementNbrs3D(ptr);
    ptr=ptr->next;
  }

  /* Step 2 - Scan world and implement Survival rules. We have a list of live
   * cells, so do the following:
   * Start at the END of the list and work backwards (so we don't have to worry
   * about scanning newly created cells since they are appended to the end) and
   * for every entry, scan its neighbours for new live cells. If found, add them
   * to the end of the list. If the centre cell is dead, unlink it.
   * Make sure we do not append multiple copies of cells.
   */
  ptr=lp->ptrend;
  while (ptr != NULL) {
    ptrnextcell = ptr->prev;
    xc = ptr->x; yc = ptr->y; zc = ptr->z;
    for (z = zc - 1; z != zc + 2; ++z)
      for (y = yc - 1; y != yc + 2; ++y)
        for (x = xc - 1; x != xc + 2; ++x)
          if (x != xc || y != yc || z != zc)
            if ((c = GetMem(x, y, z))) {
              if (CellState3D(c) == OFF) {
                if (CellNbrs3D(c) >= lp->newcell_min &&
                    CellNbrs3D(c) <= lp->newcell_max)
                  SetList3D(x, y, z);
                else
                  Reset3D(x, y, z);
              }
            }
    c = GetMem(xc, yc, zc);
    if (CellNbrs3D(c) < lp->living_min || CellNbrs3D(c) > lp->living_max) {
      SetMem(ptr->x, ptr->y, ptr->z, OFF);
      DelFromList(ptr);
    } else
      Set3D(xc, yc, zc);
    ptr = ptrnextcell;
  }
  ClearMem();
}

#ifdef DEBUG
static int CountCells3D()
{
  life3dstruct *lp = &life3ds[screen];
  CellList *ptr;
  int count = 0;

  ptr = lp->ptrhead;
  while(ptr != NULL) {
    ++count;
    ptr = ptr->next;
  }
  return count;
}

void DisplayList()
{
  life3dstruct *lp = &life3ds[screen];
  CellList *ptr;
  int count = 0;

  ptr=lp->ptrhead;
  while (ptr != NULL) {
    (void) printf("(%x)=[%d,%d,%d] ", (int)ptr, ptr->x, ptr->y, ptr->z);
    ptr = ptr->next;
    ++count;
  }
  (void) printf("Living cells = %d\n", count);
}
#endif

static void RandomSoup(n, v)
int n, v;
{
  life3dstruct *lp = &life3ds[screen];
  unsigned int x, y, z;

  v /= 2;
  if (v < 1)
    v = 1;
  for (z = lp->nstacks / 2 - v; z < lp->nstacks / 2 + v; ++z)
    for (y = lp->nrows / 2 - v; y < lp->nrows / 2 + v; ++y)
      for (x = lp->ncolumns / 2 - v; x < lp->ncolumns / 2 + v; ++x)
        if ((RAND() % 100) < n)
          SetList3D(x, y, z);
}

static void GetPattern(i)
int i;
{
  life3dstruct *lp = &life3ds[screen];
  int x, y, z;
  int *patptr;

  patptr = &patterns[i][0];
  while ((x = *patptr++) != 127) {
    y = *patptr++;
    z = *patptr++;
    x += lp->ncolumns / 2;
    y += lp->nrows / 2;
    z += lp->nstacks / 2;
    if (x >= 0 && y >= 0 && z >= 0 &&
        x < lp->ncolumns && y < lp->nrows && z < lp->nstacks)
      SetList3D(x, y, z);
  }
}

static void NewViewpoint(x, y, z)
double x, y, z;
{
  life3dstruct *lp = &life3ds[screen];
  double k, l, d1, d2;

  k = x * x + y * y;
  l = sqrt(k + z * z);
  k = sqrt(k);
  d1 = (EyeToScreen / HalfScreenD);
  d2 = EyeToScreen / (HalfScreenD * lp->height / lp->width);
  lp->A = d1 * l * (lp->width / 2) / k;
  lp->B = l * l;
  lp->C = d2 * (lp->height / 2) / k;
  lp->F = k * k;
}

static void NewPoint(x, y, z, cubepts)
double x, y, z;
register XPoint *cubepts;
{
  life3dstruct *lp = &life3ds[screen];
  double p1, E;

  p1 = x * lp->vx + y * lp->vy;
  E = lp->B - p1 - z * lp->vz;
  cubepts->x = (int)(lp->width / 2 - lp->A * (lp->vx * y - lp->vy * x) / E);
  cubepts->y = (int)(lp->height / 2 - lp->C * (z * lp->F - lp->vz * p1) / E);
}


/* Chain together all cells that are at the same distance. These
 * cannot mutually overlap.
 */
static void SortList()
{
  life3dstruct *lp = &life3ds[screen];
  short dist;
  double d, x, y, z, rsize;
  int i, r;
  XPoint point;
  CellList *ptr;

  for (i = 0; i < NBUCKETS; ++i)
    lp->buckethead[i] = lp->bucketend[i] = NULL;

  /* Calculate distances and re-arrange pointers to chain off buckets */
  ptr = lp->ptrhead;
  while (ptr != NULL) {

    x = (double) ptr->x - lp->ox;
    y = (double) ptr->y - lp->oy;
    z = (double) ptr->z - lp->oz;
    d = Distance(lp->vx, lp->vy, lp->vz, x, y, z);
    if (lp->vx * (lp->vx - x) + lp->vy * (lp->vy - y) +
        lp->vz * (lp->vz - z) > 0 && d > 1.5)
      ptr->visible = 1;
    else
      ptr->visible = 0;

    ptr->dist = (short)d;
    dist = (short)(d * BUCKETSIZE);
    if (dist > NBUCKETS - 1)
      dist = NBUCKETS - 1;

    if (lp->buckethead[dist] == NULL) {
      lp->buckethead[dist] = lp->bucketend[dist] = ptr;
      ptr->priority = NULL;
    } else {
      lp->bucketend[dist]->priority = ptr;
      lp->bucketend[dist] = ptr;
      lp->bucketend[dist]->priority = NULL;
    }
    ptr = ptr->next;
  }

  /* Check for invisibility */
  rsize = 0.47 * lp->width / ((double)HalfScreenD * 2);
  i = lp->azm;
  if (i < 0)
    i = -i;
  i = i % RT_ANGLE;
  if (i > HALFRT_ANGLE)
    i = RT_ANGLE - i;
  rsize /= cos(i * IP);

  lp->visible = 0;
  for(i = 0; i < NBUCKETS; ++i)
    if (lp->buckethead[i] != NULL) {
      ptr = lp->buckethead[i];
      while (ptr != NULL) {
        if (ptr->visible) {
          x = (double) ptr->x - lp->ox;
          y = (double) ptr->y - lp->oy;
          z = (double) ptr->z - lp->oz;
          NewPoint(x, y, z, &point);

          r = (int)(rsize * (double)EyeToScreen / (double)ptr->dist);
          if (point.x + r >= 0 && point.y + r >= 0 &&
              point.x - r < lp->width && point.y - r < lp->height)
            lp->visible = 1;
        } 
        ptr = ptr->priority;
      }
    }
}

static void DrawFace(win, color, cubepts, p1, p2, p3, p4)
Window win;
int color;
XPoint cubepts[];
int p1, p2, p3, p4;
{
  life3dstruct *lp = &life3ds[screen];
  XPoint facepts[5];

  facepts[0] = cubepts[p1];
  facepts[1] = cubepts[p2];
  facepts[2] = cubepts[p3];
  facepts[3] = cubepts[p4];
  facepts[4] = cubepts[p1];

  if (!lp->wireframe) {
    XSetForeground(dsp, Scr[screen].gc, lp->color[color]);
    XFillPolygon(dsp, win, Scr[screen].gc, facepts, 4, Convex, CoordModeOrigin);
  }
  if (color == BLACK || ((mono || Scr[screen].npixels <= 2) && !lp->wireframe))
    XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
  else
    XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
  XDrawLines(dsp, win, Scr[screen].gc, facepts, 5, CoordModeOrigin);
}

#define LEN 0.45
#define LEN2 0.9

static int DrawCube(win, cell)
Window win;
CellList *cell;
{
  life3dstruct *lp = &life3ds[screen];
  XPoint cubepts[8];        /* screen coords for point */
  int i = 0, out;
  unsigned int mask;
  double x, y, z;
  double dx, dy, dz;

  x = (double) cell->x - lp->ox;
  y = (double) cell->y - lp->oy;
  z = (double) cell->z - lp->oz;
  out = 0;
  for(dz = z - LEN; dz <= z + LEN2; dz += LEN2)
    for(dy = y - LEN; dy <= y + LEN2; dy += LEN2)
      for(dx = x - LEN; dx <= x + LEN2; dx += LEN2) {
        NewPoint(dx, dy, dz, &cubepts[i]);
        if (cubepts[i].x < 0 || cubepts[i].x >= lp->width ||
            cubepts[i].y < 0 || cubepts[i].y >= lp->height)
          ++out;
        ++i;
      }
  if (out == 8)
    return(0);

  if (cell->visible)
    mask = 0xFFFF;
  else
    mask = 0x0;

  /* Only draw those faces that are visible */
  dx = lp->vx - x; dy = lp->vy - y; dz = lp->vz - z;
  if (dz > LEN)
    DrawFace(win, BLUE & mask, cubepts, 4, 5, 7, 6);
  else if (dz < -LEN)
    DrawFace(win, BLUE & mask, cubepts, 0, 1, 3, 2);
  if (dx > LEN)
    DrawFace(win, GREEN & mask, cubepts, 1, 3, 7, 5);
  else if (dx < -LEN)
    DrawFace(win, GREEN & mask, cubepts, 0, 2, 6, 4);
  if (dy > LEN)
    DrawFace(win, RED & mask, cubepts, 2, 3, 7, 6);
  else if (dy < -LEN)
    DrawFace(win, RED & mask, cubepts, 0, 1, 5, 4);
  return(1);
}

static void DrawScreen(win)
Window win;
{
  life3dstruct *lp = &life3ds[screen];
  CellList *ptr;
  CellList *eraserptr;
  int i;
#ifdef DEBUG
  int count = 0, v = 0;
#endif

  SortList();

  /* Erase dead cubes */
  eraserptr = lp->eraserhead.next;
  while (eraserptr != &lp->eraserend) {
    eraserptr->visible = 0;
    (void) DrawCube(win, eraserptr);
    DelFromEraseList(eraserptr);
    eraserptr = lp->eraserhead.next;
  }

  /* draw furthest cubes first */
  for (i = NBUCKETS - 1; i >= 0; --i) {
    ptr = lp->buckethead[i];
    while (ptr != NULL) {
      /*if (ptr->visible)*/
        /* v += */(void) DrawCube(win, ptr);
      ptr = ptr->priority;
      /* ++count; */
    }
  }
#ifdef DEBUG
  (void) printf("Pop=%-4d  Viewpoint (%3d,%3d,%3d)  Origin (%3d,%3d,%3d)  Mode %dx%d\
(%d,%d) %d\n",
     count, (int)(lp->vx + lp->ox), (int)(lp->vy + lp->oy),
     (int)(lp->vz + lp->oz), (int)lp->ox,(int)lp->oy,(int)lp->oz,
     lp->width, lp->height, lp->alt, lp->azm, v);
#endif
}

void initlife3d(win)
Window win;
{
  XWindowAttributes xgwa;
  life3dstruct *lp = &life3ds[screen];
  int i;

  lp->generation = 0;
  lp->shooterTime = seconds();

  if (!lp->initialized) {
    /* 4555 life
       (5766 life is good too (has lifeforms and a glider similar to life)
       also 5655 and 6767 are interesting) */
    lp->living_min = 4;
    lp->living_max = 5;
    lp->newcell_min = 5;
    lp->newcell_max = 5;
    lp->dist = 50.0/*30.0*/;
    lp->alt = 20 /*30*/;
    lp->azm = 10 /*30*/;
    lp->ncolumns = MAXCOLUMNS;
    lp->nrows = MAXROWS;
    lp->nstacks = MAXSTACKS;
    lp->ox = lp->ncolumns / 2;
    lp->oy = lp->nrows / 2;
    lp->oz = lp->nstacks / 2;
    lp->wireframe = 0;
    lp->initialized = 1;
    Init3D();
  } else
    End3D();
  (void) XGetWindowAttributes(dsp, win, &xgwa);
  lp->color[0] = BlackPixel(dsp, screen);
  if (!mono && Scr[screen].npixels > 2) {
    i = RAND() % 3;

    lp->color[i + 1] = Scr[screen].pixels[RAND() %
      (Scr[screen].npixels / COLORBASE)];
    lp->color[(i + 1) % 3 + 1] = Scr[screen].pixels[RAND() %
      (Scr[screen].npixels / COLORBASE) + Scr[screen].npixels / COLORBASE];
    lp->color[(i + 2) % 3 + 1] = Scr[screen].pixels[RAND() %
      (Scr[screen].npixels / COLORBASE) + 2 * Scr[screen].npixels / COLORBASE];
  } else
    lp->color[1] = lp->color[2] = lp->color[3] = WhitePixel(dsp, screen);
  lp->color[4] = WhitePixel(dsp, screen);
  lp->width = xgwa.width;
  lp->height = xgwa.height;
  lp->memstart = 1;
  /*lp->tablesMade = 0;*/

  XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
  XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, lp->width, lp->height);

  if (lp->alt > 89)
    lp->alt = 89;
  else if (lp->alt < -89)
    lp->alt = -89;
  /* Calculate viewpoint */
  lp->vx = (sin(lp->azm * IP) * cos(lp->alt * IP) * lp->dist);
  lp->vy = (cos(lp->azm * IP) * cos(lp->alt * IP) * lp->dist);
  lp->vz = (sin(lp->alt * IP) * lp->dist);
  NewViewpoint(lp->vx, lp->vy, lp->vz);

  i = RAND() % (NPATS + 2);
  if (i >= NPATS)
    RandomSoup(30, 10);
  else
    GetPattern(i);

  DrawScreen(win);
}

static void shooter()
{
    int hsp, vsp, asp, hoff = 1, voff = 1, aoff = 1, r, c2, r2, s2;
    life3dstruct *lp = &life3ds[screen];

    /* Generate the glider at the edge of the screen */
#define V 10
#define V2 (V/2)
    c2 = lp->ncolumns / 2;
    r2 = lp->nrows / 2;
    s2 = lp->nstacks / 2;
    r = RAND() % 3;
    if (!r) {
      hsp = RAND() % V2 + c2  - V2 / 2;
      vsp = (RAND() % 2) ? r2 - V : r2 + V;
      asp = (RAND() % 2) ? s2 - V : s2 + V;
      if (asp > s2)
        aoff = -1;
      if (vsp > r2)
        voff = -1;
      if (hsp > c2)
        hoff = -1;
      SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 0 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 0 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 0 * aoff);
      SetList3D(hsp + 2 * hoff, vsp + 2 * voff, asp + 0 * aoff);
      SetList3D(hsp + 3 * hoff, vsp + 0 * voff, asp + 0 * aoff);
      SetList3D(hsp + 3 * hoff, vsp + 1 * voff, asp + 0 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 1 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
      SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 1 * aoff);
      SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 1 * aoff);
    } else if (r == 1) {
      hsp = (RAND() % 2) ? c2 - V : c2 + V;
      vsp = (RAND() % 2) ? r2 - V : r2 + V;
      asp = RAND() % V2 + s2 - V2 / 2;
      if (asp > s2)
        aoff = -1;
      if (vsp > r2)
        voff = -1;
      if (hsp > c2)
        hoff = -1;
      SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 0 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 0 * aoff);
      SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 1 * aoff);
      SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 2 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 3 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 3 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 1 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 2 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 2 * aoff);
    } else {
      hsp = (RAND() % 2) ? c2 - V : c2 + V;
      vsp = RAND() % V2 + r2 - V2 / 2;
      asp = (RAND() % 2) ? s2 - V : s2 + V;
      if (asp > s2)
        aoff = -1;
      if (vsp > r2)
        voff = -1;
      if (hsp > c2)
        hoff = -1;
      SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 0 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 1 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 2 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 2 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 3 * voff, asp + 0 * aoff);
      SetList3D(hsp + 0 * hoff, vsp + 3 * voff, asp + 1 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 0 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 0 * aoff);
      SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 1 * aoff);
    }
    lp->shooterTime = seconds();
}

void drawlife3d(win)
Window win;
{
  life3dstruct *lp = &life3ds[screen];

  RunLife3D();
  DrawScreen(win);

  if (++lp->generation > batchcount || !lp->visible)
     /*CountCells3D() == 0)*/
    initlife3d(win);

    /*
     * generate a randomized shooter aimed roughly toward the center of the
     * screen after timeout.
     */

    if (seconds() - lp->shooterTime > TIMEOUT)  
        shooter();
}
