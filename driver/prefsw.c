/* prefs.c --- reading and writing the ~/.xscreensaver file.
 * xscreensaver, Copyright © 1998-2022 Jamie Zawinski <jwz@jwz.org>
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

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>   /* for PATH_MAX */
#include <pwd.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Intrinsic.h>

#include "version.h"
#include "types.h"
#include "prefs.h"
#include "resources.h"
#include "blurb.h"

/* don't use realpath() on fedora system */
#ifdef _FORTIFY_SOURCE
# undef HAVE_REALPATH
#endif


extern char *progclass;

static void get_screenhacks (Display *, saver_preferences *);
static char *format_command (const char *cmd, Bool wrap_p);
static void merge_system_screenhacks (Display *, saver_preferences *,
                                      screenhack **system_list, int count);
static void stop_the_insanity (saver_preferences *p);

static char *format_hack (Display *, screenhack *, Bool wrap_p);

static char *
chase_symlinks (const char *file)
{
# ifdef HAVE_REALPATH
  if (file)
    {
# ifndef PATH_MAX
#  ifdef MAXPATHLEN
#   define PATH_MAX MAXPATHLEN
#  else
#   define PATH_MAX 2048
#  endif
# endif
      char buf[PATH_MAX];
      if (realpath (file, buf))
        return strdup (buf);

/*      sprintf (buf, "%.100s: realpath %.200s", blurb(), file);
      perror(buf);*/
    }
# endif /* HAVE_REALPATH */
  return 0;
}


static Bool
i_am_a_nobody (uid_t uid)
{
  struct passwd *p;

  p = getpwnam ("nobody");
  if (! p) p = getpwnam ("noaccess");
  if (! p) p = getpwnam ("daemon");

  if (! p) /* There is no nobody? */
    return False;

  return (uid == p->pw_uid);
}


const char *
init_file_name (void)
{
  static char *file = 0;

  if (!file)
    {
      uid_t uid = getuid ();
      const char *home = getenv("HOME");

      if (i_am_a_nobody (uid) || !home || !*home)
	{
	  /* If we're running as nobody, then use root's .xscreensaver file
	     (since ~root/.xscreensaver and ~nobody/.xscreensaver are likely
	     to be different -- without this, xscreensaver-settings would
	     appear to have no effect when the luser is running as root.)
	   */
          struct passwd *p = getpwuid (uid);
	  if (!p || !p->pw_name || !*p->pw_name)
	    {
	      fprintf (stderr, "%s: couldn't get user info of uid %d\n",
		       blurb(), getuid ());
	    }
	  else if (!p->pw_dir || !*p->pw_dir)
	    {
	      fprintf (stderr, "%s: couldn't get home directory of \"%s\"\n",
		       blurb(), (p->pw_name ? p->pw_name : "???"));
	    }
	  else
	    {
	      home = p->pw_dir;
	    }
	}
      if (home && *home)
	{
	  const char *name = ".xscreensaver";
	  file = (char *) malloc(strlen(home) + strlen(name) + 2);
	  strcpy(file, home);
	  if (!*home || home[strlen(home)-1] != '/')
	    strcat(file, "/");
	  strcat(file, name);
	}
      else
	{
	  file = "";
	}
    }

  if (file && *file)
    return file;
  else
    return 0;
}


static const char *
init_file_tmp_name (void)
{
  static char *file = 0;
  if (!file)
    {
      const char *name = init_file_name();
      const char *suffix = ".tmp";

      char *n2 = chase_symlinks (name);
      if (n2) name = n2;

      if (!name || !*name)
	file = "";
      else
	{
	  file = (char *) malloc(strlen(name) + strlen(suffix) + 2);
	  strcpy(file, name);
	  strcat(file, suffix);
	}

      if (n2) free (n2);
    }

  if (file && *file)
    return file;
  else
    return 0;
}

static const char * const prefs[] = {
  "timeout",
  "cycle",
  "lock",
  "lockVTs",			/* not saved */
  "lockTimeout",
  "passwdTimeout",
  "visualID",
  "installColormap",
  "verbose",
  "splash",
  "splashDuration",
  "quad",
  "demoCommand",
  "prefsCommand",
  "newLoginCommand",
  "helpURL",			/* not saved */
  "loadURL",			/* not saved */
  "newLoginCommand",		/* not saved */
  "nice",
  "memoryLimit",		/* not saved */
  "fade",
  "unfade",
  "fadeSeconds",
  "fadeTicks",			/* not saved */
  "logFile",			/* not saved */
  "ignoreUninstalledPrograms",
  "font",
  "dpmsEnabled",
  "dpmsQuickOff",
  "dpmsStandby",
  "dpmsSuspend",
  "dpmsOff",
  "grabDesktopImages",
  "grabVideoFrames",
  "chooseRandomImages",
  "imageDirectory",
  "mode",
  "selected",
  "textMode",
  "textLiteral",
  "textFile",
  "textProgram",
  "textURL",
  "dialogTheme",
  "",
  "programs",
  "",
  "pointerHysteresis",
  "bourneShell",		/* not saved -- X resources only */
  "authWarningSlack",
  0
};

static char *
strip (char *s)
{
  char *s2;
  while (*s == '\t' || *s == ' ' || *s == '\r' || *s == '\n')
    s++;
  for (s2 = s; *s2; s2++)
    ;
  for (s2--; s2 >= s; s2--) 
    if (*s2 == '\t' || *s2 == ' ' || *s2 == '\r' || *s2 =='\n') 
      *s2 = 0;
    else
      break;
  return s;
}


/* Reading
 */

static int
handle_entry (XrmDatabase *db, const char *key, const char *value,
	      const char *filename, int line)
{
  int i;
  for (i = 0; prefs[i]; i++)
    if (*prefs[i] && !strcasecmp(key, prefs[i]))
      {
	char *val = strdup(value);
	char *spec = (char *) malloc(strlen(progclass) + strlen(prefs[i]) +10);
	strcpy(spec, progclass);
	strcat(spec, ".");
	strcat(spec, prefs[i]);

	XrmPutStringResource (db, spec, val);

	free(spec);
	free(val);
	return 0;
      }

  /* fprintf(stderr, "%s: %s:%d: unknown option \"%s\"\n",
	  blurb(), filename, line, key); */
  return 1;
}


