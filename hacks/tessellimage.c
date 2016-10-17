/* tessellimage, Copyright (c) 2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "screenhack.h"
#include "delaunay.h"

#undef DO_VORONOI


#ifndef HAVE_JWXYZ
# define XK_MISCELLANY
# include <X11/keysymdef.h>
#endif

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  GC wgc, pgc;
  int delay;
  Bool outline_p, cache_p, fill_p;
  double duration, duration2;
  int max_depth;
  double start_time, start_time2;

  XImage *img, *delta;
  Pixmap image, output, deltap;
  int nthreshes, threshes[256], vsizes[256];
  int thresh, dthresh;
  Pixmap cache[256];

  async_load_state *img_loader;
  XRectangle geom;
  Bool button_down_p;
};


/* Returns the current time in seconds as a double.
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static void *
tessellimage_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));

  st->dpy = dpy;
  st->window = window;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  if (st->delay < 1) st->delay = 1;

  st->outline_p = get_boolean_resource (st->dpy, "outline", "Boolean");
  st->cache_p   = get_boolean_resource (st->dpy, "cache", "Boolean");
  st->fill_p    = get_boolean_resource (st->dpy, "fillScreen", "Boolean");

  st->max_depth = get_integer_resource (st->dpy, "maxDepth", "MaxDepth");
  if (st->max_depth < 100) st->max_depth = 100;

  st->duration = get_float_resource (st->dpy, "duration", "Seconds");
  if (st->duration < 1) st->duration = 1;

  st->duration2 = get_float_resource (st->dpy, "duration2", "Seconds");
  if (st->duration2 < 0.001) st->duration = 0.001;

  XClearWindow(st->dpy, st->window);

  return st;
}


/* Given a bitmask, returns the position and width of the field.
 */
static void
decode_mask (unsigned int mask, unsigned int *pos_ret, unsigned int *size_ret)
{
  int i;
  for (i = 0; i < 32; i++)
    if (mask & (1L << i))
      {
        int j = 0;
        *pos_ret = i;
        for (; i < 32; i++, j++)
          if (! (mask & (1L << i)))
            break;
        *size_ret = j;
        return;
      }
}


static unsigned long
pixel_distance (Visual *v, unsigned long p1, unsigned long p2)
{
  static int initted_p = 0;
  static unsigned int rmsk=0, gmsk=0, bmsk=0;
  static unsigned int rpos=0, gpos=0, bpos=0;
  static unsigned int rsiz=0, gsiz=0, bsiz=0;

  unsigned char r1, g1, b1;
  unsigned char r2, g2, b2;
  long distance;

  if (!p1 && !p2) return 0;

  if (! initted_p) {
    rmsk = v->red_mask;
    gmsk = v->green_mask;
    bmsk = v->blue_mask;
    decode_mask (rmsk, &rpos, &rsiz);
    decode_mask (gmsk, &gpos, &gsiz);
    decode_mask (bmsk, &bpos, &bsiz);
    initted_p = 1;
  }

  r1 = (p1 & rmsk) >> rpos;
  g1 = (p1 & gmsk) >> gpos;
  b1 = (p1 & bmsk) >> bpos;

  r2 = (p2 & rmsk) >> rpos;
  g2 = (p2 & gmsk) >> gpos;
  b2 = (p2 & bmsk) >> bpos;

#if 0
  /* Compute the distance in linear RGB space.
   */
  distance = cbrt (((r2 - r1) * (r2 - r1)) +
                   ((g2 - g1) * (g2 - g1)) +
                   ((b2 - b1) * (b2 - b1)));

# elif 1
  /* Compute the distance in luminance-weighted RGB space.
   */
  {
    int rd = (r2 - r1) * 0.2989 * (1 / 0.5870);
    int gd = (g2 - g1) * 0.5870 * (1 / 0.5870);
    int bd = (b2 - b1) * 0.1140 * (1 / 0.5870);
    distance = cbrt ((rd * rd) + (gd * gd) + (bd * bd));
  }
# else
  /* Compute the distance in brightness-weighted HSV space.
     (Slower, and doesn't seem to look better than luminance RGB.)
   */
  {
    int h1, h2;
    double s1, s2;
    double v1, v2;
    double hd, sd, vd, dd;
    rgb_to_hsv (r1, g1, b1, &h1, &s1, &v1);
    rgb_to_hsv (r2, g2, b2, &h2, &s2, &v2);

    hd = abs (h2 - h1);
    if (hd >= 180) hd -= 180;
    hd /= 180.0;
    sd = fabs (s2 - s1);
    vd = fabs (v2 - v1);

    /* [hsv]d are now the distance as 0.0 - 1.0. */
    /* Compute the overall distance, giving more weight to V. */
    dd = (hd * 0.25 + sd * 0.25 + vd * 0.5);

    if (dd < 0 || dd > 1.0) abort();
    distance = dd * 255;
  }
# endif

  if (distance < 0) distance = -distance;
  return distance;
}


