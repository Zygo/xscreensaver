/* fuzzyflakes, Copyright (c) 2004
 *  Barry Dmytro <badcherry@mailc.net>
 *
 * ! 2004.06.10 21:05
 * ! - Added support for resizing
 * ! - Added a color scheme generation algorithm
 * !    Thanks to <ZoeB> from #vegans@irc.blitzed.org
 * ! - Added random color generation
 * ! - Fixed errors in the xml config file
 * ! - Cleaned up a few inconsistencies in the code
 * ! - Changed the default color to #EFBEA5
 *
 * ! 2004.05.?? ??:??
 * ! -original creation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <math.h>
#include "screenhack.h"

/* I have need of 1/3 and 2/3 constants later on */
#define N1_3 0.3333333333
#define N2_3 0.6666666666

typedef struct _flake_var
{
   double              Ticks;
   double              XPos, YPos;
   double              TrueX;
   double              XOffset;
   double              Angle;
} FlakeVariable;

/* Struct containing the atrributes to our flakes */
typedef struct _flake
{
   Display *dpy;
   Window window;

   int                 Arms;
   int                 Thickness;
   int                 BorderThickness;
   int                 Radius;
   unsigned long       BordColor;
   unsigned long       ForeColor;
   unsigned long       BackColor;
   Bool                RandomColors;
   int                 Layers;
   int                 Density;
   int                 Delay;
   int                 FallingSpeed;
   struct _colors
   {
      char               *Fore;
      char               *Bord;
      char               *Back;
   } Colors;
/* a dynamic array containing positions of all the flakes */
   FlakeVariable    ***Flakes;
   XGCValues           GCValues;
   unsigned long       GCFlags;
   GC                  GCVar;
   XWindowAttributes   XGWA;
   struct _dbevar
   {
      Bool                dbuf;
      Pixmap              b, ba, bb;
   } DB;
} Flake;

/*
 *This gets the pixel resource for a color: #ffffff
 */
static unsigned int
FuzzyFlakesColorResource(Flake *flake, char *Color)
{
   XColor              color;

   if (!XParseColor(flake->dpy, flake->XGWA.colormap, Color, &color))
     {
	fprintf(stderr, "%s: can't parse color %s", progname, Color);
	return 0;
     }
   if (!XAllocColor(flake->dpy, flake->XGWA.colormap, &color))
     {
	fprintf(stderr, "%s: can't allocate color %s", progname, Color);
	return 0;
     }
   return color.pixel;
}

/*
 * This is a great color matching algorithm that I got from
 * a friend of mine on #vegans@irc.blitzed.org
 * She wrote it in PHP and I ported it over to C
 * her site is http://beautifulfreak.net/
 */