struct parser_closure {
  const char *file;
  saver_preferences *prefs;
};

static void line_handler (int lineno, 
                          const char *key, const char *val,
                          void *closure)
{
  struct parser_closure *c = (struct parser_closure *) closure;
  saver_preferences *p = c->prefs;
  if (!p->db) abort();
  handle_entry (&p->db, key, val, c->file, lineno);
}

static int
parse_init_file_1 (saver_preferences *p)
{
  time_t write_date = 0;
  const char *name = init_file_name();
  struct parser_closure C;
  struct stat st;
  int status;

  if (!name) return 0;

  C.file = name;
  C.prefs = p;

  if (stat(name, &st) == 0)
    write_date = st.st_mtime;
  else
    {
      p->init_file_date = 0;
      return 0;
    }

  status = parse_init_file (name, line_handler, &C);

  p->init_file_date = write_date;
  return status;
}


Bool
init_file_changed_p (saver_preferences *p)
{
  const char *name = init_file_name();
  struct stat st;

  if (!name) return False;

  if (stat(name, &st) != 0)
    return False;

  if (p->init_file_date == st.st_mtime)
    return False;

  return True;
}


/* Writing
 */

static int
tab_to (FILE *out, int from, int to)
{
  int tab_width = 8;
  int to_mod = (to / tab_width) * tab_width;
  while (from < to_mod)
    {
      fprintf(out, "\t");
      from = (((from / tab_width) + 1) * tab_width);
    }
  while (from < to)
    {
      fprintf(out, " ");
      from++;
    }
  return from;
}

static char *
stab_to (char *out, int from, int to)
{
  int tab_width = 8;
  int to_mod = (to / tab_width) * tab_width;
  while (from < to_mod)
    {
      *out++ = '\t';
      from = (((from / tab_width) + 1) * tab_width);
    }
  while (from < to)
    {
      *out++ = ' ';
      from++;
    }
  return out;
}

static int
string_columns (const char *string, int length, int start)
{
  int tab_width = 8;
  int col = start;
  const char *end = string + length;
  while (string < end)
    {
      if (*string == '\n')
        col = 0;
      else if (*string == '\t')
        col = (((col / tab_width) + 1) * tab_width);
      else
        col++;
      string++;
    }
  return col;
}


static void
write_entry (FILE *out, const char *key, const char *value)
{
  char *v = strdup(value ? value : "");
  char *v2 = v;
  char *nl = 0;
  int col;
  Bool programs_p = (!strcmp(key, "programs"));
  int tab = (programs_p ? 32 : 16);
  Bool first = True;

  fprintf(out, "%s:", key);
  col = strlen(key) + 1;

  if (strlen(key) > 14)
    col = tab_to (out, col, 20);

  while (1)
    {
      if (!programs_p)
        v2 = strip(v2);
      nl = strchr(v2, '\n');
      if (nl)
	*nl = 0;

      if (first && programs_p)
        {
	  col = tab_to (out, col, 77);
	  fprintf (out, " \\\n");
	  col = 0;
        }

      if (first)
	first = False;
      else
	{
	  col = tab_to (out, col, 75);
	  fprintf (out, " \\n\\\n");
	  col = 0;
	}

      if (!programs_p)
        col = tab_to (out, col, tab);

      if (programs_p &&
	  string_columns(v2, strlen (v2), col) + col > 75)
	{
	  int L = strlen (v2);
	  int start = 0;
	  int end = start;
	  while (start < L)
	    {
	      while (v2[end] == ' ' || v2[end] == '\t')
		end++;
	      while (v2[end] != ' ' && v2[end] != '\t' &&
		     v2[end] != '\n' && v2[end] != 0)
		end++;
	      if (string_columns (v2 + start, (end - start), col) >= 74)
		{
		  col = tab_to (out, col, 75);
		  fprintf(out, "   \\\n");
		  col = tab_to (out, 0, tab + 2);
		  while (v2[start] == ' ' || v2[start] == '\t')
		    start++;
		}

              col = string_columns (v2 + start, (end - start), col);
	      while (start < end)
                fputc(v2[start++], out);
	    }
	}
      else
	{
	  fprintf (out, "%s", v2);
	  col += string_columns(v2, strlen (v2), col);
	}

      if (nl)
	v2 = nl + 1;
      else
	break;
    }

  fprintf(out, "\n");
  free(v);
}