static void
flush_cache (struct state *st)
{
  int i;
  for (i = 0; i < countof(st->cache); i++)
    if (st->cache[i])
      {
        XFreePixmap (st->dpy, st->cache[i]);
        st->cache[i] = 0;
      }
  if (st->deltap)
    {
      XFreePixmap (st->dpy, st->deltap);
      st->deltap = 0;
    }
}


/* Scale up the bits in st->img so that it fills the screen, centered.
 */
static void
scale_image (struct state *st)
{
  double scale, s1, s2;
  XImage *img2;
  int x, y, cx, cy;

  if (st->geom.width <= 0 || st->geom.height <= 0)
    return;

  s1 = st->geom.width  / (double) st->img->width;
  s2 = st->geom.height / (double) st->img->height;
  scale = (s1 < s2 ? s1 : s2);

  img2 = XCreateImage (st->dpy, st->xgwa.visual, st->img->depth,
                       ZPixmap, 0, NULL,
                       st->img->width, st->img->height, 8, 0);
  if (! img2) abort();
  img2->data = (char *) calloc (img2->height, img2->bytes_per_line);
  if (! img2->data) abort();

  cx = st->img->width  / 2;
  cy = st->img->height / 2;

  if (st->geom.width < st->geom.height)  /* portrait: aim toward the top */
    cy = st->img->height / (2 / scale);

  for (y = 0; y < img2->height; y++)
    for (x = 0; x < img2->width; x++)
      {
        int x2 = cx + ((x - cx) * scale);
        int y2 = cy + ((y - cy) * scale);
        unsigned long p = 0;
        if (x2 >= 0 && y2 >= 0 &&
            x2 < st->img->width && y2 < st->img->height)
          p = XGetPixel (st->img, x2, y2);
        XPutPixel (img2, x, y, p);
      }
  free (st->img->data);
  st->img->data = 0;
  XDestroyImage (st->img);
  st->img = img2;

  st->geom.x = 0;
  st->geom.y = 0;
  st->geom.width = st->img->width;
  st->geom.height = st->img->height;
}



