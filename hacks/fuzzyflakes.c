/* fuzzyflakes, Copyright (c) 2004
 *  Barry Dmytro <badcherry@mailc.net>
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
#define PI 3.14159265359

typedef struct _flake_var
{
  double Ticks;
  double XPos, YPos;
  double TrueX;
  double XOffset;
  double Angle;
} FlakeVariable;

static struct _flake
{ /* Struct containing the atrributes to our flakes */
  int Arms;
  int Thickness;
  int BorderThickness;
  int Radius;
  unsigned long BordColor;
  unsigned long ForeColor;
  unsigned long BackColor;
  int Layers;
  int Density;
  int Delay;
  int FallingSpeed;
  FlakeVariable *** Flakes; /* a dynamic array containing positions of all the flakes */
  XGCValues GCValues;
  unsigned long GCFlags;
  GC GCVar;
  Display * DisplayVar;
  Window WindowVar;
  XWindowAttributes XGWA;
  struct _dbevar
  {
    Bool dbuf;
    Pixmap b,ba,bb;
  } DB;
} Flake;

static void
InitFuzzyFlakes (Display *dpy, Window window)
{
  int i,j;
  XWindowAttributes xgwa;
  Colormap cmap;
  
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  Flake.XGWA = xgwa;
  Flake.DB.b = Flake.DB.ba = Flake.DB.bb = 0;
  Flake.DB.dbuf		= get_boolean_resource ("doubleBuffer", "Boolean");
  

  if (Flake.DB.dbuf)
    {
      Flake.DB.ba = XCreatePixmap (dpy, window, xgwa.width, xgwa.height,xgwa.depth);
      Flake.DB.bb = XCreatePixmap (dpy, window, xgwa.width, xgwa.height,xgwa.depth);
      Flake.DB.b = Flake.DB.ba;
    }
  else
    {
      Flake.DB.b = window;
    }


  Flake.DisplayVar	= dpy;
  Flake.WindowVar	= window;
  Flake.Arms		= get_integer_resource ("arms", "Integer");
  Flake.Thickness	= get_integer_resource ("thickness", "Integer");
  Flake.BorderThickness	= get_integer_resource ("bthickness", "Integer");
  Flake.Radius		= get_integer_resource ("radius", "Integer");
  Flake.BordColor	= get_pixel_resource   ("border","Border",dpy,cmap);
  Flake.ForeColor	= get_pixel_resource   ("foreground","Foreground",dpy,cmap);
  Flake.BackColor	= get_pixel_resource   ("background","Background",dpy,cmap);
  Flake.Density		= get_integer_resource ("density", "Integer");
  Flake.Layers		= get_integer_resource ("layers", "Integer");
  Flake.FallingSpeed	= get_integer_resource ("fallingspeed", "Integer");
  Flake.Delay		= get_integer_resource ("delay", "Integer");
  
  if (Flake.Delay < 0) Flake.Delay = 0;

  Flake.GCValues.foreground = get_pixel_resource("foreground","Foreground", dpy, cmap);
  Flake.GCValues.background = get_pixel_resource("background","Background", dpy, cmap);
  Flake.GCValues.line_width = Flake.Thickness;
  Flake.GCValues.line_style = LineSolid;
  Flake.GCValues.cap_style  = CapProjecting;
  Flake.GCValues.join_style = JoinMiter;
  Flake.GCFlags |= (GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle);

  Flake.GCVar = XCreateGC (Flake.DisplayVar, Flake.WindowVar, Flake.GCFlags, &Flake.GCValues);

  Flake.Density = Flake.XGWA.width/200*Flake.Density;
  Flake.Flakes = malloc(sizeof(FlakeVariable**)*Flake.Layers);
  for(i=1;i<=Flake.Layers;i++)
  {
     Flake.Flakes[i-1] = malloc(sizeof(FlakeVariable*)*Flake.Density);
     for(j=0;j<Flake.Density;j++)
     {
       Flake.Flakes[i-1][j] = malloc(sizeof(FlakeVariable));
       Flake.Flakes[i-1][j]->XPos = random()%Flake.XGWA.width;
       Flake.Flakes[i-1][j]->YPos = random()%Flake.XGWA.height;
       Flake.Flakes[i-1][j]->Angle = random()%360*(PI/180);
       Flake.Flakes[i-1][j]->Ticks = random()%360;
       Flake.Flakes[i-1][j]->XOffset = random()%Flake.XGWA.height;
     }
  }
}

