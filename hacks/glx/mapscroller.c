/* mapscroller, Copyright © 2021-2023 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This displays a slowly-scrolling map of a random place on Earth, using
 * data from openstreetmap.org or compatible services.
 *
 * Network access and management of the image file cache happens in the
 * mapscroller.pl helper program, since doing https from C code is untenable.
 * Sadly, this division of labor means that this program won't work on iOS or
 * Android.
 *
 * If we wanted to get this working on iOS and Android, it might be easier to
 * just rewrite the whole thing as a webview running https://openlayers.org/ -
 * it would probably be 50 lines of code.
 * 
 * Created: 14-Dec-2021.
 */

#define DEFAULTS	"*delay:	 30000            \n" \
			"*showFPS:       False            \n" \
			"*wireframe:     False            \n" \
			"*titleFont:     sans-serif 18"  "\n" \
			"*loaderProgram: mapscroller.pl" "\n" \
			"*cacheSize:     20MB"           "\n" \
			"*texFontOmitDropShadow: True     \n" \
			"*suppressRotationAnimation: True \n" \

# define release_map 0
# undef DEBUG_TEXTURE

#include "xlockmore.h"
#include "ximage-loader.h"
#include "texfont.h"
#include "../images/gen/oceantiles_12_png.h"

#ifdef USE_GL /* whole file */

#ifndef HAVE_COCOA
# include <X11/Intrinsic.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define DEF_URL_TEMPLATE "(default)"
#define DEF_MAP_LEVEL    "15"
#define DEF_SPEED        "1.0"
#define DEF_DURATION     "1800"
#define DEF_ORIGIN       "random-city"
#define DEF_TITLES       "True"
#define DEF_ARROW        "True"
#define DEF_VERBOSE      "0"

#define FALLBACK_URL "https://{a-c}.tile.openstreetmap.org/{z}/{x}/{y}.png"
#define TILE_PIXEL_SIZE 256
#define MIN_LEVEL 5
#define MAX_LEVEL 18  /* Some servers go to 19, but not all */

typedef struct { double lat, lon; } LL;
typedef struct { double x, y; } XY;
typedef struct { long x, y; } XYi;

static const LL cities[] = {
# include "mapcities.h"
};

typedef struct {
  int id;
  int map_level;
  XYi grid;
  XYi map;
  enum { BLANK, LOADING, OK, FAILED, RETRY } status;
  int retries;
  GLuint texid;   /* Non-zero if we have the texture image data */
  GLfloat opacity;
} tile;

typedef struct {
  GLXContext *glx_context;
  char *url_template;
  int map_level;
  GLfloat speed;
  int duration;
  LL pos;
  XY heading[3];
  double heading_ratio;
  int grid_w, grid_h;
  tile *tiles;
  texture_font_data *font_data;
  XImage *oceans;
  Bool ocean_p;

  enum { FADE_OUT, RUN } mode;
  time_t change_time;
  double opacity;

  pid_t pid;
  XtInputId pipe_id;
  int pipe_in, pipe_out;
  Bool input_available_p;

  Bool button_down_p;
  LL drag_start_deg;
  XYi drag_start_px;
  long tile_count;
  Bool dead_p;

} map_configuration;

static map_configuration *bps = NULL;

static char *url_template_arg;
static int map_level_arg;
static GLfloat speed_arg;
static int duration_arg;
static char *origin_arg;
static Bool do_titles;
static Bool do_arrow;
static int verbose_p;

static XrmOptionDescRec opts[] = {
  { "-url-template", ".urlTemplate", XrmoptionSepArg, 0 },
  { "-level",        ".mapLevel",    XrmoptionSepArg, 0 },
  { "-speed",        ".speed",       XrmoptionSepArg, 0 },
  { "-duration",     ".duration",    XrmoptionSepArg, 0 },
  { "-origin",       ".origin",      XrmoptionSepArg, 0 },
  { "-titles",	     ".titles",      XrmoptionNoArg, "True" },
  { "+titles",	     ".titles",      XrmoptionNoArg, "False" },
  { "-arrow",	     ".arrow",       XrmoptionNoArg, "True" },
  { "+arrow",	     ".arrow",       XrmoptionNoArg, "False" },
  { "-verbose",	     ".verbose",     XrmoptionNoArg, "1" },
  { "-v",	     ".verbose",     XrmoptionNoArg, "1" },
  { "-vv",	     ".verbose",     XrmoptionNoArg, "2" },
  { "-vvv",	     ".verbose",     XrmoptionNoArg, "3" },
  { "-vvvv",	     ".verbose",     XrmoptionNoArg, "4" },
  { "-vvvvv",	     ".verbose",     XrmoptionNoArg, "5" },
  { "-vvvvvv",	     ".verbose",     XrmoptionNoArg, "6" },
  { "-quiet",	     ".verbose",     XrmoptionNoArg, "0" },
};

static argtype vars[] = {
  {&url_template_arg, "urlTemplate", "URLTemplate", DEF_URL_TEMPLATE, t_String},
  {&map_level_arg,    "mapLevel",    "MapLevel",    DEF_MAP_LEVEL,    t_Int},
  {&speed_arg,        "speed",       "Speed",       DEF_SPEED,        t_Float},
  {&duration_arg,     "duration",    "Duration",    DEF_DURATION,     t_Int},
  {&origin_arg,       "origin",      "Origin",      DEF_ORIGIN,       t_String},
  {&do_titles,        "titles",      "Titles",      DEF_TITLES,       t_Bool},
  {&do_arrow,         "arrow",       "Arrow",       DEF_ARROW,        t_Bool},
  {&verbose_p,        "verbose",     "Verbose",     DEF_VERBOSE,      t_Int},
};

ENTRYPOINT ModeSpecOpt map_opts = {countof(opts), opts, countof(vars), vars, NULL};


static const char *
blurb (ModeInfo *mi)
{
  static char buf[255];
  time_t now = time ((time_t *) 0);
  char *ct = (char *) ctime (&now);
  int n = strlen(progname);
  if (n > 100) n = 99;
  strncpy(buf, progname, n);
  buf[n++] = ':';
  buf[n++] = ' ';
  strncpy(buf+n, ct+11, 8);
  sprintf(buf+n+8, ": %lu", (unsigned long) getpid());
  return buf;
}


