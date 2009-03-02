/* demo-Gtk-conf.c --- implements the dynamic configuration dialogs.
 * xscreensaver, Copyright (c) 2001, 2003 Jamie Zawinski <jwz@jwz.org>
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

#if defined(HAVE_GTK) && defined(HAVE_XML)   /* whole file */

#include <xscreensaver-intl.h>

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/*
 * Both of these workarounds can be removed when support for ancient
 * libxml versions is dropped.  versions 1.8.11 and 2.3.4 provide the
 * correct fixes.
 */

/* 
 * Older libxml polluted the global headerspace, while libxml2 fixed
 * this.  To support both old and recent libxmls, we have this
 * workaround.
 */
#ifdef HAVE_OLD_XML_HEADERS
# include <parser.h>
#else /* ! HAVE_OLD_XML_HEADERS */
# include <libxml/parser.h> 
#endif /* HAVE_OLD_XML_HEADERS */

/* 
 * handle non-native spelling mistakes in earlier versions and provide
 * the source-compat fix for this that may not be in older versions.
 */
#ifndef xmlChildrenNode
# if LIBXML_VERSION >= 20000
#  define xmlChildrenNode children
#  define xmlRootNode children
# else
#  define xmlChildrenNode childs
#  define xmlRootNode root
# endif /* LIBXML_VERSION */
#endif /* xmlChildrenNode */

#include <gtk/gtk.h>

#include "demo-Gtk-conf.h"

 
extern const char *blurb (void);


const char *hack_configuration_path = HACK_CONFIGURATION_PATH;

static gboolean debug_p = FALSE;


typedef enum {
  COMMAND,
  FAKE,
  DESCRIPTION,
  FAKEPREVIEW,
  STRING,
  FILENAME,
  SLIDER,
  SPINBUTTON,
  BOOLEAN,
  SELECT,
  SELECT_OPTION
} parameter_type;


typedef struct {

  parameter_type type;

  xmlChar *id;		/* widget name */
  xmlChar *label;	/* heading label, or null */

  /* command, fake, description, fakepreview, string, file
   */
  xmlChar *string;	/* file name, description, whatever. */

  /* slider, spinbutton
   */
  xmlChar *low_label;	/* label for the left side */
  xmlChar *high_label;	/* label for the right side */
  float low;		/* minimum value */
  float high;		/* maximum value */
  float value;		/* default value */
  gboolean integer_p;	/* whether the range is integral, or real */
  xmlChar *arg;		/* command-line option to set (substitute "%") */
  gboolean invert_p;	/* whether to flip the value and pretend the
                           range goes from hi-low instead of low-hi. */

  /* boolean, select-option
   */
  xmlChar *arg_set;	/* command-line option to set for "yes", or null */
  xmlChar *arg_unset;	/* command-line option to set for "no", or null */
  xmlChar *test;	/* #### no idea - enablement? */

  /* select
   */
  GList *options;

  /* select_option
   */
  GList *enable;

  GtkWidget *widget;

} parameter;


static parameter *make_select_option (const char *file, xmlNodePtr);
static void make_parameter_widget (const char *filename,
                                   parameter *, GtkWidget *, int *);
static void browse_button_cb (GtkButton *button, gpointer user_data);


/* Frees the parameter object and all strings and sub-parameters.
   Does not destroy the widget, if any.
 */
static void
free_parameter (parameter *p)
{
  GList *rest;
  if (p->id)         free (p->id);
  if (p->label)      free (p->label);
  if (p->string)     free (p->string);
  if (p->low_label)  free (p->low_label);
  if (p->high_label) free (p->high_label);
  if (p->arg)        free (p->arg);
  if (p->arg_set)    free (p->arg_set);
  if (p->arg_unset)  free (p->arg_unset);
  if (p->test)       free (p->test);

  for (rest = p->options; rest; rest = rest->next)
    if (rest->data)
      free_parameter ((parameter *) rest->data);

  for (rest = p->enable; rest; rest = rest->next)
    {
      free ((char *) rest->data);
      rest->data = 0;
    }
  if (p->enable) g_list_free (p->enable);

  memset (p, ~0, sizeof(*p));
  free (p);
}


/* Debugging: dumps out a `parameter' structure.
 */
#if 0
void
describe_parameter (FILE *out, parameter *p)
{
  fprintf (out, "<");
  switch (p->type)
    {
    case COMMAND:     fprintf (out, "command");      break;
    case FAKE:        fprintf (out, "fake");         break;
    case DESCRIPTION: fprintf (out, "_description"); break;
    case FAKEPREVIEW: fprintf (out, "fakepreview");  break;
    case STRING:      fprintf (out, "string");       break;
    case FILENAME:    fprintf (out, "filename");     break;
    case SLIDER:      fprintf (out, "number type=\"slider\"");     break;
    case SPINBUTTON:  fprintf (out, "number type=\"spinbutton\""); break;
    case BOOLEAN:     fprintf (out, "boolean");      break;
    case SELECT:      fprintf (out, "select");       break;
    default: abort(); break;
    }
  if (p->id)         fprintf (out, " id=\"%s\"",            p->id);
  if (p->label)      fprintf (out, " _label=\"%s\"",        p->label);
  if (p->string && p->type != DESCRIPTION)
                     fprintf (out, " string=\"%s\"",        p->string);
  if (p->low_label)  fprintf (out, " _low-label=\"%s\"",    p->low_label);
  if (p->high_label) fprintf (out, " _high-label=\"%s\"",   p->high_label);
  if (p->low)        fprintf (out, " low=\"%.2f\"",         p->low);
  if (p->high)       fprintf (out, " high=\"%.2f\"",        p->high);
  if (p->value)      fprintf (out, " default=\"%.2f\"",     p->value);
  if (p->arg)        fprintf (out, " arg=\"%s\"",           p->arg);
  if (p->invert_p)   fprintf (out, " convert=\"invert\"");
  if (p->arg_set)    fprintf (out, " arg-set=\"%s\"",       p->arg_set);
  if (p->arg_unset)  fprintf (out, " arg-unset=\"%s\"",     p->arg_unset);
  if (p->test)       fprintf (out, " test=\"%s\"",          p->test);
  fprintf (out, ">\n");

  if (p->type == SELECT)
    {
      GList *opt;
      for (opt = p->options; opt; opt = opt->next)
        {
          parameter *o = (parameter *) opt->data;
          if (o->type != SELECT_OPTION) abort();
          fprintf (out, "  <option");
          if (o->id)        fprintf (out, " id=\"%s\"",        o->id);
          if (o->label)     fprintf (out, " _label=\"%s\"",    o->label);
          if (o->arg_set)   fprintf (out, " arg-set=\"%s\"",   o->arg_set);
          if (o->arg_unset) fprintf (out, " arg-unset=\"%s\"", o->arg_unset);
          if (o->test)      fprintf (out, " test=\"%s\"",      o->test);
          if (o->enable)
            {
              GList *e;
              fprintf (out, " enable=\"");
              for (e = o->enable; e; e = e->next)
                fprintf (out, "%s%s", (char *) e->data, (e->next ? "," : ""));
              fprintf (out, "\"");
            }
          fprintf (out, ">\n");
        }
      fprintf (out, "</select>\n");
    }
  else if (p->type == DESCRIPTION)
    {
      if (p->string)
        fprintf (out, "  %s\n", p->string);
      fprintf (out, "</_description>\n");
    }
}
#endif /* 0 */


