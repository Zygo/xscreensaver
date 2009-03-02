/* xscreensaver, Copyright (c) 1998-2003 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Blue Screen of Death: the finest in personal computer emulation.
 * Concept cribbed from Stephen Martin <smartin@mks.com>;
 * this version written by jwz, 4-Jun-98.
 */

#include <math.h>
#include "screenhack.h"
#include "xpm-pixmap.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <X11/Xutil.h>

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
#endif

#ifdef HAVE_UNAME
# include <sys/utsname.h>
#endif /* HAVE_UNAME */

#include "images/amiga.xpm"
#include "images/atari.xbm"
#include "images/mac.xbm"
#include "images/macbomb.xbm"
#include "images/hmac.xpm"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static int
draw_string (Display *dpy, Window window, GC gc, XGCValues *gcv,
	     XFontStruct *font,
	     int xoff, int yoff,
	     int win_width, int win_height,
	     const char *string, int delay)
{
  int x, y;
  int width = 0, height = 0, cw = 0;
  int char_width, line_height;

  const char *s = string;
  const char *se = string;

  /* This pretty much assumes fixed-width fonts */
  char_width = (font->per_char
		? font->per_char['n'-font->min_char_or_byte2].width
		: font->min_bounds.width);
  line_height = font->ascent + font->descent + 1;

  while (1)
    {
      if (*s == '\n' || !*s)
	{
	  height++;
	  if (cw > width) width = cw;
	  cw = 0;
	  if (!*s) break;
	}
      else
	cw++;
      s++;
    }

  x = (win_width - (width * char_width)) / 2;
  y = (win_height - (height * line_height)) / 2;

  if (x < 0) x = 2;
  if (y < 0) y = 2;

  x += xoff;
  y += yoff;

  se = s = string;
  while (1)
    {
      if (*s == '\n' || !*s)
	{
	  int off = 0;
	  Bool flip = False;

	  if (*se == '@' || *se == '_')
	    {
	      if (*se == '@') flip = True;
	      se++;
	      off = (char_width * (width - (s - se))) / 2;
	    }

	  if (flip)
	    {
	      XSetForeground(dpy, gc, gcv->background);
	      XSetBackground(dpy, gc, gcv->foreground);
	    }

	  if (s != se)
	    XDrawImageString(dpy, window, gc, x+off, y+font->ascent, se, s-se);

	  if (flip)
	    {
	      XSetForeground(dpy, gc, gcv->foreground);
	      XSetBackground(dpy, gc, gcv->background);
	    }

	  se = s;
	  y += line_height;
	  if (!*s) break;
	  se = s+1;

	  if (delay)
	    {
	      XSync(dpy, False);
	      usleep(delay);
	    }
	}
      s++;
    }

  return width * char_width;
}


static Pixmap
double_pixmap(Display *dpy, GC gc, Visual *visual, int depth, Pixmap pixmap,
	     int pix_w, int pix_h)
{
  int x, y;
  Pixmap p2 = XCreatePixmap(dpy, pixmap, pix_w*2, pix_h*2, depth);
  XImage *i1 = XGetImage(dpy, pixmap, 0, 0, pix_w, pix_h, ~0L, ZPixmap);
  XImage *i2 = XCreateImage(dpy, visual, depth, ZPixmap, 0, 0,
			    pix_w*2, pix_h*2, 8, 0);
  i2->data = (char *) calloc(i2->height, i2->bytes_per_line);
  for (y = 0; y < pix_h; y++)
    for (x = 0; x < pix_w; x++)
      {
	unsigned long p = XGetPixel(i1, x, y);
	XPutPixel(i2, x*2,   y*2,   p);
	XPutPixel(i2, x*2+1, y*2,   p);
	XPutPixel(i2, x*2,   y*2+1, p);
	XPutPixel(i2, x*2+1, y*2+1, p);
      }
  free(i1->data); i1->data = 0;
  XDestroyImage(i1);
  XPutImage(dpy, p2, gc, i2, 0, 0, 0, 0, i2->width, i2->height);
  free(i2->data); i2->data = 0;
  XDestroyImage(i2);
  XFreePixmap(dpy, pixmap);
  return p2;
}


/* Sleep for N seconds and return False.  But if a key or mouse event is
   seen, discard all pending key or mouse events, and return True.
 */
static Bool
bsod_sleep(Display *dpy, int seconds)
{
  int q = seconds * 4;
  int quantum = 250000;

  if (seconds == -1)
    q = 1, quantum = 100000;

  do
    {
      XSync(dpy, False);
      while (XPending (dpy))
        {
          XEvent event;
          XNextEvent (dpy, &event);
          if (event.xany.type == ButtonPress)
            return True;
          if (event.xany.type == KeyPress)
            {
              KeySym keysym;
              char c = 0;
              XLookupString (&event.xkey, &c, 1, &keysym, 0);
              if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                return True;
            }
          screenhack_handle_event (dpy, &event);
        }

      if (q > 0)
	{
	  q--;
	  usleep(quantum);
	}
    }
  while (q > 0);

  return False; 
}


static void
windows (Display *dpy, Window window, int delay, int which)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *fontname;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc;

  const char *w95 =
    ("@Windows\n"
     "A fatal exception 0E has occured at F0AD:42494C4C\n"
     "the current application will be terminated.\n"
     "\n"
     "* Press any key to terminate the current application.\n"
     "* Press CTRL+ALT+DELETE again to restart your computer.\n"
     "  You will lose any unsaved information in all applications.\n"
     "\n"
     "\n"
     "_Press any key to continue");

  const char *wnt = /* from Jim Niemira <urmane@urmane.org> */
    ("*** STOP: 0x0000001E (0x80000003,0x80106fc0,0x8025ea21,0xfd6829e8)\n"
   "Unhandled Kernel exception c0000047 from fa8418b4 (8025ea21,fd6829e8)\n"
   "\n"
   "Dll Base Date Stamp - Name             Dll Base Date Stamp - Name\n"
   "80100000 2be154c9 - ntoskrnl.exe       80400000 2bc153b0 - hal.dll\n"
   "80258000 2bd49628 - ncrc710.sys        8025c000 2bd49688 - SCSIPORT.SYS \n"
   "80267000 2bd49683 - scsidisk.sys       802a6000 2bd496b9 - Fastfat.sys\n"
   "fa800000 2bd49666 - Floppy.SYS         fa810000 2bd496db - Hpfs_Rec.SYS\n"
   "fa820000 2bd49676 - Null.SYS           fa830000 2bd4965a - Beep.SYS\n"
   "fa840000 2bdaab00 - i8042prt.SYS       fa850000 2bd5a020 - SERMOUSE.SYS\n"
   "fa860000 2bd4966f - kbdclass.SYS       fa870000 2bd49671 - MOUCLASS.SYS\n"
   "fa880000 2bd9c0be - Videoprt.SYS       fa890000 2bd49638 - NCC1701E.SYS\n"
   "fa8a0000 2bd4a4ce - Vga.SYS            fa8b0000 2bd496d0 - Msfs.SYS\n"
   "fa8c0000 2bd496c3 - Npfs.SYS           fa8e0000 2bd496c9 - Ntfs.SYS\n"
   "fa940000 2bd496df - NDIS.SYS           fa930000 2bd49707 - wdlan.sys\n"
   "fa970000 2bd49712 - TDI.SYS            fa950000 2bd5a7fb - nbf.sys\n"
   "fa980000 2bd72406 - streams.sys        fa9b0000 2bd4975f - ubnb.sys\n"
   "fa9c0000 2bd5bfd7 - usbser.sys         fa9d0000 2bd4971d - netbios.sys\n"
   "fa9e0000 2bd49678 - Parallel.sys       fa9f0000 2bd4969f - serial.SYS\n"
   "faa00000 2bd49739 - mup.sys            faa40000 2bd4971f - SMBTRSUP.SYS\n"
   "faa10000 2bd6f2a2 - srv.sys            faa50000 2bd4971a - afd.sys\n"
   "faa60000 2bd6fd80 - rdr.sys            faaa0000 2bd49735 - bowser.sys\n"
   "\n"
   "Address dword dump Dll Base                                      - Name\n"
   "801afc20 80106fc0 80106fc0 00000000 00000000 80149905 : "
     "fa840000 - i8042prt.SYS\n"
   "801afc24 80149905 80149905 ff8e6b8c 80129c2c ff8e6b94 : "
     "8025c000 - SCSIPORT.SYS\n"
   "801afc2c 80129c2c 80129c2c ff8e6b94 00000000 ff8e6b94 : "
     "80100000 - ntoskrnl.exe\n"
   "801afc34 801240f2 80124f02 ff8e6df4 ff8e6f60 ff8e6c58 : "
     "80100000 - ntoskrnl.exe\n"
   "801afc54 80124f16 80124f16 ff8e6f60 ff8e6c3c 8015ac7e : "
     "80100000 - ntoskrnl.exe\n"
   "801afc64 8015ac7e 8015ac7e ff8e6df4 ff8e6f60 ff8e6c58 : "
     "80100000 - ntoskrnl.exe\n"
   "801afc70 80129bda 80129bda 00000000 80088000 80106fc0 : "
     "80100000 - ntoskrnl.exe\n"
   "\n"
   "Kernel Debugger Using: COM2 (Port 0x2f8, Baud Rate 19200)\n"
   "Restart and set the recovery options in the system control panel\n"
   "or the /CRASHDEBUG system start option. If this message reappears,\n"
   "contact your system administrator or technical support group."
     );

  const char *w2ka =
    ("*** STOP: 0x000000D1 (0xE1D38000,0x0000001C,0x00000000,0xF09D42DA)\n"
     "DRIVER_IRQL_NOT_LESS_OR_EQUAL \n"
     "\n"
    "*** Address F09D42DA base at F09D4000, DateStamp 39f459ff - CRASHDD.SYS\n"
     "\n"
     "Beginning dump of physical memory\n");
  const char *w2kb =
    ("Physical memory dump complete. Contact your system administrator or\n"
     "technical support group.\n");

  const char *wmea =
    ("    Windows protection error.  You need to restart your computer.");
  const char *wmeb =
    ("    System halted.");

  if (which < 0 || which > 2) abort();

  /* kludge to lump Win2K and WinME together; seems silly to add another
     preference/command line option just for this little one. */
  if (which == 2 && (random() % 2))
    which = 3;

  XGetWindowAttributes (dpy, window, &xgwa);

  fontname = get_string_resource ((xgwa.height > 600
				   ? (which == 0 ? "windows95.font2" :
                                      which == 1 ? "windowsNT.font2" :
                                      which == 2 ? "windows2K.font2" :
                                                   "windowsME.font2")
				   : (which == 0 ? "windows95.font" :
				      which == 1 ? "windowsNT.font" :
				      which == 2 ? "windows2K.font" :
                                                   "windowsME.font")),
				  "Windows.Font");
  if (!fontname || !*fontname) fontname = (char *)def_font;
  font = XLoadQueryFont (dpy, fontname);
  if (!font) font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
  if (fontname && fontname != def_font)
    free (fontname);

  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource((which == 0 ? "windows95.foreground" :
				       which == 1 ? "windowsNT.foreground" :
				       which == 2 ? "windows2K.foreground" :
                                                    "windowsME.foreground"),
				      "Windows.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource((which == 0 ? "windows95.background" :
				       which == 1 ? "windowsNT.background" :
				       which == 2 ? "windows2K.background" :
                                                    "windowsME.background"),
				      "Windows.Background",
				      dpy, xgwa.colormap);
  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy, window);

  gc = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);

  if (which == 0)
    draw_string(dpy, window, gc, &gcv, font,
		0, 0, xgwa.width, xgwa.height, w95, 0);
  else if (which == 1)
    draw_string(dpy, window, gc, &gcv, font, 0, 0, 10, 10, wnt, 750);
  else if (which == 2)
    {
      int line_height = font->ascent + font->descent + 1;
      int x = 20;
      int y = (xgwa.height / 4);

      draw_string(dpy, window, gc, &gcv, font, x, y, 10, 10, w2ka, 750);
      y += line_height * 6;
      bsod_sleep(dpy, 4);
      draw_string(dpy, window, gc, &gcv, font, x, y, 10, 10, w2kb, 750);
    }
  else if (which == 3)
    {
      int line_height = font->ascent + font->descent;
      int x = 0;
      int y = (xgwa.height - line_height * 3) / 2;
      draw_string (dpy, window, gc, &gcv, font, x, y, 10, 10, wmea, 0);
      y += line_height * 2;
      x = draw_string (dpy, window, gc, &gcv, font, x, y, 10, 10, wmeb, 0);
      y += line_height;
      while (delay > 0)
        {
          XDrawImageString (dpy, window, gc, x, y, "_", 1);
          XSync(dpy, False);
          usleep(120000L);
          XDrawImageString (dpy, window, gc, x, y, " ", 1);
          XSync(dpy, False);
          usleep(120000L);
          if (bsod_sleep(dpy, 0))
            delay = 0;
          else
            delay--;
        }
    }
  else
    abort();

  XFreeGC(dpy, gc);
  XSync(dpy, False);
  bsod_sleep(dpy, delay);
  XClearWindow(dpy, window);
  XFreeFont(dpy, font);
}

static void
windows_31 (Display *dpy, Window window, int delay)
{
  windows (dpy, window, delay, 0);
}

static void
windows_nt (Display *dpy, Window window, int delay)
{
  windows (dpy, window, delay, 1);
}

static void
windows_2k (Display *dpy, Window window, int delay)
{
  windows (dpy, window, delay, 2);
}


/* SCO OpenServer 5 panic, by Tom Kelly <tom@ancilla.toronto.on.ca>
 */
static void
sco (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *fontname;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc;
  int lines_1 = 0, lines_2 = 0, lines_3 = 0, lines_4 = 0;
  const char *s;

  const char *sco_panic_1 =
    ("Unexpected trap in kernel mode:\n"
     "\n"
     "cr0 0x80010013     cr2  0x00000014     cr3 0x00000000  tlb  0x00000000\n"
     "ss  0x00071054    uesp  0x00012055     efl 0x00080888  ipl  0x00000005\n"
     "cs  0x00092585     eip  0x00544a4b     err 0x004d4a47  trap 0x0000000E\n"
     "eax 0x0045474b     ecx  0x0042544b     edx 0x57687920  ebx  0x61726520\n"
     "esp 0x796f7520     ebp  0x72656164     esi 0x696e6720  edi  0x74686973\n"
     "ds  0x3f000000     es   0x43494c48     fs  0x43525343  gs   0x4f4d4b53\n"
     "\n"
     "PANIC: k_trap - kernel mode trap type 0x0000000E\n"
     "Trying to dump 5023 pages to dumpdev hd (1/41), 63 pages per '.'\n"
    );
  const char *sco_panic_2 =
   ("................................................................."
    "..............\n"
    );
  const char *sco_panic_3 =
    ("5023 pages dumped\n"
     "\n"
     "\n"
     );
  const char *sco_panic_4 =
    ("**   Safe to Power Off   **\n"
     "           - or -\n"
     "** Press Any Key to Reboot **\n"
    );

  for (s = sco_panic_1; *s; s++) if (*s == '\n') lines_1++;
  for (s = sco_panic_2; *s; s++) if (*s == '\n') lines_2++;
  for (s = sco_panic_3; *s; s++) if (*s == '\n') lines_3++;
  for (s = sco_panic_4; *s; s++) if (*s == '\n') lines_4++;

  XGetWindowAttributes (dpy, window, &xgwa);

  fontname = get_string_resource ((xgwa.height > 600
				   ? "sco.font2"
				   : "sco.font"),
				  "SCO.Font");
  if (!fontname || !*fontname) fontname = (char *)def_font;
  font = XLoadQueryFont (dpy, fontname);
  if (!font) font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
  if (fontname && fontname != def_font)
    free (fontname);

  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource(("sco.foreground"),
				      "SCO.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource(("sco.background"),
				      "SCO.Background",
				      dpy, xgwa.colormap);
  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy, window);

  gc = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);

  draw_string(dpy, window, gc, &gcv, font,
	      10, xgwa.height - ((lines_1 + lines_2 + lines_3 + lines_4 + 1) *
                                 (font->ascent + font->descent + 1)),
	      10, 10,
	      sco_panic_1, 0);
  XSync(dpy, False);
  for (s = sco_panic_2; *s; s++)
    {
      char *ss = strdup(sco_panic_2);
      ss[s - sco_panic_2] = 0;
      draw_string(dpy, window, gc, &gcv, font,
                  10, xgwa.height - ((lines_2 + lines_3 + lines_4 + 1) *
                                     (font->ascent + font->descent + 1)),
                  10, 10,
                  ss, 0);
      XSync(dpy, False);
      free(ss);
      if (bsod_sleep (dpy, -1))
        goto DONE;
    }

  draw_string(dpy, window, gc, &gcv, font,
	      10, xgwa.height - ((lines_3 + lines_4 + 1) *
                                 (font->ascent + font->descent + 1)),
	      10, 10,
	      sco_panic_3, 0);
  XSync(dpy, False);
  if (bsod_sleep(dpy, 1))
    goto DONE;
  draw_string(dpy, window, gc, &gcv, font,
	      10, xgwa.height - ((lines_4 + 1) *
                                 (font->ascent + font->descent + 1)),
	      10, 10,
	      sco_panic_4, 0);
  XSync(dpy, False);

  bsod_sleep(dpy, delay);
 DONE:
  XClearWindow(dpy, window);
  XFreeGC(dpy, gc);
  XFreeFont(dpy, font);
}


/* Linux (sparc) panic, by Tom Kelly <tom@ancilla.toronto.on.ca>
 */
