/* pdf2jpeg -- converts a PDF file to a JPEG file, using Cocoa
 *
 * Copyright (c) 2003, 2008 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Inspired by clues provided by Jan Kujawa and Jonathan Hendry.
 */

#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char** argv)
{
  const char *progname = argv[0];
  const char *infile = 0, *outfile = 0;
  double compression = 0.85;
  double scale = 1.0;
  int verbose = 0;
  int i;

  for (i = 1; i < argc; i++)
    {
      char c;
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;
      if (!strcmp (argv[i], "-q") ||
          !strcmp (argv[i], "-qual") ||
          !strcmp (argv[i], "-quality"))
        {
          int q;
          if (1 != sscanf (argv[++i], " %d %c", &q, &c) ||
              q < 5 || q > 100)
            {
              fprintf (stderr, "%s: quality must be 5 - 100 (%d)\n",
                       progname, q);
              goto USAGE;
            }
          compression = q / 100.0;
        }
      else if (!strcmp (argv[i], "-scale"))
        {
          float s;
          if (1 != sscanf (argv[++i], " %f %c", &s, &c) ||
              s <= 0 || s > 50)
            {
              fprintf (stderr, "%s: scale must be 0.0 - 50.0 (%f)\n",
                       progname, s);
              goto USAGE;
            }
          scale = s;
        }
      else if (!strcmp (argv[i], "-verbose"))
        verbose++;
      else if (!strcmp (argv[i], "-v") ||
               !strcmp (argv[i], "-vv") ||
               !strcmp (argv[i], "-vvv"))
        verbose += strlen(argv[i])-1;
      else if (argv[i][0] == '-')
        {
          fprintf (stderr, "%s: unknown option %s\n", progname, argv[i]);
          goto USAGE;
        }
      else if (!infile)
        infile  = argv[i];
      else if (!outfile)
        outfile = argv[i];
      else
        {
        USAGE:
          fprintf (stderr,
                   "usage: %s [-verbose] [-scale N] [-quality NN] "
                   "infile.pdf outfile.jpg\n",
                   progname);
          exit (1);
        }
    }

  if (!infile || !outfile)
    goto USAGE;


  // Much of Cocoa needs one of these to be available.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  //Need an NSApp instance to make [NSImage TIFFRepresentation] work
  NSApp = [NSApplication sharedApplication];
  [NSApp autorelease];

  if (verbose)
    fprintf (stderr, "%s: reading %s...\n", progname, infile);

  // Load the PDF file into an NSData object:
  NSData *pdf_data = [NSData dataWithContentsOfFile:
                               [NSString stringWithCString:infile
                                         encoding:NSUTF8StringEncoding]];

  // Create an NSPDFImageRep from the data:
  NSPDFImageRep *pdf_rep = [NSPDFImageRep imageRepWithData:pdf_data];

  // Create an NSImage instance
  NSRect rect;
  rect.size = [pdf_rep size];
  rect.size.width  *= scale;
  rect.size.height *= scale;
  rect.origin.x = rect.origin.y = 0;
  NSImage *image = [[NSImage alloc] initWithSize:rect.size];

  // Draw the PDFImageRep in the NSImage
  [image lockFocus];
  [pdf_rep drawInRect:rect];
  [image unlockFocus];

  // Load the NSImage's contents into an NSBitmapImageRep:
  NSBitmapImageRep *bit_rep = [NSBitmapImageRep
                                imageRepWithData:[image TIFFRepresentation]];

  // Write the bitmapImageRep to a JPEG file:
  if (bit_rep == nil)
    {
      fprintf (stderr, "%s: error converting image?\n", argv[0]);
      exit (1);
    }

  if (verbose)
    fprintf (stderr, "%s: writing %s (%d%% quality)...\n",
             progname, outfile, (int) (compression * 100));

  NSDictionary *props = [NSDictionary
                          dictionaryWithObject:
                            [NSNumber numberWithFloat:compression]
                          forKey:NSImageCompressionFactor];
  NSData *jpeg_data = [bit_rep representationUsingType:NSBitmapImageFileTypeJPEG
                               properties:props];

  [jpeg_data writeToFile:
               [NSString stringWithCString:outfile
                         encoding:NSUTF8StringEncoding]
             atomically:YES];
  [image release];

  [pool release];
  exit (0);
}