/* Like xmlGetProp() but parses a float out of the string.
   If the number was expressed as a float and not an integer
   (that is, the string contained a decimal point) then
   `floatp' is set to TRUE.  Otherwise, it is unchanged.
 */
static float
xml_get_float (xmlNodePtr node, const xmlChar *name, gboolean *floatpP)
{
  const char *s = (char *) xmlGetProp (node, name);
  float f;
  char c;
  if (!s || 1 != sscanf (s, "%f %c", &f, &c))
    return 0;
  else
    {
      if (strchr (s, '.')) *floatpP = TRUE;
      return f;
    }
}


static void sanity_check_parameter (const char *filename,
                                    const xmlChar *node_name,
                                    parameter *p);

/* Allocates and returns a new `parameter' object based on the
   properties in the given XML node.  Returns 0 if there's nothing
   to create (comment, or unknown tag.)
 */
static parameter *
make_parameter (const char *filename, xmlNodePtr node)
{
  parameter *p;
  const char *name = (char *) node->name;
  const char *convert;
  gboolean floatp = FALSE;

  if (node->type == XML_COMMENT_NODE)
    return 0;

  p = calloc (1, sizeof(*p));

  if (!name) abort();
  else if (!strcmp (name, "command"))      p->type = COMMAND;
  else if (!strcmp (name, "fullcommand"))  p->type = COMMAND;
  else if (!strcmp (name, "_description")) p->type = DESCRIPTION;
  else if (!strcmp (name, "fakepreview"))  p->type = FAKEPREVIEW;
  else if (!strcmp (name, "fake"))         p->type = FAKE;
  else if (!strcmp (name, "boolean"))      p->type = BOOLEAN;
  else if (!strcmp (name, "string"))       p->type = STRING;
  else if (!strcmp (name, "file"))         p->type = FILENAME;
  else if (!strcmp (name, "number"))       p->type = SPINBUTTON;
  else if (!strcmp (name, "select"))       p->type = SELECT;
  else
    {
      if (debug_p)
        fprintf (stderr, "%s: WARNING: %s: unknown tag: \"%s\"\n",
                 blurb(), filename, name);
      free (p);
      return 0;
    }

  if (p->type == SPINBUTTON)
    {
      const char *type = (char *) xmlGetProp (node, (xmlChar *) "type");
      if (!type || !strcmp (type, "spinbutton")) p->type = SPINBUTTON;
      else if (!strcmp (type, "slider"))         p->type = SLIDER;
      else
        {
          if (debug_p)
            fprintf (stderr, "%s: WARNING: %s: unknown %s type: \"%s\"\n",
                     blurb(), filename, name, type);
          free (p);
          return 0;
        }
    }
  else if (p->type == DESCRIPTION)
    {
      if (node->xmlChildrenNode &&
          node->xmlChildrenNode->type == XML_TEXT_NODE &&
          !node->xmlChildrenNode->next)
        p->string = (xmlChar *)
          strdup ((char *) node->xmlChildrenNode->content);
    }

  p->id         = xmlGetProp (node, (xmlChar *) "id");
  p->label      = xmlGetProp (node, (xmlChar *) "_label");
  p->low_label  = xmlGetProp (node, (xmlChar *) "_low-label");
  p->high_label = xmlGetProp (node, (xmlChar *) "_high-label");
  p->low        = xml_get_float (node, (xmlChar *) "low",     &floatp);
  p->high       = xml_get_float (node, (xmlChar *) "high",    &floatp);
  p->value      = xml_get_float (node, (xmlChar *) "default", &floatp);
  p->integer_p  = !floatp;
  convert       = (char *) xmlGetProp (node, (xmlChar *) "convert");
  p->invert_p   = (convert && !strcmp (convert, "invert"));
  p->arg        = xmlGetProp (node, (xmlChar *) "arg");
  p->arg_set    = xmlGetProp (node, (xmlChar *) "arg-set");
  p->arg_unset  = xmlGetProp (node, (xmlChar *) "arg-unset");
  p->test       = xmlGetProp (node, (xmlChar *) "test");

  /* Check for missing decimal point */
  if (debug_p &&
      p->integer_p &&
      (p->high != p->low) &&
      (p->high - p->low) <= 1)
    fprintf (stderr,
            "%s: WARNING: %s: %s: range [%.1f, %.1f] shouldn't be integral!\n",
             blurb(), filename, p->id,
             p->low, p->high);

  if (p->type == SELECT)
    {
      xmlNodePtr kids;
      for (kids = node->xmlChildrenNode; kids; kids = kids->next)
        {
          parameter *s = make_select_option (filename, kids);
          if (s)
            p->options = g_list_append (p->options, s);
        }
    }

  sanity_check_parameter (filename, (const xmlChar *) name, p);

  return p;
}


/* Allocates and returns a new SELECT_OPTION `parameter' object based
   on the properties in the given XML node.  Returns 0 if there's nothing
   to create (comment, or unknown tag.)
 */
static parameter *
make_select_option (const char *filename, xmlNodePtr node)
{
  if (node->type == XML_COMMENT_NODE)
    return 0;
  else if (node->type != XML_ELEMENT_NODE)
    {
      if (debug_p)
        fprintf (stderr,
                 "%s: WARNING: %s: %s: unexpected child tag type %d\n",
                 blurb(), filename, node->name, (int)node->type);
      return 0;
    }
  else if (strcmp ((char *) node->name, "option"))
    {
      if (debug_p)
        fprintf (stderr,
                 "%s: WARNING: %s: %s: child not an option tag: \"%s\"\n",
                 blurb(), filename, node->name, node->name);
      return 0;
    }
  else
    {
      parameter *s = calloc (1, sizeof(*s));
      char *enable, *e;

      s->type       = SELECT_OPTION;
      s->id         = xmlGetProp (node, (xmlChar *) "id");
      s->label      = xmlGetProp (node, (xmlChar *) "_label");
      s->arg_set    = xmlGetProp (node, (xmlChar *) "arg-set");
      s->arg_unset  = xmlGetProp (node, (xmlChar *) "arg-unset");
      s->test       = xmlGetProp (node, (xmlChar *) "test");
      enable = (char*)xmlGetProp (node, (xmlChar *) "enable");

      if (enable)
        {
          enable = strdup (enable);
          e = strtok (enable, ", ");
          while (e)
            {
              s->enable = g_list_append (s->enable, strdup (e));
              e = strtok (0, ", ");
            }
          free (enable);
        }

      sanity_check_parameter (filename, node->name, s);
      return s;
    }
}


