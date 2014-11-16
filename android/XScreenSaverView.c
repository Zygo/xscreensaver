#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include "screenhackI.h"
#include "xlockmoreI.h"

extern struct xscreensaver_function_table *xscreensaver_function_table;

extern const char *progname;
extern const char *progclass;
int mono_p = 0;

static int sproingies_count = 8;
static double superquadrics_speed = 5.0;
static int stonerview_delay = 20000;
static char *hilbert_mode;
static Bool stonerview_transparent = False;

// TODO: fill in what is not here
Bool get_boolean_resource(Display * dpy, char *name, char *class)
{
    Bool b;

    if (strcmp(progname, "hilbert") == 0) {

	if (strcmp(name, "spin") == 0 && strcmp(class, "Spin") == 0) {
	    b = True;
	} else if (strcmp(name, "wireframe") == 0
		   && strcmp(class, "Boolean") == 0) {
	    b = False;
	} else if (strcmp(name, "wander") == 0
		   && strcmp(class, "Wander") == 0) {
	    b = False;
	}
    } else if (strcmp(progname, "stonerview") == 0) {
	if (strcmp(name, "use3d") == 0 && strcmp(class, "Boolean") == 0) {
	    b = True;
	} else if (strcmp(name, "transparent") == 0
		   && strcmp(class, "Transparent") == 0) {
	    b = stonerview_transparent;
	} else if (strcmp(name, "wireframe") == 0
		   && strcmp(class, "Boolean") == 0) {
	    b = False;
	} else if (strcmp(name, "doFPS") == 0
		   && strcmp(class, "DoFPS") == 0) {
	    b = False;
	}
    } else if (strcmp(progname, "superquadrics") == 0) {
	if (strcmp(name, "wireframe") == 0
	    && strcmp(class, "Boolean") == 0) {
	    b = False;
	}
    } else if (strcmp(progname, "sproingies") == 0) {
	if (strcmp(name, "wireframe") == 0
	    && strcmp(class, "Boolean") == 0) {
	    b = False;
	}
    }

    return b;
}

// TODO: fill in what is not here
char *get_string_resource(Display * dpy, char *name, char *class)
{
    char *implement;

    if (strcmp(progname, "hilbert") == 0) {
	if (strcmp(name, "mode") == 0 && strcmp(class, "Mode") == 0) {
	    implement = malloc(8 * sizeof(char));
	    strcpy(implement, hilbert_mode);
	} else if (strcmp(name, "ends") == 0 && strcmp(class, "Ends") == 0) {
	    implement = malloc(5 * sizeof(char));
	    strcpy(implement, "open");
	} else {
	    implement = 0;
	}

    } else {
	implement = 0;
    }

    return implement;
}


// TODO: fill in what is not here
int get_integer_resource(Display * dpy, char *name, char *class)
{

    if (strcmp(progname, "sproingies") == 0) {
	if (strcmp(name, "count") == 0 && strcmp(class, "Int") == 0) {
	    return sproingies_count;
	}
    } else if (strcmp(progname, "superquadrics") == 0) {

	if (strcmp(name, "count") == 0 && strcmp(class, "Int") == 0) {
	    return 25;
	} else if (strcmp(name, "cycles") == 0
		   && strcmp(class, "Int") == 0) {
	    return 40;
	} else if (strcmp(name, "delay") == 0
		   && strcmp(class, "Usecs") == 0) {
	    return 40000;
	}

    } else if (strcmp(progname, "hilbert") == 0) {
	if (strcmp(name, "delay") == 0 && strcmp(class, "Usecs") == 0) {
	    return 30000;
	}
	if (strcmp(name, "maxDepth") == 0
	    && strcmp(class, "MaxDepth") == 0) {
	    // too deep is too much for less powerful Android phones
	    return 3;
	    //return 5;
	}
    }

}


// TODO: fill in what is not here
double get_float_resource(Display * dpy, char *name, char *class)
{

    if (strcmp(progname, "superquadrics") == 0) {
	if (strcmp(name, "spinspeed") == 0
	    && strcmp(class, "Spinspeed") == 0) {
	    return superquadrics_speed;
	}
    } else if (strcmp(progname, "hilbert") == 0) {
	if (strcmp(name, "speed") == 0 && strcmp(class, "Speed") == 0) {
	    return 1.0;
	} else if (strcmp(name, "thickness") == 0
		   && strcmp(class, "Thickness") == 0) {
	    return 0.25;
	}
    }
}


void setSproingiesCount(int c)
{
    sproingies_count = c;
}

void setStonerviewTransparency(int c)
{
    if (c == 1) {
	stonerview_transparent = True;
    } else {
	stonerview_transparent = False;
    }
}

void setHilbertMode(char *mode)
{
    if (!hilbert_mode) {
	hilbert_mode = malloc(8 * sizeof(char));
    }
    if (strcmp(mode, "3d") == 0) {
	strcpy(hilbert_mode, "3d");
    }
    if (strcmp(mode, "2d") == 0) {
	strcpy(hilbert_mode, "2d");
    } else {
	strcpy(hilbert_mode, "random");
    }
}


void setSuperquadricsSpeed(double sq_speed)
{
    superquadrics_speed = sq_speed;
}
