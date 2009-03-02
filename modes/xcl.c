/*
 *  xcl.c
 *  Control-Line in a box
 *  Copyright (c) 2000 by Martin Berentsen <berentsen@sent5.uni-duisburg.de>
 *
 * change-log
 * 0.4pl2 -> 0.5pl0 25/07/2000
 * -singl command line option becomes to -count num
 * 0.5pl0 -> 0.5pl1 02.02.2001
 * bugs in -oldcolor and -randomstart option removed
 *
 * TODO as next:
 * - a user defined speed for every used plane
 *
 */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xcl.c    5.00 2000/11/01 xlockmore";
#endif

#define RAND(x)    (((my_random() % x )- x/2))  /* random number around 0 */
#define ROTATEDELAY 20000      /* delay for view-model rotating */
#define STARTUPDELAY 5000      /* delay for the first calibration loop */
#define REGULATE 25                    /* regulate delay every xx frames */
#define FRAMETIME 45000                /* time for one frame */
#define MINPLANES 1           /* define the min number of planes */
#define MAXCOUNT 30           /* define the max number of planes */

#ifdef STANDALONE
#define MODE_xcl
#define PROGCLASS "Xcl"
#define HACK_INIT init_xcl
#define HACK_DRAW draw_xcl
#define xcl_opts xlockmore_opts
#define DEFAULTS "*delay: 20000 \n" \
 "*count: 2 \n"
#define UNIFORM_COLORS
#include "xlockmore.h"        /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"        /* in xlockmore distribution */
#endif

#ifdef WIN32
#include <sys/time.h>
#endif

#include "xcl.h"             /* model line data file */

#ifdef MODE_xcl

/* all parameters are global */
static float speed[MAXCOUNT];     /* Speed in km/h */
static float speed_in;       /* speed set by user */
static int frametime;       /* time for one frame in usecs */
static int line_length;     /* lines in mm*/
static float spectator;     /* spectator distance from zero*/
static Bool viewmodel;      /* shows one rotating model*/
static Bool oldcolor;       /* use the old yellow/red color combination */
static Bool debug;          /* debug modus */
static Bool automatic;      /* automatic scale for fit into window */
static Bool randomstart;    /* don't use the same start position */
static int random_pid;


#define DEF_SPEED_IN  "105.0"
#define DEF_FRAMETIME  "45000"
#define DEF_LINE_LENGTH  "15910"
#define DEF_SPECTATOR  "22000"
#define DEF_VIEWMODEL  "False"
#define DEF_OLDCOLOR  "False"
#define DEF_XCLDEBUG  "False"
#define DEF_AUTOMATIC  "True"
#define DEF_RANDOMSTART  "False"


