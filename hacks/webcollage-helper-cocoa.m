/* webcollage-helper-cocoa --- scales and pastes one image into another
 * xscreensaver, Copyright (c) 2002-2009 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is the Cocoa implementation.  See webcollage-helper.c for the
   GDK + JPEGlib implementation.
 */

#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>


#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
 typedef int          NSInteger;
 typedef unsigned int NSUInteger;
#endif


char *progname;
static int verbose_p = 0;

static void write_image (NSImage *img, const char *file);


/* NSImage can't load PPMs by default...
 */
static NSImage *
load_ppm_image (const char *file)
{
  FILE *in = fopen (file, "r");
  if (! in) return 0;

  char buf[255];

  char *s = fgets (buf, sizeof(buf)-1, in);		/* P6 */
  if (!s || !!strcmp (s, "P6\n")) 
    return 0;

  s = fgets (buf, sizeof(buf)-1, in);			/* W H */
  if (!s)
    return 0;

  int w = 0, h = 0, d = 0;
  if (2 != sscanf (buf, " %d %d \n", &w, &h))
    return 0;
  if (w <= 0 || h <= 0)
    return 0;

  s = fgets (buf, sizeof(buf)-1, in);			/* 255 */
  if (!s)
    return 0;

  if (1 != sscanf (buf, " %d \n", &d))
    return 0;
  if (d != 255)
    return 0;

  int size = (w * (h+1) * 3);
  unsigned char *bits = malloc (size);
  if (!bits) return 0;

  int n = read (fileno (in), (void *) bits, size);	/* body */
  if (n < 20) return 0;

  fclose (in);
  
  NSBitmapImageRep *rep =
    [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes: &bits
                    pixelsWide: w
                    pixelsHigh: h
                 bitsPerSample: 8
               samplesPerPixel: 3
                      hasAlpha: NO
                      isPlanar: NO
                colorSpaceName: NSDeviceRGBColorSpace
                  bitmapFormat: NSAlphaFirstBitmapFormat
                   bytesPerRow: w * 3
                  bitsPerPixel: 8 * 3];

  NSImage *image = [[NSImage alloc] initWithSize: NSMakeSize (w, h)];
  [image addRepresentation: rep];
  [rep release];

  // #### 'bits' is leaked... the NSImageRep doesn't free it when freed.

  return image;
}


static NSImage *
load_image (const char *file)
{
  NSImage *image = [[NSImage alloc] 
                     initWithContentsOfFile:
                       [NSString stringWithCString: file
                                          encoding: kCFStringEncodingUTF8]];
  if (! image)
    image = load_ppm_image (file);

  if (! image) {
    fprintf (stderr, "%s: unable to load %s\n", progname, file);
    exit (1);
  }

  return image;
}


static void
bevel_image (NSImage *img, int bevel_pct,
             int x, int y, int w, int h, double scale)
{
  int small_size = (w > h ? h : w);

  int bevel_size = small_size * (bevel_pct / 100.0);

  bevel_size /= scale;

  /* Use a proportionally larger bevel size for especially small images. */
  if      (bevel_size < 20 && small_size > 40) bevel_size = 20;
  else if (bevel_size < 10 && small_size > 20) bevel_size = 10;
  else if (bevel_size < 5)    /* too small to bother bevelling */
    return;


  NSBitmapImageRep *rep =
    [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes: NULL
                    pixelsWide: w
                    pixelsHigh: h
                 bitsPerSample: 8
               samplesPerPixel: 4
                      hasAlpha: YES
                      isPlanar: NO
                colorSpaceName: NSDeviceRGBColorSpace
                  bitmapFormat: NSAlphaFirstBitmapFormat
                   bytesPerRow: 0
                  bitsPerPixel: 0];

  NSInteger xx, yy;
  double *ramp = (double *) malloc (sizeof(*ramp) * (bevel_size + 1));

  if (!ramp)
    {
      fprintf (stderr, "%s: out of memory (%d)\n", progname, bevel_size);
      exit (1);
    }

  for (xx = 0; xx <= bevel_size; xx++)
    {
# if 0  /* linear */
      ramp[xx] = xx / (double) bevel_size;

# else /* sinusoidal */
      double p = (xx / (double) bevel_size);
      double s = sin (p * M_PI / 2);
      ramp[xx] = s;
# endif
    }

  memset ([rep bitmapData], 0xFFFFFFFF,
          [rep bytesPerRow] * h);

  for (yy = 0; yy < h; yy++)
    {
      for (xx = 0; xx < w; xx++)
        {
          double rx, ry, r;

          if (xx < bevel_size)           rx = ramp[xx];
          else if (xx >= w - bevel_size) rx = ramp[w - xx - 1];
          else rx = 1;

          if (yy < bevel_size)           ry = ramp[yy];
          else if (yy >= h - bevel_size) ry = ramp[h - yy - 1];
          else ry = 1;

          r = rx * ry;
          if (r != 1)
            {
              NSUInteger p[4];
              p[0] = 0xFF * r;
              p[1] = p[2] = p[3] = 0xFF;
              [rep setPixel:p atX:xx y:yy];
            }
        }
    }

  free (ramp);

  NSImage *bevel_img = [[NSImage alloc]
                         initWithData: [rep TIFFRepresentation]];

  [img lockFocus];
  y = [img size].height - (y + h);
  [bevel_img drawAtPoint: NSMakePoint (x, y)
                fromRect: NSMakeRect (0, 0, w, h)
               operation: NSCompositeDestinationIn /* Destination image
                                                      wherever both images are
                                                      opaque, transparent
                                                      elsewhere. */
                fraction: 1.0];
  [img unlockFocus];

  [rep release];
  [bevel_img release];

  if (verbose_p)
    fprintf (stderr, "%s: added %d%% bevel (%d px)\n", progname,
             bevel_pct, bevel_size);
}