int
write_init_file (Display *dpy,
                 saver_preferences *p, const char *version_string,
                 Bool verbose_p)
{
  int status = -1;
  const char *name = init_file_name();
  const char *tmp_name = init_file_tmp_name();
  char *n2 = chase_symlinks (name);
  struct stat st;
  int i, j;

  /* Kludge, since these aren't in the saver_preferences struct as strings...
   */
  char *visual_name;
  char *programs;
  Bool overlay_stderr_p;
  char *stderr_font;
  FILE *out;

  if (!name) goto END;

  if (n2) name = n2;

  /* Throttle the various timeouts to reasonable values before writing
     the file to disk. */
  stop_the_insanity (p);


  if (verbose_p)
    fprintf (stderr, "%s: writing \"%s\"\n", blurb(), name);

  unlink (tmp_name);
  out = fopen(tmp_name, "w");
  if (!out)
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: error writing \"%s\"", blurb(), name);
      perror(buf);
      free(buf);
      goto END;
    }

  /* Give the new .xscreensaver file the same permissions as the old one;
     except ensure that it is readable and writable by owner, and not
     executable.  Extra hack: if we're running as root, make the file
     be world-readable (so that the daemon, running as "nobody", will
     still be able to read it.)
   */
  if (stat(name, &st) == 0)
    {
      mode_t mode = st.st_mode;
      mode |= S_IRUSR | S_IWUSR;		/* read/write by user */
      mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH);	/* executable by none */

      if (getuid() == (uid_t) 0)		/* read by group/other */
        mode |= S_IRGRP | S_IROTH;

      if (fchmod (fileno(out), mode) != 0)
	{
	  char *buf = (char *) malloc(1024 + strlen(name));
	  sprintf (buf, "%s: error fchmodding \"%s\" to 0%o", blurb(),
		   tmp_name, (unsigned int) mode);
	  perror(buf);
	  free(buf);
	  goto END;
	}
    }

  /* Kludge, since these aren't in the saver_preferences struct... */
  visual_name = get_string_resource (dpy, "visualID", "VisualID");
  programs = 0;
  overlay_stderr_p = get_boolean_resource (dpy, "overlayStderr", "Boolean");
  stderr_font = get_string_resource (dpy, "font", "Font");

  i = 0;
  {
    char *ss;
    char **hack_strings = (char **)
      calloc (p->screenhacks_count, sizeof(char *));

    for (j = 0; j < p->screenhacks_count; j++)
      {
        hack_strings[j] = format_hack (dpy, p->screenhacks[j], True);
        i += strlen (hack_strings[j]);
        i += 2;
      }

    ss = programs = (char *) malloc(i + 10);
    *ss = 0;
    for (j = 0; j < p->screenhacks_count; j++)
      {
        strcat (ss, hack_strings[j]);
        free (hack_strings[j]);
	ss += strlen(ss);
	*ss++ = '\n';
	*ss = 0;
      }
    free (hack_strings);
  }

  {
    struct passwd *pw = getpwuid (getuid ());
    char *whoami = (pw && pw->pw_name && *pw->pw_name
		    ? pw->pw_name
		    : "<unknown>");
    time_t now = time ((time_t *) 0);
    char *timestr = (char *) ctime (&now);
    char *nl = (char *) strchr (timestr, '\n');
    if (nl) *nl = 0;
    fprintf (out,
	     "# %s Preferences File\n"
	     "# Written by %s %s for %s on %s.\n"
	     "# https://www.jwz.org/xscreensaver/\n"
	     "\n",
	     progclass, progname, version_string, whoami, timestr);
  }

  for (j = 0; prefs[j]; j++)
    {
      char buf[255];
      const char *pr = prefs[j];
      enum pref_type { pref_str, pref_int, pref_bool, pref_byte, pref_time
      } type = pref_str;
      const char *s = 0;
      int i = 0;
      Bool b = False;
      Time t = 0;

      if (pr && !*pr)
	{
	  fprintf(out, "\n");
	  continue;
	}

# undef CHECK
# define CHECK(X) else if (!strcmp(pr, X))
      if (!pr || !*pr)		;
      CHECK("timeout")		type = pref_time, t = p->timeout;
      CHECK("cycle")		type = pref_time, t = p->cycle;
      CHECK("lock")		type = pref_bool, b = p->lock_p;
      CHECK("lockVTs")		continue;  /* don't save, unused */
      CHECK("lockTimeout")	type = pref_time, t = p->lock_timeout;
      CHECK("passwdTimeout")	type = pref_time, t = p->passwd_timeout;
      CHECK("visualID")		type = pref_str,  s =    visual_name;
      CHECK("installColormap")	type = pref_bool, b = p->install_cmap_p;
      CHECK("verbose")		type = pref_bool, b = p->verbose_p;
      CHECK("splash")		type = pref_bool, b = p->splash_p;
      CHECK("splashDuration")	type = pref_time, t = p->splash_duration;
      CHECK("quad")		continue;  /* don't save */
      CHECK("demoCommand")	type = pref_str,  s = p->demo_command;
      CHECK("prefsCommand")	continue;  /* don't save, unused */
      CHECK("helpURL")		continue;  /* don't save */
      CHECK("loadURL")		continue;  /* don't save */
      CHECK("newLoginCommand")	continue;  /* don't save */
      CHECK("dialogTheme")      type = pref_str,  s = p->dialog_theme;
      CHECK("nice")		type = pref_int,  i = p->nice_inferior;
      CHECK("memoryLimit")	continue;  /* don't save */
      CHECK("fade")		type = pref_bool, b = p->fade_p;
      CHECK("unfade")		type = pref_bool, b = p->unfade_p;
      CHECK("fadeSeconds")	type = pref_time, t = p->fade_seconds;
      CHECK("fadeTicks")	continue;  /* don't save */
      CHECK("captureStdout")	continue;  /* don't save */
      CHECK("logFile")		continue;  /* don't save */
      CHECK("ignoreUninstalledPrograms")
                                type = pref_bool, b = p->ignore_uninstalled_p;

      CHECK("font")		type = pref_str,  s =    stderr_font;

      CHECK("dpmsEnabled")	type = pref_bool, b = p->dpms_enabled_p;
      CHECK("dpmsQuickOff")	type = pref_bool, b = p->dpms_quickoff_p;
      CHECK("dpmsStandby")	type = pref_time, t = p->dpms_standby;
      CHECK("dpmsSuspend")	type = pref_time, t = p->dpms_suspend;
      CHECK("dpmsOff")		type = pref_time, t = p->dpms_off;

      CHECK("grabDesktopImages") type =pref_bool, b = p->grab_desktop_p;
      CHECK("grabVideoFrames")   type =pref_bool, b = p->grab_video_p;
      CHECK("chooseRandomImages")type =pref_bool, b = p->random_image_p;
      CHECK("imageDirectory")    type =pref_str,  s = p->image_directory;

      CHECK("mode")             type = pref_str,
                                s = (p->mode == ONE_HACK ? "one" :
                                     p->mode == BLANK_ONLY ? "blank" :
                                     p->mode == DONT_BLANK ? "off" :
                                     p->mode == RANDOM_HACKS_SAME
                                     ? "random-same"
                                     : "random");
      CHECK("selected")         type = pref_int,  i = p->selected_hack;

      CHECK("textMode")         type = pref_str,
                                s = (p->tmode == TEXT_URL     ? "url" :
                                     p->tmode == TEXT_LITERAL ? "literal" :
                                     p->tmode == TEXT_FILE    ? "file" :
                                     p->tmode == TEXT_PROGRAM ? "program" :
                                     "date");
      CHECK("textLiteral")      type = pref_str,  s = p->text_literal;
      CHECK("textFile")         type = pref_str,  s = p->text_file;
      CHECK("textProgram")      type = pref_str,  s = p->text_program;
      CHECK("textURL")          type = pref_str,  s = p->text_url;

      CHECK("programs")		type = pref_str,  s =    programs;
      CHECK("pointerHysteresis")type = pref_int,  i = p->pointer_hysteresis;
      CHECK("overlayStderr")	type = pref_bool, b = overlay_stderr_p;
      CHECK("overlayTextBackground") continue;  /* don't save */
      CHECK("overlayTextForeground") continue;  /* don't save */
      CHECK("bourneShell")	     continue;  /* don't save */
      CHECK("authWarningSlack") type = pref_int, i = p->auth_warning_slack;
      else
        {
          fprintf (stderr, "%s: internal error: key %s\n", blurb(), pr);
          abort();
        }
# undef CHECK

      switch (type)
	{
	case pref_str:
	  break;
	case pref_int:
	  sprintf(buf, "%d", i);
	  s = buf;
	  break;
	case pref_bool:
	  s = b ? "True" : "False";
	  break;
	case pref_time:
	  {
	    unsigned int hour = 0, min = 0, sec = (unsigned int) (t/1000);
	    if (sec >= 60)
	      {
		min += (sec / 60);
		sec %= 60;
	      }
	    if (min >= 60)
	      {
		hour += (min / 60);
		min %= 60;
	      }
	    sprintf (buf, "%u:%02u:%02u", hour, min, sec);
	    s = buf;
	  }
	  break;
	case pref_byte:
	  {
            if      (i >= (1<<30) && i == ((i >> 30) << 30))
              sprintf(buf, "%dG", i >> 30);
            else if (i >= (1<<20) && i == ((i >> 20) << 20))
              sprintf(buf, "%dM", i >> 20);
            else if (i >= (1<<10) && i == ((i >> 10) << 10))
              sprintf(buf, "%dK", i >> 10);
            else
              sprintf(buf, "%d", i);
            s = buf;
          }
	  break;
	default:
	  abort();
	  break;
	}

      if (pr && (!strcmp(pr, "mode") || !strcmp(pr, "textMode")))
        fprintf(out, "\n");

      write_entry (out, pr, s);
    }

  fprintf(out, "\n");

  if (visual_name) free(visual_name);
  if (stderr_font) free(stderr_font);
  if (programs) free(programs);

  if (fclose(out) == 0)
    {
      time_t write_date = 0;

      if (stat(tmp_name, &st) == 0)
	{
	  write_date = st.st_mtime;
	}
      else
	{
	  char *buf = (char *) malloc(1024 + strlen(tmp_name) + strlen(name));
	  sprintf(buf, "%s: couldn't stat \"%s\"", blurb(), tmp_name);
	  perror(buf);
	  unlink (tmp_name);
	  free(buf);
	  goto END;
	}

      if (rename (tmp_name, name) != 0)
	{
	  char *buf = (char *) malloc(1024 + strlen(tmp_name) + strlen(name));
	  sprintf(buf, "%s: error renaming \"%s\" to \"%s\"",
		  blurb(), tmp_name, name);
	  perror(buf);
	  unlink (tmp_name);
	  free(buf);
	  goto END;
	}
      else
	{
	  p->init_file_date = write_date;

	  /* Since the .xscreensaver file is used for IPC, let's try and make
	     sure that the bits actually land on the disk right away. */
          /* Update 2020: Apparently here in the future, this sometimes takes
             3+ seconds, so let's not. */
	  /* sync(); */

          status = 0;    /* wrote and renamed successfully! */
	}
    }
  else
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: error closing \"%s\"", blurb(), name);
      perror(buf);
      free(buf);
      unlink (tmp_name);
      goto END;
    }

 END:
  if (n2) free (n2);
  return status;
}