static void
sparc_linux (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *fontname;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc;
  int lines = 1;
  const char *s;

  const char *linux_panic =
    ("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
	"Unable to handle kernel paging request at virtual address f0d4a000\n"
	"tsk->mm->context = 00000014\n"
	"tsk->mm->pgd = f26b0000\n"
	"              \\|/ ____ \\|/\n"
	"              \"@'/ ,. \\`@\"\n"
	"              /_| \\__/ |_\\\n"
	"                 \\__U_/\n"
	"gawk(22827): Oops\n"
	"PSR: 044010c1 PC: f001c2cc NPC: f001c2d0 Y: 00000000\n"
	"g0: 00001000 g1: fffffff7 g2: 04401086 g3: 0001eaa0\n"
	"g4: 000207dc g5: f0130400 g6: f0d4a018 g7: 00000001\n"
	"o0: 00000000 o1: f0d4a298 o2: 00000040 o3: f1380718\n"
	"o4: f1380718 o5: 00000200 sp: f1b13f08 ret_pc: f001c2a0\n"
	"l0: efffd880 l1: 00000001 l2: f0d4a230 l3: 00000014\n"
	"l4: 0000ffff l5: f0131550 l6: f012c000 l7: f0130400\n"
	"i0: f1b13fb0 i1: 00000001 i2: 00000002 i3: 0007c000\n"
	"i4: f01457c0 i5: 00000004 i6: f1b13f70 i7: f0015360\n"
	"Instruction DUMP:\n"
    );

  for (s = linux_panic; *s; s++) if (*s == '\n') lines++;

  XGetWindowAttributes (dpy, window, &xgwa);

  fontname = get_string_resource ((xgwa.height > 600
				   ? "sparclinux.font2"
				   : "sparclinux.font"),
				  "SparcLinux.Font");
  if (!fontname || !*fontname) fontname = (char *)def_font;
  font = XLoadQueryFont (dpy, fontname);
  if (!font) font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
  if (fontname && fontname != def_font)
    free (fontname);

  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource(("sparclinux.foreground"),
				      "SparcLinux.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource(("sparclinux.background"),
				      "SparcLinux.Background",
				      dpy, xgwa.colormap);
  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy, window);

  gc = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);

  draw_string(dpy, window, gc, &gcv, font,
	      10, xgwa.height - (lines * (font->ascent + font->descent + 1)),
	      10, 10,
	      linux_panic, 0);
  XFreeGC(dpy, gc);
  XSync(dpy, False);
  bsod_sleep(dpy, delay);
  XClearWindow(dpy, window);
  XFreeFont(dpy, font);
}

/* BSD Panic by greywolf@starwolf.com - modeled after the Linux panic above.
   By Grey Wolf <greywolf@siteROCK.com>
 */
static void
bsd (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *fontname;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc;
  int lines = 1;
  int i, n, b;
  const char *rbstr, *panicking;
  char syncing[80], bbuf[5], *bp;

  const char *panicstr[] =
   {"panic: ifree: freeing free inode",
    "panic: blkfree: freeing free block",
    "panic: improbability coefficient below zero",
    "panic: cgsixmmap",
    "panic: crazy interrupts",
    "panic: nmi",
    "panic: attempted windows install",
    "panic: don't",
    "panic: free inode isn't",
    "panic: cpu_fork: curproc",
    "panic: malloc: out of space in kmem_map",
    "panic: vogon starship detected",
    "panic: teleport chamber: out of order",
    "panic: Brain fried - core dumped"};
     
  for (i = 0; i < sizeof(syncing); i++)
    syncing[i] = 0;

  i = (random() & 0xffff) % (sizeof(panicstr) / sizeof(*panicstr));

  panicking = panicstr[i];
  strcpy(syncing, "Syncing disks: ");

  b = (random() & 0xff) % 40;
  for (n = 0; (n < 20) && (b > 0); n++)
    {
      if (i)
        {
          i = (random() & 0x7);
          b -= (random() & 0xff) % 20;
          if (b < 0)
            b = 0;
        }
      sprintf (bbuf, "%d ", b);
      strcat (syncing, bbuf);
    }

  if (b)
    rbstr = "damn!";
  else
    rbstr = "sunk!";

  lines = 5;

  XGetWindowAttributes (dpy, window, &xgwa);

  fontname = get_string_resource ((xgwa.height > 600
				   ? "bsd.font2"
				   : "bsd.font"),
				  "BSD.Font");
  if (!fontname || !*fontname) fontname = (char *)def_font;
  font = XLoadQueryFont (dpy, fontname);
  if (!font) font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
  if (fontname && fontname != def_font)
    free (fontname);

  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource(("bsd.foreground"),
				      "BSD.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource(("bsd.background"),
				      "BSD.Background",
				      dpy, xgwa.colormap);
  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy, window);

  gc = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);

  draw_string(dpy, window, gc, &gcv, font,
	      10, xgwa.height - (lines * (font->ascent + font->descent + 1)),
	      10, 10,
	      panicking, 0);
  XSync(dpy, False);
  lines--;

  for (bp = syncing; *bp;)
    {
      char *bsd_bufs, oc = 0;
      for (;*bp && (*bp != ' '); bp++)
        ;
      if (*bp == ' ')
        {
          oc = *bp;
          *bp = 0;
        }
      bsd_bufs = strdup(syncing);
      draw_string(dpy, window, gc, &gcv, font,
                  10,
                  xgwa.height - (lines * (font->ascent + font->descent + 1)),
                  10, 10,
                  bsd_bufs, 0);
      XSync(dpy, False);
      free(bsd_bufs);
      if (oc)
	*bp = oc;
      if (bsod_sleep(dpy, -1))
        goto DONE;
      bp++;
    }

  lines--;
  
  draw_string(dpy, window, gc, &gcv, font,
	      10, xgwa.height - (lines * (font->ascent + font->descent + 1)),
	      10, 10,
	      rbstr, 0);
  lines--;
  draw_string(dpy, window, gc, &gcv, font,
	      10, xgwa.height - (lines * (font->ascent + font->descent + 1)),
	      10, 10,
	      "Rebooting", 0);

  XFreeGC(dpy, gc);
  XSync(dpy, False);
  bsod_sleep(dpy, delay);

DONE:
  XClearWindow(dpy, window);
  XFreeFont(dpy, font);
}

static void
amiga (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *fontname;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc, gc2;
  int height;
  unsigned long fg, bg, bg2;
  Pixmap pixmap = 0;
  int pix_w = 0, pix_h = 0;
  int string_width;
  int margin;

  const char *string =
    ("_Software failure.  Press left mouse button to continue.\n"
     "_Guru Meditation #00000003.00C01570");

  XGetWindowAttributes (dpy, window, &xgwa);

  fontname = get_string_resource ((xgwa.height > 600
				   ? "amiga.font2" : "amiga.font"),
				  "Amiga.Font");
  if (!fontname || !*fontname) fontname = (char *)def_font;
  font = XLoadQueryFont (dpy, fontname);
  if (!font) font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
  if (fontname && fontname != def_font)
    free (fontname);

  gcv.font = font->fid;
  fg = gcv.foreground = get_pixel_resource("amiga.foreground",
					   "Amiga.Foreground",
					   dpy, xgwa.colormap);
  bg = gcv.background = get_pixel_resource("amiga.background",
					   "Amiga.Background",
					   dpy, xgwa.colormap);
  bg2 = get_pixel_resource("amiga.background2", "Amiga.Background",
			   dpy, xgwa.colormap);
  XSetWindowBackground(dpy, window, bg2);
  XClearWindow(dpy, window);

  gc = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);
  gcv.background = fg; gcv.foreground = bg;
  gc2 = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);

  height = (font->ascent + font->descent) * 6;

#if defined(HAVE_GDK_PIXBUF) || defined (HAVE_XPM)
  pixmap = xpm_data_to_pixmap (dpy, window, (char **) amiga_hand,
                               &pix_w, &pix_h, 0);
#endif /* HAVE_GDK_PIXBUF || HAVE_XPM */

  if (pixmap && xgwa.height > 600)	/* scale up the bitmap */
    {
      pixmap = double_pixmap(dpy, gc, xgwa.visual, xgwa.depth,
			     pixmap, pix_w, pix_h);
      pix_w *= 2;
      pix_h *= 2;
    }

  if (pixmap)
    {
      int x = (xgwa.width - pix_w) / 2;
      int y = ((xgwa.height - pix_h) / 2);
      XCopyArea(dpy, pixmap, window, gc, 0, 0, pix_w, pix_h, x, y);

      XSync(dpy, False);
      bsod_sleep(dpy, 2);

      XCopyArea(dpy, pixmap, window, gc, 0, 0, pix_w, pix_h, x, y + height);
      XClearArea(dpy, window, 0, 0, xgwa.width, y + height, False);
      XFreePixmap(dpy, pixmap);
    }

  XFillRectangle(dpy, window, gc2, 0, 0, xgwa.width, height);
  margin = font->ascent;
  string_width = draw_string(dpy, window, gc, &gcv, font,
                             margin, 0,
                             xgwa.width - (margin * 2), height,
                             string, 0);
  {
    GC gca = gc;
    while (delay > 0)
      {
        int x2;
	XFillRectangle(dpy, window, gca, 0, 0, xgwa.width, margin);
	XFillRectangle(dpy, window, gca, 0, 0, margin, height);
        XFillRectangle(dpy, window, gca,
                       0, height - margin, xgwa.width, margin);
        x2 = margin + string_width;
        if (x2 < xgwa.width - margin) x2 = xgwa.width - margin;
        XFillRectangle(dpy, window, gca, x2, 0, margin, height);

	gca = (gca == gc ? gc2 : gc);
	XSync(dpy, False);
	if (bsod_sleep(dpy, 1))
	  break;
	delay--;
      }
  }

  XFreeGC(dpy, gc);
  XFreeGC(dpy, gc2);
  XSync(dpy, False);
  XClearWindow(dpy, window);
  XFreeFont(dpy, font);
}


/* Atari ST, by Marcus Herbert <rhoenie@nobiscum.de>
   Marcus had this to say:

	Though I still have my Atari somewhere, I hardly remember
	the meaning of the bombs. I think 9 bombs was "bus error" or
	something like that.  And you often had a few bombs displayed
	quickly and then the next few ones coming up step by step.
	Perhaps somebody else can tell you more about it..  its just
	a quick hack :-}
 */
static void
atari (Display *dpy, Window window, int delay)
{
	
  XGCValues gcv;
  XWindowAttributes xgwa;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc;
  Pixmap pixmap = 0;
  int pix_w = atari_width;
  int pix_h = atari_height;
  int offset;
  int i, x, y;

  XGetWindowAttributes (dpy, window, &xgwa);

  font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
                
  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource("atari.foreground", "Atari.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource("atari.background", "Atari.Background",
				      dpy, xgwa.colormap);

  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy, window);

  gc = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);

  pixmap = XCreatePixmapFromBitmapData(dpy, window, (char *) atari_bits,
				       pix_w, pix_h,
				       gcv.foreground, gcv.background,
				       xgwa.depth);
  pixmap = double_pixmap(dpy, gc, xgwa.visual, xgwa.depth,
			 pixmap, pix_w, pix_h);
  pix_w *= 2;
  pix_h *= 2;

  offset = pix_w + 2;
  x = 5;
  y = (xgwa.height - (xgwa.height / 5));
  if (y < 0) y = 0;

  for (i=0 ; i<7 ; i++) {
    XCopyArea(dpy, pixmap, window, gc, 0, 0, pix_w, pix_h,
	      (x + (i*offset)), y);
  }  
  
  for (i=7 ; i<10 ; i++) {
    if (bsod_sleep(dpy, 1))
      goto DONE;
    XCopyArea(dpy, pixmap, window, gc, 0, 0, pix_w, pix_h,
	      (x + (i*offset)), y);
  }

  bsod_sleep(dpy, delay);
 DONE:
  XFreePixmap(dpy, pixmap);
  XFreeGC(dpy, gc);
  XSync(dpy, False);
  XClearWindow(dpy, window);
  XFreeFont(dpy, font);
}


static void
mac (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *fontname;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc;
  Pixmap pixmap = 0;
  int pix_w = mac_width;
  int pix_h = mac_height;
  int offset = mac_height * 4;
  int i;

  const char *string = ("0 0 0 0 0 0 0 F\n"
			"0 0 0 0 0 0 0 3");

  XGetWindowAttributes (dpy, window, &xgwa);

  fontname = get_string_resource ("mac.font", "Mac.Font");
  if (!fontname || !*fontname) fontname = (char *)def_font;
  font = XLoadQueryFont (dpy, fontname);
  if (!font) font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
  if (fontname && fontname != def_font)
    free (fontname);

  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource("mac.foreground", "Mac.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource("mac.background", "Mac.Background",
				      dpy, xgwa.colormap);
  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy, window);

  gc = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);

  pixmap = XCreatePixmapFromBitmapData(dpy, window, (char *) mac_bits,
				       mac_width, mac_height,
				       gcv.foreground,
				       gcv.background,
				       xgwa.depth);

  for(i = 0; i < 2; i++)
    {
      pixmap = double_pixmap(dpy, gc, xgwa.visual, xgwa.depth,
			     pixmap, pix_w, pix_h);
      pix_w *= 2; pix_h *= 2;
    }

  {
    int x = (xgwa.width - pix_w) / 2;
    int y = (((xgwa.height + offset) / 2) -
	     pix_h -
	     (font->ascent + font->descent) * 2);
    if (y < 0) y = 0;
    XCopyArea(dpy, pixmap, window, gc, 0, 0, pix_w, pix_h, x, y);
    XFreePixmap(dpy, pixmap);
  }

  draw_string(dpy, window, gc, &gcv, font, 0, 0,
	      xgwa.width, xgwa.height + offset, string, 0);

  XFreeGC(dpy, gc);
  XSync(dpy, False);
  bsod_sleep(dpy, delay);
  XClearWindow(dpy, window);
  XFreeFont(dpy, font);
}

static void
macsbug (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *fontname;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc, gc2;

  int char_width, line_height;
  int col_right, row_top, row_bottom, page_right, page_bottom, body_top;
  int xoff, yoff;

  const char *left = ("    SP     \n"
		      " 04EB0A58  \n"
		      "58 00010000\n"
		      "5C 00010000\n"
		      "   ........\n"
		      "60 00000000\n"
		      "64 000004EB\n"
		      "   ........\n"
		      "68 0000027F\n"
		      "6C 2D980035\n"
		      "   ....-..5\n"
		      "70 00000054\n"
		      "74 0173003E\n"
		      "   ...T.s.>\n"
		      "78 04EBDA76\n"
		      "7C 04EBDA8E\n"
		      "   .S.L.a.U\n"
		      "80 00000000\n"
		      "84 000004EB\n"
		      "   ........\n"
		      "88 00010000\n"
		      "8C 00010000\n"
		      "   ...{3..S\n"
		      "\n"
		      "\n"
		      " CurApName \n"
		      "  Finder   \n"
		      "\n"
		      " 32-bit VM \n"
		      "SR Smxnzvc0\n"
		      "D0 04EC0062\n"
		      "D1 00000053\n"
		      "D2 FFFF0100\n"
		      "D3 00010000\n"
		      "D4 00010000\n"
		      "D5 04EBDA76\n"
		      "D6 04EBDA8E\n"
		      "D7 00000001\n"
		      "\n"
		      "A0 04EBDA76\n"
		      "A1 04EBDA8E\n"
		      "A2 A0A00060\n"
		      "A3 027F2D98\n"
		      "A4 027F2E58\n"
		      "A5 04EC04F0\n"
		      "A6 04EB0A86\n"
		      "A7 04EB0A58");
  const char *bottom = ("  _A09D\n"
			"     +00884    40843714     #$0700,SR         "
			"                  ; A973        | A973\n"
			"     +00886    40843765     *+$0400           "
			"                                | 4A1F\n"
			"     +00888    40843718     $0004(A7),([0,A7[)"
			"                  ; 04E8D0AE    | 66B8");

#if 0
  const char *body = ("Bus Error at 4BF6D6CC\n"
		      "while reading word from 4BF6D6CC in User data space\n"
		      " Unable to access that address\n"
		      "  PC: 2A0DE3E6\n"
		      "  Frame Type: B008");
#else
  const char * body = ("PowerPC unmapped memory exception at 003AFDAC "
						"BowelsOfTheMemoryMgr+04F9C\n"
		      " Calling chain using A6/R1 links\n"
		      "  Back chain  ISA  Caller\n"
		      "  00000000    PPC  28C5353C  __start+00054\n"
		      "  24DB03C0    PPC  28B9258C  main+0039C\n"
		      "  24DB0350    PPC  28B9210C  MainEvent+00494\n"
		      "  24DB02B0    PPC  28B91B40  HandleEvent+00278\n"
		      "  24DB0250    PPC  28B83DAC  DoAppleEvent+00020\n"
		      "  24DB0210    PPC  FFD3E5D0  "
						"AEProcessAppleEvent+00020\n"
		      "  24DB0132    68K  00589468\n"
		      "  24DAFF8C    68K  00589582\n"
		      "  24DAFF26    68K  00588F70\n"
		      "  24DAFEB3    PPC  00307098  "
						"EmToNatEndMoveParams+00014\n"
		      "  24DAFE40    PPC  28B9D0B0  DoScript+001C4\n"
		      "  24DAFDD0    PPC  28B9C35C  RunScript+00390\n"
		      "  24DAFC60    PPC  28BA36D4  run_perl+000E0\n"
		      "  24DAFC10    PPC  28BC2904  perl_run+002CC\n"
		      "  24DAFA80    PPC  28C18490  Perl_runops+00068\n"
		      "  24DAFA30    PPC  28BE6CC0  Perl_pp_backtick+000FC\n"
		      "  24DAF9D0    PPC  28BA48B8  Perl_my_popen+00158\n"
		      "  24DAF980    PPC  28C5395C  sfclose+00378\n"
		      "  24DAF930    PPC  28BA568C  free+0000C\n"
		      "  24DAF8F0    PPC  28BA6254  pool_free+001D0\n"
		      "  24DAF8A0    PPC  FFD48F14  DisposePtr+00028\n"
		      "  24DAF7C9    PPC  00307098  "
						"EmToNatEndMoveParams+00014\n"
		      "  24DAF780    PPC  003AA180  __DisposePtr+00010");
#endif

  const char *s;
  int body_lines = 1;

  for (s = body; *s; s++) if (*s == '\n') body_lines++;

  XGetWindowAttributes (dpy, window, &xgwa);

  fontname = get_string_resource ((xgwa.height > 850
				   ? "macsbug.font3"
				   : (xgwa.height > 700
				      ? "macsbug.font2"
				      : "macsbug.font")),
				  "MacsBug.Font");
  if (!fontname || !*fontname) fontname = (char *)def_font;
  font = XLoadQueryFont (dpy, fontname);
  if (!font) font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
  if (fontname && fontname != def_font)
    free (fontname);

  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource("macsbug.foreground",
				      "MacsBug.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource("macsbug.background",
				      "MacsBug.Background",
				      dpy, xgwa.colormap);

  gc = XCreateGC(dpy, window, GCFont|GCForeground|GCBackground, &gcv);

  gcv.foreground = gcv.background;
  gc2 = XCreateGC(dpy, window, GCForeground, &gcv);

  XSetWindowBackground(dpy, window,
		       get_pixel_resource("macsbug.borderColor",
					  "MacsBug.BorderColor",
					  dpy, xgwa.colormap));
  XClearWindow(dpy, window);

  char_width = (font->per_char
		? font->per_char['n'-font->min_char_or_byte2].width
		: font->min_bounds.width);
  line_height = font->ascent + font->descent + 1;

  col_right = char_width * 12;
  page_bottom = line_height * 47;

  if (page_bottom > xgwa.height) page_bottom = xgwa.height;

  row_bottom = page_bottom - line_height;
  row_top = row_bottom - (line_height * 4);
  page_right = col_right + (char_width * 88);
  body_top = row_top - (line_height * body_lines);

  page_bottom += 2;
  row_bottom += 2;
  body_top -= 4;

  xoff = (xgwa.width - page_right) / 2;
  yoff = (xgwa.height - page_bottom) / 2;
  if (xoff < 0) xoff = 0;
  if (yoff < 0) yoff = 0;

  XFillRectangle(dpy, window, gc2, xoff, yoff, page_right, page_bottom);

  draw_string(dpy, window, gc, &gcv, font, xoff, yoff, 10, 10, left, 0);
  draw_string(dpy, window, gc, &gcv, font, xoff+col_right, yoff+row_top,
	      10, 10, bottom, 0);

  XFillRectangle(dpy, window, gc, xoff + col_right, yoff, 2, page_bottom);
  XDrawLine(dpy, window, gc,
	    xoff+col_right, yoff+row_top, xoff+page_right, yoff+row_top);
  XDrawLine(dpy, window, gc,
	    xoff+col_right, yoff+row_bottom, xoff+page_right, yoff+row_bottom);
  XDrawRectangle(dpy, window, gc,  xoff, yoff, page_right, page_bottom);

  if (body_top > 4)
    body_top = 4;

  draw_string(dpy, window, gc, &gcv, font,
	      xoff + col_right + char_width, yoff + body_top, 10, 10, body,
	      500);

  while (delay > 0)
    {
      XDrawLine(dpy, window, gc,
		xoff+col_right+(char_width/2)+2, yoff+row_bottom+3,
		xoff+col_right+(char_width/2)+2, yoff+page_bottom-3);
      XSync(dpy, False);
      usleep(666666L);
      XDrawLine(dpy, window, gc2,
		xoff+col_right+(char_width/2)+2, yoff+row_bottom+3,
		xoff+col_right+(char_width/2)+2, yoff+page_bottom-3);
      XSync(dpy, False);
      usleep(333333L);
      if (bsod_sleep(dpy, 0))
	break;
      delay--;
    }

  XFreeGC(dpy, gc);
  XFreeGC(dpy, gc2);
  XClearWindow(dpy, window);
  XFreeFont(dpy, font);
}

