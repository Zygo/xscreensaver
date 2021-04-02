/* xscreensaver, Copyright © 1991-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Some random diagnostics printed in -verbose mode about what extensions
   are available on the server, and their version numbers.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Intrinsic.h>

#include <X11/extensions/XInput2.h>

#ifdef HAVE_XSHM_EXTENSION
# include <X11/extensions/XShm.h>
#endif /* HAVE_XSHM_EXTENSION */

#ifdef HAVE_DPMS_EXTENSION
# include <X11/extensions/dpms.h>
#endif /* HAVE_DPMS_EXTENSION */


#ifdef HAVE_DOUBLE_BUFFER_EXTENSION
# include <X11/extensions/Xdbe.h>
#endif /* HAVE_DOUBLE_BUFFER_EXTENSION */

#ifdef HAVE_XF86VMODE
# include <X11/extensions/xf86vmode.h>
#endif /* HAVE_XF86VMODE */

#ifdef HAVE_XINERAMA
# include <X11/extensions/Xinerama.h>
#endif /* HAVE_XINERAMA */

#ifdef HAVE_RANDR
# include <X11/extensions/Xrandr.h>
#endif /* HAVE_RANDR */

#ifdef HAVE_XCOMPOSITE_EXTENSION
# include <X11/extensions/Xcomposite.h>
#endif

#ifdef HAVE_XKB
# include <X11/XKBlib.h>
#endif

#include "xscreensaver.h"


void
print_available_extensions (saver_info *si)
{
  int i, j;
  static struct {
    const char *name; const char *desc; 
    Bool useful_p;
    Status (*version_fn) (Display *, int *majP, int *minP);
  } exts[] = {

# if 0
   { "MIT-SCREEN-SAVER",                        "MIT Screen-Saver",
        False, 0
   },
# endif
   { "MIT-SHM",                                 "Shared Memory",
#     ifdef HAVE_XSHM_EXTENSION
        True, (Status (*) (Display*,int*,int*)) XShmQueryVersion /* 4 args */
#     else
        False, 0
#     endif
   }, { "DOUBLE-BUFFER",                        "Double-Buffering",
#     ifdef HAVE_DOUBLE_BUFFER_EXTENSION
        True, XdbeQueryExtension
#     else
        False, 0
#     endif
   }, { "DPMS",                                 "Power Management",
#     ifdef HAVE_DPMS_EXTENSION
        True,  DPMSGetVersion
#     else
        False, 0
#     endif
   }, { "GLX",                                  "GLX",
#     ifdef HAVE_GL
        True,  0
#     else
        False, 0
#     endif
   }, { "XFree86-VidModeExtension",             "XF86 Video-Mode",
#     ifdef HAVE_XF86VMODE
        True,  XF86VidModeQueryVersion
#     else
        False, 0
#     endif
   }, { "XC-VidModeExtension",                  "XC Video-Mode",
#     ifdef HAVE_XF86VMODE
        True,  XF86VidModeQueryVersion
#     else
        False, 0
#     endif
   }, { "XINERAMA",                             "Xinerama",
#     ifdef HAVE_XINERAMA
        True,  XineramaQueryVersion
#     else
        False, 0
#     endif
   }, { "RANDR",                                "Resize-and-Rotate",
#     ifdef HAVE_RANDR
        True,  XRRQueryVersion
#     else
        False, 0
#     endif
   }, { "Composite",                            "Composite",
#     ifdef HAVE_XCOMPOSITE_EXTENSION
        True,  XCompositeQueryVersion
#     else
        True, 0
#     endif
   }, { "XKEYBOARD",                            "XKeyboard",
#     ifdef HAVE_XKB
        True,  0,
#     else
        False, 0
#     endif
   }, { "DRI",			"DRI",		True, 0
   }, { "NV-CONTROL",		"NVidia",	True, 0
   }, { "NV-GLX",		"NVidia GLX",	True, 0
   }, { "Apple-DRI",		"Apple-DRI",	True, 0
   }, { "Apple-WM",		"Apple-WM",	True, 0
   }, { "XInputExtension",	"XInput",	True, 0
   },
  };

  fprintf (stderr, "%s: running on display \"%s\"\n", blurb(), 
           DisplayString(si->dpy));
  fprintf (stderr, "%s: vendor is %s, %d\n", blurb(),
	   ServerVendor(si->dpy), VendorRelease(si->dpy));

  fprintf (stderr, "%s: useful extensions:\n", blurb());
  for (i = 0; i < countof(exts); i++)
    {
      int op = 0, event = 0, error = 0;
      char buf [255];
      int maj = 0, min = 0;
      int dummy1, dummy2, dummy3;

      /* Most of the extension version functions take 3 args,
         writing results into args 2 and 3, but some take more.
         We only ever care about the first two results, but we
         pass in three extra pointers just in case.
       */
      Status (*version_fn_2) (Display*,int*,int*,int*,int*,int*) =
        (Status (*) (Display*,int*,int*,int*,int*,int*)) exts[i].version_fn;

      if (!XQueryExtension (si->dpy, exts[i].name, &op, &event, &error))
        continue;
      sprintf (buf, "%s:   ", blurb());
      strcat (buf, exts[i].desc);

      if (!strcmp (exts[i].desc, "XInput"))
        {
          int maj = 999, min = 999;              /* Desired */
          XIQueryVersion (si->dpy, &maj, &min);  /* Actual */
          sprintf (buf+strlen(buf), " (%d.%d)", maj, min);
        }
      else if (!version_fn_2)
        ;
      else if (version_fn_2 (si->dpy, &maj, &min, &dummy1, &dummy2, &dummy3))
        sprintf (buf+strlen(buf), " (%d.%d)", maj, min);
      else
        strcat (buf, " (unavailable)");

      if (!exts[i].useful_p)
        strcat (buf, " (disabled at compile time)");
      fprintf (stderr, "%s\n", buf);
    }

# ifdef HAVE_LIBSYSTEMD
  fprintf (stderr, "%s:   libsystemd\n", blurb());
# else
  fprintf (stderr, "%s:   libsystemd (disabled at compile time)\n", blurb());
# endif

  for (i = 0; i < si->nscreens; i++)
    {
      saver_screen_info *ssi = &si->screens[i];
      unsigned long colormapped_depths = 0;
      unsigned long non_mapped_depths = 0;
      XVisualInfo vi_in, *vi_out;
      int out_count;

      if (!ssi->real_screen_p) continue;

      vi_in.screen = ssi->real_screen_number;
      vi_out = XGetVisualInfo (si->dpy, VisualScreenMask, &vi_in, &out_count);
      if (!vi_out) continue;
      for (j = 0; j < out_count; j++) {
	if (vi_out[j].depth >= 32) continue;
	if (vi_out[j].class == PseudoColor)
	  colormapped_depths |= (1 << vi_out[j].depth);
	else
	  non_mapped_depths  |= (1 << vi_out[j].depth);
	}
      XFree ((char *) vi_out);

      if (colormapped_depths)
	{
	  fprintf (stderr, "%s: screen %d colormapped depths:", blurb(),
                   ssi->real_screen_number);
	  for (j = 0; j < 32; j++)
	    if (colormapped_depths & (1 << j))
	      fprintf (stderr, " %d", j);
	  fprintf (stderr, "\n");
	}
      if (non_mapped_depths)
	{
	  fprintf (stderr, "%s: screen %d non-colormapped depths:",
                   blurb(), ssi->real_screen_number);
	  for (j = 0; j < 32; j++)
	    if (non_mapped_depths & (1 << j))
	      fprintf (stderr, " %d", j);
	  fprintf (stderr, "\n");
	}
    }
}