/* Rudimentary check to make sure someone hasn't typed "arg-set="
   when they should have typed "arg=", etc.
 */
static void
sanity_check_parameter (const char *filename, const xmlChar *node_name,
                        parameter *p)
{
  struct {
    gboolean id;
    gboolean label;
    gboolean string;
    gboolean low_label;
    gboolean high_label;
    gboolean low;
    gboolean high;
    gboolean value;
    gboolean arg;
    gboolean invert_p;
    gboolean arg_set;
    gboolean arg_unset;
  } allowed, require;

  memset (&allowed, 0, sizeof (allowed));
  memset (&require, 0, sizeof (require));

  switch (p->type)
    {
    case COMMAND:
      allowed.arg = TRUE;
      require.arg = TRUE;
      break;
    case FAKE:
      break;
    case DESCRIPTION:
      allowed.string = TRUE;
      break;
    case FAKEPREVIEW:
      break;
    case STRING:
      allowed.id = TRUE;
      require.id = TRUE;
      allowed.label = TRUE;
      require.label = TRUE;
      allowed.arg = TRUE;
      require.arg = TRUE;
      break;
    case FILENAME:
      allowed.id = TRUE;
      require.id = TRUE;
      allowed.label = TRUE;
      allowed.arg = TRUE;
      require.arg = TRUE;
      break;
    case SLIDER:
      allowed.id = TRUE;
      require.id = TRUE;
      allowed.label = TRUE;
      allowed.low_label = TRUE;
      allowed.high_label = TRUE;
      allowed.arg = TRUE;
      require.arg = TRUE;
      allowed.low = TRUE;
      /* require.low = TRUE; -- may be 0 */
      allowed.high = TRUE;
      /* require.high = TRUE; -- may be 0 */
      allowed.value = TRUE;
      /* require.value = TRUE; -- may be 0 */
      allowed.invert_p = TRUE;
      break;
    case SPINBUTTON:
      allowed.id = TRUE;
      require.id = TRUE;
      allowed.label = TRUE;
      allowed.arg = TRUE;
      require.arg = TRUE;
      allowed.low = TRUE;
      /* require.low = TRUE; -- may be 0 */
      allowed.high = TRUE;
      /* require.high = TRUE; -- may be 0 */
      allowed.value = TRUE;
      /* require.value = TRUE; -- may be 0 */
      allowed.invert_p = TRUE;
      break;
    case BOOLEAN:
      allowed.id = TRUE;
      require.id = TRUE;
      allowed.label = TRUE;
      allowed.arg_set = TRUE;
      allowed.arg_unset = TRUE;
      break;
    case SELECT:
      allowed.id = TRUE;
      require.id = TRUE;
      break;
    case SELECT_OPTION:
      allowed.id = TRUE;
      allowed.label = TRUE;
      require.label = TRUE;
      allowed.arg_set = TRUE;
      break;
    default:
      abort();
      break;
    }

# define WARN(STR) \
   fprintf (stderr, "%s: %s: " STR " in <%s%s id=\"%s\">\n", \
              blurb(), filename, node_name, \
              (!strcmp((char *) node_name, "number") \
               ? (p->type == SPINBUTTON ? " type=spinbutton" : " type=slider")\
               : ""), \
              (p->id ? (char *) p->id : ""))
# define CHECK(SLOT,NAME) \
   if (p->SLOT && !allowed.SLOT) \
     WARN ("\"" NAME "\" is not a valid option"); \
   if (!p->SLOT && require.SLOT) \
     WARN ("\"" NAME "\" is required")

  CHECK (id,         "id");
  CHECK (label,      "_label");
  CHECK (string,     "(body text)");
  CHECK (low_label,  "_low-label");
  CHECK (high_label, "_high-label");
  CHECK (low,        "low");
  CHECK (high,       "high");
  CHECK (value,      "default");
  CHECK (arg,        "arg");
  CHECK (invert_p,   "convert");
  CHECK (arg_set,    "arg-set");
  CHECK (arg_unset,  "arg-unset");
# undef CHECK
# undef WARN
}



/* Helper for make_parameters()
 */
static GList *
make_parameters_1 (const char *filename, xmlNodePtr node,
                   GtkWidget *parent, int *row)
{
  GList *list = 0;

  for (; node; node = node->next)
    {
      const char *name = (char *) node->name;
      if (!strcmp (name, "hgroup") ||
          !strcmp (name, "vgroup"))
        {
          GtkWidget *box = (*name == 'h'
                            ? gtk_hbox_new (FALSE, 0)
                            : gtk_vbox_new (FALSE, 0));
          GList *list2;
          gtk_widget_show (box);

          if (row)
            gtk_table_attach (GTK_TABLE (parent), box, 0, 3, *row, *row + 1,
                              0, 0, 0, 0);
          else
            gtk_box_pack_start (GTK_BOX (parent), box, FALSE, FALSE, 0);

          if (row)
            (*row)++;

          list2 = make_parameters_1 (filename, node->xmlChildrenNode, box, 0);
          if (list2)
            list = g_list_concat (list, list2);
        }
      else
        {
          parameter *p = make_parameter (filename, node);
          if (p)
            {
              list = g_list_append (list, p);
              make_parameter_widget (filename, p, parent, row);
            }
        }
    }
  return list;
}


/* Calls make_parameter() and make_parameter_widget() on each relevant
   tag in the XML tree.  Also handles the "hgroup" and "vgroup" flags.
   Returns a GList of `parameter' objects.
 */
static GList *
make_parameters (const char *filename, xmlNodePtr node, GtkWidget *parent)
{
  int row = 0;
  for (; node; node = node->next)
    {
      if (node->type == XML_ELEMENT_NODE &&
          !strcmp ((char *) node->name, "screensaver"))
        return make_parameters_1 (filename, node->xmlChildrenNode, parent, &row);
    }
  return 0;
}


static gfloat
invert_range (gfloat low, gfloat high, gfloat value)
{
  gfloat range = high-low;
  gfloat off = value-low;
  return (low + (range - off));
}