static void
mac1 (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  GC gc;
  Pixmap pixmap = 0;
  int pix_w = macbomb_width;
  int pix_h = macbomb_height;

  XGetWindowAttributes (dpy, window, &xgwa);

  gcv.foreground = get_pixel_resource("mac1.foreground", "Mac.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource("mac1.background", "Mac.Background",
				      dpy, xgwa.colormap);
  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy, window);

  gc = XCreateGC(dpy, window, GCForeground|GCBackground, &gcv);

  pixmap = XCreatePixmapFromBitmapData(dpy, window, (char *) macbomb_bits,
				       macbomb_width, macbomb_height,
				       gcv.foreground,
				       gcv.background,
				       xgwa.depth);

  {
    int x = (xgwa.width - pix_w) / 2;
    int y = (xgwa.height - pix_h) / 2;
    if (y < 0) y = 0;
    XFillRectangle (dpy, window, gc, 0, 0, xgwa.width, xgwa.height);
    XSync(dpy, False);
    if (bsod_sleep(dpy, 1))
      goto DONE;
    XCopyArea(dpy, pixmap, window, gc, 0, 0, pix_w, pix_h, x, y);
  }

 DONE:
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, pixmap);
  XSync(dpy, False);
  bsod_sleep(dpy, delay);
  XClearWindow(dpy, window);
}


static void
macx (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xgwa;
  char *fontname = 0;
  const char *def_font = "fixed";
  XFontStruct *font;
  GC gc;

  const char *macx_panic =
   ("panic(cpu 0): Unable to find driver for this platform: "
    "\"PowerMac 3,5\".\n"
    "\n"
    "backtrace: 0x0008c2f4 0x0002a7a0 0x001f0204 0x001d4e4c 0x001d4c5c "
    "0x001a56cc 0x01d5dbc 0x001c621c 0x00037430 0x00037364\n"
    "\n"
    "\n"
    "\n"
    "No debugger configured - dumping debug information\n"
    "\n"
    "version string : Darwin Kernel Version 1.3:\n"
    "Thu Mar  1 06:56:40 PST 2001; root:xnu/xnu-123.5.obj~1/RELEASE_PPC\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "DBAT0: 00000000 00000000\n"
    "DBAT1: 00000000 00000000\n"
    "DBAT2: 80001FFE 8000003A\n"
    "DBAT3: 90001FFE 9000003A\n"
    "MSR=00001030\n"
    "backtrace: 0x0008c2f4 0x0002a7a0 0x001f0204 0x001d4e4c 0x001d4c5c "
    "0x001a56cc 0x01d5dbc 0x001c621c 0x00037430 0x00037364\n"
    "\n"
    "panic: We are hanging here...\n");

  XGetWindowAttributes (dpy, window, &xgwa);

  gcv.background = get_pixel_resource("macX.background",
                                      "MacX.Background",
				      dpy, xgwa.colormap);
  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy, window);

  fontname = get_string_resource ((xgwa.height > 900
				   ? "macX.font2"
				   : "macX.font"),
				  "MacX.Font");
  if (!fontname || !*fontname) fontname = (char *)def_font;
  font = XLoadQueryFont (dpy, fontname);
  if (!font) font = XLoadQueryFont (dpy, def_font);
  if (!font) exit(-1);
  if (fontname && fontname != def_font)
    free (fontname);

  gcv.font = font->fid;
  gcv.foreground = get_pixel_resource("macsbug.foreground",
				      "MacsBug.Foreground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource("macsbug.background",
				      "MacsBug.Background",
				      dpy, xgwa.colormap);


  gcv.foreground = get_pixel_resource("macX.textForeground",
                                      "MacX.TextForeground",
				      dpy, xgwa.colormap);
  gcv.background = get_pixel_resource("macX.textBackground",
                                      "MacX.TextBackground",
				      dpy, xgwa.colormap);
  gc = XCreateGC(dpy, window, GCForeground|GCBackground|GCFont, &gcv);

#if defined(HAVE_GDK_PIXBUF) || defined (HAVE_XPM)
  {
    Pixmap pixmap = 0;
    Pixmap mask = 0;
    int x, y, pix_w, pix_h;
    pixmap = xpm_data_to_pixmap (dpy, window, (char **) happy_mac,
                                 &pix_w, &pix_h, &mask);

    x = (xgwa.width - pix_w) / 2;
    y = (xgwa.height - pix_h) / 2;
    if (y < 0) y = 0;
    XSync(dpy, False);
    bsod_sleep(dpy, 2);
    XSetClipMask (dpy, gc, mask);
    XSetClipOrigin (dpy, gc, x, y);
    XCopyArea (dpy, pixmap, window, gc, 0, 0, pix_w, pix_h, x, y);
    XSetClipMask (dpy, gc, None);
    XFreePixmap (dpy, pixmap);
  }
#endif /* HAVE_GDK_PIXBUF || HAVE_XPM */

  bsod_sleep(dpy, 3);

  {
    const char *s;
    int x = 0, y = 0;
    int char_width, line_height;
    char_width = (font->per_char
                  ? font->per_char['n'-font->min_char_or_byte2].width
                  : font->min_bounds.width);
    line_height = font->ascent + font->descent;

    s = macx_panic;
    y = font->ascent;
    while (*s)
      {
        int ox = x;
        int oy = y;
        if (*s == '\n' || x + char_width >= xgwa.width)
          {
            x = 0;
            y += line_height;
          }

        if (*s == '\n')
          {
            /* Note that to get this goofy effect, they must be actually
               emitting LF CR at the end of each line instead of CR LF!
             */
            XDrawImageString (dpy, window, gc, ox, oy, " ", 1);
            XDrawImageString (dpy, window, gc, ox, y, " ", 1);
          }
        else
          {
            XDrawImageString (dpy, window, gc, x, y, s, 1);
            x += char_width;
          }
        s++;
      }
  }

  XFreeGC(dpy, gc);
  XSync(dpy, False);
  bsod_sleep(dpy, delay);
  XClearWindow(dpy, window);
}


/* blit damage
 *
 * by Martin Pool <mbp@samba.org>, Feb 2000.
 *
 * This is meant to look like the preferred failure mode of NCD
 * Xterms.  The parameters for choosing what to copy where might not
 * be quite right, but it looks about ugly enough.
 */
static void
blitdamage (Display *dpy, Window window, int delay)
{
  XGCValues gcv;
  XWindowAttributes xwa;
  GC gc0;
  int i;
  int delta_x = 0, delta_y = 0;
  int w, h;
  int chunk_h, chunk_w;
  int steps;
  long gc_mask = 0;
  int src_x, src_y;
  int x, y;
  
  XGetWindowAttributes(dpy, window, &xwa);

  load_random_image (xwa.screen, window, window);

  w = xwa.width;
  h = xwa.height;

  gc_mask = GCForeground;
  
  gcv.plane_mask = random();
  gc_mask |= GCPlaneMask;
  
  gc0 = XCreateGC(dpy, window, gc_mask, &gcv);

  steps = 50;
  chunk_w = w / (random() % 1 + 1);
  chunk_h = h / (random() % 1 + 1);
  if (random() & 0x1000) 
    delta_y = random() % 600;
  if (!delta_y || (random() & 0x2000))
    delta_x = random() % 600;
  src_x = 0; 
  src_y = 0; 
  x = 0;
  y = 0;
  
  for (i = 0; i < steps; i++) {
    if (x + chunk_w > w) 
      x -= w;
    else
      x += delta_x;
    
    if (y + chunk_h > h)
      y -= h;
    else
      y += delta_y;
    
    XCopyArea(dpy, window, window, gc0,
	      src_x, src_y, 
	      chunk_w, chunk_h,
	      x, y);

    if (bsod_sleep(dpy, 0))
      goto DONE;
  }

  bsod_sleep(dpy, delay);

 DONE:
  XFreeGC(dpy, gc0);
}


/*
 * SPARC Solaris panic. Should look pretty authentic on Solaris boxes.
 * Anton Solovyev <solovam@earthlink.net>
 */ 

typedef struct
{
  Display *dpy;
  Window window;
  GC gc, erase_gc;
  XFontStruct *xfs;
  int sub_width;                /* Text subwindow width in pixels */
  int sub_height;               /* Text subwindow height in pixels */
  int sub_x;                    /* upper left corner of the text subwindow */
  int sub_y;                    /* upper left corner of the text subwindow */
  int char_width;               /* Char width in pixels */
  int line_height;              /* Line height in pixels */
  int columns;                  /* Number of columns in the text screen */
  int x;                        /* horizontal position of the cursor */
} scrolling_window;


static scrolling_window *
make_scrolling_window (Display *dpy, Window window,
                       const char *name,
                       Bool grab_screen_p)
{
  const char *def_font = "fixed";
  scrolling_window* ts;
  XWindowAttributes xgwa;
  XGCValues gcv;
  char* fn;
  char buf1[100], buf2[100];

  ts = malloc (sizeof (*ts));
  ts->window = window;
  ts->dpy = dpy;

  ts->x = 0;

  XGetWindowAttributes (dpy, window, &xgwa);

  if (grab_screen_p)
    {
      ts->sub_width = xgwa.width * 0.8;
      ts->sub_height = xgwa.height * 0.8;
    }
  else
    {
      ts->sub_width  = xgwa.width - 20;
      ts->sub_height = xgwa.height - 20;
      if (ts->sub_width  < 20) ts->sub_width  = 20;
      if (ts->sub_height < 20) ts->sub_height = 20;
    }

  sprintf (buf1, "%.50s.font", name);
  sprintf (buf2, "%.50s.Font", name);
  fn = get_string_resource (buf1, buf2);
  ts->xfs = XLoadQueryFont (dpy, fn);
  if (!ts->xfs)
    {
      sprintf (buf1, "%.50s.font2", name);
      fn = get_string_resource(buf1, buf2);
      ts->xfs = XLoadQueryFont(dpy, fn);
    }
  if (!ts->xfs)
    ts->xfs = XLoadQueryFont(dpy, def_font);
  if (!ts->xfs)
    exit (1);
  gcv.font = ts->xfs->fid;
  ts->char_width = (ts->xfs->per_char
                    ? ts->xfs->per_char['n'-ts->xfs->min_char_or_byte2].width
                    : ts->xfs->min_bounds.width);
  ts->line_height = ts->xfs->ascent + ts->xfs->descent + 1;

  ts->columns = ts->sub_width / ts->char_width;

  ts->sub_x = (xgwa.width - ts->sub_width) / 2;
  ts->sub_y = (xgwa.height - ts->sub_height) / 2;

  if (!grab_screen_p) ts->sub_height += ts->sub_y, ts->sub_y = 0;

  if (grab_screen_p)
    load_random_image (xgwa.screen, window, window);

  sprintf (buf1, "%.50s.background", name);
  sprintf (buf2, "%.50s.Background", name);
  gcv.background = get_pixel_resource (buf1, buf2, dpy, xgwa.colormap);

  sprintf (buf1, "%.50s.foreground", name);
  sprintf (buf2, "%.50s.Foreground", name);
  gcv.foreground = get_pixel_resource (buf1, buf2, dpy, xgwa.colormap);

  ts->gc = XCreateGC (dpy, window,
                      GCForeground | GCBackground | GCFont,
                      &gcv);
  gcv.foreground = gcv.background;
  ts->erase_gc = XCreateGC (dpy, window,
                            GCForeground | GCBackground,
                            &gcv);
  XSetWindowBackground (dpy, window, gcv.background);
  return(ts);
}

static void
free_scrolling_window (scrolling_window* ts)
{
  XFreeGC (ts->dpy, ts->gc);
  XFreeGC (ts->dpy, ts->erase_gc);
  XFreeFont (ts->dpy, ts->xfs);
  free (ts);
}

static void
scrolling_putc (scrolling_window* ts, const char aChar)
{
  switch (aChar)
    {
    case '\n':
      ts->x = 0;
      XCopyArea (ts->dpy, ts->window, ts->window, ts->gc,
                 ts->sub_x, ts->sub_y + ts->line_height,
                 ts->sub_width, ts->sub_height,
                 ts->sub_x, ts->sub_y);
      XFillRectangle (ts->dpy, ts->window, ts->erase_gc,
                      ts->sub_x, ts->sub_y + ts->sub_height - ts->line_height,
                      ts->sub_width, ts->line_height);
      break;
    case '\r':
      ts->x = 0;
    case '\b':
      if(ts->x > 0)
        ts->x--;
      break;
    default:
      if (ts->x >= ts->columns)
        scrolling_putc (ts, '\n');
      XDrawImageString (ts->dpy, ts->window, ts->gc,
                        (ts->sub_x +
                         (ts->x * ts->char_width)
                         - ts->xfs->min_bounds.lbearing),
                        (ts->sub_y + ts->sub_height - ts->xfs->descent),
                        &aChar, 1);
      ts->x++;
      break;
    }
}

static Bool
scrolling_puts (scrolling_window *ts, const char* aString, int delay)
{
  const char *c;
  for (c = aString; *c; ++c)
    {
      scrolling_putc (ts, *c);
      if (delay)
        {
          XSync(ts->dpy, 0);
          usleep(delay);
          if (bsod_sleep (ts->dpy, 0))
            return True;
        }
    }
  XSync (ts->dpy, 0);
  return False;
}