static int
FuzzyFlakesColorHelper(Flake *flake)
{
   unsigned int        iR, iG, iB;
   unsigned int        iR0, iG0, iB0;
   unsigned int        iR1, iG1, iB1;
   float               fR, fG, fB;
   float               Max = 0, Min = 0, Lig, Sat;
   float               Hue, Hue0, Hue1;
   float               f1, f2;
   float               fR0, fG0, fB0;
   float               fR1, fG1, fB1;
   float               nR0, nG0, nB0;
   float               nR1, nG1, nB1;
   XColor              color;

   /* First convert from hex to dec */
   /* while splitting up the RGB values */
   if (!XParseColor(flake->dpy, flake->XGWA.colormap,
                    flake->Colors.Back, &color))
     {
	fprintf(stderr, "%s: can't parse color %s", progname,
                flake->Colors.Back);
	return 1;
     }
   iR = color.red   >> 8;
   iG = color.green >> 8;
   iB = color.blue  >> 8;

   /* Convert from int to float */
   fR = iR;
   fG = iG;
   fB = iB;

   /* convert from 0-255 to 0-1 */
   fR = fR / 255;
   fG = fG / 255;
   fB = fB / 255;

   /* work out the lightness */
   if (fR >= fG && fR >= fB)
      Max = fR;
   if (fG >= fR && fG >= fB)
      Max = fG;
   if (fB >= fR && fB >= fG)
      Max = fB;

   if (fR <= fG && fR <= fB)
      Min = fR;
   if (fG <= fR && fG <= fB)
      Min = fG;
   if (fB <= fR && fB <= fG)
      Min = fB;

   Lig = (Max + Min) / 2;

   /* work out the saturation */
   if (Max == Min)
      Sat = 0;
   else
     {
	if (Lig < 0.5)
	   Sat = (Max - Min) / (Max + Min);
	else
	   Sat = (Max - Min) / (2 - Max - Min);
     }

   /*
    * if our satration is too low we won't be
    * able to see any objects
    */
   if (Sat < 0.03)
     {
	return 1;
     }

   /* work out the hue */
   if (fR == Max)
      Hue = (fG - fB) / (Max - Min);
   else if (fG == Max)
      Hue = 2 + (fB - fR) / (Max - Min);
   else
      Hue = 4 + (fR - fG) / (Max - Min);

   Hue = Hue / 6;

   /* fine two equidistant hues */
   Hue0 = Hue + N1_3;
   if (Hue0 > 1)
      Hue0 = Hue0 - 1;
   Hue1 = Hue0 + N1_3;
   if (Hue1 > 1)
      Hue1 = Hue1 - 1;

   /* convert the colors into hex codes */
   if (Lig < 0.5)
      f2 = Lig * (1 + Sat);
   else
      f2 = (Lig + Sat) - (Lig * Sat);

   f1 = (2 * Lig) - f2;

   fR0 = (Hue0 + 1) / 3;
   fR1 = (Hue1 + 1) / 3;
   fG0 = Hue0;
   fG1 = Hue1;
   fB0 = (Hue0 - 1) / 3;
   fB1 = (Hue1 - 1) / 3;

   if (fR0 < 0)
      fR0 = fR0 + 1;
   if (fR0 > 1)
      fR0 = fR0 - 1;
   if (fG0 < 0)
      fG0 = fG0 + 1;
   if (fG0 > 1)
      fG0 = fG0 - 1;
   if (fB0 < 0)
      fB0 = fB0 + 1;
   if (fB0 > 1)
      fB0 = fB0 - 1;

   if (fR1 < 0)
      fR1 = fR1 + 1;
   if (fR1 > 1)
      fR1 = fR1 - 1;
   if (fG1 < 0)
      fG1 = fG1 + 1;
   if (fG1 > 1)
      fG1 = fG1 - 1;
   if (fB1 < 0)
      fB1 = fB1 + 1;
   if (fB1 > 1)
      fB1 = fB1 - 1;

   if (6 * fR0 < 1)
      nR0 = f1 + (f2 - f1) * 6 * fR0;
   else if (2 * fR0 < 1)
      nR0 = f2;
   else if (3 * fR0 < 2)
      nR0 = f1 + (f2 - f1) * (N2_3 - fR0) * 6;
   else
      nR0 = f1;

   if (6 * fG0 < 1)
      nG0 = f1 + (f2 - f1) * 6 * fG0;
   else if (2 * fG0 < 1)
      nG0 = f2;
   else if (3 * fG0 < 2)
      nG0 = f1 + (f2 - f1) * (N2_3 - fG0) * 6;
   else
      nG0 = f1;

   if (6 * fB0 < 1)
      nB0 = f1 + (f2 - f1) * 6 * fB0;
   else if (2 * fB0 < 1)
      nB0 = f2;
   else if (3 * fB0 < 2)
      nB0 = f1 + (f2 - f1) * (N2_3 - fB0) * 6;
   else
      nB0 = f1;

   if (6 * fR1 < 1)
      nR1 = f1 + (f2 - f1) * 6 * fR1;
   else if (2 * fR1 < 1)
      nR1 = f2;
   else if (3 * fR1 < 2)
      nR1 = f1 + (f2 - f1) * (N2_3 - fR1) * 6;
   else
      nR1 = f1;

   if (6 * fG1 < 1)
      nG1 = f1 + (f2 - f1) * 6 * fG1;
   else if (2 * fG1 < 1)
      nG1 = f2;
   else if (3 * fG1 < 2)
      nG1 = f1 + (f2 - f1) * (N2_3 - fG1) * 6;
   else
      nG1 = f1;

   if (6 * fB1 < 1)
      nB1 = f1 + (f2 - f1) * 6 * fB1;
   else if (2 * fB1 < 1)
      nB1 = f2;
   else if (3 * fB1 < 2)
      nB1 = f1 + (f2 - f1) * (N2_3 - fB1) * 6;
   else
      nB1 = f1;

   /* at last convert them to a hex string */
   iR0 = nR0 * 255;
   iG0 = nG0 * 255;
   iB0 = nB0 * 255;

   iR1 = nR1 * 255;
   iG1 = nG1 * 255;
   iB1 = nB1 * 255;

   flake->Colors.Fore = malloc(sizeof(unsigned char) * 8);
   flake->Colors.Bord = malloc(sizeof(unsigned char) * 8);

   sprintf(flake->Colors.Fore, "#%02X%02X%02X", iR0, iG0, iB0);
   sprintf(flake->Colors.Bord, "#%02X%02X%02X", iR1, iG1, iB1);

   return 0;
}