static void
analyze (struct state *st)
{
  Window root;
  int x, y, i;
  unsigned int w, h, bw, d;
  unsigned long histo[256];

  flush_cache (st);

  /* Convert the loaded pixmap to an XImage.
   */
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  XGetGeometry (st->dpy, st->image, &root, &x, &y, &w, &h, &bw, &d);

  if (st->img)
    {
      free (st->img->data);
      st->img->data = 0;
      XDestroyImage (st->img);
    }
  st->img = XGetImage (st->dpy, st->image, 0, 0, w, h, ~0L, ZPixmap);

  if (st->fill_p) scale_image (st);

  /* Create the delta map: color space distance between each pixel.
     Maybe doing running a Sobel Filter matrix on this would be a
     better idea.  That might be a bit faster, but I think it would
     make no visual difference.
   */
  if (st->delta)
    {
      free (st->delta->data);
      st->delta->data = 0;
      XDestroyImage (st->delta);
    }
  st->delta = XCreateImage (st->dpy, st->xgwa.visual, d, ZPixmap, 0, NULL,
                            w, h, 32, 0);
  st->delta->data = (char *)
    calloc (st->delta->height, st->delta->bytes_per_line);

  for (y = 0; y < st->delta->height; y++)
    {
      for (x = 0; x < st->delta->width; x++)
        {
          unsigned long pixels[5];
          int i = 0;
          int distance = 0;
          pixels[i++] =                     XGetPixel (st->img, x,   y);
          pixels[i++] = (x > 0 && y > 0   ? XGetPixel (st->img, x-1, y-1) : 0);
          pixels[i++] = (         y > 0   ? XGetPixel (st->img, x,   y-1) : 0);
          pixels[i++] = (x > 0            ? XGetPixel (st->img, x-1, y)   : 0);
          pixels[i++] = (x > 0 && y < h-1 ? XGetPixel (st->img, x-1, y+1) : 0);

          for (i = 1; i < countof(pixels); i++)
            distance += pixel_distance (st->xgwa.visual, pixels[0], pixels[i]);
          distance /= countof(pixels)-1;
          XPutPixel (st->delta, x, y, distance);
        }
    }

  /* Collect a histogram of every distance value.
   */
  memset (histo, 0, sizeof(histo));
  for (y = 0; y < st->delta->height; y++)
    for (x = 0; x < st->delta->width; x++)
      {
        unsigned long p = XGetPixel (st->delta, x, y);
        if (p > sizeof(histo)) abort();
        histo[p]++;
      }

  /* Convert that from "occurrences of N" to ">= N".
   */
  for (i = countof(histo) - 1; i > 0; i--)
    histo[i-1] += histo[i];

# if 0
  fprintf (stderr, "%s: histo: ", progname);
  for (i = 0; i < countof(histo); i++)
    fprintf(stderr, "%d:%lu ", i, histo[i]);
  fprintf(stderr, "\n");
# endif

  /* Collect a useful set of threshold values, ignoring thresholds that
     result in a very similar number of control points (since those images
     probably won't look very different).
   */

  {
    int max_vsize = st->max_depth;
    int min_vsize = 20;
    int min_delta = 100;

    if (min_vsize > max_vsize/100)
      min_vsize = max_vsize/100;

    if (min_delta > max_vsize/1000)
      min_delta = max_vsize/1000;

    st->nthreshes = 0;
    for (i = countof(histo)-1; i >= 0; i--)
      {
        unsigned long vsize = histo[i];

        /* If this is a different vsize, push it. */
        if (vsize >= min_vsize &&
            vsize <= max_vsize &&
            (st->nthreshes == 0 ||
             vsize >= st->vsizes[st->nthreshes-1] + min_delta))
          {
            st->threshes[st->nthreshes] = i;
            st->vsizes[st->nthreshes] = vsize;
            st->nthreshes++;
          }
      }
  }
  
  st->thresh = 0;   /* startup */
  st->dthresh = 1;  /* forward */

  if (st->output)
    {
      XFreePixmap (st->dpy, st->output);
      st->output = 0;
    }


# if 0
  fprintf (stderr, "%s: threshes:", progname);
  for (i = 0; i < st->nthreshes; i++)
    fprintf (stderr, " %d=%d", st->threshes[i], st->vsizes[i]);
  fprintf (stderr, "\n");
# endif
}


#ifndef DO_VORONOI
/* True if the distance between any two corners is too small for it to
   make sense to draw an outline around this triangle.
 */
static Bool
small_triangle_p (const XPoint *p)
{
  int min = 4;
  if (abs (p[0].x - p[1].x) < min) return True;
  if (abs (p[0].y - p[1].y) < min) return True;
  if (abs (p[1].x - p[2].x) < min) return True;
  if (abs (p[1].y - p[2].y) < min) return True;
  if (abs (p[2].x - p[0].x) < min) return True;
  if (abs (p[2].y - p[0].y) < min) return True;
  return False;
}
#endif /* DO_VORONOI */

#ifdef DO_VORONOI

typedef struct {
  int npoints;
  XPoint *p;
} voronoi_polygon;