static void
sparc_solaris (Display* dpy, Window window, int delay)
{
  const char *msg1 =
    "BAD TRAP: cpu=0 type=0x31 rp=0x2a10043b5e0 addr=0xf3880 mmu_fsr=0x0\n"
    "BAD TRAP occured in module \"unix\" due to an illegal access to a"
    " user address.\n"
    "adb: trap type = 0x31\n"
    "addr=0xf3880\n"
    "pid=307, pc=0x100306e4, sp=0x2a10043ae81, tstate=0x4480001602,"
    " context=0x87f\n"
    "g1-g7: 1045b000, 32f, 10079440, 180, 300000ebde8, 0, 30000953a20\n"
    "Begin traceback... sp = 2a10043ae81\n"
    "Called from 100bd060, fp=2a10043af31, args=f3700 300008cc988 f3880 0"
    " 1 300000ebde0.\n"
    "Called from 101fe1bc, fp=2a10043b011, args=3000045a240 104465a0"
    " 300008e47d0 300008e48fa 300008ae350 300008ae410\n"
    "Called from 1007c520, fp=2a10043b0c1, args=300008e4878 300003596e8 0"
    " 3000045a320 0 3000045a220\n"
    "Called from 1007c498, fp=2a10043b171, args=1045a000 300007847f0 20"
    " 3000045a240 1 0\n"
    "Called from 1007972c, fp=2a10043b221, args=1 300009517c0 30000951e58 1"
    " 300007847f0 0\n"
    "Called from 10031e10, fp=2a10043b2d1, args=3000095b0c8 0 300009396a8"
    " 30000953a20 0 1\n"
    "Called from 10000bdd8, fp=ffffffff7ffff1c1, args=0 57 100131480"
    " 100131480 10012a6e0 0\n"
    "End traceback...\n"
    "panic[cpu0]/thread=30000953a20: trap\n"
    "syncing file systems...";
  const char *msg2 =
    " 1 done\n"
    "dumping to /dev/dsk/c0t0d0s3, offset 26935296\n";
  const char *msg3 =
    ": 2803 pages dumped, compression ratio 2.88, dump succeeded\n";
  const char *msg4 =
    "rebooting...\n"
    "Resetting ...";

  scrolling_window *ts;
  int i;
  char buf[256];

  ts = make_scrolling_window (dpy, window, "Solaris", True);

  scrolling_puts (ts, msg1, 0);
  if (bsod_sleep (dpy, 3))
    goto DONE;

  scrolling_puts (ts, msg2, 0);
  if (bsod_sleep (dpy, 2))
    goto DONE;

  for (i = 1; i <= 100; ++i)
    {
      sprintf(buf, "\b\b\b\b\b\b\b\b\b\b\b%3d%% done", i);
      scrolling_puts (ts, buf, 0);
      if (bsod_sleep (dpy, -1))
        goto DONE;
    }

  scrolling_puts (ts, msg3, 0);
  if (bsod_sleep (dpy, 2))
    goto DONE;

  scrolling_puts (ts, msg4, 0);
  if (bsod_sleep(dpy, 3))
    goto DONE;

  bsod_sleep (dpy, 3);

 DONE:
  free_scrolling_window (ts);
}

/* Linux panic and fsck, by jwz
 */
static void
linux_fsck (Display *dpy, Window window, int delay)
{
  XWindowAttributes xgwa;
  scrolling_window *ts;
  int i;
  const char *sysname;
  char buf[1024];

  const char *linux_panic[] = {
   " kernel: Unable to handle kernel paging request at virtual "
     "address 0000f0ad\n",
   " kernel:  printing eip:\n",
   " kernel: c01becd7\n",
   " kernel: *pde = 00000000\n",
   " kernel: Oops: 0000\n",
   " kernel: CPU:    0\n",
   " kernel: EIP:    0010:[<c01becd7>]    Tainted: P \n",
   " kernel: EFLAGS: 00010286\n",
   " kernel: eax: 0000ff00   ebx: ca6b7e00   ecx: ce1d7a60   edx: ce1d7a60\n",
   " kernel: esi: ca6b7ebc   edi: 00030000   ebp: d3655ca0   esp: ca6b7e5c\n",
   " kernel: ds: 0018   es: 0018   ss: 0018\n",
   " kernel: Process crond (pid: 1189, stackpage=ca6b7000)\n",
   " kernel: Stack: d3655ca0 ca6b7ebc 00030054 ca6b7e7c c01c1e5b "
       "00000287 00000020 c01c1fbf \n",
   "",
   " kernel:        00005a36 000000dc 000001f4 00000000 00000000 "
       "ce046d40 00000001 00000000 \n",
   "", "", "",
   " kernel:        ffffffff d3655ca0 d3655b80 00030054 c01bef93 "
       "d3655ca0 ca6b7ebc 00030054 \n",
   "", "", "",
   " kernel: Call Trace:    [<c01c1e5b>] [<c01c1fbf>] [<c01bef93>] "
       "[<c01bf02b>] [<c0134c4f>]\n",
   "", "", "",
   " kernel:   [<c0142562>] [<c0114f8c>] [<c0134de3>] [<c010891b>]\n",
   " kernel: \n",
   " kernel: Code: 2a 00 75 08 8b 44 24 2c 85 c0 74 0c 8b 44 24 58 83 48 18 "
      "08 \n",
   0
  };

  XGetWindowAttributes (dpy, window, &xgwa);
  XSetWindowBackground (dpy, window, 
                        get_pixel_resource("Linux.background",
                                           "Linux.Background",
                                           dpy, xgwa.colormap));
  XClearWindow(dpy, window);

  sysname = "linux";
# ifdef HAVE_UNAME
  {
    struct utsname uts;
    char *s;
    if (uname (&uts) >= 0)
      sysname = uts.nodename;
    s = strchr (sysname, '.');
    if (s) *s = 0;
  }
# endif	/* !HAVE_UNAME */


  ts = make_scrolling_window (dpy, window, "Linux", False);

  scrolling_puts (ts, "waiting for X server to shut down ", 0);
  usleep (100000);
  if (bsod_sleep (dpy, 0))
    goto PANIC;
  scrolling_puts (ts,
                  "XIO:  fatal IO error 2 (broken pipe) on X server \":0.0\"\n"
                  "        after 339471 requests (339471 known processed) "
                  "with 0 events remaining\n",
                  0);
  if (scrolling_puts (ts, ".........\n", 300000))
    goto PANIC;
  if (bsod_sleep (dpy, 0))
    goto PANIC;
  scrolling_puts (ts,
                  "xinit:  X server slow to shut down, sending KILL signal.\n",
                  0);
  scrolling_puts (ts, "waiting for server to die ", 0);
  if (scrolling_puts (ts, "...\n", 300000))
    goto PANIC;
  if (bsod_sleep (dpy, 0))
    goto PANIC;
  scrolling_puts (ts, "xinit:  Can't kill server\n", 0);

  if (bsod_sleep (dpy, 2))
    goto PANIC;

  sprintf (buf, "\n%s Login: ", sysname);
  scrolling_puts (ts, buf, 0);
  if (bsod_sleep (dpy, 1))
    goto PANIC;
  scrolling_puts (ts,
    "\n\n"
    "Parallelizing fsck version 1.22 (22-Jun-2001)\n"
    "e2fsck 1.22, 22-Jun-2001 for EXT2 FS 0.5b, 95/08/09\n"
    "Warning!  /dev/hda1 is mounted.\n"
    "/dev/hda1 contains a file system with errors, check forced.\n",
                0);
  if (bsod_sleep (dpy, 1))
    goto PANIC;

  if (0 == random() % 2)
  scrolling_puts (ts,
     "Couldn't find ext2 superblock, trying backup blocks...\n"
     "The filesystem size (according to the superblock) is 3644739 blocks\n"
     "The physical size of the device is 3636706 blocks\n"
     "Either the superblock or the partition table is likely to be corrupt!\n"
     "Abort<y>? no\n",
                0);
  if (bsod_sleep (dpy, 1))
    goto PANIC;

 AGAIN:

  scrolling_puts (ts, "Pass 1: Checking inodes, blocks, and sizes\n", 0);
  if (bsod_sleep (dpy, 2))
    goto PANIC;

  i = (random() % 60) - 20;
  while (--i > 0)
    {
      int b = random() % 0xFFFF;
      sprintf (buf, "Deleted inode %d has zero dtime.  Fix<y>? yes\n\n", b);
      scrolling_puts (ts, buf, 0);
    }

  i = (random() % 40) - 10;
  if (i > 0)
    {
      int g = random() % 0xFFFF;
      int b = random() % 0xFFFFFFF;

      if (bsod_sleep (dpy, 1))
        goto PANIC;

      sprintf (buf, "Warning: Group %d's copy of the group descriptors "
               "has a bad block (%d).\n", g, b);
      scrolling_puts (ts, buf, 0);

      b = random() % 0x3FFFFF;
      while (--i > 0)
        {
          b += random() % 0xFFFF;
          sprintf (buf,
                   "Error reading block %d (Attempt to read block "
                   "from filesystem resulted in short read) while doing "
                   "inode scan.  Ignore error<y>?",
                   b);
          scrolling_puts (ts, buf, 0);
          usleep (10000);
          scrolling_puts (ts, " yes\n\n", 0);
        }
    }

  if (0 == (random() % 10))
    {

      if (bsod_sleep (dpy, 1))
        goto PANIC;

      i = 3 + (random() % 10);
      while (--i > 0)
        scrolling_puts (ts, "Could not allocate 256 block(s) for inode table: "
                        "No space left on device\n", 0);
      scrolling_puts (ts, "Restarting e2fsck from the beginning...\n", 0);

      if (bsod_sleep (dpy, 2))
        goto PANIC;

      goto AGAIN;
    }

  i = (random() % 20) - 5;

  if (i > 0)
    if (bsod_sleep (dpy, 1))
      goto PANIC;

  while (--i > 0)
    {
      int j = 5 + (random() % 10);
      int w = random() % 4;

      while (--j > 0)
        {
          int b = random() % 0xFFFFF;
          int g = random() % 0xFFF;

          if (0 == (random() % 10))
            b = 0;
          else if (0 == (random() % 10))
            b = -1;

          if (w == 0)
            sprintf (buf,
                     "Inode table for group %d not in group.  (block %d)\n"
                     "WARNING: SEVERE DATA LOSS POSSIBLE.\n"
                     "Relocate<y>?",
                     g, b);
          else if (w == 1)
            sprintf (buf,
                     "Block bitmap for group %d not in group.  (block %d)\n"
                     "Relocate<y>?",
                     g, b);
          else if (w == 2)
            sprintf (buf,
                     "Inode bitmap %d for group %d not in group.\n"
                     "Continue<y>?",
                     b, g);
          else /* if (w == 3) */
            sprintf (buf,
                     "Bad block %d in group %d's inode table.\n"
                     "WARNING: SEVERE DATA LOSS POSSIBLE.\n"
                     "Relocate<y>?",
                     b, g);

          scrolling_puts (ts, buf, 0);
          scrolling_puts (ts, " yes\n\n", 0);
        }
      if (bsod_sleep (dpy, 0))
        goto PANIC;
      usleep (1000);
    }


  if (0 == random() % 10) goto PANIC;
  scrolling_puts (ts, "Pass 2: Checking directory structure\n", 0);
  if (bsod_sleep (dpy, 2))
    goto PANIC;

  i = (random() % 20) - 5;
  while (--i > 0)
    {
      int n = random() % 0xFFFFF;
      int o = random() % 0xFFF;
      sprintf (buf, "Directory inode %d, block 0, offset %d: "
               "directory corrupted\n"
               "Salvage<y>? ",
               n, o);
      scrolling_puts (ts, buf, 0);
      usleep (1000);
      scrolling_puts (ts, " yes\n\n", 0);

      if (0 == (random() % 100))
        {
          sprintf (buf, "Missing '.' in directory inode %d.\nFix<y>?", n);
          scrolling_puts (ts, buf, 0);
          usleep (1000);
          scrolling_puts (ts, " yes\n\n", 0);
        }

      if (bsod_sleep (dpy, 0))
        goto PANIC;
    }

  if (0 == random() % 10) goto PANIC;
  scrolling_puts (ts,
                  "Pass 3: Checking directory connectivity\n"
                  "/lost+found not found.  Create? yes\n",
                0);
  if (bsod_sleep (dpy, 2))
    goto PANIC;

  /* Unconnected directory inode 4949 (/var/spool/squid/06/???)
     Connect to /lost+found<y>? yes

     '..' in /var/spool/squid/06/08 (20351) is <The NULL inode> (0), should be 
     /var/spool/squid/06 (20350).
     Fix<y>? yes

     Unconnected directory inode 128337 (/var/spool/squid/06/???)
     Connect to /lost+found<y>? yes
   */


  if (0 == random() % 10) goto PANIC;
  scrolling_puts (ts, "Pass 4: Checking reference counts\n", 0);
  if (bsod_sleep (dpy, 2))
    goto PANIC;

  /* Inode 2 ref count is 19, should be 20.  Fix<y>? yes

     Inode 4949 ref count is 3, should be 2.  Fix<y>? yes

        ...

     Inode 128336 ref count is 3, should be 2.  Fix<y>? yes

     Inode 128337 ref count is 3, should be 2.  Fix<y>? yes

   */


  if (0 == random() % 10) goto PANIC;
  scrolling_puts (ts, "Pass 5: Checking group summary information\n", 0);
  if (bsod_sleep (dpy, 2))
    goto PANIC;

  i = (random() % 200) - 50;
  if (i > 0)
    {
      scrolling_puts (ts, "Block bitmap differences: ", 0);
      while (--i > 0)
        {
          sprintf (buf, " %d", -(random() % 0xFFF));
          scrolling_puts (ts, buf, 0);
          usleep (1000);
        }
      scrolling_puts (ts, "\nFix? yes\n\n", 0);
    }


  i = (random() % 100) - 50;
  if (i > 0)
    {
      scrolling_puts (ts, "Inode bitmap differences: ", 0);
      while (--i > 0)
        {
          sprintf (buf, " %d", -(random() % 0xFFF));
          scrolling_puts (ts, buf, 0);
          usleep (1000);
        }
      scrolling_puts (ts, "\nFix? yes\n\n", 0);
    }

  i = (random() % 20) - 5;
  while (--i > 0)
    {
      int g = random() % 0xFFFF;
      int c = random() % 0xFFFF;
      sprintf (buf,
               "Free blocks count wrong for group #0 (%d, counted=%d).\nFix? ",
               g, c);
      scrolling_puts (ts, buf, 0);
      usleep (1000);
      scrolling_puts (ts, " yes\n\n", 0);
      if (bsod_sleep (dpy, 0))
        goto PANIC;
    }

 PANIC:

  i = 0;
  scrolling_puts (ts, "\n\n", 0);
  while (linux_panic[i])
    {
      time_t t = time ((time_t *) 0);
      struct tm *tm = localtime (&t);
      char prefix[100];

      if (*linux_panic[i])
        {
          strftime (prefix, sizeof(prefix)-1, "%b %d %k:%M:%S ", tm);
          scrolling_puts (ts, prefix, 0);
          scrolling_puts (ts, sysname, 0);
          scrolling_puts (ts, linux_panic[i], 0);
          XSync(dpy, False);
          usleep(1000);
        }
      else
        usleep (300000);

      if (bsod_sleep (dpy, 0))
        goto DONE;
      i++;
    }

  if (bsod_sleep (dpy, 4))
    goto DONE;


  XSync(dpy, False);
  bsod_sleep(dpy, delay);

 DONE:
  free_scrolling_window (ts);
  XClearWindow(dpy, window);
}



/* HPUX panic, by Tobias Klausmann <klausman@schwarzvogel.de>
 */

static void
hpux (Display* dpy, Window window, int delay)
{
  XWindowAttributes xgwa;
  scrolling_window *ts;
  const char *sysname;
  char buf[2048];

  const char *msg1 =
   "Console Login:\n"
   "\n"
   "     ******* Unexpected HPMC/TOC. Processor HPA FFFFFFFF'"
   "FFFA0000 *******\n"
   "                              GENERAL REGISTERS:\n"
   "r00/03 00000000'00000000 00000000'00000000 00000000'00000000 00000000'"
   "006C76C0\n"
   "r04/07 00000000'00000001 00000000'0126E328 00000000'00000000 00000000'"
   "0122B640\n"
   "r08/11 00000000'00000000 00000000'0198CFC0 00000000'000476FE 00000000'"
   "00000001\n"
   "r12/15 00000000'40013EE8 00000000'08000080 00000000'4002530C 00000000'"
   "4002530C\n"
   "r16/19 00000000'7F7F2A00 00000000'00000001 00000000'00000000 00000000'"
   "00000000\n"
   "r20/23 00000000'006C8048 00000000'00000001 00000000'00000000 00000000'"
   "00000000\n"
   "r24/27 00000000'00000000 00000000'00000000 00000000'00000000 00000000'"
   "00744378\n"
   "r28/31 00000000'00000000 00000000'007DD628 00000000'0199F2B0 00000000'"
   "00000000\n"
   "                              CONTROL REGISTERS:\n"
   "sr0/3  00000000'0F3B4000 00000000'0C2A2000 00000000'016FF800 00000000'"
   "00000000\n"
   "sr4/7  00000000'00000000 00000000'016FF800 00000000'0DBF1400 00000000'"
   "00000000\n"
   "pcq =  00000000'00000000.00000000'00104950 00000000'00000000.00000000'"
   "00104A14\n"
   "isr =  00000000'10240006 ior = 00000000'67D9E220 iir = 08000240 rctr = "
   "7FF10BB6\n"
   "\n"
   "pid reg cr8/cr9    00007700'0000B3A9 00000000'0000C5D8\n"
   "pid reg cr12/cr13  00000000'00000000 00000000'00000000\n"
   "ipsw = 000000FF'080CFF1F iva = 00000000'0002C000 sar = 3A ccr = C0\n"
   "tr0/3  00000000'006C76C0 00000000'00000001 00000000'00000000 00000000'"
   "7F7CE000\n"
   "tr4/7  00000000'03790000 0000000C'4FB68340 00000000'C07EE13F 00000000'"
   "0199F2B0\n"
   "eiem = FFFFFFF0'FFFFFFFF eirr = 80000000'00000000 itmr = 0000000C'"
   "4FD8EDE1\n"
   "cr1/4  00000000'00000000 00000000'00000000 00000000'00000000 00000000'"
   "00000000\n"
   "cr5/7  00000000'00000000 00000000'00000000 00000000'"
   "00000000\n"
   "                           MACHINE CHECK PARAMETERS:\n"
   "Check Type = 00000000 CPU STATE = 9E000001 Cache Check = 00000000\n"
   "TLB Check = 00000000 Bus Check = 00000000 PIM State = ? SIU "
   "Status = ????????\n"
   "Assists = 00000000 Processor = 00000000\n"
   "Slave Addr = 00000000'00000000 Master Addr = 00000000'00000000\n"
   "\n"
   "\n"
   "TOC,    pcsq.pcoq = 0'0.0'104950   , isr.ior = 0'10240006.0'67d9e220\n"
   "@(#)B2352B/9245XB HP-UX (B.11.00) #1: Wed Nov  5 22:38:19 PST 1997\n"
   "Transfer of control: (display==0xd904, flags==0x0)\n"
   "\n"
   "\n"
   "\n"
   "*** A system crash has occurred.  (See the above messages for details.)\n"
   "*** The system is now preparing to dump physical memory to disk, for use\n"
   "*** in debugging the crash.\n"
   "\n"
   "*** The dump will be a SELECTIVE dump:  40 of 256 megabytes.\n"
   "*** To change this dump type, press any key within 10 seconds.\n"
   "*** Proceeding with selective dump.\n"
   "\n"
   "*** The dump may be aborted at any time by pressing ESC.\n";
  const char *msg2 =
   "\n*** System rebooting.\n";

  XGetWindowAttributes (dpy, window, &xgwa);
  ts = make_scrolling_window (dpy, window, "HPUX", False);
  XClearWindow(dpy,window);
  ts->columns = 10000;  /* never wrap */
  ts->sub_x = 0;
  ts->sub_y = 0;
  ts->sub_width = xgwa.width;
  ts->sub_height = xgwa.height;

  sysname = "HPUX";
# ifdef HAVE_UNAME
  {
    struct utsname uts;
    char *s;
    if (uname (&uts) >= 0)
      sysname = uts.nodename;
    s = strchr (sysname, '.');
    if (s) *s = 0;
  }
# endif	/* !HAVE_UNAME */

  if (bsod_sleep (dpy, 1))
    goto DONE;
  
  scrolling_puts (ts,
                  "                                                       "
                  "                                                       "
                  "                                                       \n",
                  0);
  sprintf (buf, "%.100s [HP Release B.11.00] (see /etc/issue)\n", sysname);
  scrolling_puts (ts, buf, 0);
  if (bsod_sleep (dpy, 1))
    goto DONE;
  scrolling_puts (ts, msg1, 0);
  {
    int i;
    int steps = 11;
    int size = 40;
    for (i = 0; i <= steps; i++)
      {
        if (i > steps) i = steps;
        sprintf (buf, 
               "*** Dumping: %3d%% complete (%d of 40 MB) (device 64:0x2)\r",
                 i * 100 / steps,
                 i * size / steps);
        scrolling_puts (ts, buf, 0);
        XSync (dpy, False);
        usleep (1500000);
        if (bsod_sleep (dpy, 0))
          goto DONE;
      }
  }

  scrolling_puts (ts, msg2, 0);

  XSync(dpy, False);
  bsod_sleep(dpy, delay);

 DONE:
  free_scrolling_window (ts);
}