/* Parsing the resource database
 */

static void
free_screenhack (screenhack *hack)
{
  if (hack->visual) free (hack->visual);
  if (hack->name) free (hack->name);
  free (hack->command);
  memset (hack, 0, sizeof(*hack));
  free (hack);
}

static void
free_screenhack_list (screenhack **list, int count)
{
  int i;
  if (!list) return;
  for (i = 0; i < count; i++)
    if (list[i])
      free_screenhack (list[i]);
  free (list);
}



/* Populate `saver_preferences' with the contents of the resource database.
   Note that this may be called multiple times -- it is re-run each time
   the ~/.xscreensaver file is reloaded.

   This function can be very noisy, since it issues resource syntax errors
   and so on.
 */
void
load_init_file (Display *dpy, saver_preferences *p)
{
  static Bool first_time = True;
  
  screenhack **system_default_screenhacks = 0;
  int system_default_screenhack_count = 0;

  if (first_time)
    {
      /* Get the programs resource before the .xscreensaver file has been
         parsed and merged into the resource database for the first time:
         this is the value of *programs from the app-defaults file.
         Then clear it out so that it will be parsed again later, after
         the init file has been read.
       */
      get_screenhacks (dpy, p);
      system_default_screenhacks = p->screenhacks;
      system_default_screenhack_count = p->screenhacks_count;
      p->screenhacks = 0;
      p->screenhacks_count = 0;
    }

  if (parse_init_file_1 (p) != 0)	/* file might have gone away */
    if (!first_time) return;

  first_time = False;

  p->xsync_p	    = get_boolean_resource (dpy, "synchronous", "Synchronous");
  p->verbose_p	    = get_boolean_resource (dpy, "verbose", "Boolean");
  p->lock_p	    = get_boolean_resource (dpy, "lock", "Boolean");
  p->fade_p	    = get_boolean_resource (dpy, "fade", "Boolean");
  p->unfade_p	    = get_boolean_resource (dpy, "unfade", "Boolean");
  p->fade_seconds   = 1000 * get_seconds_resource (dpy, "fadeSeconds", "Time");
  p->install_cmap_p = get_boolean_resource (dpy, "installColormap", "Boolean");
  p->nice_inferior  = get_integer_resource (dpy, "nice", "Nice");
  p->splash_p       = get_boolean_resource (dpy, "splash", "Boolean");
  p->ignore_uninstalled_p = get_boolean_resource (dpy, 
                                                  "ignoreUninstalledPrograms",
                                                  "Boolean");

  p->splash_duration = 1000 * get_seconds_resource (dpy, "splashDuration", "Time");
  p->timeout         = 1000 * get_minutes_resource (dpy, "timeout", "Time");
  p->lock_timeout    = 1000 * get_minutes_resource (dpy, "lockTimeout", "Time");
  p->cycle           = 1000 * get_minutes_resource (dpy, "cycle", "Time");
  p->passwd_timeout  = 1000 * get_seconds_resource (dpy, "passwdTimeout", "Time");
  p->pointer_hysteresis = get_integer_resource (dpy, "pointerHysteresis","Integer");

  p->dpms_enabled_p  = get_boolean_resource (dpy, "dpmsEnabled", "Boolean");
  p->dpms_quickoff_p = get_boolean_resource (dpy, "dpmsQuickOff", "Boolean");
  p->dpms_standby    = 1000 * get_minutes_resource (dpy, "dpmsStandby", "Time");
  p->dpms_suspend    = 1000 * get_minutes_resource (dpy, "dpmsSuspend", "Time");
  p->dpms_off        = 1000 * get_minutes_resource (dpy, "dpmsOff",     "Time");

  p->grab_desktop_p  = get_boolean_resource (dpy, "grabDesktopImages",  "Boolean");
  p->grab_video_p    = get_boolean_resource (dpy, "grabVideoFrames",    "Boolean");
  p->random_image_p  = get_boolean_resource (dpy, "chooseRandomImages", "Boolean");
  p->image_directory = get_string_resource  (dpy,
                                             "imageDirectory",
                                             "ImageDirectory");

  p->text_literal = get_string_resource (dpy, "textLiteral", "TextLiteral");
  p->text_file    = get_string_resource (dpy, "textFile",    "TextFile");
  p->text_program = get_string_resource (dpy, "textProgram", "TextProgram");
  p->text_url     = get_string_resource (dpy, "textURL",     "TextURL");

  p->shell = get_string_resource (dpy, "bourneShell", "BourneShell");

  p->demo_command = get_string_resource(dpy, "demoCommand", "Command");
  p->help_url = get_string_resource(dpy, "helpURL", "URL");
  p->load_url_command = get_string_resource(dpy, "loadURL", "Command");
  p->new_login_command = get_string_resource(dpy, "newLoginCommand",
                                             "Command");
  p->dialog_theme = get_string_resource(dpy, "dialogTheme", "String");
  p->auth_warning_slack = get_integer_resource(dpy, "authWarningSlack",
                                               "Integer");

  /* If "*splash" is unset, default to true. */
  {
    char *s = get_string_resource (dpy, "splash", "Boolean");
    if (s)
      free (s);
    else
      p->splash_p = True;
  }

  /* If "*grabDesktopImages" is unset, default to true. */
  {
    char *s = get_string_resource (dpy, "grabDesktopImages", "Boolean");
    if (s)
      free (s);
    else
      p->grab_desktop_p = True;
  }

  get_screenhacks (dpy, p);             /* Parse the "programs" resource. */

  {
    char *s = get_string_resource (dpy, "selected", "Integer");
    if (!s || !*s)
      p->selected_hack = -1;
    else
      p->selected_hack = get_integer_resource (dpy, "selected", "Integer");
    if (s) free (s);
    if (p->selected_hack < 0 || p->selected_hack >= p->screenhacks_count)
      p->selected_hack = -1;
  }

  {
    char *s = get_string_resource (dpy, "mode", "Mode");
    if      (s && !strcasecmp (s, "one"))         p->mode = ONE_HACK;
    else if (s && !strcasecmp (s, "blank"))       p->mode = BLANK_ONLY;
    else if (s && !strcasecmp (s, "off"))         p->mode = DONT_BLANK;
    else if (s && !strcasecmp (s, "random-same")) p->mode = RANDOM_HACKS_SAME;
    else                                          p->mode = RANDOM_HACKS;
    if (s) free (s);
  }

  {
    char *s = get_string_resource (dpy, "textMode", "TextMode");
    if      (s && !strcasecmp (s, "url"))         p->tmode = TEXT_URL;
    else if (s && !strcasecmp (s, "literal"))     p->tmode = TEXT_LITERAL;
    else if (s && !strcasecmp (s, "file"))        p->tmode = TEXT_FILE;
    else if (s && !strcasecmp (s, "program"))     p->tmode = TEXT_PROGRAM;
    else                                          p->tmode = TEXT_DATE;
    if (s) free (s);
  }

  if (system_default_screenhack_count)  /* note: first_time is also true */
    {
      merge_system_screenhacks (dpy, p, system_default_screenhacks,
                                system_default_screenhack_count);
      free_screenhack_list (system_default_screenhacks,
                            system_default_screenhack_count);
      system_default_screenhacks = 0;
      system_default_screenhack_count = 0;
    }

  if (p->debug_p)
    {
      p->xsync_p = True;
      p->verbose_p = True;
    }

  /* Throttle the various timeouts to reasonable values after reading the
     disk file. */
  stop_the_insanity (p);
}


