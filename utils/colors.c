/* xscreensaver, Copyright (c) 1997, 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This file contains some utility routines for randomly picking the colors
   to hack the screen with.
 */

#include "utils.h"
#include "hsv.h"
#include "yarandom.h"
#include "visual.h"
#include "colors.h"

extern char *progname;

void
free_colors(Display *dpy, Colormap cmap, XColor *colors, int ncolors)
{
  int i;
  if (ncolors > 0)
    {
      unsigned long *pixels = (unsigned long *)
	malloc(sizeof(*pixels) * ncolors);
      for (i = 0; i < ncolors; i++)
	pixels[i] = colors[i].pixel;
      XFreeColors (dpy, cmap, pixels, ncolors, 0L);
      free(pixels);
    }
}


void
allocate_writable_colors (Display *dpy, Colormap cmap,
			  unsigned long *pixels, int *ncolorsP)
{
  int desired = *ncolorsP;
  int got = 0;
  int requested = desired;
  unsigned long *new_pixels = pixels;

  *ncolorsP = 0;
  while (got < desired
	 && requested > 0)
    {
      if (desired - got < requested)
	requested = desired - got;

      if (XAllocColorCells (dpy, cmap, False, 0, 0, new_pixels, requested))
	{
	  /* Got all the pixels we asked for. */
	  new_pixels += requested;
	  got += requested;
	}
      else
	{
	  /* We didn't get all/any of the pixels we asked for.  This time, ask
	     for half as many.  (If we do get all that we ask for, we ask for
	     the same number again next time, so we only do O(log(n)) server
	     roundtrips.)
	  */
	  requested = requested / 2;
	}
    }
  *ncolorsP += got;
}


static void
complain (int wanted_colors, int got_colors,
	  Bool wanted_writable, Bool got_writable)
{
  if (got_colors > wanted_colors - 10)
    /* don't bother complaining if we're within ten pixels. */
    return;

  if (wanted_writable && !got_writable)
    fprintf (stderr,
             "%s: wanted %d writable colors; got %d read-only colors.\n",
             progname, wanted_colors, got_colors);
  else
    fprintf (stderr, "%s: wanted %d%s colors; got %d.\n",
             progname, wanted_colors, (got_writable ? " writable" : ""),
             got_colors);
}



void
make_color_ramp (Display *dpy, Colormap cmap,
		 int h1, double s1, double v1,   /* 0-360, 0-1.0, 0-1.0 */
		 int h2, double s2, double v2,   /* 0-360, 0-1.0, 0-1.0 */
		 XColor *colors, int *ncolorsP,
		 Bool closed_p,
		 Bool allocate_p,
		 Bool writable_p)
{
  Bool verbose_p = True;  /* argh. */
  int i;
  int total_ncolors = *ncolorsP;
  int ncolors, wanted;
  Bool wanted_writable = (allocate_p && writable_p);
  double dh, ds, dv;		/* deltas */

  wanted = total_ncolors;
  if (closed_p)
    wanted = (wanted / 2) + 1;

 AGAIN:
  ncolors = total_ncolors;

  memset (colors, 0, (*ncolorsP) * sizeof(*colors));

  if (closed_p)
    ncolors = (ncolors / 2) + 1;

  /* Note: unlike other routines in this module, this function assumes that
     if h1 and h2 are more than 180 degrees apart, then the desired direction
     is always from h1 to h2 (rather than the shorter path.)  make_uniform
     depends on this.
   */
  dh = ((double)h2 - (double)h1) / ncolors;
  ds = (s2 - s1) / ncolors;
  dv = (v2 - v1) / ncolors;

  for (i = 0; i < ncolors; i++)
    {
      colors[i].flags = DoRed|DoGreen|DoBlue;
      hsv_to_rgb ((int) (h1 + (i*dh)), (s1 + (i*ds)), (v1 + (i*dv)),
		  &colors[i].red, &colors[i].green, &colors[i].blue);
    }

  if (closed_p)
    for (i = ncolors; i < *ncolorsP; i++)
      colors[i] = colors[(*ncolorsP)-i];

  if (!allocate_p)
    return;

  if (writable_p)
    {
      unsigned long *pixels = (unsigned long *)
	malloc(sizeof(*pixels) * ((*ncolorsP) + 1));

      /* allocate_writable_colors() won't do here, because we need exactly this
	 number of cells, or the color sequence we've chosen won't fit. */
      if (! XAllocColorCells(dpy, cmap, False, 0, 0, pixels, *ncolorsP))
	{
	  free(pixels);
	  goto FAIL;
	}

      for (i = 0; i < *ncolorsP; i++)
	colors[i].pixel = pixels[i];
      free (pixels);

      XStoreColors (dpy, cmap, colors, *ncolorsP);
    }
  else
    {
      for (i = 0; i < *ncolorsP; i++)
	{
	  XColor color;
	  color = colors[i];
	  if (XAllocColor (dpy, cmap, &color))
	    {
	      colors[i].pixel = color.pixel;
	    }
	  else
	    {
	      free_colors (dpy, cmap, colors, i);
	      goto FAIL;
	    }
	}
    }

  goto WARN;

 FAIL:
  /* we weren't able to allocate all the colors we wanted;
     decrease the requested number and try again.
   */
  total_ncolors = (total_ncolors > 170 ? total_ncolors - 20 :
                   total_ncolors > 100 ? total_ncolors - 10 :
                   total_ncolors >  75 ? total_ncolors -  5 :
                   total_ncolors >  25 ? total_ncolors -  3 :
                   total_ncolors >  10 ? total_ncolors -  2 :
                   total_ncolors >   2 ? total_ncolors -  1 :
                   0);
  *ncolorsP = total_ncolors;
  ncolors = total_ncolors;
  if (total_ncolors > 0)
    goto AGAIN;

 WARN:
  
  if (verbose_p &&
      /* don't warn if we got 0 writable colors -- probably TrueColor. */
      (ncolors != 0 || !wanted_writable))
    complain (wanted, ncolors, wanted_writable, wanted_writable && writable_p);
}


