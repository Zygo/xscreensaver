/****************************************************************************
 *                                                                           *
 * xsublim -- Submit.  Conform.  Obey.                                       *
 *                                                                           *
 * Copyright (c) 1999 - 2000 Greg Knauss (greg@eod.com)                      *
 *                                                                           *
 * Thanks to Jamie Zawinski, whose suggestions and advice made what was a    *
 * boring little program into a less boring (and a _lot_ less little)        *
 * program.                                                                  *
 *                                                                           *
 * Permission to use, copy, modify, distribute, and sell this software and   *
 * its documentation for any purpose is hereby granted without fee, provided *
 * that the above copyright notice appear in all copies and that both that   *
 * copyright notice and this permission notice appear in supporting          *
 * documentation.  No representations are made about the suitability of this *
 * software for any purpose.  It is provided "as is" without express or      *
 * implied warranty.                                                         *
 *                                                                           *
 * Stare into the subliminal for as long as you can...                       *
 *                                                                           *
 *****************************************************************************/


/* Warnings *******************************************************************

	Please don't end this process with a SIGKILL.  If it's got the X server
	grabbed when you do, you'll never get it back and it won't be my fault.
*/


/* Arguments ******************************************************************

	-font font           Font to use
	-file filename       New-line delimited phrase file
	-program executable  New-line delimited phrase-producing executable
	-delayShow ms        Microsecs for display of each word
	-delayWord ms        Microsecs for blank between words
	-delayPhraseMin ms   Microsecs for min blank between phrases
	-delayPhraseMax ms   Microsecs for max blank between phrases
	-random              Show phrases in random order (Default)
	-no-random           Show phrases in listed order
	-screensaver         Wait for an active screensaver (Default)
	-no-screensaver      Draw over active windows       
	-outline             Draw a contrasting outline around words (Default)
	-no-outline          Draw words without an outline
	-center              Draw words in the center of the screen (Default)
	-no-center           Draw words randomly around the screen
*/


/* Changelog ******************************************************************

	1.1.1  20000407  Added -program
	1.1.0  19991221  Added -file
	1.0.1  19990716  Assume that XGetImage()/XDestroyImage() don't leak,
	                  which they apparently don't.  I have no idea how I
	                  convinced myself that they did.  Huh.  (greg@eod.com)
	1.0.0  19990716  Initial release
*/


/* Defines *******************************************************************/
#define XSUBLIM_NAME               "XSublim"
#define XSUBLIM_TEXT_COUNT         1000
#define XSUBLIM_TEXT_LENGTH        128
#define XSUBLIM_TEXT_OUTLINE       1
#define XSUBLIM_PROGRAM_SIZE       1024*10
#define XSUBLIM_ARG_DELAYSHOW      "delayShow"
#define XSUBLIM_ARG_DELAYWORD      "delayWord"
#define XSUBLIM_ARG_DELAYPHRASEMIN "delayPhraseMin"
#define XSUBLIM_ARG_DELAYPHRASEMAX "delayPhraseMax"
#define XSUBLIM_ARG_RANDOM         "random"
#define XSUBLIM_ARG_FILE           "file"
#define XSUBLIM_ARG_PROGRAM        "program"
#define XSUBLIM_ARG_SCREENSAVER    "screensaver"
#define XSUBLIM_ARG_OUTLINE        "outline"
#define XSUBLIM_ARG_CENTER         "center"
#define XSUBLIM_ARG_FONT           "font"
#define XSUBLIM_ARG_PHRASES        "phrases"
#ifndef TRUE
#define FALSE 0
#define TRUE  1
#endif


/* Includes ******************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xproto.h>
#if defined(__sgi)
#include <X11/SGIScheme.h>
#endif

#include "usleep.h"
#include "yarandom.h"
#include "resources.h"
#include "xft.h"
#include "font-retry.h"
#include "vroot.h"


/* Globals *******************************************************************/
char*        progname;
char*        progclass = XSUBLIM_NAME;

