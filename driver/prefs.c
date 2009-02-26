/* dotfile.c --- management of the ~/.xscreensaver file.
 * xscreensaver, Copyright (c) 1998 Jamie Zawinski <jwz@jwz.org>
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
#include <sys/stat.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>

#ifndef VMS
# include <pwd.h>
#else /* VMS */
# include "vms-pwd.h"
#endif /* VMS */


/* This file doesn't need the Xt headers, so stub these types out... */
#undef XtPointer
#define XtAppContext void*
#define XtIntervalId void*
#define XtPointer    void*
#define Widget       void*


/* Just in case there's something pathological about stat.h... */
#ifndef  S_IRUSR
# define S_IRUSR 00400
#endif
#ifndef  S_IWUSR
# define S_IWUSR 00200
#endif
#ifndef  S_IXUSR
# define S_IXUSR 00100
#endif
#ifndef  S_IXGRP
# define S_IXGRP 00010
#endif
#ifndef  S_IXOTH
# define S_IXOTH 00001
#endif


#include "prefs.h"
#include "resources.h"


extern char *progname;
extern char *progclass;
extern const char *blurb (void);



static void get_screenhacks (saver_preferences *p);


const char *
init_file_name (void)
{
  static char *file = 0;

  if (!file)
    {
      struct passwd *p = getpwuid (getuid ());

      if (!p || !p->pw_name || !*p->pw_name)
	{
	  fprintf (stderr, "%s: couldn't get user info of uid %d\n",
		   blurb(), getuid ());
	  file = "";
	}
      else if (!p->pw_dir || !*p->pw_dir)
	{
	  fprintf (stderr, "%s: couldn't get home directory of \"%s\"\n",
		   blurb(), (p->pw_name ? p->pw_name : "???"));
	  file = "";
	}
      else
	{
	  const char *home = p->pw_dir;
	  const char *name = ".xscreensaver";
	  file = (char *) malloc(strlen(home) + strlen(name) + 2);
	  strcpy(file, home);
	  if (!*home || home[strlen(home)-1] != '/')
	    strcat(file, "/");
	  strcat(file, name);
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
      if (!name || !*name)
	file = "";
      else
	{
	  file = (char *) malloc(strlen(name) + strlen(suffix) + 2);
	  strcpy(file, name);
	  strcat(file, suffix);
	}
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
  "lockVTs",
  "lockTimeout",
  "passwdTimeout",
  "visualID",
  "installColormap",
  "verbose",
  "timestamp",
  "splash",			/* not saved -- same as "splashDuration: 0" */
  "splashDuration",
  "demoCommand",
  "prefsCommand",
  "helpURL",
  "loadURL",
  "nice",
  "fade",
  "unfade",
  "fadeSeconds",
  "fadeTicks",
  "captureStderr",
  "captureStdout",		/* not saved -- obsolete */
  "font",
  "",
  "programs",
  "",
  "pointerPollTime",
  "windowCreationTimeout",
  "initialDelay",
  "sgiSaverExtension",
  "mitSaverExtension",
  "xidleExtension",
  "procInterrupts",
  "overlayStderr",
  "overlayTextBackground",	/* not saved -- X resources only */
  "overlayTextForeground",	/* not saved -- X resources only */
  "bourneShell",		/* not saved -- X resources only */
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

  fprintf(stderr, "%s: %s:%d: unknown option \"%s\"\n",
	  blurb(), filename, line, key);
  return 1;
}


static int
parse_init_file (saver_preferences *p)
{
  time_t write_date = 0;
  const char *name = init_file_name();
  int line = 0;
  struct stat st;
  FILE *in;
  int buf_size = 1024;
  char *buf;

  if (!name) return 0;

  if (stat(name, &st) != 0)
    {
      p->init_file_date = 0;
      return 0;
    }

  in = fopen(name, "r");
  if (!in)
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: error reading \"%s\"", blurb(), name);
      perror(buf);
      free(buf);
      return -1;
    }

  if (fstat (fileno(in), &st) == 0)
    {
      write_date = st.st_mtime;
    }
  else
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: couldn't re-stat \"%s\"", blurb(), name);
      perror(buf);
      free(buf);
      return -1;
    }

  buf = (char *) malloc(buf_size);

  while (fgets (buf, buf_size-1, in))
    {
      char *key, *value;
      int L = strlen(buf);

      line++;
      while (L > 2 &&
	     (buf[L-1] != '\n' ||	/* whole line didn't fit in buffer */
	      buf[L-2] == '\\'))	/* or line ended with backslash */
	{
	  if (buf[L-2] == '\\')		/* backslash-newline gets swallowed */
	    {
	      buf[L-2] = 0;
	      L -= 2;
	    }
	  buf_size += 1024;
	  buf = (char *) realloc(buf, buf_size);
	  if (!buf) exit(1);

	  line++;
	  if (!fgets (buf + L, buf_size-L-1, in))
	    break;
	  L = strlen(buf);
	}

      /* Now handle other backslash escapes. */
      {
	int i, j;
	for (i = 0; buf[i]; i++)
	  if (buf[i] == '\\')
	    {
	      switch (buf[i+1])
		{
		case 'n': buf[i] = '\n'; break;
		case 'r': buf[i] = '\r'; break;
		case 't': buf[i] = '\t'; break;
		default:  buf[i] = buf[i+1]; break;
		}
	      for (j = i+2; buf[j]; j++)
		buf[j-1] = buf[j];
	      buf[j-1] = 0;
	    }
      }

      key = strip(buf);

      if (*key == '#' || *key == '!' || *key == ';' ||
	  *key == '\n' || *key == 0)
	continue;

      value = strchr (key, ':');
      if (!value)
	{
	  fprintf(stderr, "%s: %s:%d: unparsable line: %s\n", blurb(),
		  name, line, key);
	  continue;
	}
      else
	{
	  *value++ = 0;
	  value = strip(value);
	}

      if (!p->db) abort();
      handle_entry (&p->db, key, value, name, line);
    }
  free(buf);

  p->init_file_date = write_date;
  return 0;
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

