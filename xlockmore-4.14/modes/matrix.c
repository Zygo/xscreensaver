/* -*- Mode: C; tab-width: 4 -*- */
/* matrix --- screensaver inspired by the 1999 sci-fi flick "The Matrix" */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)matrix.c	1.0 99/04/20 xlockmore";
#endif

/* Matrix-style screensaver
 *
 * Author: Erik O'Shaughnessy (eriko@xenolab.com)  20 Apr 1999
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 *
 * 22-Apr-1999: Initial version
 *
 * 09-Jun-1999: Tweaks by Joan Touzet to look more like the original film's
 *              effects
 */

#include <X11/Intrinsic.h>

#define BRIGHTGREEN (27 * MI_NPIXELS(mi) / 64)
#define GREEN (23 * MI_NPIXELS(mi) / 64)

/* these characters stolen from a font named "7x14rk" */
#define katakana_cell_width  7
#define katakana_cell_height 10
#define katakana_width       384
#define katakana_height      10

static unsigned char katakana_bits[] = {
 0xc0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x1f,0xf8,0xdd,0xff,0xdf,0xfb,
 0xfd,0xbe,0xff,0x6f,0xf7,0xff,0x7d,0xef,0xfe,0xb9,0x0d,0xf7,0xef,0xff,0xff,
 0xfd,0xfd,0xef,0xff,0xf7,0xfb,0x7f,0x7e,0xff,0x0e,0xf7,0xff,0xff,0x70,0xf7,
 0xde,0xff,0xff,0xdf,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfb,0xde,
 0x83,0xdf,0xfb,0xfd,0xb0,0x0f,0x6e,0xef,0xc1,0xfd,0xee,0x30,0x6e,0xfd,0xf7,
 0xef,0x61,0xf0,0xfd,0xbd,0xef,0x03,0xf7,0x7b,0xe0,0x79,0xff,0xde,0xf7,0xe1,
 0xe0,0x7f,0xb7,0xde,0x07,0x03,0xdf,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0x3a,0x07,0xee,0xdf,0xfb,0xe1,0xb6,0xff,0x6e,0xef,0xdf,0xfd,0xee,0xf6,
 0x5e,0xfd,0xf7,0xef,0xff,0x37,0xf0,0x7d,0xef,0x7e,0xeb,0xc0,0xef,0x77,0xdf,
 0xde,0x07,0xef,0xef,0x7f,0xb7,0xde,0x77,0x7b,0x40,0xe0,0x77,0xff,0x7f,0xf7,
 0xff,0xff,0xed,0xff,0x4c,0xf7,0xee,0x81,0x40,0x7a,0x37,0xf8,0x06,0x7a,0xdf,
 0xc1,0xee,0xf6,0xde,0x05,0xe6,0x81,0xff,0xf7,0xfb,0xfd,0x6e,0x7f,0xeb,0xfb,
 0x6f,0x7e,0xbf,0xde,0x73,0xef,0x6f,0x60,0xb7,0xde,0x77,0x7b,0xdf,0xef,0x1b,
 0x08,0x06,0x06,0xe3,0x60,0x2b,0xf0,0x7e,0xf7,0xee,0xcf,0xdb,0xbb,0xb7,0xfe,
 0x6e,0x77,0xdf,0xdc,0x6f,0x15,0xf8,0xbd,0xd7,0xef,0xff,0xf6,0xfd,0xdd,0x8e,
 0x7f,0xdd,0xe9,0xf6,0x79,0x7f,0x06,0xae,0xef,0xef,0x6f,0xb7,0xde,0x75,0x7b,
 0xef,0xf3,0xdc,0xbb,0x3f,0xb3,0xef,0xef,0xea,0xff,0x7e,0x7f,0xef,0xcf,0xdb,
 0xe3,0xdb,0xfe,0x6e,0xb7,0xef,0xed,0xb7,0xfb,0xfe,0xbd,0xb7,0xef,0xff,0xf9,
 0xf8,0xde,0xed,0xbf,0xdf,0xd9,0xf5,0x7f,0xfe,0xde,0xcf,0xef,0xe0,0x6f,0xb7,
 0xda,0x75,0xbf,0xef,0x3b,0xdd,0xbb,0x5f,0xcf,0xef,0xe1,0xf6,0xff,0x7e,0x7f,
 0xef,0xd7,0x5b,0xf8,0xfb,0xfe,0x6e,0xbf,0xef,0xf5,0xf7,0xfb,0xfe,0xbe,0xb7,
 0xef,0xff,0x3b,0xf5,0xde,0xed,0xbf,0xdf,0xda,0xfb,0xbf,0x7d,0xdd,0xef,0xef,
 0xef,0xf7,0xbb,0xda,0x76,0xbf,0xf7,0xfb,0xfd,0xbd,0x6f,0xef,0xef,0xef,0xf7,
 0x7f,0x7f,0xbf,0xef,0xdb,0xdd,0xf7,0xfd,0xfe,0xbe,0xdf,0xd7,0xfd,0xfb,0xfd,
 0xfe,0xbe,0xf7,0xef,0xff,0xf5,0x7d,0xdf,0xed,0xdf,0xbf,0xda,0x3b,0xbe,0xbd,
 0xdd,0xdf,0xef,0xef,0xf7,0xbb,0xdc,0x76,0xdf,0xfb,0xfd,0xfd,0xbe,0x77,0xef,
 0xef,0xef,0xfb,0x7f,0x7f,0xdf,0xef,0xdd,0xdd,0xf7,0x7e,0x0f,0xbe,0xef,0xbb,
 0xfd,0xfd,0x7e,0x7f,0xdf,0xf7,0xf7,0xc0,0xee,0xbd,0xdf,0xed,0xef,0xbf,0xfb,
 0xf7,0x19,0xda,0xdf,0xdf,0xef,0xef,0xfb,0xdd,0x5c,0x07,0xef,0xfd,0xfe,0x3d,
 0x0f,0x3c,0xef,0x81,0xe0,0xfd,0xbf,0x7f,0xef,0x03,0xcf,0xe6,0x77,0xbf,0xff,
 0xdf,0xf3,0xbd,0xc3,0x7e,0xbf,0xbf,0xef,0xf7,0xfb,0x7f,0xef,0xdd,0xef,0x1d,
 0xf6,0xbf,0xfb,0xf7,0xf7,0xeb,0x3f,0xde,0x81,0xe0,0xfd,0xde,0x9e,0xff,0xf7
 };