static int
lon2tilex (double lon, int z)
{ 
  return (int) (floor ((lon + 180.0) / 360.0 * (1 << z))); 
}

static int
lat2tiley (double lat, int z)
{ 
  double latrad = lat * M_PI/180.0;
  return (int) (floor ((1.0 - asinh(tan(latrad)) / M_PI) / 2.0 * (1 << z))); 
}

static double
tilex2lon (int x, int z)
{
  return (x / (double) (1 << z) * 360.0 - 180);
}

static double
tiley2lat (int y, int z)
{
  double n = M_PI - 2.0 * M_PI * y / (double) (1 << z);
  return (180.0 / M_PI * atan (0.5 * (exp(n) - exp(-n))));
}


static Bool
constrain_mercator (LL *ll)
{
  Bool changed = False;
  if (ll->lat >=  85) changed = True, ll->lat =  85;  /* Mercator max */
  if (ll->lat <= -85) changed = True, ll->lat = -85;
  while (ll->lon >   180) ll->lon -= 180;
  while (ll->lon <= -180) ll->lon += 180;
  return changed;
}


/* oceantiles_12.png contains one 2-bit pixel for each level 12 tile
   (4096 x 4096 tiles).  The colors mean:
   white - coastline intersects with this tile
   green - no coastline intersect, land tile
   blue  - no coastline intersect, sea tile
   black - unknown
 */
static Bool
ocean_tile_p (ModeInfo *mi, double lat, double lon)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  int z = 12;
  int x = lon2tilex (lon, z);
  int y = lat2tiley (lat, z);
  unsigned long p = ((x >= 0 && y >= 0 &&
                      x < bp->oceans->width && 
                      y < bp->oceans->height)
                     ? XGetPixel (bp->oceans, x, bp->oceans->height-y-1)
                     : 0);
  /* #### Why is this ABGR instead of RGBA? */
  return (p == 0xFFFF0000);  /* blue */
  return False;
}


static Bool
mostly_ocean_p (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  double deg = 360 / exp2(bp->map_level);
  /* Check center and the 8 tiles around it */
  return (ocean_tile_p (mi, bp->pos.lat,       bp->pos.lon      ) &&
          ocean_tile_p (mi, bp->pos.lat - deg, bp->pos.lon - deg) &&
          ocean_tile_p (mi, bp->pos.lat - deg, bp->pos.lon      ) &&
          ocean_tile_p (mi, bp->pos.lat - deg, bp->pos.lon + deg) &&
          ocean_tile_p (mi, bp->pos.lat,       bp->pos.lon - deg) &&
          ocean_tile_p (mi, bp->pos.lat,       bp->pos.lon + deg) &&
          ocean_tile_p (mi, bp->pos.lat + deg, bp->pos.lon - deg) &&
          ocean_tile_p (mi, bp->pos.lat + deg, bp->pos.lon      ) &&
          ocean_tile_p (mi, bp->pos.lat + deg, bp->pos.lon + deg));
}


static void
free_tiles (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  for (i = 0; i < bp->grid_w * bp->grid_h; i++)
    if (bp->tiles[i].texid)
      glDeleteTextures (1, &bp->tiles[i].texid);
  free (bp->tiles);
  bp->tiles = 0;
}


/* qsort comparator for sorting tiles from center out */
static XYi cmp_tiles_ctr;
static int
cmp_tiles (const void *aa, const void *bb)
{
  const tile *a = *(const tile **) aa;
  const tile *b = *(const tile **) bb;
  long adx = cmp_tiles_ctr.x - a->grid.x;
  long ady = cmp_tiles_ctr.y - a->grid.y;
  long bdx = cmp_tiles_ctr.x - b->grid.x;
  long bdy = cmp_tiles_ctr.y - b->grid.y;
  long adist2 = adx*adx + ady*ady;  /* No need for sqrt */
  long bdist2 = bdx*bdx + bdy*bdy;
  return (int) (adist2 - bdist2);
}


