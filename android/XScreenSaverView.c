#include <stdio.h>
#include <string.h>
#include <zlib.h>
//#include <android/log.h>
#include "screenhackI.h"
#include "xlockmoreI.h"

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif

extern struct xscreensaver_function_table *xscreensaver_function_table;

extern const char *progname;
extern const char *progclass;
int mono_p = 0;

static char *hilbert_mode;
static char *sproingies_count;
static char *sproingies_wireframe;
static char *bouncingcow_count;
static char *bouncingcow_speed;
static char *superquadrics_spinspeed;
static char *stonerview_delay;
static char *stonerview_transparent;
static char *unknownpleasures_wireframe;
static char *unknownpleasures_speed;
static char *hypertorus_displayMode;
static char *hypertorus_appearance;
static char *hypertorus_colors;
static char *hypertorus_projection3d;
static char *hypertorus_projection4d;
static char *hypertorus_speedwx;
static char *hypertorus_speedwy;
static char *hypertorus_speedwz;
static char *hypertorus_speedxy;
static char *hypertorus_speedxz;
static char *hypertorus_speedyz;
static char *glhanoi_light;
static char *glhanoi_fog;
static char *glhanoi_trails;
static char *glhanoi_poles;
static char *glhanoi_speed;


Bool 
get_boolean_resource (Display *dpy, char *res_name, char *res_class)
{
  char *tmp, buf [100];
  char *s = get_string_resource (dpy, res_name, res_class);
  char *os = s;
  if (! s) return 0;
  for (tmp = buf; *s; s++)
    *tmp++ = isupper (*s) ? _tolower (*s) : *s;
  *tmp = 0;
  //free (os);

  while (*buf &&
         (buf[strlen(buf)-1] == ' ' ||
          buf[strlen(buf)-1] == '\t'))
    buf[strlen(buf)-1] = 0;

  if (!strcmp (buf, "on") || !strcmp (buf, "true") || !strcmp (buf, "yes"))
    return 1;
  if (!strcmp (buf,"off") || !strcmp (buf, "false") || !strcmp (buf,"no"))
    return 0;
  fprintf (stderr, "%s: %s must be boolean, not %s.\n",
           progname, res_name, buf);
  return 0;
}

int 
get_integer_resource (Display *dpy, char *res_name, char *res_class)
{
  int val;
  char c, *s = get_string_resource (dpy, res_name, res_class);
  char *ss = s;
  if (!s) return 0;

  while (*ss && *ss <= ' ') ss++;                       /* skip whitespace */

  if (ss[0] == '0' && (ss[1] == 'x' || ss[1] == 'X'))   /* 0x: parse as hex */
    {
      if (1 == sscanf (ss+2, "%x %c", (unsigned int *) &val, &c))
        {
          //free (s);
          return val;
        }
    }
  else                                                  /* else parse as dec */
    {
      if (1 == sscanf (ss, "%d %c", &val, &c))
        {
          //free (s);
          return val;
        }
    }

  fprintf (stderr, "%s: %s must be an integer, not %s.\n",
           progname, res_name, s);
  //free (s);
  return 0;
}

double
get_float_resource (Display *dpy, char *res_name, char *res_class)
{
  double val;
  char c, *s = get_string_resource (dpy, res_name, res_class);
  if (! s) return 0.0;
  if (1 == sscanf (s, " %lf %c", &val, &c))
    {
      //free (s);
      return val;
    }
  fprintf (stderr, "%s: %s must be a float, not %s.\n",
           progname, res_name, s);
  //free (s);
  return 0.0;
}