static voronoi_polygon *
delaunay_to_voronoi (int np, XYZ *p, int nv, ITRIANGLE *v)
{
  struct tri_list {
    int count, size;
    int *tri;
  };

  int i, j;
  struct tri_list *vert_to_tri = (struct tri_list *)
    calloc (np + 1, sizeof(*vert_to_tri));
  voronoi_polygon *out = (voronoi_polygon *) calloc (np + 1, sizeof(*out));

/*
  for (i = 0; i < np; i++)
    printf("# p %d = %d %d\n", i, (int)p[i].x, (int)p[i].y);
  printf("\n");
  for (i = 0; i < nv; i++)
    printf("@ t %d = %d %d %d\n", i, (int)v[i].p1, (int)v[i].p2, (int)v[i].p3);
  printf("\n");
*/

  /* Iterate the triangles to construct a map of vertices to the
     triangles that contain them.
   */
  for (i = 0; i < nv; i++)
    {
      for (j = 0; j < 3; j++)	/* iterate points in each triangle */
        {
          int p = *((&v[i].p1) + j);
          struct tri_list *t = &vert_to_tri[p];
          if (p < 0 || p >= np) abort();
          if (t->size <= t->count + 1)
            {
              t->size += 3;
              t->size *= 1.3;
              t->tri = realloc (t->tri, t->size * sizeof(*t->tri));
              if (! t->tri) abort();
            }
          t->tri[t->count++] = i;
        }
    }

/*
  for (i = 0; i < nv; i++)
    {
      struct tri_list *t = &vert_to_tri[i];
      printf("p %d [%d %d]:", i, (int)p[i].x, (int)p[i].y);
      for (j = 0; j < t->count; j++) {
        int tt = t->tri[j];
        printf(" t %d [%d(%d %d) %d(%d %d) %d(%d %d)]",
               tt,
               (int)v[tt].p1,
               (int)p[v[tt].p1].x, (int)p[v[tt].p1].y,
               (int)v[tt].p2,
               (int)p[v[tt].p2].x, (int)p[v[tt].p2].y,
               (int)v[tt].p3,
               (int)p[v[tt].p3].x, (int)p[v[tt].p3].y
               );
        if (tt < 0 || tt >= nv) abort();
      }
      printf("\n");
    }
*/

  /* For every vertex, compose a polygon whose corners are the centers
     of each triangle using that vertex.  Skip any with less than 3 points.
   */
  for (i = 0; i < np; i++)
    {
      struct tri_list *t = &vert_to_tri[i];
      int n = t->count;
      if (n < 3) n = 0;
      out[i].npoints = n;
      out[i].p = (n > 0
                  ? (XPoint *) calloc (out[i].npoints + 1, sizeof (*out[i].p))
                  : 0);
//printf("%d: ", i);
      for (j = 0; j < out[i].npoints; j++)
        {
          ITRIANGLE *tt = &v[t->tri[j]];
          out[i].p[j].x = (p[tt->p1].x + p[tt->p2].x + p[tt->p3].x) / 3;
          out[i].p[j].y = (p[tt->p1].y + p[tt->p2].y + p[tt->p3].y) / 3;
//printf(" [%d: %d %d]", j, out[i].p[j].x, out[i].p[j].y);
        }
//printf("\n");
    }

  free (vert_to_tri);
  return out;
}

#endif /* DO_VORONOI */