#ifdef STANDALONE
#define PROGCLASS "Matrix"
#define HACK_INIT init_matrix
#define HACK_DRAW draw_matrix
#define matrix_opts xlockmore_opts
#define DEFAULTS "*delay: 100\n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt matrix_opts = {0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   matrix_description =
{"matrix", "init_matrix","draw_matrix","release_matrix",
 "refresh_matrix","change_matrix", NULL,&matrix_opts, 
 100, 1, 1, 1, 64, 1.0, "", 
 "Shows the matrix", 0, NULL};
#endif

typedef struct {
  int speed;					/* pixels downward per update */
  int yoffset[2];				/* current offset relative to root origin */
  int width,height;
  Pixmap pix[2];
} column_t;

typedef struct {
  Pixel fg;						/* foreground */
  Pixel bg;						/* background */
  Pixel bold;					/* standout foreground color */
  Pixmap kana[2];				/* pixmap containing katakana 
								 * [0] is fg on bg
								 * [1] is bold on bg
								 */
  int width;					/* width of screen */
  int height;					/* height of screen */
  int num_cols;					/* number of columns of "matrix" data */
  int col_width;
  int col_height;				
  column_t *columns;
} matrix_t;

Pixel get_color(ModeInfo *mi,char *colorname);
void create_columns(ModeInfo *mi,matrix_t *mp);
void new_column(ModeInfo *mi,matrix_t *mp,Pixmap pix);