static void
paste (const char *paste_file,
       const char *base_file,
       double from_scale,
       double opacity, int bevel_pct,
       int from_x, int from_y, int to_x, int to_y,
       int w, int h)
{
  NSImage *paste_img = load_image (paste_file);
  NSImage *base_img  = load_image (base_file);

  int paste_w = [paste_img size].width;
  int paste_h = [paste_img size].height;

  int base_w  = [base_img size].width;
  int base_h  = [base_img size].height;

  if (verbose_p)
    {
      fprintf (stderr, "%s: loaded %s: %dx%d\n",
               progname, base_file, base_w, base_h);
      fprintf (stderr, "%s: loaded %s: %dx%d\n",
               progname, paste_file, paste_w, paste_h);
    }

  if (bevel_pct > 0 && paste_w > 5 && paste_h > 5)
    bevel_image (paste_img, bevel_pct,
                 from_x, from_y, w, h, 
                 from_scale);

  int scaled_w = w * from_scale;
  int scaled_h = h * from_scale;

  from_y = paste_h - (from_y + h);  // Cocoa flipped coordinate system
  to_y   = base_h  - (to_y + scaled_h);

  [base_img lockFocus];
  [paste_img drawInRect: NSMakeRect (to_x, to_y, scaled_w, scaled_h)
               fromRect: NSMakeRect (from_x, from_y, w, h)
              operation: NSCompositeSourceOver
               fraction: opacity];
  [base_img unlockFocus];

  if (verbose_p)
    fprintf (stderr, "%s: pasted %dx%d (%dx%d) from %d,%d to %d,%d\n",
             progname, w, h, scaled_w, scaled_h, from_x, from_y, to_x, to_y);

  [paste_img release];
  write_image (base_img, base_file);
  [base_img release];
}


static void
write_image (NSImage *img, const char *file)
{
  float jpeg_quality = .85;

  // Load the NSImage's contents into an NSBitmapImageRep:
  NSBitmapImageRep *bit_rep = [NSBitmapImageRep
                                imageRepWithData:[img TIFFRepresentation]];

  // Write the bitmapImageRep to a JPEG file.
  if (bit_rep == nil)
    {
      fprintf (stderr, "%s: error converting image?\n", progname);
      exit (1);
    }

  if (verbose_p)
    fprintf (stderr, "%s: writing %s (q=%d%%) ", progname, file, 
             (int) (jpeg_quality * 100));

  NSDictionary *props = [NSDictionary
                          dictionaryWithObject:
                            [NSNumber numberWithFloat:jpeg_quality]
                          forKey:NSImageCompressionFactor];
  NSData *jpeg_data = [bit_rep representationUsingType:NSJPEGFileType
                               properties:props];

  [jpeg_data writeToFile:
               [NSString stringWithCString:file
                                  encoding:NSISOLatin1StringEncoding]
             atomically:YES];

  if (verbose_p)
    {
      struct stat st;
      if (stat (file, &st))
        {
          char buf[255];
          sprintf (buf, "%.100s: %.100s", progname, file);
          perror (buf);
          exit (1);
        }
      fprintf (stderr, " %luK\n", ((unsigned long) st.st_size + 1023) / 1024);
    }
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
  int from_x, from_y, to_x, to_y, w, h, bevel_pct;

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
  s = argv[i];   if (1 != sscanf (s, " %d %c", &h, &dummy)) usage();

  bevel_pct = 10; /* #### */

  if (w < 0) usage();
  if (h < 0) usage();


  // Much of Cocoa needs one of these to be available.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  //Need an NSApp instance to make [NSImage TIFFRepresentation] work
  NSApp = [NSApplication sharedApplication];
  [NSApp autorelease];

  paste (paste_file, base_file,
         from_scale, opacity, bevel_pct,
         from_x, from_y, to_x, to_y,
         w, h);

  [pool release];

  exit (0);
}