#define MAXPOINTS 50	/* yeah, so I'm lazy */


static void
make_color_path (Display *dpy, Colormap cmap,
		 int npoints, int *h, double *s, double *v,
		 XColor *colors, int *ncolorsP,
		 Bool allocate_p,
		 Bool writable_p)
{
  int i, j, k;
  int total_ncolors = *ncolorsP;

  int ncolors[MAXPOINTS];  /* number of pixels per edge */
  double dh[MAXPOINTS];    /* distance between pixels, per edge (0 - 360.0) */
  double ds[MAXPOINTS];    /* distance between pixels, per edge (0 - 1.0) */
  double dv[MAXPOINTS];    /* distance between pixels, per edge (0 - 1.0) */

  if (npoints == 0)
    {
      *ncolorsP = 0;
      return;
    }
  else if (npoints == 2)	/* using make_color_ramp() will be faster */
    {
      make_color_ramp (dpy, cmap,
		       h[0], s[0], v[0], h[1], s[1], v[1],
		       colors, ncolorsP,
		       True,  /* closed_p */
		       allocate_p, writable_p);
      return;
    }
  else if (npoints >= MAXPOINTS)
    {
      npoints = MAXPOINTS-1;
    }

 AGAIN:

  {
    double DH[MAXPOINTS];	/* Distance between H values in the shortest
				   direction around the circle, that is, the
				   distance between 10 and 350 is 20.
				   (Range is 0 - 360.0.)
				*/
    double edge[MAXPOINTS];	/* lengths of edges in unit HSV space. */
    double ratio[MAXPOINTS];	/* proportions of the edges (total 1.0) */
    double circum = 0;
    double one_point_oh = 0;	/* (debug) */

    for (i = 0; i < npoints; i++)
      {
	int j = (i+1) % npoints;
	double d = ((double) (h[i] - h[j])) / 360;
	if (d < 0) d = -d;
	if (d > 0.5) d = 0.5 - (d - 0.5);
	DH[i] = d;
      }

    for (i = 0; i < npoints; i++)
      {
	int j = (i+1) % npoints;
	edge[i] = sqrt((DH[i] * DH[j]) +
		       ((s[j] - s[i]) * (s[j] - s[i])) +
		       ((v[j] - v[i]) * (v[j] - v[i])));
	circum += edge[i];
      }

#ifdef DEBUG
    fprintf(stderr, "\ncolors:");
    for (i=0; i < npoints; i++)
      fprintf(stderr, " (%d, %.3f, %.3f)", h[i], s[i], v[i]);
    fprintf(stderr, "\nlengths:");
    for (i=0; i < npoints; i++)
      fprintf(stderr, " %.3f", edge[i]);
#endif /* DEBUG */

    if (circum < 0.0001)
      goto FAIL;

    for (i = 0; i < npoints; i++)
      {
	ratio[i] = edge[i] / circum;
	one_point_oh += ratio[i];
      }

#ifdef DEBUG
    fprintf(stderr, "\nratios:");
    for (i=0; i < npoints; i++)
      fprintf(stderr, " %.3f", ratio[i]);
#endif /* DEBUG */

    if (one_point_oh < 0.99999 || one_point_oh > 1.00001)
      abort();

    /* space the colors evenly along the circumference -- that means that the
       number of pixels on a edge is proportional to the length of that edge
       (relative to the lengths of the other edges.)
     */
    for (i = 0; i < npoints; i++)
      ncolors[i] = total_ncolors * ratio[i];


#ifdef DEBUG
    fprintf(stderr, "\npixels:");
    for (i=0; i < npoints; i++)
      fprintf(stderr, " %d", ncolors[i]);
    fprintf(stderr, "  (%d)\n", total_ncolors);
#endif /* DEBUG */

    for (i = 0; i < npoints; i++)
      {
	int j = (i+1) % npoints;

	if (ncolors[i] > 0)
	  {
	    dh[i] = 360 * (DH[i] / ncolors[i]);
	    ds[i] = (s[j] - s[i]) / ncolors[i];
	    dv[i] = (v[j] - v[i]) / ncolors[i];
	  }
      }
  }

  memset (colors, 0, (*ncolorsP) * sizeof(*colors));

  k = 0;
  for (i = 0; i < npoints; i++)
    {
      int distance, direction;
      distance = h[(i+1) % npoints] - h[i];
      direction = (distance >= 0 ? -1 : 1);

      if (distance > 180)
	distance = 180 - (distance - 180);
      else if (distance < -180)
	distance = -(180 - ((-distance) - 180));
      else
	direction = -direction;

#ifdef DEBUG
      fprintf (stderr, "point %d: %3d %.2f %.2f\n",
	       i, h[i], s[i], v[i]);
      fprintf(stderr, "  h[i]=%d  dh[i]=%.2f  ncolors[i]=%d\n",
	      h[i], dh[i], ncolors[i]);
#endif /* DEBUG */
      for (j = 0; j < ncolors[i]; j++, k++)
	{
	  double hh = (h[i] + (j * dh[i] * direction));
	  if (hh < 0) hh += 360;
	  else if (hh > 360) hh -= 0;
	  colors[k].flags = DoRed|DoGreen|DoBlue;
	  hsv_to_rgb ((int)
		      hh,
		      (s[i] + (j * ds[i])),
		      (v[i] + (j * dv[i])),
		      &colors[k].red, &colors[k].green, &colors[k].blue);
#ifdef DEBUG
	  fprintf (stderr, "point %d+%d: %.2f %.2f %.2f  %04X %04X %04X\n",
		   i, j,
		   hh,
		   (s[i] + (j * ds[i])),
		   (v[i] + (j * dv[i])),
		   colors[k].red, colors[k].green, colors[k].blue);
#endif /* DEBUG */
	}
    }

  /* Floating-point round-off can make us decide to use fewer colors. */
  if (k < *ncolorsP)
    {
      *ncolorsP = k;
      if (k <= 0)
	return;
    }

  if (!allocate_p)
    return;

  if (writable_p)
    {
      unsigned long *pixels = (unsigned long *)
	malloc(sizeof(*pixels) * ((*ncolorsP) + 1));

      /* allocate_writable_colors() won't do here, because we need exactly this
	 number of cells, or the color sequence we've chosen won't fit. */
      if (! XAllocColorCells(dpy, cmap, False, 0, 0, pixels, *ncolorsP))
	{
	  free(pixels);
	  goto FAIL;
	}

      for (i = 0; i < *ncolorsP; i++)
	colors[i].pixel = pixels[i];
      free (pixels);

      XStoreColors (dpy, cmap, colors, *ncolorsP);
    }
  else
    {
      for (i = 0; i < *ncolorsP; i++)
	{
	  XColor color;
	  color = colors[i];
	  if (XAllocColor (dpy, cmap, &color))
	    {
	      colors[i].pixel = color.pixel;
	    }
	  else
	    {
	      free_colors (dpy, cmap, colors, i);
	      goto FAIL;
	    }
	}
    }

  return;

 FAIL:
  /* we weren't able to allocate all the colors we wanted;
     decrease the requested number and try again.
   */
  total_ncolors = (total_ncolors > 170 ? total_ncolors - 20 :
		   total_ncolors > 100 ? total_ncolors - 10 :
		   total_ncolors >  75 ? total_ncolors -  5 :
		   total_ncolors >  25 ? total_ncolors -  3 :
		   total_ncolors >  10 ? total_ncolors -  2 :
		   total_ncolors >   2 ? total_ncolors -  1 :
		   0);
  *ncolorsP = total_ncolors;
  if (total_ncolors > 0)
    goto AGAIN;
}