static void
tessellate (struct state *st)
{
  Bool ticked_p = False;

  if (! st->image) return;

  if (! st->wgc)
    {
      XGCValues gcv;
      gcv.function = GXcopy;
      gcv.subwindow_mode = IncludeInferiors;
      st->wgc = XCreateGC(st->dpy, st->window, GCFunction, &gcv);
      st->pgc = XCreateGC(st->dpy, st->image, GCFunction, &gcv);
    }

  if (! st->nthreshes) return;


  /* If duration2 has expired, switch to the next threshold. */

  if (! st->button_down_p)
    {
      double t2 = double_time();
      if (st->start_time2 + st->duration2 < t2)
        {
          st->start_time2 = t2;
          st->thresh += st->dthresh;
          ticked_p = True;
          if (st->thresh >= st->nthreshes)
            {
              st->thresh = st->nthreshes - 1;
              st->dthresh = -1;
            }
          else if (st->thresh < 0)
            {
              st->thresh = 0;
              st->dthresh = 1;
            }
        }
    }

  if (! st->output)
    ticked_p = True;

  /* If we've picked a new threshold, regenerate the output image. */

  if (ticked_p && st->cache[st->thresh])
    {
      if (st->output)
        XCopyArea (st->dpy, 
                   st->cache[st->thresh],
                   st->output, st->pgc,
                   0, 0, st->delta->width, st->delta->height, 
                   0, 0);
    }
  else if (ticked_p)
    {
      int threshold = st->threshes[st->thresh];
      int vsize = st->vsizes[st->thresh];
      ITRIANGLE *v;
      XYZ *p = 0;
      int nv = 0;
      int ntri = 0;
      int x, y, i;

#if 0
      fprintf(stderr, "%s: thresh %d/%d = %d=%d\n", 
              progname, st->thresh, st->nthreshes, threshold, vsize);
#endif

      /* Create a control point at every pixel where the delta is above
         the current threshold.  Triangulate from those. */

      vsize += 8;  /* corners of screen + corners of image */

      p = (XYZ *) calloc (vsize+4, sizeof(*p));
      v = (ITRIANGLE *) calloc (3*(vsize+4), sizeof(*v));
      if (!p || !v)
        {
          fprintf (stderr, "%s: out of memory (%d)\n", progname, vsize);
          abort();
        }

      /* Add control points for the corners of the screen, and for the
         corners of the image.
       */
      if (st->geom.width  <= 0) st->geom.width  = st->delta->width;
      if (st->geom.height <= 0) st->geom.height = st->delta->height;

      for (y = 0; y <= 1; y++)
        for (x = 0; x <= 1; x++)
          {
            p[nv].x = x ? st->delta->width-1  : 0;
            p[nv].y = y ? st->delta->height-1 : 0;
            p[nv].z = XGetPixel (st->delta, (int) p[nv].x, (int) p[nv].y);
            nv++;
            p[nv].x = st->geom.x + (x ? st->geom.width-1  : 0);
            p[nv].y = st->geom.y + (y ? st->geom.height-1 : 0);
            p[nv].z = XGetPixel (st->delta, (int) p[nv].x, (int) p[nv].y);
            nv++;
          }

      /* Add control points for every pixel that exceeds the threshold.
       */
      for (y = 0; y < st->delta->height; y++)
        for (x = 0; x < st->delta->width; x++)
          {
            unsigned long px = XGetPixel (st->delta, x, y);
            if (px >= threshold)
              {
                if (nv >= vsize) abort();
                p[nv].x = x;
                p[nv].y = y;
                p[nv].z = px;
                nv++;
              }
          }

      if (nv != vsize) abort();

      qsort (p, nv, sizeof(*p), delaunay_xyzcompare);
      if (delaunay (nv, p, v, &ntri))
        {
          fprintf (stderr, "%s: out of memory\n", progname);
          abort();
        }

      /* Create the output pixmap based on that triangulation. */

      if (st->output)
        XFreePixmap (st->dpy, st->output);
      st->output = XCreatePixmap (st->dpy, st->window,
                                  st->delta->width, st->delta->height,
                                  st->xgwa.depth);
      XFillRectangle (st->dpy, st->output, st->pgc, 
                      0, 0, st->delta->width, st->delta->height);

#ifdef DO_VORONOI

      voronoi_polygon *polys = delaunay_to_voronoi (nv, p, ntri, v);
      for (i = 0; i < nv; i++)
        {
          if (polys[i].npoints >= 3)
            {
              unsigned long color = XGetPixel (st->img, p[i].x, p[i].y);
              XSetForeground (st->dpy, st->pgc, color);
              XFillPolygon (st->dpy, st->output, st->pgc,
                            polys[i].p, polys[i].npoints,
                            Convex, CoordModeOrigin);

              if (st->outline_p)
                {
                  XColor bd;
                  double scale = 0.8;
                  bd.pixel = color;
                  XQueryColor (st->dpy, st->xgwa.colormap, &bd);
                  bd.red   *= scale;
                  bd.green *= scale;
                  bd.blue  *= scale;

                  /* bd.red = 0xFFFF; bd.green = 0; bd.blue = 0; */

                  XAllocColor (st->dpy, st->xgwa.colormap, &bd);
                  XSetForeground (st->dpy, st->pgc, bd.pixel);
                  XDrawLines (st->dpy, st->output, st->pgc,
                              polys[i].p, polys[i].npoints,
                              CoordModeOrigin);
                  XFreeColors (st->dpy, st->xgwa.colormap, &bd.pixel, 1, 0);
                }
            }
          if (polys[i].p) free (polys[i].p);
          polys[i].p = 0;
        }
      free (polys);

#else /* !DO_VORONOI */

      for (i = 0; i < ntri; i++)
        {
          XPoint xp[3];
          unsigned long color;
          xp[0].x = p[v[i].p1].x; xp[0].y = p[v[i].p1].y;
          xp[1].x = p[v[i].p2].x; xp[1].y = p[v[i].p2].y;
          xp[2].x = p[v[i].p3].x; xp[2].y = p[v[i].p3].y;

          /* Set the color of this triangle to the pixel at its midpoint. */
          color = XGetPixel (st->img,
                             (xp[0].x + xp[1].x + xp[2].x) / 3,
                             (xp[0].y + xp[1].y + xp[2].y) / 3);

          XSetForeground (st->dpy, st->pgc, color);
          XFillPolygon (st->dpy, st->output, st->pgc, xp, countof(xp),
                        Convex, CoordModeOrigin);

          if (st->outline_p && !small_triangle_p(xp))
            {	/* Border the triangle with a color that is darker */
              XColor bd;
              double scale = 0.8;
              bd.pixel = color;
              XQueryColor (st->dpy, st->xgwa.colormap, &bd);
              bd.red   *= scale;
              bd.green *= scale;
              bd.blue  *= scale;

              /* bd.red = 0xFFFF; bd.green = 0; bd.blue = 0; */

              XAllocColor (st->dpy, st->xgwa.colormap, &bd);
              XSetForeground (st->dpy, st->pgc, bd.pixel);
              XDrawLines (st->dpy, st->output, st->pgc,
                          xp, countof(xp), CoordModeOrigin);
              XFreeColors (st->dpy, st->xgwa.colormap, &bd.pixel, 1, 0);
            }
        }
#endif /* !DO_VORONOI */

      free (p);
      free (v);

      if (st->cache_p && !st->cache[st->thresh])
        {
          st->cache[st->thresh] =
            XCreatePixmap (st->dpy, st->window,
                           st->delta->width, st->delta->height,
                           st->xgwa.depth);
          if (! st->cache[st->thresh])
            {
              fprintf (stderr, "%s: out of memory\n", progname);
              abort();
            }
          if (st->output)
            XCopyArea (st->dpy, 
                       st->output,
                       st->cache[st->thresh],
                       st->pgc,
                       0, 0, st->delta->width, st->delta->height, 
                       0, 0);
        }
    }

  if (! st->output) abort();
}


