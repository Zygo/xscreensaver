#ifndef _FILE_H_
#define _FILE_H_

#ifdef STANDALONE
#ifdef HAVE_XPM
#define USE_XPM
#endif
#endif /* STANDALONE */

#define FALLBACK_FONTNAME "fixed"
#ifndef DEF_MESSAGEFONT
#define DEF_MESSAGEFONT "-*-times-*-*-*-*-18-*-*-*-*-*-*-*"
#endif
#ifndef DEF_PROGRAM		/* Try the -o option ;) */
#define DEF_PROGRAM "fortune -s"
#endif

#define IS_NONE 0
#define IS_XBMDONE 1		/* Only need one mono image */
#define IS_XBM 2
#define IS_XBMFILE 3
#define IS_XPM 4
#define IS_XPMFILE 5
#define IS_RASTERFILE 6

extern FILE *my_fopen(char *, char *);

extern void getImage(ModeInfo * mi, XImage ** logo,
	  int default_width, int default_height, unsigned char *default_bits,
#if defined( USE_XPM ) || defined( USE_XPMINC )
		     int default_xpm, char **name,
#endif
		     int *graphics_format, Colormap * newcolormap,
		     unsigned long *black);
extern void destroyImage(XImage ** logo, int *graphics_format);
extern void getPixmap(ModeInfo * mi, Drawable drawable,
	  int default_width, int default_height, unsigned char *default_bits,
		      int *width, int *height, Pixmap * pixmap,
		      int *graphics_format);

#endif /* _FILE_H_ */