/* IBM OS/390 aka MVS aka z/OS.
   Text from Dan Espen <dane@mk.telcordia.com>.
   Apparently this isn't actually a crash, just a random session...
   But who can tell.
 */

static void
os390 (Display* dpy, Window window, int delay)
{
  GC gc;
  XGCValues gcv;
  XWindowAttributes xgwa;
  scrolling_window *ts;
  int i;

  const char *msg[] = {
   "* ISPF Subtask abend *\n",
   "SPF      ENDED DUE TO ERROR+\n",
   "READY\n",
   "\n",
   "IEA995I SYMPTOM DUMP OUTPUT\n",
   "  USER COMPLETION CODE=0222\n",
   " TIME=23.00.51  SEQ=03210  CPU=0000  ASID=00AE\n",
   " PSW AT TIME OF ERROR  078D1000   859DAF18  ILC 2  INTC 0D\n",
   "   NO ACTIVE MODULE FOUND\n",
   "   NAME=UNKNOWN\n",
   "   DATA AT PSW  059DAF12 - 00181610  0A0D9180  70644710\n",
   "   AR/GR 0: 00000000/80000000   1: 00000000/800000DE\n",
   "         2: 00000000/196504DC   3: 00000000/00037A78\n",
   "         4: 00000000/00037B78   5: 00000000/0003351C\n",
   "         6: 00000000/0000F0AD   7: 00000000/00012000\n",
   "         8: 00000000/059DAF10   9: 00000000/0002D098\n",
   "         A: 00000000/059D9F10   B: 00000000/059D8F10\n",
   "         C: 00000000/859D7F10   D: 00000000/00032D60\n",
   "         E: 00000000/00033005   F: 01000002/00000041\n",
   " END OF SYMPTOM DUMP\n",
   "ISPS014 - ** Logical screen request failed - abend 0000DE **\n",
   "ISPS015 - ** Contact your system programmer or dialog developer.**\n",
   "*** ISPF Main task abend ***\n",
   "IEA995I SYMPTOM DUMP OUTPUT\n",
   "  USER COMPLETION CODE=0222\n",
   " TIME=23.00.52  SEQ=03211  CPU=0000  ASID=00AE\n",
   " PSW AT TIME OF ERROR  078D1000   8585713C  ILC 2  INTC 0D\n",
   "   ACTIVE LOAD MODULE           ADDRESS=05855000  OFFSET=0000213C\n",
   "   NAME=ISPMAIN\n",
   "   DATA AT PSW  05857136 - 00181610  0A0D9180  D3304770\n",
   "   GR 0: 80000000   1: 800000DE\n",
   "      2: 00015260   3: 00000038\n",
   "      4: 00012508   5: 00000000\n",
   "      6: 000173AC   7: FFFFFFF8\n",
   "      8: 05858000   9: 00012CA0\n",
   "      A: 05857000   B: 05856000\n",
   "      C: 85855000   D: 00017020\n",
   "      E: 85857104   F: 00000000\n",
   " END OF SYMPTOM DUMP\n",
   "READY\n",
   "***_\n"
  };

  XGetWindowAttributes (dpy, window, &xgwa);
  ts = make_scrolling_window (dpy, window, "OS390", False);
  ts->columns = 10000;  /* never wrap */
  ts->sub_x = 0;
  ts->sub_y = 0;
  ts->sub_width = xgwa.width;
  ts->sub_height = xgwa.height;

  gcv.foreground = get_pixel_resource ("390.background", "390.Background",
                                       dpy, xgwa.colormap);
  gc = XCreateGC (dpy, window, GCForeground, &gcv);
  XFillRectangle (dpy, window, gc, 0, 0, xgwa.width, xgwa.height);
  XFreeGC (dpy, gc);

  for (i = 0; i < countof (msg); i++)
    {
      scrolling_puts (ts, msg[i], 0);
      usleep (100000);
      if (bsod_sleep(dpy, 0)) goto DONE;
    }

  XSync(dpy, False);
  bsod_sleep(dpy, delay);
DONE:
  free_scrolling_window (ts);
}



/*
 * Simulate various Apple II crashes. The memory map encouraged many
 * programs to use the primary hi-res video page for various storage,
 * and the secondary hi-res page for active display. When it crashed
 * into Applesoft or the monitor, it would revert to the primary page
 * and you'd see memory garbage on the screen. Also, it was common for
 * copy-protected games to use the primary text page for important
 * code, because that made it really hard to reverse-engineer them. The
 * result often looked like what this generates.
 *
 * Sometimes an imaginary user types some of the standard commands to
 * recover from crashes.  You can turn off BSOD*apple2SimulateUser to
 * prevent this.
 *
 * It simulates the following characteristics of standard television
 * monitors:
 *
 * - Realistic rendering of a composite video signal
 * - Compression & brightening on the right, as the scan gets truncated
 *   because of saturation in the flyback transformer
 * - Blooming of the picture dependent on brightness
 * - Overscan, cutting off a few pixels on the left side.
 * - Colored text in mixed graphics/text modes
 *
 * It's amazing how much it makes your high-end monitor look like at
 * large late-70s TV.  All you need is to put a big "Solid State" logo
 * in curly script on it and you'd be set.
 *
 * Trevor Blackwell <tlb@tlb.org> 
 */

/*
 * Implementation notes:
 *
 * There are roughly 3 parts to this hack:
 *
 * - emulation of A2 Basic and Monitor. Not much more than printing random
 *   plausible messages. Here we work in the A2 memory space.
 *
 * - emulation of the A2's video output section, which shifted bits out of main
 *   memory at a 14 MHz dot clock rate, sort of. You could only read one byte
 *   per MHz, so there were various schemes for turning 8 bits into 14 screen
 *   pixels.
 *
 * - simulation of an NTSC television, which turned the bits into colored
 *   graphics and text.
 * 
 * The A2 had 3 display modes: text, lores, and hires. Text was 40x24, and it
 * disabled color in the TV. Lores gave you 40x48 graphics blocks, using the
 * same memory as the text screen. Each could be one of 16 colors. Hires gave
 * you 280x192 pixels. Odd pixels were blue or purple, and even pixels were
 * orange or green depending on the setting of the high bit in each byte.
 *
 * The graphics modes could also have 4 lines of text at the bottom. This was
 * fairly unreadable if you had a color monitor.
 *
 * Each mode had 2 different screens using different memory space. In hires
 * mode this was sometimes used for double buffering, but more often the lower
 * screen was full of code/data and the upper screen was used for display, so
 * you got random garbage on the screen.
 * 
 * In DirectColor or TrueColor modes, it generates pixel values directly from
 * RGB values it calculates across each scan line. In PseudoColor mode, it
 * consider each possible pattern of 5 preceding bit values in each possible
 * position modulo 4 and allocates a color for each. A few things, like the
 * brightening on the right side as the horizontal trace slows down, aren't
 * done in PseudoColor.
 *
 * The text font is based on X's standard 6x10 font, with a few tweaks like
 * putting a slash across the zero.
 *
 * I'd like to add a bit of visible retrace, but it conflicts with being able
 * to bitcopy the image when fast scrolling. After another couple of CPU
 * generations, we could probably regenerate the whole image from scratch every
 * time. On a P4 2 GHz it can manage this fine for blinking text, but scrolling
 * looks too slow.
 */

static char * apple2_basic_errors[]={
  "BREAK",
  "NEXT WITHOUT FOR",
  "SYNTAX ERROR",
  "RETURN WITHOUT GOSUB",
  "ILLEGAL QUANTITY",
  "OVERFLOW",
  "OUT OF MEMORY",
  "BAD SUBSCRIPT ERROR",
  "DIVISION BY ZERO",
  "STRING TOO LONG",
  "FORMULA TOO COMPLEX",
  "UNDEF'D FUNCTION",
  "OUT OF DATA"
};
static char * apple2_dos_errors[]={
  "VOLUME MISMATCH",
  "I/O ERROR",
  "DISK FULL",
  "NO BUFFERS AVAILABLE",
  "PROGRAM TOO LARGE",
};

struct apple2_state {
  char hireslines[192][40];
  char textlines[24][40];
  int gr_text;
  enum {
    A2_GR_FULL=1,
    A2_GR_LORES=2,
    A2_GR_HIRES=4
  } gr_mode;
  int cursx;
  int cursy;
  int blink;
  int rowimage[24];
};

enum {
  A2_SP_ROWMASK=1023,
  A2_SP_PUT=1024,
  A2_SP_COPY=2048,
};

static void
a2_scroll(struct apple2_state *st)
{
  int i;
  int top=(st->gr_mode&(A2_GR_LORES|A2_GR_HIRES)) ? 20 : 0;
  if ((st->gr_mode&A2_GR_FULL) && (st->gr_mode&A2_GR_HIRES)) return;
  if (st->gr_mode&A2_GR_FULL) top=0;
  for (i=top; i<23; i++) {
    if (memcmp(st->textlines[i],st->textlines[i+1],40)) {
      memcpy(st->textlines[i],st->textlines[i+1],40);
      st->rowimage[i]=st->rowimage[i+1];
    }
  }
  memset(st->textlines[23],0xe0,40);
  st->rowimage[23]=-1;
}

static void
a2_printc(struct apple2_state *st, char c)
{
  st->textlines[st->cursy][st->cursx] |= 0xc0; /* turn off blink */
  if (c=='\n') {
    if (st->cursy==23) {
      a2_scroll(st);
    } else {
      st->rowimage[st->cursy]=-1;
      st->cursy++;
      st->rowimage[st->cursy]=-1;
    }
    st->cursx=0;
  } else {
    st->textlines[st->cursy][st->cursx]=c ^ 0xc0;
    st->rowimage[st->cursy]=-1;
    st->cursx++;
    if (st->cursx==40) {
      if (st->cursy==23) {
        a2_scroll(st);
      } else {
        st->rowimage[st->cursy]=-1;
        st->cursy++;
        st->rowimage[st->cursy]=-1;
      }
      st->cursx=0;
    }
  }
  st->textlines[st->cursy][st->cursx] &= 0x7f; /* turn on blink */
}

static void
a2_goto(struct apple2_state *st, int r, int c)
{
  st->textlines[st->cursy][st->cursx] |= 0xc0; /* turn off blink */
  st->cursy=r;
  st->cursx=c;
  st->textlines[st->cursy][st->cursx] &= 0x7f; /* turn on blink */
}

static void
a2_cls(struct apple2_state *st) 
{
  int i;
  for (i=0; i<24; i++) {
    memset(st->textlines[i],0xe0,40);
    st->rowimage[i]=-1;
  }
}

static void
a2_invalidate(struct apple2_state *st) 
{
  int i;
  for (i=0; i<24; i++) {
    st->rowimage[i]=-1;
  }
}

static void
a2_poke(struct apple2_state *st, int addr, int val)
{
  
  if (addr>=0x400 && addr<0x800) {
    /* text memory */
    int row=((addr&0x380)/0x80) + ((addr&0x7f)/0x28)*8;
    int col=(addr&0x7f)%0x28;
    if (row<24 && col<40) {
      st->textlines[row][col]=val;
      if (!(st->gr_mode&(A2_GR_HIRES)) ||
          (!(st->gr_mode&(A2_GR_FULL)) && row>=20)) {
        st->rowimage[row]=-1;
      }
    }
  }
  else if (addr>=0x2000 && addr<0x4000) {
    int row=(((addr&0x1c00) / 0x400) * 1 +
             ((addr&0x0380) / 0x80) * 8 +
             ((addr&0x0078) / 0x28) * 64);
    int col=((addr&0x07f)%0x28);
    if (row<192 && col<40) {
      st->hireslines[row][col]=val;
      if (st->gr_mode&A2_GR_HIRES) {
        st->rowimage[row/8]=-1;
      }
    }
  }
}

/* This table lists fixes for characters that differ from the standard 6x10
   font. Each encodes a pixel, as (charindex*7 + x) + (y<<10) + (value<<15)
   where value is 0 for white and 1 for black. */
static unsigned short a2_fixfont[] = {
  /* Fix $ */  0x8421, 0x941d,
  /* Fix % */  0x8024, 0x0028, 0x8425, 0x0426, 0x0825, 0x1027, 0x1426, 0x9427,
               0x1824, 0x9828,
  /* Fix * */  0x8049, 0x8449, 0x8849, 0x0c47, 0x0c48, 0x0c4a, 0x0c4b, 0x9049,
               0x9449, 0x9849,
  /* Fix , */  0x9057, 0x1458, 0x9856, 0x1857, 0x1c56,
  /* Fix . */  0x1465, 0x1864, 0x1866, 0x1c65,
  /* Fix / */  0x006e, 0x186a,
  /* Fix 0 */  0x8874, 0x8c73, 0x9072,
  /* Fix 1 */  0x0878, 0x1878, 0x187c,
  /* Fix 5 */  0x8895, 0x0c94, 0x0c95,
  /* Fix 6 */  0x809f, 0x8c9c, 0x109c,
  /* Fix 7 */  0x8ca4, 0x0ca5, 0x90a3, 0x10a4,
  /* Fix 9 */  0x08b3, 0x8cb3, 0x98b0,
  /* Fix : */  0x04b9, 0x08b8, 0x08ba, 0x0cb9, 0x90b9, 0x14b9, 0x18b8, 0x18b9,
               0x18ba, 0x1cb9,
  /* Fix ; */  0x04c0, 0x08bf, 0x08c1, 0x0cc0, 0x90c0, 0x14c1, 0x98bf, 0x18c0,
               0x1cbf,
  /* Fix < */  0x80c8, 0x00c9, 0x84c7, 0x04c8, 0x88c6, 0x08c7, 0x8cc5, 0x0cc6,
               0x90c6, 0x10c7, 
               0x94c7, 0x14c8, 0x98c8, 0x18c9,
  /* Fix > */  0x80d3, 0x00d4, 0x84d4, 0x04d5, 0x88d5, 0x08d6, 0x8cd6, 0x0cd7,
               0x90d5, 0x10d6, 
               0x94d4, 0x14d5, 0x98d3, 0x18d4,
  /* Fix @ */  0x88e3, 0x08e4, 0x8ce4, 0x98e5,
  /* Fix B */  0x84ef, 0x04f0, 0x88ef, 0x08f0, 0x8cef, 0x90ef, 0x10f0, 0x94ef,
               0x14f0,
  /* Fix D */  0x84fd, 0x04fe, 0x88fd, 0x08fe, 0x8cfd, 0x0cfe, 0x90fd, 0x10fe,
               0x94fd, 0x14fe,
  /* Fix G */  0x8116, 0x0516, 0x9916,
  /* Fix J */  0x0129, 0x012a, 0x052a, 0x852b, 0x092a, 0x892b, 0x0d2a, 0x8d2b,
               0x112a, 0x912b, 
               0x152a, 0x952b, 0x992a,
  /* Fix M */  0x853d, 0x853f, 0x093d, 0x893e, 0x093f,
  /* Fix Q */  0x915a, 0x155a, 0x955b, 0x155c, 0x195b, 0x995c, 0x1d5c,
  /* Fix V */  0x8d7b, 0x0d7c, 0x0d7e, 0x8d7f, 0x917b, 0x117c, 0x117e, 0x917f,
  /* Fix [ */  0x819e, 0x81a2, 0x859e, 0x899e, 0x8d9e, 0x919e, 0x959e, 0x999e,
               0x99a2,
  /* Fix \ */  0x01a5, 0x19a9,
  /* Fix ] */  0x81ac, 0x81b0, 0x85b0, 0x89b0, 0x8db0, 0x91b0, 0x95b0, 0x99ac,
               0x99b0,
  /* Fix ^ */  0x01b5, 0x05b4, 0x05b6, 0x09b3, 0x89b5, 0x09b7, 0x8db4, 0x8db6,
               0x91b3, 0x91b7,
  /* Fix _ */  0x9db9, 0x9dbf,
  0,
};