// TODO: fill in what is not here
char *get_string_resource(Display * dpy, char *name, char *class)
{
    char *implement;
    //__android_log_print (ANDROID_LOG_INFO, "xscreensaver", "s %s %s %s", progname, name, class); 

    if (strcmp(progname, "hilbert") == 0) {
	if (strcmp(name, "mode") == 0 && strcmp(class, "Mode") == 0) {
	    implement = malloc(8 * sizeof(char));
	    strcpy(implement, hilbert_mode);
	} else if (strcmp(name, "ends") == 0 && strcmp(class, "Ends") == 0) {
	    implement = malloc(5 * sizeof(char));
	    strcpy(implement, "open");
	} else if (strcmp(name, "speed") == 0 && strcmp(class, "Speed") == 0) {
	    return "1.0";
	} else if (strcmp(name, "thickness") == 0
		   && strcmp(class, "Thickness") == 0) {
	    return "0.25";
	} else if (strcmp(name, "delay") == 0 && strcmp(class, "Usecs") == 0) {
	    return "30000";
	} else if (strcmp(name, "maxDepth") == 0
	    && strcmp(class, "MaxDepth") == 0) {
	    // too deep is too much for less powerful Android phones
	    return "3";
	    //return 5;
	} else if (strcmp(name, "spin") == 0 && strcmp(class, "Spin") == 0) {
            return "True";
	} else if (strcmp(name, "wireframe") == 0
		   && strcmp(class, "Boolean") == 0) {
            return "False";
	} else if (strcmp(name, "wander") == 0
		   && strcmp(class, "Wander") == 0) {
            return "False";
	} else {
            return 0;
        }
    }
    else if (strcmp(progname, "sproingies") == 0) {
	if (strcmp(name, "count") == 0 && strcmp(class, "Int") == 0) {
	    return sproingies_count;
	} else if (strcmp(name, "wireframe") == 0
	    && strcmp(class, "Boolean") == 0) {
	    return sproingies_wireframe;
	} else {
            return 0;
        }
    }
    else if (strcmp(progname, "superquadrics") == 0) {
	if (strcmp(name, "spinspeed") == 0
	    && strcmp(class, "Spinspeed") == 0) {
	    return superquadrics_spinspeed;
	} else if (strcmp(name, "count") == 0 && strcmp(class, "Int") == 0) {
	    return "25";
	} else if (strcmp(name, "cycles") == 0
		   && strcmp(class, "Int") == 0) {
	    return "40";
	} else if (strcmp(name, "delay") == 0
		   && strcmp(class, "Usecs") == 0) {
	    return "40000";
	} else if (strcmp(name, "wireframe") == 0
	    && strcmp(class, "Boolean") == 0) {
            return "False";
	} else {
            return 0;
        }
    }
    else if (strcmp(progname, "stonerview") == 0) {
	if (strcmp(name, "use3d") == 0 && strcmp(class, "Boolean") == 0) {
	    return "True";
	} else if (strcmp(name, "transparent") == 0
		   && strcmp(class, "Transparent") == 0) {
	    return stonerview_transparent;
	} else if (strcmp(name, "wireframe") == 0
		   && strcmp(class, "Boolean") == 0) {
	    return "False";
	} else if (strcmp(name, "doFPS") == 0
		   && strcmp(class, "DoFPS") == 0) {
	    return "False";
        } else {
            return 0;
        }
    }
    else if (strcmp(progname, "bouncingcow") == 0) {
	if (strcmp(name, "count") == 0
	    && strcmp(class, "Int") == 0) {
	    return bouncingcow_count;
        } else if (strcmp(name, "speed") == 0
	    && strcmp(class, "Speed") == 0) {
	    return bouncingcow_speed;
        } else {
            return 0;
        }
    }
    else if (strcmp(progname, "unknownpleasures") == 0) {

        if (strcmp(name, "wireframe") == 0) {
          return unknownpleasures_wireframe;
        } else if (strcmp(name, "speed") == 0) {
          return unknownpleasures_speed;
        } else if (strcmp(name, "count") == 0) {
          return "80";
        } else if (strcmp(name, "resolution") == 0) {
          return "100";
          //return "200";
        } else if (strcmp(name, "ortho") == 0) {
          return "True";
          //return "False";
        } else {
          return 0;
        }

    }
    else if (strcmp(progname, "hypertorus") == 0) {
	if (strcmp(name, "displayMode") == 0) {
	    return hypertorus_displayMode;
	} else if (strcmp(name, "appearance") == 0) {
	    return hypertorus_appearance;
	} else if (strcmp(name, "colors") == 0) {
	    return hypertorus_colors;
	} else if (strcmp(name, "projection3d") == 0) {
	    return hypertorus_projection3d;
	} else if (strcmp(name, "projection4d") == 0) {
	    return hypertorus_projection4d;
	} else if (strcmp(name, "speedwx") == 0) {
	    return hypertorus_speedwz;
	} else if (strcmp(name, "speedwy") == 0) {
	    return hypertorus_speedwy;
	} else if (strcmp(name, "speedwz") == 0) {
	    return hypertorus_speedwz;
	} else if (strcmp(name, "speedxy") == 0) {
	    return hypertorus_speedxy;
	} else if (strcmp(name, "speedxz") == 0) {
	    return hypertorus_speedxz;
	} else if (strcmp(name, "speedyz") == 0) {
	    return hypertorus_speedyz;
	} else {
            return 0;
        }
    }
    else if (strcmp(progname, "glhanoi") == 0) {
        if (strcmp(name, "light") == 0) {
            return glhanoi_light;
        } else if (strcmp(name, "fog") == 0) {
            return glhanoi_fog;
        } else if (strcmp(name, "trails") == 0) {
            return glhanoi_trails;
        } else if (strcmp(name, "poles") == 0) {
            return glhanoi_poles;
        } else if (strcmp(name, "speed") == 0) {
            return glhanoi_speed;
        } else {
            return 0;
        }
    }
    else {
	implement = 0;
    }

    return implement;
}


void setSuperquadricsSettings(char *hck, char *khck)
{
    if (strcmp(khck, "superquadrics_spinspeed") == 0) {
        superquadrics_spinspeed = malloc(4 * sizeof(char));
        strcpy(superquadrics_spinspeed, hck);
    }
}

void setHilbertSettings(char *hck, char *khck)
{
    if (strcmp(khck, "hilbert_mode") == 0) {
        if (!hilbert_mode) {
            hilbert_mode = malloc(8 * sizeof(char));
        }
        if (strcmp(hck, "3D") == 0) {
            strcpy(hilbert_mode, "3D");
        }
        else if (strcmp(hck, "2D") == 0) {
            strcpy(hilbert_mode, "2D");
        }
    }
}