/* Convert the delta map into a displayable pixmap.
 */
static Pixmap
get_deltap (struct state *st)
{
  int x, y;
  int w = st->delta->width;
  int h = st->delta->height;
  XImage *dimg;

  Visual *v = st->xgwa.visual;
  unsigned int rmsk=0, gmsk=0, bmsk=0;
  unsigned int rpos=0, gpos=0, bpos=0;
  unsigned int rsiz=0, gsiz=0, bsiz=0;

  if (st->deltap) return st->deltap;

  rmsk = v->red_mask;
  gmsk = v->green_mask;
  bmsk = v->blue_mask;
  decode_mask (rmsk, &rpos, &rsiz);
  decode_mask (gmsk, &gpos, &gsiz);
  decode_mask (bmsk, &bpos, &bsiz);

  dimg = XCreateImage (st->dpy, st->xgwa.visual, st->xgwa.depth,
                       ZPixmap, 0, NULL, w, h, 8, 0);
  if (! dimg) abort();
  dimg->data = (char *) calloc (dimg->height, dimg->bytes_per_line);
  if (! dimg->data) abort();

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        unsigned long v = XGetPixel (st->delta, x, y) << 5;
        unsigned long p = (((v << rpos) & rmsk) |
                           ((v << gpos) & gmsk) |
                           ((v << bpos) & bmsk));
        XPutPixel (dimg, x, y, p);
      }

  st->deltap = XCreatePixmap (st->dpy, st->window, w, h, st->xgwa.depth);
  XPutImage (st->dpy, st->deltap, st->pgc, dimg, 0, 0, 0, 0, w, h);
  XDestroyImage (dimg);
  return st->deltap;
}