# ifdef __GNUC__
  __extension__   /* don't warn about "string length is greater than the
                     length ISO C89 compilers are required to support"
                     in the following string constant... */
# endif

static char*        defaults[] =
{
	".background:                     #000000",
	".foreground:                     #FFFFFF",
	"*" XSUBLIM_ARG_PHRASES ":"
	 "Submit.\\n"
	 "Conform.\\n"
	 "Obey.\\n"
	 "OBEY. OBEY. OBEY.\\n"
	 "Consume.\\n"
	 "Be silent.\\n"
	 "Fear.\\n"
	 "Waste.\\n"
	 "Money.\\n"
	 "Watch TV.\\n"
	 "Hate yourself.\\n"
	 "Buy needlessly.\\n"
	 "Despair quietly.\\n"
	 "God hates you.\\n"
	 "You are being watched.\\n"
	 "You will be punished.\\n"
	 "You serve no purpose.\\n"
	 "Your contributions are ignored.\\n"
	 "They are laughing at you.\\n"
	 "They lied to you.\\n"
	 "They read your mail.\\n"
	 "They know.\\n"
	 "Surrender.\\n"
	 "You will fail.\\n"
	 "Never question.\\n"
	 "You are a prisoner.\\n"
	 "You are helpless.\\n"
	 "You are diseased.\\n"
	 "Fear the unknown.\\n"
	 "Happiness follows obedience.\\n"
	 "Ignorance is strength.\\n"
	 "War is peace.\\n"
	 "Freedom is slavery.\\n"
	 "Abandon all hope.\\n"
	 "You will be assimilated.\\n"
	 "Resistance is futile.\\n"
	 "Resistance is useless.\\n"
	 "Life is pain.\\n"
	 "No escape.\\n"
	 "What's that smell?\\n"
	 "All praise the company.\\n"
	 "Fnord.\\n",
	"*" XSUBLIM_ARG_FONT ":           -*-utopia-*-r-*-*-*-600-*-*-p-*-*-*",
	"*" XSUBLIM_ARG_DELAYSHOW ":      40000",
	"*" XSUBLIM_ARG_DELAYWORD ":      100000",
	"*" XSUBLIM_ARG_DELAYPHRASEMIN ": 5000000",
	"*" XSUBLIM_ARG_DELAYPHRASEMAX ": 20000000",
	"*" XSUBLIM_ARG_RANDOM ":         true",
	"*" XSUBLIM_ARG_SCREENSAVER ":    true",
	"*" XSUBLIM_ARG_OUTLINE":         true",
	"*" XSUBLIM_ARG_CENTER":          true",
	NULL
};

static XrmOptionDescRec options[] =
{
	{"-" XSUBLIM_ARG_FONT,          "." XSUBLIM_ARG_FONT,
	 XrmoptionSepArg,0},
	{"-" XSUBLIM_ARG_DELAYSHOW,     "." XSUBLIM_ARG_DELAYSHOW,
	 XrmoptionSepArg,0},
	{"-" XSUBLIM_ARG_DELAYWORD,     "." XSUBLIM_ARG_DELAYWORD,
	 XrmoptionSepArg,0},
	{"-" XSUBLIM_ARG_DELAYPHRASEMIN,"." XSUBLIM_ARG_DELAYPHRASEMIN,
	 XrmoptionSepArg,0},
	{"-" XSUBLIM_ARG_DELAYPHRASEMAX,"." XSUBLIM_ARG_DELAYPHRASEMAX,
	 XrmoptionSepArg,0},
	{"-" XSUBLIM_ARG_RANDOM,        "." XSUBLIM_ARG_RANDOM,
	 XrmoptionNoArg,"true"},
	{"-no-" XSUBLIM_ARG_RANDOM,     "." XSUBLIM_ARG_RANDOM,
	 XrmoptionNoArg,"false"},
	{"-" XSUBLIM_ARG_FILE,          "." XSUBLIM_ARG_FILE,
	 XrmoptionSepArg,0 },
#if !defined(VMS)
	{"-" XSUBLIM_ARG_PROGRAM,       "." XSUBLIM_ARG_PROGRAM,
	 XrmoptionSepArg,0 },
#endif
	{"-" XSUBLIM_ARG_SCREENSAVER,   "." XSUBLIM_ARG_SCREENSAVER,
	 XrmoptionNoArg,"true"},
	{"-no-" XSUBLIM_ARG_SCREENSAVER,"." XSUBLIM_ARG_SCREENSAVER,
	 XrmoptionNoArg,"false"},
	{"-" XSUBLIM_ARG_OUTLINE,       "." XSUBLIM_ARG_OUTLINE,
	 XrmoptionNoArg,"true"},
	{"-no-" XSUBLIM_ARG_OUTLINE,    "." XSUBLIM_ARG_OUTLINE,
	 XrmoptionNoArg,"false"},
	{"-" XSUBLIM_ARG_CENTER,        "." XSUBLIM_ARG_CENTER,
	 XrmoptionNoArg,"true"},
	{"-no-" XSUBLIM_ARG_CENTER,     "." XSUBLIM_ARG_CENTER,
	 XrmoptionNoArg,"false"},
	{NULL,                          NULL,
	 0,              0 }
};
static int Xsublim_Sig_Last;


