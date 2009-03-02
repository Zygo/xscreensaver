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
static struct _flake
{
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
   Display            *DisplayVar;
   Window              WindowVar;
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
unsigned int
FuzzyFlakesColorResource(char *Color)
{
   XColor              color;

   if (!XParseColor(Flake.DisplayVar, Flake.XGWA.colormap, Color, &color))
     {
	fprintf(stderr, "%s: can't parse color %s", progname, Color);
	return 0;
     }
   if (!XAllocColor(Flake.DisplayVar, Flake.XGWA.colormap, &color))
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
FuzzyFlakesColorHelper(void)
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
   if (!XParseColor(Flake.DisplayVar, Flake.XGWA.colormap,
                    Flake.Colors.Back, &color))
     {
	fprintf(stderr, "%s: can't parse color %s", progname,
                Flake.Colors.Back);
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

   Flake.Colors.Fore = malloc(sizeof(unsigned char) * 8);
   Flake.Colors.Bord = malloc(sizeof(unsigned char) * 8);

   sprintf(Flake.Colors.Fore, "#%02X%02X%02X", iR0, iG0, iB0);
   sprintf(Flake.Colors.Bord, "#%02X%02X%02X", iR1, iG1, iB1);

   return 0;
}

static void
FuzzyFlakesInit(Display * dpy, Window window)
{
   int                 i, j;
   XWindowAttributes   xgwa;
   Colormap            cmap;

   XGetWindowAttributes(dpy, window, &xgwa);
   cmap = xgwa.colormap;
   Flake.XGWA = xgwa;
   Flake.DB.b = Flake.DB.ba = Flake.DB.bb = 0;
   Flake.DB.dbuf = get_boolean_resource("doubleBuffer", "Boolean");

   if (Flake.DB.dbuf)
     {
	Flake.DB.ba =
	   XCreatePixmap(dpy, window, xgwa.width, xgwa.height, xgwa.depth);
	Flake.DB.bb =
	   XCreatePixmap(dpy, window, xgwa.width, xgwa.height, xgwa.depth);
	Flake.DB.b = Flake.DB.ba;
     }
   else
     {
	Flake.DB.b = window;
     }

   Flake.DisplayVar = dpy;
   Flake.WindowVar = window;
   Flake.Arms = get_integer_resource("arms", "Integer");
   Flake.Thickness = get_integer_resource("thickness", "Integer");
   Flake.BorderThickness = get_integer_resource("bthickness", "Integer");
   Flake.Radius = get_integer_resource("radius", "Integer");

   Flake.Density = get_integer_resource("density", "Integer");
   Flake.Layers = get_integer_resource("layers", "Integer");
   Flake.FallingSpeed = get_integer_resource("fallingspeed", "Integer");
   Flake.Delay = get_integer_resource("delay", "Integer");
   if (Flake.RandomColors == True)
      Flake.RandomColors = get_boolean_resource("randomColors", "Boolean");

   if (Flake.Delay < 0)
      Flake.Delay = 0;

   if (!Flake.Colors.Back)
     {
	Flake.Colors.Back = get_string_resource("color", "Color");
	if (!FuzzyFlakesColorResource(Flake.Colors.Back))
	  {
	     fprintf(stderr, " reverting to random\n");
	     Flake.RandomColors = True;
	  }

	if (Flake.RandomColors)
	  {
	     if (Flake.Colors.Back)
		free(Flake.Colors.Back);
	     Flake.Colors.Back = malloc(sizeof(unsigned char) * 8);
	     sprintf(Flake.Colors.Back, "#%X%X%X%X%X%X", random() % 16,
		     random() % 16, random() % 16, random() % 16, random() % 16,
		     random() % 16);
	  }

	/*
	 * Here we establish our colormap based on what is in
	 * Flake.Colors.Back
	 */
	if (FuzzyFlakesColorHelper())
	  {
	     fprintf(stderr, " reverting to random\n");
	     if (Flake.Colors.Back)
		free(Flake.Colors.Back);
	     Flake.Colors.Back = malloc(sizeof(unsigned char) * 8);
	     sprintf(Flake.Colors.Back, "#%X%X%X%X%X%X", random() % 16,
		     random() % 16, random() % 16, random() % 16, random() % 16,
		     random() % 16);
	     FuzzyFlakesColorHelper();
	  }

	Flake.ForeColor = FuzzyFlakesColorResource(Flake.Colors.Fore);
	Flake.BackColor = FuzzyFlakesColorResource(Flake.Colors.Back);
	Flake.BordColor = FuzzyFlakesColorResource(Flake.Colors.Bord);

	Flake.GCValues.foreground = Flake.ForeColor;
	Flake.GCValues.background = Flake.BackColor;
	Flake.RandomColors = False;
     }

   Flake.GCValues.line_width = Flake.Thickness;
   Flake.GCValues.line_style = LineSolid;
   Flake.GCValues.cap_style = CapProjecting;
   Flake.GCValues.join_style = JoinMiter;
   Flake.GCFlags |= (GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle);

   Flake.GCVar =
      XCreateGC(Flake.DisplayVar, Flake.WindowVar, Flake.GCFlags,
		&Flake.GCValues);

   Flake.Density = Flake.XGWA.width / 200 * Flake.Density;
   Flake.Flakes = malloc(sizeof(FlakeVariable **) * Flake.Layers);
   for (i = 1; i <= Flake.Layers; i++)
     {
	Flake.Flakes[i - 1] = malloc(sizeof(FlakeVariable *) * Flake.Density);
	for (j = 0; j < Flake.Density; j++)
	  {
	     Flake.Flakes[i - 1][j] = malloc(sizeof(FlakeVariable));
	     Flake.Flakes[i - 1][j]->XPos = random() % Flake.XGWA.width;
	     Flake.Flakes[i - 1][j]->YPos = random() % Flake.XGWA.height;
	     Flake.Flakes[i - 1][j]->Angle = random() % 360 * (M_PI / 180);
	     Flake.Flakes[i - 1][j]->Ticks = random() % 360;
	     Flake.Flakes[i - 1][j]->XOffset = random() % Flake.XGWA.height;
	  }
     }
}

static void
FuzzyFlakesFreeFlake(void)
{
   int                 i, j;

   for (i = 1; i <= Flake.Layers; i++)
     {
	for (j = 0; j < Flake.Density; j++)
	  {
	     free(Flake.Flakes[i - 1][j]);
	  }
	free(Flake.Flakes[i - 1]);
     }

   XFreePixmap(Flake.DisplayVar, Flake.DB.bb);
   XFreePixmap(Flake.DisplayVar, Flake.DB.ba);
}

static void
FuzzyFlakesResizeTest(void)
{
   XWindowAttributes   xgwa;

   XGetWindowAttributes(Flake.DisplayVar, Flake.WindowVar, &xgwa);
   if (Flake.XGWA.width != xgwa.width || Flake.XGWA.height != xgwa.height)
     {
	FuzzyFlakesFreeFlake();
	FuzzyFlakesInit(Flake.DisplayVar, Flake.WindowVar);
     }
}

static void
FuzzyFlakesMove(void)
{
   int                 i, j;

   for (i = 1; i <= Flake.Layers; i++)
     {
	for (j = 0; j < Flake.Density; j++)
	  {
	     FlakeVariable      *FlakeVar;

	     FlakeVar = Flake.Flakes[i - 1][j];
	     FlakeVar->Ticks++;
	     FlakeVar->YPos =
		FlakeVar->YPos + ((double)Flake.FallingSpeed) / 10 / i;
	     FlakeVar->TrueX =
		(sin
		 (FlakeVar->XOffset +
		  FlakeVar->Ticks * (M_PI / 180) * ((double)Flake.FallingSpeed /
						    10))) * 10 + FlakeVar->XPos;
	     FlakeVar->Angle =
		FlakeVar->Angle + 0.005 * ((double)Flake.FallingSpeed / 10);
	     if (FlakeVar->YPos - Flake.Radius > Flake.XGWA.height)
	       {
		  FlakeVar->Ticks = 0;
		  FlakeVar->YPos = 0 - Flake.Radius;
	       }
	  }
     }
}

static void
FuzzyFlakesDrawFlake(int XPos, int YPos, double AngleOffset, int Layer)
{
   int                 i;
   double              x, y, Angle, Radius;

   /* calculate the shrink factor debending on which layer we are drawing atm */
   Radius = (double)(Flake.Radius - Layer * 5);
   /* draw the flake one arm at a time */
   for (i = 1; i <= Flake.Arms; i++)
     {
	int                 Diameter;

	Diameter = (Flake.BorderThickness * 2 + Flake.Thickness) / Layer;
	/* compute the angle of this arm of the flake */
	Angle = ((2 * M_PI) / Flake.Arms) * i + AngleOffset;
	/* calculate the x and y dispositions for this arm */
	y = (int)(sin(Angle) * Radius);
	x = (int)(cos(Angle) * Radius);
	/* draw the base for the arm */
	Flake.GCValues.line_width = Diameter;
	XFreeGC(Flake.DisplayVar, Flake.GCVar);
	Flake.GCVar =
	   XCreateGC(Flake.DisplayVar, Flake.DB.b, Flake.GCFlags,
		     &Flake.GCValues);
	XSetForeground(Flake.DisplayVar, Flake.GCVar, Flake.BordColor);
	XDrawLine(Flake.DisplayVar, Flake.DB.b, Flake.GCVar, XPos, YPos,
		  XPos + x, YPos + y);
     }
   /* draw the flake one arm at a time */
   for (i = 1; i <= Flake.Arms; i++)
     {
	/* compute the angle of this arm of the flake */
	Angle = ((2 * M_PI) / Flake.Arms) * i + AngleOffset;
	/* calculate the x and y dispositions for this arm */
	y = (int)(sin(Angle) * Radius);
	x = (int)(cos(Angle) * Radius);
	/* draw the inside of the arm */
	Flake.GCValues.line_width = Flake.Thickness / Layer;
	XFreeGC(Flake.DisplayVar, Flake.GCVar);
	Flake.GCVar =
	   XCreateGC(Flake.DisplayVar, Flake.DB.b, Flake.GCFlags,
		     &Flake.GCValues);
	XSetForeground(Flake.DisplayVar, Flake.GCVar, Flake.ForeColor);
	XDrawLine(Flake.DisplayVar, Flake.DB.b, Flake.GCVar, XPos, YPos,
		  XPos + x, YPos + y);
     }
}

static void
FuzzyFlakes(Display * dpy, Window window)
{
   int                 i, j;

   FuzzyFlakesMove();
   XSetForeground(Flake.DisplayVar, Flake.GCVar, Flake.BackColor);
   XFillRectangle(Flake.DisplayVar, Flake.DB.b, Flake.GCVar, 0, 0,
		  Flake.XGWA.width, Flake.XGWA.height);
   for (i = Flake.Layers; i >= 1; i--)
     {
	for (j = 0; j < Flake.Density; j++)
	  {
	     FuzzyFlakesDrawFlake(Flake.Flakes[i - 1][j]->TrueX,
				  Flake.Flakes[i - 1][j]->YPos,
				  Flake.Flakes[i - 1][j]->Angle, i);
	  }
     }

}

char               *progclass = "FuzzyFlakes";
char               *defaults[] = {
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

XrmOptionDescRec    options[] = {
   {
    "-color", ".color", XrmoptionSepArg, 0},
   {
    "-arms", ".arms", XrmoptionSepArg, 0},
   {
    "-thickness", ".thickness", XrmoptionSepArg, 0},
   {
    "-bthickness", ".bthickness", XrmoptionSepArg, 0},
   {
    "-radius", ".radius", XrmoptionSepArg, 0},
   {
    "-layers", ".layers", XrmoptionSepArg, 0},
   {
    "-density", ".density", XrmoptionSepArg, 0},
   {
    "-speed", ".fallingspeed", XrmoptionSepArg, 0},
   {
    "-delay", ".delay", XrmoptionSepArg, 0},
   {
    "-db", ".doubleBuffer", XrmoptionNoArg, "True"},
   {
    "-no-db", ".doubleBuffer", XrmoptionNoArg, "False"},
   {
    "-random-colors", ".randomColors", XrmoptionNoArg, "True"},
   {
    0, 0, 0, 0}
};

void
screenhack(Display * dpy, Window window)
{
   register int        tick = 0;

   /* This is needed even if it is going to be set to false */
   Flake.RandomColors = True;

   /* set up our colors amoung many other things */
   FuzzyFlakesInit(dpy, window);

   while (1)
     {
	/* Test every 50 ticks for a screen resize */
	tick++;
	if (tick == 50)
	  {
	     FuzzyFlakesResizeTest();
	     tick = 0;
	  }
	FuzzyFlakes(dpy, Flake.DB.b);
	if (Flake.DB.dbuf)
	  {
	     XCopyArea(Flake.DisplayVar, Flake.DB.b, Flake.WindowVar,
		       Flake.GCVar, 0, 0, Flake.XGWA.width, Flake.XGWA.height,
		       0, 0);
	     Flake.DB.b =
		(Flake.DB.b == Flake.DB.ba ? Flake.DB.bb : Flake.DB.ba);
	  }
	screenhack_handle_events(dpy);
	XSync(dpy, False);
	if (Flake.Delay)
	   usleep(Flake.Delay);
     }
}

/* EOF */