#define PICK_MATRIX(mi) &matrix[MI_SCREEN((mi))]
#define MATRIX_RANDOM(min,max) ((LRAND()% ((max)-(min)))+(min))

matrix_t *matrix;

/* defining RANDOM_MATRIX causes each column pixmap to be regenerated
 * after scrolling off the screen rather than just repeating
 * infinitely.  this is a better effect, but uses more CPU and will
 * cause update hesitations (especially on multi-headed machines or
 * slower hardware).   Undefining RANDOM_MATRIX will cause the pixmaps
 * to be generated only once and should save alot of horsepower for
 * whatever else your machine might be doing.
 */

#define RANDOM_MATRIX


/* NAME: init_matrix
 *
 * FUNCTION: allocate space for global matrix array, 
 *           initialize colors,
 *           initialize dimensions
 *           allocate a pair of pixmaps containing character set 
 *               (0 is green on black, 1 is bold on black)
 *           create columns of "matrix" data
 *
 * RETURNS: void
 */

void init_matrix(ModeInfo * mi){
	matrix_t *mp;

	if( ! matrix ){
		if((matrix = (matrix_t *) calloc(MI_NUM_SCREENS(mi),
			sizeof(matrix_t)))==(matrix_t *)NULL){
			perror("calloc");
			return;
		}
	}

	MI_CLEARWINDOW(mi);    

	mp = PICK_MATRIX(mi);

	if (mp->columns == NULL)  {
		if (MI_NPIXELS(mi) > 2) {
			mp->fg = MI_PIXEL(mi, GREEN);
			mp->bold =  MI_PIXEL(mi, BRIGHTGREEN);
		} else {
			mp->fg = mp->bold = MI_WHITE_PIXEL(mi);
		}
		mp->bg = MI_BLACK_PIXEL(mi);
		mp->width = MI_WIDTH(mi);
		mp->height = MI_HEIGHT(mi);

		mp->kana[0] = XCreatePixmapFromBitmapData(MI_DISPLAY(mi),
			MI_WINDOW(mi), (char *) katakana_bits,
			katakana_width, katakana_height,
			mp->bg, mp->fg, MI_DEPTH(mi));

		mp->kana[1] = XCreatePixmapFromBitmapData(MI_DISPLAY(mi),
			MI_WINDOW(mi), (char *) katakana_bits,
			katakana_width, katakana_height,
			mp->bg, mp->bold, MI_DEPTH(mi));

		/* don't want any exposure events from XCopyArea */
	 	XSetGraphicsExposures(MI_DISPLAY(mi), MI_GC(mi), False);

		create_columns(mi,mp);
	}
}


/* NAME: draw_matrix
 *
 * FUNCTION: copy column pixmaps to the root window
 *
 * RETURNS: void
 */

void draw_matrix(ModeInfo *mi){
  matrix_t *mp = PICK_MATRIX(mi);
  int i,xoffset;

  xoffset = (mp->width / mp->num_cols);
  MI_IS_DRAWN(mi) = True;

  /* for each column in the current matrix
   *   move each pixmap down by increment it's offset by the column's speed
   *   copy each pixmap to the root window
   *   if a pixmap has scrolled off the bottom of the root window,
   *      reset it's yoffset so that it's bottom edge is at the top edge of
   *       the root window.
   *      generate a new speed to spice things up
   *   fi
   * rof
   */

  for(i=0;i<mp->num_cols;i++){

    mp->columns[i].yoffset[0] += mp->columns[i].speed;
    mp->columns[i].yoffset[1] += mp->columns[i].speed;

    XCopyArea(MI_DISPLAY(mi), mp->columns[i].pix[0],
	      MI_WINDOW(mi), MI_GC(mi),
	      0,0,
	      mp->col_width,mp->col_height,
	      (i+1)*xoffset,mp->columns[i].yoffset[0]);

    XCopyArea(MI_DISPLAY(mi), mp->columns[i].pix[1],
              MI_WINDOW(mi), MI_GC(mi),
	      0,0,
	      mp->col_width,mp->col_height,
	      (i+1)*xoffset,mp->columns[i].yoffset[1]);

    if( mp->columns[i].yoffset[0] > mp->height ){
      mp->columns[i].yoffset[0] = -(mp->height);
      mp->columns[i].speed = MATRIX_RANDOM(5,20); 
#ifdef RANDOM_DISPLAY
	  new_column(mi,mp,mp->columns[i].pix[0]);
#endif
    }

    if( mp->columns[i].yoffset[1] > mp->height ){
      mp->columns[i].yoffset[1] = -(mp->height);
      mp->columns[i].speed = MATRIX_RANDOM(5,20); 
#ifdef RANDOM_DISPLAY
	  new_column(mi,mp,mp->columns[i].pix[1]);
#endif
    }

  }
}