static void
write_entry (FILE *out, const char *key, const char *value)
{
  char *v = strdup(value ? value : "");
  char *v2 = v;
  char *nl = 0;
  int col;
  Bool do_visual_kludge = (!strcmp(key, "programs"));
  Bool do_wrap = do_visual_kludge;
  int tab = (do_visual_kludge ? 16 : 23);
  int tab2 = 3;
  Bool first = True;

  fprintf(out, "%s:", key);
  col = strlen(key) + 1;

  while (1)
    {
      char *s;
      Bool disabled_p = False;

      v2 = strip(v2);
      nl = strchr(v2, '\n');
      if (nl)
	*nl = 0;

      if (do_visual_kludge && *v2 == '-')
	{
	  disabled_p = True;
	  v2++;
	  v2 = strip(v2);
	}

      if (first && disabled_p)
	first = False;

      if (first)
	first = False;
      else
	{
	  col = tab_to(out, col, 75);
	  fprintf(out, " \\n\\\n");
	  col = 0;
	}

      if (disabled_p)
	{
	  fprintf(out, "-");
	  col++;
	}

      s = (do_visual_kludge ? strpbrk(v2, " \t\n:") : 0);
      if (s && *s == ':')
	col = tab_to (out, col, tab2);
      else
	col = tab_to (out, col, tab);

      if (do_wrap &&
	  strlen(v2) + col > 75)
	{
	  int L = strlen(v2);
	  int start = 0;
	  int end = start;
	  while (start < L)
	    {
	      while (v2[end] == ' ' || v2[end] == '\t')
		end++;
	      while (v2[end] != ' ' && v2[end] != '\t' &&
		     v2[end] != '\n' && v2[end] != 0)
		end++;
	      if (col + (end - start) >= 74)
		{
		  col = tab_to (out, col, 75);
		  fprintf(out, "   \\\n");
		  col = tab_to (out, 0, tab + 2);
		  while (v2[start] == ' ' || v2[start] == '\t')
		    start++;
		}

	      while (start < end)
		{
		  fputc(v2[start++], out);
		  col++;
		}
	    }
	}
      else
	{
	  fprintf (out, "%s", v2);
	  col += strlen(v2);
	}

      if (nl)
	v2 = nl + 1;
      else
	break;
    }

  fprintf(out, "\n");
  free(v);
}