static void
reshape_tiles (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  int tile_w = TILE_PIXEL_SIZE;
  int tile_h = TILE_PIXEL_SIZE;
  int x, y, i, j;
  int ow = bp->grid_w;
  int oh = bp->grid_h;
  tile *otiles = bp->tiles;
  XYi center_tile;
  XYi topleft_tile;
  long map_w = exp2 (bp->map_level);
  long map_h = map_w;
  static int tick = 0;

  /* Two tile border around viewport, and round up. */
  int w2 = (MI_WIDTH(mi)  / (double) tile_w) + 4 + 0.5;
  int h2 = (MI_HEIGHT(mi) / (double) tile_h) + 4 + 0.5;

  center_tile.x  = lon2tilex (bp->pos.lon, bp->map_level);
  center_tile.y  = lat2tiley (bp->pos.lat, bp->map_level);
  topleft_tile.x = center_tile.x - w2/2;
  topleft_tile.y = center_tile.y - h2/2;

  bp->grid_w = w2;
  bp->grid_h = h2;
  bp->tiles = (tile *) calloc (w2 * (h2 + 1), sizeof (*bp->tiles));

  /* Generate blank tiles for our current grid. */
  for (y = 0; y < h2; y++)
    for (x = 0; x < w2; x++)
      {
        tile *t = &bp->tiles[y * w2 + x];
        t->id = -1;
        t->map_level = bp->map_level;
        t->map.x = topleft_tile.x + x;
        t->map.y = topleft_tile.y + y;
        t->grid.x = x;
        t->grid.y = y;
        while (t->map.x < 0) t->map.x += map_w;
        while (t->map.y < 0) t->map.y += map_h;
        while (t->map.x >= map_w) t->map.x -= map_w;
        while (t->map.y >= map_h) t->map.y -= map_h;
      }


  /* If any of the old tiles have the same map coordinates, preserve
     their contents (images and textures) */
  for (i = 0; i < ow * oh; i++)
    {
      for (j = 0; j < w2 * h2; j++)
        {
          if (otiles[i].id > 0 &&
              bp->tiles[j].id <= 0 &&
              otiles[i].map.x == bp->tiles[j].map.x &&
              otiles[i].map.y == bp->tiles[j].map.y)
            {
              XYi ogrid = bp->tiles[j].grid;
              bp->tiles[j] = otiles[i];  /* copy whole struct */
              otiles[i].id = -1;
              otiles[i].texid = 0;
              bp->tiles[j].grid = ogrid;
            }
        }
    }

  /* Give IDs to the retained new tiles */
  for (i = 0; i < w2 * h2; i++)
    if (bp->tiles[i].id < 0)
      bp->tiles[i].id = ++tick;

  /* Queue a loader for any tiles that don't have them.
     Enqueue them from the center out, rather than left to right. */
  if (! bp->dead_p)
  {
    tile **queue = (tile **) malloc (w2 * h2 * sizeof(*queue));
    int count = 0;
    for (i = 0; i < w2 * h2; i++)
      {
        tile *t = &bp->tiles[i];
        if (t->status == BLANK ||
            (t->status == RETRY && t->retries < 3))
          {
            queue[count++] = t;
            if (t->status == RETRY)
              {
                t->retries++;
                if (verbose_p)
                  fprintf (stderr,
                       "%s: retrying tile: %ld %ld %d (%d) pos: %.4f, %.4f\n",
                           blurb(mi), t->map.x, t->map.y, t->map_level,
                           t->retries,
                           tiley2lat (t->map.y, t->map_level),
                           tilex2lon (t->map.x, t->map_level));
              }
          }
      }

    cmp_tiles_ctr.x = bp->grid_w / 2;  /* Because qsort_r is not portable */
    cmp_tiles_ctr.y = bp->grid_h / 2;
    qsort (queue, count, sizeof(*queue), cmp_tiles);

    for (i = 0; i < count; i++)
      {
        tile *t = queue[i];
        int L;
        char buf[1024];
        t->status = LOADING;
        sprintf (buf, "%ld %ld %d\n", t->map.x, t->map.y, t->map_level);
        L = strlen (buf);
        if (L != write (bp->pipe_out, buf, L))
          {
            sprintf (buf, "%.100s: write", blurb(mi));
            perror (buf);
            exit (1);
          }
        if (verbose_p > 1)
         fprintf (stderr, "%s: requesting tile %s", blurb(mi), buf);
      }
    free (queue);
  }

  /* Free textures of any now-unused tiles. */
  for (i = 0; i < ow * oh; i++)
    if (otiles[i].texid)
      glDeleteTextures (1, &otiles[i].texid);

  free (otiles);
}


static void
draw_tile (ModeInfo *mi, tile *t)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  double w = TILE_PIXEL_SIZE;

  if (t->texid) mi->polygon_count++;

  if (! wire)
    {
      if  (t->texid && t->status != OK) abort();
      if (!t->texid && t->status == OK) abort();

      glBindTexture (GL_TEXTURE_2D, t->texid);

      /* Must be after glBindTexture */
      glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

  if (wire)
    glColor3f (0, 1, 0);
  else if (!t->texid)
    glColor4f (0, 0, 0, 0.03 * bp->opacity);  /* grid color */
  else
    {
      glColor4f (1, 1, 1, t->opacity * bp->opacity);
      t->opacity += 0.1;
      if (t->opacity > 1) t->opacity = 1;
    }

  glFrontFace (GL_CCW);
  glBegin (wire || !t->texid ? GL_LINE_LOOP : GL_QUADS);
  glTexCoord2f (0, 0); glVertex3f (0, 0, 0);
  glTexCoord2f (1, 0); glVertex3f (w, 0, 0);
  glTexCoord2f (1, 1); glVertex3f (w, w, 0);
  glTexCoord2f (0, 1); glVertex3f (0, w, 0);
  glEnd();

  if (t->status == FAILED || t->status == RETRY)   /* X them out */
    {
      GLfloat s = 0.15;
      if (!wire)
        {
          glDisable (GL_TEXTURE_2D);
          glColor4f (0, 0, 0, 0.2 * bp->opacity);  /* line color */
        }
      glPushMatrix();
      glTranslatef (w/2, w/2, 0);
      glScalef (s, s, s);
      glTranslatef (-w/2, -w/2, 0);
      glBegin (GL_LINES);
      glVertex3f (0, 0, 0); glVertex3f (w, w, 0);
      glVertex3f (w, 0, 0); glVertex3f (0, w, 0);
      glEnd();
      glPopMatrix();
      if (!wire)
        glEnable (GL_TEXTURE_2D);
    }

  if (wire)
    {
      char buf[1024];
      XCharStruct c;
      int ascent, descent;

      if (! t->texid)
        {
          glColor3f (0, 0.5, 0);
          glBegin (GL_LINES);
          glVertex3f (0, 0, 0);
          glVertex3f (w, w, 0);
          glVertex3f (w, 0, 0);
          glVertex3f (0, w, 0);
          glEnd();
        }

      sprintf (buf,
               "%ld x %ld @ %d\n"
               "%.3f\xC2\xB0, %.3f\xC2\xB0\n\n"
               "%d",
               t->map.x, t->map.y, t->map_level,
               tiley2lat (t->map.y, t->map_level),
               tilex2lon (t->map.x, t->map_level),
               t->id);
      texture_string_metrics (bp->font_data, buf, &c, &ascent, &descent);
      glPushMatrix();
      glTranslatef (ascent / 2, w - (ascent + descent), 0);
      glColor3f (1, 1, 0);
      print_texture_string (bp->font_data, buf);
      glPopMatrix();
    }
}