void setSproingiesSettings(char *hck, char *khck)
{
    if (strcmp(khck, "sproingies_count") == 0) {
        sproingies_count = malloc(3 * sizeof(char));
        strcpy(sproingies_count, hck);
    }
    else if (strcmp(khck, "sproingies_wireframe") == 0) {
        sproingies_wireframe = malloc(6 * sizeof(char));
        strcpy(sproingies_wireframe, hck);
    }
}

void setStonerviewSettings(char *hck, char *khck)
{
    if (strcmp(khck, "stonerview_transparent") == 0) {
        stonerview_transparent = malloc(6 * sizeof(char));
        strcpy(stonerview_transparent, hck);
    }
}

void setBouncingcowSettings(char *hck, char *khck)
{
    if (strcmp(khck, "bouncingcow_count") == 0) {
        bouncingcow_count = malloc(3 * sizeof(char));
        strcpy(bouncingcow_count, hck);
    }
    else if (strcmp(khck, "bouncingcow_speed") == 0) {
        bouncingcow_speed = malloc(4 * sizeof(char));
        strcpy(bouncingcow_speed, hck);
    }
}

void setUnknownpleasuresSettings(char *hck, char *khck)
{
    if (strcmp(khck, "unknownpleasures_speed") == 0) {
        unknownpleasures_speed = malloc(3 * sizeof(char));
        strcpy(unknownpleasures_speed, hck);
    }
    else if (strcmp(khck, "unknownpleasures_wireframe") == 0) {
        unknownpleasures_wireframe = malloc(6 * sizeof(char));
        strcpy(unknownpleasures_wireframe, hck);
    }
}

void setHypertorusSettings(char *hck, char *khck)
{
    if (strcmp(khck, "hypertorus_displayMode") == 0) {
        hypertorus_displayMode = malloc(13 * sizeof(char));
        strcpy(hypertorus_displayMode, hck);
    }
    else if (strcmp(khck, "hypertorus_appearance") == 0) {
        hypertorus_appearance = malloc(12 * sizeof(char));
        strcpy(hypertorus_appearance, hck);
    }
    else if (strcmp(khck, "hypertorus_colors") == 0) {
        hypertorus_colors = malloc(5 * sizeof(char));
        strcpy(hypertorus_colors, hck);
    }
    else if (strcmp(khck, "hypertorus_projection3d") == 0) {
        hypertorus_projection3d = malloc(17 * sizeof(char));
        strcpy(hypertorus_projection3d, hck);
    }
    else if (strcmp(khck, "hypertorus_projection4d") == 0) {
        hypertorus_projection4d = malloc(17 * sizeof(char));
        strcpy(hypertorus_projection4d, hck);
    }
    else if (strcmp(khck, "hypertorus_speedwx") == 0) {
        hypertorus_speedwx = malloc(5 * sizeof(char));
        strcpy(hypertorus_speedwx, hck);
    }
    else if (strcmp(khck, "hypertorus_speedwy") == 0) {
        hypertorus_speedwy = malloc(5 * sizeof(char));
        strcpy(hypertorus_speedwy, hck);
    }
    else if (strcmp(khck, "hypertorus_speedwz") == 0) {
        hypertorus_speedwz = malloc(5 * sizeof(char));
        strcpy(hypertorus_speedwz, hck);
    }
    else if (strcmp(khck, "hypertorus_speedxy") == 0) {
        hypertorus_speedxy = malloc(5 * sizeof(char));
        strcpy(hypertorus_speedxy, hck);
    }
    else if (strcmp(khck, "hypertorus_speedxz") == 0) {
        hypertorus_speedxz = malloc(5 * sizeof(char));
        strcpy(hypertorus_speedxz, hck);
    }
    else if (strcmp(khck, "hypertorus_speedyz") == 0) {
        hypertorus_speedyz = malloc(5 * sizeof(char));
        strcpy(hypertorus_speedyz, hck);
    }
}

void setGlhanoiSettings(char *hck, char *khck) {

    if (strcmp(khck, "glhanoi_light") == 0) {
        glhanoi_light = malloc(6 * sizeof(char));
        strcpy(glhanoi_light , hck);
    }
    else if (strcmp(khck, "glhanoi_fog") == 0) {
        glhanoi_fog = malloc(6 * sizeof(char));
        strcpy(glhanoi_fog , hck);
    }
    else if (strcmp(khck, "glhanoi_trails") == 0) {
        glhanoi_trails = malloc(3 * sizeof(char));
        strcpy(glhanoi_trails , hck);
    }
    else if (strcmp(khck, "glhanoi_poles") == 0) {
        glhanoi_poles = malloc(3 * sizeof(char));
        strcpy(glhanoi_poles , hck);
    }
    else if (strcmp(khck, "glhanoi_speed") == 0) {
        glhanoi_speed = malloc(3 * sizeof(char));
        strcpy(glhanoi_speed , hck);
    }
}