void
make_color_loop (Display *dpy, Colormap cmap,
		 int h0, double s0, double v0,   /* 0-360, 0-1.0, 0-1.0 */
		 int h1, double s1, double v1,   /* 0-360, 0-1.0, 0-1.0 */
		 int h2, double s2, double v2,   /* 0-360, 0-1.0, 0-1.0 */
		 XColor *colors, int *ncolorsP,
		 Bool allocate_p,
		 Bool writable_p)
{
  int h[3];
  double s[3], v[3];
  h[0] = h0; h[1] = h1; h[2] = h2;
  s[0] = s0; s[1] = s1; s[2] = s2;
  v[0] = v0; v[1] = v1; v[2] = v2;
  make_color_path(dpy, cmap,
		  3, h, s, v,
		  colors, ncolorsP,
		  allocate_p, writable_p);
}


void
make_smooth_colormap (Display *dpy, Visual *visual, Colormap cmap,
		      XColor *colors, int *ncolorsP,
		      Bool allocate_p,
		      Bool *writable_pP,
		      Bool verbose_p)
{
  int npoints;
  int ncolors = *ncolorsP;
  Bool wanted_writable = (allocate_p && writable_pP && *writable_pP);
  int i;
  int h[MAXPOINTS];
  double s[MAXPOINTS];
  double v[MAXPOINTS];
  double total_s = 0;
  double total_v = 0;
  Screen *screen = (dpy ? DefaultScreenOfDisplay(dpy) : 0); /* #### WRONG! */
  int loop = 0;

  if (*ncolorsP <= 0) return;

  {
    int n = random() % 20;
    if      (n <= 5)  npoints = 2;	/* 30% of the time */
    else if (n <= 15) npoints = 3;	/* 50% of the time */
    else if (n <= 18) npoints = 4;	/* 15% of the time */
    else             npoints = 5;	/*  5% of the time */
  }

 REPICK_ALL_COLORS:
  for (i = 0; i < npoints; i++)
    {
    REPICK_THIS_COLOR:
      if (++loop > 10000) abort();
      h[i] = random() % 360;
      s[i] = frand(1.0);
      v[i] = frand(0.8) + 0.2;

      /* Make sure that no two adjascent colors are *too* close together.
	 If they are, try again.
       */
      if (i > 0)
	{
	  int j = (i+1 == npoints) ? 0 : (i-1);
	  double hi = ((double) h[i]) / 360;
	  double hj = ((double) h[j]) / 360;
	  double dh = hj - hi;
	  double distance;
	  if (dh < 0) dh = -dh;
	  if (dh > 0.5) dh = 0.5 - (dh - 0.5);
	  distance = sqrt ((dh * dh) +
			   ((s[j] - s[i]) * (s[j] - s[i])) +
			   ((v[j] - v[i]) * (v[j] - v[i])));
	  if (distance < 0.2)
	    goto REPICK_THIS_COLOR;
	}
      total_s += s[i];
      total_v += v[i];
    }

  /* If the average saturation or intensity are too low, repick the colors,
     so that we don't end up with a black-and-white or too-dark map.
   */
  if (total_s / npoints < 0.2)
    goto REPICK_ALL_COLORS;
  if (total_v / npoints < 0.3)
    goto REPICK_ALL_COLORS;

  /* If this visual doesn't support writable cells, don't bother trying.
   */
  if (wanted_writable && !has_writable_cells(screen, visual))
    *writable_pP = False;

 RETRY_NON_WRITABLE:
  make_color_path (dpy, cmap, npoints, h, s, v, colors, &ncolors,
		   allocate_p, (writable_pP && *writable_pP));

  /* If we tried for writable cells and got none, try for non-writable. */
  if (allocate_p && *ncolorsP == 0 && *writable_pP)
    {
      *writable_pP = False;
      goto RETRY_NON_WRITABLE;
    }

  if (verbose_p)
    complain(*ncolorsP, ncolors, wanted_writable,
	     wanted_writable && *writable_pP);

  *ncolorsP = ncolors;
}