/* NAME: release_matrix
 *
 * FUNCTION: frees all allocated resources
 *
 * RETURNS: void
 */

void release_matrix(ModeInfo * mi){
  int i,j;

  /* free allocated resources:
   * 
   * foreach submatrix in matrix
   *   foreach column in submatrix
   *      free pixmaps
   *   free column
   * free matrix
   */

  /*  check matrix for NULL in the event that it's called twice
   */

  if (matrix != (matrix_t *) NULL) {
	for (i = 0; i< (sizeof(matrix) / sizeof(matrix_t)); i++) {
	  for (j = 0; j < matrix[i].num_cols; j++){
		XFreePixmap(MI_DISPLAY(mi),matrix[i].columns[j].pix[0]);
		XFreePixmap(MI_DISPLAY(mi),matrix[i].columns[j].pix[1]);
	  }
	  (void) free((void *) matrix[i].columns);
	  XFreePixmap(MI_DISPLAY(mi),matrix[i].kana[0]);
	  XFreePixmap(MI_DISPLAY(mi),matrix[i].kana[1]);
	}
	  (void) free((void *) matrix->columns);
	(void) free((void *) matrix);
	matrix = (matrix_t *)NULL;
  }
}

/* NAME: refresh_matrix
 *
 * FUNCTION: same as draw_matrix, clears screen to repair damage 
 *
 * RETURNS: void
 */

void refresh_matrix(ModeInfo * mi){

  MI_CLEARWINDOW(mi);			/* nuke anything that might have been drawn */
  
  draw_matrix(mi);
}

/* NAME: change_matrix
 *
 * FUNCTION: resets column offsets and generates new column pixmaps
 *
 * RETURNS: void
 */

void change_matrix(ModeInfo *mi){
  matrix_t *mp = PICK_MATRIX(mi);
  int i;

  /* 
   * 1. reset column pixmap offsets to inital conditions
   * 2. generate new column speed
   * 3. generate new pixmap contents
   */

  MI_CLEARWINDOW(mi);    
  for(i=0;i<mp->num_cols;i++){
	mp->columns[i].yoffset[0] = -(mp->height);
	mp->columns[i].yoffset[1] = -(mp->height * 2);
	mp->columns[i].speed = MATRIX_RANDOM(5,20); 
	new_column(mi,mp,mp->columns[i].pix[0]);
	new_column(mi,mp,mp->columns[i].pix[1]);
  }
}

/* NAME: get_color
 *
 * FUNCTION: wrapper function for XAllocNamedColor
 *
 * RETURNS: pixel value for requested named color
 */

Pixel get_color(ModeInfo *mi,char *colorname){
  XColor ret,exact;

  if( ! colorname || ! mi )
    return (Pixel )0;
    
  (void) XAllocNamedColor(MI_DISPLAY(mi),MI_COLORMAP(mi),colorname,&ret,&exact);

  return ret.pixel;
}

/* NAME: create_columns
 *
 * FUNCTION: based on the width and height of the display
 *           allocates and initializes column data structures
 *
 * RETURNS: void
 */