static void
FuzzyFlakesInit(Flake *flake)
{
   int                 i, j;
   XWindowAttributes   xgwa;

   XGetWindowAttributes(flake->dpy, flake->window, &xgwa);
   flake->XGWA = xgwa;
   flake->DB.b = flake->DB.ba = flake->DB.bb = 0;
   flake->DB.dbuf = get_boolean_resource(flake->dpy, "doubleBuffer", "Boolean");

# ifdef HAVE_JWXYZ	/* Don't second-guess Quartz's double-buffering */
   flake->DB.dbuf = False;
# endif

   if (flake->DB.dbuf)
     {
	flake->DB.ba =
	   XCreatePixmap(flake->dpy, flake->window, xgwa.width, xgwa.height, xgwa.depth);
	flake->DB.bb =
	   XCreatePixmap(flake->dpy, flake->window, xgwa.width, xgwa.height, xgwa.depth);
	flake->DB.b = flake->DB.ba;
     }
   else
     {
	flake->DB.b = flake->window;
     }

   flake->Arms = get_integer_resource(flake->dpy, "arms", "Integer");
   flake->Thickness = get_integer_resource(flake->dpy, "thickness", "Integer");
   flake->BorderThickness = get_integer_resource(flake->dpy, "bthickness", "Integer");
   flake->Radius = get_integer_resource(flake->dpy, "radius", "Integer");

   flake->Density = get_integer_resource(flake->dpy, "density", "Integer");
   flake->Layers = get_integer_resource(flake->dpy, "layers", "Integer");
   flake->FallingSpeed = get_integer_resource(flake->dpy, "fallingspeed", "Integer");
   flake->Delay = get_integer_resource(flake->dpy, "delay", "Integer");
   if (flake->RandomColors == True)
      flake->RandomColors = get_boolean_resource(flake->dpy, "randomColors", "Boolean");

   if (xgwa.width > 2560) {  /* Retina displays */
     flake->Thickness *= 2;
     flake->BorderThickness *= 2;
     flake->Radius *= 2;
     flake->FallingSpeed *= 2;
   }

   if (flake->Delay < 0)
      flake->Delay = 0;

   if (!flake->Colors.Back)
     {
	flake->Colors.Back = get_string_resource(flake->dpy, "color", "Color");
	if (!FuzzyFlakesColorResource(flake, flake->Colors.Back))
	  {
	     fprintf(stderr, " reverting to random\n");
	     flake->RandomColors = True;
	  }

	if (flake->RandomColors)
	  {
	     if (flake->Colors.Back)
		free(flake->Colors.Back);
	     flake->Colors.Back = malloc(sizeof(unsigned char) * 8);
	     sprintf(flake->Colors.Back, "#%X%X%X%X%X%X", random() % 16,
		     random() % 16, random() % 16, random() % 16, random() % 16,
		     random() % 16);
	  }

	/*
	 * Here we establish our colormap based on what is in
	 * flake->Colors.Back
	 */
	if (FuzzyFlakesColorHelper(flake))
	  {
	     fprintf(stderr, " reverting to random\n");
	     if (flake->Colors.Back)
		free(flake->Colors.Back);
	     flake->Colors.Back = malloc(sizeof(unsigned char) * 8);
	     sprintf(flake->Colors.Back, "#%X%X%X%X%X%X", random() % 16,
		     random() % 16, random() % 16, random() % 16, random() % 16,
		     random() % 16);
	     FuzzyFlakesColorHelper(flake);
	  }

	flake->ForeColor = FuzzyFlakesColorResource(flake, flake->Colors.Fore);
	flake->BackColor = FuzzyFlakesColorResource(flake, flake->Colors.Back);
	flake->BordColor = FuzzyFlakesColorResource(flake, flake->Colors.Bord);

	flake->GCValues.foreground = flake->ForeColor;
	flake->GCValues.background = flake->BackColor;
	flake->RandomColors = False;
     }

   flake->GCValues.line_width = flake->Thickness;
   flake->GCValues.cap_style = CapProjecting;
   flake->GCValues.join_style = JoinMiter;
   flake->GCFlags |= (GCLineWidth | GCCapStyle | GCJoinStyle);

   flake->GCVar =
      XCreateGC(flake->dpy, flake->window, flake->GCFlags,
		&flake->GCValues);

   flake->Density = flake->XGWA.width / 200 * flake->Density;
   flake->Flakes = malloc(sizeof(FlakeVariable **) * flake->Layers);
   for (i = 1; i <= flake->Layers; i++)
     {
	flake->Flakes[i - 1] = malloc(sizeof(FlakeVariable *) * flake->Density);
	for (j = 0; j < flake->Density; j++)
	  {
	     flake->Flakes[i - 1][j] = malloc(sizeof(FlakeVariable));
	     flake->Flakes[i - 1][j]->XPos = random() % flake->XGWA.width;
	     flake->Flakes[i - 1][j]->YPos = random() % flake->XGWA.height;
	     flake->Flakes[i - 1][j]->Angle = random() % 360 * (M_PI / 180);
	     flake->Flakes[i - 1][j]->Ticks = random() % 360;
	     flake->Flakes[i - 1][j]->XOffset = random() % flake->XGWA.height;
	  }
     }
}

