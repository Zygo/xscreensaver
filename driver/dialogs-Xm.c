/* dialogs-Xm.c --- Motif widgets for demo, options, and password dialogs.
 * xscreensaver, Copyright (c) 1993-1998 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* The code in this file started off its life as the output of XDesigner,
   but I've since hacked it by hand...  It's a mess, avert your eyes.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>

#include <Xm/Xm.h>
#include <Xm/DialogS.h>
#include <Xm/DrawnB.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/ScrollBar.h>
#include <Xm/Separator.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>

#include <Xm/SelectioB.h>

#include <stdio.h>

#include "visual.h"	/* for visual_depth() */

Widget resources_dialog;
Widget resources_form;
Widget timeout_text;
Widget cycle_text;
Widget fade_text;
Widget ticks_text;
Widget lock_time_text;
Widget passwd_time_text;
Widget verbose_toggle;
Widget cmap_toggle;
Widget fade_toggle;
Widget unfade_toggle;
Widget lock_toggle;
Widget res_done;
Widget res_cancel;

Widget demo_dialog;
Widget demo_form;
Widget label1;
Widget label2;
Widget text_area;
Widget demo_list;
Widget text_line;
Widget vline;
Widget next;
Widget prev;
Widget edit;
Widget done;
Widget restart;
Widget spacer;

