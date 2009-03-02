#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)iostuff.c	5.03 2001/11/16 xlockmore";

#endif

/*-
 * iostuff.c - various file utilities for images, text, fonts
 *
 * Copyright (c) 1998 by David Bagley
 *
 * Revision History:
 *
 * Changes maintained by David Bagley <bagleyd@tux.org>
 * 16-Nov-2001: Keep same sel_image wildcard behavior as xlockmore 5.02 except
 *              on VMS so it works for directories. <tschmidt@micron.com>
 * 29-Oct-2001: Bug fix for sel_image by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
 *              It treated wildcards like wild*.* as being *wild*.*.
 * 23-Apr-1998: Separated out of util.c
 *
 */
#ifdef STANDALONE
#include "utils.h"
extern char *message, *messagefile, *messagesfile, *program;
extern char *messagefontname;

#else
#include "xlock.h"
#include "vis.h"
#include "color.h"
char       *message, *messagefile, *messagesfile, *program;
char       *messagefontname;

#endif
#include "mode.h"
#include "iostuff.h"
#include "random.h"
#ifdef USE_MAGICK
#undef inline
#include "magick.h"
#endif

#ifndef WIN32
#include <X11/Xutil.h>
#else
#include "Xapi.h"
#endif
#include <sys/stat.h>
#include <stdio.h>

static int
readable(char *filename)
{
	FILE       *fp;

	if ((fp = my_fopen(filename, "r")) == NULL)
		return False;
	(void) fclose(fp);
	return True;
}

FILE       *
my_fopen(char *filename, const char *type)
{
	FILE       *fp = (FILE *) NULL;
	int         s;
	struct stat fileStat;

	s = stat(filename, &fileStat);
	if ((s >= 0 && S_ISREG(fileStat.st_mode)) || (s < 0 && type[0] == 'w')) {
		if ((fp = fopen(filename, type)) == NULL)
			return (FILE *) NULL;
	} else {
		return (FILE *) NULL;
	}

	return fp;
}

FILE       *
my_fopenSize(char *filename, const char *type, int *size)
{
	FILE       *fp = (FILE *) NULL;
	int         s;
	struct stat fileStat;

	s = stat(filename, &fileStat);
	*size = fileStat.st_size;
	if ((s >= 0 && S_ISREG(fileStat.st_mode)) || (s < 0 && type[0] == 'w')) {
		if ((fp = fopen(filename, type)) == NULL)
			return (FILE *) NULL;
	} else {
		return (FILE *) NULL;
	}

	return fp;
}

#if HAVE_DIRENT_H
static void
randomFileFromList(char *directory,
		   struct dirent **filelist, int numfiles, char *file_local)
{
	int         num;

	num = NRAND(numfiles);
	if (strlen(directory) + strlen(filelist[num]->d_name) + 1 < 256) {
		(void) sprintf(file_local, "%s%s",
			       directory, filelist[num]->d_name);
	}
}

#endif

#if HAVE_DIRENT_H

extern Bool debug;

/* index_dir emulation of FORTRAN's index in C. Author: J. Jansen */
int
index_dir(char *str1, char *substr)
{
	int         i, num, l1 = strlen(str1), ls = strlen(substr), found;
	char       *str1_tmp, *substr_tmp, *substr_last;

	num = l1 - ls + 1;
	substr_last = substr + ls;
	for (i = 0; i < num; ++i) {
		str1_tmp = str1 + i;
		substr_tmp = substr;
		found = 1;
		while (substr_tmp < substr_last)
			if (*str1_tmp++ != *substr_tmp++) {
				found = 0;
				break;
			}
		if (found)
			return (i + 1);
	}
	return (0);
}

#ifdef VMS
/* Upcase string  Author: J. Jansen */
static void
upcase(char *s)
{
	int         i;

	for (i = 0; s[i] != '\0'; i++) {
		if (s[i] >= 'a' && s[i] <= 'z') {
			s[i] = (char) (s[i] - 32);
		}
	}
}