void
make_uniform_colormap (Display *dpy, Visual *visual, Colormap cmap,
		       XColor *colors, int *ncolorsP,
		       Bool allocate_p,
		       Bool *writable_pP,
		       Bool verbose_p)
{
  int ncolors = *ncolorsP;
  Bool wanted_writable = (allocate_p && writable_pP && *writable_pP);
  Screen *screen = (dpy ? DefaultScreenOfDisplay(dpy) : 0); /* #### WRONG! */

  double S = ((double) (random() % 34) + 66) / 100.0;	/* range 66%-100% */
  double V = ((double) (random() % 34) + 66) / 100.0;	/* range 66%-100% */

  if (*ncolorsP <= 0) return;

  /* If this visual doesn't support writable cells, don't bother trying. */
  if (wanted_writable && !has_writable_cells(screen, visual))
    *writable_pP = False;

 RETRY_NON_WRITABLE:
  make_color_ramp(dpy, cmap,
		  0,   S, V,
		  359, S, V,
		  colors, &ncolors,
		  False, allocate_p,
                  (writable_pP && *writable_pP));

  /* If we tried for writable cells and got none, try for non-writable. */
  if (allocate_p && *ncolorsP == 0 && writable_pP && *writable_pP)
    {
      ncolors = *ncolorsP;
      *writable_pP = False;
      goto RETRY_NON_WRITABLE;
    }

  if (verbose_p)
    complain(*ncolorsP, ncolors, wanted_writable,
	     wanted_writable && *writable_pP);

  *ncolorsP = ncolors;
}