static void
FuzzyFlakesMove(void)
{
  int i,j;
  for(i=1;i<=Flake.Layers;i++)
  {
     for(j=0;j<Flake.Density;j++)
     {
       FlakeVariable * FlakeVar;
       FlakeVar = Flake.Flakes[i-1][j];
       FlakeVar->Ticks++;
       FlakeVar->YPos = FlakeVar->YPos + ((double)Flake.FallingSpeed)/10/i;
       FlakeVar->TrueX = (sin(FlakeVar->XOffset + FlakeVar->Ticks*(PI/180)*((double)Flake.FallingSpeed/10)))*10 + FlakeVar->XPos;
       FlakeVar->Angle = FlakeVar->Angle + 0.005*((double)Flake.FallingSpeed/10);
       if(FlakeVar->YPos - Flake.Radius > Flake.XGWA.height)
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
  int i;
  double x,y,Angle,Radius;
  
  /* calculate the shrink factor debending on which layer we are drawing atm */
  Radius = (double)(Flake.Radius - Layer * 5);
  
  /* draw the flake one arm at a time */
  for(i=1;i<=Flake.Arms;i++)
  {
    int Diameter;
    Diameter = (Flake.BorderThickness*2 + Flake.Thickness)/Layer;
    /* compute the angle of this arm of the flake */
    Angle = ((2*PI)/Flake.Arms)*i + AngleOffset;
    /* calculate the x and y dispositions for this arm */
    y=(int)(sin(Angle)*Radius);
    x=(int)(cos(Angle)*Radius);
    /* draw the base for the arm */
    Flake.GCValues.line_width = Diameter;
    XFreeGC(Flake.DisplayVar,Flake.GCVar);
    Flake.GCVar = XCreateGC (Flake.DisplayVar, Flake.DB.b, Flake.GCFlags, &Flake.GCValues);
    XSetForeground(Flake.DisplayVar,Flake.GCVar,Flake.BordColor);
    XDrawLine(Flake.DisplayVar, Flake.DB.b, Flake.GCVar, XPos,YPos,XPos+x,YPos+y);
    
  }
  /* draw the flake one arm at a time */
  for(i=1;i<=Flake.Arms;i++)
  {
    /* compute the angle of this arm of the flake */
    Angle = ((2*PI)/Flake.Arms)*i + AngleOffset;
    /* calculate the x and y dispositions for this arm */
    y=(int)(sin(Angle)*Radius);
    x=(int)(cos(Angle)*Radius);
    /* draw the inside of the arm */
    Flake.GCValues.line_width = Flake.Thickness/Layer;
    XFreeGC(Flake.DisplayVar,Flake.GCVar);
    Flake.GCVar = XCreateGC (Flake.DisplayVar, Flake.DB.b, Flake.GCFlags, &Flake.GCValues);
    XSetForeground(Flake.DisplayVar,Flake.GCVar,Flake.ForeColor);
    XDrawLine(Flake.DisplayVar, Flake.DB.b, Flake.GCVar, XPos,YPos,XPos+x,YPos+y);
  }
}

static void
FuzzyFlakes (Display *dpy, Window window)
{
  int i,j;
  
  FuzzyFlakesMove();

  XSetForeground(Flake.DisplayVar,Flake.GCVar,Flake.BackColor);
  XFillRectangle(Flake.DisplayVar,Flake.DB.b,Flake.GCVar,0,0,Flake.XGWA.width,Flake.XGWA.height);

  for(i=Flake.Layers;i>=1;i--)
  {
     for(j=0;j<Flake.Density;j++)
     {
       FuzzyFlakesDrawFlake(
       		Flake.Flakes[i-1][j]->TrueX,
       		Flake.Flakes[i-1][j]->YPos,
		Flake.Flakes[i-1][j]->Angle,i);
     }
  }

}


char *progclass = "FuzzyFlakes";

char *defaults [] = {
  ".background:	pale green",
  ".foreground:	pink",
  ".border:	snow4",
  "*arms:	5",
  "*thickness:	10",
  "*bthickness:	3",
  "*radius:	20",
  "*layers:	3",
  "*density:	5",
  "*fallingspeed:	10",
  "*delay:	10000",
  "*doubleBuffer:	True",
  0
};

XrmOptionDescRec options [] = {
  { "-arms",		".arms",	XrmoptionSepArg, 0 },
  { "-thickness",	".thickness",	XrmoptionSepArg, 0 },
  { "-bthickness",	".bthickness",	XrmoptionSepArg, 0 },
  { "-radius",		".radius",	XrmoptionSepArg, 0 },
  { "-layers",		".layers",	XrmoptionSepArg, 0 },
  { "-density",		".density",	XrmoptionSepArg, 0 },
  { "-speed",		".fallingspeed",XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-db",		".doubleBuffer", XrmoptionNoArg,  "True" },
  { "-no-db",		".doubleBuffer", XrmoptionNoArg,  "False" },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  InitFuzzyFlakes (dpy, window);
  while (1)
    {
      FuzzyFlakes (dpy, Flake.DB.b);
      if (Flake.DB.dbuf)
        {
          XCopyArea (Flake.DisplayVar, Flake.DB.b, Flake.WindowVar, Flake.GCVar, 0, 0,
                     Flake.XGWA.width, Flake.XGWA.height, 0, 0);
          Flake.DB.b = (Flake.DB.b == Flake.DB.ba ? Flake.DB.bb : Flake.DB.ba);
        }
      screenhack_handle_events (dpy);
      XSync (dpy, False);
      if (Flake.Delay) usleep (Flake.Delay);
    }
}