static GtkAdjustment *
make_adjustment (const char *filename, parameter *p)
{
  float range = (p->high - p->low);
  float value = (p->invert_p
                 ? invert_range (p->low, p->high, p->value)
                 : p->value);
  gfloat si = (p->high - p->low) / 100;
  gfloat pi = (p->high - p->low) / 10;

  if (p->value < p->low || p->value > p->high)
    {
      if (debug_p && p->integer_p)
        fprintf (stderr, "%s: WARNING: %s: %d is not in range [%d, %d]\n",
                 blurb(), filename,
                 (int) p->value, (int) p->low, (int) p->high);
      else if (debug_p)
        fprintf (stderr,
                 "%s: WARNING: %s: %.2f is not in range [%.2f, %.2f]\n",
                 blurb(), filename, p->value, p->low, p->high);
      value = (value < p->low ? p->low : p->high);
    }
#if 0
  else if (debug_p && p->value < 1000 && p->high >= 10000)
    {
      if (p->integer_p)
        fprintf (stderr,
                 "%s: WARNING: %s: %d is suspicious for range [%d, %d]\n",
                 blurb(), filename,
                 (int) p->value, (int) p->low, (int) p->high);
      else
        fprintf (stderr,
               "%s: WARNING: %s: %.2f is suspicious for range [%.2f, %.2f]\n",
                 blurb(), filename, p->value, p->low, p->high);
    }
#endif /* 0 */

  si = (int) (si + 0.5);
  pi = (int) (pi + 0.5);
  if (si < 1) si = 1;
  if (pi < 1) pi = 1;

  if (range <= 500) si = 1;

  return GTK_ADJUSTMENT (gtk_adjustment_new (value,
                                             p->low,
                                             p->high + 1,
                                             si, pi, 1));
}



/* Given a `parameter' struct, allocates an appropriate GtkWidget for it,
   and stores it in `p->widget'.
   `row' is used for keeping track of our position during table layout.
   `parent' must be a GtkTable or a GtkBox.
 */
static void
make_parameter_widget (const char *filename,
                       parameter *p, GtkWidget *parent, int *row)
{
  const char *label = (char *) p->label;
  if (p->widget) return;

  switch (p->type)
    {
    case STRING:
      {
        if (label)
          {
            GtkWidget *w = gtk_label_new (_(label));
            gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_RIGHT);
            gtk_misc_set_alignment (GTK_MISC (w), 1.0, 0.5);
            gtk_widget_show (w);
            if (row)
              gtk_table_attach (GTK_TABLE (parent), w, 0, 1, *row, *row + 1,
                                GTK_FILL, 0, 0, 0);
            else
              gtk_box_pack_start (GTK_BOX (parent), w, FALSE, FALSE, 4);
          }

        p->widget = gtk_entry_new ();
        if (p->string)
          gtk_entry_set_text (GTK_ENTRY (p->widget), (char *) p->string);
        if (row)
          gtk_table_attach (GTK_TABLE (parent), p->widget, 1, 3,
                            *row, *row + 1,
                            GTK_FILL, 0, 0, 0);
        else
          gtk_box_pack_start (GTK_BOX (parent), p->widget, FALSE, FALSE, 4);
        break;
      }
    case FILENAME:
      {
        GtkWidget *L = gtk_label_new (label ? _(label) : "");
        GtkWidget *entry = gtk_entry_new ();
        GtkWidget *button = gtk_button_new_with_label (_("Browse..."));
        gtk_widget_show (entry);
        gtk_widget_show (button);
        p->widget = entry;

        gtk_signal_connect (GTK_OBJECT (button),
                            "clicked", GTK_SIGNAL_FUNC (browse_button_cb),
                            (gpointer) entry);

        gtk_label_set_justify (GTK_LABEL (L), GTK_JUSTIFY_RIGHT);
        gtk_misc_set_alignment (GTK_MISC (L), 1.0, 0.5);
        gtk_widget_show (L);

        if (p->string)
          gtk_entry_set_text (GTK_ENTRY (entry), (char *) p->string);

        if (row)
          {
            gtk_table_attach (GTK_TABLE (parent), L, 0, 1,
                              *row, *row + 1,
                              GTK_FILL, 0, 0, 0);
            gtk_table_attach (GTK_TABLE (parent), entry, 1, 2,
                              *row, *row + 1,
                              GTK_EXPAND | GTK_FILL, 0, 0, 0);
            gtk_table_attach (GTK_TABLE (parent), button, 2, 3,
                              *row, *row + 1,
                              0, 0, 0, 0);
          }
        else
          {
            gtk_box_pack_start (GTK_BOX (parent), L,      FALSE, FALSE, 4);
            gtk_box_pack_start (GTK_BOX (parent), entry,  TRUE,  TRUE,  4);
            gtk_box_pack_start (GTK_BOX (parent), button, FALSE, FALSE, 4);
          }
        break;
      }
    case SLIDER:
      {
        GtkAdjustment *adj = make_adjustment (filename, p);
        GtkWidget *scale = gtk_hscale_new (adj);
        GtkWidget *labelw = 0;

        if (label)
          {
            labelw = gtk_label_new (_(label));
            gtk_label_set_justify (GTK_LABEL (labelw), GTK_JUSTIFY_LEFT);
            gtk_misc_set_alignment (GTK_MISC (labelw), 0.0, 0.5);
            gtk_widget_show (labelw);
          }

        if (GTK_IS_VBOX (parent))
          {
            /* If we're inside a vbox, we need to put an hbox in it, to get
               the low/high labels to be to the left/right of the slider.
             */
            GtkWidget *hbox = gtk_hbox_new (FALSE, 0);

            /* But if we have a label, put that above the slider's hbox. */
            if (labelw)
              {
                gtk_box_pack_start (GTK_BOX (parent), labelw, FALSE, TRUE, 2);
                labelw = 0;
              }

            gtk_box_pack_start (GTK_BOX (parent), hbox, TRUE, TRUE, 6);
            gtk_widget_show (hbox);
            parent = hbox;
          }

        if (labelw)
          {
            if (row)
              {
                gtk_table_attach (GTK_TABLE (parent), labelw,
                                  0, 3, *row, *row + 1,
                                  GTK_EXPAND | GTK_FILL, 0, 0, 0);
                (*row)++;
              }
            else
              {
                if (GTK_IS_HBOX (parent))
                  {
                    GtkWidget *box = gtk_vbox_new (FALSE, 0);
                    gtk_box_pack_start (GTK_BOX (parent), box, FALSE, TRUE, 0);
                    gtk_widget_show (box);
                    gtk_box_pack_start (GTK_BOX (box), labelw, FALSE, TRUE, 4);
                    parent = box;
                    box = gtk_hbox_new (FALSE, 0);
                    gtk_widget_show (box);
                    gtk_box_pack_start (GTK_BOX (parent), box, TRUE, TRUE, 0);
                    parent = box;
                  }
                else
                  gtk_box_pack_start (GTK_BOX (parent), labelw,
                                      FALSE, TRUE, 0);
              }
          }

        if (p->low_label)
          {
            GtkWidget *w = gtk_label_new (_((char *) p->low_label));
            gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_RIGHT);
            gtk_misc_set_alignment (GTK_MISC (w), 1.0, 0.5);
            gtk_widget_show (w);
            if (row)
              gtk_table_attach (GTK_TABLE (parent), w, 0, 1, *row, *row + 1,
                                GTK_FILL, 0, 0, 0);
            else
              gtk_box_pack_start (GTK_BOX (parent), w, FALSE, FALSE, 4);
          }

        gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_BOTTOM);
        gtk_scale_set_draw_value (GTK_SCALE (scale), debug_p);
        gtk_scale_set_digits (GTK_SCALE (scale), (p->integer_p ? 0 : 2));
        if (row)
          gtk_table_attach (GTK_TABLE (parent), scale, 1, 2,
                            *row, *row + 1,
                            GTK_EXPAND | GTK_FILL, 0, 0, 0);
        else
          gtk_box_pack_start (GTK_BOX (parent), scale, TRUE, TRUE, 4);

        gtk_widget_show (scale);

        if (p->high_label)
          {
            GtkWidget *w = gtk_label_new (_((char *) p->high_label));
            gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_LEFT);
            gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
            gtk_widget_show (w);
            if (row)
              gtk_table_attach (GTK_TABLE (parent), w, 2, 3, *row, *row + 1,
                                GTK_FILL, 0, 0, 0);
            else
              gtk_box_pack_start (GTK_BOX (parent), w, FALSE, FALSE, 4);
          }

        p->widget = scale;
        break;
      }
    case SPINBUTTON:
      {
        GtkAdjustment *adj = make_adjustment (filename, p);
        GtkWidget *spin = gtk_spin_button_new (adj, 15, 0);
        gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
        gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (spin), TRUE);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), adj->value);

        if (label)
          {
            GtkWidget *w = gtk_label_new (_(label));
            gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_RIGHT);
            gtk_misc_set_alignment (GTK_MISC (w), 1.0, 0.5);
            gtk_widget_show (w);
            if (row)
              gtk_table_attach (GTK_TABLE (parent), w, 0, 1, *row, *row + 1,
                                GTK_FILL, 0, 0, 0);
            else
              gtk_box_pack_start (GTK_BOX (parent), w, TRUE, TRUE, 4);
          }

        gtk_widget_show (spin);
        if (row)
          {
            GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
            gtk_widget_show (hbox);
            gtk_table_attach (GTK_TABLE (parent), hbox, 1, 3,
                              *row, *row + 1,
                              GTK_EXPAND | GTK_FILL, 0, 8, 0);
            gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, FALSE, 0);
          }
        else
          gtk_box_pack_start (GTK_BOX (parent), spin, FALSE, FALSE, 4);

        p->widget = spin;
        break;
      }
    case BOOLEAN:
      {
        p->widget = gtk_check_button_new_with_label (_(label));
        if (row)
          gtk_table_attach (GTK_TABLE (parent), p->widget, 0, 3,
                            *row, *row + 1,
                            GTK_EXPAND | GTK_FILL, 0, 0, 0);
        else
          gtk_box_pack_start (GTK_BOX (parent), p->widget, FALSE, FALSE, 4);
        break;
      }
    case SELECT:
      {
        GtkWidget *opt = gtk_option_menu_new ();
        GtkWidget *menu = gtk_menu_new ();
        GList *opts;

        if (label && row)
          {
            GtkWidget *w = gtk_label_new (_(label));
            gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_LEFT);
            gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
            gtk_widget_show (w);
            gtk_table_attach (GTK_TABLE (parent), w, 0, 3, *row, *row + 1,
                              GTK_EXPAND | GTK_FILL, 0, 0, 0);
            (*row)++;
          }

        for (opts = p->options; opts; opts = opts->next)
          {
            parameter *s = (parameter *) opts->data;
            GtkWidget *i = gtk_menu_item_new_with_label (_((char *) s->label));
            gtk_widget_show (i);
            gtk_menu_append (GTK_MENU (menu), i);
          }

        gtk_option_menu_set_menu (GTK_OPTION_MENU (opt), menu);
        p->widget = opt;
        if (row)
          gtk_table_attach (GTK_TABLE (parent), p->widget, 0, 3,
                            *row, *row + 1,
                            GTK_EXPAND | GTK_FILL, 0, 0, 0);
        else
          gtk_box_pack_start (GTK_BOX (parent), p->widget, TRUE, TRUE, 4);
        break;
      }

    case COMMAND:
    case FAKE:
    case DESCRIPTION:
    case FAKEPREVIEW:
      break;
    default:
      abort();
    }

  if (p->widget)
    {
      gtk_widget_set_name (p->widget, (char *) p->id);
      gtk_widget_show (p->widget);
      if (row)
        (*row)++;
    }
}