static XrmOptionDescRec opts[] =
{
  {(char *) "-speed", (char *) ".xcl.speed", XrmoptionSepArg, (caddr_t) NULL},
  {(char *) "-frametime", (char *) ".xcl.frametime", XrmoptionSepArg, (caddr_t) NULL},
  {(char *) "-line_length", (char *) ".xcl.line_length", XrmoptionSepArg, (caddr_t) NULL},
  {(char *) "-spectator", (char *) ".xcl.spectator", XrmoptionSepArg, (caddr_t) NULL},
  {(char *) "-viewmodel", (char *) ".xcl.viewmodel", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+viewmodel", (char *) ".xcl.viewmodel", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-oldcolor", (char *) ".xcl.oldcolor", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+oldcolor", (char *) ".xcl.oldcolor", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-xcldebug", (char *) ".xcl.xcldebug", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+xcldebug", (char *) ".xcl.xcldebug", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-automatic", (char *) ".xcl.automatic", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+automatic", (char *) ".xcl.automatic", XrmoptionNoArg, (caddr_t) "off"},
  {(char *) "-randomstart", (char *) ".xcl.randomstart", XrmoptionNoArg, (caddr_t) "on"},
  {(char *) "+randomstart", (char *) ".xcl.randomstart", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
  {(void *) & speed_in, (char *) "speed", (char *) "Speed", (char *) DEF_SPEED_IN, t_Float},
  {(void *) & frametime, (char *) "frametime", (char *) "frametime", (char *) DEF_FRAMETIME, t_Int},
  {(void *) & line_length, (char *) "line_length", (char *) "Line_length", (char *) DEF_LINE_LENGTH, t_Int},
  {(void *) & spectator, (char *) "spectator", (char *) "Spectator", (char *) DEF_SPECTATOR, t_Float},
  {(void *) & viewmodel, (char *) "viewmodel", (char *) "Viewmodel", (char *) DEF_VIEWMODEL, t_Bool},
  {(void *) & oldcolor, (char *) "oldcolor", (char *) "Oldcolor", (char *) DEF_OLDCOLOR, t_Bool},
  {(void *) & debug, (char *) "xcldebug", (char *) "Xcldebug", (char *) DEF_XCLDEBUG, t_Bool},
  {(void *) & automatic, (char *) "automatic", (char *) "Automatic", (char *) DEF_AUTOMATIC, t_Bool},
  {(void *) & randomstart, (char *) "randomstart", (char *) "Randomstart", (char *) DEF_RANDOMSTART, t_Bool}
};
static OptionStruct desc[] =
{
  {(char *) "-speed num", (char *) "speed for the planes in km/h "},
  {(char *) "-frametime num", (char *) "time for one frame on the screen in usecs "},
  {(char *) "-line_length num", (char *) "distance between the pilot and the plane in mm "},
  {(char *) "-spectator num", (char *) "distance  between  spectator  and  pilot in mm"},
  {(char *) "-/+viewmodel", (char *) "turn on/off an anim view of one model"},
  {(char *) "-/+oldcolor", (char *) "turn on/off the old yellow/red combination"},
  {(char *) "-/+xcldebug", (char *) "turn on/off some timing information"},
  {(char *) "-/+automatic", (char *) "turn on/off auto scale for fit into the window"},
  {(char *) "-/+randomstart", (char *) "turn on/off a random start point for models at startup"}
};

ModeSpecOpt xcl_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES    /* for xlockmore */
ModStruct   xcl_description =
{"xcl", "init_xcl", "draw_xcl", "release_xcl",
 "draw_xcl", "init_xcl", (char *) NULL, &xcl_opts ,
 20000, -3, 1, 1, 64, 1.0, "",
 "Shows a control line combat model race", 0, NULL};
#endif

typedef struct
{
  int planes;    /* number of planes viewed*/
  int lines;     /* number of lines counted in model */
  double az[MAXCOUNT];  /* azimuth of plane */
  double el[MAXCOUNT];  /* elevation of plane */
  int width;
  int height;
  int mid_x;
  int mid_y;
  float Alpha;   /* rotate.1 */
  float Beta;    /* rotate.2 */
  float Gamma;   /* rotate.3 */
  float Vx;      /* width from zero in X */
  float Vy;      /* width from zero in Y */
  float Vz;      /* width from zero in Z */
  float G;       /* ZOOM  */
  float Ca,Cb,Cc,Sa,Sb,Sc; /* only for faster calculation */
  float Bx,By ;
  struct timeval tv1;
  double time1, time2, time3;        /* calibrate time */
  int drawtime;
  double alpha[MAXCOUNT];            /* direction */
  double omega_const[MAXCOUNT];      /*  constant omega speed */
  double delta_az[MAXCOUNT];
  double delta_el[MAXCOUNT];
  int turn[MAXCOUNT];
  int turn_direction[MAXCOUNT];
  int random_pid;
  long planecolor[MAXCOUNT],bg;      /* used colours */
  GC gc[MAXCOUNT];
  GC erase_gc;
  XSegment *xseg[MAXCOUNT];
  XSegment *xseg_old[MAXCOUNT];
  int xcldelay;
  int no_preset;
} xclstruct;

static xclstruct *xcls = (xclstruct *) NULL;

static int my_random(void)  /* not really good, but it works */
{
  static int number;
  number -= random_pid;
  return ( 0x0fff & (number));
}

static int countlines(void)
{
  int array;
  int count = 0;
  for(array = 0;array<LINEARRAYS;array++) {
    count += model_data2[array][0] - 1;
  }
  return(count);
}

static void um2(int X, int Y, int Z, xclstruct *dp)
{
  float X2,X3,X4,Y1,Y3,Y4,Z1,Z2,Z4;

  Y1 = dp->Ca * Y - dp->Sa * Z;
  Z1 = dp->Sa * Y + dp->Ca * Z;
  X2 = dp->Cb * X - dp->Sb * Z1;
  Z2 = dp->Sb * X + dp->Cb * Z1;
  X3 = dp->Cc * X2 - dp->Sc * Y1;
  Y3 = dp->Sc * X2 + dp->Cc * Y1;
  X4 = X3 + dp->Vx;
  Y4 = Y3 + dp->Vy;
  Z4 = Z2 + dp->Vz;

  dp->Bx = dp->mid_x + (-X4 / Y4 * dp->G);
  dp->By = dp->mid_y - (-Z4 / Y4 * dp->G);
}

static void view_3d(XSegment *xseg, xclstruct *dp)
{
  int count = 0;
  int I,J;
  float BX [ENDPOINTS];
  float BY [ENDPOINTS];

  dp->Ca = cos(dp->Alpha);
  dp->Cb = cos(dp->Beta);
  dp->Cc = cos(dp->Gamma);
  dp->Sa = sin(dp->Alpha);
  dp->Sb = sin(dp->Beta);
  dp->Sc = sin(dp->Gamma);

  for(I = 0; I < ENDPOINTS; I++) {
    um2( model_data1 [ I * 3 + 0], model_data1 [ I * 3 + 1],
         model_data1 [ I * 3 + 2],dp);
    BX [I] = dp->Bx;
    BY [I] = dp->By;
  }
  for (I = 0; I < LINEARRAYS; I++) {
    for (J = 1; J < model_data2[I][0]; J++) {
      xseg[count].x1 = (short) BX[(model_data2[I][J])-1];
      xseg[count].y1 = (short) BY[(model_data2[I][J])-1]+(dp->mid_y/2);
      xseg[count].x2 = (short) BX[(model_data2[I][J+1])-1];
      xseg[count].y2 = (short) BY[(model_data2[I][J+1])-1]+(dp->mid_y/2);
      count++;
    }
  }
}

static long get_color(Display *dpy, char *color, XColor *final_color)
{
  XColor cdef;
  Colormap cmap;
  cmap = DefaultColormap(dpy,DefaultScreen(dpy));

  if (!XParseColor(dpy, cmap, color, &cdef) ||
      !XAllocColor(dpy, cmap, &cdef))
    {
      (void) fprintf(stderr, "Color \"%s\" wasn't found\n", color);
    }

  if (final_color != NULL) *final_color = cdef;  /* copy the final color. */

  return(cdef.pixel);
}

static Bool get_GC(Display *dpy,Window win,GC *gc,long color)
{
  unsigned long valuemask = 0;  /*ignore XGCvalues and use defaults */
  XGCValues values;
  unsigned int line_width = 1;
  int line_style = LineSolid;   /* LineOnOffDash;*/
  int cap_style = CapRound;
  int join_style = JoinRound;

  if ((*gc = XCreateGC(dpy, win, valuemask , &values)) == None)
    return False;
  XSetForeground(dpy, *gc, color);

  XSetLineAttributes(dpy, *gc, line_width, line_style,
             cap_style, join_style);
  return True;
}

static void
free_xcl(Display *display, xclstruct  *dp)
{
    int plane;

    for (plane = 0; plane < dp->planes; plane++) {
      if (dp->xseg[plane] != NULL) {
        free(dp->xseg[plane]);
        dp->xseg[plane] = (XSegment *) NULL;
      }
      if (dp->xseg_old[plane] != NULL) {
        free(dp->xseg_old[plane]);
        dp->xseg_old[plane] = (XSegment *) NULL;
      }
      if (dp->gc[plane] != None) {
        XFreeGC(display, dp->gc[plane]);
        dp->gc[plane] = None;
      }
    }
    if (dp->erase_gc != None) {
      XFreeGC(display, dp->erase_gc);
      dp->erase_gc = None;
    }
}

void release_xcl(ModeInfo * mi)
{
  if (xcls != NULL) {
    int screen;
    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
      free_xcl(MI_DISPLAY(mi), &xcls[screen]);
    free(xcls);
    xcls = (xclstruct *) NULL;
  }
}

void init_xcl(ModeInfo * mi)
{
  Display *display = MI_DISPLAY(mi);
  int i;            /* scratch */
  xclstruct *dp;

  if (xcls == NULL) {
    if ((xcls = (xclstruct *) calloc(MI_NUM_SCREENS(mi),
                                     sizeof (xclstruct))) == NULL)
      return;
  }
  dp = &xcls[MI_SCREEN(mi)];

  /* Update every time */
  dp->width = MI_WIDTH(mi);
  dp->height = MI_HEIGHT(mi);
  dp->mid_x = (dp->width / 2);
  dp->mid_y = (dp->height / 2);

  if(dp->no_preset != 1) {
    dp->no_preset = 1;
    /* some presettings */
    dp->planes = MI_COUNT(mi);
    if (dp->planes < -MINPLANES) {
      dp->planes = NRAND(-MI_COUNT(mi) -MINPLANES + 1) + MINPLANES;
    } else if (dp->planes < MINPLANES) {
      dp->planes = MINPLANES;
    }
    if(dp->planes > MAXCOUNT)
      dp->planes = MAXCOUNT;
    dp->Alpha = 0.0;          /* rotate.1 */
    dp->Beta  = 0.0;          /* rotate.2 */
    dp->Gamma = 0.0;          /* rotate.3 */
    dp->Vx = 1;               /* width from zero in X */
    dp->Vy = 800;             /* width from zero in Y */
    dp->Vz = -300;            /* width from zero in Z */
    dp->G =  500.0;           /* ZOOM  */
    dp->time3 = 1.0;
    dp->drawtime = 25000;
    dp->xcldelay = STARTUPDELAY;
    for(i=0;i< dp->planes; i++) {
      dp->az[i] = 2 * M_PI * i / (float)((dp->planes));
      dp->el[i] = 0.0;
      dp->alpha[i] = 0.75;      /* direction */
      dp->turn[i] = 0;
      dp->turn_direction[i] = 1;

      speed[i] = speed_in;  /* see TODO */
    }

    random_pid = getpid(); /* goes here first for randomstart */

    if(randomstart) {
      for(i=0;i< dp->planes; i++) {
        switch(i) {
        case 0:
          dp->az[0] += (random_pid % 31) / 5.0;
          break;
        default:
          dp->az[i] = dp->az[0] + 2 * M_PI * i / (float)((dp->planes));
        }
      }
    }

    dp->bg = MI_BLACK_PIXEL(mi);

    if(MI_IS_MONO(mi))
      for(i=0;i< dp->planes; i++) {
        dp->planecolor[i] = MI_WHITE_PIXEL(mi);
      }
    else {
      if(!oldcolor) {
        for(i=0;i< dp->planes; i++) {
          dp->planecolor[i] = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
        }
      }
      else { /* with count >2 no so good */
        for(i=0;i< dp->planes; i++) {
          switch(i) {
          case 0:
            dp->planecolor[0] = get_color(display, (char *) "yellow",
		(XColor *) NULL);
            break;
          case 1:
            dp->planecolor[1] = get_color(display, (char *) "red",
		(XColor *) NULL);
            break;
          default:
            dp->planecolor[i] = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
          }
        }
      }
    }

    if(dp->erase_gc == NULL)
      if (!get_GC(display, MI_WINDOW(mi), &(dp->erase_gc),dp->bg)) {
        free_xcl(display, dp);
        return;
      }

    dp->lines = countlines();

    for(i=0;i< dp->planes; i++) {
      if(dp->gc[i] == NULL)
        if (!get_GC(display, MI_WINDOW(mi), &(dp->gc[i]),
                    dp->planecolor[i])) {
          free_xcl(display, dp);
          return;
        }
      dp->omega_const[i] = speed[i]/3.6 /line_length*1000.0;

      if(dp->xseg[i] == NULL)
        if ((dp-> xseg[i] = (XSegment *) malloc(sizeof(XSegment) *
                                                dp->lines)) == NULL) {
          free_xcl(display, dp);
          return;
        }
      if(dp->xseg_old[i] == NULL)
        if ((dp->xseg_old[i] = (XSegment *) malloc(sizeof(XSegment) *
                                                   dp->lines)) == NULL) {
          free_xcl(display, dp);
          return;
        }
    }

    if(MI_IS_VERBOSE(mi)) {
      (void) printf("X control line combat in a box\n");
#if !defined( lint ) && !defined( SABER )
      (void) printf("Version: %s\n",sccsid);
#endif
      (void) printf("Line length: %gm\n",line_length/1000.0);
      (void) printf("Speed %g km/h  \n",speed[0]);
      (void) printf("Lines per plane: %d\n",dp->lines);
      (void) printf("Spectator at %gm\n",spectator/1000.0);
      (void) printf("Try %g frames per Second (frametime: %dus)\n",
                    1000000.0/frametime,frametime);
      (void) printf("Calibration at %d frames\n",REGULATE);
    }
  }

  /* clear the screen */

  MI_CLEARWINDOW(mi);

  (void) gettimeofday(&(dp->tv1),0);
  dp->time1 = (double)dp->tv1.tv_sec +
    (double)dp->tv1.tv_usec/(double)1000000;

  dp->xcldelay = frametime;
}

void draw_xcl(ModeInfo * mi)
{
  static int count = 0;
  int i;
  xclstruct *dp;

  if (xcls == NULL)
    return;
  dp = &xcls[MI_SCREEN(mi)];
  if (dp->erase_gc == None)
    return;

  MI_IS_DRAWN(mi) = True;

  if(viewmodel == True)
    {
      dp->Vx = 1;                     /* movement in X (width,negativ)*/
      if(dp->width < 900)
        dp->Vy = 800*800/dp->width;   /*  Y (deep)*/
      else
        dp->Vy = 800*1200/dp->width;  /*  Y (deep)*/
      dp->Vz = -300;                  /*  Z (height) */
      dp->G = 350.0;          /* make it smaller when display is smaller */
      dp->Alpha += 0.03;
      dp->Beta += 0.006;
      dp->Gamma += 0.009;

      if (count != 0) {
        XDrawSegments(MI_DISPLAY(mi),MI_WINDOW(mi),dp->erase_gc,
                      dp->xseg_old[0],dp->lines);
        XDrawSegments(MI_DISPLAY(mi),MI_WINDOW(mi),dp->gc[0],
                      dp->xseg[0],dp->lines);
        (void) usleep(ROTATEDELAY * 2);
      }
      (void) memcpy(dp->xseg_old[0],dp->xseg[0],sizeof(XSegment)*dp->lines);
      view_3d(dp->xseg[0],dp);
      count ++;
    }
  else {
    if(automatic)
      dp->G = dp->width / 2.1 ;

    for(i=0;i<dp->planes;i++) {
      (void) memcpy(dp->xseg_old[i],dp->xseg[i],sizeof(XSegment)*dp->lines);

      dp->Alpha = - dp->alpha[i];
      dp->Beta = - dp->el[i];
      dp->Gamma = dp->az[i];

      dp->Vx = -(int)(cos(dp->az[i]) * cos(dp->el[i]) * line_length);
      dp->Vy = spectator -
        (int)(sin(dp->az[i]) * cos(dp->el[i]) * line_length);
      dp->Vz = (int)(sin(dp->el[i]) * line_length);

      view_3d(dp->xseg[i],dp);
      XDrawSegments(MI_DISPLAY(mi),MI_WINDOW(mi),dp->erase_gc,
                    dp->xseg_old[i],dp->lines);
      XDrawSegments(MI_DISPLAY(mi),MI_WINDOW(mi),dp->gc[i],
                    dp->xseg[i],dp->lines);
    }

    XFlush(MI_DISPLAY(mi));

    /* now move all planes */
    for(i=0;i<dp->planes;i++) {
      dp->delta_az[i] = cos(dp->alpha[i]) * dp->omega_const[i] *
        frametime/1000000;
      dp->delta_el[i] = sin(dp->alpha[i]) * dp->omega_const[i] *
        frametime/1000000;

      dp->az[i] -= dp->delta_az[i];
      dp->el[i] -= dp->delta_el[i];

      if (dp->el[i] >= 0.0)
        switch (dp->turn[i]) {
        case 0:
          dp->turn_direction[i] *= -1;
          dp->alpha[i] += 0.62831853 * dp->turn_direction[i];
          dp->turn[i] ++;
          break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
          dp->alpha[i] += 0.62831853 * dp->turn_direction[i];
          dp->turn[i] ++;
          break;
        default:
          dp->turn[i] ++;
          break;
        }
      else
        dp->turn[i] = 0;
      if (dp->el[i] <= -(M_PI / 2.0))
        {
          dp->alpha[i] += M_PI;
          dp->az[i] += M_PI;
          /* el[i] = el[i] + (M_PI / 2.0) ; */
        }
      else
        if(dp->turn[i] == 0)
          dp->alpha[i] += (double)(RAND(600))  / 6283.0 * 2.0 * M_PI;
    } /* for (i) */

    count++;

    (void) usleep(dp->xcldelay);
    if((count % REGULATE) == 0) {
      (void) gettimeofday(&(dp->tv1),0);
      dp->time2 = (double)dp->tv1.tv_sec + (double)dp->tv1.tv_usec/
        (double)1000000;
      dp->time3 = dp->time2 - dp->time1;
      dp->time1 = dp->time2;
      dp->drawtime = (int) (dp->time3 * (1000000/REGULATE))
        - dp->xcldelay;
      if((dp->xcldelay = frametime - dp->drawtime) <= 0) {
        dp->xcldelay = 10;  /* 1 is possible xor xscreensaver mode */
      }
      if(debug == True)
        (void) printf("t_draw: %d, t_delay: %d\n",dp->drawtime,
                      dp->xcldelay);
    }
  }
}

#endif /* MODE_xcl */
