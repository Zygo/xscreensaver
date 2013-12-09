/* demo-Xm.c --- implements the interactive demo-mode and options dialogs.
 * xscreensaver, Copyright (c) 1999, 2003 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#include <X11/IntrinsicP.h>  /* just for debug info */
#include <X11/ShellP.h>

#include <X11/Shell.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/ScrolledW.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/CascadeBG.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h>
#include <Xm/SeparatoG.h>
#include <Xm/SelectioB.h>

#ifdef HAVE_XMCOMBOBOX		/* a Motif 2.0 widget */
# include <Xm/ComboBox.h>
# ifndef XmNtextField		/* Lesstif 0.89.4 bug */
#  undef HAVE_XMCOMBOBOX
# endif
#endif /* HAVE_XMCOMBOBOX */

#include <stdio.h>
#include <stdlib.h>



const char *visual_menu[] = {
  "Any", "Best", "Default", "Default-N", "GL", "TrueColor", "PseudoColor",
  "StaticGray", "GrayScale", "DirectColor", "Color", "Gray", "Mono", 0 
};



static Widget create_demos_page (Widget parent);
static Widget create_options_page (Widget parent);

static void
tab_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  Widget parent = XtParent(button);
  Widget tabber = XtNameToWidget (parent, "*folder");
  Widget this_tab = (Widget) client_data;
  Widget *kids = 0;
  Cardinal nkids = 0;
  if (!tabber) abort();
  
  XtVaGetValues (tabber, XmNnumChildren, &nkids, XmNchildren, &kids, NULL);
  if (!kids) abort();
  if (nkids > 0)
    XtUnmanageChildren (kids, nkids);

  XtManageChild (this_tab);
}


Widget
create_xscreensaver_demo (Widget parent)
{
  /* MainWindow
       Form
         Menubar
         DemoTab
         OptionsTab
         HR
         Tabber
           (demo page)
           (options page)
   */

  Widget mainw, form, menubar;
  Widget demo_tab, options_tab, hr, tabber, demos, options;
  Arg av[100];
  int ac = 0;

  mainw = XmCreateMainWindow (parent, "demoForm", av, ac);
  form = XmCreateForm (mainw, "form", av, ac);
  menubar = XmCreateSimpleMenuBar (form, "menubar", av, ac);
  XtVaSetValues (menubar,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 NULL);

  {
    Widget menu = 0, item = 0;
    char *menus[] = {
      "*file", "blank", "lock", "kill", "restart", "-", "exit",
      "*edit", "cut", "copy", "paste",
      "*help", "about", "docMenu" };
    int i;
    for (i = 0; i < sizeof(menus)/sizeof(*menus); i++)
      {
        ac = 0;
        if (menus[i][0] == '-')
          item = XmCreateSeparatorGadget (menu, "separator", av, ac);
        else if (menus[i][0] != '*')
          item = XmCreatePushButtonGadget (menu, menus[i], av, ac);
        else
          {
            menu = XmCreatePulldownMenu (parent, menus[i]+1, av, ac);
            XtSetArg (av [ac], XmNsubMenuId, menu); ac++;
            item = XmCreateCascadeButtonGadget (menubar, menus[i]+1, av, ac);

            if (!strcmp (menus[i]+1, "help"))
              XtVaSetValues(menubar, XmNmenuHelpWidget, item, NULL);
          }
        XtManageChild (item);
      }
    ac = 0;
  }

  demo_tab = XmCreatePushButtonGadget (form, "demoTab", av, ac);
  XtVaSetValues (demo_tab,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNtopAttachment, XmATTACH_WIDGET,
                 XmNtopWidget, menubar,
                 NULL);

  options_tab = XmCreatePushButtonGadget (form, "optionsTab", av, ac);
  XtVaSetValues (options_tab,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftWidget, demo_tab,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopWidget, demo_tab,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomWidget, demo_tab,
                 NULL);

  hr = XmCreateSeparatorGadget (form, "hr", av, ac);
  XtVaSetValues (hr,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNtopAttachment, XmATTACH_WIDGET,
                 XmNtopWidget, demo_tab,
                 NULL);

  tabber = XmCreateForm (form, "folder", av, ac);
  XtVaSetValues (tabber,
                 XmNtopAttachment, XmATTACH_WIDGET,
                 XmNtopWidget, hr,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);

  demos = create_demos_page (tabber);
  options = create_options_page (tabber);

  XtAddCallback (demo_tab, XmNactivateCallback, tab_cb, demos);
  XtAddCallback (options_tab, XmNactivateCallback, tab_cb, options);

  XtManageChild (demos);
  XtManageChild (options);

  XtManageChild (demo_tab);
  XtManageChild (options_tab);
  XtManageChild (hr);
  XtManageChild (menubar);
  XtManageChild (tabber);
  XtManageChild (form);

#if 1
  XtUnmanageChild (options);
  XtManageChild (demos);
#endif

  return mainw;
}