/* File selection.
   Absurdly, there is no GTK file entry widget, only a GNOME one,
   so in order to avoid depending on GNOME in this code, we have
   to do it ourselves.
 */

/* cancel button on GtkFileSelection: user_data unused */
static void
file_sel_cancel (GtkWidget *button, gpointer user_data)
{
  GtkWidget *dialog = button;
  while (dialog->parent)
    dialog = dialog->parent;
  gtk_widget_destroy (dialog);
}

/* ok button on GtkFileSelection: user_data is the corresponding GtkEntry */
static void
file_sel_ok (GtkWidget *button, gpointer user_data)
{
  GtkWidget *entry = GTK_WIDGET (user_data);
  GtkWidget *dialog = button;
  const char *path;
  while (dialog->parent)
    dialog = dialog->parent;
  gtk_widget_hide (dialog);

  path = gtk_file_selection_get_filename (GTK_FILE_SELECTION (dialog));
  /* apparently one doesn't free `path' */

  gtk_entry_set_text (GTK_ENTRY (entry), path);
  gtk_entry_set_position (GTK_ENTRY (entry), strlen (path));

  gtk_widget_destroy (dialog);
}

/* WM close on GtkFileSelection: user_data unused */
static void
file_sel_close (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  file_sel_cancel (widget, user_data);
}

/* "Browse" button: user_data is the corresponding GtkEntry */
static void
browse_button_cb (GtkButton *button, gpointer user_data)
{
  GtkWidget *entry = GTK_WIDGET (user_data);
  const char *text = gtk_entry_get_text (GTK_ENTRY (entry));
  GtkFileSelection *selector =
    GTK_FILE_SELECTION (gtk_file_selection_new (_("Select file.")));

  gtk_file_selection_set_filename (selector, text);
  gtk_signal_connect (GTK_OBJECT (selector->ok_button),
                      "clicked", GTK_SIGNAL_FUNC (file_sel_ok),
                      (gpointer) entry);
  gtk_signal_connect (GTK_OBJECT (selector->cancel_button),
                      "clicked", GTK_SIGNAL_FUNC (file_sel_cancel),
                      (gpointer) entry);
  gtk_signal_connect (GTK_OBJECT (selector), "delete_event",
                      GTK_SIGNAL_FUNC (file_sel_close),
                      (gpointer) entry);

  gtk_window_set_modal (GTK_WINDOW (selector), TRUE);
  gtk_widget_show (GTK_WIDGET (selector));
}


/* Converting to and from command-lines
 */