void
write_init_file (saver_preferences *p, const char *version_string)
{
  const char *name = init_file_name();
  const char *tmp_name = init_file_tmp_name();
  struct stat st;
  int i, j;

  /* Kludge, since these aren't in the saver_preferences struct as strings...
   */
  char *visual_name;
  char *programs;
  Bool capture_stderr_p;
  Bool overlay_stderr_p;
  char *stderr_font;
  FILE *out;

  if (!name) return;

  if (p->verbose_p)
    fprintf (stderr, "%s: writing \"%s\".\n", blurb(), name);

  unlink (tmp_name);
  out = fopen(tmp_name, "w");
  if (!out)
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: error writing \"%s\"", blurb(), name);
      perror(buf);
      free(buf);
      return;
    }

  /* Give the new .xscreensaver file the same permissions as the old one;
     except ensure that it is readable and writable by owner, and not
     executable.
   */
  if (stat(name, &st) == 0)
    {
      mode_t mode = st.st_mode;
      mode |= S_IRUSR | S_IWUSR;
      mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH);
      if (fchmod (fileno(out), mode) != 0)
	{
	  char *buf = (char *) malloc(1024 + strlen(name));
	  sprintf (buf, "%s: error fchmodding \"%s\" to 0%o", blurb(),
		   tmp_name, (unsigned int) mode);
	  perror(buf);
	  free(buf);
	  return;
	}
    }

  /* Kludge, since these aren't in the saver_preferences struct... */
  visual_name = get_string_resource ("visualID", "VisualID");
  programs = 0;
  capture_stderr_p = get_boolean_resource ("captureStderr", "Boolean");
  overlay_stderr_p = get_boolean_resource ("overlayStderr", "Boolean");
  stderr_font = get_string_resource ("font", "Font");

  i = 0;
  for (j = 0; j < p->screenhacks_count; j++)
    i += strlen(p->screenhacks[j]) + 2;
  {
    char *ss = programs = (char *) malloc(i + 10);
    *ss = 0;
    for (j = 0; j < p->screenhacks_count; j++)
      {
	strcat(ss, p->screenhacks[j]);
	ss += strlen(ss);
	*ss++ = '\n';
	*ss = 0;
      }
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
	     "# http://www.jwz.org/xscreensaver/\n"
	     "\n",
	     progclass, progname, version_string, whoami, timestr);
  }

  for (j = 0; prefs[j]; j++)
    {
      char buf[255];
      const char *pr = prefs[j];
      enum pref_type { pref_str, pref_int, pref_bool, pref_time
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
# if 0 /* #### not ready yet */
      CHECK("lockVTs")		type = pref_bool, b = p->lock_vt_p;
# else
      CHECK("lockVTs")		continue;  /* don't save */
# endif
      CHECK("lockTimeout")	type = pref_time, t = p->lock_timeout;
      CHECK("passwdTimeout")	type = pref_time, t = p->passwd_timeout;
      CHECK("visualID")		type = pref_str,  s =    visual_name;
      CHECK("installColormap")	type = pref_bool, b = p->install_cmap_p;
      CHECK("verbose")		type = pref_bool, b = p->verbose_p;
      CHECK("timestamp")	type = pref_bool, b = p->timestamp_p;
      CHECK("splash")		continue;  /* don't save */
      CHECK("splashDuration")	type = pref_time, t = p->splash_duration;
      CHECK("demoCommand")	type = pref_str,  s = p->demo_command;
      CHECK("prefsCommand")	type = pref_str,  s = p->prefs_command;
      CHECK("helpURL")		type = pref_str,  s = p->help_url;
      CHECK("loadURL")		type = pref_str,  s = p->load_url_command;
      CHECK("nice")		type = pref_int,  i = p->nice_inferior;
      CHECK("fade")		type = pref_bool, b = p->fade_p;
      CHECK("unfade")		type = pref_bool, b = p->unfade_p;
      CHECK("fadeSeconds")	type = pref_time, t = p->fade_seconds;
      CHECK("fadeTicks")	type = pref_int,  i = p->fade_ticks;
      CHECK("captureStderr")	type = pref_bool, b =    capture_stderr_p;
      CHECK("captureStdout")	continue;  /* don't save */
      CHECK("font")		type = pref_str,  s =    stderr_font;
      CHECK("programs")		type = pref_str,  s =    programs;
      CHECK("pointerPollTime")	type = pref_time, t = p->pointer_timeout;
      CHECK("windowCreationTimeout")type=pref_time,t= p->notice_events_timeout;
      CHECK("initialDelay")	type = pref_time, t = p->initial_delay;
      CHECK("sgiSaverExtension")type = pref_bool, b=p->use_sgi_saver_extension;
      CHECK("mitSaverExtension")type = pref_bool, b=p->use_mit_saver_extension;
      CHECK("xidleExtension")	type = pref_bool, b = p->use_xidle_extension;
      CHECK("procInterrupts")	type = pref_bool, b = p->use_proc_interrupts;
      CHECK("overlayStderr")	type = pref_bool, b = overlay_stderr_p;
      CHECK("overlayTextBackground") continue;  /* don't save */
      CHECK("overlayTextForeground") continue;  /* don't save */
      CHECK("bourneShell")	continue;
      else			abort();
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
	default:
	  abort();
	  break;
	}
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
	  return;
	}

      if (rename (tmp_name, name) != 0)
	{
	  char *buf = (char *) malloc(1024 + strlen(tmp_name) + strlen(name));
	  sprintf(buf, "%s: error renaming \"%s\" to \"%s\"",
		  blurb(), tmp_name, name);
	  perror(buf);
	  unlink (tmp_name);
	  free(buf);
	  return;
	}
      else
	{
	  p->init_file_date = write_date;

	  /* Since the .xscreensaver file is used for IPC, let's try and make
	     sure that the bits actually land on the disk right away. */
	  sync ();
	}
    }
  else
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: error closing \"%s\"", blurb(), name);
      perror(buf);
      free(buf);
      unlink (tmp_name);
      return;
    }
}