static void
draw_tiles (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  int x, y;

  /* Grid has enough cells to cover the screen plus a 2 tile border.
     We want 'pos' at the center of the screen, which means its tile
     will contain the center point.
   */
  XY pos0;
  LL tile0, tile1;
  XY off;

  /* What tile contains pos? */
  pos0.x = lon2tilex (bp->pos.lon, bp->map_level);
  pos0.y = lat2tiley (bp->pos.lat, bp->map_level);

  /* What is that tile's origin and extent? */
  tile0.lon = tilex2lon (pos0.x,   bp->map_level);
  tile0.lat = tiley2lat (pos0.y,   bp->map_level);
  tile1.lon = tilex2lon (pos0.x+1, bp->map_level);
  tile1.lat = tiley2lat (pos0.y+1, bp->map_level);

  /* How far should the tile be scrolled? */
  off.x =  (bp->pos.lon - tile0.lon) / (tile1.lon - tile0.lon);
  off.y = -(bp->pos.lat - tile0.lat) / (tile1.lat - tile0.lat);
  off.x *= TILE_PIXEL_SIZE;
  off.y *= TILE_PIXEL_SIZE;

  /* And center */
  off.x += bp->grid_w / 2.0 * TILE_PIXEL_SIZE - TILE_PIXEL_SIZE/2;
  off.y -= bp->grid_h / 2.0 * TILE_PIXEL_SIZE - TILE_PIXEL_SIZE/2;

  glPushMatrix();
  glScalef (1.0 / MI_WIDTH(mi),
            1.0 / MI_HEIGHT(mi),
            1);

  for (y = 0; y < bp->grid_h; y++)
    for (x = 0; x < bp->grid_w; x++)
      {
        glPushMatrix();
        glTranslatef (-bp->grid_w / 2.0 - off.x + (x * TILE_PIXEL_SIZE),
                      -bp->grid_h / 2.0 - off.y - (y * TILE_PIXEL_SIZE)
                      - TILE_PIXEL_SIZE,
                      0);
        draw_tile (mi, &bp->tiles[y * bp->grid_w + x]);
        glPopMatrix();
      }

  glPopMatrix();
}


static void
draw_arrow (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  GLfloat s = 0.02;

  glPushMatrix();
  glScalef (s, s, s);
  glScalef (MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi), 1, 1);
  glRotatef (-atan2 (bp->heading[2].x, bp->heading[2].y) * (180 / M_PI),
             0, 0, 1);

  glFrontFace (GL_CW);
  glDisable (GL_TEXTURE_2D);
  glColor3f (1, 1, 0);
  glBegin (GL_TRIANGLE_FAN);
  glVertex3f (0,    -0.25, 0);
  glVertex3f (-0.5, -1,    0);
  glVertex3f (0,     1,    0);
  glVertex3f (0.5,  -1,    0);
  glEnd();

  glColor3f (0, 0, 0);
  glLineWidth (MI_WIDTH(mi) >= 1280 || MI_HEIGHT(mi) >= 1280  ? 2 : 1);
  glBegin (GL_LINE_LOOP);
  glVertex3f (0,    -0.25, 0);
  glVertex3f (-0.5, -1,    0);
  glVertex3f (0,     1,    0);
  glVertex3f (0.5,  -1,    0);
  glEnd();

  glPopMatrix();
}


static const char *
heading_str (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  double deg = atan2 (bp->heading[1].x, bp->heading[1].y) * (180 / M_PI);
# if 0
  const char *dirs[] = { "N", "NNE", "NE", "ENE",
                         "E", "ESE", "SE", "SSE",
                         "S", "SSW", "SW", "WSW",
                         "W", "WNW", "NW", "NNW" };
# else
  const char *dirs[] = {"N", "NbE", "NNE", "NEbN", "NE", "NEbE", "ENE", "EbN",
                        "E", "EbS", "ESE", "SEbE", "SE", "SEbS", "SSE", "SbE",
                        "S", "SbW", "SSW", "SWbS", "SW", "SWbW", "WSW", "WbS",
                        "W", "WbN", "WNW", "NWbW", "NW", "NWbN", "NNW", "NbW"};
# endif
  double step = 360 / countof(dirs);
  while (deg < 0) deg += 360;
  return dirs[((int) ((deg + step/2) / step)) % countof(dirs)];
}


static void
loader_cb (XtPointer closure, int *source, XtInputId *id)
{
  ModeInfo *mi = (ModeInfo *) closure;
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  /* Handle it back in the 'draw' function, since we need a GL context. */
  bp->input_available_p = True;
}


/* Launch the Perl helper program in another process, at the end of a
   bidirectional pipe.
 */
static void
fork_loader (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  pid_t forked;
  char buf[1024];
  char *av[20];
  int fd1[2], fd2[2];
  int ac = 0;

  av[ac++] = get_string_resource (mi->dpy, "loaderProgram", "Program");
  if      (verbose_p == 1) av[ac++] = "-v";
  if      (verbose_p == 2) av[ac++] = "-vv";
  else if (verbose_p == 3) av[ac++] = "-vvv";
  else if (verbose_p == 4) av[ac++] = "-vvvv";
  else if (verbose_p == 5) av[ac++] = "-vvvvv";
  else if (verbose_p >= 6) av[ac++] = "-vvvvvv";
  av[ac++] = "--url-template";
  av[ac++] = bp->url_template;
  av[ac++] = "--cache-size";
  av[ac++] = get_string_resource (mi->dpy, "cacheSize", "CacheSize");
  av[ac] = 0;

  if (pipe (fd1))
    {
      sprintf (buf, "%.100s: error creating pipe", blurb(mi));
      perror (buf);
      exit (1);
    }

  if (pipe (fd2))
    {
      sprintf (buf, "%.100s: error creating pipe", blurb(mi));
      perror (buf);
      exit (1);
    }

  forked = fork();
  switch ((int) forked)
    {
    case -1:
      {
        sprintf (buf, "%.100s: couldn't fork", blurb(mi));
        perror (buf);
        exit (1);
        break;
      }
    case 0:
      {
# ifndef HAVE_COCOA
        close (ConnectionNumber (mi->dpy));
# endif
        close (fd1[1]);
        close (fd2[0]);

        if (dup2 (fd1[0], STDIN_FILENO) < 0)	/* pipe stdin */
          {
            sprintf (buf, "%s.100: could not dup() a new stdin:", blurb(mi));
            perror (buf);
            return;
          }

        if (dup2 (fd2[1], STDOUT_FILENO) < 0)	/* pipe stdout */
          {
            sprintf (buf, "%s.100: could not dup() a new stdout", blurb(mi));
            perror (buf);
            return;
          }

        execvp (av[0], av);			/* shouldn't return. */
        sprintf (buf, "%.100s: running %.100s", blurb(mi), av[0]);
        perror (buf);
        exit (1);
        break;
      }
    default:
      {
        close (fd1[0]);
        close (fd2[1]);
        bp->pid = forked;
        bp->pipe_id =
          XtAppAddInput (XtDisplayToApplicationContext (mi->dpy),
                         fd2[0],
                         (XtPointer) (XtInputReadMask | XtInputExceptMask),
                         loader_cb, (XtPointer) mi);
        bp->pipe_out = fd1[1];
        bp->pipe_in  = fd2[0];
        break;
      }
    }
}