void
create_resources_dialog(Widget parent, Visual *visual, Colormap colormap)
{
  Widget children[22];      /* Children to manage */
  Arg al[64];           /* Arg List */
  register int ac = 0;      /* Arg Count */
  Widget widget12;
  Widget widget13;
  Widget widget14;
  Widget widget15;
  Widget widget16;
  Widget widget17;
  Widget widget18;
  Widget widget48;
  Widget widget29;

  Widget real_dialog;
  Widget w;

  ac = 0;
  XtSetArg (al[ac], XmNvisual, visual); ac++;
  XtSetArg (al[ac], XmNcolormap, colormap); ac++;
  XtSetArg (al[ac], XmNdepth, visual_depth(XtScreen(parent), visual)); ac++;

  real_dialog = XmCreatePromptDialog (parent, "resourcesForm", al, ac);
  resources_dialog = XtParent(real_dialog);

  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_SEPARATOR);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_TEXT);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_SELECTION_LABEL);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_HELP_BUTTON);
  if (w) XtUnmanageChild (w);

  ac = 0;
  XtSetArg (al [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (al [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (al [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (al [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  resources_form = XmCreateForm (real_dialog, "form", al, ac);
  XtManageChild (resources_form);

  ac = 0;

  widget12 = XmCreateLabel ( resources_form, "resourcesLabel", al, ac );
  widget13 = XmCreateSeparator ( resources_form, "separator", al, ac );
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_END); ac++;
  widget14 = XmCreateLabel ( resources_form, "timeoutLabel", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_END); ac++;
  widget15 = XmCreateLabel ( resources_form, "cycleLabel", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_END); ac++;
  widget16 = XmCreateLabel ( resources_form, "fadeSecondsLabel", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_END); ac++;
  widget17 = XmCreateLabel ( resources_form, "fadeTicksLabel", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_END); ac++;
  widget18 = XmCreateLabel ( resources_form, "lockLabel", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_END); ac++;
  widget48 = XmCreateLabel ( resources_form, "passwdLabel", al, ac );
  ac = 0;
  timeout_text = XmCreateTextField ( resources_form, "timeoutText", al, ac );
  cycle_text = XmCreateTextField ( resources_form, "cycleText", al, ac );
  fade_text = XmCreateTextField ( resources_form, "fadeSecondsText", al, ac );
  ticks_text = XmCreateTextField ( resources_form, "fadeTicksText", al, ac );
  lock_time_text = XmCreateTextField ( resources_form, "passwdText", al, ac );
  passwd_time_text = XmCreateTextField ( resources_form, "lockText", al, ac );
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  verbose_toggle = XmCreateToggleButton ( resources_form, "verboseToggle", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  cmap_toggle = XmCreateToggleButton ( resources_form, "cmapToggle", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  fade_toggle = XmCreateToggleButton ( resources_form, "fadeToggle", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  unfade_toggle = XmCreateToggleButton ( resources_form, "unfadeToggle", al, ac );
  ac = 0;
  XtSetArg(al[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  lock_toggle = XmCreateToggleButton ( resources_form, "lockToggle", al, ac );
  ac = 0;
  widget29 = XmCreateSeparator ( resources_form, "separator", al, ac );

  res_done = XmSelectionBoxGetChild (real_dialog, XmDIALOG_OK_BUTTON);
  res_cancel = XmSelectionBoxGetChild (real_dialog, XmDIALOG_CANCEL_BUTTON);

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNtopOffset, 4); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetValues ( widget12,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 4); ac++;
  XtSetArg(al[ac], XmNtopWidget, widget12); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 0); ac++;
  XtSetValues ( widget13,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 4); ac++;
  XtSetArg(al[ac], XmNtopWidget, widget13); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomWidget, timeout_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 20); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightWidget, timeout_text); ac++;
  XtSetValues ( widget14,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, cycle_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, cycle_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 20); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightWidget, cycle_text); ac++;
  XtSetValues ( widget15,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, fade_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, fade_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 20); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightWidget, fade_text); ac++;
  XtSetValues ( widget16,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, ticks_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, ticks_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 20); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightWidget, ticks_text); ac++;
  XtSetValues ( widget17,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, lock_time_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, lock_time_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 19); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightWidget, lock_time_text); ac++;
  XtSetValues ( widget18,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, passwd_time_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, passwd_time_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 14); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightWidget, passwd_time_text); ac++;
  XtSetValues ( widget48,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 4); ac++;
  XtSetArg(al[ac], XmNtopWidget, widget13); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 141); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
  XtSetValues ( timeout_text,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 2); ac++;
  XtSetArg(al[ac], XmNtopWidget, timeout_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, timeout_text); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
  XtSetValues ( cycle_text,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 2); ac++;
  XtSetArg(al[ac], XmNtopWidget, cycle_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, cycle_text); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
  XtSetValues ( fade_text,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 2); ac++;
  XtSetArg(al[ac], XmNtopWidget, fade_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, fade_text); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
  XtSetValues ( ticks_text,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 2); ac++;
  XtSetArg(al[ac], XmNtopWidget, ticks_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, ticks_text); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
  XtSetValues ( lock_time_text,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 4); ac++;
  XtSetArg(al[ac], XmNtopWidget, lock_time_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, lock_time_text); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
  XtSetValues ( passwd_time_text,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 4); ac++;
  XtSetArg(al[ac], XmNtopWidget, widget13); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, timeout_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 20); ac++;
  XtSetArg(al[ac], XmNleftWidget, timeout_text); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 20); ac++;
  XtSetValues ( verbose_toggle,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, cycle_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, cycle_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, verbose_toggle); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 20); ac++;
  XtSetValues ( cmap_toggle,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, fade_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, fade_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, cmap_toggle); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 20); ac++;
  XtSetValues ( fade_toggle,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, ticks_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, ticks_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, fade_toggle); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 20); ac++;
  XtSetValues ( unfade_toggle,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, lock_time_text); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 0); ac++;
  XtSetArg(al[ac], XmNbottomWidget, lock_time_text); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
  XtSetArg(al[ac], XmNleftOffset, 0); ac++;
  XtSetArg(al[ac], XmNleftWidget, unfade_toggle); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 20); ac++;
  XtSetValues ( lock_toggle,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 0); ac++;
  XtSetArg(al[ac], XmNtopWidget, passwd_time_text); ac++;

  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNbottomOffset, 4); ac++;

  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetValues ( widget29,al, ac );
  ac = 0;



  ac = 0;
  children[ac++] = widget12;
  children[ac++] = widget13;
  children[ac++] = widget14;
  children[ac++] = widget15;
  children[ac++] = widget16;
  children[ac++] = widget17;
  children[ac++] = widget18;
  children[ac++] = widget48;
  children[ac++] = timeout_text;
  children[ac++] = cycle_text;
  children[ac++] = fade_text;
  children[ac++] = ticks_text;
  children[ac++] = lock_time_text;
  children[ac++] = passwd_time_text;
  children[ac++] = verbose_toggle;
  children[ac++] = cmap_toggle;
  children[ac++] = fade_toggle;
  children[ac++] = unfade_toggle;
  children[ac++] = lock_toggle;
  children[ac++] = widget29;

  XtManageChildren(children, ac);
  ac = 0;

  resources_form = real_dialog;
}