static unsigned long
tessellimage_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0,
                                                &st->geom);
      if (! st->img_loader) {  /* just finished */
        analyze (st);
        st->start_time = double_time();
        st->start_time2 = st->start_time;
      }
      goto DONE;
    }

  if (!st->img_loader &&
      st->start_time + st->duration < double_time()) {
    XClearWindow (st->dpy, st->window);
    if (st->image) XFreePixmap (dpy, st->image);
    st->image = XCreatePixmap (st->dpy, st->window,
                               st->xgwa.width, st->xgwa.height,
                               st->xgwa.depth);
    st->img_loader = load_image_async_simple (0, st->xgwa.screen, st->window,
                                              st->image, 0, &st->geom);
    goto DONE;
  }

  tessellate (st);

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  XClearWindow (st->dpy, st->window);

  if (st->output)
    XCopyArea (st->dpy, 
               (st->button_down_p ? get_deltap (st) : st->output),
               st->window, st->wgc,
               0, 0, st->delta->width, st->delta->height, 
               (st->xgwa.width  - st->delta->width)  / 2,
               (st->xgwa.height - st->delta->height) / 2);
  else if (!st->nthreshes)
    XCopyArea (st->dpy,
               st->image,
               st->window, st->wgc,
               0, 0, st->xgwa.width, st->xgwa.height,
               0,
               0);


 DONE:
  return st->delay;
}
  
static void
tessellimage_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
}

static Bool
tessellimage_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (event->xany.type == ButtonPress)
    {
      st->button_down_p = True;
      return True;
    }
  else if (event->xany.type == ButtonRelease)
    {
      st->button_down_p = False;
      return True;
    }
  else if (screenhack_event_helper (dpy, window, event))
    {
      st->start_time = 0;   /* load next image */
      return True;
    }

  return False;
}


static void
tessellimage_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  flush_cache (st);
  if (st->wgc) XFreeGC (dpy, st->wgc);
  if (st->pgc) XFreeGC (dpy, st->pgc);
  if (st->image)  XFreePixmap (dpy, st->image);
  if (st->output) XFreePixmap (dpy, st->output);
  if (st->delta)  XDestroyImage (st->delta);
  free (st);
}




static const char *tessellimage_defaults [] = {
  ".background:			black",
  ".foreground:			white",
  "*dontClearRoot:		True",
  "*fpsSolid:			true",
  "*delay:			30000",
  "*duration:			120",
  "*duration2:			0.4",
  "*maxDepth:			30000",
  "*outline:			True",
  "*fillScreen:			True",
  "*cache:			True",
#ifdef HAVE_MOBILE
  "*ignoreRotation:             True",
  "*rotateImages:               True",
#endif
  0
};

static XrmOptionDescRec tessellimage_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-duration",	".duration",		XrmoptionSepArg, 0 },
  { "-duration2",	".duration2",		XrmoptionSepArg, 0 },
  { "-max-depth",	".maxDepth",		XrmoptionSepArg, 0 },
  { "-outline",		".outline",		XrmoptionNoArg, "True"  },
  { "-no-outline",	".outline",		XrmoptionNoArg, "False" },
  { "-fill-screen",	".fillScreen",		XrmoptionNoArg, "True"  },
  { "-no-fill-screen",	".fillScreen",		XrmoptionNoArg, "False" },
  { "-cache",		".cache",		XrmoptionNoArg, "True"  },
  { "-no-cache",	".cache",		XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Tessellimage", tessellimage)