/* Process a single line from the Perl helper program.
 */
static void
read_loader (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  char buf[4096];
  int L = 0;
  bp->input_available_p = False;

  /* Read a single line -- the file name.
     If there is more data available, leave it there; select will fire again.
   */
  while (1)
    {
      unsigned char s[2];
      int n = read (bp->pipe_in, (void *) s, 1);
      if (n > 0)
        {
          if (*s && *s != '\n')
            {
              if (L >= sizeof(buf)-1)
                abort();
              buf[L++] = *s;
            }
          else
            {
              buf[L] = 0;
              break;
            }
        }
      else if (n == 0)
        {
          fprintf (stderr, "%s: subprocess died\n", blurb(mi));
          bp->dead_p = TRUE;
          bp->mode = FADE_OUT;
          return;
        }
      else
        {
          sprintf (buf, "%.100s: read", blurb(mi));
          perror (buf);
          exit (1);
        }
    }

  /* Line looks like "x y z \t FILE"
     Find the tile it corresponds to and texturize it. */
  {
    int i;
    long x, y, z;
    char *coords, *file;
    char *s = strchr (buf, '\t');
    Bool matched_p = False;
    if (!s) abort();
    coords = buf;
    *s = 0;
    file = s+1;
    if (3 != sscanf (coords, " %ld %ld %ld ", &x, &y, &z))
      abort();

    bp->tile_count++;

    for (i = 0; i < bp->grid_w * bp->grid_h; i++)
      {
        tile *t = &bp->tiles[i];
        if (t->map.x == x &&
            t->map.y == y &&
            t->map_level == z &&
            !t->texid)
          {
            char buf2[1024];
            XImage *image;
            struct stat st;
            matched_p = True;

            if (strlen(file) == 3)   /* HTTP error code */
              {
                /* 504 timeout error code: retry; all others: don't.
                   The perl script returns 504 if and only if it did not
                   get a response. All other codes are from the server.
                 */
                t->status = (!strcmp (file, "504") ? RETRY : FAILED);
                if (verbose_p)
                  fprintf (stderr, "%s: error %s: tile: %s pos: %.4f, %.4f\n",
                           blurb(mi), file, buf,
                           tiley2lat (y, z), tilex2lon (x, z));
                break;
              }

            if (stat (file, &st))
              {
                /* This can happen if mapscroller.pl has a file cached but
                   the cache was cleared by another copy running on another
                   screen. */
                if (verbose_p)
                  fprintf (stderr, "%s: file does not exist: %s\n",
                           blurb(mi), file);
                t->status = RETRY;
                break;
              }

            image = file_to_ximage (MI_DISPLAY(mi), MI_VISUAL(mi), file);
            if (!image)
              {
                if (verbose_p)
                  fprintf (stderr, "%s: file unloadable: %s\n",
                           blurb(mi), file);
                t->status = RETRY;
                break;
              }

            t->status = OK;
            if (t->texid) abort();
            glGenTextures (1, &t->texid);
            if (!t->texid) abort();
            glBindTexture (GL_TEXTURE_2D, t->texid);

            clear_gl_error();
            glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
            glPixelStorei (GL_UNPACK_ROW_LENGTH, image->width);
            glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA,
                          image->width, image->height, 0,
                          GL_RGBA, GL_UNSIGNED_BYTE, image->data);
            sprintf (buf2, "texture %d: %.100s (%dx%d)", t->texid,
                     file, image->width, image->height);
            check_gl_error (buf2);
            XDestroyImage (image);
            if (verbose_p > 3)
              fprintf (stderr, "%s: %s\n", blurb(mi), buf2);
            else if (verbose_p > 1)
              fprintf (stderr, "%s: got tile %ld %ld %ld\n", blurb(mi),
                       x, y, z);
          }
      }

    if (verbose_p > 2 && !matched_p)
      fprintf (stderr, "%s: got unmatched tile %ld %ld %ld\n", blurb(mi),
               x, y, z);


  }
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_map (ModeInfo *mi, int width, int height)
{
  GLfloat s = 2;

  glViewport (0, 0, width, height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity();

  /* if (MI_IS_WIREFRAME(mi)) s *= 0.35; */
  glScalef (s, s, s);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  reshape_tiles (mi);
}


ENTRYPOINT Bool
map_handle_event (ModeInfo *mi, XEvent *event)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  if (event->xany.type == ButtonPress)
    {
      bp->button_down_p++;

      if (event->xbutton.button == Button4)
        {
        WHEEL_UP:
          bp->map_level++;
          if (bp->map_level > MAX_LEVEL)
            {
              bp->map_level = MAX_LEVEL;
              return False;
            }
          if (verbose_p)
            fprintf (stderr, "%s: level %d\n", blurb(mi), bp->map_level);
        }
      else if (event->xbutton.button == Button5)
        {
        WHEEL_DOWN:
          bp->map_level--;
          if (bp->map_level < MIN_LEVEL)
            {
              bp->map_level = MIN_LEVEL;
              return False;
            }
          if (verbose_p)
            fprintf (stderr, "%s: level %d\n", blurb(mi), bp->map_level);
        }

      bp->drag_start_deg = bp->pos;
      bp->drag_start_px.x = event->xbutton.x;
      bp->drag_start_px.y = event->xbutton.y;
      return True;
    }
  else if (event->xany.type == ButtonRelease && bp->button_down_p)
    {
      bp->button_down_p--;

      if (! bp->button_down_p && event->xbutton.button == Button1)
        {
          bp->heading[0] = bp->heading[2];
          bp->heading[1] = bp->heading[2];
          bp->heading[1].x =  (event->xbutton.x - MI_WIDTH(mi)/2);
          bp->heading[1].y = -(event->xbutton.y - MI_HEIGHT(mi)/2);
          bp->heading_ratio = 0;
          if (verbose_p)
            fprintf (stderr, "%s: click heading %s\n", blurb(mi),
                     heading_str(mi));
        }

      return True;
    }
  else if (event->xany.type == MotionNotify && bp->button_down_p)
    {
      /* Size in vertical degrees decreases toward the poles, so the easiest
         way to find that is to just examine one of the current tiles. */
      double tile_degrees_x = 360 / exp2(bp->map_level);
      double tile_degrees_y =
        (tiley2lat (bp->tiles[0].map.y, bp->map_level) -
         tiley2lat (bp->tiles[bp->grid_w].map.y, bp->map_level));
      double degrees_per_pixel_x = tile_degrees_x / TILE_PIXEL_SIZE;
      double degrees_per_pixel_y = tile_degrees_y / TILE_PIXEL_SIZE;
      int dx = event->xmotion.x - bp->drag_start_px.x;
      int dy = event->xmotion.y - bp->drag_start_px.y;
      bp->pos.lat = bp->drag_start_deg.lat + dy * degrees_per_pixel_y;
      bp->pos.lon = bp->drag_start_deg.lon - dx * degrees_per_pixel_x;
      constrain_mercator (&bp->pos);
    }
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      if (keysym == XK_Prior)
        goto WHEEL_UP;
      else if (keysym == XK_Next)
        goto WHEEL_DOWN;
      else if (keysym == XK_Up || c == '=' || c == '+')
        {
          bp->heading[0] = bp->heading[2];
          bp->heading[1] = bp->heading[2];
          bp->heading[1].x = 0;
          bp->heading[1].y = 1;
          bp->heading_ratio = 0;
          if (verbose_p)
            fprintf (stderr, "%s: new heading %s\n", blurb(mi),
                     heading_str(mi));
          return True;
        }
      else if (keysym == XK_Down || c == '-' || c == '_')
        {
          bp->heading[0] = bp->heading[2];
          bp->heading[1] = bp->heading[2];
          bp->heading[1].x = 0;
          bp->heading[1].y = -1;
          bp->heading_ratio = 0;
          if (verbose_p)
            fprintf (stderr, "%s: new heading %s\n", blurb(mi),
                     heading_str(mi));
          return True;
        }
      else if (keysym == XK_Left || c == ',' || c == '<')
        {
          bp->heading[0] = bp->heading[2];
          bp->heading[1] = bp->heading[2];
          bp->heading[1].x = -1;
          bp->heading[1].y = 0;
          bp->heading_ratio = 0;
          if (verbose_p)
            fprintf (stderr, "%s: new heading %s\n", blurb(mi),
                     heading_str(mi));
          return True;
        }
      else if (keysym == XK_Right || c == '.' || c == '>')
        {
          bp->heading[0] = bp->heading[2];
          bp->heading[1] = bp->heading[2];
          bp->heading[1].x = 1;
          bp->heading[1].y = 0;
          bp->heading_ratio = 0;
          if (verbose_p)
            fprintf (stderr, "%s: new heading %s\n", blurb(mi),
                     heading_str(mi));
          return True;
        }
      else if (c == ' ')
        {
          bp->heading[0] = bp->heading[2];
          bp->heading[1] = bp->heading[2];
          bp->heading[1].x = frand(2)-1;
          bp->heading[1].y = frand(2)-1;
          bp->heading_ratio = 0;
          if (verbose_p)
            fprintf (stderr, "%s: new random heading %s\n", blurb(mi),
                     heading_str(mi));
          return True;
        }
    }

  return False;
}