void
create_demo_dialog(Widget parent, Visual *visual, Colormap colormap)
{
  Arg al[64];           /* Arg List */
  register int ac = 0;      /* Arg Count */

  Widget real_dialog;
  Widget w;


  ac = 0;
  XtSetArg (al[ac], XmNvisual, visual); ac++;
  XtSetArg (al[ac], XmNcolormap, colormap); ac++;
  XtSetArg (al[ac], XmNdepth, visual_depth(XtScreen(parent), visual)); ac++;


  real_dialog = XmCreatePromptDialog (parent, "demoForm", al, ac);
  demo_dialog = XtParent(real_dialog);

  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_SEPARATOR);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_TEXT);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_SELECTION_LABEL);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_OK_BUTTON);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_CANCEL_BUTTON);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (real_dialog, XmDIALOG_HELP_BUTTON);
  if (w) XtUnmanageChild (w);

  ac = 0;
  XtSetArg (al [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (al [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (al [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (al [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  demo_form = XmCreateForm (real_dialog, "form", al, ac);
  XtManageChild (demo_form);

  label1 = XmCreateLabel ( demo_form, "label1", al, ac );
  label2 = XmCreateLabel ( demo_form, "label2", al, ac );
  demo_list = XmCreateScrolledList ( demo_form, "demoList", al, ac );
  text_area = XtParent ( demo_list );

  ac = 0;
  text_line = XmSelectionBoxGetChild (real_dialog, XmDIALOG_TEXT);
  XtManageChild(text_line);

  /* #### ARRGH!  This is apparently the only way to make hitting return in
     the text field not *ALSO* activate the most-recently-selected button!

     This has the unfortunate side effect of making the buttons not be
     keyboard-traversable, but that's less bad than not being able to try
     out new switches by typing them into the text field.

     XmSelectionBox(3M) says in the "Additional Behavior" section:
	  KActivate:
		    Calls the activate callbacks for the button with
		    the keyboard focus.  [... ]  In a List widget or
		    single-line Text widget, the List or Text action
		    associated with KActivate is called before the
		    SelectionBox actions associated with KActivate."

     So they take it as a given that when running activateCallback on a single-
     line Text widget, you'll also want to run activateCallback on whatever the
     currently-focussed button is as well!  Morons!  Villains!  Shitheads!

     (Perhaps there's some way to override XmSelectionBox's KActivate behavior.
     I doubt it, but if there is, I don't know it.)
  */
  ac = 0;
  XtSetArg(al[ac], XmNtraversalOn, False); ac++;

  next = XmCreatePushButton ( real_dialog, "next", al, ac );
  prev = XmCreatePushButton ( real_dialog, "prev", al, ac );
  edit = XmCreatePushButton ( real_dialog, "edit", al, ac );
  done = XmCreatePushButton ( real_dialog, "done", al, ac );
  restart = XmCreatePushButton ( real_dialog, "restart", al, ac );
  XtManageChild(next);
  XtManageChild(prev);
  XtManageChild(edit);
  XtManageChild(done);
  XtManageChild(restart);

  ac = 0;
  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNtopOffset, 5); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetValues ( label1,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 4); ac++;
  XtSetArg(al[ac], XmNtopWidget, label1); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetValues ( label2,al, ac );
  ac = 0;

  XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(al[ac], XmNtopOffset, 4); ac++;
  XtSetArg(al[ac], XmNtopWidget, label2); ac++;
  XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNleftOffset, 4); ac++;
  XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(al[ac], XmNrightOffset, 4); ac++;
  XtSetValues ( text_area,al, ac );

  XtManageChild(demo_list);
  XtManageChild(label1);
  XtManageChild(label2);

  demo_form = real_dialog;
}