/* If there are any hacks in the system-wide defaults that are not in
   the ~/.xscreensaver file, add the new ones to the end of the list.
   This does *not* actually save the file.
 */
static void
merge_system_screenhacks (Display *dpy, saver_preferences *p,
                          screenhack **system_list, int system_count)
{
  /* Yeah yeah, this is an N^2 operation, but I don't have hashtables handy,
     so fuck it. */

  int made_space = 0;
  int i;
  for (i = 0; i < system_count; i++)
    {
      int j;
      Bool matched_p = False;

      for (j = 0; j < p->screenhacks_count; j++)
        {
          char *name;
          if (!system_list[i]->name)
            system_list[i]->name = make_hack_name (dpy, 
                                                   system_list[i]->command);

          name = p->screenhacks[j]->name;
          if (!name)
            name = make_hack_name (dpy, p->screenhacks[j]->command);

          matched_p = !strcasecmp (name, system_list[i]->name);

          if (name != p->screenhacks[j]->name)
            free (name);

          if (matched_p)
            break;
        }

      if (!matched_p)
        {
          /* We have an entry in the system-wide list that is not in the
             user's .xscreensaver file.  Add it to the end.
             Note that p->screenhacks is a single malloc block, not a
             linked list, so we have to realloc it.
           */
          screenhack *oh = system_list[i];
          screenhack *nh = (screenhack *) malloc (sizeof(screenhack));

          if (made_space == 0)
            {
              made_space = 10;
              p->screenhacks = (screenhack **)
                realloc (p->screenhacks,
                         (p->screenhacks_count + made_space + 1)
                         * sizeof(*p->screenhacks));
              if (!p->screenhacks) abort();
            }

          nh->enabled_p = oh->enabled_p;
          nh->visual    = oh->visual  ? strdup(oh->visual)  : 0;
          nh->name      = oh->name    ? strdup(oh->name)    : 0;
          nh->command   = oh->command ? strdup(oh->command) : 0;

          p->screenhacks[p->screenhacks_count++] = nh;
          p->screenhacks[p->screenhacks_count] = 0;
          made_space--;

#if 0
          fprintf (stderr, "%s: noticed new hack: %s\n", blurb(),
                   (nh->name ? nh->name : make_hack_name (dpy, nh->command)));
#endif
        }
    }
}



