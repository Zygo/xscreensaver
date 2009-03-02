#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)magick.c     5.00 00/10/26 xlockmore";

#endif

/*-
 * Utilities for Reading images using ImageMagick
 * 
 * Copyright (c) 2000 by J.Jansen (joukj@hrem.stm.tudelft.nl)
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 26-Oct-00: Created
 */

#ifdef USE_MAGICK

#include "magick.h"

XLockImage xlockimage;

int
MagickFileToImage(ModeInfo * mi, char *filename, XImage ** image)
{
   Image* imageData = (Image*) NULL;
   ImageInfo imageInfo;
   ExceptionInfo exception;
   PixelPacket* pixdata;
   int i , fuzz;
   unsigned long black, white, fgpix, bgpix;
   XColor      fgcol, bgcol;
   
   /* Read image data from file */
   GetImageInfo( &imageInfo );
   imageInfo.dither = 0;
   strcpy( imageInfo.filename , filename );
   if ( MI_IS_VERBOSE( mi ) )
	(void) printf ( "Reading image %s\n", imageInfo.filename );
   imageData = ReadImage ( &imageInfo, &exception );
   if ( imageData == (Image*) NULL )
     {
	(void) fprintf ( stderr , "Error reading image %s\n", imageInfo.filename );
	return MagickFileInvalid;
     }

   /* setup X-image */
   xlockimage.width = imageData->columns;
   xlockimage.height = imageData->rows;
   if ( MI_IS_VERBOSE( mi ) )
     {
	(void) printf ( "MagickImage dimensions %d %d\n", imageData->columns ,
		       imageData->rows );
	(void) printf ( "XlockImage dimensions %d %d ( %d )\n",
		       xlockimage.width , xlockimage.height ,
		       xlockimage.width );
     }
   xlockimage.data = (unsigned char *) malloc((int) ( xlockimage.width *
						     xlockimage.height));
   if (!xlockimage.data)
     {
	(void) fprintf(stderr, "out of memory for Raster file\n");
	return MagickNoMemory;
     }

   if ( imageData->colors == 0 )
     {
	PixelPacket* pixtmp;
	
	AllocateImageColormap ( imageData , COLORMAP_SIZE );
	pixdata = GetImagePixels( imageData , 0 , 0 , imageData->columns ,
					     imageData->rows );
	imageData->colors = imageData->columns*imageData->rows;

	for ( fuzz=0; imageData->colors > COLORMAP_SIZE; fuzz++)
	  {
	     int num_col , j;
	     
	     pixtmp = pixdata;
	     num_col = 0;
	     for ( i=0; i<imageData->columns*imageData->rows; i++)
	       {
		  PixelPacket* pixtmp2;
		    
		  pixtmp2 = imageData->colormap;
		  for ( j=0; j<num_col; j++ )
		    {
		       if ( ABS( pixtmp->red - pixtmp2->red ) +
			 ABS( pixtmp->green - pixtmp2->green ) +
			 ABS( pixtmp->blue - pixtmp2->blue ) < fuzz )
			 break;
		       pixtmp2++;
		    }
		  if ( j == num_col )
		    {
		       pixtmp2->red = pixtmp->red;
		       pixtmp2->green = pixtmp->green;
		       pixtmp2->blue = pixtmp->blue;
		       num_col++;
		       if ( num_col == COLORMAP_SIZE )
			 {
			    num_col++;
			    break;
			 }
		    }
		  xlockimage.data[ i ] = j;
		  pixtmp++;
	       }
	     imageData->colors = num_col;
	  }
     }
   else
     {
	if ( imageData->colors > COLORMAP_SIZE )
	  {
	     imageData->colors = COLORMAP_SIZE;
	     (void) fprintf( stderr ,
			    "wrong colour reducing algorithm used\n" );
	     (void) fprintf( stderr ,
	     "Please notify the maintainer that this statement is reached\n" );
	  }
   
   /* get pixel information */
   if ( !( pixdata = GetImagePixels( imageData , 0 , 0 , imageData->columns ,
				    imageData->rows ) ) )
     {
	(void) fprintf( stderr , "Error getting pixels\n" );
	return MagickFileInvalid;
     }
   
	xlockimage.data = GetIndexes ( imageData );
     }
   *image = XCreateImage( MI_DISPLAY(mi), MI_VISUAL(mi), 8, ZPixmap, 0,
			 (char *) xlockimage.data, (int) xlockimage.width,
			 (int) xlockimage.height, 16, (int) xlockimage.width );
   if (!*image)
     {
	(void) fprintf(stderr, "could not create image from file\n");
	return MagickColorError;
     }
   
   /*set up colourmap */
	black = MI_BLACK_PIXEL(mi);
	white = MI_WHITE_PIXEL(mi);
	fgpix = MI_FG_PIXEL(mi);
	bgpix = MI_BG_PIXEL(mi);
	fgcol.pixel = fgpix;
	bgcol.pixel = bgpix;
	XQueryColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &fgcol);
	XQueryColor(MI_DISPLAY(mi), MI_COLORMAP(mi), &bgcol);

	/* Set these, if Image does not overwrite some, so much the better. */
	if (fgpix < COLORMAP_SIZE) {
		xlockimage.red[fgpix] = fgcol.red >> 8;
		xlockimage.green[fgpix] = fgcol.green >> 8;
		xlockimage.blue[fgpix] = fgcol.blue >> 8;
	}
	if (bgpix < COLORMAP_SIZE) {
		xlockimage.red[bgpix] = bgcol.red >> 8;
		xlockimage.green[bgpix] = bgcol.green >> 8;
		xlockimage.blue[bgpix] = bgcol.blue >> 8;
	}
	if (white < COLORMAP_SIZE) {
		xlockimage.red[white] = 0xFF;
		xlockimage.green[white] = 0xFF;
		xlockimage.blue[white] = 0xFF;
	}
	if (black < COLORMAP_SIZE) {
		xlockimage.red[black] = 0;
		xlockimage.green[black] = 0;
		xlockimage.blue[black] = 0;
	}
   /* supply data and colours*/
	xlockimage.colors = imageData->colors;
	for ( i=0 ; i<xlockimage.colors ; i++ )
	  {
	     xlockimage.red[ i ] = imageData->colormap->red;
	     xlockimage.green[ i ] = imageData->colormap->green;
	     xlockimage.blue[ i ] = imageData->colormap->blue;
	     imageData->colormap++;
	  }
   
   /* Make sure there is a black ... */
   if (xlockimage.colors <= 0xff)
     xlockimage.red[0xff] = xlockimage.green[0xff] = xlockimage.blue[0xff] = 0;

   /* clean up */
   DestroyImage( imageData );

   return MagickSuccess;
}

#endif /* USE_MAGICK */