/* Functions *****************************************************************/

/* Defer signals to protect the server grab ================================ */
static void xsublim_Sig_Catch(int sig_Number)
{
	/* BSD needs this reset each time, and it shouldn't hurt anything
	   else */
	signal(sig_Number,xsublim_Sig_Catch);
	Xsublim_Sig_Last = sig_Number;
}

/* This was all basically swiped from driver/remote.c and util/vroot.h */
static Window xsublim_Ss_GetWindow(Display* ss_Display)
{
  Screen *s = DefaultScreenOfDisplay (ss_Display);
  Window root = XRootWindowOfScreen (s);
  Window vroot = VirtualRootWindowOfScreen (s);
  if (root == vroot)
    return 0;
  else
    return vroot;
}

/* Main ==================================================================== */
static XErrorHandler Xsublim_Sh_Handler = NULL;
static int           Xsublim_Sh_Status = 0;

static int xsublim_Sh_Handler(Display* handle_Display,
            XErrorEvent* handle_Error)
{
	if (handle_Error->error_code == BadMatch)
	{
		Xsublim_Sh_Status = BadMatch;
		return 0;
	}
	if (Xsublim_Sh_Handler == NULL)
	{
		fprintf(stderr,"%s: ",progname);
		abort();
	}
	return (*Xsublim_Sh_Handler)(handle_Display,handle_Error);
}


