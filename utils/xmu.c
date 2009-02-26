/* This file contains compatibility routines for systems without Xmu.
 * You would be better served by installing Xmu on your machine or
 * yelling at your vendor to ship it.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_XMU
/*
 * Copyright 1989 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "xmu.h"

#ifndef NEED_EVENTS
# define NEED_EVENTS		/* to make Xproto.h define xEvent */
#endif
#ifndef VMS
# include <X11/Xproto.h>	/* for xEvent (used by Xlibint.h) */
# include <X11/Xlibint.h>	/* for _XExtension */
#else /* VMS */
# include <X11/Xlib.h>
#endif /* VMS */
#include <X11/Intrinsic.h>	/* for XtSpecificationRelease */

/*
 * XmuPrintDefaultErrorMessage - print a nice error that looks like the usual 
 * message.  Returns 1 if the caller should consider exitting else 0.
 */
int XmuPrintDefaultErrorMessage (Display *dpy, XErrorEvent *event, FILE *fp)
{
    char buffer[BUFSIZ];
    char mesg[BUFSIZ];
    char number[32];
    char *mtype = "XlibMessage";
    _XExtension *ext = (_XExtension *)NULL;
    XGetErrorText(dpy, event->error_code, buffer, BUFSIZ);
    XGetErrorDatabaseText(dpy, mtype, "XError", "X Error", mesg, BUFSIZ);
    (void) fprintf(fp, "%s:  %s\n  ", mesg, buffer);
    XGetErrorDatabaseText(dpy, mtype, "MajorCode", "Request Major code %d", 
	mesg, BUFSIZ);
    (void) fprintf(fp, mesg, event->request_code);
    if (event->request_code < 128) {
	sprintf(number, "%d", event->request_code);
	XGetErrorDatabaseText(dpy, "XRequest", number, "", buffer, BUFSIZ);
    } else {
	/* XXX this is non-portable */
	for (ext = dpy->ext_procs;
	     ext && (ext->codes.major_opcode != event->request_code);
	     ext = ext->next)
	  ;
	if (ext)
	    strcpy(buffer, ext->name);
	else
	    buffer[0] = '\0';
    }
    (void) fprintf(fp, " (%s)", buffer);
    fputs("\n  ", fp);
#if (XtSpecificationRelease >= 5)
    if (event->request_code >= 128) {
	XGetErrorDatabaseText(dpy, mtype, "MinorCode", "Request Minor code %d",
			      mesg, BUFSIZ);
	(void) fprintf(fp, mesg, event->minor_code);
	if (ext) {
	    sprintf(mesg, "%s.%d", ext->name, event->minor_code);
	    XGetErrorDatabaseText(dpy, "XRequest", mesg, "", buffer, BUFSIZ);
	    (void) fprintf(fp, " (%s)", buffer);
	}
	fputs("\n  ", fp);
    }
    if (event->error_code >= 128) {
	/* let extensions try to print the values */
	/* XXX this is non-portable code */
	for (ext = dpy->ext_procs; ext; ext = ext->next) {
	    if (ext->error_values)
		(*ext->error_values)(dpy, event, fp);
	}
	/* the rest is a fallback, providing a simple default */
	/* kludge, try to find the extension that caused it */
	buffer[0] = '\0';
	for (ext = dpy->ext_procs; ext; ext = ext->next) {
	    if (ext->error_string) 
		(*ext->error_string)(dpy, event->error_code, &ext->codes,
				     buffer, BUFSIZ);
	    if (buffer[0])
		break;
	}    
	if (buffer[0])
	    sprintf(buffer, "%s.%d", ext->name,
		    event->error_code - ext->codes.first_error);
	else
	    strcpy(buffer, "Value");
	XGetErrorDatabaseText(dpy, mtype, buffer, "", mesg, BUFSIZ);
	if (*mesg) {
	    (void) fprintf(fp, mesg, event->resourceid);
	    fputs("\n  ", fp);
	}
    } else if ((event->error_code == BadWindow) ||
	       (event->error_code == BadPixmap) ||
	       (event->error_code == BadCursor) ||
	       (event->error_code == BadFont) ||
	       (event->error_code == BadDrawable) ||
	       (event->error_code == BadColor) ||
	       (event->error_code == BadGC) ||
	       (event->error_code == BadIDChoice) ||
	       (event->error_code == BadValue) ||
	       (event->error_code == BadAtom)) {
	if (event->error_code == BadValue)
	    XGetErrorDatabaseText(dpy, mtype, "Value", "Value 0x%x",
				  mesg, BUFSIZ);
	else if (event->error_code == BadAtom)
	    XGetErrorDatabaseText(dpy, mtype, "AtomID", "AtomID 0x%x",
				  mesg, BUFSIZ);
	else
	    XGetErrorDatabaseText(dpy, mtype, "ResourceID", "ResourceID 0x%x",
				  mesg, BUFSIZ);
	(void) fprintf(fp, mesg, event->resourceid);
	fputs("\n  ", fp);
    }
#elif (XtSpecificationRelease == 4)
    XGetErrorDatabaseText(dpy, mtype, "MinorCode", "Request Minor code %d",
			  mesg, BUFSIZ);
    (void) fprintf(fp, mesg, event->minor_code);
    fputs("\n  ", fp);
    if (ext) {
      sprintf(mesg, "%s.%d", ext->name, event->minor_code);
      XGetErrorDatabaseText(dpy, "XRequest", mesg, "", buffer, BUFSIZ);
      (void) fprintf(fp, " (%s)", buffer);
    }
    XGetErrorDatabaseText(dpy, mtype, "ResourceID", "ResourceID 0x%x",
			  mesg, BUFSIZ);
    (void) fprintf(fp, mesg, event->resourceid);
    fputs("\n  ", fp);
#else
ERROR! Unsupported release of X11
#endif
    XGetErrorDatabaseText(dpy, mtype, "ErrorSerial", "Error Serial #%d", 
	mesg, BUFSIZ);
    (void) fprintf(fp, mesg, event->serial);
    fputs("\n  ", fp);
    XGetErrorDatabaseText(dpy, mtype, "CurrentSerial", "Current Serial #%d",
	mesg, BUFSIZ);
    (void) fprintf(fp, mesg, NextRequest(dpy)-1);
    fputs("\n", fp);
    if (event->error_code == BadImplementation) return 0;
    return 1;
}

#endif /* !HAVE_XMU */
