/*    xscreensaver, Copyright (c) 1993-1996 Jamie Zawinski <jwz@netscape.com>
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


#if !__STDC__
# define _NO_PROTO
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

extern Visual *visual;
extern int visual_depth;
extern Colormap cmap;


Widget passwd_dialog;
Widget passwd_form;
Widget roger_label;
Widget passwd_label1;
Widget passwd_label3;
Widget passwd_text;
Widget passwd_done;
Widget passwd_cancel;

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
create_passwd_dialog( parent )
Widget parent;
{
  Widget shell;
  Widget  form1;
  Widget   roger;
  Widget   dialog;
  Widget    form2;
  Widget     label1, label2, label3;
  Widget    text;
  Widget    ok, cancel;
  Widget w;

  shell = XmCreateDialogShell (parent, "passwdDialog", 0, 0);

  form1 = XmCreateForm (shell, "form", 0, 0);

  roger = XmCreateDrawnButton (form1, "rogerLabel", 0, 0);

  dialog = XmCreateSelectionBox (form1, "passwdForm", 0, 0);

  form2 = XmCreateForm ( dialog, "form", 0, 0);
  label1 = XmCreateLabel ( form2, "passwdLabel1", 0, 0);
  label2 = XmCreateLabel ( form2, "passwdLabel2", 0, 0);
  label3 = XmCreateLabel ( form2, "passwdLabel3", 0, 0);

  text = XmSelectionBoxGetChild (dialog, XmDIALOG_TEXT);
  ok = XmSelectionBoxGetChild (dialog, XmDIALOG_OK_BUTTON);
  cancel = XmSelectionBoxGetChild (dialog, XmDIALOG_CANCEL_BUTTON);

  w = XmSelectionBoxGetChild (dialog, XmDIALOG_LIST_LABEL);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (dialog, XmDIALOG_LIST);
  if (w) XtUnmanageChild (XtParent(w));
  w = XmSelectionBoxGetChild (dialog, XmDIALOG_SELECTION_LABEL);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (dialog, XmDIALOG_SEPARATOR);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (dialog, XmDIALOG_APPLY_BUTTON);
  if (w) XtUnmanageChild (w);
  w = XmSelectionBoxGetChild (dialog, XmDIALOG_HELP_BUTTON);
  if (w) XtUnmanageChild (w);

  XtVaSetValues(label1,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_NONE,
		0);
  XtVaSetValues(label2,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, label1,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_NONE,
		0);
  XtVaSetValues(label3,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, label2,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		0);

  XtVaSetValues(roger,
		XmNsensitive, FALSE,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_NONE,
		XmNbottomAttachment, XmATTACH_FORM,
		0);
  XtVaSetValues(dialog,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, roger,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		0);

  XtManageChild(label1);
  XtManageChild(label2);
  XtManageChild(label3);

  XtManageChild(form2);
  XtManageChild(text);
  XtManageChild(ok);
  XtManageChild(cancel);

  XtManageChild(roger);
  XtManageChild(dialog);

  {
    Dimension w = 0, h = 0;
    XtRealizeWidget(form1);
    XtVaGetValues(roger, XmNwidth, &w, XmNheight, &h, 0);
    if (w == h)
      ;
    else if (w > h)
      XtVaSetValues(roger, XmNwidth, w, XmNheight, w, 0);
    else
      XtVaSetValues(roger, XmNwidth, h, XmNheight, h, 0);
  }

  passwd_dialog = shell;
  passwd_form = form1;
  roger_label = roger;
  passwd_label1 = label1;
  passwd_label3 = label3;
  passwd_text = text;
  passwd_done = ok;
  passwd_cancel = cancel;
}



void
create_resources_dialog( parent )
Widget parent;
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
  XtSetArg (al[ac], XmNcolormap, cmap); ac++;
  XtSetArg (al[ac], XmNdepth, visual_depth); ac++;

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
  widget13 = XmCreateSeparator ( resources_form, "widget13", al, ac );
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
  widget29 = XmCreateSeparator ( resources_form, "widget29", al, ac );

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
create_demo_dialog( parent )
Widget parent;
{
  Widget children[11];      /* Children to manage */
  Arg al[64];           /* Arg List */
  register int ac = 0;      /* Arg Count */
  XmString xmstrings[15];    /* temporary storage for XmStrings */

  Widget real_dialog;
  Widget w;


  ac = 0;
  XtSetArg (al[ac], XmNvisual, visual); ac++;
  XtSetArg (al[ac], XmNcolormap, cmap); ac++;
  XtSetArg (al[ac], XmNdepth, visual_depth); ac++;


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