static void
randomize_position (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  Bool city_p = !strcasecmp (origin_arg, "random-city");
  LL pos = { 0, 0 };
  int i;
  for (i = 0; i < 1000; i++)  /* Don't get stuck */
    {
      if (city_p)
        {
          if (i == 0)
            pos = cities[random() % countof(cities)];  /* Random city center */
          /* Offset by a few miles, but not into the ocean */
          bp->pos.lat = pos.lat + frand(0.05) - 0.025;
          bp->pos.lon = pos.lon + frand(0.05) - 0.025;
        }
      else
        {
          bp->pos.lat = frand (180)  - 90;
          bp->pos.lon = frand (360) - 180;
        }
      constrain_mercator (&bp->pos);
      bp->ocean_p = mostly_ocean_p (mi);
      if (! bp->ocean_p)
        break;
    }
}


ENTRYPOINT void 
init_map (ModeInfo *mi)
{
  map_configuration *bp;

  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  bp->font_data = load_texture_font (mi->dpy, "titleFont");

  bp->url_template = url_template_arg;
  if (!bp->url_template ||
      !*bp->url_template ||
      *bp->url_template == '(')
    bp->url_template = FALLBACK_URL;

  bp->map_level = map_level_arg;
  if (bp->map_level <= MIN_LEVEL) bp->map_level = MIN_LEVEL;
  if (bp->map_level >= MAX_LEVEL) bp->map_level = MAX_LEVEL;

  bp->speed = speed_arg;
  if (bp->speed < 0.001) bp->speed = 0.001;
  if (bp->speed > 10)    bp->speed = 10;

  bp->duration = duration_arg;
  if (bp->duration < 30) bp->duration = 30;
  bp->duration *= 1 + frand(0.2);  /* Keep multiple instances out of sync */

  bp->oceans = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                     oceantiles_12_png,
                                     sizeof (oceantiles_12_png));
  if (! bp->oceans) abort();

  {
    float lat = 0, lon = 0;
    if (!origin_arg || !*origin_arg)
      origin_arg = "random-city";

    if (2 == sscanf (origin_arg, " %f , %f ", &lat, &lon) ||
        2 == sscanf (origin_arg, " %f ; %f ", &lat, &lon) ||
        2 == sscanf (origin_arg, " %f / %f ", &lat, &lon) ||
        2 == sscanf (origin_arg, " %f   %f ", &lat, &lon))
      {
        bp->pos.lat = lat;
        bp->pos.lon = lon;
        constrain_mercator (&bp->pos);
      }
    else
      {
        if (strcasecmp (origin_arg, "random") &&
            strcasecmp (origin_arg, "random-city"))
          {
            fprintf (stderr, "%s: unparsable origin: \"%s\"\n",
                     blurb(mi), origin_arg);
            origin_arg = "random-city";
          }
        randomize_position (mi);
      }
  }

  bp->heading[0].x = frand(2)-1;
  bp->heading[0].y = frand(2)-1;
  bp->heading[1] = bp->heading[0];
  bp->heading[2] = bp->heading[0];
  bp->heading_ratio = 1;

  bp->mode = RUN;
  bp->opacity = 1;
  bp->change_time = time ((time_t *) 0);

  fork_loader (mi);
  reshape_map (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}