void
make_random_colormap (Display *dpy, Visual *visual, Colormap cmap,
		      XColor *colors, int *ncolorsP,
		      Bool bright_p,
		      Bool allocate_p,
		      Bool *writable_pP,
		      Bool verbose_p)
{
  Bool wanted_writable = (allocate_p && writable_pP && *writable_pP);
  int ncolors = *ncolorsP;
  int i;
  Screen *screen = (dpy ? DefaultScreenOfDisplay(dpy) : 0); /* #### WRONG! */

  if (*ncolorsP <= 0) return;

  /* If this visual doesn't support writable cells, don't bother trying. */
  if (wanted_writable && !has_writable_cells(screen, visual))
    *writable_pP = False;

  for (i = 0; i < ncolors; i++)
    {
      colors[i].flags = DoRed|DoGreen|DoBlue;
      if (bright_p)
	{
	  int H = random() % 360;			   /* range 0-360    */
	  double S = ((double) (random()%70) + 30)/100.0;  /* range 30%-100% */
	  double V = ((double) (random()%34) + 66)/100.0;  /* range 66%-100% */
	  hsv_to_rgb (H, S, V,
		      &colors[i].red, &colors[i].green, &colors[i].blue);
	}
      else
	{
	  colors[i].red   = random() % 0xFFFF;
	  colors[i].green = random() % 0xFFFF;
	  colors[i].blue  = random() % 0xFFFF;
	}
    }

  if (!allocate_p)
    return;

 RETRY_NON_WRITABLE:
  if (writable_pP && *writable_pP)
    {
      unsigned long *pixels = (unsigned long *)
	malloc(sizeof(*pixels) * (ncolors + 1));

      allocate_writable_colors (dpy, cmap, pixels, &ncolors);
      if (ncolors > 0)
	for (i = 0; i < ncolors; i++)
	  colors[i].pixel = pixels[i];
      free (pixels);
      if (ncolors > 0)
	XStoreColors (dpy, cmap, colors, ncolors);
    }
  else
    {
      for (i = 0; i < ncolors; i++)
	{
	  XColor color;
	  color = colors[i];
	  if (!XAllocColor (dpy, cmap, &color))
	    break;
	  colors[i].pixel = color.pixel;
	}
      ncolors = i;
    }

  /* If we tried for writable cells and got none, try for non-writable. */
  if (allocate_p && ncolors == 0 && writable_pP && *writable_pP)
    {
      ncolors = *ncolorsP;
      *writable_pP = False;
      goto RETRY_NON_WRITABLE;
    }

  if (verbose_p)
    complain(*ncolorsP, ncolors, wanted_writable,
	     wanted_writable && *writable_pP);

  *ncolorsP = ncolors;
}


void
rotate_colors (Display *dpy, Colormap cmap,
	       XColor *colors, int ncolors, int distance)
{
  int i;
  XColor *colors2 = (XColor *) malloc(sizeof(*colors2) * ncolors);
  if (ncolors < 2) return;
  distance = distance % ncolors;
  for (i = 0; i < ncolors; i++)
    {
      int j = i - distance;
      if (j >= ncolors) j -= ncolors;
      if (j < 0) j += ncolors;
      colors2[i] = colors[j];
      colors2[i].pixel = colors[i].pixel;
    }
  XStoreColors (dpy, cmap, colors2, ncolors);
  XFlush(dpy);
  memcpy(colors, colors2, sizeof(*colors) * ncolors);
  free(colors2);
}