struct ntsc_dec {
  char pattern[600];
  int ntscy[600];
  int ntsci[600];
  int ntscq[600];
  int multi[600];
  int multq[600];
  int brightness_control;
};

/*
  First generate the I and Q reference signals, which we'll multiply by the
  input signal to accomplish the demodulation. Normally they are shifted 33
  degrees from the colorburst. I think this was convenient for
  inductor-capacitor-vacuum tube implementation.
               
  The tint control, FWIW, just adds a phase shift to the chroma signal, and 
  the color control controls the amplitude.
               
  In text modes (colormode==0) the system disabled the color burst, and no
  color was detected by the monitor.

  freq_error gives a mismatch between the built-in oscillator and the TV's
  colorbust. Older II Plus machines seemed to occasionally get instability
  problems -- the crystal oscillator was a single transistor if I remember
  correctly -- and the frequency would vary enough that the tint would change
  across the width of the screen.  The left side would be in correct tint
  because it had just gotten resynchronized with the color burst.
*/
static void
ntsc_set_demod(struct ntsc_dec *it, double tint_control, 
               double color_control, double brightness_control,
               double freq_error, 
               int colormode)
{
  int i;

  it->brightness_control=(int)(1024.0*brightness_control);

  for (i=0; i<600; i++) {
    double phase=90.0-90.0*i + freq_error*i/600.0 + tint_control;
    it->multi[i]=(int)(-cos(3.1415926/180.0*(phase-303)) * 65536.0 * 
                       color_control * colormode * 4);
    it->multq[i]=(int)(cos(3.1415926/180.0*(phase-33)) * 65536.0 * 
                       color_control * colormode * 4);
  }
}

/* Here we model the analog circuitry of an NTSC television. Basically, it
   splits the signal into 3 signals: Y, I and Q. Y corresponds to luminance,
   and you get it by low-pass filtering the input signal to below 3.57 MHz.

   I and Q are the in-phase and quadrature components of the 3.57 MHz
   subcarrier. We get them by multiplying by cos(3.57 MHz*t) and sin(3.57
   MHz*t), and low-pass filtering. Because the eye has less resolution in some
   colors than others, the I component gets low-pass filtered at 1.5 MHz and
   the Q at 0.5 MHz. The I component is approximately orange-blue, and Q is
   roughly purple-green. See http://www.ntsc-tv.com for details.
 */
static void
ntsc_to_yiq(struct ntsc_dec *it) 
{
  int i;
  int fyx[10],fyy[10];
  int fix[10],fiy[10];
  int fqx[10],fqy[10];
  int pixghost;
  int iny,ini,inq,pix,blank;
  
  for (i=0; i<10; i++) fyx[i]=fyy[i]=fix[i]=fiy[i]=fqx[i]=fqy[i]=0.0;
  pixghost=0;
  for (i=0; i<600; i++) {
    /* Get the video out signal, and add a teeny bit of ghosting, typical of RF
       monitor cables. This corresponds to a pretty long cable, but looks right
       to me. */
    pix=it->pattern[i]*1024;
    if (i>=20) pixghost += it->pattern[i-20]*15;
    if (i>=30) pixghost -= it->pattern[i-30]*15;
    pix += pixghost;

    /* Get Y, I, Q before filtering */
    iny=pix;
    ini=(pix*it->multi[i])>>16;
    inq=(pix*it->multq[i])>>16;
            
    blank = (i>=7 && i<596 ? it->brightness_control : -200);

    /* Now filter them. These are infinite impulse response filters calculated
       by the script at http://www-users.cs.york.ac.uk/~fisher/mkfilter. This
       is fixed-point integer DSP, son. No place for wimps. We do it in integer
       because you can count on integer being faster on most CPUs. We care
       about speed because we need to recalculate every time we blink text, and
       when we spew random bytes into screen memory. This is roughly 16.16
       fixed point arithmetic, but we scale some filter values up by a few bits
       to avoid some nasty precision errors. */
            
    /* Filter y at with a 4-pole low-pass Butterworth filter at 3.5 MHz 
       with an extra zero at 3.5 MHz, from
       mkfilter -Bu -Lp -o 4 -a 2.1428571429e-01 0 -Z 2.5e-01 -l
       Delay about 2 */

    fyx[0] = fyx[1]; fyx[1] = fyx[2]; fyx[2] = fyx[3]; 
    fyx[3] = fyx[4]; fyx[4] = fyx[5]; fyx[5] = fyx[6]; 
    fyx[6] = (iny * 1897) >> 13;
    fyy[0] = fyy[1]; fyy[1] = fyy[2]; fyy[2] = fyy[3]; 
    fyy[3] = fyy[4]; fyy[4] = fyy[5]; fyy[5] = fyy[6]; 
    fyy[6] = (fyx[0]+fyx[6]) + 4*(fyx[1]+fyx[5]) + 7*(fyx[2]+fyx[4]) + 8*fyx[3]
      + ((-151*fyy[2] + 8115*fyy[3] - 38312*fyy[4] + 36586*fyy[5]) >> 16);
    if (i>=2) it->ntscy[i-2] = blank + (fyy[6]>>3);

    /* Filter I and Q at 1.5 MHz. 3 pole Butterworth from
       mkfilter -Bu -Lp -o 3 -a 1.0714285714e-01 0
       Delay about 3.

       The NTSC spec says the Q value should be filtered at 0.5 MHz at the
       transmit end, But the Apple's video circuitry doesn't any such thing.
       AFAIK, oldish televisions (before comb filters) simply applied a 1.5 MHz
       filter to both after the demodulator.
    */

    fix[0] = fix[1]; fix[1] = fix[2]; fix[2] = fix[3];
    fix[3] = (ini * 1413) >> 14;
    fiy[0] = fiy[1]; fiy[1] = fiy[2]; fiy[2] = fiy[3];
    fiy[3] = (fix[0]+fix[3]) + 3*(fix[1]+fix[2])
      + ((16559*fiy[0] - 72008*fiy[1] + 109682*fiy[2]) >> 16);
    if (i>=3) it->ntsci[i-3] = fiy[3]>>2;

    fqx[0] = fqx[1]; fqx[1] = fqx[2]; fqx[2] = fqx[3];
    fqx[3] = (inq * 1413) >> 14;
    fqy[0] = fqy[1]; fqy[1] = fqy[2]; fqy[2] = fqy[3];
    fqy[3] = (fqx[0]+fqx[3]) + 3*(fqx[1]+fqx[2])
      + ((16559*fqy[0] - 72008*fqy[1] + 109682*fqy[2]) >> 16);
    if (i>=3) it->ntscq[i-3] = fqy[3]>>2;

  }
  for (; i<610; i++) {
    if (i-2<600) it->ntscy[i-2]=0;
    if (i-3<600) it->ntsci[i-3]=0;
    if (i-9<600) it->ntscq[i-9]=0;
  }
}

enum {
  A2_CMAP_HISTBITS=5,
  A2_CMAP_LEVELS=2,
  A2_CMAP_OFFSETS=4,
};

#define A2_CMAP_INDEX(COLORMODE, LEVEL, HIST, OFFSET) \
((((COLORMODE)*A2_CMAP_LEVELS+(LEVEL))<<A2_CMAP_HISTBITS)+(HIST))* \
A2_CMAP_OFFSETS+(OFFSET)

