
#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)utils.c	4.03 97/05/08 xlockmore";

#endif

/*-
 * utils.c - various utilities - usleep, seconds, SetRNG, LongRNG, hsbramp.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * Revision History:
 *
 * Changes maintained by David Bagley <bagleyd@bigfoot.com>
 * 08-Jul-96: Bug in strcat_firstword fixed thanks to
              <Jeffrey_Doggett@caradon.com>.  Fix for ":not found" text
              that appears after about 40 minutes.
 * 04-Apr-96: Added procedures to handle wildcards on filenames
 *            J. Jansen <joukj@crys.chem.uva.nl>
 * 15-May-95: random number generator added, moved hsbramp.c to utils.c .
 *            Also renamed file from usleep.c to utils.c .
 * 14-Mar-95: patches for rand and seconds for VMS
 * 27-Feb-95: fixed nanosleep for times >= 1 second
 * 05-Jan-95: nanosleep for Solaris 2.3 and greater Greg Onufer
 *            <Greg.Onufer@Eng.Sun.COM>
 * 22-Jun-94: Fudged for VMS by Anthony Clarke
 *            <Anthony.D.Clarke@Support.Hatfield.Raytheon.bae.eurokom.ie>
 * 10-Jun-94: patch for BSD from Victor Langeveld <vic@mbfys.kun.nl>
 * 02-May-94: patch for Linux, got ideas from Darren Senn's xlock
 *            <sinster@scintilla.capitola.ca.us>
 * 21-Mar-94: patch fix for HP from <R.K.Lloyd@csc.liv.ac.uk> 
 * 01-Dec-93: added patch for HP
 *
 * Changes of Patrick J. Naughton
 * 30-Aug-90: written.
 *
 */

#include "xlock.h"

XPoint hexagonUnit[6] =
{
  {0, 0},
  {1, 1},
  {0, 2},
  {-1, 1},
  {-1, -1},
  {0, -2}
};

XPoint triangleUnit[2][3] =
{
  {
    {0, 0},
    {1, -1},
    {0, 2}
	},
  {
    {0, 0},
    {-1, 1},
    {0, -2}
	}
};

#include <sys/stat.h>

#ifdef USE_OLD_EVENT_LOOP
#if !defined( VMS ) || defined( XVMSUTILS ) || ( __VMS_VER >= 70000000 )
#ifdef USE_XVMSUTILS
#include <X11/unix_time.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif
#else
#include <starlet.h>
#endif
#if defined( SYSV ) || defined( SVR4 )
#ifdef LESS_THAN_AIX3_2
#include <sys/poll.h>
#else /* !LESS_THAN_AIX3_2 */
#include <poll.h>
#endif /* !LESS_THAN_AIX3_2 */
#endif /* defined( SYSV ) || defined( SVR4 ) */

#ifndef HAVE_USLEEP
 /* usleep should be defined */
int
usleep(unsigned long usec)
{
#if (defined( SYSV ) || defined( SVR4 )) && !defined( __hpux )
#ifdef HAVE_NANOSLEEP
	{
		struct timespec rqt;

		rqt.tv_nsec = 1000 * (usec % (unsigned long) 1000000);
		rqt.tv_sec = usec / (unsigned long) 1000000;
		return nanosleep(&rqt, NULL);
	}
#else /* !HAVE_NANOSLEEP */
	(void) poll((void *) 0, (int) 0, usec / 1000);	/* ms resolution */
#endif /* !HAVE_NANOSLEEP */
#else /* !SYSV */
#if HAVE_GETTIMEOFDAY
	struct timeval time_out;

	time_out.tv_usec = usec % (unsigned long) 1000000;
	time_out.tv_sec = usec / (unsigned long) 1000000;
	(void) select(0, (void *) 0, (void *) 0, (void *) 0, &time_out);
#else
	long        timadr[2];

	if (usec != 0) {
		timadr[0] = -usec * 10;
		timadr[1] = -1;

		sys$setimr(4, &timadr, 0, 0, 0);
		sys$waitfr(4);
	}
#endif
#endif /* !SYSV */
	return 0;
}
#endif /* !HAVE_USLEEP */
#endif /* USE_OLD_EVENT_LOOP */

/*-
 * returns the number of seconds since 01-Jan-70.
 * This is used to control rate and timeout in many of the animations.
 */
