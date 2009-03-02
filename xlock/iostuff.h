#ifndef _FILE_H_
#define _FILE_H_

#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)iostuff.h	4.14 99/06/17 xlockmore" */

#endif

/*-
 * IO stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-06-99: Started log. :)
 */

#ifdef STANDALONE
#ifdef HAVE_XPM
#define USE_XPM
#endif
#endif /* STANDALONE */

#define FALLBACK_FONTNAME "fixed"
#ifndef DEF_MESSAGEFONT
#define DEF_MESSAGEFONT "-*-times-*-*-*-*-18-*-*-*-*-*-*-*"
#define DEF_MESSAGEFONTSET "-*-*-medium-r-normal-*-18-*-*-*-*-*-*-*"
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
#define IS_MAGICKFILE 7


#ifdef __cplusplus
  extern "C" {
#endif

extern FILE *my_fopen(char *, const char *);
int index_dir(char *str1, char *substr);
extern void get_dir(char *fullpath, char *dir, char *filename);
#if HAVE_DIRENT_H
extern int  sel_image(struct dirent *name);
extern int scan_dir(const char *directoryname, struct dirent ***namelist,
   int         (*specify) (struct dirent *),
   int         (*compare) (const void *, const void *));
#endif

extern int isRibbon(void);
extern char * getWords(int screen, int screens);
extern XFontStruct * getFont(Display * display);

extern void pickPixmap(Display * display, Drawable drawable, char *name,
   int default_width, int default_height,
	 unsigned char *default_bits,
   int *width, int *height, Pixmap * pixmap,
   int *graphics_format);
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

extern char * getModeFont(char *infont);
#ifdef __cplusplus
  }
#endif

#endif /* _FILE_H_ */
