#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)utils.c	4.04 97/07/28 xlockmore";

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
 *            <Jeffrey_Doggett@caradon.com>.  Fix for ":not found" text
 *            that appears after about 40 minutes.
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

XPoint      hexagonUnit[6] =
{
	{0, 0},
	{1, 1},
	{0, 2},
	{-1, 1},
	{-1, -1},
	{0, -2}
};

XPoint      triangleUnit[2][3] =
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

unsigned char stipples[NUMSTIPPLES][STIPPLESIZE] =
{
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	/* white */
	{0x11, 0x22, 0x11, 0x22, 0x11, 0x22, 0x11, 0x22},	/* grey+white | stripe */
	{0x00, 0x66, 0x66, 0x00, 0x00, 0x66, 0x66, 0x00},	/* spots */
	{0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11},	/* lt. / stripe */
	{0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66},	/* | bars */
	{0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa},	/* 50% grey */
	{0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00},	/* - bars */
	{0xee, 0xdd, 0xbb, 0x77, 0xee, 0xdd, 0xbb, 0x77},	/* dark \ stripe */
	{0xff, 0x99, 0x99, 0xff, 0xff, 0x99, 0x99, 0xff},	/* spots */
	{0xaa, 0xff, 0xff, 0x55, 0xaa, 0xff, 0xff, 0x55},	/* black+grey - stripe */
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}	/* black */
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

	GETTIMEOFDAY(&now);
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
	if ((s >= 0 && S_ISREG(fileStat.st_mode)) || (s < 0 && type[0] == 'w')) {
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
	while (*words && !(isspace((int) *words)))
		*fword++ = *words++;
	*fword = '\0';
}

int
isRibbon(void)
{
	return (getwordsfrom == FROM_RESRC);
}