static Widget
create_demos_page (Widget parent)
{
  /* Form1 (horizontal)
       Form2 (vertical)
         Scroller
           List
         ButtonBox1 (vertical)
           Button ("Down")
           Button ("Up")
       Form3 (vertical)
         Frame
           Label
           TextArea (doc)
         Label
         Text ("Command Line")
         Form4 (horizontal)
           Checkbox ("Enabled")
           Label ("Visual")
           ComboBox
         HR
         ButtonBox2 (vertical)
           Button ("Demo")
           Button ("Documentation")
   */
  Widget form1, form2, form3, form4;
  Widget scroller, list, buttonbox1, down, up;
  Widget frame, frame_label, doc, cmd_label, cmd_text, enabled, vis_label;
  Widget   combo;
  Widget hr, buttonbox2, demo, man;
  Arg av[100];
  int ac = 0;
  int i;

  form1 = XmCreateForm (parent, "form1", av, ac);
  form2 = XmCreateForm (form1, "form2", av, ac);
  XtVaSetValues (form2,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);

  scroller = XmCreateScrolledWindow (form2, "scroller", av, ac);
  XtVaSetValues (scroller,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_WIDGET,
                 NULL);
  list = XmCreateList (scroller, "list", av, ac);

  buttonbox1 = XmCreateForm (form2, "buttonbox1", av, ac);
  XtVaSetValues (buttonbox1,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
  XtVaSetValues (scroller, XmNbottomWidget, buttonbox1, NULL);

  down = XmCreatePushButton (buttonbox1, "down", av, ac);
  XtVaSetValues (down,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
  up = XmCreatePushButton (buttonbox1, "up", av, ac);
  XtVaSetValues (up,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftWidget, down,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);

  form3 = XmCreateForm (form1, "form3", av, ac);
  XtVaSetValues (form3,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftWidget, form2,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
  frame = XmCreateFrame (form3, "frame", av, ac);

  ac = 0;
  XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
  frame_label = XmCreateLabelGadget (frame, "frameLabel", av, ac);

  ac = 0;
  XtVaSetValues (frame,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_WIDGET,
                 NULL);

  ac = 0;
  XtSetArg (av [ac], XmNchildType, XmFRAME_WORKAREA_CHILD); ac++;
  doc = XmCreateText (frame, "doc", av, ac);

  ac = 0;
  XtVaSetValues (doc,
                 XmNeditable, FALSE,
                 XmNcursorPositionVisible, FALSE,
                 XmNwordWrap, TRUE,
                 XmNeditMode, XmMULTI_LINE_EDIT,
                 XmNshadowThickness, 0,
                 NULL);

  cmd_label = XmCreateLabelGadget (form3, "cmdLabel", av, ac);
  XtVaSetValues (cmd_label,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_WIDGET,
                 NULL);
  XtVaSetValues (frame, XmNbottomWidget, cmd_label, NULL);

  cmd_text = XmCreateTextField (form3, "cmdText", av, ac);
  XtVaSetValues (cmd_text,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_WIDGET,
                 NULL);
  XtVaSetValues (cmd_label, XmNbottomWidget, cmd_text, NULL);

  form4 = XmCreateForm (form3, "form4", av, ac);
  XtVaSetValues (form4,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_WIDGET,
                 NULL);
  XtVaSetValues (cmd_text, XmNbottomWidget, form4, NULL);

  enabled = XmCreateToggleButtonGadget (form4, "enabled", av, ac);
  XtVaSetValues (enabled,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
  vis_label = XmCreateLabelGadget (form4, "visLabel", av, ac);
  XtVaSetValues (vis_label,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftWidget, enabled,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
#ifdef HAVE_XMCOMBOBOX
  {
    Widget list;
    ac = 0;
    XtSetArg (av [ac], XmNcomboBoxType, XmDROP_DOWN_COMBO_BOX); ac++;
    combo = XmCreateComboBox (form4, "combo", av, ac);
    for (i = 0; visual_menu[i]; i++)
      {
        XmString xs = XmStringCreate ((char *) visual_menu[i],
                                      XmSTRING_DEFAULT_CHARSET);
        XmComboBoxAddItem (combo, xs, 0, False);
        XmStringFree (xs);
      }
    XtVaGetValues (combo, XmNlist, &list, NULL);
    XtVaSetValues (list, XmNvisibleItemCount, i, NULL);
  }
#else /* !HAVE_XMCOMBOBOX */
  {
    Widget popup_menu = XmCreatePulldownMenu (parent, "menu", av, ac);
    Widget kids[100];
    for (i = 0; visual_menu[i]; i++)
      {
        XmString xs = XmStringCreate ((char *) visual_menu[i],
                                      XmSTRING_DEFAULT_CHARSET);
        ac = 0;
        XtSetArg (av [ac], XmNlabelString, xs); ac++;
        kids[i] = XmCreatePushButtonGadget (popup_menu, "button", av, ac);
        /* XtAddCallback (combo, XmNactivateCallback, visual_popup_cb,
           combo); */
        XmStringFree (xs);
      }
    XtManageChildren (kids, i);

    ac = 0;
    XtSetArg (av [ac], XmNsubMenuId, popup_menu); ac++;
    combo = XmCreateOptionMenu (form4, "combo", av, ac);
    ac = 0;
  }
#endif /* !HAVE_XMCOMBOBOX */

  XtVaSetValues (combo,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftWidget, vis_label,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);

  hr = XmCreateSeparatorGadget (form3, "hr", av, ac);
  XtVaSetValues (hr,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_WIDGET,
                 NULL);
  XtVaSetValues (form4, XmNbottomWidget, hr, NULL);

  buttonbox2 = XmCreateForm (form3, "buttonbox2", av, ac);
  XtVaSetValues (buttonbox2,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
  XtVaSetValues (hr, XmNbottomWidget, buttonbox2, NULL);

  demo = XmCreatePushButtonGadget (buttonbox2, "demo", av, ac);
  XtVaSetValues (demo,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
  man = XmCreatePushButtonGadget (buttonbox2, "man", av, ac);
  XtVaSetValues (man,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);

  XtManageChild (demo);
  XtManageChild (man);
  XtManageChild (buttonbox2);
  XtManageChild (hr);

  XtManageChild (combo);
  XtManageChild (vis_label);
  XtManageChild (enabled);
  XtManageChild (form4);

  XtManageChild (cmd_text);
  XtManageChild (cmd_label);

  XtManageChild (doc);
  XtManageChild (frame_label);
  XtManageChild (frame);
  XtManageChild (form3);

  XtManageChild (up);
  XtManageChild (down);
  XtManageChild (buttonbox1);

  XtManageChild (list);
  XtManageChild (scroller);
  XtManageChild (form2);

  XtManageChild (form1);

  XtVaSetValues (form1,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);

  return form1;
}



static Widget
create_options_page (Widget parent)
{
  /* This is what the layout is today:

     Form (horizontal)
       Label ("Saver Timeout")
       Label ("Cycle Timeout")
       Label ("Fade Duration")
       Label ("Fade Ticks")
       Label ("Lock Timeout")
       Label ("Password Timeout")

       Text (timeout)
       Text (cycle)
       Text (fade seconds)
       Text (fade ticks)
       Text (lock)
       Text (passwd)

       Toggle ("Verbose")
       Toggle ("Install Colormap")
       Toggle ("Fade Colormap")
       Toggle ("Unfade Colormap")
       Toggle ("Require Password")

       HR
       Button ("OK")
       Button ("Cancel")
   */

  /* This is what it should be:

     Form (horizontal)
       Form (vertical) ("column1")
         Frame
           Label ("Blanking and Locking")
           Form
             Label ("Blank After")
             Label ("Cycle After")
             Text ("Blank After")
             Text ("Cycle After")
             HR
             Checkbox ("Require Password")
             Label ("Lock After")
             Text ("Lock After")
         Frame
           Label ("Image Manipulation")
           Form
             Checkbox ("Grab Desktop Images")
             Checkbox ("Grab Video Frames")
             Checkbox ("Choose Random Image")
             Text (pathname)
             Button ("Browse")
         Frame
           Label ("Diagnostics")
           Form
             Checkbox ("Verbose Diagnostics")
             Checkbox ("Display Subprocess Errors")
             Checkbox ("Display Splash Screen at Startup")
       Form (vertical) ("column2")
         Frame
           Label ("Display Power Management")
           Form
             Checkbox ("Power Management Enabled")
             Label ("Standby After")
             Label ("Suspend After")
             Label ("Off After")
             Text ("Standby After")
             Text ("Suspend After")
             Text ("Off After")
         Frame
           Label ("Colormaps")
           Form
             Checkbox ("Install Colormap")
             HR
             Checkbox ("Fade To Black When Blanking")
             Checkbox ("Fade From Black When Unblanking")
             Label ("Fade Duration")
             Text ("Fade Duration")

       timeoutLabel
       cycleLabel
       fadeSecondsLabel
       fadeTicksLabel
       lockLabel
       passwdLabel

       timeoutText
       cycleText
       fadeSecondsText
       fadeTicksText
       lockText
       passwdText

       verboseToggle
       cmapToggle
       fadeToggle
       unfadeToggle
       lockToggle

       separator
       OK
       Cancel
   */



  Arg av[64];
  int ac = 0;
  Widget children[100];
  Widget timeout_label, cycle_label, fade_seconds_label, fade_ticks_label;
  Widget lock_label, passwd_label, hr;
  Widget preferences_form;

  Widget timeout_text, cycle_text, fade_text, fade_ticks_text;
  Widget lock_timeout_text, passwd_timeout_text, verbose_toggle;
  Widget install_cmap_toggle, fade_toggle, unfade_toggle;
  Widget lock_toggle, prefs_done, prefs_cancel;

  ac = 0;
  XtSetArg (av [ac], XmNdialogType, XmDIALOG_PROMPT); ac++;

  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  preferences_form = XmCreateForm (parent, "preferencesForm", av, ac);
  XtManageChild (preferences_form);

  ac = 0;

  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
  timeout_label = XmCreateLabelGadget (preferences_form, "timeoutLabel",
                                       av, ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
  cycle_label = XmCreateLabelGadget (preferences_form, "cycleLabel",
                                     av, ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
  fade_seconds_label = XmCreateLabelGadget (preferences_form,
                                            "fadeSecondsLabel", av, ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
  fade_ticks_label = XmCreateLabelGadget (preferences_form, "fadeTicksLabel",
                                          av, ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
  lock_label = XmCreateLabelGadget (preferences_form, "lockLabel", av, ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
  passwd_label = XmCreateLabelGadget (preferences_form, "passwdLabel", av, ac);
  ac = 0;
  timeout_text = XmCreateTextField (preferences_form, "timeoutText", av, ac);
  cycle_text = XmCreateTextField (preferences_form, "cycleText", av, ac);
  fade_text = XmCreateTextField (preferences_form, "fadeSecondsText", av, ac);
  fade_ticks_text = XmCreateTextField (preferences_form, "fadeTicksText",
                                       av, ac);
  lock_timeout_text = XmCreateTextField (preferences_form, "lockText",
                                         av, ac);
  passwd_timeout_text = XmCreateTextField (preferences_form, "passwdText",
                                           av, ac);
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  verbose_toggle = XmCreateToggleButtonGadget (preferences_form,
                                               "verboseToggle", av, ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  install_cmap_toggle = XmCreateToggleButtonGadget (preferences_form,
                                                    "cmapToggle", av, ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  fade_toggle = XmCreateToggleButtonGadget (preferences_form, "fadeToggle",
                                            av, ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  unfade_toggle = XmCreateToggleButtonGadget (preferences_form, "unfadeToggle",
                                              av,ac);
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  lock_toggle = XmCreateToggleButtonGadget (preferences_form, "lockToggle",
                                            av, ac);
  ac = 0;
  hr = XmCreateSeparatorGadget (preferences_form, "separator", av, ac);

  prefs_done = XmCreatePushButtonGadget (preferences_form, "OK", av, ac);
  prefs_cancel = XmCreatePushButtonGadget (preferences_form, "Cancel", av, ac);

  XtVaSetValues (timeout_label,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNtopOffset, 4,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomWidget, timeout_text,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNleftOffset, 20,
                 XmNrightAttachment, XmATTACH_WIDGET,
                 XmNrightOffset, 4,
                 XmNrightWidget, timeout_text,
                 NULL);

  XtVaSetValues (cycle_label,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, cycle_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, cycle_text,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNleftOffset, 20,
                 XmNrightAttachment, XmATTACH_WIDGET,
                 XmNrightOffset, 4,
                 XmNrightWidget, cycle_text,
                 NULL);

  XtVaSetValues (fade_seconds_label,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, fade_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, fade_text,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNleftOffset, 20,
                 XmNrightAttachment, XmATTACH_WIDGET,
                 XmNrightOffset, 4,
                 XmNrightWidget, fade_text,
                 NULL);

  XtVaSetValues (fade_ticks_label,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, fade_ticks_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, fade_ticks_text,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNleftOffset, 20,
                 XmNrightAttachment, XmATTACH_WIDGET,
                 XmNrightOffset, 4,
                 XmNrightWidget, fade_ticks_text,
                 NULL);

  XtVaSetValues (lock_label,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, lock_timeout_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, lock_timeout_text,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNleftOffset, 19,
                 XmNrightAttachment, XmATTACH_WIDGET,
                 XmNrightOffset, 4,
                 XmNrightWidget, lock_timeout_text,
                 NULL);

  XtVaSetValues (passwd_label,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, passwd_timeout_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, passwd_timeout_text,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNleftOffset, 14,
                 XmNrightAttachment, XmATTACH_WIDGET,
                 XmNrightOffset, 4,
                 XmNrightWidget, passwd_timeout_text,
                 NULL);

  XtVaSetValues (timeout_text,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNtopOffset, 4,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNleftOffset, 141,
                 NULL);

  XtVaSetValues (cycle_text,
                 XmNtopAttachment, XmATTACH_WIDGET,
                 XmNtopOffset, 2,
                 XmNtopWidget, timeout_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, timeout_text,
                 NULL);

  XtVaSetValues (fade_text,
                 XmNtopAttachment, XmATTACH_WIDGET,
                 XmNtopOffset, 2,
                 XmNtopWidget, cycle_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, cycle_text,
                 NULL);

  XtVaSetValues (fade_ticks_text,
                 XmNtopAttachment, XmATTACH_WIDGET,
                 XmNtopOffset, 2,
                 XmNtopWidget, fade_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, fade_text,
                 NULL);

  XtVaSetValues (lock_timeout_text,
                 XmNtopAttachment, XmATTACH_WIDGET,
                 XmNtopOffset, 2,
                 XmNtopWidget, fade_ticks_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, fade_ticks_text,
                 NULL);

  XtVaSetValues (passwd_timeout_text,
                 XmNtopAttachment, XmATTACH_WIDGET,
                 XmNtopOffset, 4,
                 XmNtopWidget, lock_timeout_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, lock_timeout_text,
                 NULL);

  XtVaSetValues (verbose_toggle,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNtopOffset, 4,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, timeout_text,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftOffset, 20,
                 XmNleftWidget, timeout_text,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNrightOffset, 20,
                 NULL);

  XtVaSetValues (install_cmap_toggle,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, cycle_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, cycle_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, verbose_toggle,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNrightOffset, 20,
                 NULL);

  XtVaSetValues (fade_toggle,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, fade_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, fade_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, install_cmap_toggle,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNrightOffset, 20,
                 NULL);

  XtVaSetValues (unfade_toggle,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, fade_ticks_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, fade_ticks_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, fade_toggle,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNrightOffset, 20,
                 NULL);

  XtVaSetValues (lock_toggle,
                 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNtopOffset, 0,
                 XmNtopWidget, lock_timeout_text,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomOffset, 0,
                 XmNbottomWidget, lock_timeout_text,
                 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNleftOffset, 0,
                 XmNleftWidget, unfade_toggle,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNrightOffset, 20,
                 NULL);

  XtVaSetValues (hr,
                 XmNtopWidget, passwd_timeout_text,
                 XmNbottomAttachment, XmATTACH_FORM,
                 XmNbottomOffset, 4,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 NULL);

  XtVaSetValues (prefs_done,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
  XtVaSetValues (prefs_cancel,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);
  XtVaSetValues (hr,
                 XmNbottomAttachment, XmATTACH_WIDGET,
                 XmNbottomWidget, prefs_done,
                 NULL);

  ac = 0;
  children[ac++] = timeout_label;
  children[ac++] = cycle_label;
  children[ac++] = fade_seconds_label;
  children[ac++] = fade_ticks_label;
  children[ac++] = lock_label;
  children[ac++] = passwd_label;
  children[ac++] = timeout_text;
  children[ac++] = cycle_text;
  children[ac++] = fade_text;
  children[ac++] = fade_ticks_text;
  children[ac++] = lock_timeout_text;
  children[ac++] = passwd_timeout_text;
  children[ac++] = verbose_toggle;
  children[ac++] = install_cmap_toggle;
  children[ac++] = fade_toggle;
  children[ac++] = unfade_toggle;
  children[ac++] = lock_toggle;
  children[ac++] = hr;

  XtManageChildren(children, ac);
  ac = 0;

  XtManageChild (prefs_done);
  XtManageChild (prefs_cancel);

  XtManageChild (preferences_form);

  XtVaSetValues (preferences_form,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_FORM,
                 XmNtopAttachment, XmATTACH_FORM,
                 XmNbottomAttachment, XmATTACH_FORM,
                 NULL);

  return preferences_form;
}