/* Parsing the programs resource.
 */

static screenhack *
parse_screenhack (const char *line)
{
  screenhack *h = (screenhack *) calloc (1, sizeof(*h));
  const char *s;

  h->enabled_p = True;

  while (isspace(*line)) line++;		/* skip whitespace */
  if (*line == '-')				/* handle "-" */
    {
      h->enabled_p = False;
      line++;
      while (isspace(*line)) line++;		/* skip whitespace */
    }

  s = line;					/* handle "visual:" */
  while (*line && *line != ':' && *line != '"' && !isspace(*line))
    line++;
  if (*line != ':')
    line = s;
  else
    {
      h->visual = (char *) malloc (line-s+1);
      strncpy (h->visual, s, line-s);
      h->visual[line-s] = 0;
      if (*line == ':') line++;			/* skip ":" */
      while (isspace(*line)) line++;		/* skip whitespace */
    }

  if (*line == '"')				/* handle "name" */
    {
      line++;
      s = line;
      while (*line && *line != '"')
        line++;
      h->name = (char *) malloc (line-s+1);
      strncpy (h->name, s, line-s);
      h->name[line-s] = 0;
      if (*line == '"') line++;			/* skip "\"" */
      while (isspace(*line)) line++;		/* skip whitespace */
    }

  h->command = format_command (line, False);	/* handle command */
  return h;
}


static char *
format_command (const char *cmd, Bool wrap_p)
{
  int tab = 30;
  int col = tab;
  char *cmd2 = (char *) calloc (1, 2 * (strlen (cmd) + 1));
  const char *in = cmd;
  char *out = cmd2;
  while (*in)
    {
      /* shrink all whitespace to one space, for the benefit of the "demo"
	 mode display.  We only do this when we can easily tell that the
	 whitespace is not significant (no shell metachars).
       */
      switch (*in)
	{
	case '\'': case '"': case '`': case '\\':
          /* Metachars are scary.  Copy the rest of the line unchanged. */
          while (*in)
            *out++ = *in++, col++;
	  break;

	case ' ': case '\t':
          /* Squeeze all other whitespace down to one space. */
          while (*in == ' ' || *in == '\t')
            in++;
          *out++ = ' ', col++;
	  break;

	default:
          /* Copy other chars unchanged. */
	  *out++ = *in++, col++;
	  break;
	}
    }

  *out = 0;

  /* Strip trailing whitespace */
  while (out > cmd2 && isspace (out[-1]))
    *(--out) = 0;

  return cmd2;
}


/* Returns a new string describing the shell command.
   This may be just the name of the program, capitalized.
   It also may be something from the resource database (gotten
   by looking for "hacks.XYZ.name", where XYZ is the program.)
 */
char *
make_hack_name (Display *dpy, const char *shell_command)
{
  char *s = strdup (shell_command);
  char *s2;
  char res_name[255];

  for (s2 = s; *s2; s2++)	/* truncate at first whitespace */
    if (isspace (*s2))
      {
        *s2 = 0;
        break;
      }

  s2 = strrchr (s, '/');	/* if pathname, take last component */
  if (s2)
    {
      s2 = strdup (s2+1);
      free (s);
      s = s2;
    }

  if (strlen (s) > 50)		/* 51 is hereby defined as "unreasonable" */
    s[50] = 0;

  sprintf (res_name, "hacks.%s.name", s);		/* resource? */
  s2 = get_string_resource (dpy, res_name, res_name);
  if (s2)
    {
      free (s);
      return s2;
    }

  for (s2 = s; *s2; s2++)	/* if it has any capitals, return it */
    if (*s2 >= 'A' && *s2 <= 'Z')
      return s;

  if (s[0] >= 'a' && s[0] <= 'z')			/* else cap it */
    s[0] -= 'a'-'A';
  if (s[0] == 'X' && s[1] >= 'a' && s[1] <= 'z')	/* (magic leading X) */
    s[1] -= 'a'-'A';
  if (s[0] == 'G' && s[1] == 'l' && 
      s[2] >= 'a' && s[2] <= 'z')		       /* (magic leading GL) */
    s[1] -= 'a'-'A',
    s[2] -= 'a'-'A';
  return s;
}