char       *
getWords(int screen, int screens)
{
	FILE       *pp;
	static char *buf = NULL, progerr[BUFSIZ], progrun[BUFSIZ];
	register char *p;
	extern int  pclose(FILE *);
	int         i;

	if (!buf) {
		buf = (char *) calloc(screens * BUFSIZ, sizeof (char));

		if (!buf)
			return NULL;
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
static void
randomFileFromList(char *directory,
		   struct dirent **filelist, int numfiles, char *file_local)
{
	int         num;

	if (numfiles > 0) {
		num = NRAND(numfiles);
		if (strlen(directory) + strlen(filelist[num]->d_name) + 1 < 256) {
			(void) sprintf(file_local, "%s%s",
				       directory, filelist[num]->d_name);
		}
	}
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

/* Procedure to select the matching filenames  Author: J. Jansen */
int
sel_image(struct dirent *name)
{
	extern char filename_r[MAXNAMLEN];
	char       *name_tmp = name->d_name;
	char       *filename_tmp = filename_r;
	int         numfrag = -1;
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
		frags[numfrag] = (char *) malloc(strlen(filename_tmp) + 1);
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
		if (!strcmp(new_entry->d_name, ".") || !strcmp(new_entry->d_name, ".."))
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
		namelist_tmp[num_list_tmp - 1] =
			(struct dirent *) malloc(sizeof (struct dirent)
#ifdef SVR4
					      + strlen    (new_entry->d_name)
#endif
		);

		(void) strcpy(namelist_tmp[num_list_tmp - 1]->d_name, new_entry->d_name);

		*namelist = namelist_tmp;
	}

	(void) closedir(dirp);
	if (num_list_tmp && compare != NULL)
		(void) qsort((void *) namelist_tmp, num_list_tmp,
			     sizeof (struct dirent *), compare);

	*namelist = namelist_tmp;
	return (num_list_tmp);
}

#endif

#if HAVE_DIRENT_H
static void
getRandomImageFile(char *randomfile, char *randomfile_local)
{
	extern char directory_r[DIRBUF];
	struct dirent ***images_list;
	extern struct dirent **image_list;
	extern int  num_list;
	extern char filename_r[MAXNAMLEN];
	extern void get_dir(char *fullpath, char *dir, char *filename);
	extern int  sel_image(struct dirent *name);
	extern int  scan_dir(const char *directoryname, struct dirent ***namelist,
			     int         (*select) (struct dirent *),
			int         (*compare) (const void *, const void *));

	get_dir(randomfile, directory_r, filename_r);
	images_list = (struct dirent ***) malloc(sizeof (struct dirent **));

	num_list = scan_dir(directory_r, images_list, sel_image, NULL);
	image_list = *images_list;
	randomFileFromList(directory_r, image_list, num_list, randomfile_local);
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
	 int default_width, int default_height, unsigned char *default_bits,
#if defined( USE_XPM ) || defined( USE_XPMINC )
	 int default_xpm, char **name,
#endif
	 int *graphics_format, Colormap * ncm,
	 unsigned long *black)
{
	Display    *display = MI_DISPLAY(mi);
	extern char *imagefile;
	static char *imagefile_local = NULL;

#if defined( USE_XPM ) || defined( USE_XPMINC )
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
#if defined( USE_XPM ) || defined( USE_XPMINC )
	attrib.visual = MI_VISUAL(mi);
	if (*ncm == None) {
		attrib.colormap = MI_COLORMAP(mi);
	} else {
		attrib.colormap = *ncm;
	}
	attrib.depth = MI_DEPTH(mi);
	attrib.valuemask = XpmVisual | XpmColormap | XpmDepth;
#endif

	*graphics_format = 0;

	if (imagefile_local) {
		(void) free((void *) imagefile_local);
		imagefile_local = NULL;
	}
	if (imagefile && strlen(imagefile)) {
		imagefile_local = malloc(256);
		(void) strncpy(imagefile_local, imagefile, sizeof (imagefile_local));
#if HAVE_DIRENT_H
		getRandomImageFile(imagefile, imagefile_local);
#endif
	}
	if (imagefile_local && strlen(imagefile_local)) {
		if (readable(imagefile_local)) {
			if (MI_NPIXELS(mi) > 2) {
				if (RasterSuccess == RasterFileToImage(mi, imagefile_local, logo)) {
					*graphics_format = IS_RASTERFILE;
					if (!fixedColors(mi))
						SetImageColors(display, *ncm);
					*black = GetColor(mi, MI_BLACK_PIXEL(mi));
					(void) GetColor(mi, MI_WHITE_PIXEL(mi));
					(void) GetColor(mi, MI_BG_PIXEL(mi));
					(void) GetColor(mi, MI_FG_PIXEL(mi));
				}
			}
		} else
			(void) fprintf(stderr,
			    "could not read file \"%s\"\n", imagefile_local);
#if defined( USE_XPM ) || defined( USE_XPMINC )
#ifndef USE_MONOXPM
		if (MI_NPIXELS(mi) > 2)
#endif
		{
			if (*graphics_format <= 0) {
				if (*ncm != None)
					reserveColors(mi, *ncm, black);
				if (XpmSuccess == XpmReadFileToImage(display, imagefile_local, logo,
						  (XImage **) NULL, &attrib))
					*graphics_format = IS_XPMFILE;
			}
		}
#endif
		if (*graphics_format <= 0) {
			if (!blogo.data) {
				if (BitmapSuccess == XbmReadFileToImage(imagefile_local,
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
		if (*graphics_format <= 0 && MI_WIN_IS_VERBOSE(mi))
			(void) fprintf(stderr,
				       "\"%s\" is in an unrecognized format or not compatible with screen\n",
				       imagefile_local);
	}
#if defined( USE_XPM ) || defined( USE_XPMINC )
	if (*graphics_format <= 0 && default_xpm)
#ifndef USE_MONOXPM
		if (MI_NPIXELS(mi) > 2)
#endif
			if (XpmSuccess == XpmCreateImageFromData(display, name,
					    logo, (XImage **) NULL, &attrib))
				*graphics_format = IS_XPM;
#endif
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
				(void) XDestroyImage(*logo);
			break;
	}
	*graphics_format = -1;
	*logo = NULL;
}

void
getPixmap(Display * display, Drawable drawable,
	  int default_width, int default_height, unsigned char *default_bits,
	  int *width, int *height, Pixmap * pixmap,
	  int *graphics_format)
{
	extern char *imagefile;
	static char *imagefile_local = NULL;
	int         x_hot, y_hot;	/* dummy */

	if (imagefile_local) {
		(void) free((void *) imagefile_local);
		imagefile_local = NULL;
	}
	if (imagefile && strlen(imagefile)) {
		imagefile_local = malloc(256);
		(void) strncpy(imagefile_local, imagefile, sizeof (imagefile_local));
#if HAVE_DIRENT_H
		getRandomImageFile(imagefile, imagefile_local);
#endif
	}
	if (imagefile_local && strlen(imagefile_local)) {
		if (readable(imagefile_local)) {
			if (BitmapSuccess == XReadBitmapFile(display, drawable,
							     imagefile_local,
			     (unsigned int *) width, (unsigned int *) height,
						   pixmap, &x_hot, &y_hot)) {
				*graphics_format = IS_XBMFILE;
			}
			if (*graphics_format <= 0)
				(void) fprintf(stderr,
				 "\"%s\" not xbm format\n", imagefile_local);
		} else
			(void) fprintf(stderr,
			    "could not read file \"%s\"\n", imagefile_local);
	}
	if (*graphics_format <= 0) {
		*width = default_width;
		*height = default_height;
		*graphics_format = IS_XBM;
		*pixmap = XCreateBitmapFromData(display, drawable,
				     (char *) default_bits, *width, *height);

	}
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