/* Returns a copy of string that has been quoted according to shell rules:
   it may have been wrapped in "" and had some characters backslashed; or
   it may be unchanged.
 */
static char *
shell_quotify (const char *string)
{
  char *string2 = (char *) malloc ((strlen (string) * 2) + 10);
  const char *in;
  char *out;
  int need_quotes = 0;
  int in_length = 0;

  out = string2;
  *out++ = '"';
  for (in = string; *in; in++)
    {
      in_length++;
      if (*in == '!' ||
          *in == '"' ||
          *in == '$')
        {
          need_quotes = 1;
          *out++ = '\\';
          *out++ = *in;
        }
      else if (*in <= ' ' ||
               *in >= 127 ||
               *in == '\'' ||
               *in == '#' ||
               *in == '%' ||
               *in == '&' ||
               *in == '(' ||
               *in == ')' ||
               *in == '*')
        {
          need_quotes = 1;
          *out++ = *in;
        }
      else
        *out++ = *in;
    }
  *out++ = '"';
  *out = 0;

  if (in_length == 0)
    need_quotes = 1;

  if (need_quotes)
    return (string2);

  free (string2);
  return strdup (string);
}

/* Modify the string in place to remove wrapping double-quotes
   and interior backslashes. 
 */
static void
de_stringify (char *s)
{
  char q = s[0];
  if (q != '\'' && q != '\"' && q != '`')
    abort();
  memmove (s, s+1, strlen (s)+1);
  while (*s && *s != q)
    {
      if (*s == '\\')
        memmove (s, s+1, strlen (s)+1);
      s++;
    }
  if (*s != q) abort();
  *s = 0;
}


/* Substitutes a shell-quotified version of `value' into `p->arg' at
   the place where the `%' character appeared.
 */
static char *
format_switch (parameter *p, const char *value)
{
  char *fmt = (char *) p->arg;
  char *v2;
  char *result, *s;
  if (!fmt || !value) return 0;
  v2 = shell_quotify (value);
  result = (char *) malloc (strlen (fmt) + strlen (v2) + 10);
  s = result;
  for (; *fmt; fmt++)
    if (*fmt != '%')
      *s++ = *fmt;
    else
      {
        strcpy (s, v2);
        s += strlen (s);
      }
  *s = 0;

  free (v2);
  return result;
}


/* Maps a `parameter' to a command-line switch.
   Returns 0 if it can't, or if the parameter has the default value.
 */
static char *
parameter_to_switch (parameter *p)
{
  switch (p->type)
    {
    case COMMAND:
      if (p->arg)
        return strdup ((char *) p->arg);
      else
        return 0;
      break;
    case STRING:
    case FILENAME:
      if (!p->widget) return 0;
      {
        const char *s = gtk_entry_get_text (GTK_ENTRY (p->widget));
        char *v;
        if (!strcmp ((s ? s : ""),
                     (p->string ? (char *) p->string : "")))
          v = 0;  /* same as default */
        else
          v = format_switch (p, s);

        /* don't free `s' */
        return v;
      }
    case SLIDER:
    case SPINBUTTON:
      if (!p->widget) return 0;
      {
        GtkAdjustment *adj =
          (p->type == SLIDER
           ? gtk_range_get_adjustment (GTK_RANGE (p->widget))
           : gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (p->widget)));
        char buf[255];
        char *s1;
        float value = (p->invert_p
                       ? invert_range (adj->lower, adj->upper, adj->value) - 1
                       : adj->value);

        if (value == p->value)  /* same as default */
          return 0;

        if (p->integer_p)
          sprintf (buf, "%d", (int) (value + 0.5));
        else
          sprintf (buf, "%.4f", value);
          
        s1 = strchr (buf, '.');
        if (s1)
          {
            char *s2 = s1 + strlen(s1) - 1;
            while (s2 > s1 && *s2 == '0')	/* lose trailing zeroes */
              *s2-- = 0;
            if (s2 >= s1 && *s2 == '.')		/* lose trailing decimal */
              *s2-- = 0;
          }
        return format_switch (p, buf);
      }
    case BOOLEAN:
      if (!p->widget) return 0;
      {
        GtkToggleButton *b = GTK_TOGGLE_BUTTON (p->widget);
        const char *s = (gtk_toggle_button_get_active (b)
                         ? (char *) p->arg_set
                         : (char *) p->arg_unset);
        if (s)
          return strdup (s);
        else
          return 0;
      }
    case SELECT:
      if (!p->widget) return 0;
      {
        GtkOptionMenu *opt = GTK_OPTION_MENU (p->widget);
        GtkMenu *menu = GTK_MENU (gtk_option_menu_get_menu (opt));
        GtkWidget *selected = gtk_menu_get_active (menu);
        GList *kids = gtk_container_children (GTK_CONTAINER (menu));
        int menu_elt = g_list_index (kids, (gpointer) selected);
        GList *ol = g_list_nth (p->options, menu_elt);
        parameter *o = (ol ? (parameter *) ol->data : 0);
        const char *s;
        if (!o) abort();
        if (o->type != SELECT_OPTION) abort();
        s = (char *) o->arg_set;
        if (s)
          return strdup (s);
        else
          return 0;
      }
    default:
      if (p->widget)
        abort();
      else
        return 0;
    }
}

/* Maps a GList of `parameter' objects to a complete command-line string.
   All arguments will be properly quoted.
 */
static char *
parameters_to_cmd_line (GList *parms)
{
  int L = g_list_length (parms);
  int LL = 0;
  char **strs = (char **) calloc (sizeof (*parms), L);
  char *result;
  char *out;
  int i;

  for (i = 0; parms; parms = parms->next, i++)
    {
      char *s = parameter_to_switch ((parameter *) parms->data);
      strs[i] = s;
      LL += (s ? strlen(s) : 0) + 1;
    }

  result = (char *) malloc (LL + 10);
  out = result;
  for (i = 0; i < L; i++)
    if (strs[i])
      {
        strcpy (out, strs[i]);
        out += strlen (out);
        *out++ = ' ';
        free (strs[i]);
      }
  *out = 0;
  while (out > result && out[-1] == ' ')  /* strip trailing spaces */
    *(--out) = 0;
  free (strs);

  return result;
}


/* Returns a GList of the tokens the string, using shell syntax;
   Quoted strings are handled as a single token.
 */
static GList *
tokenize_command_line (const char *cmd)
{
  GList *result = 0;
  const char *s = cmd;
  while (*s)
    {
      const char *start;
      char *ss;
      for (; isspace(*s); s++);		/* skip whitespace */

      start = s;
      if (*s == '\'' || *s == '\"' || *s == '`')
        {
          char q = *s;
          s++;
          while (*s && *s != q)		/* skip to matching quote */
            {
              if (*s == '\\' && s[1])	/* allowing backslash quoting */
                s++;
              s++;
            }
          s++;
        }
      else
        {
          while (*s &&
                 (! (isspace(*s) ||
                     *s == '\'' ||
                     *s == '\"' ||
                     *s == '`')))
            s++;
        }

      if (s > start)
        {
          ss = (char *) malloc ((s - start) + 1);
          strncpy (ss, start, s-start);
          ss[s-start] = 0;
          if (*ss == '\'' || *ss == '\"' || *ss == '`')
            de_stringify (ss);
          result = g_list_append (result, ss);
        }
    }

  return result;
}