static void
apple2(Display *dpy, Window window, int delay)
{
  int w,h,i,j,x,y,textrow,row,col,stepno,colormode,imgrow;
  char c,*s;
  struct timeval basetime_tv;
  double next_actiontime;
  XWindowAttributes xgwa;
  int visclass;
  int screen_xo,screen_yo;
  XImage *image=NULL;
  XGCValues gcv;
  GC gc=NULL;
  XImage *text_im=NULL;
  unsigned long colors[A2_CMAP_INDEX(1, A2_CMAP_LEVELS-1,
                                     (1<<A2_CMAP_HISTBITS)-1,
                                     A2_CMAP_OFFSETS-3)+1];
  int n_colors=0;
  int screen_plan[24];
  struct ntsc_dec *dec=NULL;
  short *raw_rgb=NULL, *rrp;
  struct apple2_state *st=NULL;
  char *typing=NULL,*printing=NULL;
  char printbuf[1024];
  char prompt=']';
  int simulate_user;
  double tint_control,color_control,brightness_control,contrast_control;
  double freq_error=0.0,freq_error_inc=0.0;
  double horiz_desync=5.0;
  int flutter_horiz_desync=0;
  int flutter_tint=0;
  double crtload[192];
  int red_invprec,red_shift,green_invprec,green_shift,blue_invprec,blue_shift;
  int fillptr, fillbyte;
  int use_shm,use_cmap,use_color;
  /* localbyteorder is 1 if MSB first, 0 otherwise */
  unsigned int localbyteorder_loc = MSBFirst<<24;
  int localbyteorder=*(char *)&localbyteorder_loc;
#ifdef HAVE_XSHM_EXTENSION
  XShmSegmentInfo shm_info;
#endif
  
#ifdef HAVE_XSHM_EXTENSION
  use_shm=get_boolean_resource ("useSHM", "Boolean");
#else
  use_shm=0;
#endif

  /* Model the video controls on a standard television */
  tint_control = get_float_resource("apple2TVTint","Apple2TVTint");
  color_control = get_float_resource("apple2TVColor","Apple2TVColor")/100.0;
  brightness_control = get_float_resource("apple2TVBrightness",
                                          "Apple2TVBrightness") / 100.0;
  contrast_control = get_float_resource("apple2TVContrast",
                                        "Apple2TVContrast") / 100.0;
  simulate_user = get_boolean_resource("apple2SimulateUser",
                                       "Apple2SimulateUser");

  XGetWindowAttributes (dpy, window, &xgwa);
  visclass=xgwa.visual->class;
  red_shift=red_invprec=green_shift=green_invprec=blue_shift=blue_invprec=-1;
  if (visclass == TrueColor || xgwa.visual->class == DirectColor) {
    use_cmap=0;
    use_color=!mono_p;
  }
  else if (visclass == PseudoColor || visclass == StaticColor) {
    use_cmap=1;
    use_color=!mono_p;
  }
  else {
    use_cmap=1;
    use_color=0;
  }

  /* The Apple II screen was 280x192, sort of. We expand the width to 300
     pixels to allow for overscan. We then pick a size within the window
     that's an integer multiple of 300x192. The small case happens when
     we're displaying in a subwindow. Then it ends up showing the center
     of the screen, which is OK. */
  w=xgwa.width;
  h = (xgwa.height/192)*192;
  if (w<300) w=300;
  if (h==0) h=192;

  dec=(struct ntsc_dec *)malloc(sizeof(struct ntsc_dec));
  
  if (use_cmap) {
    int hist,offset,level;
    int colorprec=8;

  cmap_again:
    n_colors=0;
    /* Typically allocates 214 distinct colors, but will scale back its
       ambitions pretty far if it can't get them */
    for (colormode=0; colormode<=use_color; colormode++) {
      ntsc_set_demod(dec, tint_control, color_control, brightness_control,
                     0.0, colormode);
      for (level=0; level<2; level++) {
        for (hist=0; hist<(1<<A2_CMAP_HISTBITS); hist++) {
          for (offset=0; offset<4; offset++) {
            int interpy,interpi,interpq,r,g,b;
            int levelmult=level ? 64 : 32;
            int prec=colormode ? colorprec : (colorprec*2+2)/3;
            int precmask=(0xffff<<(16-prec))&0xffff;
            XColor col;

            if (A2_CMAP_INDEX(colormode,level,hist,offset) != n_colors) {
              fprintf(stderr, "apple2: internal colormap allocation error\n");
              goto bailout;
            }

            for (i=0; i<600; i++) dec->pattern[i]=0;
            for (i=0; i<A2_CMAP_HISTBITS; i++) {
              dec->pattern[64+offset-i]=(hist>>i)&1;
            }
        
            ntsc_to_yiq(dec);
            interpy=dec->ntscy[63+offset];
            interpi=dec->ntsci[63+offset];
            interpq=dec->ntscq[63+offset];

            r=(interpy + ((+68128*interpi+40894*interpq)>>16))*levelmult;
            g=(interpy + ((-18087*interpi-41877*interpq)>>16))*levelmult;
            b=(interpy + ((-72417*interpi+113312*interpq)>>16))*levelmult;
            if (r<0) r=0;
            if (r>65535) r=65535;
            if (g<0) g=0;
            if (g>65535) g=65535;
            if (b<0) b=0;
            if (b>65535) b=65535;
          
            col.red=r & precmask;
            col.green=g & precmask;
            col.blue=b & precmask;
            col.pixel=0;
            if (!XAllocColor(dpy, xgwa.colormap, &col)) {
              XFreeColors(dpy, xgwa.colormap, colors, n_colors, 0L);
              n_colors=0;
              colorprec--;
              if (colorprec<3) {
                goto bailout;
              }
              goto cmap_again;
            }
            colors[n_colors++]=col.pixel;
          }
        }
      }
    }
  } else {
    /* Is there a standard way to do this? Does this handle all cases? */
    int shift, prec;
    for (shift=0; shift<32; shift++) {
      for (prec=1; prec<16 && prec<32-shift; prec++) {
        unsigned long mask=(0xffffUL>>(16-prec)) << shift;
        if (red_shift<0 && mask==xgwa.visual->red_mask)
          red_shift=shift, red_invprec=16-prec;
        if (green_shift<0 && mask==xgwa.visual->green_mask)
          green_shift=shift, green_invprec=16-prec;
        if (blue_shift<0 && mask==xgwa.visual->blue_mask)
          blue_shift=shift, blue_invprec=16-prec;
      }
    }
    if (red_shift<0 || green_shift<0 || blue_shift<0) {
      if (0) fprintf(stderr,"Can't figure out color space\n");
      goto bailout;
    }
    raw_rgb=(short *)calloc(w*3, sizeof(short));
  }

  gcv.background=0;
  gc = XCreateGC(dpy, window, GCBackground, &gcv);
  XSetWindowBackground(dpy, window, gcv.background);
  XClearWindow(dpy,window);

  screen_xo=(xgwa.width-w)/2;
  screen_yo=(xgwa.height-h)/2;

  if (use_shm) {
#ifdef HAVE_XSHM_EXTENSION
    image = create_xshm_image (dpy, xgwa.visual, xgwa.depth, ZPixmap, 0, 
                               &shm_info, w, h);
#endif
    if (!image) {
      fprintf(stderr, "create_xshm_image failed\n");
      use_shm=0;
    }
  }
  if (!image) {
    image = XCreateImage(dpy, xgwa.visual, xgwa.depth, ZPixmap, 0, 0,
                         w, h, 8, 0);
    image->data = (char *)calloc(image->height, image->bytes_per_line);
  }

  st=(struct apple2_state *)calloc(1,sizeof(struct apple2_state));

  /*
    Generate the font. It used a 5x7 font which looks a lot like the standard X
    6x10 font, with a few differences. So we render up all the uppercase
    letters of 6x10, and make a few tweaks (like putting a slash across the
    zero) according to fixfont.
   */
  {
    const char *def_font="6x10";
    XFontStruct *font;
    Pixmap text_pm;
    GC gc;
    
    font = XLoadQueryFont (dpy, def_font);
    if (!font) {
      fprintf(stderr,"Can't load font %s\n",def_font);
      goto bailout;
    }
    
    text_pm=XCreatePixmap(dpy, window, 64*7, 8, xgwa.depth);
    
    gcv.foreground=1;
    gcv.background=0;
    gcv.font=font->fid;
    gc=XCreateGC(dpy, text_pm, GCFont|GCBackground|GCForeground, &gcv);
    
    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, text_pm, gc, 0, 0, 64*7, 8);
    XSetForeground(dpy, gc, 1);
    for (i=0; i<64; i++) {
      char c=32+i;
      int x=7*i+1;
      int y=7;
      if (c=='0') {
        c='O';
        XDrawString(dpy, text_pm, gc, x, y, &c, 1);
      } else {
        XDrawString(dpy, text_pm, gc, x, y, &c, 1);
      }
    }
    text_im = XGetImage(dpy, text_pm, 0, 0, 64*7, 8, ~0L, ZPixmap);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, text_pm);

    for (i=0; a2_fixfont[i]; i++) {
      XPutPixel(text_im, a2_fixfont[i]&0x3ff,
                (a2_fixfont[i]>>10)&0xf,
                (a2_fixfont[i]>>15)&1);
    }
  }

  /*
    Simulate plausible initial memory contents.
   */
  {
    int addr=0;
    while (addr<0x4000) {
      int n;

      switch (random()%4) {
      case 0:
      case 1:
        n=random()%500;
        for (i=0; i<n && addr<0x4000; i++) {
          u_char rb=((random()%6==0 ? 0 : random()%16) |
                     ((random()%5==0 ? 0 : random()%16)<<4));
          a2_poke(st, addr++, rb);
        }
        break;
      
      case 2:
        /* Simulate shapes stored in memory. We use the font since we have it.
           Unreadable, since rows of each character are stored in consecutive
           bytes. It was typical to store each of the 7 possible shifts of
           bitmaps, for fastest blitting to the screen. */
        x=random()%(text_im->width);
        for (i=0; i<100; i++) {
          for (y=0; y<8; y++) {
            c=0;
            for (j=0; j<8; j++) {
              c |= XGetPixel(text_im, (x+j)%text_im->width, y)<<j;
            }
            a2_poke(st, addr++, c);
          }
          x=(x+1)%(text_im->width);
        }
        break;

      case 3:
        if (addr>0x2000) {
          n=random()%200;
          for (i=0; i<n && addr<0x4000; i++) {
            a2_poke(st, addr++, 0);
          }
        }
        break;

      }
    }
  }
  
  if (random()%4==0 &&
      !use_cmap && use_color &&
      xgwa.visual->bits_per_rgb>=8) {
    flutter_tint=1;
  }
  else if (random()%3==0) {
    flutter_horiz_desync=1;
  }

  crtload[0]=0.0;
  stepno=0;
  a2_goto(st,23,0);
  gettimeofday(&basetime_tv, NULL);
  if (random()%2==0) basetime_tv.tv_sec -= 1; /* random blink phase */
  next_actiontime=0.0;
  fillptr=fillbyte=0;
  while (1) {
    double curtime,blinkphase;
    int startdisplayrow=0;
    int cheapdisplay=0;
    int nodelay=0;
    {
      struct timeval curtime_tv;
      gettimeofday(&curtime_tv, NULL);
      curtime=(curtime_tv.tv_sec - basetime_tv.tv_sec) + 
        0.000001*(curtime_tv.tv_usec - basetime_tv.tv_usec);
    }
    if (curtime>delay) goto finished;

    if (bsod_sleep(dpy,0)) goto finished;

    if (flutter_tint && st->gr_mode && !printing) {
      /* Oscillator instability. Look for freq_error below. We should only do
         this with color depth>=8, since otherwise you see pixels changing. */
      freq_error_inc += (-0.10*freq_error_inc
                         + ((int)(random()&0xff)-0x80) * 0.01);
      freq_error += freq_error_inc;
      a2_invalidate(st);
      nodelay=1;
    }
    else if (flutter_horiz_desync) {
      /* Horizontal sync during vertical sync instability. */
      horiz_desync += (-0.10*(horiz_desync-3.0) +
                       ((int)(random()&0xff)-0x80) * 
                       ((int)(random()&0xff)-0x80) *
                       ((int)(random()&0xff)-0x80) * 0.0000003);
      for (i=0; i<3; i++) st->rowimage[i]=-1;
      nodelay=1;
    } 

    /* It's super-important to get the cursor/text flash out at exactly the
       right time, or it looks wrong. So if we're almost due for a blink, wait
       for it so we don't miss it in the middle of a screen update. */
    blinkphase=curtime/0.8;
    if (blinkphase-floor(blinkphase)>0.7 && !printing && !nodelay) {
      /* We're about to blink */
      int delay = ((1.0-(blinkphase-floor(blinkphase)))*0.8) * 1000000;
      if (delay<1000) delay=1000;
      usleep(delay);
      continue;
    }

    /* The blinking rate was controlled by 555 timer with a resistor/capacitor
       time constant. Because the capacitor was electrolytic, the flash rate
       varied somewhat between machines. I'm guessing 1.6 seconds/cycle was
       reasonable. (I soldered a resistor in mine to make it blink faster.) */
    i=st->blink;
    st->blink=((int)blinkphase)&1;
    if (st->blink!=i && !(st->gr_mode&A2_GR_FULL)) {
      int downcounter=0;
      /* For every row with blinking text, set the changed flag. This basically
         works great except with random screen garbage in text mode, when we
         end up redrawing the whole screen every second */
      for (row=(st->gr_mode ? 20 : 0); row<24; row++) {
        for (col=0; col<40; col++) {
          c=st->textlines[row][col];
          if ((c & 0xc0) == 0x40) {
            downcounter=4;
            break;
          }
        }
        if (downcounter>0) {
          st->rowimage[row]=-1;
          downcounter--;
        }
      }
      st->rowimage[st->cursy]=-1;
      startdisplayrow=random()%24;
    } 
    else if (next_actiontime > curtime && !printing && !nodelay) {
      int delay = (next_actiontime-curtime)*1000000;

      if (delay>100000) delay=100000;
      if (delay<1000) delay=1000;
      usleep(delay);
      continue;
    }

    if (printing) {
      cheapdisplay=1;
      while (*printing) {
        if (*printing=='\001') { /* pause */
          printing++;
          for (i=20; i<24; i++) st->rowimage[i]=-1;
          break;
        } 
        else if (*printing=='\n') {
          a2_printc(st,*printing);
          printing++;
          break;
        }
        else {
          a2_printc(st,*printing);
          printing++;
        }
      }
      if (!*printing) printing=NULL;
    }
    else if (curtime >= next_actiontime) {
      if (typing) {
        /* If we're in the midst of typing a string, emit a character with
           random timing. */
        a2_printc(st, *typing);
        if (*typing=='\n') {
          next_actiontime = curtime;
        } else {
          next_actiontime = curtime + (random()%1000)*0.0003 + 0.3;
        }
        typing++;

        if (!*typing) typing=NULL;

      } 
      else {
        next_actiontime=curtime;

        switch(stepno) {
        case 0:
          a2_invalidate(st);
          if (0) {
            /*
              For testing color rendering. The spec is:
                           red grn blu
              0  black       0   0   0
              1  red       227  30  96
              2  dk blue    96  78 189
              3  purple    255  68 253
              4  dk green    0 163  96
              5  gray      156 156 156
              6  med blue   20 207 253
              7  lt blue   208 195 255
              8  brown      96 114   3
              9  orange    255 106  60
              10 grey      156 156 156
              11 pink      255 160 208
              12 lt green   20 245  60
              13 yellow    208 221 141
              14 aqua      114 255 208
              15 white     255 255 255
            */
            st->gr_mode=A2_GR_LORES;
            for (row=0; row<24; row++) {
              for (col=0; col<40; col++) {
                st->textlines[row][col]=(row&15)*17;
              }
            }
            next_actiontime+=0.4;
            stepno=88;
          }
          else if (random()%3==0) {
            st->gr_mode=0;
            next_actiontime+=0.4;
            stepno=88;
          }
          else if (random()%4==0) {
            st->gr_mode=A2_GR_LORES;
            if (random()%3==0) st->gr_mode |= A2_GR_FULL;
            next_actiontime+=0.4;
            stepno=88;
          } 
          else if (random()%2==0) {
            st->gr_mode=A2_GR_HIRES;
            stepno=73;
          }
          else {
            st->gr_mode=A2_GR_HIRES;
            next_actiontime+=0.4;
            stepno=88;
          }
          break;

        case 88:
          /* An illegal instruction or a reset caused it to drop into the
             assembly language monitor, where you could disassemble code & view
             data in hex. */
          if (random()%3==0) {
            char ibytes[128];
            char itext[128];
            int addr=0xd000+random()%0x3000;
            sprintf(ibytes,
                    "%02X",random()%0xff);
            sprintf(itext,
                    "???");
            sprintf(printbuf,
                    "\n\n"
                    "%04X: %-15s %s\n"
                    " A=%02X X=%02X Y=%02X S=%02X F=%02X\n"
                    "*",
                    addr,ibytes,itext,
                    random()%0xff, random()%0xff,
                    random()%0xff, random()%0xff,
                    random()%0xff);
            printing=printbuf;
            a2_goto(st,23,1);
            if (st->gr_mode) {
              stepno=11;
            } else {
              stepno=13;
            }
            prompt='*';
            next_actiontime += 2.0 + (random()%1000)*0.0002;
          }
          else {
            /* Lots of programs had at least their main functionality in
               Applesoft Basic, which had a lot of limits (memory, string
               length, etc) and would sometimes crash unexpectedly. */
            sprintf(printbuf,
                    "\n"
                    "\n"
                    "\n"
                    "?%s IN %d\n"
                    "\001]",
                    apple2_basic_errors[random() %
                                        (sizeof(apple2_basic_errors)
                                         /sizeof(char *))],
                    (1000*(random()%(random()%59+1)) +
                     100*(random()%(random()%9+1)) +
                     5*(random()%(random()%199+1)) +
                     1*(random()%(random()%(random()%2+1)+1))));
            printing=printbuf;
            a2_goto(st,23,1);
            stepno=1;
            prompt=']';
            next_actiontime += 2.0 + (random()%1000)*0.0002;
          }
          break;
      
        case 1:
          if (simulate_user && random()%3==0) {
            /* This was how you reset the Basic interpreter. The sort of
               incantation you'd have on a little piece of paper taped to the
               side of your machine */
            typing="CALL -1370";
            stepno=2;
          } 
          else if (simulate_user && random()%2==0) {
            typing="CATALOG\n";
            stepno=22;
          }
          else {
            next_actiontime+=1.0;
            stepno=6;
          }
          break;

        case 2:
          stepno=3;
          next_actiontime += 0.5;
          break;
      
        case 3:
          st->gr_mode=0;
          a2_cls(st);
          a2_goto(st,0,16);
          for (s="APPLE ]["; *s; s++) a2_printc(st,*s);
          a2_goto(st,23,0);
          a2_printc(st,']');
          next_actiontime+=1.0;
          stepno=6;
          break;

        case 6:
          if (simulate_user && random()%50==0 && 0) { /* disabled, too goofy */
            typing="10 PRINT \"TRS-80S SUCK!!!\"\n"
              "]20 GOTO 10\n"
              "]RUN";
            stepno=7;
          }
          else {
            stepno=8;
            next_actiontime += delay;
          }
          break;

        case 7:
          for (i=0; i<30; i++) {
            for (s="\nTRS-80S SUCK"; *s; s++) a2_printc(st,*s);
          }
          stepno=8;
          next_actiontime+=delay;

        case 8:
          break;

        case 22:
          if (random()%50==0) {
            sprintf(printbuf,"\nDISK VOLUME 254\n\n"
                    " A 002 HELLO\n"
                    "\n"
                    "]");
            printing=printbuf;
          }
          else {
            sprintf(printbuf,"\n?%s\n]",
                    apple2_dos_errors[random()%
                                      (sizeof(apple2_dos_errors) /
                                       sizeof(char *))]);
            printing=printbuf;
          }
          stepno=6;
          next_actiontime+=1.0;
          break;

        case 11:
          if (simulate_user && random()%2==0) {
            /* This was how you went back to text mode in the monitor */
            typing="FB4BG";
            stepno=12;
          } else {
            next_actiontime+=1.0;
            stepno=6;
          }
          break;

        case 12:
          st->gr_mode=0;
          a2_invalidate(st);
          a2_printc(st,'\n');
          a2_printc(st,'*');
          stepno=13;
          next_actiontime+=2.0;
          break;

        case 13:
          /* This reset things into Basic */
          if (simulate_user && random()%2==0) {
            typing="FAA6G";
            stepno=2;
          }
          else {
            stepno=8;
            next_actiontime+=delay;
          }
          break;

        case 73:
          for (i=0; i<1500; i++) {
            a2_poke(st, fillptr, fillbyte);
            fillptr++;
            fillbyte = (fillbyte+1)&0xff;
          }
          next_actiontime += 0.08;
          /* When you hit c000, it changed video settings */
          if (fillptr>=0xc000) {
            a2_invalidate(st);
            st->gr_mode=0;
          }
          /* And it seemed to reset around here, I dunno why */
          if (fillptr>=0xcf00) stepno=3;
          break;
        }
      }
    }

    /* Now, we turn the data in the Apple II video into a screen display. This
       is interesting because of the interaction with the NTSC color decoding
       in a color television. */

    colormode=use_color && st->gr_mode!=0;
    if (!use_cmap) {
      ntsc_set_demod(dec, tint_control, color_control, brightness_control,
                     freq_error, colormode);
    }
    imgrow=0;
    for (textrow=0; textrow<24; textrow++) {
      if (st->rowimage[textrow] == textrow) {
        screen_plan[textrow]=0;
      }
      else if (cheapdisplay && st->rowimage[textrow]>=0 &&
               textrow<21 && st->rowimage[textrow]<21 && 
               st->rowimage[textrow]>=2 && textrow>=2 &&
               (st->rowimage[textrow]+1)*h/24 + screen_xo <= xgwa.height) {
        screen_plan[textrow]= A2_SP_COPY | st->rowimage[textrow];
        for (i=0; i<8; i++) {
          crtload[textrow*8+i]=crtload[st->rowimage[textrow]*8+i];
        }
        startdisplayrow=0;
      }
      else {
        st->rowimage[textrow]=imgrow;
        screen_plan[textrow]=imgrow | A2_SP_PUT;
        
        for (row=textrow*8; row<textrow*8+8; row++) {
          char *pp;
          int pixmultinc,pixbright;
          int scanstart_i, scanend_i;
          int squishright_i, squishdiv;
          int pixrate;
          double bloomthisrow,shiftthisrow;
          int ytop=(imgrow*h/24) + ((row-textrow*8) * h/192);
          int ybot=ytop+h/192;

          /* First we generate the pattern that the video circuitry shifts out
             of memory. It has a 14.something MHz dot clock, equal to 4 times
             the color burst frequency. So each group of 4 bits defines a
             color.  Each character position, or byte in hires, defines 14
             dots, so odd and even bytes have different color spaces. So,
             pattern[0..600] gets the dots for one scan line. */

          memset(dec->pattern,0,sizeof(dec->pattern));
          pp=dec->pattern+20;
        
          if ((st->gr_mode&A2_GR_HIRES) && (row<160 ||
                                            (st->gr_mode&A2_GR_FULL))) {

            /* Emulate the mysterious pink line, due to a bit getting
               stuck in a shift register between the end of the last
               row and the beginning of this one. */
            if ((st->hireslines[row][0] & 0x80) &&
                (st->hireslines[row][39]&0x40)) {
              pp[-1]=1;
            }

            for (col=0; col<40; col++) {
              u_char b=st->hireslines[row][col];
              int shift=(b&0x80)?0:1;

              /* Each of the low 7 bits in hires mode corresponded to 2 dot
                 clocks, shifted by one if the high bit was set. */
              for (i=0; i<7; i++) {
                pp[shift+1] = pp[shift] =(b>>i)&1;
                pp+=2;
              }
            }
          } 
          else if ((st->gr_mode&A2_GR_LORES) && (row<160 ||
                                                 (st->gr_mode&A2_GR_FULL))) {
            for (col=0; col<40; col++) {
              u_char nib=(st->textlines[textrow][col] >> (((row/4)&1)*4))&0xf;
              /* The low or high nybble was shifted out one bit at a time. */
              for (i=0; i<14; i++) {
                *pp = (nib>>((col*14+i)&3))&1;
                pp++;
              }
            }
          }
          else {
            for (col=0; col<40; col++) {
              int rev;
              c=st->textlines[textrow][col];
              /* hi bits control inverse/blink as follows:
                  0x00: inverse
                  0x40: blink
                  0x80: normal
                  0xc0: normal */
              rev=!(c&0x80) && (!(c&0x40) || st->blink);

              for (i=0; i<7; i++) {
                for (i=0; i<7; i++) {
                  unsigned long pix=XGetPixel(text_im,
                                              ((c&0x3f)^0x20)*7+i, row%8);
                  pp[1] = pp[2] = pix^rev;
                  pp+=2;
                }
              }
            }
          }

          /*
            Interpolate the 600-dotclock line into however many horizontal
            screen pixels we're using, and convert to RGB. 

            We add some 'bloom', variations in the horizontal scan width with
            the amount of brightness, extremely common on period TV sets. They
            had a single oscillator which generated both the horizontal scan
            and (during the horizontal retrace interval) the high voltage for
            the electron beam. More brightness meant more load on the
            oscillator, which caused an decrease in horizontal deflection. Look
            for (bloomthisrow).

            Also, the A2 did a bad job of generating horizontal sync pulses
            during the vertical blanking interval. This, and the fact that the
            horizontal frequency was a bit off meant that TVs usually went a
            bit out of sync during the vertical retrace, and the top of the
            screen would be bent a bit to the left or right. Look for
            (shiftthisrow).

            We also add a teeny bit of left overscan, just enough to be
            annoying, but you can still read the left column of text.
            
            We also simulate compression & brightening on the right side of the
            screen. Most TVs do this, but you don't notice because they
            overscan so it's off the right edge of the CRT. But the A2 video
            system used so much of the horizontal scan line that you had to
            crank the horizontal width down in order to not lose the right few
            characters, and you'd see the compression on the right
            edge. Associated with compression is brightening; since the
            electron beam was scanning slower, the same drive signal hit the
            phosphor harder. Look for (squishright_i) and (squishdiv).
          */

          for (i=j=0; i<600; i++) {
            j += dec->pattern[i];
          }
          crtload[row] = (crtload[row>1 ? row-1 : 0]) * 0.98 + 0.02*(j/600.0) +
            (row>180 ? (row-180)*(row-180)*0.0005 : 0.0);
          bloomthisrow = -10.0 * crtload[row];
          shiftthisrow=((row<18) ? ((18-row)*(18-row)* 0.002 + (18-row)*0.05)
                        * horiz_desync : 0.0);

          scanstart_i=(int)((bloomthisrow+shiftthisrow+18.0)*65536.0);
          if (scanstart_i<0) scanstart_i=0;
          if (scanstart_i>30*65536) scanstart_i=30*65536;
          scanend_i=599*65536;
          squishright_i=scanstart_i + 530*65536;
          squishdiv=w/15;
          pixrate=(int)((560.0-2.0*bloomthisrow)*65536.0/w);
          
          if (use_cmap) {
            for (y=ytop; y<ybot; y++) {
              int level=(!(y==ytop && ybot-ytop>=3) &&
                         !(y==ybot-1 && ybot-ytop>=5));
              int hist=0;
              int histi=0;

              pixmultinc=pixrate;
              for (x=0, i=scanstart_i;
                   x<w && i<scanend_i;
                   x++, i+=pixmultinc) {
                int pati=(i>>16);
                int offset=pati&3;
                while (pati>=histi) {
                  hist=(((hist<<1) & ((1<<A2_CMAP_HISTBITS)-1)) |
                        dec->pattern[histi]);
                  histi++;
                }
                XPutPixel(image, x, y, 
                          colors[A2_CMAP_INDEX(colormode,level,hist,offset)]);
                if (i >= squishright_i) {
                  pixmultinc += pixmultinc/squishdiv;
                }
              }
              for ( ; x<w; x++) {
                XPutPixel(image, x, y, colors[0]);
              }
            }
          } else {

            ntsc_to_yiq(dec);

            pixbright=(int)(contrast_control*65536.0);
            pixmultinc=pixrate;
            for (x=0, i=scanstart_i, rrp=raw_rgb;
                 x<w && i<scanend_i;
                 x++, i+=pixmultinc, rrp+=3) {
              int pixfrac=i&0xffff;
              int invpixfrac=65536-pixfrac;
              int pati=i>>16;
              int r,g,b;

              int interpy=((dec->ntscy[pati]*invpixfrac + 
                            dec->ntscy[pati+1]*pixfrac)>>16);
              int interpi=((dec->ntsci[pati]*invpixfrac + 
                            dec->ntsci[pati+1]*pixfrac)>>16);
              int interpq=((dec->ntscq[pati]*invpixfrac + 
                            dec->ntscq[pati+1]*pixfrac)>>16);

              /*
                According to the NTSC spec, Y,I,Q are generated as:
                
                y=0.30 r + 0.59 g + 0.11 b
                i=0.60 r - 0.28 g - 0.32 b
                q=0.21 r - 0.52 g + 0.31 b
                
                So if you invert the implied 3x3 matrix you get what standard
                televisions implement with a bunch of resistors (or directly in
                the CRT -- don't ask):
                
                r = y + 0.948 i + 0.624 q
                g = y - 0.276 i - 0.639 q
                b = y - 1.105 i + 1.729 q
                
                These coefficients are below in 16.16 format.
              */

              r=((interpy + ((+68128*interpi+40894*interpq)>>16))*pixbright)
                                                           >>16;
              g=((interpy + ((-18087*interpi-41877*interpq)>>16))*pixbright)
                                                           >>16;
              b=((interpy + ((-72417*interpi+113312*interpq)>>16))*pixbright)
                                                            >>16;
              if (r<0) r=0;
              if (g<0) g=0;
              if (b<0) b=0;
              rrp[0]=r;
              rrp[1]=g;
              rrp[2]=b;

              if (i>=squishright_i) {
                pixmultinc += pixmultinc/squishdiv;
                pixbright += pixbright/squishdiv;
              }
            }
            for ( ; x<w; x++, rrp+=3) {
              rrp[0]=rrp[1]=rrp[2]=0;
            }

            for (y=ytop; y<ybot; y++) {
              /* levelmult represents the vertical size of scan lines. Each
                 line is brightest in the middle, and there's a dark band
                 between them. */
              int levelmult;
              double levelmult_fp=(y + 0.5 - (ytop+ybot)*0.5) / (ybot-ytop);
              levelmult_fp = 1.0-(levelmult_fp*levelmult_fp*levelmult_fp
                                  *levelmult_fp)*16.0;
              if (levelmult_fp<0.0) levelmult_fp=0.0;
              levelmult = (int)(64.9*levelmult_fp);

              /* Fast special cases to avoid the slow XPutPixel. Ugh. It goes
                  to show why standard graphics sw has to be fast, or else
                  people will have to work around it and risk incompatibility.
                  The quickdraw folks understood this. The other answer would
                  be for X11 to have fewer formats for bitm.. oh, never
                  mind. If neither of these cases work (they probably cover 99%
                  of setups) it falls back on the Xlib routines. */
              if (image->format==ZPixmap && image->bits_per_pixel==32 && 
                  sizeof(unsigned long)==4 &&
                  image->byte_order==localbyteorder) {
                unsigned long *pixelptr =
                  (unsigned long *) (image->data + y * image->bytes_per_line);
                for (x=0, rrp=raw_rgb; x<w; x++, rrp+=3) {
                  unsigned long ntscri, ntscgi, ntscbi;
                  ntscri=((unsigned long)rrp[0])*levelmult;
                  ntscgi=((unsigned long)rrp[1])*levelmult;
                  ntscbi=((unsigned long)rrp[2])*levelmult;
                  if (ntscri>65535) ntscri=65535;
                  if (ntscgi>65535) ntscgi=65535;
                  if (ntscbi>65535) ntscbi=65535;
                  *pixelptr++ = ((ntscri>>red_invprec)<<red_shift) |
                    ((ntscgi>>green_invprec)<<green_shift) |
                    ((ntscbi>>blue_invprec)<<blue_shift);
                }
              }
              else if (image->format==ZPixmap && image->bits_per_pixel==16 && 
                       sizeof(unsigned short)==2 &&
                       image->byte_order==localbyteorder) {
                unsigned short *pixelptr =
                (unsigned short *)(image->data + y*image->bytes_per_line);
                for (x=0, rrp=raw_rgb; x<w; x++, rrp+=3) {
                  unsigned long ntscri, ntscgi, ntscbi;
                  ntscri=((unsigned long)rrp[0])*levelmult;
                  ntscgi=((unsigned long)rrp[1])*levelmult;
                  ntscbi=((unsigned long)rrp[2])*levelmult;
                  if (ntscri>65535) ntscri=65535;
                  if (ntscgi>65535) ntscgi=65535;
                  if (ntscbi>65535) ntscbi=65535;
                  *pixelptr++ = ((ntscri>>red_invprec)<<red_shift) |
                    ((ntscgi>>green_invprec)<<green_shift) |
                    ((ntscbi>>blue_invprec)<<blue_shift);
                }
                
              }
              else {
                for (x=0, rrp=raw_rgb; x<w; x++, rrp+=3) {
                  unsigned long pixel, ntscri, ntscgi, ntscbi;
                  /* Convert to 16-bit color values, with saturation. The ntscr
                     values are 22.10 fixed point, and levelmult is 24.6, so we
                     get 16 bits out*/
                  ntscri=((unsigned long)rrp[0])*levelmult;
                  ntscgi=((unsigned long)rrp[1])*levelmult;
                  ntscbi=((unsigned long)rrp[2])*levelmult;
                  if (ntscri>65535) ntscri=65535;
                  if (ntscgi>65535) ntscgi=65535;
                  if (ntscbi>65535) ntscbi=65535;
                  pixel = ((ntscri>>red_invprec)<<red_shift) |
                    ((ntscgi>>green_invprec)<<green_shift) |
                    ((ntscbi>>blue_invprec)<<blue_shift);
                  XPutPixel(image, x, y, pixel);
                }
              }
            }
          }
        }
        imgrow++;
      }
    }

    /* For just the the rows which changed, blit the image to the screen. */
    for (textrow=0; textrow<24; ) {
      int top,bot,srcrow,srctop,nrows;
      
      nrows=1;
      while (textrow+nrows < 24 &&
             screen_plan[textrow+nrows] == screen_plan[textrow]+nrows)
        nrows++;

      top=h*textrow/24;
      bot=h*(textrow+nrows)/24;
      srcrow=screen_plan[textrow]&A2_SP_ROWMASK;
      srctop=srcrow*h/24;

      if (screen_plan[textrow] & A2_SP_COPY) {
        if (0) printf("Copy %d screenrows %d to %d\n", nrows, srcrow, textrow);
        XCopyArea(dpy, window, window, gc,
                  screen_xo, screen_yo + srctop,
                  w, bot-top,
                  screen_xo, screen_yo + top);
      }
      else if (screen_plan[textrow] & A2_SP_PUT) {
        if (0) printf("Draw %d imgrows %d to %d\n", nrows, srcrow, textrow);
        if (use_shm) {
#ifdef HAVE_XSHM_EXTENSION
          XShmPutImage(dpy, window, gc, image, 
                       0, srctop, screen_xo, screen_yo + top,
                       w, bot-top, False);
#endif
        } else {
          XPutImage(dpy, window, gc, image, 
                    0, srctop,
                    screen_xo, screen_yo + top,
                    w, bot-top);
        }
      }
      textrow += nrows;
    }
    XSync(dpy,0);

    for (textrow=0; textrow<24; textrow++) {
      st->rowimage[textrow]=textrow;
    }
  }

 finished:
  XSync(dpy,False);
  XClearWindow(dpy, window);
  goto cleanup;

 bailout:
  ;

 cleanup:
  if (image) {
    if (use_shm) {
#ifdef HAVE_XSHM_EXTENSION
      destroy_xshm_image(dpy, image, &shm_info);
#endif
    } else {
      XDestroyImage(image);
    }
    image=NULL;
  }
  if (text_im) XDestroyImage(text_im);
  if (gc) XFreeGC(dpy, gc);
  if (st) free(st);
  if (raw_rgb) free(raw_rgb);
  if (dec) free(dec);
  if (n_colors) XFreeColors(dpy, xgwa.colormap, colors, n_colors, 0L);
}