/* Parsing the resource database
 */


/* Populate `saver_preferences' with the contents of the resource database.
   Note that this may be called multiple times -- it is re-run each time
   the ~/.xscreensaver file is reloaded.

   This function can be very noisy, since it issues resource syntax errors
   and so on.
 */
void
load_init_file (saver_preferences *p)
{
  static Bool first_time = True;
  
  if (parse_init_file (p) != 0)		/* file might have gone away */
    if (!first_time) return;

  first_time = False;

  p->xsync_p	    = get_boolean_resource ("synchronous", "Synchronous");
  p->verbose_p	    = get_boolean_resource ("verbose", "Boolean");
  p->timestamp_p    = get_boolean_resource ("timestamp", "Boolean");
  p->lock_p	    = get_boolean_resource ("lock", "Boolean");
  p->lock_vt_p	    = get_boolean_resource ("lockVTs", "Boolean");
  p->fade_p	    = get_boolean_resource ("fade", "Boolean");
  p->unfade_p	    = get_boolean_resource ("unfade", "Boolean");
  p->fade_seconds   = 1000 * get_seconds_resource ("fadeSeconds", "Time");
  p->fade_ticks	    = get_integer_resource ("fadeTicks", "Integer");
  p->install_cmap_p = get_boolean_resource ("installColormap", "Boolean");
  p->nice_inferior  = get_integer_resource ("nice", "Nice");

  p->initial_delay   = 1000 * get_seconds_resource ("initialDelay", "Time");
  p->splash_duration = 1000 * get_seconds_resource ("splashDuration", "Time");
  p->timeout         = 1000 * get_minutes_resource ("timeout", "Time");
  p->lock_timeout    = 1000 * get_minutes_resource ("lockTimeout", "Time");
  p->cycle           = 1000 * get_minutes_resource ("cycle", "Time");
  p->passwd_timeout  = 1000 * get_seconds_resource ("passwdTimeout", "Time");
  p->pointer_timeout = 1000 * get_seconds_resource ("pointerPollTime", "Time");
  p->notice_events_timeout = 1000*get_seconds_resource("windowCreationTimeout",
						       "Time");
  p->shell = get_string_resource ("bourneShell", "BourneShell");

  p->demo_command = get_string_resource("demoCommand", "URL");
  p->prefs_command = get_string_resource("prefsCommand", "URL");
  p->help_url = get_string_resource("helpURL", "URL");
  p->load_url_command = get_string_resource("loadURL", "LoadURL");

  {
    char *s;
    if ((s = get_string_resource ("splash", "Boolean")))
      if (!get_boolean_resource("splash", "Boolean"))
	p->splash_duration = 0;
    if (s) free (s);
  }

  p->use_xidle_extension = get_boolean_resource ("xidleExtension","Boolean");
  p->use_mit_saver_extension = get_boolean_resource ("mitSaverExtension",
						     "Boolean");
  p->use_sgi_saver_extension = get_boolean_resource ("sgiSaverExtension",
						     "Boolean");
  p->use_proc_interrupts = get_boolean_resource ("procInterrupts", "Boolean");

  /* Throttle the various timeouts to reasonable values.
   */
  if (p->passwd_timeout <= 0) p->passwd_timeout = 30000;	 /* 30 secs */
  if (p->timeout < 10000) p->timeout = 10000;			 /* 10 secs */
  if (p->cycle < 0) p->cycle = 0;
  if (p->cycle != 0 && p->cycle < 2000) p->cycle = 2000;	 /*  2 secs */
  if (p->pointer_timeout <= 0) p->pointer_timeout = 5000;	 /*  5 secs */
  if (p->notice_events_timeout <= 0)
    p->notice_events_timeout = 10000;				 /* 10 secs */
  if (p->fade_seconds <= 0 || p->fade_ticks <= 0)
    p->fade_p = False;
  if (! p->fade_p) p->unfade_p = False;

  if (p->verbose_p && !p->fading_possible_p && (p->fade_p || p->unfade_p))
    {
      fprintf (stderr, "%s: there are no PseudoColor or GrayScale visuals.\n",
	       blurb());
      fprintf (stderr, "%s: ignoring the request for fading/unfading.\n",
	       blurb());
    }

  p->watchdog_timeout = p->cycle;
  if (p->watchdog_timeout < 30000) p->watchdog_timeout = 30000;	  /* 30 secs */
  if (p->watchdog_timeout > 3600000) p->watchdog_timeout = 3600000; /*  1 hr */

  get_screenhacks (p);

  if (p->debug_p)
    {
      p->xsync_p = True;
      p->verbose_p = True;
      p->timestamp_p = True;
      p->initial_delay = 0;
    }
}