void create_columns(ModeInfo *mi,matrix_t *mp){
  int i;

  /* 
   * using the width of the selected font, determine how many
   * columns can fit across the current screen.  initalize the
   * column width and height for later use too.
   */
  mp->num_cols = mp->width / (katakana_cell_width * 1.3);
  mp->col_width = katakana_cell_width;
  mp->col_height = mp->height;

  /* 
   * allocate columns
   */
  mp->columns = (column_t *) calloc(mp->num_cols,sizeof(column_t));

  if( mp->columns == (column_t *)NULL ){
    perror("create_columns:calloc");
    return ;
  }

  /* 
   * initialize column data structures. 
   *
   *  1. pick a random speed
   *  2. set the yoffset's for each of the two pixmaps in a column
   *     the yoffset is relative to the origin of the root window
   *     and is gradually increased causing the pixmap to scroll
   *     downward from the top of the root window.
   *  3. create a pair of pixmaps.  a pair is used so that when the top
   *     edge of the first scrolls past the top of the root window, the
   *     second can begin scrolling down creating the effect of seamless
   *     scrolling.
   *  4. initalize the column pixmaps with a series of random strings and
   *     inter-string gaps.
   */

  for(i=0;i<mp->num_cols;i++){
    mp->columns[i].speed = MATRIX_RANDOM(5,20);

    mp->columns[i].yoffset[0] = -(mp->col_height);
    mp->columns[i].yoffset[1] = -(mp->col_height * 2);

    mp->columns[i].pix[0] = XCreatePixmap(MI_DISPLAY(mi), MI_WINDOW(mi),
	  mp->col_width, mp->col_height, MI_DEPTH(mi));
    mp->columns[i].pix[1] = XCreatePixmap(MI_DISPLAY(mi), MI_WINDOW(mi),
	  mp->col_width, mp->col_height, MI_DEPTH(mi));
    new_column(mi,mp,mp->columns[i].pix[0]);
    new_column(mi,mp,mp->columns[i].pix[1]);
  }
}


/* NAME: new_column
 *
 * FUNCTION: clears and generates a pixmap of "matrix" data
 *
 * RETURNS: void
 */

void new_column(ModeInfo *mi,matrix_t *mp,Pixmap pix){
  int yoffset,koffset,max;
  int i,first,gap_len,str_len;
  Pixmap src;

  /* 
   * clear the pixmap to get rid of previous data or initialize
   */

  XSetForeground(MI_DISPLAY(mi),MI_GC(mi),mp->bg);
  XFillRectangle(MI_DISPLAY(mi), pix, MI_GC(mi),
				 0,0, mp->col_width, mp->col_height);

  /* yoffset is the max. height of a cell in the katakana data + fudge */
  yoffset = katakana_cell_height + 2;

  /* max is the number of characters that fit in a column */
  max = mp->col_height / yoffset;

  /* the first character in a string (read up to down) is drawn in a
   * differnt color, the first flag helps choose the color.
   */
  first = 1;					

  /* 
   * each column is composed of a series of "strings" and inter-string
   * gaps. 
   */
  str_len = MATRIX_RANDOM(6,40); /* initial string length */
  gap_len = 0;

  /* 
   * write characters into the column pixmap, starting at the
   * bottom and moving upwards. 
   */

  for(i = max;i>=0;i--){

	src = (first)?mp->kana[1]:mp->kana[0];

	if( gap_len ){			/* either starting or in a gap */
	  gap_len--;
	  continue;
	} else{				/* either starting or in a string */
	  koffset = MATRIX_RANDOM(0,55);
	  str_len--;
	  first = 0;
	}

	XCopyArea(MI_DISPLAY(mi),src,pix,MI_GC(mi),
			  koffset*katakana_cell_width,0,
			  katakana_cell_width, katakana_cell_height,
			  0,i*yoffset);

	/* 
	 * if we have reached the end of the current string, so we generate
	 * a gap of random length.  the next string's length is also generated
	 * here, and the first flag set to indicate the beginning of a new
	 * string.
	 */

	if( str_len == 0 ){
	  gap_len = MATRIX_RANDOM(1,10);
	  str_len = MATRIX_RANDOM(6,40);
	  first = 1;
	}
  }
}