static char *
format_hack (Display *dpy, screenhack *hack, Bool wrap_p)
{
  int tab = 32;
  int size;
  char *h2, *out, *s;
  int col = 0;

  char *def_name = make_hack_name (dpy, hack->command);

  /* Don't ever write out a name for a hack if it's the same as the default.
   */
  if (hack->name && !strcmp (hack->name, def_name))
    {
      free (hack->name);
      hack->name = 0;
    }
  free (def_name);

  size = (2 * (strlen(hack->command) +
               (hack->visual ? strlen(hack->visual) : 0) +
               (hack->name ? strlen(hack->name) : 0) +
               tab));
  h2 = (char *) malloc (size);
  out = h2;

  if (!hack->enabled_p) *out++ = '-';		/* write disabled flag */

  if (hack->visual && *hack->visual)		/* write visual name */
    {
      if (hack->enabled_p) *out++ = ' ';
      *out++ = ' ';
      strcpy (out, hack->visual);
      out += strlen (hack->visual);
      *out++ = ':';
      *out++ = ' ';
    }

  *out = 0;
  col = string_columns (h2, strlen (h2), 0);

  if (hack->name && *hack->name)		/* write pretty name */
    {
      int L = (strlen (hack->name) + 2);
      if (L + col < tab)
        out = stab_to (out, col, tab - L - 2);
      else
        *out++ = ' ';
      *out++ = '"';
      strcpy (out, hack->name);
      out += strlen (hack->name);
      *out++ = '"';
      *out = 0;

      col = string_columns (h2, strlen (h2), 0);
      if (wrap_p && col >= tab)
        out = stab_to (out, col, 77);
      else
        *out++ = ' ';

      if (out >= h2+size) abort();
    }

  *out = 0;
  col = string_columns (h2, strlen (h2), 0);
  out = stab_to (out, col, tab);		/* indent */

  if (out >= h2+size) abort();
  s = format_command (hack->command, wrap_p);
  strcpy (out, s);
  out += strlen (s);
  free (s);
  *out = 0;

  return h2;
}


static void
get_screenhacks (Display *dpy, saver_preferences *p)
{
  int i, j;
  int start = 0;
  int end = 0;
  int size;
  char *d = get_string_resource (dpy, "programs", "Programs");

  free_screenhack_list (p->screenhacks, p->screenhacks_count);
  p->screenhacks = 0;
  p->screenhacks_count = 0;

  if (!d || !*d)
    return;

  size = strlen (d);


  /* Count up the number of newlines (which will be equal to or larger than
     one less than the number of hacks.)
   */
  for (i = j = 0; d[i]; i++)
    if (d[i] == '\n')
      j++;
  j++;

  p->screenhacks = (screenhack **) calloc (j + 1, sizeof (screenhack *));

  /* Iterate over the lines in `d' (the string with newlines)
     and make new strings to stuff into the `screenhacks' array.
   */
  p->screenhacks_count = 0;
  while (start < size)
    {
      /* skip forward over whitespace. */
      while (d[start] == ' ' || d[start] == '\t' || d[start] == '\n')
	start++;

      /* skip forward to newline or end of string. */
      end = start;
      while (d[end] != 0 && d[end] != '\n')
	end++;

      /* null terminate. */
      d[end] = 0;

      p->screenhacks[p->screenhacks_count++] = parse_screenhack (d + start);
      if (p->screenhacks_count >= i)
	abort();

      start = end+1;
    }

  free (d);

  if (p->screenhacks_count == 0)
    {
      free (p->screenhacks);
      p->screenhacks = 0;
    }
}


/* Make sure all the values in the preferences struct are sane.
 */
static void
stop_the_insanity (saver_preferences *p)
{
  if (p->passwd_timeout <= 0) p->passwd_timeout = 30000;	 /* 30 secs */
  if (p->timeout < 15000) p->timeout = 15000;			 /* 15 secs */
  if (p->cycle != 0 && p->cycle < 2000) p->cycle = 2000;	 /*  2 secs */
  if (p->fade_seconds <= 0)
    p->fade_p = False;
  if (! p->fade_p) p->unfade_p = False;

  /* The DPMS settings may have the value 0.
     But if they are negative, or are a range less than 10 seconds,
     reset them to sensible defaults.  (Since that must be a mistake.)
   */
  if (p->dpms_standby != 0 &&
      p->dpms_standby < 10 * 1000)
    p->dpms_standby =  2 * 60 * 60 * 1000;			 /* 2 hours */
  if (p->dpms_suspend != 0 &&
      p->dpms_suspend < 10 * 1000)
    p->dpms_suspend =  2 * 60 * 60 * 1000;			 /* 2 hours */
  if (p->dpms_off != 0 &&
      p->dpms_off < 10 * 1000)
    p->dpms_off      = 4 * 60 * 60 * 1000;			 /* 4 hours */

  /* suspend may not be greater than off, unless off is 0.
     standby may not be greater than suspend, unless suspend is 0.
   */
  if (p->dpms_off != 0 &&
      p->dpms_suspend > p->dpms_off)
    p->dpms_suspend = p->dpms_off;
  if (p->dpms_suspend != 0 &&
      p->dpms_standby > p->dpms_suspend)
    p->dpms_standby = p->dpms_suspend;

  /* These fixes above ignores the case
     suspend = 0 and standby > off ...
   */
  if (p->dpms_off != 0 &&
      p->dpms_standby > p->dpms_off)
    p->dpms_standby = p->dpms_off;

  if (p->dpms_standby == 0 &&	   /* if *all* are 0, then DPMS is disabled */
      p->dpms_suspend == 0 &&
      p->dpms_off     == 0 &&
      !p->dpms_quickoff_p)         /* ... but we want to do DPMS quick off */
    p->dpms_enabled_p = False;


  /* Set watchdog timeout to about half of the cycle timeout, but
     don't let it be faster than 1/2 minute or slower than 1 minute.
   */
  p->watchdog_timeout = p->cycle * 0.6;
  if (p->watchdog_timeout < 27000) p->watchdog_timeout = 27000;	  /* 27 secs */
  if (p->watchdog_timeout > 57000) p->watchdog_timeout = 57000;   /* 57 secs */

  if (p->pointer_hysteresis < 0)   p->pointer_hysteresis = 0;

  if (p->auth_warning_slack < 0)   p->auth_warning_slack = 0;
  if (p->auth_warning_slack > 300) p->auth_warning_slack = 300;
}


