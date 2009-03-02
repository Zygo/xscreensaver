/* Taken from X11R6.4 ParseCmd.c
   Modified so it does not change (or shadow) argv and argc.
   This is only tested for a few cases in the switch.
   This is needed if one has multiple use options to set ...
   like a program.a.optname1 and program.b.optname1 .
   One can use this to set the options and then run the real
   XrmParseCommand on a null XrmDatabase to have it
   then modify argv and argc for checking purposes.
 */

/* $TOG: ParseCmd.c /main/26 1998/02/06 17:46:14 kaleb $ */

/***********************************************************

Copyright 1987, 1988, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* XrmParseCommand()

   Parse command line and store argument values into resource database

   Allows any un-ambiguous abbreviation for an option name, but requires
   that the table be ordered with any options that are prefixes of
   other options appearing before the longer version in the table.
*/

#ifdef VMS
#include "vms_x_fix.h"
#endif

#ifndef WIN32
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#else /* WIN32 */
#include "Xapi.h"
#endif /* WIN32 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _XReportParseError(XrmOptionDescRec *arg, char *msg)
{
    (void) fprintf(stderr, "Error parsing argument \"%s\" (%s); %s\n",
		   arg->option, arg->specifier, msg);
    exit(1);
}

void
XlockrmParseCommand(
    XrmDatabase		*pdb,		/* data base */
    register XrmOptionDescList options, /* pointer to table of valid options */
    int			num_options,	/* number of options		     */
    char	*prefix,	/* name to prefix resources with     */
    int			*arg_c,		/* address of argument count 	     */
    char		**arg_v)		/* argument list (command line)	     */
{
#ifndef WIN32
    int 		foundOption;
    char		**argsave;
    register int	i, myargc;
    XrmBinding		bindings[100];
    XrmQuark		quarks[100];
    XrmBinding		*start_bindings;
    XrmQuark		*start_quarks;
    char		*optP, *argP = (char *) NULL, optchar, argchar = '\0';
    int			matches;
    enum {DontCare, Check, NotSorted, Sorted} table_is_sorted;
#if 0
    char		**argend;
#endif

#define PutCommandResource(value_str)				\
    {								\
    XrmStringToBindingQuarkList(				\
	options[i].specifier, start_bindings, start_quarks);    \
    XrmQPutStringResource(pdb, bindings, quarks, value_str);    \
    } /* PutCommandResource */

    myargc = (*arg_c);
#if 0
    argend = arg_v + myargc;
    argsave = ++arg_v;
#else
    argsave = arg_v;
    ++arg_v;
#endif
    /* Initialize bindings/quark list with prefix (typically app name). */
    quarks[0] = XrmStringToName(prefix);
    bindings[0] = XrmBindTightly;
    start_quarks = quarks+1;
    start_bindings = bindings+1;

    table_is_sorted = (myargc > 2) ? Check : DontCare;
    for (--myargc; myargc > 0; --myargc, ++arg_v) {
	foundOption = False;
	matches = 0;
	for (i=0; i < num_options; ++i) {
	    /* checking the sort order first insures we don't have to
	       re-do the check if the arg hits on the last entry in
	       the table.  Useful because usually '=' is the last entry
	       and users frequently specify geometry early in the command */
	    if (table_is_sorted == Check && i > 0 &&
		strcmp(options[i].option, options[i-1].option) < 0) {
		table_is_sorted = NotSorted;
	    }
	    for (argP = *arg_v, optP = options[i].option;
		 (optchar = *optP++) &&
		 (argchar = *argP++) &&
		 argchar == optchar;);
	    if (!optchar) {
		if (!*argP ||
		    options[i].argKind == XrmoptionStickyArg ||
		    options[i].argKind == XrmoptionIsArg) {
		    /* give preference to exact matches, StickyArg and IsArg */
		    matches = 1;
		    foundOption = i;
		    break;
		}
	    }
	    else if (!argchar) {
		/* may be an abbreviation for this option */
		matches++;
		foundOption = i;
	    }
	    else if (table_is_sorted == Sorted && optchar > argchar) {
		break;
	    }
	    if (table_is_sorted == Check && i > 0 &&
		strcmp(options[i].option, options[i-1].option) < 0) {
		table_is_sorted = NotSorted;
	    }
	}
	if (table_is_sorted == Check && i >= (num_options-1))
	    table_is_sorted = Sorted;
	if (matches == 1) {
		i = foundOption;
		switch (options[i].argKind){
		  /* So far only 3 are used... No, Sep, Res */
		case XrmoptionNoArg:
#if 0
		    --(*arg_c);
#endif
		    PutCommandResource(options[i].value);
		    break;

		case XrmoptionIsArg:
#if 0
		    --(*arg_c);
#endif
		    PutCommandResource(*arg_v);
		    break;

		case XrmoptionStickyArg:
#if 0
		    --(*arg_c);
#endif
		    PutCommandResource(argP);
		    break;

		case XrmoptionSepArg:
		    if (myargc > 1) {
			++arg_v; --myargc;
#if 0
			 --(*arg_c); --(*arg_c);
#endif
			PutCommandResource(*arg_v);
		    }
#if 0
		else
			(*argsave++) = (*arg_v);
#endif
		    break;

		case XrmoptionResArg:
		    if (myargc > 1) {
			++arg_v; --myargc;
#if 0
			 --(*arg_c); --(*arg_c);
#endif
			XrmPutLineResource(pdb, *arg_v);
		    }
#if 0
		else
			(*argsave++) = (*arg_v);
#endif
		    break;

		case XrmoptionSkipArg:
		    if (myargc > 1) {
			--myargc;
#if 0
			(*argsave++) = (*arg_v++);
#endif
		    }
#if 0
		    (*argsave++) = (*arg_v);
#endif
		    break;

		case XrmoptionSkipLine:
#if 0
		    for (; myargc > 0; myargc--)
			(*argsave++) = (*arg_v++);
#endif
		    break;

		case XrmoptionSkipNArgs:
		    {
			register int j = 1 + (int) options[i].value;

			if (j > myargc) j = myargc;
			for (; j > 0; j--) {
#if 0
			    (*argsave++) = (*arg_v++);
#endif
			    myargc--;
			}
			arg_v--;		/* went one too far before */
			myargc++;
		    }
		    break;

		default:
		    _XReportParseError (&options[i], (char *) "unknown kind");
		    break;
		}
	}
#if 0
	else
	    (*argsave++) = (*arg_v);  /*compress arglist*/
#endif
    }

#if 0
    if (argsave < argend)
	(*argsave)=NULL; /* put NULL terminator on compressed arg_v */
#endif
   (*arg_v) = (*argsave);  /*return unmotified arglist*/
#endif /* !WIN32 */
}