unsigned long
seconds(void)
{
#if HAVE_GETTIMEOFDAY
	struct timeval now;

	(void) gettimeofday(&now, NULL);
	return (unsigned long) now.tv_sec;
#else
	return (unsigned long) time((time_t *) 0);
#endif
}

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
my_fopen(char *filename, char *type)
{
	FILE       *fp = NULL;
	int         s;
	struct stat fileStat;

	s = stat(filename, &fileStat);
	if (s >= 0 && S_ISREG(fileStat.st_mode)) {
		if ((fp = fopen(filename, type)) == NULL)
			return NULL;
	} else {
		return NULL;
	}

	return fp;
}

#if (! HAVE_STRDUP )
char       *
strdup(char *str)
{
	register char *ptr;

	ptr = (char *) malloc(strlen(str) + 1);
	(void) strcpy(ptr, str);
	return ptr;
}
#endif

/*-
   Dr. Park's algorithm published in the Oct. '88 ACM  "Random Number
   Generators: Good Ones Are Hard To Find" His version available at
   ftp://cs.wm.edu/pub/rngs.tar Present form by many authors.
 */

static int  Seed = 1;		/* This is required to be 32 bits long */

/*-
 *      Given an integer, this routine initializes the RNG seed.
 */
void
SetRNG(long int s)
{
	Seed = (int) s;
}

/*-
 *      Returns an integer between 0 and 2147483647, inclusive.
 */
long
LongRNG(void)
{
	if ((Seed = Seed % 44488 * 48271 - Seed / 44488 * 3399) < 0)
		Seed += 2147483647;
	return (long) (Seed - 1);
}


#include <ctype.h>
#define FROM_PROGRAM 1
#define FROM_FORMATTEDFILE    2
#define FROM_FILE    3
#define FROM_RESRC   4

char       *program, *messagesfile, *messagefile, *message, *mfont;

static char *def_words = "I'm out running around.";
static int  getwordsfrom;

static void
strcat_firstword(char *fword, char *words)
{
	while (*fword)
		fword++;
	while (*words && !(isspace(*words)))
		*fword++ = *words++;
	*fword = '\0';
}

int
isRibbon(void)
{
	return (getwordsfrom == FROM_RESRC);
}

