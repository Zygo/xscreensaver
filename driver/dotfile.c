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

#include <sys/types.h>
#include <sys/stat.h>

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

#include "xscreensaver.h"
#include "resources.h"

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


static const char *
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
	  fprintf (stderr, "%s: couldn't get home directory of %s\n",
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
  "overlayStderr",
  "overlayTextBackground",	/* not saved -- X resources only */
  "overlayTextForeground",	/* not saved -- X resources only */
  "bourneShell",		/* not saved -- X resources only */
  0
};

static char *strip(char *s)
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
handle_entry (saver_info *si, const char *key, const char *value,
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

	XrmPutStringResource (&si->db, spec, val);

	free(spec);
	free(val);
	return 0;
      }

  fprintf(stderr, "%s: %s:%d: unknown option \"%s\"\n",
	  blurb(), filename, line, key);
  return 1;
}

int
read_init_file (saver_info *si)
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
      si->init_file_date = 0;
      return 0;
    }

  in = fopen(name, "r");
  if (!in)
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: error reading %s", blurb(), name);
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
      sprintf(buf, "%s: couldn't re-stat %s", blurb(), name);
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

      handle_entry (si, key, value, name, line);
    }
  free(buf);

  si->init_file_date = write_date;
  return 0;
}


int
maybe_reload_init_file (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  const char *name = init_file_name();
  struct stat st;
  int status = 0;

  if (!name) return 0;

  if (stat(name, &st) != 0)
    return 0;

  if (si->init_file_date == st.st_mtime)
    return 0;

  if (p->verbose_p)
    fprintf (stderr, "%s: file %s has changed, reloading.\n", blurb(), name);

  status = read_init_file (si);
  if (status == 0)
    get_resources (si);
  return status;
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
      if (first)
	first = False;
      else
	{
	  col = tab_to(out, col, 75);
	  fprintf(out, " \\n\\\n");
	  col = 0;
	}

      v2 = strip(v2);
      nl = strchr(v2, '\n');
      if (nl)
	*nl = 0;

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

int
write_init_file (saver_info *si)
{
  const char *name = init_file_name();
  const char *tmp_name = init_file_tmp_name();
  struct stat st;
  saver_preferences *p = &si->prefs;
  int i, j;

  /* Kludge, since these aren't in the saver_preferences struct as strings...
   */
  char *visual_name;
  char *programs;
  Bool capture_stderr_p;
  Bool overlay_stderr_p;
  char *stderr_font;
  FILE *out;

  if (!name) return 0;

  if (si->dangerous_uid_p)
    {
      if (p->verbose_p)
	{
	  fprintf (stderr, "%s: not writing \"%s\":\n", blurb(), name);
	  describe_uids (si, stderr);
	}
      return 0;
    }

  if (p->verbose_p)
    fprintf (stderr, "%s: writing \"%s\".\n", blurb(), name);

  unlink (tmp_name);
  out = fopen(tmp_name, "w");
  if (!out)
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: error writing %s", blurb(), name);
      perror(buf);
      free(buf);
      return -1;
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
	  sprintf (buf, "%s: error fchmodding %s to 0%o", blurb(),
		   tmp_name, (unsigned int) mode);
	  perror(buf);
	  free(buf);
	  return -1;
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
    struct passwd *p = getpwuid (getuid ());
    char *whoami = (p && p->pw_name && *p->pw_name
		    ? p->pw_name : "<unknown>");
    fprintf (out, 
	     "# %s Preferences File\n"
	     "# Written by %s %s for %s on %s.\n"
	     "# http://www.jwz.org/xscreensaver/\n"
	     "\n",
	     progclass, progname, si->version,
	     (whoami ? whoami : "<unknown>"),
	     timestring());
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
	  sprintf(buf, "%s: couldn't stat %s", blurb(), tmp_name);
	  perror(buf);
	  unlink (tmp_name);
	  free(buf);
	  return -1;
	}

      if (rename (tmp_name, name) != 0)
	{
	  char *buf = (char *) malloc(1024 + strlen(tmp_name) + strlen(name));
	  sprintf(buf, "%s: error renaming %s to %s", blurb(), tmp_name, name);
	  perror(buf);
	  unlink (tmp_name);
	  free(buf);
	  return -1;
	}
      else
	{
	  si->init_file_date = write_date;
	}
    }
  else
    {
      char *buf = (char *) malloc(1024 + strlen(name));
      sprintf(buf, "%s: error closing %s", blurb(), name);
      perror(buf);
      free(buf);
      unlink (tmp_name);
      return -1;
    }

  return 0;
}