Bool
senescent_p (void)
{
  /* If you are in here because you're planning on disabling this warning
     before redistributing my software, please don't.

     I sincerely request that you do one of the following:

         1: leave this code intact and this warning in place, -OR-

         2: Remove xscreensaver from your distribution.

     I would seriously prefer that you not distribute my software at all
     than that you distribute one version and then never update it for
     years.

     I am *constantly* getting email from users reporting bugs that have
     been fixed for literally years who have no idea that the software
     they are running is years out of date.  Yes, it would be great if we
     lived in the ideal world where people checked that they were running
     the latest release before they report a bug, but we don't.  To most
     people, "running the latest release" is synonymous with "running the
     latest release that my distro packages for me."

     When they even bother to tell me what version they're running, I
     say, "That version is three years old!", and they say "But this is
     the latest version my distro ships".  Then I say, "your distro
     sucks", and they say "but I don't know how to compile from source,
     herp derp I eat paste", and *everybody* goes away unhappy.

     It wastes an enormous amount of my time, but worse than that, it
     does a grave disservice to the users, who are stuck experiencing
     bugs that are already fixed!  These users think they are running the
     latest release, and they are not.  They would like to be running the
     actual latest release, but they don't know how, because their distro
     makes that very difficult for them.  It's terrible for everyone, and
     kind of makes me regret ever having released this software in the
     first place.

     So seriously. I ask that if you're planning on disabling this
     obsolescence warning, that you instead just remove xscreensaver from
     your distro entirely.  Everybody will be happier that way.  Check
     out gnome-screensaver instead, I understand it's really nice.

     Of course, my license allows you to ignore me and do whatever the
     fuck you want, but as the author, I hope you will have the common
     courtesy of complying with my request.

     Thank you!

     jwz, 2014, 2016, 2018, 2021.

     PS: In particular, since Debian refuses to upgrade software on any
     kind of rational timeline, I have asked that they stop shipping
     xscreensaver at all.  They have refused.  Instead of upgrading the
     software, they simply patched out this warning.

     If you want to witness the sad state of the open source peanut
     gallery, look no farther than the comments on my blog:
     http://jwz.org/b/yiYo

     Many of these people fall back on their go-to argument of, "If it is
     legal, it must be right."  If you believe in that rhetorical device
     then you are a terrible person, and possibly a sociopath.

     There are also the armchair lawyers who say "Well, instead of
     *asking* people to do the right thing out of common courtesy, you
     should just change your license to prohibit them from acting
     amorally."  Again, this is the answer of a sociopath, but that aside,
     if you devote even a second's thought to this you will realize that
     the end result of this would be for distros like Debian to just keep
     shipping the last version with the old license and then never
     upgrading it again -- which would be the worst possible outcome for
     everyone involved, most especially the users.

     Also, some have incorrectly characterized this as a "time bomb".
     It is a software update notification, nothing more.  A "time bomb"
     makes software stop working.  This merely alerts the user that the
     security-critical software that they are running is dangerously out
     of date.
  */

  time_t now = time ((time_t *) 0);				/*   d   */
  struct tm *tm = localtime (&now);				/*   o   */
  const char *s = screensaver_id;				/*   n   */
  char mon[4], year[5];						/*   '   */
  int m, y, mrnths;						/*   t   */
  s = strchr (s, ' '); if (!s) abort(); s++;			/*       */
  s = strchr (s, '('); if (!s) abort(); s++;			/*   d   */
  s = strchr (s, '-'); if (!s) abort(); s++;			/*   o   */
  strncpy (mon, s, 3);						/*   o   */
  mon[3] = 0;							/*       */
  s = strchr (s, '-'); if (!s) abort(); s++;			/*   e   */
  strncpy (year, s, 4);						/*   e   */
  year[4] = 0;							/*   t   */
  y = atoi (year);						/*   ,   */
  if      (!strcmp(mon, "Jan")) m = 0;				/*       */
  else if (!strcmp(mon, "Feb")) m = 1;				/*   s   */
  else if (!strcmp(mon, "Mar")) m = 2;				/*   t   */
  else if (!strcmp(mon, "Apr")) m = 3;				/*   o   */
  else if (!strcmp(mon, "May")) m = 4;				/*   p   */
  else if (!strcmp(mon, "Jun")) m = 5;				/*   ,   */
  else if (!strcmp(mon, "Jul")) m = 6;				/*       */
  else if (!strcmp(mon, "Aug")) m = 7;				/*   s   */
  else if (!strcmp(mon, "Sep")) m = 8;				/*   t   */
  else if (!strcmp(mon, "Oct")) m = 9;				/*   a   */
  else if (!strcmp(mon, "Nov")) m = 10;				/*   a   */
  else if (!strcmp(mon, "Dec")) m = 11;				/*   a   */
  else abort();							/*   h   */
  mrnths = ((((tm->tm_year + 1900) * 12) + tm->tm_mon) -	/*   h   */
            (y * 12 + m));					/*   h   */
							  	/*   p   */
  return (mrnths >= 17);					/*   .   */
}