#endif

/* Split full path into directory and filename parts  Author: J. Jansen */
void
get_dir(char *fullpath, char *dir, char *filename)
{
	char       *ln;
	int         ip_temp = 0, ip;

#ifdef VMS
	ip = index_dir(fullpath, "]");
#else
	ln = fullpath;
	ip = 0;
	while ((ip_temp = index_dir(ln, (char *) "/"))) {
		ip = ip + ip_temp;
		ln = fullpath + ip;
	}
#endif
	if (ip == 0) {
#ifdef VMS
		(void) strcpy(dir, "[]");
#else
		(void) strcpy(dir, "./");
#endif
	} else {
		if (ip > DIRBUF - 1)
			ip_temp = DIRBUF - 1;
		else
			ip_temp = ip;
		(void) strncpy(dir, fullpath, ip_temp);
		dir[ip_temp] = '\0';
	}
	ln = fullpath + ip;
	(void) strncpy(filename, ln, MAXNAMLEN - 1);
	filename[MAXNAMLEN - 1] = '\0';
#ifdef VMS
	upcase(filename);	/* VMS knows uppercase filenames only */
#else
	{
		DIR        *dirp;

		/* This finds a directory if there is no final slash */
		if (filename[0] && (dirp = opendir(fullpath)) != NULL) {
			(void) closedir(dirp);
			(void) strncpy(dir, fullpath, DIRBUF - 1);
			ip = strlen(fullpath);
			if (ip < DIRBUF - 2) {
				dir[ip] = '/';
				dir[ip + 1] = '\0';
			}
			dir[DIRBUF - 1] = '\0';
			filename[0] = '\0';
		}
	}
#endif
	if (debug)
		(void) printf("get_dir %s %s %s\n", fullpath, dir, filename);
}

extern char filename_r[MAXNAMLEN];

