/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Wendy, let me explain something to you.  Whenever you come in here and
 * interrupt me, you're BREAKING my CONCENTRATION.  You're DISTRACTING me!
 * And it will then take me time to get back to where I was. You understand?
 * Now, we're going to make a new rule.  When you come in here and you hear
 * me typing, or whether you DON'T hear me typing, or whatever the FUCK you
 * hear me doing; when I'm in here, it means that I am working, THAT means
 * don't come in!  Now, do you think you can handle that?
 */

#include <ctype.h>
#include "screenhack.h"

char *progclass = "XJack";

char *defaults [] = {
  "XJack.background:	black",		/* to placate SGI */
  "XJack.foreground:	#00EE00",
  "XJack.font:		-*-courier-medium-r-*-*-*-240-*-*-m-*-*-*",
  "*delay:		50000",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-font",		".font",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  static const char *source = "All work and no play makes Jack a dull boy.  ";
  const char *s = source;
  int columns, rows;		/* characters */
  int left, right;		/* characters */
  int char_width, line_height;	/* pixels */
  int x, y;			/* characters */
  int mode = 0;
  int hspace = 15;		/* pixels */
  int vspace = 15;		/* pixels */
  Bool break_para = True;
  Bool caps = False;
  int sentences = 0;
  int paras = 0;

  XWindowAttributes xgwa;
  XGCValues gcv;
  GC gc;
  int delay = get_integer_resource ("delay", "Integer");
  char *fontname = get_string_resource ("font", "Font");
  XFontStruct *font = XLoadQueryFont (dpy, fontname);

  if (!font)
    font = XLoadQueryFont (dpy, "-*-*-medium-r-*-*-*-240-*-*-m-*-*-*");
  if (!font)
    font = XLoadQueryFont (dpy, "-*-courier-medium-r-*-*-*-180-*-*-m-*-*-*");
  if (!font)
    font = XLoadQueryFont (dpy, "-*-*-*-r-*-*-*-240-*-*-m-*-*-*");
  if (!font)
    {
      fprintf(stderr, "no big fixed-width font like \"%s\"\n", fontname);
      exit(1);
    }

  XGetWindowAttributes (dpy, window, &xgwa);

  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource ("foreground", "Foreground", dpy,
				       xgwa.colormap);
  gcv.background = get_pixel_resource ("background", "Background", dpy,
				       xgwa.colormap);
  gc = XCreateGC (dpy, window, (GCFont | GCForeground | GCBackground), &gcv);

  char_width = (font->per_char
		? font->per_char['n'-font->min_char_or_byte2].rbearing
		: font->min_bounds.rbearing);
  line_height = font->ascent + font->descent + 1;

  columns = (xgwa.width - hspace - hspace) / char_width;
  rows = (xgwa.height - vspace - vspace) / line_height;

  left = 0xFF & (random() % ((columns / 2)+1));
  right = left + (0xFF & (random() % (columns - left - 10)+10));
  x = 0;
  y = 0;

  while (1)
    {
      int word_length = 0;
      const char *s2;
      for (s2 = s; *s2 && *s2 != ' '; s2++)
	word_length++;

      if (break_para ||
	  (*s != ' ' &&
	   (x + word_length) >= right))
	{
	  x = left;
	  y++;

	  if (break_para)
	    y++;

	  if (mode == 1 || mode == 2)
	    {
	      /* 1 = left margin goes southwest; 2 = southeast */
	      left += (mode == 1 ? 1 : -1);
	      if (left >= right - 10)
		{
		  if ((right < (columns - 10)) && (random() & 1))
		    right += (0xFF & (random() % (columns - right)));
		  else
		    mode = 2;
		}
	      else if (left <= 0)
		{
		  left = 0;
		  mode = 1;
		}
	    }
	  else if (mode == 3 || mode == 4)
	    {
	      /* 3 = right margin goes southeast; 4 = southwest */
	      right += (mode == 3 ? 1 : -1);
	      if (right >= columns)
		{
		  right = columns;
		  mode = 4;
		}
	      else if (right <= left + 10)
		mode = 3;
	    }

	  if (y >= rows)	/* bottom of page */
	    {
	      /* scroll by 1-5 lines */
	      int lines = (random() % 5 ? 0 : (0xFF & (random() % 5))) + 1;
	      if (break_para)
		lines++;

	      /* but sometimes scroll by a whole page */
	      if (0 == (random() % 100))
		lines += rows;

	      while (lines > 0)
		{
		  XCopyArea (dpy, window, window, gc,
			     0, hspace + line_height,
			     xgwa.width,
			     xgwa.height - vspace - vspace - line_height,
			     0, vspace);
		  XClearArea (dpy, window,
			      0, xgwa.height - vspace - line_height,
			      xgwa.width,
			      line_height + vspace + vspace,
			      False);
		  XClearArea (dpy, window, 0, 0, xgwa.width, vspace, False);
		  /* See? It's OK. He saw it on the television. */
		  XClearArea (dpy, window, 0, 0, hspace, xgwa.height, False);
		  XClearArea (dpy, window, xgwa.width - vspace, 0,
			      hspace, xgwa.height, False);
		  y--;
		  lines--;
		  XSync (dpy, True);
		  if (delay) usleep (delay * 10);
		}
	      if (y < 0) y = 0;
	    }

	  break_para = False;
	}

      if (*s != ' ')
	{
	  char c = *s;
	  int xshift = 0, yshift = 0;
	  if (0 == random() % 50)
	    {
	      xshift = random() % ((char_width / 3) + 1);      /* mis-strike */
	      yshift = random() % ((line_height / 6) + 1);
	      if (0 == (random() % 3))
		yshift *= 2;
	      if (random() & 1)
		xshift = -xshift;
	      if (random() & 1)
		yshift = -yshift;
	    }

	  if (0 == (random() % 250))	/* introduce adjascent-key typo */
	    {
	      static const char * const typo[] = {
		"asqw", "ASQW", "bgvhn", "cxdfv", "dserfcx", "ewsdrf",
		"Jhuikmn", "kjiol,m", "lkop;.,", "mnjk,", "nbhjm", "oiklp09",
		"pol;(-0", "redft54", "sawedxz", "uyhji87", "wqase32",
		"yuhgt67", ".,l;/", 0 };
	      int i = 0;
	      while (typo[i] && typo[i][0] != c)
		i++;
	      if (typo[i])
		c = typo[i][0xFF & (random() % strlen(typo[i]+1))];
	    }

	  /* caps typo */
	  if (c >= 'a' && c <= 'z' && (caps || 0 == (random() % 350)))
	    {
	      c -= ('a'-'A');
	      if (c == 'O' && random() & 1)
		c = '0';
	    }

	OVERSTRIKE:
	  XDrawString (dpy, window, gc,
		       (x * char_width) + hspace + xshift,
		       (y * line_height) + vspace + font->ascent + yshift,
		       &c, 1);
	  if (xshift == 0 && yshift == 0 && (0 == (random() & 3000)))
	    {
	      if (random() & 1)
		xshift--;
	      else
		yshift--;
	      goto OVERSTRIKE;
	    }

	  if ((tolower(c) != tolower(*s))
	      ? (0 == (random() % 10))		/* backup to correct */
	      : (0 == (random() % 400)))	/* fail to advance */
	    {
	      x--;
	      s--;
	      XSync (dpy, True);
	      if (delay) usleep (0xFFFF & (delay + (random() % (delay * 10))));
	    }
	}

      x++;
      s++;

      if (0 == random() % 200)
	{
	  if (random() & 1 && s != source)
	    s--;	/* duplicate character */
	  else if (*s)
	    s++;	/* skip character */
	}

      if (*s == 0)
	{
	  sentences++;
	  caps = (0 == random() % 40);	/* capitalize sentence */

	  if (0 == (random() % 10) ||	/* randomly break paragraph */
	      (mode == 0 &&
	       ((0 == (random() % 10)) || sentences > 20)))
	    {
	      break_para = True;
	      sentences = 0;
	      paras++;

	      if (random() & 1)		/* mode=0 50% of the time */
		mode = 0;
	      else
		mode = (0xFF & (random() % 5));

	      if (0 == (random() % 2))	/* re-pick margins */
		{
		  left = 0xFF & (random() % (columns / 3));
		  right = columns - (0xFF & (random() % (columns / 3)));

		  if (0 == random() % 3)	/* sometimes be wide */
		    right = left + ((right - left) / 2);
		}

	      if (right - left <= 10)	/* introduce sanity */
		{
		  left = 0;
		  right = columns;
		}

	      if (right - left > 50)	/* if wide, shrink and move */
		{
		  left += (0xFF & (random() % ((columns - 50) + 1)));
		  right = left + (0xFF & ((random() % 40) + 10));
		}

	      /* oh, gag. */
	      if (mode == 0 &&
		  right - left < 25 &&
		  columns > 40)
		{
		  right += 20;
		  if (right > columns)
		    left -= (right - columns);
		}
	    }
	  s = source;
	}

      XSync (dpy, True);
      if (delay)
	{
	  usleep (delay);
	  if (0 == random() % 3)
	    usleep(0xFFFFFF & ((random() % (delay * 5)) + 1));

	  if (break_para)
	    usleep(0xFFFFFF & ((random() % (delay * 15)) + 1));
	}

      if (paras > 5 &&
	  (0 == (random() % 1000)) &&
	  y < rows-5)
	{
	  int i;
	  int n = random() & 3;
	  y++;
	  for (i = 0; i < n; i++)
	    {
	      /* See also http://catalog.com/hopkins/unix-haters/login.html */
	      const char *n1 =
		"NFS server overlook not responding, still trying...";
	      const char *n2 = "NFS server overlook ok.";
	      while (*n1)
		{
		  XDrawString (dpy, window, gc,
			       (x * char_width) + hspace,
			       (y * line_height) + vspace + font->ascent,
			       n1, 1);
		  x++;
		  if (x >= columns) x = 0, y++;
		  n1++;
		}
	      XSync (dpy, True);
	      usleep (5000000);
	      while (*n2)
		{
		  XDrawString (dpy, window, gc,
			       (x * char_width) + hspace,
			       (y * line_height) + vspace + font->ascent,
			       n2, 1);
		  x++;
		  if (x >= columns) x = 0, y++;
		  n2++;
		}
	      y++;
	      XSync (dpy, True);
	      usleep (500000);
	    }
	}
    }
}
