/* webcollage-helper --- scales and pastes one image into another
 * xscreensaver, Copyright (c) 2002, 2003 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(HAVE_GDK_PIXBUF) && defined(HAVE_JPEGLIB)  /* whole file */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <jpeglib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


char *progname;
static int verbose_p = 0;

static void add_jpeg_comment (struct jpeg_compress_struct *cinfo);
static void write_pixbuf (GdkPixbuf *pb, const char *file);

static GdkPixbuf *
load_pixbuf (const char *file)
{
  GdkPixbuf *pb;
#ifdef HAVE_GTK2
  GError *err = NULL;

  pb = gdk_pixbuf_new_from_file (file, &err);
#else  /* !HAVE_GTK2 */
  pb = gdk_pixbuf_new_from_file (file);
#endif /* HAVE_GTK2 */

  if (!pb)
    {
#ifdef HAVE_GTK2
      fprintf (stderr, "%s: %s\n", progname, err->message);
      g_error_free (err);
#else  /* !HAVE_GTK2 */
      fprintf (stderr, "%s: unable to load %s\n", progname, file);
#endif /* !HAVE_GTK2 */
      exit (1);
    }

  return pb;
}

static void
paste (const char *paste_file,
       const char *base_file,
       double from_scale,
       double opacity,
       int from_x, int from_y, int to_x, int to_y,
       int w, int h)
{
  GdkPixbuf *paste_pb;
  GdkPixbuf *base_pb;

  int paste_w, paste_h;
  int base_w, base_h;

  paste_pb = load_pixbuf (paste_file);
  base_pb  = load_pixbuf (base_file);

  paste_w = gdk_pixbuf_get_width (paste_pb);
  paste_h = gdk_pixbuf_get_height (paste_pb);

  base_w = gdk_pixbuf_get_width (base_pb);
  base_h = gdk_pixbuf_get_height (base_pb);

  if (verbose_p)
    {
      fprintf (stderr, "%s: loaded %s: %dx%d\n",
               progname, base_file, base_w, base_h);
      fprintf (stderr, "%s: loaded %s: %dx%d\n",
               progname, paste_file, paste_w, paste_h);
    }

  if (from_scale != 1.0)
    {
      int new_w = paste_w * from_scale;
      int new_h = paste_h * from_scale;
      GdkPixbuf *new_pb = gdk_pixbuf_scale_simple (paste_pb, new_w, new_h,
                                                   GDK_INTERP_HYPER);
      gdk_pixbuf_unref (paste_pb);
      paste_pb = new_pb;
      paste_w = gdk_pixbuf_get_width (paste_pb);
      paste_h = gdk_pixbuf_get_height (paste_pb);

      if (verbose_p)
        fprintf (stderr, "%s: %s: scaled to %dx%d (%.2f)\n",
                 progname, paste_file, paste_w, paste_h, from_scale);
    }

  if (w == 0) w = paste_w - from_x;
  if (h == 0) h = paste_h - from_y;

  {
    int ofx = from_x;
    int ofy = from_y;
    int otx = to_x;
    int oty = to_y;
    int ow = w;
    int oh = h;

    int clipped = 0;

    if (from_x < 0)		/* from left out of bounds */
      {
        w += from_x;
        from_x = 0;
        clipped = 1;
      }

    if (from_y < 0)		/* from top out of bounds */
      {
        h += from_y;
        from_y = 0;
        clipped = 1;
      }

    if (to_x < 0)		/* to left out of bounds */
      {
        w += to_x;
        from_x -= to_x;
        to_x = 0;
        clipped = 1;
      }

    if (to_y < 0)		/* to top out of bounds */
      {
        h += to_y;
        from_y -= to_y;
        to_y = 0;
        clipped = 1;
      }

    if (from_x + w > paste_w)	/* from right out of bounds */
      {
        w = paste_w - from_x;
        clipped = 1;
      }

    if (from_y + h > paste_h)	/* from bottom out of bounds */
      {
        h = paste_h - from_y;
        clipped = 1;
      }

    if (to_x + w > base_w)	/* to right out of bounds */
      {
        w = base_w - to_x;
        clipped = 1;
      }

    if (to_y + h > base_h)	/* to bottom out of bounds */
      {
        h = base_h - to_y;
        clipped = 1;
      }


    if (clipped && verbose_p)
      {
        fprintf (stderr, "clipped from: %dx%d %d,%d %d,%d\n",
                 ow, oh, ofx, ofy, otx, oty);
        fprintf (stderr, "clipped   to: %dx%d %d,%d %d,%d\n",
                 w, h, from_x, from_y, to_x, to_y);
      }
  }

  if (opacity == 1.0)
    gdk_pixbuf_copy_area (paste_pb,
                          from_x, from_y, w, h,
                          base_pb,
                          to_x, to_y);
  else
    gdk_pixbuf_composite (paste_pb, base_pb,
                          to_x, to_y, w, h,
                          to_x - from_x, to_y - from_y,
                          1.0, 1.0,
                          GDK_INTERP_HYPER,
                          opacity * 255);

  if (verbose_p)
    fprintf (stderr, "%s: pasted %dx%d from %d,%d to %d,%d\n",
             progname, paste_w, paste_h, from_x, from_y, to_x, to_y);

  gdk_pixbuf_unref (paste_pb);
  write_pixbuf (base_pb, base_file);
  gdk_pixbuf_unref (base_pb);
}