char       *
getWords(int screen)
{
	FILE       *pp;
	static char *buf = NULL, progerr[BUFSIZ], progrun[BUFSIZ];
	register char *p;
	extern int  pclose(FILE *);
	int         i;

  if (!buf) {
    buf = (char *) calloc(MAXSCREENS * BUFSIZ, sizeof(char));
    if (!buf)
      return NULL;
  }
	p = &buf[screen * BUFSIZ];
	*p = '\0';
	if (strlen(message))
		getwordsfrom = FROM_RESRC;
	else if (strlen(messagefile)) {
		getwordsfrom = FROM_FILE;
	} else if (strlen(messagesfile)) {
		getwordsfrom = FROM_FORMATTEDFILE;
	} else {
		getwordsfrom = FROM_PROGRAM;
		(void) sprintf(progrun, "( %s ) 2>&1",
			       (!program) ? DEF_PROGRAM : program);
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
					strcat_firstword(progerr, (!program) ? DEF_PROGRAM : program);
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
	XFontStruct *mode_font;

	if (!(mode_font = XLoadQueryFont(display, (mfont) ? mfont : DEF_MFONT))) {
		if (mfont) {
			(void) fprintf(stderr, "can not find font: %s, using %s...\n",
				       mfont, DEF_MFONT);
			mode_font = XLoadQueryFont(display, DEF_MFONT);
		}
		if (!(mode_font)) {
			(void) fprintf(stderr, "can not find font: %s, using %s...\n",
				       DEF_MFONT, FALLBACK_FONTNAME);
			mode_font = XLoadQueryFont(display, FALLBACK_FONTNAME);
			if (!mode_font) {
				(void) fprintf(stderr, "can not even find %s!!!\n", FALLBACK_FONTNAME);
				return (None);	/* Do not want to exit when in a mode */
			}
		}
	}
	return mode_font;
}

#if HAVE_DIRENT_H
static char *
randomImage(char *im_file)
{
	extern char directory_r[DIRBUF];
	extern struct dirent **image_list;
	extern int  num_list;
	char *newimf;
	int         num;

	if (num_list > 0) {
		num = NRAND(num_list);
		if ((newimf = malloc(strlen(directory_r)
		       + strlen(image_list[num]->d_name) + 1)) != 0)
		{
		    (void) sprintf(newimf, "%s%s",
		             directory_r, image_list[num]->d_name);
		    free(im_file);
		    im_file = newimf;
		}
	}
	return im_file;
}

#endif

#if HAVE_DIRENT_H

extern Bool debug;

/* index_dir emulation of FORTRAN's index in C. Author: J. Jansen */
static int
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
void
upcase(char *s)
{
	int         i;
	char        c[1];

	for (i = 0; s[i] != '\0'; i++) {
		if (s[i] >= 'a' && s[i] <= 'z') {
			*c = s[i];
			s[i] = (char) (*c - 32);
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
	while ((ip_temp = index_dir(ln, "/"))) {
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
#endif
	if (debug)
		(void) printf("get_dir %s %s %s\n", fullpath, dir, filename);
}

/* Procedure to select the matching filenames  Author: J. Jansen */
int
sel_image(struct dirent *name)
{
	extern char filename_r[MAXNAMLEN];
	char       *name_tmp = name->d_name;
	char       *filename_tmp = filename_r;
	static int  numfrag = -1;
	static char *frags[64];
	int         ip, i;

	if (numfrag == -1) {
		++numfrag;
		while ((ip = index_dir(filename_tmp, "*"))) {
			frags[numfrag] = (char *) malloc(ip);
			(void) strcpy(frags[numfrag], "\0");
			(void) strncat(frags[numfrag], filename_tmp, ip - 1);
			++numfrag;
			filename_tmp = filename_tmp + ip;
		}
		frags[numfrag] = (char *) malloc(strlen(filename_tmp)+1);
		(void) strcpy(frags[numfrag], filename_tmp);
	}
	for (i = 0; i <= numfrag; ++i) {
		ip = index_dir(name_tmp, frags[i]);
		if (ip == 0)
			return (0);
		name_tmp = name_tmp + ip;
	}

	return (1);
}

/* scandir implementiation for VMS  Author: J. Jansen */
/* name changed to scan_dir to solve portablity problems */
#define _MEMBL_ 64
int
scan_dir(const char *directoryname, struct dirent ***namelist,
	 int         (*select) (struct dirent *),
	 int         (*compare) (const void *, const void *))
{
	DIR        *dirp;
	struct dirent *new_entry, **namelist_tmp;
	int         size_tmp, num_list_tmp;

	if ((dirp = opendir(directoryname)) == NULL)
		return (-1);
	size_tmp = _MEMBL_;
	namelist_tmp = (struct dirent **) malloc(size_tmp * sizeof (struct dirent *));

	if (namelist_tmp == NULL)
		return (-1);
	num_list_tmp = 0;
	while ((new_entry = readdir(dirp)) != NULL) {
#ifndef VMS
		if (!strcmp(new_entry->d_name, ".") && !strcmp(new_entry->d_name, ".."))
			continue;
#endif
		if (select != NULL && !(*select) (new_entry))
			continue;
		if (++num_list_tmp >= size_tmp) {
			size_tmp = size_tmp + _MEMBL_;
			namelist_tmp = (struct dirent **) realloc(
									 (void *) namelist_tmp, size_tmp * sizeof (struct dirent *));

			if (namelist_tmp == NULL)
				return (-1);
		}
		/* Core Dumps here for Solaris2 if files names in dir are > 14 characters */
		namelist_tmp[num_list_tmp - 1] =
			(struct dirent *) malloc(sizeof (struct dirent)
#ifdef SVR4
                     + strlen(new_entry->d_name)
#endif
             );

		(void) strcpy(namelist_tmp[num_list_tmp - 1]->d_name, new_entry->d_name);

		*namelist = namelist_tmp;
#ifdef SOLARIS2
		if (debug)
			(void) printf("file %d: %s\n",
				      num_list_tmp - 1, namelist_tmp[num_list_tmp - 1]->d_name);
#endif
	}
#ifdef SOLARIS2			/* Data seems to get corrupted for Solaris2 */
	if (debug) {
		(void) printf("directory: %s\n", directoryname);
		for (size_tmp = 0; size_tmp < num_list_tmp; size_tmp++)
			(void) printf("file %d: %s\n", size_tmp, namelist_tmp[size_tmp]->d_name);
	}
#endif

	(void) closedir(dirp);
	if (num_list_tmp && compare != NULL)
		(void) qsort((void *) namelist_tmp, num_list_tmp,
			     sizeof (struct dirent *), compare);

	*namelist = namelist_tmp;
	return (num_list_tmp);
}

#endif

extern int  XbmReadFileToImage(char *filename,
			       int *width, int *height, unsigned char **bits);

#if defined( USE_XPM ) || defined( USE_XPMINC )
#if USE_XPMINC
#include <xpm.h>
#else
#include <X11/xpm.h>		/* Normal spot */
#endif
#endif
#include "ras.h"

static XImage blogo =
{
	0, 0,			/* width, height */
	0, XYBitmap, 0,		/* xoffset, format, data */
	LSBFirst, 8,		/* byte-order, bitmap-unit */
	LSBFirst, 8, 1		/* bitmap-bit-order, bitmap-pad, depth */
};

void
getImage(ModeInfo * mi, XImage ** logo,
	 int width, int height, unsigned char *bits,
#if defined( USE_XPM ) || defined( USE_XPMINC )
	 char **name,
#endif
	 int *graphics_format, Colormap * ncm,
	 unsigned long *blackpix, unsigned long *whitepix)
{
	extern char *imagefile;
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

#if defined( USE_XPM ) || defined( USE_XPMINC )
	XpmAttributes attrib;

#endif

	if (!fixedColors(mi))
		*ncm = XCreateColormap(display, window, MI_VISUAL(mi), AllocNone);
	else
		*ncm = None;

#if defined( USE_XPM ) || defined( USE_XPMINC )
	attrib.visual = MI_VISUAL(mi);
	if (*ncm != None)
		attrib.colormap = *ncm;
	else
		attrib.colormap = MI_WIN_COLORMAP(mi);
	attrib.depth = MI_WIN_DEPTH(mi);
	attrib.valuemask = XpmVisual | XpmColormap | XpmDepth;
#endif

	*graphics_format = 0;

#if HAVE_DIRENT_H
	imagefile = randomImage(imagefile);
#endif

	if (strlen(imagefile))
		if (readable(imagefile)) {
			if (MI_NPIXELS(mi) > 2) {
				if (RasterSuccess == RasterFileToImage(mi, imagefile, logo)) {
					*graphics_format = IS_RASTERFILE;
					if (!fixedColors(mi))
						SetImageColors(display, *ncm);
					*blackpix = GetBlack();
					*whitepix = GetWhite();
				}
			}
#if defined( USE_XPM ) || defined( USE_XPMINC )
#ifndef USE_MONOXPM
			if (MI_NPIXELS(mi) > 2)
#endif
			{
				if (*graphics_format <= 0) {
					if (*ncm != None)
						reserveColors(mi, *ncm, blackpix, whitepix);
					if (XpmSuccess == XpmReadFileToImage(display, imagefile, logo,
						  (XImage **) NULL, &attrib))
						*graphics_format = IS_XPMFILE;
				}
			}
#endif
			if (*graphics_format <= 0)
				if (!blogo.data) {
					if (BitmapSuccess == XbmReadFileToImage(imagefile,
										&blogo.width, &blogo.height, (unsigned char **) &blogo.data)) {
						blogo.bytes_per_line = (blogo.width + 7) / 8;
						*graphics_format = IS_XBMFILE;
						*logo = &blogo;
					}
				} else {
					*graphics_format = IS_XBMDONE;
					*logo = &blogo;
				}
			if (*graphics_format <= 0)
				(void) fprintf(stderr,
					       "\"%s\" is in an unrecognized format\n", imagefile);
		} else
			(void) fprintf(stderr,
				  "could not read file \"%s\"\n", imagefile);
#if defined( USE_XPM ) || defined( USE_XPMINC )
	if (*graphics_format <= 0)
#ifndef USE_MONOXPM
		if (MI_NPIXELS(mi) > 2)
#endif
			if (XpmSuccess == XpmCreateImageFromData(display, name,
					    logo, (XImage **) NULL, &attrib))
				*graphics_format = IS_XPM;
#endif
	if (*graphics_format <= 0) {
		if (!blogo.data) {
			blogo.data = (char *) bits;
			blogo.width = width;
			blogo.height = height;
			blogo.bytes_per_line = (blogo.width + 7) / 8;
			*graphics_format = IS_XBM;
		} else
			*graphics_format = IS_XBMDONE;
		*logo = &blogo;
	}
	if (*ncm != None && *graphics_format != IS_RASTERFILE &&
	    *graphics_format != IS_XPMFILE && *graphics_format != IS_XPM) {
		XFreeColormap(display, *ncm);
		*ncm = None;
	}
}

void
destroyImage(XImage ** logo, int *graphics_format)
{
	switch (*graphics_format) {
		case IS_XBM:
			blogo.data = NULL;
			break;
		case IS_XBMFILE:
			if (blogo.data) {
				(void) free((void *) blogo.data);
				blogo.data = NULL;
			}
			break;
		case IS_XPM:
		case IS_XPMFILE:
		case IS_RASTERFILE:
			if (logo)
				XDestroyImage(*logo);
			break;
	}
	*graphics_format = -1;
	*logo = NULL;
}

void
getBitmap(XImage ** logo, int width, int height, unsigned char *bits,
	  int *graphics_format)
{
	extern char *imagefile;

#if HAVE_DIRENT_H
	imagefile = randomImage(imagefile);
#endif

	if (strlen(imagefile))
		if (readable(imagefile)) {
			if (!blogo.data) {
				if (BitmapSuccess == XbmReadFileToImage(imagefile,
									&blogo.width, &blogo.height, (unsigned char **) &blogo.data)) {
					blogo.bytes_per_line = (blogo.width + 7) / 8;
					*graphics_format = IS_XBMFILE;
					*logo = &blogo;
				}
			} else {
				*graphics_format = IS_XBMDONE;
				*logo = &blogo;
			}
			if (*graphics_format <= 0)
				(void) fprintf(stderr,
				       "\"%s\" not xbm format\n", imagefile);
		} else
			(void) fprintf(stderr,
				  "could not read file \"%s\"\n", imagefile);
	if (*graphics_format <= 0) {
		if (!blogo.data) {
			blogo.data = (char *) bits;
			blogo.width = width;
			blogo.height = height;
			blogo.bytes_per_line = (blogo.width + 7) / 8;
			*graphics_format = IS_XBM;
		} else
			*graphics_format = IS_XBMDONE;
		*logo = &blogo;
	}
}

void
destroyBitmap(XImage ** logo, int *graphics_format)
{
	switch (*graphics_format) {
		case IS_XBM:
			blogo.data = NULL;
			break;
		case IS_XBMFILE:
			if (blogo.data) {
				(void) free((void *) blogo.data);
				blogo.data = NULL;
			}
			break;
	}
	*graphics_format = -1;
	*logo = NULL;
}

static struct visual_class_name {
	int         visualclass;
	char       *name;
} VisualClassName[] = {

	{
		StaticGray, "StaticGray"
	},
	{
		GrayScale, "GrayScale"
	},
	{
		StaticColor, "StaticColor"
	},
	{
		PseudoColor, "PseudoColor"
	},
	{
		TrueColor, "TrueColor"
	},
	{
		DirectColor, "DirectColor"
	},
	{
		-1, NULL
	},
};

int
visualClassFromName(char *name)
{
	int         a;
	char       *s1, *s2;
	int         visualclass = -1;

	for (a = 0; VisualClassName[a].name; a++) {
		for (s1 = VisualClassName[a].name, s2 = name; *s1 && *s2; s1++, s2++)
			if ((isupper(*s1) ? tolower(*s1) : *s1) !=
			    (isupper(*s2) ? tolower(*s2) : *s2))
				break;

		if ((*s1 == '\0') || (*s2 == '\0')) {

			if (visualclass != -1) {
				(void) fprintf(stderr,
					       "%s does not uniquely describe a visual class (ignored)\n", name);
				return (-1);
			}
			visualclass = VisualClassName[a].visualclass;
		}
	}
	if (visualclass == -1)
		(void) fprintf(stderr, "%s is not a visual class (ignored)\n", name);
	return (visualclass);
}

char       *
nameOfVisualClass(int visualclass)
{
	int         a;

	for (a = 0; VisualClassName[a].name; a++)
		if (VisualClassName[a].visualclass == visualclass)
			return (VisualClassName[a].name);
	return ("[Unknown Visual Class]");
}

#if defined(__cplusplus) || defined(c_plusplus)
#define DEFAULT_VIS(v)		v.c_class = DefaultVisual(display, screen)->c_class;
#define WANTED_VIS(v)		v.c_class = VisualClassWanted;
#define CHECK_WANTED_VIS(v) 	   ((v)->c_class == VisualClassWanted)
#define NAME_VIS(v) nameOfVisualClass(v->c_class)
#else
#define DEFAULT_VIS(v)		v.class = DefaultVisual(display, screen)->class;
#define WANTED_VIS(v)		v.class = VisualClassWanted;
#define CHECK_WANTED_VIS(v) 	   ((v)->class == VisualClassWanted)
#define NAME_VIS(v) nameOfVisualClass(v->class)
#endif

void
showVisualInfo(XVisualInfo * Vis)
{
	(void) fprintf(stderr, "Visual info: ");
	(void) fprintf(stderr, "screen %d, ", Vis->screen);
	(void) fprintf(stderr, "class %s, ", NAME_VIS(Vis));
	(void) fprintf(stderr, "depth %d\n", Vis->depth);
}

/*- 
 * default_visual_info
 *
 * Gets a XVisualInfo structure that refers to a given visual or the default
 * visual.
 *
 * -      mi is the ModeInfo
 * -      visual is the visual to look up NULL => look up the default visual
 * -      default_info is set to point to the member of the returned list
 *          that corresponds to the default visual.
 *
 * Returns a list of XVisualInfo structures or NULL on failure. Free the list
 *   with XFree.
 */
static XVisualInfo *
defaultVisualInfo(ModeInfo * mi, Visual * visual, XVisualInfo ** default_info)
{
	Display    *display = MI_DISPLAY(mi);
	int         screen = MI_SCREEN(mi);
	XVisualInfo *info_list, vTemplate;
	int         i, n;
	extern int  VisualClassWanted;

	/* get a complete list of visuals */
	/*info_list = XGetVisualInfo(display, VisualNoMask, NULL, &n); */
	vTemplate.screen = screen;
	vTemplate.depth = MI_WIN_DEPTH(mi);
	if (VisualClassWanted == -1) {
		DEFAULT_VIS(vTemplate);
	} else {
		WANTED_VIS(vTemplate);
	}
	info_list = XGetVisualInfo(display,
			VisualScreenMask | VisualDepthMask | VisualClassMask,
				   &vTemplate, &n);
	if (VisualClassWanted != -1 && n == 0) {
		/* Wanted visual not found so use default */
		DEFAULT_VIS(vTemplate);
		info_list = XGetVisualInfo(display,
			VisualScreenMask | VisualDepthMask | VisualClassMask,
					   &vTemplate, &n);
	}
/*-
	WANTED_VIS(vTemplate);
	 if (VisualClassWanted == -1)
		info_list = XGetVisualInfo(display, VisualScreenMask | VisualDepthMask,
			&vTemplate, &n);
	 else
		 info_list = XGetVisUalInfo(display,
			 VisualScreenMask | VisualDepthMask | VisualClassMask,
			 &vTemplateWant, &n);
 */
	if ((info_list == NULL) || (n == 0)) {
		if (MI_WIN_IS_VERBOSE(mi))
			(void) fprintf(stderr, "Could not get any Visuals, numvisuals = %d.\n",
				       n);
		return (NULL);
	}
	/* do we need to get the default visual? */
	if (visual == NULL)
		visual = DefaultVisual(display, screen);

	*default_info = info_list;
	if (VisualClassWanted == -1) {
		/* search through the list for the default visual */
		for (i = 0; i < n; i++, (*default_info)++)
			if ((*default_info)->visual == visual &&
			    (*default_info)->screen == screen)
				break;
	} else {
		/* search through the list for the requested visual
		 * with the greatest color depth */
		for (i = 0; i < n; i++, (*default_info)++)
			if ((*default_info)->depth >= 24 &&
			    (*default_info)->screen == screen &&
			    CHECK_WANTED_VIS(*default_info))
				break;
		if (i == n) {
			*default_info = info_list;
			for (i = 0; i < n; i++, (*default_info)++)
				if ((*default_info)->depth == 16 &&
				    (*default_info)->screen == screen &&
				    CHECK_WANTED_VIS(*default_info))
					break;
		}
		if (i == n) {
			*default_info = info_list;
			for (i = 0; i < n; i++, (*default_info)++)
				if ((*default_info)->depth == 8 &&
				    (*default_info)->screen == screen &&
				    CHECK_WANTED_VIS(*default_info))
					break;
		}
		if (i == n) {
			*default_info = info_list;
			for (i = 0; i < n; i++, (*default_info)++)
				if ((*default_info)->depth == 4 &&
				    (*default_info)->screen == screen &&
				    CHECK_WANTED_VIS(*default_info))
					break;
		}
		if (i == n) {
			*default_info = info_list;
			for (i = 0; i < n; i++, (*default_info)++)
				if ((*default_info)->screen == screen &&
				    CHECK_WANTED_VIS(*default_info))
					break;
		}
	}
	if (MI_WIN_IS_VERBOSE(mi)) {
		(void) fprintf(stderr, "Visual info: ");
		(void) fprintf(stderr, "screen %d, ", (*default_info)->screen);
		(void) fprintf(stderr, "class %s, ", NAME_VIS((*default_info)));
		(void) fprintf(stderr, "depth %d\n", (*default_info)->depth);
	}
	/* return the list (and the default info) */
	return (info_list);
}


Bool
fixedColors(ModeInfo * mi)
{
#ifdef FORCEFIXEDCOLORS
	/* pretending a fixed colourmap */
	return FALSE;
#else
	XVisualInfo *list, *default_info;
  Bool writeable;

	/* get information about the default visual */
	list = defaultVisualInfo(mi, MI_VISUAL(mi), &default_info);
	writeable = (
#if defined(__cplusplus) || defined(c_plusplus)
		(default_info->c_class != StaticGray) &&
		(default_info->c_class != StaticColor) &&
		(default_info->c_class != TrueColor) &&
#else
		(default_info->class != StaticGray) &&
		(default_info->class != StaticColor) &&
		(default_info->class != TrueColor) &&
#endif
		(MI_NPIXELS(mi) > 2) &&
    !MI_WIN_IS_INROOT(mi) &&
    MI_WIN_IS_INSTALL(mi));
	XFree((char *) list);
  return !writeable;
#endif
}

/*- 
 * setupColormap
 *
 * Create a read/write colourmap to use
 *
 */

Bool
setupColormap(ModeInfo * mi, int *colors, Bool * truecolor,
   unsigned long *redmask, unsigned long *bluemask, unsigned long *greenmask)
{

	XVisualInfo *list, *default_info;

	/* get information about the default visual */
	list = defaultVisualInfo(mi, MI_VISUAL(mi), &default_info);

	/* how many colours are there altogether? */
	*colors = MI_NPIXELS(mi);
	if (*colors > default_info->colormap_size) {
		*colors = default_info->colormap_size;
	}
	if (*colors < 2)
		*colors = 2;

#if defined(__cplusplus) || defined(c_plusplus)
	*truecolor = (default_info->c_class == TrueColor);
#else
	*truecolor = (default_info->class == TrueColor);
#endif

	*redmask = default_info->red_mask;
	*greenmask = default_info->green_mask;
	*bluemask = default_info->blue_mask;

	XFree((char *) list);

	return !fixedColors(mi);
}

/*-
 * useableColors
 */
int
preserveColors(unsigned long fg, unsigned long bg,
	       unsigned long white, unsigned long black)
{
	/* how many colours should we preserve (out of white, black, fg, bg)? */
	if (((bg == black) || (bg == white)) && ((fg == black) || (fg == white)))
		return 2;
	else if ((bg == black) || (fg == black) ||
		 (bg == white) || (fg == white) || (bg == fg))
		return 3;
	else
		return 4;
}

#ifdef USE_MATHERR
/* Handle certain math exception errors */
int
matherr(register struct exception *x)
{
	extern Bool debug;

	switch (x->type) {
		case DOMAIN:
			/* Suppress "atan2: DOMAIN error" stderr message */
			if (!strcmp(x->name, "atan2")) {
				x->retval = 0.0;
				return ((debug) ? 0 : 1);	/* suppress message unless debugging */
			}
			if (!strcmp(x->name, "sqrt")) {
				x->retval = sqrt(-x->arg1);
				/* x->retval = 0.0; */
				return ((debug) ? 0 : 1);	/* suppress message unless debugging */
			}
			break;
#ifdef __hpux
			/* Fix how HP-UX does not like sin and cos of angles >= 360.  */
		case TLOSS:
			if (!strcmp(x->name, "cos")) {
				x->retval = cos(fmod(x->arg1, 360.0));
				return (1);	/* suppress message */
			}
			if (!strcmp(x->name, "sin")) {
				x->retval = sin(fmod(x->arg1, 360.0));
				return (1);	/* suppress message */
			}
			break;
		case PLOSS:
			return (1);
			break;
#endif
	}
	return (0);		/* all other exceptions, execute default procedure */
}
#endif

#ifdef USE_GL

#include <GL/gl.h>
#include <GL/glx.h>

static XVisualInfo **glVis = NULL;
static GLXContext *glXContext = NULL;

/* Don't ever call this one from a module.  It is only for xlock.c to use. */
void
FreeAllGL(Display * display)
{
	int         scr;

	if (glXContext) {
		for (scr = 0; scr < MAXSCREENS; scr++) {
			if (glXContext[scr])
				glXDestroyContext(display, glXContext[scr]);
			glXContext[scr] = NULL;
		}
    (void) free((void *) glXContext);
    glXContext = NULL;
		}
	if (glVis) {
		for (scr = 0; scr < MAXSCREENS; scr++) {
			if (glVis[scr])
				XFree(glVis[scr]);
			glVis[scr] = NULL;
		}
    (void) free((void *) glVis);
    glVis = NULL;
	}
}

/*-
 * NOTE WELL:  We _MUST_ destroy the glXContext between each mode
 * in random mode, otherwise OpenGL settings and paramaters from one
 * mode will affect the default initial state for the next mode.
 * BUT, we are going to keep the visual returned by glXChooseVisual,
 * because it will still be good (and because Mesa must keep track
 * of each one, even after XFree(), causing a small memory leak).
 */

XVisualInfo *
getGLVisual(Display * display, int screen, XVisualInfo * wantVis, int mono)
{

	if (!glVis) {
    glVis = (XVisualInfo **) calloc(MAXSCREENS, sizeof(XVisualInfo *));
		if (!glVis)
			return NULL;
	}
	/* If we already have it, use it! */
	if (glVis[screen])
		return (glVis[screen]);

	if (wantVis) {
		/* Use glXGetConfig() to see if wantVis has what we need already. */
		int         depthBits, doubleBuffer;

		/* I don't check up on RGBA mode... we might want MONO */
		/* glXGetConfig(display, wantVis, GLX_RGBA, &rgbaMode); */

		glXGetConfig(display, wantVis, GLX_DEPTH_SIZE, &depthBits);
		glXGetConfig(display, wantVis, GLX_DOUBLEBUFFER, &doubleBuffer);

		if ((depthBits > 0) && doubleBuffer) {
			return (glVis[screen] = wantVis);
		}
	}
	/* If wantVis is useless, try glXChooseVisal() */
	if (mono) {
		/* Monochrome display - use color index mode */
		int         attribList[] =
		{GLX_DOUBLEBUFFER, None};

		glVis[screen] = glXChooseVisual(display, screen, attribList);
	} else {
		int         attribList[] =
#if 0
		{GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1,
		 GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 1, None};
#else
    {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 1, None};
#endif
		glVis[screen] = glXChooseVisual(display, screen, attribList);
	}
	return (glVis[screen]);
}

/*-
 * The following function should be called on startup of any GL mode.
 * It returns a GLXContext for the calling mode to use with
 * glXMakeCurrent().  Do NOT destroy this glXContext, as it will
 * be taken care of by the next caller to init_GL.  You should
 * remember to delete your display lists, however.
 */
GLXContext
init_GL(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         screen = MI_SCREEN(mi);
	int         mono = MI_WIN_IS_MONO(mi) ? 1 : 0;
	int         n;
	XVisualInfo *wantVis, *gotVis, vTemplate;
	extern int  VisualClassWanted;
	GLboolean   rgbaMode;

	if (!glXContext) {
    glXContext = (GLXContext *) calloc(MAXSCREENS, sizeof(GLXContext));
		if (!glXContext)
      return NULL;
	}
	if (glXContext[screen]) {
		glXDestroyContext(display, glXContext[screen]);
		glXContext[screen] = NULL;
	}
	if (VisualClassWanted == -1) {

		wantVis = NULL;	/* NULL means use the default in getVisual() */

	} else {
		vTemplate.screen = screen;
		vTemplate.depth = MI_WIN_DEPTH(mi);
		WANTED_VIS(vTemplate)
			wantVis = XGetVisualInfo(display,
			VisualScreenMask | VisualDepthMask | VisualClassMask,
						 &vTemplate, &n);

		if (n == 0) {
			/* Wanted visual not found so use default */
			wantVis = NULL;
		}
	}

	/* if User asked for color, try that first, then try mono */
	/* if User asked for mono.  Might fail on 16/24 bit displays,
	   so fall back on color, but keep the mono "look & feel". */
	if (!(gotVis = getGLVisual(display, screen, wantVis, mono))) {
		if (!(gotVis = getGLVisual(display, screen, wantVis, !mono))) {
			(void) fprintf(stderr, "GL can not render with root visual\n");
			return (NULL);
		}
	}
	if (MI_WIN_IS_VERBOSE(mi))
		showVisualInfo(gotVis);

/*-
 * PURIFY 4.0.1 on SunOS4 and on Solaris 2 reports a 104 byte memory leak on
 * the next line each time that a GL mode is run in random mode when using
 * MesaGL 2.2.  This cumulative leak can cause xlock to eventually crash if
 * available memory is depleted.  This bug is fixed in MesaGL 2.3. */
	glXContext[screen] = glXCreateContext(display, gotVis, 0, GL_TRUE);
	if (wantVis && (wantVis != gotVis))
		XFree((char *) wantVis);

	if (!glXContext[screen]) {
		(void) fprintf(stderr, "GL could not create rendering context\n");
		return (NULL);
	}
/*-
 * PURIFY 4.0.1 on Solaris2 reports an unitialized memory read on the next
 * line. PURIFY 4.0.1 on SunOS4 does not report this error. */
	glXMakeCurrent(display, window, glXContext[screen]);

	glGetBooleanv(GL_RGBA_MODE, &rgbaMode);
	if (!rgbaMode) {
		glIndexi(WhitePixel(display, screen));
		glClearIndex(BlackPixel(display, screen));
	}
	return (glXContext[screen]);
}
#endif