int main(int argc,char* argv[])
{
	XtAppContext app;
	int               sig_Number;
	int               sig_Signal[] =
	{
		SIGHUP,
		SIGINT,
		SIGQUIT,
		SIGILL,
		SIGTRAP,
#if defined(SIGIOT)
		SIGIOT,
#endif
		SIGABRT,
#if defined(SIGEMT)
		SIGEMT,
#endif
		SIGFPE,
		SIGBUS,
		SIGSEGV,
#if defined(SIGSYS)
		SIGSYS,
#endif
		SIGTERM,
#if defined(SIGXCPU)
		SIGXCPU,
#endif
#if defined(SIGXFSZ)
		SIGXFSZ,
#endif
#if defined(SIGDANGER)
		SIGDANGER,
#endif
		-1
	};
	Widget            app_App;
	Display*          dpy;
	Window            win_Root;
	XWindowAttributes attr_Win;
	XGCValues         gc_ValFore;
	XGCValues         gc_ValBack;
	GC                gc_GcFore;
	GC                gc_GcBack;
	XFontStruct*      font_Font;
	char*             font_List[] =
	{
		"-*-character-*-r-*-*-*-600-*-*-p-*-*-*",
		"-*-helvetica-*-r-*-*-*-600-*-*-p-*-*-*",
		"-*-lucida-*-r-*-*-*-600-*-*-p-*-*-*",
		"-*-*-*-r-*-sans-*-600-*-*-p-*-*-*",
		"-*-*-*-r-*-*-*-600-*-*-m-*-*-*",
		"-*-helvetica-*-r-*-*-*-240-*-*-p-*-*-*",
		"-*-lucida-*-r-*-*-*-240-*-*-p-*-*-*",
		"-*-*-*-r-*-sans-*-240-*-*-p-*-*-*",
		"-*-*-*-r-*-*-*-240-*-*-m-*-*-*",
		"fixed",
		NULL
	};
	int               font_Index;
	int               text_Length;
	int               text_X;
	int               text_Y;
	int               text_Width;
	int               text_Height;
	char*             text_List[XSUBLIM_TEXT_COUNT];
	int               text_Used[XSUBLIM_TEXT_COUNT];
	char              text_Text[XSUBLIM_TEXT_LENGTH+1];
	char*             text_Phrase;
	char*             text_Word;
	int               text_Index;
	int               text_Item;
	int               text_Count;
	struct
	{
		int outline_X;
		int outline_Y;
	}                 text_Outline[] =
	{
		{ -1,-1 },
		{  1,-1 },
		{ -1, 1 },
		{  1, 1 },
		{  0, 0 }
	};
	int               text_OutlineIndex;
	XImage*           image_Image = NULL;
	int               image_X = 0;
	int               image_Y = 0;
	int               image_Width = 0;
	int               image_Height = 0;
	int               arg_Count;
	int               arg_FlagCenter;
	int               arg_FlagOutline;
	int               arg_FlagScreensaver;
	int               arg_FlagRandom;
	int               arg_DelayShow;
	int               arg_DelayWord;
	int               arg_DelayPhraseMin;
	int               arg_DelayPhraseMax;
	char*             arg_Text;
	char*             arg_Source;

	/* Set-up ---------------------------------------------------------- */

	/* Catch signals */
	Xsublim_Sig_Last = -1;
	for (sig_Number = 0;sig_Signal[sig_Number] != -1;sig_Number++)
	{
		signal(sig_Number,xsublim_Sig_Catch);
	}

	/* Randomize -- only need to do this here because this program
           doesn't use the `screenhack.h' or `lockmore.h' APIs. */
# undef ya_rand_init
        ya_rand_init (0);

	/* Handle all the X nonsense */
#if defined(__sgi)
	SgiUseSchemes("none");
#endif
	for (arg_Count = 0;options[arg_Count].option != NULL;arg_Count++)
	{
		;
	}
	app_App = XtAppInitialize(&app,progclass,options,arg_Count,&argc,argv,
	 defaults,0,0);

        /* jwz */
        if (argc > 1)
          {
            int x = 18;
            int end = 78;
            int i;
            int count = (sizeof(options)/sizeof(*options))-1;
            fprintf(stderr, "Unrecognised option: %s\n", argv[1]);
            fprintf (stderr, "Options include: ");
            for (i = 0; i < count; i++)
              {
                char *sw = options [i].option;
                Bool argp = (options [i].argKind == XrmoptionSepArg);
                int size = strlen (sw) + (argp ? 6 : 0) + 2;
                if (x + size >= end)
                  {
                    fprintf (stderr, "\n\t\t ");
                    x = 18;
                  }
                x += size;
                fprintf (stderr, "%s", sw);
                if (argp) fprintf (stderr, " <arg>");
                if (i != count-1) fprintf (stderr, ", ");
              }
            fprintf (stderr, ".\n");
            exit (-1);
          }

	dpy = XtDisplay(app_App);
	XtGetApplicationNameAndClass(dpy,&progname,&progclass);
	win_Root = RootWindowOfScreen(XtScreen(app_App));
	XtDestroyWidget(app_App);

	/* Get the arguments */
	arg_FlagCenter = get_boolean_resource(dpy, XSUBLIM_ARG_CENTER,"Boolean");
	arg_FlagOutline = get_boolean_resource(dpy, XSUBLIM_ARG_OUTLINE,"Boolean");
	arg_FlagScreensaver = get_boolean_resource(dpy, XSUBLIM_ARG_SCREENSAVER,
	 "Boolean");
	arg_FlagRandom = get_boolean_resource(dpy, XSUBLIM_ARG_RANDOM,"Boolean");
	arg_DelayShow = get_integer_resource(dpy, XSUBLIM_ARG_DELAYSHOW,"Integer");
	arg_DelayWord = get_integer_resource(dpy, XSUBLIM_ARG_DELAYWORD,"Integer");
	arg_DelayPhraseMin = get_integer_resource(dpy, XSUBLIM_ARG_DELAYPHRASEMIN,
	 "Integer");
	arg_DelayPhraseMax = get_integer_resource(dpy, XSUBLIM_ARG_DELAYPHRASEMAX,
	 "Integer");
	if (arg_DelayPhraseMax < arg_DelayPhraseMin)
	{
		arg_DelayPhraseMax = arg_DelayPhraseMin;
	}

	/* Get the phrases */
	text_Index = 0;
	text_Item = 0;
	text_Count = 0;
	memset(text_Used,0,sizeof(text_Used));
	arg_Source = get_string_resource(dpy, XSUBLIM_ARG_FILE,"Filename");
	if (arg_Source != NULL)
	{
		FILE*       file_Fs;
		struct stat file_Stat;

		file_Fs = fopen(arg_Source,"rb");
		if (file_Fs == NULL)
		{
			fprintf(stderr,"%s: Could not open '%s'\n",progname,
			 arg_Source);
			exit(-1);
		}
		if (fstat(fileno(file_Fs),&file_Stat) != 0)
		{
			fprintf(stderr,"%s: Could not stat '%s'\n",progname,
			 arg_Source);
			exit(-1);
		}
		arg_Text = calloc(1,file_Stat.st_size+1);
		if (arg_Text != NULL)
		{
			if (fread(arg_Text,file_Stat.st_size,1,file_Fs) != 1)
			{
				fprintf(stderr,"%s: Could not read '%s'\n",
				 progname,arg_Source);
				exit(-1);
			}
		}
		fclose(file_Fs);
	}
	else
	{
		arg_Source = get_string_resource(dpy, XSUBLIM_ARG_PROGRAM,
		 "Executable");
		if (arg_Source != NULL)
		{
			char* exe_Command = calloc(1,strlen(arg_Source)+10);
			FILE* exe_Fs;

			if (exe_Command == NULL)
			{
				fprintf(stderr,
				 "%s: Could not allocate space for '%s'\n",
				 progname,arg_Source);
				exit(-1);
			}
			sprintf(exe_Command,"( %s ) 2>&1",arg_Source);

			exe_Fs = popen(exe_Command,"r");
			if (exe_Fs == NULL)
			{
				fprintf(stderr,"%s: Could not run '%s'\n",
				 progname,arg_Source);
				exit(-1);
			}
			arg_Text = calloc(1,XSUBLIM_PROGRAM_SIZE);
			if (arg_Text != NULL)
			{
				if (fread(arg_Text,1,XSUBLIM_PROGRAM_SIZE,
				 exe_Fs) <= 0)
				{
					fprintf(stderr,
					 "%s: Could not read output of '%s'\n",
					 progname,arg_Source);
					exit(-1);
				}
				if (
				 strstr(arg_Text,": not found") ||
				 strstr(arg_Text,": Not found") ||
				 strstr(arg_Text,": command not found") ||
				 strstr(arg_Text,": Command not found"))
				{
					fprintf(stderr,
					 "%s: Could not find '%s'\n",
					 progname,arg_Source);
					exit(-1);
				}
			}
			fclose(exe_Fs);
		}
		else
		{
			arg_Text =
			 get_string_resource(dpy, XSUBLIM_ARG_PHRASES,"Phrases");
			if (arg_Text != NULL)
			{
				arg_Text = strdup(arg_Text);
			}
		}
	}
	if (arg_Text != NULL)
	{
		while (((text_Phrase = strtok(arg_Text,"\n")) != NULL) &&
		 (text_Count < XSUBLIM_TEXT_COUNT))
		{
			arg_Text = NULL;
			text_List[text_Count] = text_Phrase;
			text_Count++;
		}
		text_List[text_Count] = NULL;
	}
	if (text_Count == 0)
	{
		fprintf(stderr,"%s: No text to display\n",progname);
		exit(-1);
	}

	/* Load the font */
	font_Font = load_font_retry(dpy,
	 get_string_resource(dpy, XSUBLIM_ARG_FONT,"Font"));
	font_Index = 0;
	while ((font_Font == NULL) && (font_List[font_Index] != NULL))
	{
		font_Font = load_font_retry(dpy,font_List[font_Index]);
		font_Index++;
	}
	if (font_Font == NULL)
	{
		fprintf(stderr,"%s: Couldn't load a font\n",progname);
		exit(-1);
	}

	/* Create the GCs */
	XGetWindowAttributes(dpy,win_Root,&attr_Win);
	gc_ValFore.font = font_Font->fid;
	gc_ValFore.foreground = get_pixel_resource(dpy,
                                                   attr_Win.colormap,
                                                   "foreground","Foreground");
	gc_ValFore.background = get_pixel_resource(dpy,
                                                   attr_Win.colormap,
                                                   "background","Background");
	gc_ValFore.subwindow_mode = IncludeInferiors;
	gc_GcFore = XCreateGC(dpy,win_Root,
	 (GCFont|GCForeground|GCBackground|GCSubwindowMode),&gc_ValFore);
	gc_ValBack.font = font_Font->fid;
	gc_ValBack.foreground = get_pixel_resource(dpy,
                                                   attr_Win.colormap,
                                                   "background","Background");
	gc_ValBack.background = get_pixel_resource(dpy,
                                                   attr_Win.colormap,
                                                   "foreground","Foreground");
	gc_ValBack.subwindow_mode = IncludeInferiors;
	gc_GcBack = XCreateGC(dpy,win_Root,
	 (GCFont|GCForeground|GCBackground|GCSubwindowMode),&gc_ValBack);

	/* Loop ------------------------------------------------------------ */
	while (Xsublim_Sig_Last == -1)
	{
		/* Once-per-phrase stuff ----------------------------------- */

		/* If we're waiting for a screensaver... */
		if (arg_FlagScreensaver != FALSE)
		{
			/* Find the screensaver's window */
			win_Root = xsublim_Ss_GetWindow(dpy);
			if (win_Root == 0)
			{
				usleep(30000000);
				continue;
			}
		}

		/* Pick the next phrase */
		if (arg_FlagRandom != FALSE)
		{
			text_Item = random()%text_Count;
			text_Index = 0;
		}
		while (text_Used[text_Item] != FALSE)
		{
			text_Index++;
			text_Item++;
			if (text_Index == text_Count)
			{
				text_Index = 0;
				memset(text_Used,0,sizeof(text_Used));
			}
			if (text_List[text_Item] == NULL)
			{
				text_Item = 0;
			}
		}
		text_Used[text_Item] = TRUE;
		strncpy(text_Text,text_List[text_Item],
		 XSUBLIM_TEXT_LENGTH);
		text_Phrase = text_Text;

		/* Run through the phrase */
		while (((text_Word = strtok(text_Phrase," \t")) != NULL) &&
		 (Xsublim_Sig_Last == -1))
		{
			text_Phrase = NULL;

			/* Once-per-word stuff ----------------------------- */

			/* Find the text's position */
			XGetWindowAttributes(dpy,win_Root,&attr_Win);
			text_Length = strlen(text_Word);
			text_Width = XTextWidth(font_Font,text_Word,
			 text_Length)+XSUBLIM_TEXT_OUTLINE*2;
			text_Height = font_Font->ascent+font_Font->descent+1+
			 XSUBLIM_TEXT_OUTLINE*2;
			if (arg_FlagCenter == FALSE)
			{
				text_X = random()%(attr_Win.width-text_Width);
				text_Y = random()%(attr_Win.height-
				 text_Height);
			}
			else
			{
				text_X = (attr_Win.width/2)-(text_Width/2);
				text_Y = (attr_Win.height/2)-(text_Height/2);
			}

			/* Find the image's position (and pad it out slightly,
			   otherwise bits of letter get left behind -- are
			   there boundry issues I don't know about?) */
			image_X = text_X-16;
			image_Y = text_Y;
			image_Width = text_Width+32;
			image_Height = text_Height;
			if (image_X < 0)
			{
				image_X = 0;
			}
			if (image_Y < 0)
			{
				image_Y = 0;
			}
			if (image_X+image_Width > attr_Win.width)
			{
				image_Width = attr_Win.width-image_X;
			}
			if (image_Y+image_Height > attr_Win.height)
			{
				image_Height = attr_Win.height-image_Y;
			}

			/* Influence people for our own ends --------------- */

			/* Grab the server -- we can't let anybody draw over
			   us */
			XSync(dpy,FALSE);
			XGrabServer(dpy);
			XSync(dpy,FALSE);

			/* Set up an error handler that ignores BadMatches --
			   since the screensaver can take its window away at
			   any time, any call that uses it might choke */
			Xsublim_Sh_Status = 0;
			Xsublim_Sh_Handler =
			 XSetErrorHandler(xsublim_Sh_Handler);

			/* Save the current background */
			image_Image = XGetImage(dpy,win_Root,image_X,
			 image_Y,image_Width,image_Height,~0L,ZPixmap);

			/* If we've successfully saved the background... */
			if (image_Image != NULL)
			{
				if (Xsublim_Sh_Status == 0)
				{
					/* Draw the outline */
					if (arg_FlagOutline != FALSE)
					{
						for (text_OutlineIndex = 0;
						 text_Outline[
						 text_OutlineIndex].outline_X
						 != 0;text_OutlineIndex++)
						{
							/* Y'know, eight
							   character tabs and
							   descriptive variable
							   names become
							   annoying at some
							   point... */
							XDrawString(
							 dpy,
							 win_Root,gc_GcBack,
							 text_X+text_Outline[
							 text_OutlineIndex].
							 outline_X*
							 XSUBLIM_TEXT_OUTLINE,
							 text_Y+
							 (font_Font->ascent)+
							 text_Outline[
							 text_OutlineIndex].
							 outline_Y*
							 XSUBLIM_TEXT_OUTLINE,
							 text_Word,
							 text_Length);
						}
					}

					/* Draw the word */
					XDrawString(dpy,win_Root,
					 gc_GcFore,text_X,
					 text_Y+(font_Font->ascent),text_Word,
					 text_Length);
				}
				if (Xsublim_Sh_Status == 0)
				{
					/* Wait a bit */
					XSync(dpy,FALSE);
					if (Xsublim_Sig_Last == -1)
					{
						usleep(arg_DelayShow);
					}
	
					/* Restore the background */
					XPutImage(dpy,win_Root,
					 gc_GcFore,image_Image,0,0,image_X,
					 image_Y,image_Width,image_Height);
				}

				/* Free the image */
				XDestroyImage(image_Image);
			}

			/* Restore the error handler, ungrab the server */
                        XSync(dpy,FALSE);
			XSetErrorHandler(Xsublim_Sh_Handler);
			XUngrabServer(dpy);
                        XSync(dpy,FALSE);

			/* Pause between words */
			if (Xsublim_Sig_Last == -1)
			{
				usleep(arg_DelayWord);
			}
		}

		/* Pause between phrases */
		if (Xsublim_Sig_Last == -1)
		{
			usleep(random()%(arg_DelayPhraseMax-
			 arg_DelayPhraseMin+1)+arg_DelayPhraseMin);
		}
	}

	/* Exit ------------------------------------------------------------ */
	for (sig_Number = 0;sig_Signal[sig_Number] != -1;sig_Number++)
	{
		signal(sig_Number,SIG_DFL);
	}
	kill(getpid(),Xsublim_Sig_Last);

	return 0;
}