static void
FuzzyFlakesFreeFlake(Flake *flake)
{
   int                 i, j;

   for (i = 1; i <= flake->Layers; i++)
     {
	for (j = 0; j < flake->Density; j++)
	  {
	     free(flake->Flakes[i - 1][j]);
	  }
	free(flake->Flakes[i - 1]);
     }

   if (flake->DB.bb) XFreePixmap(flake->dpy, flake->DB.bb);
   if (flake->DB.ba) XFreePixmap(flake->dpy, flake->DB.ba);
}

static void
FuzzyFlakesMove(Flake *flake)
{
   int                 i, j;

   for (i = 1; i <= flake->Layers; i++)
     {
	for (j = 0; j < flake->Density; j++)
	  {
	     FlakeVariable      *FlakeVar;

	     FlakeVar = flake->Flakes[i - 1][j];
	     FlakeVar->Ticks++;
	     FlakeVar->YPos =
		FlakeVar->YPos + ((double)flake->FallingSpeed) / 10 / i;
	     FlakeVar->TrueX =
		(sin
		 (FlakeVar->XOffset +
		  FlakeVar->Ticks * (M_PI / 180) * ((double)flake->FallingSpeed /
						    10))) * 10 + FlakeVar->XPos;
	     FlakeVar->Angle =
		FlakeVar->Angle + 0.005 * ((double)flake->FallingSpeed / 10);
	     if (FlakeVar->YPos - flake->Radius > flake->XGWA.height)
	       {
		  FlakeVar->Ticks = 0;
		  FlakeVar->YPos = 0 - flake->Radius;
	       }
	  }
     }
}

static void
FuzzyFlakesDrawFlake(Flake *flake, int XPos, int YPos, double AngleOffset, int Layer)
{
   int                 i;
   double              x, y, Angle, Radius;

   /* calculate the shrink factor debending on which layer we are drawing atm */
   Radius = (double)(flake->Radius - Layer * 5);
   /* draw the flake one arm at a time */
   for (i = 1; i <= flake->Arms; i++)
     {
	int                 Diameter;

	Diameter = (flake->BorderThickness * 2 + flake->Thickness) / Layer;
	/* compute the angle of this arm of the flake */
	Angle = ((2 * M_PI) / flake->Arms) * i + AngleOffset;
	/* calculate the x and y dispositions for this arm */
	y = (int)(sin(Angle) * Radius);
	x = (int)(cos(Angle) * Radius);
	/* draw the base for the arm */
	flake->GCValues.line_width = Diameter;
	XFreeGC(flake->dpy, flake->GCVar);
	flake->GCVar =
	   XCreateGC(flake->dpy, flake->DB.b, flake->GCFlags,
		     &flake->GCValues);
	XSetForeground(flake->dpy, flake->GCVar, flake->BordColor);
	XDrawLine(flake->dpy, flake->DB.b, flake->GCVar, XPos, YPos,
		  XPos + x, YPos + y);
     }
   /* draw the flake one arm at a time */
   for (i = 1; i <= flake->Arms; i++)
     {
	/* compute the angle of this arm of the flake */
	Angle = ((2 * M_PI) / flake->Arms) * i + AngleOffset;
	/* calculate the x and y dispositions for this arm */
	y = (int)(sin(Angle) * Radius);
	x = (int)(cos(Angle) * Radius);
	/* draw the inside of the arm */
	flake->GCValues.line_width = flake->Thickness / Layer;
	XFreeGC(flake->dpy, flake->GCVar);
	flake->GCVar =
	   XCreateGC(flake->dpy, flake->DB.b, flake->GCFlags,
		     &flake->GCValues);
	XSetForeground(flake->dpy, flake->GCVar, flake->ForeColor);
	XDrawLine(flake->dpy, flake->DB.b, flake->GCVar, XPos, YPos,
		  XPos + x, YPos + y);
     }
}