char *progclass = "BSOD";

char *defaults [] = {
  "*delay:		   30",

  "*doOnly:		   ",
  "*doWindows:		   True",
  "*doNT:		   True",
  "*doWin2K:		   True",
  "*doAmiga:		   True",
  "*doMac:		   True",
  "*doMacsBug:		   True",
  "*doMac1:		   True",
  "*doMacX:		   True",
  "*doSCO:		   True",
  "*doAtari:		   False",	/* boring */
  "*doBSD:		   False",	/* boring */
  "*doLinux:		   True",
  "*doSparcLinux:	   False",	/* boring */
  "*doBlitDamage:          True",
  "*doSolaris:             True",
  "*doHPUX:                True",
  "*doApple2:              True",
  "*doOS390:               True",

  ".Windows.font:	   -*-courier-bold-r-*-*-*-120-*-*-m-*-*-*",
  ".Windows.font2:	   -*-courier-bold-r-*-*-*-180-*-*-m-*-*-*",
  ".Windows.foreground:	   White",
  ".Windows.background:	   #0000AA",    /* EGA color 0x01. */

  ".Amiga.font:		   -*-courier-bold-r-*-*-*-120-*-*-m-*-*-*",
  ".Amiga.font2:	   -*-courier-bold-r-*-*-*-180-*-*-m-*-*-*",
  ".Amiga.foreground:	   Red",
  ".Amiga.background:	   Black",
  ".Amiga.background2:	   White",

  ".Mac.font:		   -*-courier-bold-r-*-*-*-120-*-*-m-*-*-*",
  ".Mac.foreground:	   PaleTurquoise1",
  ".Mac.background:	   Black",

  ".Atari.foreground:	   Black",
  ".Atari.background:	   White",

  ".MacsBug.font:	   -*-courier-medium-r-*-*-*-100-*-*-m-*-*-*",
  ".MacsBug.font2:	   -*-courier-bold-r-*-*-*-120-*-*-m-*-*-*",
  ".MacsBug.font3:	   -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*",
  ".MacsBug.foreground:	   Black",
  ".MacsBug.background:	   White",
  ".MacsBug.borderColor:   #AAAAAA",

  ".mac1.foreground:	   Black",
  ".mac1.background:	   White",

  ".macX.textForeground:   White",
  ".macX.textBackground:   Black",
  ".macX.background:	   #888888",
  ".macX.font:		   -*-courier-bold-r-*-*-*-120-*-*-m-*-*-*",
  ".macX.font2:		   -*-courier-bold-r-*-*-*-240-*-*-m-*-*-*",

  ".SCO.font:		   -*-courier-bold-r-*-*-*-120-*-*-m-*-*-*",
  ".SCO.font2:		   -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*",
  ".SCO.foreground:	   White",
  ".SCO.background:	   Black",

  ".Linux.font:		   9x15bold",
  ".Linux.font2:	   -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*",
  ".Linux.foreground:      White",
  ".Linux.background:      Black",

  ".SparcLinux.font:	   -*-courier-bold-r-*-*-*-120-*-*-m-*-*-*",
  ".SparcLinux.font2:	   -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*",
  ".SparcLinux.foreground: White",
  ".SparcLinux.background: Black",

  ".BSD.font:		   vga",
  ".BSD.font:		   -*-courier-bold-r-*-*-*-120-*-*-m-*-*-*",
  ".BSD.font2:		   -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*",
/* ".BSD.font2:		   -sun-console-medium-r-*-*-22-*-*-*-m-*-*-*", */
  ".BSD.foreground:	   #c0c0c0",
  ".BSD.background:	   Black",

  ".Solaris.font:          -sun-gallant-*-*-*-*-19-*-*-*-*-120-*-*",
  ".Solaris.font2:         -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*",
  ".Solaris.foreground:    Black",
  ".Solaris.background:    White",
  "*dontClearRoot:         True",

  ".HPUX.font:		   9x15bold",
  ".HPUX.font2:		   -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*",
  ".HPUX.foreground:	   White",
  ".HPUX.background:	   Black",

  ".OS390.font:		   9x15bold",
  ".OS390.font2:	   -*-courier-bold-r-*-*-*-140-*-*-m-*-*-*",
  ".OS390.background:	   Black",
  ".OS390.foreground:	   Red",

  "*apple2TVColor:         50",
  "*apple2TVTint:          5",
  "*apple2TVBrightness:    10",
  "*apple2TVContrast:      90",
  "*apple2SimulateUser:    True",

#ifdef HAVE_XSHM_EXTENSION
  "*useSHM:                True",
#endif
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-only",		".doOnly",		XrmoptionSepArg, 0 },
  { "-windows",		".doWindows",		XrmoptionNoArg,  "True"  },
  { "-no-windows",	".doWindows",		XrmoptionNoArg,  "False" },
  { "-nt",		".doNT",		XrmoptionNoArg,  "True"  },
  { "-no-nt",		".doNT",		XrmoptionNoArg,  "False" },
  { "-2k",		".doWin2K",		XrmoptionNoArg,  "True"  },
  { "-no-2k",		".doWin2K",		XrmoptionNoArg,  "False" },
  { "-amiga",		".doAmiga",		XrmoptionNoArg,  "True"  },
  { "-no-amiga",	".doAmiga",		XrmoptionNoArg,  "False" },
  { "-mac",		".doMac",		XrmoptionNoArg,  "True"  },
  { "-no-mac",		".doMac",		XrmoptionNoArg,  "False" },
  { "-mac1",		".doMac1",		XrmoptionNoArg,  "True"  },
  { "-no-mac1",		".doMac1",		XrmoptionNoArg,  "False" },
  { "-macx",		".doMacX",		XrmoptionNoArg,  "True"  },
  { "-no-macx",		".doMacX",		XrmoptionNoArg,  "False" },
  { "-atari",		".doAtari",		XrmoptionNoArg,  "True"  },
  { "-no-atari",	".doAtari",		XrmoptionNoArg,  "False" },
  { "-macsbug",		".doMacsBug",		XrmoptionNoArg,  "True"  },
  { "-no-macsbug",	".doMacsBug",		XrmoptionNoArg,  "False" },
  { "-apple2",		".doApple2",		XrmoptionNoArg,  "True"  },
  { "-no-apple2",	".doApple2",		XrmoptionNoArg,  "False" },
  { "-sco",		".doSCO",		XrmoptionNoArg,  "True"  },
  { "-no-sco",		".doSCO",		XrmoptionNoArg,  "False" },
  { "-bsd",		".doBSD",		XrmoptionNoArg,  "True"  },
  { "-no-bsd",		".doBSD",		XrmoptionNoArg,  "False" },
  { "-linux",		".doLinux",		XrmoptionNoArg,  "True"  },
  { "-no-linux",	".doLinux",		XrmoptionNoArg,  "False" },
  { "-sparclinux",	".doSparcLinux",	XrmoptionNoArg,  "True"  },
  { "-no-sparclinux",	".doSparcLinux",	XrmoptionNoArg,  "False" },
  { "-blitdamage",	".doBlitDamage",	XrmoptionNoArg,  "True"  },
  { "-no-blitdamage",	".doBlitDamage",	XrmoptionNoArg,  "False" },
  { "-solaris",		".doSolaris",		XrmoptionNoArg,  "True"  },
  { "-no-solaris",	".doSolaris",		XrmoptionNoArg,  "False" },
  { "-hpux",		".doHPUX",		XrmoptionNoArg,  "True"  },
  { "-no-hpux",		".doHPUX",		XrmoptionNoArg,  "False" },
  { "-os390",		".doOS390",		XrmoptionNoArg,  "True"  },
  { "-no-os390",	".doOS390",		XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
};


static struct {
  const char *name;
  void (*fn) (Display *, Window, int delay);
} all_modes[] = {
  { "Windows",		windows_31 },
  { "Nt",		windows_nt },
  { "2k",		windows_2k },
  { "Amiga",		amiga },
  { "Mac",		mac },
  { "MacsBug",		macsbug },
  { "Mac1",		mac1 },
  { "MacX",		macx },
  { "SCO",		sco },
  { "SparcLinux",	sparc_linux },
  { "BSD",		bsd },
  { "Atari",		atari },
  { "BlitDamage",	blitdamage },
  { "Solaris",		sparc_solaris },
  { "Linux",		linux_fsck },
  { "HPUX",		hpux },
  { "OS390",		os390 },
  { "Apple2",		apple2 },
};


void
screenhack (Display *dpy, Window window)
{
  int loop = 0;
  int i = -1;
  int j = -1;
  int only = -1;
  int delay = get_integer_resource ("delay", "Integer");
  if (delay < 3) delay = 3;

  {
    char *s = get_string_resource("doOnly", "DoOnly");
    if (s && *s)
      {
        int count = countof(all_modes);
        for (only = 0; only < count; only++)
          if (!strcasecmp (s, all_modes[only].name))
            break;
        if (only >= count)
          {
            fprintf (stderr, "%s: unknown -only mode: \"%s\"\n", progname, s);
            only = -1;
          }
      }
    if (s) free (s);
  }

  if (!get_boolean_resource ("root", "Boolean"))
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, window, &xgwa);
      XSelectInput (dpy, window,
                    xgwa.your_event_mask | KeyPressMask | ButtonPressMask);
    }

  while (1)
    {
      Bool did;
      int count = countof(all_modes);
      char name[100], class[100];

      if (only > 0)
        i = only;
      else
        do {  i = (random() & 0xFF) % count; } while (i == j);

      sprintf (name,  "do%s", all_modes[i].name);
      sprintf (class, "Do%s", all_modes[i].name);

      did = False;
      if (only > 0 || get_boolean_resource(name, class))
        {
          all_modes[i].fn (dpy, window, delay);
          did = True;
        }

      loop++;
      if (loop > 100) j = -1;
      if (loop > 200)
        {
          fprintf (stderr, "%s: no display modes enabled?\n", progname);
          exit(-1);
        }
      if (!did) continue;
      XSync (dpy, False);
      j = i;
      loop = 0;
    }
}