static void parameter_set_switch (parameter *, gpointer value);
static gboolean parse_command_line_into_parameters_1 (const char *filename,
                                                      GList *parms,
                                                      const char *option,
                                                      const char *value,
                                                      parameter *parent);


/* Parses the command line, and flushes those options down into
   the `parameter' structs in the list.
 */
static void
parse_command_line_into_parameters (const char *filename,
                                    const char *cmd, GList *parms)
{
  GList *tokens = tokenize_command_line (cmd);
  GList *rest;
  for (rest = tokens; rest; rest = rest->next)
    {
      char *option = rest->data;
      rest->data = 0;

      if (option[0] != '-')
        {
          if (debug_p)
            fprintf (stderr, "%s: WARNING: %s: not a switch: \"%s\"\n",
                     blurb(), filename, option);
        }
      else
        {
          char *value = 0;

          if (rest->next)   /* pop off the arg to this option */
            {
              char *s = (char *) rest->next->data;
              /* the next token is the next switch iff it matches "-[a-z]".
                 (To avoid losing on "-x -3.1".)
               */
              if (s && (s[0] != '-' || !isalpha(s[1])))
                {
                  value = s;
                  rest->next->data = 0;
                  rest = rest->next;
                }
            }

          parse_command_line_into_parameters_1 (filename, parms,
                                                option, value, 0);
          if (value) free (value);
          free (option);
        }
    }
  g_list_free (tokens);
}


static gboolean
compare_opts (const char *option, const char *value,
              const char *template)
{
  int ol = strlen (option);
  char *c;

  if (strncmp (option, template, ol))
    return FALSE;

  if (template[ol] != (value ? ' ' : 0))
    return FALSE;

  /* At this point, we have a match against "option".
     If template contains a %, we're done.
     Else, compare against "value" too.
   */
  c = strchr (template, '%');
  if (c)
    return TRUE;

  if (!value)
    return (template[ol] == 0);
  if (strcmp (template + ol + 1, value))
    return FALSE;

  return TRUE;
}


static gboolean
parse_command_line_into_parameters_1 (const char *filename,
                                      GList *parms,
                                      const char *option,
                                      const char *value,
                                      parameter *parent)
{
  GList *p;
  parameter *match = 0;
  gint which = -1;
  gint index = 0;

  for (p = parms; p; p = p->next)
    {
      parameter *pp = (parameter *) p->data;
      which = -99;

      if (pp->type == SELECT)
        {
          if (parse_command_line_into_parameters_1 (filename,
                                                    pp->options,
                                                    option, value,
                                                    pp))
            {
              which = -2;
              match = pp;
            }
        }
      else if (pp->arg)
        {
          if (compare_opts (option, value, (char *) pp->arg))
            {
              which = -1;
              match = pp;
            }
        }
      else if (pp->arg_set)
        {
          if (compare_opts (option, value, (char *) pp->arg_set))
            {
              which = 1;
              match = pp;
            }
        }
      else if (pp->arg_unset)
        {
          if (compare_opts (option, value, (char *) pp->arg_unset))
            {
              which = 0;
              match = pp;
            }
        }

      if (match)
        break;

      index++;
    }

  if (!match)
    {
      if (debug_p && !parent)
        fprintf (stderr, "%s: WARNING: %s: no match for %s %s\n",
                 blurb(), filename, option, (value ? value : ""));
      return FALSE;
    }

  switch (match->type)
    {
    case STRING:
    case FILENAME:
    case SLIDER:
    case SPINBUTTON:
      if (which != -1) abort();
      parameter_set_switch (match, (gpointer) value);
      break;
    case BOOLEAN:
      if (which != 0 && which != 1) abort();
      parameter_set_switch (match, GINT_TO_POINTER(which));
      break;
    case SELECT_OPTION:
      if (which != 1) abort();
      parameter_set_switch (parent, GINT_TO_POINTER(index));
      break;
    default:
      break;
    }
  return TRUE;
}


/* Set the parameter's value.
   For STRING, FILENAME, SLIDER, and SPINBUTTON, `value' is a char*.
   For BOOLEAN and SELECT, `value' is an int.
 */
static void
parameter_set_switch (parameter *p, gpointer value)
{
  if (p->type == SELECT_OPTION) abort();
  if (!p->widget) return;
  switch (p->type)
    {
    case STRING:
    case FILENAME:
      {
        gtk_entry_set_text (GTK_ENTRY (p->widget), (char *) value);
        break;
      }
    case SLIDER:
    case SPINBUTTON:
      {
        GtkAdjustment *adj =
          (p->type == SLIDER
           ? gtk_range_get_adjustment (GTK_RANGE (p->widget))
           : gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (p->widget)));
        float f;
        char c;

        if (1 == sscanf ((char *) value, "%f %c", &f, &c))
          {
            if (p->invert_p)
              f = invert_range (adj->lower, adj->upper, f) - 1;
            gtk_adjustment_set_value (adj, f);
          }
        break;
      }
    case BOOLEAN:
      {
        GtkToggleButton *b = GTK_TOGGLE_BUTTON (p->widget);
        gtk_toggle_button_set_active (b, GPOINTER_TO_INT(value));
        break;
      }
    case SELECT:
      {
        gtk_option_menu_set_history (GTK_OPTION_MENU (p->widget),
                                     GPOINTER_TO_INT(value));
        break;
      }
    default:
      abort();
    }
}


static void
restore_defaults (const char *progname, GList *parms)
{
  for (; parms; parms = parms->next)
    {
      parameter *p = (parameter *) parms->data;
      if (!p->widget) continue;
      switch (p->type)
        {
        case STRING:
        case FILENAME:
          {
            gtk_entry_set_text (GTK_ENTRY (p->widget),
                                (p->string ? (char *) p->string : ""));
            break;
          }
        case SLIDER:
        case SPINBUTTON:
          {
            GtkAdjustment *adj =
              (p->type == SLIDER
               ? gtk_range_get_adjustment (GTK_RANGE (p->widget))
               : gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (p->widget)));
            float value = (p->invert_p
                           ? invert_range (p->low, p->high, p->value)
                           : p->value);
            gtk_adjustment_set_value (adj, value);
            break;
          }
        case BOOLEAN:
          {
            /* A toggle button should be on by default if it inserts
               nothing into the command line when on.  E.g., it should
               be on if `arg_set' is null.
             */
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (p->widget),
                                          (!p->arg_set || !*p->arg_set));
            break;
          }
        case SELECT:
          {
            GtkOptionMenu *opt = GTK_OPTION_MENU (p->widget);
            GList *opts;
            int selected = 0;
            int index;

            for (opts = p->options, index = 0; opts;
                 opts = opts->next, index++)
              {
                parameter *s = (parameter *) opts->data;
                /* The default menu item is the first one with
                   no `arg_set' field. */
                if (!s->arg_set)
                  {
                    selected = index;
                    break;
                  }
              }

            gtk_option_menu_set_history (GTK_OPTION_MENU (opt), selected);
            break;
          }
        default:
          abort();
        }
    }
}