static void
FuzzyFlakes(Flake *flake)
{
   int                 i, j;

   FuzzyFlakesMove(flake);
   XSetForeground(flake->dpy, flake->GCVar, flake->BackColor);
   XFillRectangle(flake->dpy, flake->DB.b, flake->GCVar, 0, 0,
		  flake->XGWA.width, flake->XGWA.height);
   for (i = flake->Layers; i >= 1; i--)
     {
	for (j = 0; j < flake->Density; j++)
	  {
	     FuzzyFlakesDrawFlake(flake,
                                  flake->Flakes[i - 1][j]->TrueX,
				  flake->Flakes[i - 1][j]->YPos,
				  flake->Flakes[i - 1][j]->Angle, i);
	  }
     }

}

static void *
fuzzyflakes_init (Display *dpy, Window window)
{
   Flake *flake = (Flake *) calloc (1, sizeof(*flake));
   flake->dpy = dpy;
   flake->window = window;

   /* This is needed even if it is going to be set to false */
   flake->RandomColors = True;

   /* set up our colors amoung many other things */
   FuzzyFlakesInit(flake);

   return flake;
}

static unsigned long
fuzzyflakes_draw (Display *dpy, Window window, void *closure)
{
  Flake *flake = (Flake *) closure;

  FuzzyFlakes(flake);
  if (flake->DB.dbuf)
    {
      XCopyArea(flake->dpy, flake->DB.b, flake->window,
                flake->GCVar, 0, 0, flake->XGWA.width, flake->XGWA.height,
                0, 0);
      flake->DB.b =
        (flake->DB.b == flake->DB.ba ? flake->DB.bb : flake->DB.ba);
    }

  return flake->Delay;
}

static void
fuzzyflakes_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  Flake *flake = (Flake *) closure;

  if (flake->XGWA.width != w || flake->XGWA.height != h)
    {
      FuzzyFlakesFreeFlake(flake);
      FuzzyFlakesInit(flake);
    }
}

static Bool
fuzzyflakes_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
fuzzyflakes_free (Display *dpy, Window window, void *closure)
{
  Flake *flake = (Flake *) closure;
  FuzzyFlakesFreeFlake(flake);
  free(flake);
}


static const char *fuzzyflakes_defaults[] = {
   "*color:	#efbea5",
   "*arms:	5",
   "*thickness:	10",
   "*bthickness:	3",
   "*radius:	20",
   "*layers:	3",
   "*density:	5",
   "*fallingspeed:	10",
   "*delay:	10000",
   "*doubleBuffer:	True",
   "*randomColors:      False",
   0
};

static XrmOptionDescRec fuzzyflakes_options[] = {
   { "-color", ".color", XrmoptionSepArg, 0},
   { "-arms", ".arms", XrmoptionSepArg, 0},
   { "-thickness", ".thickness", XrmoptionSepArg, 0},
   { "-bthickness", ".bthickness", XrmoptionSepArg, 0},
   { "-radius", ".radius", XrmoptionSepArg, 0},
   { "-layers", ".layers", XrmoptionSepArg, 0},
   { "-density", ".density", XrmoptionSepArg, 0},
   { "-speed", ".fallingspeed", XrmoptionSepArg, 0},
   { "-delay", ".delay", XrmoptionSepArg, 0},
   { "-db", ".doubleBuffer", XrmoptionNoArg, "True"},
   { "-no-db", ".doubleBuffer", XrmoptionNoArg, "False"},
   { "-random-colors", ".randomColors", XrmoptionNoArg, "True"},
   { 0, 0, 0, 0}
};


XSCREENSAVER_MODULE ("FuzzyFlakes", fuzzyflakes)