static GLfloat
ease_fn (GLfloat r)
{
  return cos ((r/2 + 1) * M_PI) + 1; /* Smooth curve up, end at slope 1. */
}


static GLfloat
ease_ratio (GLfloat r)
{
  GLfloat ease = 0.5;
  if      (r <= 0)     return 0;
  else if (r >= 1)     return 1;
  else if (r <= ease)  return     ease * ease_fn (r / ease);
  else if (r > 1-ease) return 1 - ease * ease_fn ((1 - r) / ease);
  else                 return r;
}


ENTRYPOINT void
draw_map (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel (GL_SMOOTH);
  glDisable (GL_DEPTH_TEST);
  glEnable (GL_NORMALIZE);
  glEnable (GL_CULL_FACE);
  glDisable (GL_LIGHTING);
  glEnable (GL_BLEND);
  glEnable (GL_LINE_SMOOTH);

  glClearColor (0xF2/255.0, 0xEF/255.0, 0xE9/255.0, 1);  /* #F2EFE9 */
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mi->polygon_count = 0;   /* This counts loaded tiles, not polygons */
  mi->recursion_depth = bp->map_level;

  if (bp->mode == RUN && !bp->button_down_p)
    {
      Bool force_change_p;
      double tile_degrees = 360 / exp2(bp->map_level);
      double degrees_per_pixel = tile_degrees / TILE_PIXEL_SIZE;
      int i, loaded_count = 0, failed_count = 0;
      double loaded_ratio, failed_ratio;
      Bool all_failed_p;

      double dx = bp->heading[2].x;
      double dy = bp->heading[2].y;
      double d = sqrt (dx*dx + dy*dy);

      dx /= d;  /* normalize */
      dy /= d;

      /* Scroll more slowly when not all the tiles are loaded. */
      for (i = 0; i < bp->grid_w * bp->grid_h; i++)
        {
          tile *t = &bp->tiles[i];
          if (t->status != LOADING)
            loaded_count++;
          if (t->status == FAILED)
            failed_count++;
        }
      loaded_ratio = loaded_count / (double) (bp->grid_w * bp->grid_h);
      failed_ratio = failed_count / (double) (bp->grid_w * bp->grid_h);
      if (loaded_ratio < 0.1) loaded_ratio = 0.1;

      bp->pos.lon += dx * bp->speed * degrees_per_pixel * 0.3 * loaded_ratio;
      bp->pos.lat += dy * bp->speed * degrees_per_pixel * 0.3 * loaded_ratio;
      force_change_p = constrain_mercator (&bp->pos);

      /* There goes the neighborhood. If we're getting a lot of 404s, move. */
      if (failed_ratio > 0.8)
        {
          all_failed_p = True;
          force_change_p = True;
        }

      if (!force_change_p && !(bp->heading_ratio < 1))
        {
          Bool was_ocean_p = bp->ocean_p;
          bp->ocean_p = (was_ocean_p
                         /* Be more strict on "land ho" */
                         ? ocean_tile_p (mi, bp->pos.lat, bp->pos.lon)
                         : mostly_ocean_p (mi));

          /* If we have just landed in the ocean, reverse gears and
             move in the opposite direction until we're out. */
          if (!was_ocean_p && bp->ocean_p)
            {
              bp->heading[0] = bp->heading[2];
              bp->heading[1] = bp->heading[2];
              bp->heading[1].x = -bp->heading[1].x;
              bp->heading[1].y = -bp->heading[1].y;
              bp->heading_ratio = 0;
              if (verbose_p)
                fprintf (stderr, "%s: out to sea: new heading %s\n",
                         blurb(mi), heading_str(mi));
            }
          else if (was_ocean_p && !bp->ocean_p && verbose_p)
            fprintf (stderr, "%s: land ho\n", blurb(mi));
        }

      if (!strcasecmp (origin_arg, "random") ||
          !strcasecmp (origin_arg, "random-city"))
        {
          time_t now = time ((time_t *) 0);
          if (force_change_p ||
              (bp->mode == RUN &&
               now > bp->change_time + bp->duration))
            {
              bp->mode = FADE_OUT;
              force_change_p = False;
              if (verbose_p)
                fprintf (stderr, "%s: new %s%s, was %.4f, %.4f\n", blurb(mi),
                         (!strcasecmp (origin_arg, "random-city")
                          ? "city" : "position"),
                         (all_failed_p ? " (all tiles failed)" :
                          bp->ocean_p ? " (ocean)" : ""),
                         bp->pos.lat, bp->pos.lon);
            }
        }

      if (force_change_p ||
          (bp->mode == RUN &&
           !bp->ocean_p &&
           bp->heading_ratio >= 1 &&
           !(random() % 1000)))
        {
          dx = frand(2)-1;
          dy = frand(2)-1;
          d = sqrt (dx*dx + dy*dy);
          dx /= d;  /* normalize */
          dy /= d;
          bp->heading[0] = bp->heading[2];
          bp->heading[1].x = dx;
          bp->heading[1].y = dy;
          bp->heading_ratio = 0;
          if (verbose_p)
            fprintf (stderr, "%s: new heading %s\n", blurb(mi),
                     heading_str(mi));
        }

      if (bp->heading_ratio < 1)
        {
          double th0, th1, th2;
          bp->heading_ratio += 0.003;
          if (bp->heading_ratio > 1)
            bp->heading_ratio = 1;

          th0 = atan2 (bp->heading[0].x, bp->heading[0].y);
          th1 = atan2 (bp->heading[1].x, bp->heading[1].y);
          while (th0 < 0) th0 += M_PI*2;
          while (th1 < 0) th1 += M_PI*2;
          if (th1 - th0 > M_PI || th1 - th0 <= -M_PI)
            {
              if (th1 < th0)
                th1 += M_PI*2;
              else
                th0 += M_PI*2;
            }

          th2 = th0 + (th1 - th0) * ease_ratio (bp->heading_ratio);
          bp->heading[2].x = sin (th2);
          bp->heading[2].y = cos (th2);
        }
    }

  if (bp->mode == FADE_OUT)
    {
      bp->opacity -= 0.02;
      if (bp->opacity < 0 && bp->dead_p)
        bp->opacity = 0;
      else if (bp->opacity < 0)
        {
          bp->opacity = 1;
          bp->mode = RUN;
          bp->change_time = time ((time_t *) 0);
          randomize_position (mi);
          if (verbose_p)
            fprintf (stderr, "%s: new position %.4f, %.4f%s\n", blurb(mi),
                     bp->pos.lat, bp->pos.lon,
                     (bp->ocean_p ? " (ocean)" : ""));
        }
    }

  if (bp->input_available_p && !bp->dead_p)
    read_loader (mi);

  if (!MI_IS_WIREFRAME(mi))
    glEnable (GL_TEXTURE_2D);

  if (bp->mode == RUN)
    reshape_tiles (mi);
  draw_tiles (mi);
  if (do_arrow)
    draw_arrow (mi);

  if (do_titles && bp->mode == RUN)
    {
      char buf[1024];
# if 0
      /* 37.771°, -122.412° */
      sprintf (buf, "%.3f\xC2\xB0, %.3f\xC2\xB0", bp->pos.lat, bp->pos.lon);
# elif 0
      /* 37° 46' 15.63" N, 122° 24' 45.70" W */
      double alat = bp->pos.lat >= 0 ? bp->pos.lat : -bp->pos.lat;
      double alon = bp->pos.lon >= 0 ? bp->pos.lon : -bp->pos.lon;
      sprintf (buf,
               "%.0f\xC2\xB0 %.0f' %.2f\" %c, "
               "%.0f\xC2\xB0 %.0f' %.2f\" %c",
               alat,
               (alat - (int) alat) * 60,
               ((alat * 60) - (int) (alat * 60)) * 60,
               (bp->pos.lat >= 0 ? 'N' : 'S'),
               alon,
               (alon - (int) alon) * 60,
               ((alon * 60) - (int) (alon * 60)) * 60,
               (bp->pos.lon >= 0 ? 'E' : 'W'));
# else
      /* 37° 46' N, 122° 24' W */
      double alat = bp->pos.lat >= 0 ? bp->pos.lat : -bp->pos.lat;
      double alon = bp->pos.lon >= 0 ? bp->pos.lon : -bp->pos.lon;
      sprintf (buf,
               "%.0f\xC2\xB0 %.0f' %c, "
               "%.0f\xC2\xB0 %.0f' %c",
               alat,
               (alat - (int) alat) * 60,
               (bp->pos.lat >= 0 ? 'N' : 'S'),
               alon,
               (alon - (int) alon) * 60,
               (bp->pos.lon >= 0 ? 'E' : 'W'));
# endif
      glColor3f (0.3, 0.3, 0.3);
      print_texture_label (mi->dpy, bp->font_data,
                           MI_WIDTH(mi), MI_HEIGHT(mi), 1, buf);
    }

  if (bp->dead_p)
    {
      static char buf[1024] = "";
      if (! *buf)
        {
          char *s = get_string_resource (mi->dpy, "loaderProgram", "Program");
          sprintf (buf, "\n\n\n\n\n\n%.100s subprocess died.", s);
          free (s);
          if (bp->tile_count < 10)
            strcat (buf,
                    " Is Perl broken? Maybe try:\n\n"
                    "sudo cpan LWP::Simple LWP::Protocol::https Mozilla::CA");
        }
      glColor3f (0.3, 0.3, 0.3);
      print_texture_label (mi->dpy, bp->font_data,
                           MI_WIDTH(mi), MI_HEIGHT(mi), 0, buf);
    }

  if (mi->fps_p && !bp->dead_p)
    {
      if (!MI_IS_WIREFRAME(mi))
        {
          /* Put a gray box behind the FPS text, but on top of the map. */
          glPushMatrix();
          glTranslatef (-0.5, -0.5, 0);
          glScalef (1.0 / MI_WIDTH(mi),
                    1.0 / MI_HEIGHT(mi),
                    1);
          glScalef (320, 190, 1);
          glDisable (GL_TEXTURE_2D);
          glColor4f (0, 0, 0, 0.4);
          glBegin(GL_QUADS);
          glVertex3f(0, 0, 0);
          glVertex3f(0, 1, 0);
          glVertex3f(1, 1, 0);
          glVertex3f(1, 0, 0);
          glEnd();
          glPopMatrix();
        }
      do_fps (mi);
    }
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_map (ModeInfo *mi)
{
  map_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->font_data)
    free_texture_font (bp->font_data);
  if (bp->tiles)
    free_tiles (mi);
  if (bp->oceans)
    XDestroyImage (bp->oceans);

  close (bp->pipe_in);
  close (bp->pipe_out);

  if (bp->pid && !kill (bp->pid, SIGTERM))
    {
      int status;
      waitpid (bp->pid, &status, 0);
    }

  if (bp->pipe_id)
    XtRemoveInput (bp->pipe_id);
}

XSCREENSAVER_MODULE_2 ("MapScroller", mapscroller, map)

#endif /* USE_GL */