static void
write_pixbuf (GdkPixbuf *pb, const char *file)
{
  int jpeg_quality = 85;

  int w = gdk_pixbuf_get_width (pb);
  int h = gdk_pixbuf_get_height (pb);
  guchar *data = gdk_pixbuf_get_pixels (pb);
  int ww = gdk_pixbuf_get_rowstride (pb);
  int channels = gdk_pixbuf_get_n_channels (pb);

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE *out;

  if (channels != 3)
    {
      fprintf (stderr, "%s: %d channels?\n", progname, channels);
      exit (1);
    }

  cinfo.err = jpeg_std_error (&jerr);
  jpeg_create_compress (&cinfo);

  out = fopen (file, "wb");
  if (!out)
    {
      char buf[255];
      sprintf (buf, "%.100s: %.100s", progname, file);
      perror (buf);
      exit (1);
    }
  else if (verbose_p)
    fprintf (stderr, "%s: writing %s...", progname, file);

  jpeg_stdio_dest (&cinfo, out);

  cinfo.image_width = w;
  cinfo.image_height = h;
  cinfo.input_components = channels;
  cinfo.in_color_space = JCS_RGB;

  jpeg_set_defaults (&cinfo);
  jpeg_simple_progression (&cinfo);
  jpeg_set_quality (&cinfo, jpeg_quality, TRUE);

  jpeg_start_compress (&cinfo, TRUE);
  add_jpeg_comment (&cinfo);

  {
    guchar *d = data;
    guchar *end = d + (ww * h);
    while (d < end)
      {
        jpeg_write_scanlines (&cinfo, &d, 1);
        d += ww;
      }
  }

  jpeg_finish_compress (&cinfo);
  jpeg_destroy_compress (&cinfo);

  if (verbose_p)
    {
      struct stat st;
      fflush (out);
      if (fstat (fileno (out), &st))
        {
          char buf[255];
          sprintf (buf, "%.100s: %.100s", progname, file);
          perror (buf);
          exit (1);
        }
      fprintf (stderr, " %luK\n", (st.st_size + 1023) / 1024);
    }

  fclose (out);
}


static void
add_jpeg_comment (struct jpeg_compress_struct *cinfo)
{
  time_t now = time ((time_t *) 0);
  struct tm *tm = localtime (&now);
  const char fmt[] =
    "\r\n"
    "    Generated by WebCollage: Exterminate All Rational Thought. \r\n"
    "    Copyright (c) 1999-%Y by Jamie Zawinski <jwz@jwz.org> \r\n"
    "\r\n"
    "        http://www.jwz.org/webcollage/ \r\n"
    "\r\n"
    "    This is what the web looked like on %d %b %Y at %I:%M:%S %p %Z. \r\n"
    "\r\n";
  char comment[sizeof(fmt) + 100];
  strftime (comment, sizeof(comment)-1, fmt, tm);
  jpeg_write_marker (cinfo, JPEG_COM,
                     (unsigned char *) comment,
                     strlen (comment));
}


static void
usage (void)
{
  fprintf (stderr, "usage: %s [-v] paste-file base-file\n"
           "\t from-scale opacity\n"
           "\t from-x from-y to-x to-y w h\n"
           "\n"
           "\t Pastes paste-file into base-file.\n"
           "\t base-file will be overwritten (with JPEG data.)\n"
           "\t scaling is applied first: coordinates apply to scaled image.\n",
           progname);
  exit (1);
}


int
main (int argc, char **argv)
{
  int i;
  char *paste_file, *base_file, *s, dummy;
  double from_scale, opacity;
  int from_x, from_y, to_x, to_y, w, h;

  i = 0;
  progname = argv[i++];
  s = strrchr (progname, '/');
  if (s) progname = s+1;

  if (argc != 11 && argc != 12) usage();

  if (!strcmp(argv[i], "-v"))
    verbose_p++, i++;

  paste_file = argv[i++];
  base_file = argv[i++];

  if (*paste_file == '-') usage();
  if (*base_file == '-') usage();

  s = argv[i++];
  if (1 != sscanf (s, " %lf %c", &from_scale, &dummy)) usage();
  if (from_scale <= 0 || from_scale > 100) usage();

  s = argv[i++];
  if (1 != sscanf (s, " %lf %c", &opacity, &dummy)) usage();
  if (opacity <= 0 || opacity > 1) usage();

  s = argv[i++]; if (1 != sscanf (s, " %d %c", &from_x, &dummy)) usage();
  s = argv[i++]; if (1 != sscanf (s, " %d %c", &from_y, &dummy)) usage();
  s = argv[i++]; if (1 != sscanf (s, " %d %c", &to_x, &dummy)) usage();
  s = argv[i++]; if (1 != sscanf (s, " %d %c", &to_y, &dummy)) usage();
  s = argv[i++]; if (1 != sscanf (s, " %d %c", &w, &dummy)) usage();
  s = argv[i++]; if (1 != sscanf (s, " %d %c", &h, &dummy)) usage();

  if (w < 0) usage();
  if (h < 0) usage();

#ifdef HAVE_GTK2
  g_type_init ();
#endif /* HAVE_GTK2 */

  paste (paste_file, base_file,
         from_scale, opacity,
         from_x, from_y, to_x, to_y,
         w, h);
  exit (0);
}

#endif /* HAVE_GDK_PIXBUF && HAVE_JPEGLIB -- whole file */