/* Documentation strings
 */

static char *
get_description (GList *parms)
{
  parameter *doc = 0;
  for (; parms; parms = parms->next)
    {
      parameter *p = (parameter *) parms->data;
      if (p->type == DESCRIPTION)
        {
          doc = p;
          break;
        }
    }

  if (!doc || !doc->string)
    return 0;
  else
    {
      char *d = strdup ((char *) doc->string);
      char *s;
      for (s = d; *s; s++)
        if (s[0] == '\n')
          {
            if (s[1] == '\n')      /* blank line: leave it */
              s++;
            else if (s[1] == ' ' || s[1] == '\t')
              s++;                 /* next line is indented: leave newline */
            else
              s[0] = ' ';          /* delete newline to un-fold this line */
          }

      /* strip off leading whitespace on first line only */
      for (s = d; *s && (*s == ' ' || *s == '\t'); s++)
        ;
      while (*s == '\n')   /* strip leading newlines */
        s++;
      if (s != d)
        memmove (d, s, strlen(s)+1);

      /* strip off trailing whitespace and newlines */
      {
        int L = strlen(d);
        while (L && isspace(d[L-1]))
          d[--L] = 0;
      }

      return _(d);
    }
}


/* External interface.
 */

static conf_data *
load_configurator_1 (const char *program, const char *arguments,
                     gboolean verbose_p)
{
  const char *dir = hack_configuration_path;
  int L = strlen (dir);
  char *file;
  char *s;
  FILE *f;
  conf_data *data;

  if (L == 0) return 0;

  file = (char *) malloc (L + strlen (program) + 10);
  data = (conf_data *) calloc (1, sizeof(*data));

  strcpy (file, dir);
  if (file[L-1] != '/')
    file[L++] = '/';
  strcpy (file+L, program);

  for (s = file+L; *s; s++)
    if (*s == '/' || *s == ' ')
      *s = '_';
    else if (isupper (*s))
      *s = tolower (*s);

  strcat (file+L, ".xml");

  f = fopen (file, "r");
  if (f)
    {
      int res, size = 1024;
      char chars[1024];
      xmlParserCtxtPtr ctxt;
      xmlDocPtr doc = 0;
      GtkWidget *table;
      GList *parms;

      if (verbose_p)
        fprintf (stderr, "%s: reading %s...\n", blurb(), file);

      res = fread (chars, 1, 4, f);
      if (res <= 0)
        {
          free (data);
          data = 0;
          goto DONE;
        }

      ctxt = xmlCreatePushParserCtxt(NULL, NULL, chars, res, file);
      while ((res = fread(chars, 1, size, f)) > 0)
        xmlParseChunk (ctxt, chars, res, 0);
      xmlParseChunk (ctxt, chars, 0, 1);
      doc = ctxt->myDoc;
      xmlFreeParserCtxt (ctxt);
      fclose (f);

      /* Parsed the XML file.  Now make some widgets. */

      table = gtk_table_new (1, 3, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 4);
      gtk_table_set_col_spacings (GTK_TABLE (table), 4);
      gtk_container_set_border_width (GTK_CONTAINER (table), 8);
      gtk_widget_show (table);

      parms = make_parameters (file, doc->xmlRootNode, table);

      xmlFreeDoc (doc);

      restore_defaults (program, parms);
      if (arguments && *arguments)
        parse_command_line_into_parameters (program, arguments, parms);

      data->widget = table;
      data->parameters = parms;
      data->description = get_description (parms);
    }
  else
    {
      parameter *p;

      if (verbose_p)
        fprintf (stderr, "%s: %s does not exist.\n", blurb(), file);

      p = calloc (1, sizeof(*p));
      p->type = COMMAND;
      p->arg = (xmlChar *) strdup (arguments);

      data->parameters = g_list_append (0, (gpointer) p);
    }

  data->progname = strdup (program);

 DONE:
  free (file);
  return data;
}

static void
split_command_line (const char *full_command_line,
                    char **prog_ret, char **args_ret)
{
  char *line = strdup (full_command_line);
  char *prog;
  char *args;
  char *s;

  prog = line;
  s = line;
  while (*s)
    {
      if (isspace (*s))
        {
          *s = 0;
          s++;
          while (isspace (*s)) s++;
          break;
        }
      else if (*s == '=')  /* if the leading word contains an "=", skip it. */
        {
          while (*s && !isspace (*s)) s++;
          while (isspace (*s)) s++;
          prog = s;
        }
      s++;
    }
  args = s;

  *prog_ret = strdup (prog);
  *args_ret = strdup (args);
  free (line);
}


conf_data *
load_configurator (const char *full_command_line, gboolean verbose_p)
{
  char *prog;
  char *args;
  conf_data *cd;
  debug_p = verbose_p;
  split_command_line (full_command_line, &prog, &args);
  cd = load_configurator_1 (prog, args, verbose_p);
  free (prog);
  free (args);
  return cd;
}



char *
get_configurator_command_line (conf_data *data)
{
  char *args = parameters_to_cmd_line (data->parameters);
  char *result = (char *) malloc (strlen (data->progname) +
                                  strlen (args) + 2);
  strcpy (result, data->progname);
  strcat (result, " ");
  strcat (result, args);
  free (args);
  return result;
}


void
set_configurator_command_line (conf_data *data, const char *full_command_line)
{
  char *prog;
  char *args;
  split_command_line (full_command_line, &prog, &args);
  if (data->progname) free (data->progname);
  data->progname = prog;
  restore_defaults (prog, data->parameters);
  parse_command_line_into_parameters (prog, args, data->parameters);
  free (args);
}

void
free_conf_data (conf_data *data)
{
  if (data->parameters)
    {
      GList *rest;
      for (rest = data->parameters; rest; rest = rest->next)
        {
          free_parameter ((parameter *) rest->data);
          rest->data = 0;
        }
      g_list_free (data->parameters);
      data->parameters = 0;
    }

  if (data->widget)
    gtk_widget_destroy (data->widget);

  if (data->progname)
    free (data->progname);
  if (data->description)
    free (data->description);

  memset (data, ~0, sizeof(*data));
  free (data);
}


#endif /* HAVE_GTK && HAVE_XML -- whole file */