/* Procedure to select the matching filenames  Author: J. Jansen */
int
sel_image(struct dirent *name)
{
	char       *name_tmp = name->d_name;
	char       *filename_tmp = filename_r;
	int         numfrag = -1, ip, i = -1;
#define MAX_FRAGS 64
	char *frags[MAX_FRAGS];
#ifdef VMS
        Bool not_first_star = True;

	upcase( name_tmp ); /* VMS-file system is not case selective */
        if ( *filename_tmp == '*' )
	  {
	    not_first_star = False;
	    filename_tmp++;
	  }
#endif

	if (numfrag == -1) {
		++numfrag;
		while ((ip = index_dir(filename_tmp, (char *) "*"))) {
			if (numfrag >= MAX_FRAGS) {
				numfrag--;
				goto FAIL;
			}
			if ((frags[numfrag] = (char *) malloc(ip)) == NULL) {
				numfrag--;
				goto FAIL;
			}
			(void) strcpy(frags[numfrag], "\0");
			(void) strncat(frags[numfrag], filename_tmp, ip - 1);
			++numfrag;
			filename_tmp = filename_tmp + ip;
		}
		if ( strlen( filename_tmp ) != 0 ) {
			if (numfrag > 0 && frags[numfrag] != NULL)
				free(frags[numfrag]);
			if ((frags[numfrag] = (char *) malloc(strlen(filename_tmp) +
					1)) == NULL) {
				numfrag--;
				goto FAIL;
			}
			(void) strcpy(frags[numfrag], filename_tmp);
		} else
			numfrag--;
	}

#ifdef VMS
	i = 0;
	ip = index_dir(name_tmp, frags[i]);
	free(frags[i]);
	if ( not_first_star ) {
		if ( ip != 1 )
			goto FAIL;
	} else {
		if ( ip == 0 )
			goto FAIL;
	}
	name_tmp = name_tmp + ip;

	if ( numfrag > 0 ) {
		for (i = 1; i <= numfrag; ++i) {
#else
		for (i = 0; i <= numfrag; ++i) {
#endif
			ip = index_dir(name_tmp, frags[i]);
			free(frags[i]);
			if (ip == 0) {
				goto FAIL;
			}
			name_tmp = name_tmp + ip;
		}
#ifdef VMS
	}
#endif

	return (1);
FAIL:
	{
		int         j;

		for (j = i + 1; j <= numfrag; ++j)
			free(frags[j]);
		return (0);
	}
}

/* scandir implementiation for VMS  Author: J. Jansen */
/* name changed to scan_dir to solve portablity problems */
#define _MEMBL_ 64
int
scan_dir(const char *directoryname, struct dirent ***namelist,
	 int         (*specify) (struct dirent *),
	 int         (*compare) (const void *, const void *))
{
	DIR        *dirp;
	struct dirent *new_entry, **namelist_tmp;
	int         size_tmp, num_list_tmp;

	if (debug)
		(void) printf("scan_dir directoryname %s\n", directoryname);
	if ((dirp = opendir(directoryname)) == NULL) {
		if (debug)
			(void) printf("scan_dir can not open directoryname %s\n", directoryname);
		return (-1);
	}
	size_tmp = _MEMBL_;
/*-
 * PURIFY on SunOS4 and on Solaris 2 reports a cumulative memory leak on
 * the next line when used with the modes/glx/text3d.cc file. It only
 * leaks like this for the C++ modes, and is OK in the C modes. */
	if ((namelist_tmp = (struct dirent **) malloc(size_tmp *
			sizeof (struct dirent *))) == NULL) {
		if (debug)
			(void) printf("scan_dir no memory\n");
		return (-1);
	}
	num_list_tmp = 0;
	while ((new_entry = readdir(dirp)) != NULL) {
#ifndef VMS
		if (!strcmp(new_entry->d_name, ".") || !strcmp(new_entry->d_name, ".."))
			continue;
#endif
		if (specify != NULL && !(*specify) (new_entry))
			continue;
		if (++num_list_tmp >= size_tmp) {
			size_tmp = size_tmp + _MEMBL_;
			if ((namelist_tmp = (struct dirent **) realloc(
					(void *) namelist_tmp, size_tmp *
					sizeof (struct dirent *))) == NULL) {
				if (debug)
					(void) printf("scan_dir no memory\n");
				return (-1);
			}
		}
		if ((namelist_tmp[num_list_tmp - 1] =
			(struct dirent *) malloc(sizeof (struct dirent)
#ifdef SVR4
			 + strlen(new_entry->d_name)
#endif
		)) == NULL)
				return (-1);

		(void) strcpy(namelist_tmp[num_list_tmp - 1]->d_name, new_entry->d_name);

		*namelist = namelist_tmp;
	}

	(void) closedir(dirp);
	if (num_list_tmp && compare != NULL)
		(void) qsort((void *) namelist_tmp, num_list_tmp,
			     sizeof (struct dirent *), compare);
	if (debug)
		(void) printf("scan_dir number %d\n", num_list_tmp);
	*namelist = namelist_tmp;
	return (num_list_tmp);
}

#endif

#if HAVE_DIRENT_H
extern char directory_r[DIRBUF];
extern struct dirent **image_list;
extern int  num_list;

static void
getRandomFile(char *randomfile, char *randomfile_local)
{
	struct dirent ***images_list = (struct dirent ***) NULL;

	get_dir(randomfile, directory_r, filename_r);
	if (image_list != NULL) {
		int         i;

		for (i = 0; i < num_list; i++) {
			if (image_list[i] != NULL)
				free(image_list[i]);
		}
		free(image_list);
		image_list = (struct dirent **) NULL;
	}
	if ((images_list = (struct dirent ***) malloc(sizeof (struct dirent **))) != NULL) {
		num_list = scan_dir(directory_r, images_list, sel_image, NULL);
		image_list = *images_list;
		free(images_list);
		if (num_list > 0) {
			randomFileFromList(directory_r, image_list, num_list,
				randomfile_local);
		} else if (num_list < 0) {
			num_list = 0;
			image_list = (struct dirent **) NULL;
		}
	} else
		num_list = 0;
}

#endif

extern int  XbmReadFileToImage(char *filename,
			       int *width, int *height, unsigned char **bits);

#ifdef HAVE_XPM
#include <X11/xpm.h>
#endif
#ifndef USE_MAGICK
# include "ras.h"
#endif

static XImage blogo =
{
	0, 0,			/* width, height */
	0, XYBitmap, 0,		/* xoffset, format, data */
	LSBFirst, 8,		/* byte-order, bitmap-unit */
	LSBFirst, 8, 1		/* bitmap-bit-order, bitmap-pad, depth */
};

void
getImage(ModeInfo * mi, XImage ** logo,
	 int default_width, int default_height, unsigned char *default_bits,
#ifdef HAVE_XPM
	 int default_xpm, char **name,
#endif
	 int *graphics_format, Colormap * ncm,
	 unsigned long *black)
{
	Display    *display = MI_DISPLAY(mi);
	static char *bitmap_local = (char *) NULL;

#ifndef STANDALONE
#ifdef HAVE_XPM
	XpmAttributes attrib;

#endif
#if 0
	/* This probably works best in most cases but for random mode used
	   with random selection of a file it will fail often. */
	*ncm = None;
#else
	if (!fixedColors(mi))
		*ncm = XCreateColormap(display, MI_WINDOW(mi), MI_VISUAL(mi), AllocNone);
	else
		*ncm = None;
#endif
#ifdef HAVE_XPM
	attrib.visual = MI_VISUAL(mi);
	if (*ncm == None) {
		attrib.colormap = MI_COLORMAP(mi);
	} else {
		attrib.colormap = *ncm;
	}
	attrib.depth = MI_DEPTH(mi);
	attrib.valuemask = XpmVisual | XpmColormap | XpmDepth;
#endif
#endif /* !STANDALONE */
	*graphics_format = 0;

	if (bitmap_local != NULL) {
		free(bitmap_local);
		bitmap_local = (char *) NULL;
	}
	if (MI_BITMAP(mi) && strlen(MI_BITMAP(mi))) {
#ifdef STANDALONE
		bitmap_local = MI_BITMAP(mi);
#else
		if ((bitmap_local = (char *) malloc(256)) == NULL) {
			(void) fprintf(stderr , "no memory for \"%s\"\n" ,
				MI_BITMAP(mi));
			return;
		}
		(void) strncpy(bitmap_local, MI_BITMAP(mi), 256);
#if HAVE_DIRENT_H
		getRandomFile(MI_BITMAP(mi), bitmap_local);
#endif
#endif /* STANDALONE */
	}
	if (bitmap_local && strlen(bitmap_local)) {
#if defined( USE_MAGICK ) && !defined( STANDALONE )
	   if ( readable( bitmap_local ) )
	     {
		if ( MI_NPIXELS( mi ) > 2 )
		  {
		     if ( MagickSuccess == MagickFileToImage( mi ,
							     bitmap_local ,
							     logo) )
		       {
			  *graphics_format = IS_MAGICKFILE;
			  if ( !fixedColors( mi ) )
			    SetImageColors( display , *ncm );
			  *black = GetColor(mi, MI_BLACK_PIXEL(mi));
			  (void) GetColor(mi, MI_WHITE_PIXEL(mi));
			  (void) GetColor(mi, MI_BG_PIXEL(mi));
			  (void) GetColor(mi, MI_FG_PIXEL(mi));
		       }
		  }
	     }
	   else
	     {
		(void) fprintf(stderr , "could not read file \"%s\"\n" ,
			bitmap_local);
	     }
#else
# ifndef STANDALONE
		if (readable(bitmap_local)) {
			if (MI_NPIXELS(mi) > 2) {
				if (RasterSuccess == RasterFileToImage(mi, bitmap_local, logo)) {
					*graphics_format = IS_RASTERFILE;
					if (!fixedColors(mi))
						SetImageColors(display, *ncm);
					*black = GetColor(mi, MI_BLACK_PIXEL(mi));
					(void) GetColor(mi, MI_WHITE_PIXEL(mi));
					(void) GetColor(mi, MI_BG_PIXEL(mi));
					(void) GetColor(mi, MI_FG_PIXEL(mi));
				}
			}
		} else {
			(void) fprintf(stderr,
			       "could not read file \"%s\"\n", bitmap_local);
		}
#ifdef HAVE_XPM
#ifndef USE_MONOXPM
		if (MI_NPIXELS(mi) > 2)
#endif
		{
			if (*graphics_format <= 0) {
				if (*ncm != None)
					reserveColors(mi, *ncm, black);
				if (XpmSuccess == XpmReadFileToImage(display,
				      bitmap_local, logo,
				      (XImage **) NULL, &attrib))
					*graphics_format = IS_XPMFILE;
			}
		}
#endif
#endif /* !STANDALONE */
		if (*graphics_format <= 0) {
			if (!blogo.data) {
				if (BitmapSuccess == XbmReadFileToImage(bitmap_local,
						 &blogo.width, &blogo.height,
					   (unsigned char **) &blogo.data)) {
					blogo.bytes_per_line = (blogo.width + 7) / 8;
					*graphics_format = IS_XBMFILE;
					*logo = &blogo;
				}
			} else {
				*graphics_format = IS_XBMDONE;
				*logo = &blogo;
			}
		}
#endif
	   if (*graphics_format <= 0 && MI_IS_VERBOSE(mi))
			(void) fprintf(stderr,
				       "\"%s\" is in an unrecognized format or not compatible with screen\n",
				       bitmap_local);
	}
#ifndef STANDALONE
#ifdef HAVE_XPM
	if (*graphics_format <= 0 &&
	    ((MI_IS_FULLRANDOM(mi)) ? LRAND() & 1: default_xpm))
#ifndef USE_MONOXPM
		if (MI_NPIXELS(mi) > 2)
#endif
			if (XpmSuccess == XpmCreateImageFromData(display, name,
					    logo, (XImage **) NULL, &attrib))
				*graphics_format = IS_XPM;
#endif
#endif /* STANDALONE */
	if (*graphics_format <= 0) {
		if (!blogo.data) {
			blogo.data = (char *) default_bits;
			blogo.width = default_width;
			blogo.height = default_height;
			blogo.bytes_per_line = (blogo.width + 7) / 8;
			*graphics_format = IS_XBM;
		} else
			*graphics_format = IS_XBMDONE;
		*logo = &blogo;
	}
#ifndef STANDALONE		/* Come back later */
	if (*ncm != None && *graphics_format != IS_RASTERFILE &&
	    *graphics_format != IS_XPMFILE && *graphics_format != IS_XPM &&
	    *graphics_format != IS_MAGICKFILE) {
		XFreeColormap(display, *ncm);
		*ncm = None;
	}
#endif	/* STANDALONE */ /* Come back later */
}

void
destroyImage(XImage ** logo, int *graphics_format)
{
	switch (*graphics_format) {
		case IS_XBM:
			blogo.data = (char *) NULL;
			break;
		case IS_XBMFILE:
			if (blogo.data) {
				free(blogo.data);
				blogo.data = (char *) NULL;
			}
			break;
		case IS_XPM:
		case IS_XPMFILE:
		case IS_RASTERFILE:
		case IS_MAGICKFILE:
			if (logo && *logo)
				(void) XDestroyImage(*logo);
			break;
	}
	*graphics_format = -1;
	*logo = (XImage *) NULL;
}

void
pickPixmap(Display * display, Drawable drawable, char *name,
	   int default_width, int default_height, unsigned char *default_bits,
	   int *width, int *height, Pixmap * pixmap,
	   int *graphics_format)
{
	int         x_hot, y_hot;	/* dummy */

	if (name && *name) {
		if (readable(name)) {
			if (BitmapSuccess == XReadBitmapFile(display, drawable, name,
			     (unsigned int *) width, (unsigned int *) height,
						   pixmap, &x_hot, &y_hot)) {
				*graphics_format = IS_XBMFILE;
			}
			if (*graphics_format <= 0)
				(void) fprintf(stderr,
					    "\"%s\" not xbm format\n", name);
		} else {
			(void) fprintf(stderr,
				       "could not read file \"%s\"\n", name);
		}
	}
	if (*graphics_format <= 0) {
		*width = default_width;
		*height = default_height;
		*graphics_format = IS_XBM;
		*pixmap = XCreateBitmapFromData(display, drawable,
				     (char *) default_bits, *width, *height);
	}
}

void
getPixmap(ModeInfo * mi, Drawable drawable,
	  int default_width, int default_height, unsigned char *default_bits,
	  int *width, int *height, Pixmap * pixmap,
	  int *graphics_format)
{
	Display    *display = MI_DISPLAY(mi);
	static char *bitmap_local = (char *) NULL;

	if (bitmap_local) {
		free(bitmap_local);
		bitmap_local = (char *) NULL;
	}
	if (MI_BITMAP(mi) && strlen(MI_BITMAP(mi))) {
#ifdef STANDALONE
		bitmap_local = MI_BITMAP(mi);
#else
		if ((bitmap_local = (char *) malloc(256)) == NULL) {
			(void) fprintf(stderr , "no memory for \"%s\"\n" ,
				MI_BITMAP(mi));
			return;
		}
		(void) strncpy(bitmap_local, MI_BITMAP(mi), 256);
#if HAVE_DIRENT_H
		getRandomFile(MI_BITMAP(mi), bitmap_local);
#endif
#endif /* STANDALONE */
	}
	pickPixmap(display, drawable, bitmap_local,
		   default_width, default_height, default_bits,
		   width, height, pixmap, graphics_format);
}

char *
getModeFont(char *infont)
{
	static char *localfont = (char *) NULL;

	if (localfont != NULL) {
		free(localfont);
		localfont = (char *) NULL;
	}
	if (infont && strlen(infont)) {
#ifdef STANDALONE
		localfont = infont;
#else
		if ((localfont = (char *) malloc(256)) == NULL) {
			(void) fprintf(stderr , "no memory for \"%s\"\n" ,
				infont);
			return (char *) NULL;
		}
		(void) strncpy(localfont, infont, 256);
#if HAVE_DIRENT_H
		getRandomFile(infont, localfont);
#endif
#endif /* STANDALONE */
	}
	if (localfont && strlen(localfont) && !readable(localfont)) {
		(void) fprintf(stderr,
		       "could not read file \"%s\"\n", localfont);
		if (localfont) {
			free(localfont);
			localfont = (char *) NULL;
		}
	}
	return localfont;
}

#define FROM_PROGRAM 1
#define FROM_FORMATTEDFILE    2
#define FROM_FILE    3
#define FROM_RESRC   4

static char *def_words = (char *) "I'm out running around.";
static int  getwordsfrom;

static void
strcat_firstword(char *fword, char *words)
{
	while (*fword)
		fword++;
	while (*words && !(isspace((int) *words)))
		*fword++ = *words++;
	*fword = '\0';
}

int
isRibbon(void)
{
	return (getwordsfrom == FROM_RESRC);
}

#if !defined(__cplusplus) && !defined(c_plusplus)
extern int  pclose(FILE *);
#endif

char *
getWords(int screen, int screens)
{
	FILE       *pp;
	static char *buf = (char *) NULL, progerr[BUFSIZ], progrun[BUFSIZ];
	register char *p;
	int         i;


	if (buf == NULL) {
		if ((buf = (char *) calloc(screens * BUFSIZ,
				sizeof (char))) == NULL)
			return (char *) NULL;
	}
	p = &buf[screen * BUFSIZ];
	*p = '\0';
	if (message && *message)
		getwordsfrom = FROM_RESRC;
	else if (messagefile && *messagefile) {
		getwordsfrom = FROM_FILE;
	} else if (messagesfile && *messagesfile) {
		getwordsfrom = FROM_FORMATTEDFILE;
	} else {
		getwordsfrom = FROM_PROGRAM;

		(void) sprintf(progrun, "( %s ) 2>&1",
			    (!program || !*program) ? DEF_PROGRAM : program);
	}

	switch (getwordsfrom) {
#ifndef VMS
		case FROM_PROGRAM:
/*-
 * PURIFY 4.0.1 on SunOS4 and on Solaris2 reports a duplication of file
 *  descriptor on the next line.  Do not know if this is a problem or not. */
			if ((pp = (FILE *) popen(progrun, "r")) != NULL) {
				while (fgets(p, BUFSIZ - strlen(&buf[screen * BUFSIZ]), pp)) {
					if (strlen(&buf[screen * BUFSIZ]) + 1 < BUFSIZ)
						p = &buf[screen * BUFSIZ] + strlen(&buf[screen * BUFSIZ]);
					else
						break;
				}
				(void) pclose(pp);
				p = &buf[screen * BUFSIZ];
				if (!buf[screen * BUFSIZ])
					(void) sprintf(&buf[screen * BUFSIZ], "\"%s\" produced no output!",
					 (!program) ? DEF_PROGRAM : program);
				else {
					(void) memset((char *) progerr, 0, sizeof (progerr));
					(void) strcpy(progerr, "sh: ");
					strcat_firstword(progerr, (!program || !*program) ?
						      (char *) DEF_PROGRAM : program);
					(void) strcat(progerr, ": not found\n");
					if (!strcmp(&buf[screen * BUFSIZ], progerr))
						switch (NRAND(12)) {
							case 0:
								(void) strcat(&buf[screen * BUFSIZ],
									      "( Get with the program, bub. )\n");
								break;
							case 1:
								(void) strcat(&buf[screen * BUFSIZ],
									      "( I blow my nose at you, you silly person! )\n");
								break;
							case 2:
								(void) strcat(&buf[screen * BUFSIZ],
									      "\nThe resource you want to\nset is `program'.\n");
								break;
							case 3:
								(void) strcat(&buf[screen * BUFSIZ],
									      "\nHelp!!  Help!!\nAAAAAAGGGGHHH!!  \n\n");
								break;
							case 4:
								(void) strcat(&buf[screen * BUFSIZ],
									      "( Hey, who called me `Big Nose'? )\n");
								break;
							case 5:
								(void) strcat(&buf[screen * BUFSIZ],
									      "( Hello?  Are you paying attention? )\n");
								break;
							case 6:
								(void) strcat(&buf[screen * BUFSIZ],
									      "sh: what kind of fool do you take me for? \n");
								break;
							case 7:
								(void) strcat(&buf[screen * BUFSIZ],
									      "\nRun me with -program \"fortune -o\".\n");
								break;
							case 8:
								(void) strcat(&buf[screen * BUFSIZ],
									      "( Where is your fortune? )\n");
								break;
							case 9:
								(void) strcat(&buf[screen * BUFSIZ],
									      "( Your fortune has not been written yet!! )");
								break;
						}
				}
				p = &buf[screen * BUFSIZ];
			} else {
				perror(progrun);
				p = def_words;
			}
			break;
#endif
		case FROM_FORMATTEDFILE:
			if ((pp = my_fopen(messagesfile, "r")) != NULL) {
				int         len_mess_file;

				if (fscanf(pp, "%d", &len_mess_file)) {
					int         no_quote;

					if (len_mess_file <= 0)
						buf[screen * BUFSIZ] = '\0';
					else {
						(void) fgets(p, BUFSIZ - strlen(&buf[screen * BUFSIZ]), pp);
						/* get first '%%' (the one after the number of quotes) */
						(void) fgets(p, BUFSIZ - strlen(&buf[screen * BUFSIZ]), pp);
						no_quote = NRAND(len_mess_file);
						for (i = 0; i <= no_quote; ++i) {
							unsigned int len_cur = 0;

							buf[screen * BUFSIZ] = '\0';
							p = &buf[screen * BUFSIZ] + strlen(&buf[screen * BUFSIZ]);
							while (fgets(p, BUFSIZ - strlen(&buf[screen * BUFSIZ]), pp)) {
								if (strlen(&buf[screen * BUFSIZ]) + 1 < BUFSIZ) {
									/* a line with '%%' contains 3 characters */
									if ((strlen(&buf[screen * BUFSIZ]) == len_cur + 3) &&
									    (p[0] == '%') && (p[1] == '%')) {
										p[0] = '\0';	/* get rid of "%%" in &buf[screen * BUFSIZ] */
										break;
									} else {
										p = &buf[screen * BUFSIZ] + strlen(&buf[screen * BUFSIZ]);
										len_cur = strlen(&buf[screen * BUFSIZ]);
									}
								} else
									break;
							}
						}
					}
					(void) fclose(pp);
					if (!buf[screen * BUFSIZ])
						(void) sprintf(&buf[screen * BUFSIZ],
							       "file \"%s\" is empty!", messagesfile);
					p = &buf[screen * BUFSIZ];
				} else {
					(void) sprintf(&buf[screen * BUFSIZ],
						       "file \"%s\" not in correct format!", messagesfile);
					p = &buf[screen * BUFSIZ];
				}
			} else {
				(void) sprintf(&buf[screen * BUFSIZ],
				"could not read file \"%s\"!", messagesfile);
				p = &buf[screen * BUFSIZ];
			}
			break;
		case FROM_FILE:
			if ((pp = my_fopen(messagefile, "r")) != NULL) {
				while (fgets(p, BUFSIZ - strlen(&buf[screen * BUFSIZ]), pp)) {
					if (strlen(&buf[screen * BUFSIZ]) + 1 < BUFSIZ)
						p = &buf[screen * BUFSIZ] + strlen(&buf[screen * BUFSIZ]);
					else
						break;
				}
				(void) fclose(pp);
				if (!buf[screen * BUFSIZ])
					(void) sprintf(&buf[screen * BUFSIZ],
					"file \"%s\" is empty!", messagefile);
				p = &buf[screen * BUFSIZ];
			} else {
				(void) sprintf(&buf[screen * BUFSIZ],
				 "could not read file \"%s\"!", messagefile);
				p = &buf[screen * BUFSIZ];
			}
			break;
		case FROM_RESRC:
			p = message;
			break;
		default:
			p = def_words;
			break;
	}

	if (!p || *p == '\0')
		p = def_words;
	return p;
}

XFontStruct *
getFont(Display * display)
{
	XFontStruct *messagefont;

	if (!(messagefont = XLoadQueryFont(display,
		   (messagefontname) ? messagefontname : DEF_MESSAGEFONT))) {
		if (messagefontname) {
			(void) fprintf(stderr, "can not find font: %s, using %s...\n",
				       messagefontname, DEF_MESSAGEFONT);
			messagefont = XLoadQueryFont(display, DEF_MESSAGEFONT);
		}
		if (!(messagefont)) {
			(void) fprintf(stderr, "can not find font: %s, using %s...\n",
				       DEF_MESSAGEFONT, FALLBACK_FONTNAME);
			messagefont = XLoadQueryFont(display, FALLBACK_FONTNAME);
			if (!messagefont) {
				(void) fprintf(stderr, "can not even find %s!!!\n", FALLBACK_FONTNAME);
				return (None);	/* Do not want to exit when in a mode */
			}
		}
	}
	return messagefont;
}

#ifdef USE_MB
XFontSet
getFontSet(Display * display)
{
	char **miss, *def;
	int n_miss;
	XFontSet fs;

	fs = XCreateFontSet(display,
			    DEF_MESSAGEFONTSET,
			    &miss, &n_miss, &def);
	return fs;
}
#endif