/* Parsing the programs resource.
 */

static char *
reformat_hack (const char *hack)
{
  int i;
  const char *in = hack;
  int indent = 15;
  char *h2 = (char *) malloc(strlen(in) + indent + 2);
  char *out = h2;
  Bool disabled_p = False;

  while (isspace(*in)) in++;		/* skip whitespace */

  if (*in == '-')			/* Handle a leading "-". */
    {
      in++;
      hack = in;
      *out++ = '-';
      *out++ = ' ';
      disabled_p = True;
      while (isspace(*in)) in++;
    }
  else
    {
      *out++ = ' ';
      *out++ = ' ';
    }

  while (*in && !isspace(*in) && *in != ':')
    *out++ = *in++;			/* snarf first token */
  while (isspace(*in)) in++;		/* skip whitespace */

  if (*in == ':')
    *out++ = *in++;			/* copy colon */
  else
    {
      in = hack;
      out = h2 + 2;			/* reset to beginning */
    }

  *out = 0;

  while (isspace(*in)) in++;		/* skip whitespace */
  for (i = strlen(h2); i < indent; i++)	/* indent */
    *out++ = ' ';

  /* copy the rest of the line. */
  while (*in)
    {
      /* shrink all whitespace to one space, for the benefit of the "demo"
	 mode display.  We only do this when we can easily tell that the
	 whitespace is not significant (no shell metachars).
       */
      switch (*in)
	{
	case '\'': case '"': case '`': case '\\':
	  {
	    /* Metachars are scary.  Copy the rest of the line unchanged. */
	    while (*in)
	      *out++ = *in++;
	  }
	  break;
	case ' ': case '\t':
	  {
	    while (*in == ' ' || *in == '\t')
	      in++;
	    *out++ = ' ';
	  }
	  break;
	default:
	  *out++ = *in++;
	  break;
	}
    }
  *out = 0;

  /* strip trailing whitespace. */
  out = out-1;
  while (out > h2 && (*out == ' ' || *out == '\t' || *out == '\n'))
    *out-- = 0;

  return h2;
}


static void
get_screenhacks (saver_preferences *p)
{
  int i = 0;
  int start = 0;
  int end = 0;
  int size;
  char *d;

  d = get_string_resource ("monoPrograms", "MonoPrograms");
  if (d && !*d) { free(d); d = 0; }
  if (!d)
    d = get_string_resource ("colorPrograms", "ColorPrograms");
  if (d && !*d) { free(d); d = 0; }

  if (d)
    {
      fprintf (stderr,
       "%s: the `monoPrograms' and `colorPrograms' resources are obsolete;\n\
	see the manual for details.\n", blurb());
      free(d);
    }

  d = get_string_resource ("programs", "Programs");

  if (p->screenhacks)
    {
      for (i = 0; i < p->screenhacks_count; i++)
	if (p->screenhacks[i])
	  free (p->screenhacks[i]);
      free(p->screenhacks);
      p->screenhacks = 0;
    }

  if (!d || !*d)
    {
      p->screenhacks_count = 0;
      p->screenhacks = 0;
      return;
    }

  size = strlen (d);


  /* Count up the number of newlines (which will be equal to or larger than
     the number of hacks.)
   */
  i = 0;
  for (i = 0; d[i]; i++)
    if (d[i] == '\n')
      i++;
  i++;

  p->screenhacks = (char **) calloc (sizeof (char *), i+1);

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

      p->screenhacks[p->screenhacks_count++] = reformat_hack (d + start);
      if (p->screenhacks_count >= i)
	abort();

      start = end+1;
    }

  if (p->screenhacks_count == 0)
    {
      free (p->screenhacks);
      p->screenhacks = 0;
    }
}
