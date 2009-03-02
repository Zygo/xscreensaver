/* -*- Mode: C; tab-width: 4 -*- */
/* Xapi --- mapped X API calls */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)Xapi.c	4.07 98/04/16 xlockmore";

#endif

#ifdef WIN32

#include <stdlib.h>
#include <math.h>
#include "xlock.h"

#define NUMBER_GC		(1024)
#define NUMBER_BITMAP	(1024)

typedef struct {
	unsigned long	background;
	unsigned long	foreground;
} TColor;

typedef struct {
	int	hasFillStyle;
	int	value;
} TFillStyle;

typedef struct {
	int		hasFont;
	Font	value;
} TFont;

typedef struct {
	int	hasFunction;
	int	value;
} TFunction;

typedef struct {
	int				hasLineAttributes;
	unsigned int	width;
	int				lineStyle;
	int				capStyle;
	int				joinStyle;
} TLineAttributes;

typedef struct {
	int		hasStipple;
	Pixmap	graphic;
} TStipple;

typedef struct {
	int				isActive;
	int				needsReset;
	TColor			color;
	TFillStyle		fillStyle;
	TFunction		function;
	TLineAttributes	lineAttributes;
	TStipple		stipple;
	TFont           font;
} TGCInfo;

typedef struct {
	int		isActive;
	int		type;
	HBITMAP	bitmap;
} TBMInfo;


/* -------------------------------------------------------------------- */

int GCCreate(void);
HDC GCGetDC(Drawable d, GC gc);
int BMInfoCreate(void);
HBITMAP DrawableGetBitmap(Drawable d);

/* -------------------------------------------------------------------- */

/*-
 *  global variables
 */
HWND hwnd;                             /* window handle */
HDC hdc;                               /* device context */
HGDIOBJ noPen = NULL;                  /* no pen used in fills */
int cred, cgreen, cblue;               /* color reference of the pen */
unsigned char *red, *green, *blue;     /* holds a list of colors */
int colorcount;                        /* number of colors used */
unsigned int randommode;               /* number of mode to index */
RECT rc;                               /* coords of the screen */

HANDLE thread;                         /* thread handle */
DWORD thread_id;                       /* thread id number */

int      numGCInfo = 0;                /* number of GC Info */
TGCInfo gcInfo[NUMBER_GC + 1] = { 0 }; /* information about GCs. Allow
                                          NUMBER_GC GC's, + 1 as a spill
										  over for any modes that need
										  more */
int      numBMInfo = 0;                /* number of Bitmap Info */
TBMInfo bmInfo[NUMBER_BITMAP + 1] = { 0 }; /* information about Bitmaps/Pixmaps */

/* -------------------------------------------------------------------- */

/* the following functions are used to build up a graphic to display in
   WIN32
 */

void SetByte(LPBYTE lpBMData, BYTE val, int loc, int maxLoc)
{
	if (loc < maxLoc)
	{
		lpBMData[loc] = val ^ 0xFF;
	}
}

void SetGraphic(LPBYTE lpBMData, XImage *image)
{
  const int w32bb = 4 * 8;
  const int xbmbb = 1 * 8;
  BYTE xbmval;
  BYTE w32val;
  int lineBitCnt = 0;
  int w32BitsRemain;
  int i, j, k;
  int bmSize;
  int loc = 0;
  int actBytesPerLine;
  int maxLoc;
 
  // initialise
  actBytesPerLine = (image->width + 7) / 8;
  bmSize = actBytesPerLine * image->height;
  maxLoc = (actBytesPerLine + ((4 - (actBytesPerLine % 4)) % 4)) *
	           image->height;

  // for each byte in the xbm bit array
  for (i=0; i<bmSize; i++)
  {
    xbmval = (image->data)[i];
    w32val = 0;

    // for each bit in the byte
	for (j=0; j<8; j++)
	{
	  w32val = (w32val << 1) | ((xbmval >> j) & 0x1);

	  if (j == 7) /* we have processed all bits */
	  {
	    SetByte(lpBMData, w32val, loc, maxLoc);
		loc++;
	  }
		
	  // check for a line of graphic
	  if (++lineBitCnt == image->width)
	  {
		w32BitsRemain = (w32bb - (lineBitCnt % w32bb)) % w32bb;

		if (w32BitsRemain % 8)
		{
		  w32val = w32val << (w32BitsRemain % 8);
		  SetByte(lpBMData, w32val, loc, maxLoc);
		  loc++;
		}
		if (w32BitsRemain / 8)
		{
		  for (k=0; k<(w32BitsRemain / 8); k++)
		  {
		    SetByte(lpBMData, 0, loc, maxLoc);
			loc++;
		  }
		}
	    lineBitCnt = 0;
		break;
	  }
	}
  }
}

void
LCreateBitmap(XImage *image, HBITMAP *hBitmap, HPALETTE *hPalette)
{
  BYTE palette[2][3];
  LPBITMAPINFO lpbi;
  HDC hScreenDC;
  LPVOID lpBMData;
  LPLOGPALETTE lpLogPal;
  int i;

  /* setup color palette */
  palette[0][0] = 255;  /* red   */
  palette[0][1] = 255;  /* green */
  palette[0][2] = 255;  /* blue  */

  palette[1][0] = 000; /* red   */
  palette[1][1] = 000; /* green */
  palette[1][2] = 000; /* blue  */

  /* allocate memory for the bitmap header */
  lpbi = (LPBITMAPINFO)LocalAlloc(LPTR, 
  				sizeof(BITMAPINFOHEADER) + (2 * sizeof(RGBQUAD)));
  if (lpbi == NULL)
  {
    return; 
  }
  
  /* set bitmap header info */
  lpbi->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
  lpbi->bmiHeader.biWidth       = image->width;
  lpbi->bmiHeader.biHeight      = -1 * image->height;
  lpbi->bmiHeader.biPlanes      = 1;
  lpbi->bmiHeader.biBitCount    = 1;
  lpbi->bmiHeader.biCompression = BI_RGB;

  /* fill in bitmap palette */
  for (i=0; i<2; i++)
  {
  	lpbi->bmiColors[i].rgbRed = palette[i][0];
  	lpbi->bmiColors[i].rgbGreen = palette[i][1];
  	lpbi->bmiColors[i].rgbBlue = palette[i][2];
  }

  /* create bitmap area */
  hScreenDC = CreateCompatibleDC(hdc);
  *hBitmap = CreateDIBSection(hScreenDC, lpbi, DIB_RGB_COLORS, 
  	&lpBMData, NULL, 0);

  if (*hBitmap == NULL && lpBMData == NULL)
  {
	DeleteDC(hScreenDC);
	LocalFree(lpbi);
	return;
  }

  DeleteDC(hScreenDC);
  if (LocalFree(lpbi) != NULL)
  	return;

  /* copy XBM to bitmap */
  SetGraphic(lpBMData, image);

  /* create logical palette header */
  lpLogPal = (LPLOGPALETTE)LocalAlloc(LPTR, 
  				sizeof(LOGPALETTE) + (1 * sizeof(PALETTEENTRY)));
  if (lpLogPal == NULL)
  {
    return; 
  }

  /* set logical palette info */
  lpLogPal->palVersion = 0x0300;
  lpLogPal->palNumEntries = 2;
  for (i=0; i < 2; i++)
  {
    lpLogPal->palPalEntry[i].peRed = palette[i][0];
    lpLogPal->palPalEntry[i].peGreen = palette[i][1];
    lpLogPal->palPalEntry[i].peBlue = palette[i][2];
  }

  /* create palette */
  *hPalette = CreatePalette(lpLogPal);
  if (*hPalette == NULL)
  {
    return; 
  }

  /* clean up palette creation data */
  LocalFree(lpLogPal);
}

/* -------------------------------------------------------------------- */

/* the following functions to create, destroy, modify and access the 
   GC Info structure
 */
int GCCreate(void)
{
	int	retVal		= 0;

	for (retVal=0; retVal<NUMBER_GC; retVal++)
	{
		if (gcInfo[retVal].isActive == FALSE)
		{
			numGCInfo++;
			break;
		}
	}

	gcInfo[retVal].isActive = TRUE;
	gcInfo[retVal].needsReset = TRUE;
	gcInfo[retVal].color.background = NUMCOLORS+1;
	gcInfo[retVal].color.foreground = NUMCOLORS;
	gcInfo[retVal].fillStyle.hasFillStyle = FALSE;
	gcInfo[retVal].fillStyle.value = 0;
	gcInfo[retVal].function.hasFunction = FALSE;
	gcInfo[retVal].function.value = 0;
	gcInfo[retVal].lineAttributes.hasLineAttributes = FALSE;
	gcInfo[retVal].lineAttributes.width = 0;
	gcInfo[retVal].lineAttributes.lineStyle = 0;
	gcInfo[retVal].lineAttributes.capStyle = 0;
	gcInfo[retVal].lineAttributes.joinStyle = 0;
	gcInfo[retVal].stipple.hasStipple = FALSE;
	gcInfo[retVal].stipple.graphic = -1;

	return retVal;
}

/* -------------------------------------------------------------------- */
HDC GCGetDC(Drawable d, GC gc)
{
	HGDIOBJ			brush;
	int				fillfunc;
	int				i;
	LOGBRUSH		lb;
	LPLOGPALETTE	lpLogPal;
	BYTE			palarray[2][3];
	HPALETTE		palette;
	HGDIOBJ 		pen;
	DWORD 			penstyle;
	HBITMAP			stipple;
	HDC				lhdc;
	char			message[80];

	static GC	lastGc = -1;
	static HDC	sbhdc = NULL;
	static HDC	dbhdc = NULL;

	if (gc < 0 || gc >= numGCInfo)
	{
		sprintf(message, "GC passed in not in range 0..%d, gc=%d",
			numGCInfo-1, gc);
		xlockmore_set_debug(message);
		return NULL;
	}

	/* get DC - try and determine if its a window or bitmap */
	if (d > 0 && d < NUMBER_BITMAP)
	{
		if (dbhdc == NULL)
			/* create a compatible DC from the saved intial DC. To draw
			   to this DC we still need to put a HBITMAP into it (not 
			   done in this function)
			 */
			if ((dbhdc = CreateCompatibleDC(hdc)) == NULL)
			{
				sprintf(message, "CreateCompatibleDC() returned NULL");
				xlockmore_set_debug(message);
				return NULL;
			}
		lhdc = dbhdc;
	}
	else
	{
		if (sbhdc == NULL)
			/* create a DC for the window. We don't (and cannot) put
			 * a HBITMAP into this. It is ready to draw to (straight to
			 * the screen)
			 */
			if ((sbhdc = GetDC(hwnd)) == NULL)
			{
				sprintf(message, "GetDC() returned NULL");
				xlockmore_set_debug(message);
				return NULL;
			}
		lhdc = sbhdc;
	}

	/* check if we have already set up the DC in a previous call */
	if (lastGc == gc && gcInfo[gc].needsReset == FALSE)
		return lhdc;

	/* set the current color constants */
	cred = red[gcInfo[gc].color.foreground];
	cgreen = green[gcInfo[gc].color.foreground];
	cblue = blue[gcInfo[gc].color.foreground];

	/* set the text color */
    if (SetTextColor(lhdc, RGB(cred,cgreen,cblue)) == CLR_INVALID)
	{
		sprintf(message, "text: SetTextColor() returned CLR_INVALID [%d %d %d]", cred, cgreen, cblue);
		xlockmore_set_debug(message);
		return NULL;
	}
    if (SetBkColor(lhdc, RGB(red[gcInfo[gc].color.background],
						green[gcInfo[gc].color.background],
						blue[gcInfo[gc].color.background])) == CLR_INVALID)
	{
		sprintf(message, "text: SetBkColor() returned CLR_INVALID");
		xlockmore_set_debug(message);
		return NULL;
	}

	/* set the pen to the new color and line attributes */
	if (gcInfo[gc].lineAttributes.hasLineAttributes == FALSE)
	{
		/* no line attributes, just create solid pen and put into the DC*/
		if ((pen = CreatePen(PS_SOLID, 0, RGB(cred, cgreen, cblue))) == NULL)
		{
			sprintf(message, "pen: CreatePen() returned NULL");
			xlockmore_set_debug(message);
			return NULL;
		}
		else
		{
			if ((pen = SelectObject(lhdc, pen)) == NULL)
			{
				sprintf(message, "pen: Failed to select pen into DC");
				xlockmore_set_debug(message);
				return NULL;
			}
			/* remove the old pen */
			if (DeleteObject(pen) ==  0)
			{
				sprintf(message, "pen: Failed to delete old pen");
				xlockmore_set_debug(message);
				return NULL;
			}
		}
	}
	else
	{
		/* line attributes need to be set, create a more complex pen */

		/* set the brush info */
		lb.lbStyle = BS_SOLID;
		lb.lbColor = RGB(cred, cgreen, cblue);
		lb.lbHatch = 0;

		/* set the pen style */
		penstyle = PS_GEOMETRIC;

		switch (gcInfo[gc].lineAttributes.lineStyle)
		{
			case LineSolid: 
				penstyle |= PS_SOLID; break;
			case LineOnOffDash:
				penstyle |= PS_DASH; break;
			case LineDoubleDash:
				penstyle |= PS_DOT; break;
		}

		switch (gcInfo[gc].lineAttributes.capStyle)
		{
			case CapNotLast:
				penstyle |= PS_ENDCAP_FLAT; break;
			case CapButt:
				penstyle |= PS_ENDCAP_FLAT; break;
			case CapRound:
				penstyle |= PS_ENDCAP_ROUND; break;
			case CapProjecting:
				penstyle |= PS_ENDCAP_SQUARE; break;
		}

		switch (gcInfo[gc].lineAttributes.joinStyle)
		{
			case JoinMiter:
				penstyle |= PS_JOIN_MITER; break;
			case JoinRound:
				penstyle |= PS_JOIN_ROUND; break;
			case JoinBevel:
				penstyle |= PS_JOIN_BEVEL; break;
		}

		/* create the pen, putting it in the DC */
		pen = SelectObject(lhdc,
			ExtCreatePen(penstyle, gcInfo[gc].lineAttributes.width,
			&lb, 0, NULL));
		/* remove the old pen */
		DeleteObject(pen);
	}

	/* set the palette to the new colors */
    palarray[0][0] = cred;   /* red   */
    palarray[0][1] = cgreen; /* green */
    palarray[0][2] = cblue;  /* blue  */

    palarray[1][0] = red[gcInfo[gc].color.background];   /* red   */
    palarray[1][1] = green[gcInfo[gc].color.background]; /* green */
    palarray[1][2] = blue[gcInfo[gc].color.background];  /* blue  */

    /* create logical palette header */
    lpLogPal = (LPLOGPALETTE)LocalAlloc(LPTR, 
				sizeof(LOGPALETTE) + (1 * sizeof(PALETTEENTRY)));
    if (lpLogPal == NULL)
    {
		sprintf(message, "palette: Cannot alloacte memory for logical palette");
		xlockmore_set_debug(message);
		return NULL; 
    }

    /* set logical palette info */
    lpLogPal->palVersion = 0x0300;
    lpLogPal->palNumEntries = 2;
    for (i=0; i < 2; i++)
    {
		lpLogPal->palPalEntry[i].peRed = palarray[i][0];
		lpLogPal->palPalEntry[i].peGreen = palarray[i][1];
		lpLogPal->palPalEntry[i].peBlue = palarray[i][2];
    }

    /* create palette */
    palette = CreatePalette(lpLogPal);
    if (palette == NULL)
    {
		sprintf(message, "palette: CreatePalette() returned NULL");
		xlockmore_set_debug(message);
		LocalFree(lpLogPal);
		return NULL; 
    }

    /* set palette color */
	palette = SelectPalette(lhdc, palette, FALSE); 

    /* clean up palette creation data */
	DeleteObject(palette);
	LocalFree(lpLogPal);

    /* check if we need a pattern brush or solid brush */
	if (gcInfo[gc].stipple.hasStipple == FALSE)
	{
		/* solid brush */
		brush = SelectObject(lhdc,
	        CreateSolidBrush(RGB(cred, cgreen, cblue)));
	    DeleteObject(brush);
	}
	else if (gcInfo[gc].stipple.graphic != -1)
	{
		/* pattern brush */
  		HBRUSH oldBrush;
  		HBRUSH brush;
  		HDC hStippleDC;
  		HBITMAP hOldBitmap;
  		RGBQUAD rgbQuad = { cblue, cgreen, cred, 0 };

		if ((stipple = DrawableGetBitmap(gcInfo[gc].stipple.graphic)) == NULL)
			return NULL;

  		/* create a device context for setting the stipples color */
  		if ((hStippleDC = CreateCompatibleDC(lhdc)) == NULL)
		{
			sprintf(message, "pattern brush: CreateCompatibleDC() returned NULL");
			xlockmore_set_debug(message);
			return NULL;
		}

		/* put the stipple into the DC */
		if ((hOldBitmap = (HBITMAP)SelectObject(hStippleDC, stipple)) == NULL)
		{
			sprintf(message, "pattern brush: cannot select stipple in stippleDC");
			xlockmore_set_debug(message);
			DeleteDC(hStippleDC);
			return NULL;
		}

		/* set the correct color table */
		SetDIBColorTable(hStippleDC, 0, 1, &rgbQuad);

		/* get the stipple back from the DC with the correct color table */
		if ((stipple = (HBITMAP)SelectObject(hStippleDC, hOldBitmap)) == NULL)
		{
			sprintf(message, "pattern brush: cannot select stipple out of stippleDC");
			xlockmore_set_debug(message);
			DeleteDC(hStippleDC);
			return NULL;
		}

		/* create a pattern brush out of it */
		if ((brush = CreatePatternBrush(stipple)) == NULL)
		{
			sprintf(message, "pattern brush: CreatePatternBrush() returned NULL");
			xlockmore_set_debug(message);
			DeleteDC(hStippleDC);
			return NULL;
		}

		/* put the pattern brush into the DC */
		if ((oldBrush = SelectObject(lhdc, brush)) == NULL)
		{
			sprintf(message, "pattern brush: cannot select brush into DC");
			xlockmore_set_debug(message);
			DeleteDC(hStippleDC);
			return NULL;
		}

		/* clean up and return */
		DeleteDC(hStippleDC);
		DeleteObject(oldBrush);
	}

	/* check if we need to set the fill function */
	if (gcInfo[gc].function.hasFunction == TRUE)
	{
		switch (gcInfo[gc].function.value)
		{
			case GXclear:        fillfunc = R2_BLACK; break;
			case GXand:          fillfunc = R2_MERGEPEN; break;
			case GXandReverse:   fillfunc = R2_MERGEPENNOT; break;
			case GXcopy:         fillfunc = R2_COPYPEN; break;
			case GXandInverted:  fillfunc = R2_MERGENOTPEN; break;
			case GXnoop:         fillfunc = R2_NOP; break;
			case GXxor:          fillfunc = R2_XORPEN; break;
			case GXor:           fillfunc = R2_MASKPEN; break;
			case GXnor:          fillfunc = R2_NOTMERGEPEN; break;
			case GXequiv:        fillfunc = R2_NOTXORPEN; break;
			case GXinvert:       fillfunc = R2_NOT; break;
			case GXorReverse:    fillfunc = R2_MASKPENNOT; break;
			case GXcopyInverted: fillfunc = R2_NOTCOPYPEN; break;
			case GXorInverted:   fillfunc = R2_MASKNOTPEN; break;
			case GXnand:         fillfunc = R2_NOTMASKPEN; break;
			case GXset:          fillfunc = R2_WHITE; break;
			default:             fillfunc = R2_COPYPEN; break;
  		}

  		SetROP2(lhdc, fillfunc);
	}

	/* 
	 * if we call this function again and there is no change to the DC
	 * that needs to be done, we want to just return the DC that had
	 * already been set up. Set the information to allow this
	 */
	gcInfo[gc].needsReset = FALSE;
	lastGc = gc;

	/* ignoring setting font for now */
	/* TODO: font */

	/* return the DC */
	return lhdc;
}


/* the following functions to create, destroy, modify and access the
   BM Info structure
 */
int BMInfoCreate(void)
{
	int retVal	= 0;
	HBITMAP hBitmap;

	/* start at 1, not 0 as this will be None */
	for (retVal=1; retVal<NUMBER_BITMAP; retVal++)
	{
		if (bmInfo[retVal].isActive == FALSE)
		{
			numBMInfo++;
			break;
		}
	}

	if (retVal == NUMBER_BITMAP)
		return -1;

	/* set the bitmap info */
	bmInfo[retVal].isActive = TRUE;
	bmInfo[retVal].bitmap = NULL;

	return retVal;
}

HBITMAP DrawableGetBitmap(Drawable d)
{
	char message[80];

	if (d < 0 || d >= NUMBER_BITMAP)
	{
		sprintf(message, "Drawable passed in not in range 0..%d, d=%d",
			numBMInfo-1, d);
		xlockmore_set_debug(message);
		return NULL;
	}

	/* check for id of 0 */
	if (d == None)
	{
		sprintf(message, "Drawable is None");
		xlockmore_set_debug(message);
		return NULL;
	}

	if (bmInfo[d].isActive == FALSE)
	{
		sprintf(message, "Drawable not active, d=%d", d);
		xlockmore_set_debug(message);
		return NULL;
	}

	return bmInfo[d].bitmap;
}

/* -------------------------------------------------------------------- */

/* the following are functions in UNIX but not WIN32 */

/*-
 *  nice
 */
int nice(int level)
{
	return 1;
}

/*-
 *  sleep
 */
void sleep(int sec)
{
}

/*-
 *  sigmask
 */
int sigmask(int signum)
{
	return 0;
}


/* the following are macros in X, but I have implemented as functions */

/*-
 *  BlackPixel
 *    returns the color cell of Black
 *    NOTE: this is currently set to the end of the color
 *    palette table, 
 */
unsigned long BlackPixel(Display *display, int screen_number)
{
	return colorcount+1;
}

/*-
 *  WhitePixel
 *    returns the color cell of White
 *    NOTE: this is currently set to the end of the color
 *    palette table - 1
 */
unsigned long WhitePixel(Display *display, int screen_number)
{
	return colorcount;
}

/*-
 *  BlackPixelOfScreen
 *    returns the color cell of Black
 *    NOTE: currently is fixed to color array + 2
 */
int BlackPixelOfScreen(Screen *screen)
{
	return NUMCOLORS+1;
}

/*-
 *  WhitePixelOfScreen
 *    returns the color cell of White
 *    NOTE: currently is fixed to color array + 1
 */
int WhitePixelOfScreen(Screen *screen)
{
	return NUMCOLORS;
}

/*-
 *  CellsOfScreen
 *    returns number of entries the colormap can hold
 */
int CellsOfScreen(Screen *screen)
{
	return colorcount;
}

/*-
 *  DefaultColormap
 *    not currently used, returns 0
 */
Colormap DefaultColormap(Display *display, int screen_number)
{
	return 0;
}

/*-
 *  DefaultColormapOfScreen
 *    not currently used, returns 0
 */
Colormap DefaultColormapOfScreen(Screen *screen)
{
	return 0;
}

/*-
 *  DefaultVisual
 *    not currently used, returns NULL
 */
Visual *DefaultVisual(Display *display, int screen_number)
{
	return NULL;
}

/*-
 *  DisplayPlanes
 *    not currently used, returns 0
 */
int DisplayPlanes(Display *display, int screen_number)
{
	return 0;
}

/*-
 *  DisplayString
 *    not currently used, returns NULL
 */
char *DisplayString(Display *display)
{
	return NULL;
}

/*-
 *  RootWindow
 *    not currently used, returns 0
 */
Window RootWindow(Display *display, int screen_number)
{
	return 0;
}

/*-
 *  ScreenCount
 *    not correct, just return 1
 */
int ScreenCount(Display *display)
{
	return 1;
}

/*-
 *  ScreenOfDisplay
 *    not currently used, returns NULL
 */
Screen *ScreenOfDisplay(Display *display, int screen_number)
{
	return NULL;
}

/* -------------------------------------------------------------------- */

/* X function mappings */

/*-
 *  XAddHosts
 *    not currently used
 */
void XAddHosts(Display *display, XHostAddress *hosts, int num_hosts)
{
}

/*-
 *  XAllocColor
 *    not currently used
 */
Status XAllocColor(Display *display, Colormap colormap, 
				   XColor *screen_in_out)
{
	screen_in_out->pixel = 0;
	return 1;
}

/*-
 *  XAllocColorCells
 *    not currently used
 */
Status XAllocColorCells(Display *display, Colormap colormap,
					    Bool contig, unsigned longplane_masks_return[],
						unsigned int nplanes, unsigned long pixels_return[],
						unsigned int npixels)
{
	return 0;
}

/*-
 *  XAllocNamedColor
 *    not currently used
 */
Status XAllocNamedColor(Display *display, Colormap colormap,
						char *color_name, XColor *screen_def_return,
						XColor *exact_def_return)
{
	return 0;
}

/*-
 *  XBell
 *    not currently used
 */
void XBell(Display *display, int percent)
{
}

/*-
 *  XChangeGC
 *    changes values of the graphics context
 */
void XChangeGC(Display *display, GC gc, unsigned long valuemask,
			   XGCValues *values)
{
	if (valuemask & GCBackground)
	{
		XSetBackground(display, gc, values->background);
	}
	if (valuemask & GCFillStyle)
	{
		XSetFillStyle(display, gc, values->fill_style);
	}
	if (valuemask & GCForeground)
	{
		XSetForeground(display, gc, values->foreground);
	}
	if (valuemask & GCFunction)
	{
		XSetFunction(display, gc, values->function);
	}
	if (valuemask & GCLineWidth)
	{
		XSetLineAttributes(display, gc, 
			values->line_width, 
			gcInfo[gc].lineAttributes.lineStyle,
			gcInfo[gc].lineAttributes.capStyle,
			gcInfo[gc].lineAttributes.joinStyle);
	}
	if (valuemask & GCLineStyle)
	{
		XSetLineAttributes(display, gc, 
			gcInfo[gc].lineAttributes.width,
			values->line_style,
			gcInfo[gc].lineAttributes.capStyle,
			gcInfo[gc].lineAttributes.joinStyle);
	}
	if (valuemask & GCCapStyle)
	{
		XSetLineAttributes(display, gc, 
			gcInfo[gc].lineAttributes.width,
			gcInfo[gc].lineAttributes.lineStyle,
			values->cap_style,
			gcInfo[gc].lineAttributes.joinStyle);
	}
	if (valuemask & GCJoinStyle)
	{
		XSetLineAttributes(display, gc, 
			gcInfo[gc].lineAttributes.width,
			gcInfo[gc].lineAttributes.lineStyle,
			gcInfo[gc].lineAttributes.capStyle,
			values->join_style);
	}
	if (valuemask & GCStipple)
	{
		XSetStipple(display, gc, values->stipple);
	}
	if (valuemask & GCFont)
	{
		XSetFont(display, gc, values->font);
	}
	
	/* the above are the only attributes we can set at this stage */
}


/*-
 *  XCheckMaskEvent
 *    not currently used, return True
 */
Bool XCheckMaskEvent(Display *display, long event_mask, 
					 XEvent *event_return)
{
	return True;
}

/*-
 *  XClearArea
 *    clears a rectangular area in a window/screen
 */
void XClearArea(Display *display, Window w, int x, int y,
				unsigned int width, unsigned int height, Bool exposures)
{
	RECT lrc;

	lrc.left = x;
	lrc.top = y;
	lrc.right = x + width;
	lrc.bottom = y + height;

	FillRect(hdc, &lrc, (struct HBRUSH__ *)GetStockObject(BLACK_BRUSH));
}

/*-
 *  XClearWindow
 *    sets the base window/screen to black
 */
void XClearWindow(Display *display, Window w)
{
	FillRect(hdc, &rc, (struct HBRUSH__ *)GetStockObject(BLACK_BRUSH));
}

/*-
 *  XCloseDisplay
 *    not currently used
 */
void XCloseDisplay(Display *display)
{
}

/*-
 *  XConfigureWindow
 *    not currently used
 */
void XConfigureWindow(Display *display, Window w, 
					  unsigned int value_mask, 
					  XWindowChanges *values)
{
}

/*-
 *  XCopyArea
 *    copies one drawable to another
 */
int XCopyArea(Display *display, Drawable src, Drawable dest, GC gc, 
              int src_x, int src_y, unsigned int width, unsigned height,
			  int dest_x, int dest_y)
{
	HBITMAP	hBitmap;
	HBITMAP	hOldBitmap;
	HDC		hDestDC;
	HDC		hSrcDC;

	if ((hDestDC = GCGetDC(dest, gc)) == NULL)
	{
		xlockmore_set_debug("XCopyArea: Failed to get dc");
		return BadGC;
	}

	/* create the source resource */
	if ((hBitmap = DrawableGetBitmap(src)) == NULL)
	{
		xlockmore_set_debug("XCopyArea: Failed to get bitmap");
		return BadDrawable;
	}

	if ((hSrcDC = CreateCompatibleDC(hdc)) == NULL)
	{
		xlockmore_set_debug("XCopyArea: Failed to create compatible DC");
		return BadDrawable;
	}

	hOldBitmap = SelectObject(hSrcDC, hBitmap);

	/* copy src to destination */
	if (BitBlt(hDestDC, dest_x, dest_y, width, height, hSrcDC, 
		src_x, src_y, SRCCOPY) == 0)
	{
		xlockmore_set_debug("XCopyArea: BitBlt() failed");
	}

	/* clean up */
	if (hOldBitmap)
		SelectObject(hSrcDC, hOldBitmap);
	DeleteDC(hSrcDC);
	return Success;
}

/*-
 *  XCopyColormapAndFree
 *    not currently used, returns 0
 */
Colormap XCopyColormapAndFree(Display *display, Colormap colormap)
{
	return 0;
}

/*-
 *  XCreateBitmapFromData
 *    used to create a bitmap
 */
Pixmap XCreateBitmapFromData(Display *display, Drawable drawable, 
                             char *data, unsigned int width,
                             unsigned int height)
{
	XImage image;
	HBITMAP hBitmap;
	HPALETTE hPalette;
	Pixmap pm;

	pm = BMInfoCreate();
	if (pm < 0 || pm >= NUMBER_BITMAP)
		return None;

	image.width = width;
	image.height = height;
	image.data = data;

	LCreateBitmap(&image, &hBitmap, &hPalette);
	DeleteObject(hPalette); /* we don't need the palette */
	
	if (hBitmap == NULL)
	{
		bmInfo[pm].isActive = FALSE;
		return None;
	}

	bmInfo[pm].bitmap = hBitmap;
	bmInfo[pm].type = 1;
	return pm;
}

/*-
 *  XCreateColormap
 *    used to create the colormap
 */
Colormap XCreateColormap(Display *display, Window w, 
						 Visual *visual, int alloc)
{
	return None;
}

/*-
 *  XCreateFontCursor
 *    not currently used, returns 0
 */
Cursor XCreateFontCursor(Display *display, unsigned int shape)
{
	return 0;
}

/*-
 *  XCreateGC
 *    creates a copy of the GC
 */
GC XCreateGC(Display *display, Drawable drawable, 
			 unsigned long valuemask, XGCValues *values)
{
	GC		newGc;
	char	message[80];

	if ((newGc = GCCreate()) == -1)
	{
		sprintf(message, "Failed to create a GC");
		xlockmore_set_debug(message);
	}
	else
	{
		XChangeGC(display, newGc, valuemask, values);
	}

	return newGc;
}

/*-
 *  XCreateImage
 *    creates an XImage structure and populates it
 */
XImage *XCreateImage(Display *display, Visual *visual, 
					 unsigned int depth, int format, int offset,
					 char *data, unsigned int width,
					 unsigned int height, int bitmap_pad,
					 int bytes_per_line)
{
	XImage *newImage;

	if ((newImage = malloc(sizeof(XImage))) == NULL)
		return NULL;
	
	memset(newImage, 0, sizeof(XImage));

	newImage->width = width;
	newImage->height = height;
	newImage->xoffset = offset;
	newImage->format = format;
	newImage->data = data;
	newImage->bitmap_pad = bitmap_pad;
	newImage->depth = depth;
	newImage->bytes_per_line = (bytes_per_line != 0) ? bytes_per_line : (width + 7) / 8;

	return newImage;
}

/*-
 *  XCreatePixmap
 *    creates a pixmap
 */
Pixmap XCreatePixmap(Display *display, Drawable d, unsigned int width,
              unsigned int height, unsigned int depth)
{
	Pixmap pm;
	HBITMAP hBitmap;

	pm = BMInfoCreate();
	if (pm < 0 || pm >= NUMBER_BITMAP)
		return None;

	/* create the bitmap */
	if ((hBitmap = CreateCompatibleBitmap(hdc, width, height)) == NULL)
	{
		bmInfo[pm].isActive = FALSE;
 		return None;
	}

	bmInfo[pm].bitmap = hBitmap;
	bmInfo[pm].type = 0;

	return pm;
}

/*-
 *  XCreatePixmapCursor
 *    not currently used, returns 0
 */
Cursor XCreatePixmapCursor(Display *display,
	Pixmap source, Pixmap mask,
	XColor *foreground_color, XColor *background_color,
	unsigned int x_hot, unsigned int y_hot)
{
	return None;
}

/*-
 *  XCreatePixmapFromBitmapData
 *    not currently used, returns 0
 */
Pixmap XCreatePixmapFromBitmapData(Display *display, Drawable drawable,
	    char *data, unsigned int width, unsigned int height,
		unsigned long fg, unsigned long bg, unsigned int depth)
{
	Pixmap pm;

	if (depth == 1)
		if ((pm = XCreateBitmapFromData(display, drawable, data, width, height)) != None)
			return pm;
	
	return None;
}

/*
 *  XDefineCursor
 *    not currently used
 */
void XDefineCursor(Display *display, Window window, Cursor cursor)
{
}

/*-
 *  XDestroyImage
 *    not currently used
 */
void XDestroyImage(XImage *ximage)
{
}

/*-
 *  XDisableAccessControl
 *    not currently used
 */
void XDisableAccessControl(Display *display)
{
}

/*-
 *  XDrawArc
 *    draws an arc
 */
void XDrawArc(Display *display, Drawable d, GC gc, int x, int y,
			  unsigned int width, unsigned int height,
			  int angle1, int angle2)
{
	int xa1, ya1;
	int xa2, ya2;
	int cx, cy;
	double a1, a2;
	double rx, ry;
	double ang_inc = M_PI * 2.0 / 23040.0;
	HDC dc;
	HBITMAP hOldBitmap = NULL;

	rx = width/2.0;
	ry = height/2.0;
	cx = x + width/2;
	cy = y + height/2;

	a1 = (23040 - (angle1 % 23040)) * ang_inc;

	xa1 = (int)((rx * cos(a1)) + cx);
	ya1 = (int)((ry * sin(a1)) + cy);

	if (angle2 == 0) {
		/* Special case - In X this is a point but Arc in Windows would
		   do a complete elipse!  Draw a (thick) line instead.  */
	  
		XDrawLine(display, d, gc, xa1, ya1, xa1, ya1);

	} else {

		a2 = (23040 - ((angle1 + angle2) % 23040)) * ang_inc;

		xa2 = (int)((rx * cos(a2)) + cx);
		ya2 = (int)((ry * sin(a2)) + cy);

		if ((dc = GCGetDC(d, gc)) == NULL)
			return;

		if (d > 0 && d < NUMBER_BITMAP)
			if (bmInfo[d].isActive == TRUE)
				hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

		Arc(dc, x, y, x + width, y + height, xa1, ya1, xa2, ya2);

		if (hOldBitmap)
			SelectObject(dc, hOldBitmap);
	}
}

/*-
 *  XDrawImageString
 *    not currently used
 */
void XDrawImageString(Display *display, Drawable d, GC gc, 
					  int x, int y, char *string, int length)
{
}

/*-
 *  XDrawLine
 *    draws a line on the screen in the current pen color
 */
void XDrawLine(Display *display, Drawable d, GC gc, 
			   int x1, int y1, int x2, int y2)
{
	HDC dc;
	HBITMAP hOldBitmap = NULL;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	MoveToEx(dc, x1, y1, NULL);
	LineTo(dc, x2, y2);

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XDrawLines
 *    draws a series of lines on the screen. Liness may be
 *    specified as being (1) relative to the origin, or 
 *    (2) relative to the previous line (with the first line 
 *    relative to the origin).
 */
void XDrawLines(Display *display, Drawable d, GC gc,
				XPoint *points, int npoints, int mode)
{
	int i;
	int tx, ty;
	HDC dc;
	HBITMAP hOldBitmap = NULL;
	POINT *lppt;
	BYTE *lpbTypes;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	if (npoints > 1) {
	  lppt = (POINT*)malloc(npoints * sizeof(POINT));
	  lpbTypes = (BYTE*)malloc(npoints * sizeof(BYTE));

	  lpbTypes[0] = PT_MOVETO;
	  lppt[0].x = points[0].x;
	  lppt[0].y = points[0].y;

	  if (mode == CoordModeOrigin) {
		for (i=1; i<npoints; i++) {
		  lpbTypes[i] = PT_LINETO;
		  lppt[i].x = points[i].x;
		  lppt[i].y = points[i].y;
		}
	  } else if (mode == CoordModePrevious) {
		tx = points[0].x;
		ty = points[0].y;
		for (i=1; i<npoints; i++) {
		  tx += points[i].x;
		  ty += points[i].y;
		  lpbTypes[i] = PT_LINETO;
		  lppt[i].x = tx;
		  lppt[i].y = ty;
		}
	  }
	  PolyDraw(dc, lppt, lpbTypes, npoints);
	  free(lppt);
	  free(lpbTypes);
	}

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XDrawPoint
 *    draws a point on the screen
 */
void XDrawPoint(Display *display, Drawable d, GC gc, int x, int y)
{
	HDC dc;
	HBITMAP hOldBitmap = NULL;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	SetPixelV(dc, x, y, RGB(cred, cgreen, cblue));

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XDrawPoints
 *    draws a series of points on the screen. Points may be
 *    specified as being (1) relative to the origin, or 
 *    (2) relative to the previous point (with the first point 
 *    relative to the origin).
 */
void XDrawPoints(Display *display, Drawable d, GC gc, 
				 XPoint *pts, int numpts, int mode)
{
	int i;
	int tx, ty;
	HDC dc;
	HBITMAP hOldBitmap = NULL;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	if (mode == CoordModeOrigin)
	{
		for (i=0; i<numpts; i++)
		{
			SetPixelV(dc, pts[i].x, pts[i].y, RGB(cred, cgreen, cblue));
		}
	}
	else if (mode == CoordModePrevious)
	{
		if (numpts > 0)
		{
			tx = pts[0].x;
			ty = pts[0].y;
			SetPixelV(dc, tx, ty, RGB(cred, cgreen, cblue));
			for (i=1; i<numpts; i++)
			{
				tx += pts[i].x;
				ty += pts[i].y;
				SetPixelV(dc, tx, ty, RGB(cred, cgreen, cblue));
			}
		}
	}

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XDrawRectangle
 *    Draws an outline of a rectangle.
 *    Uses PolyDraw as Rectangle fills rectangle with brush
 */
void XDrawRectangle(Display *display, Drawable d, GC gc, int x, int y,
                    unsigned int width, unsigned int height)
{
	BYTE lpbTypes[6];
	POINT lppt[6];
	HDC dc;
	HBITMAP hOldBitmap = NULL;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	lpbTypes[0] = PT_MOVETO;
	lppt[0].x = x, lppt[0].y = y;
	lpbTypes[1] = PT_LINETO;
	lppt[1].x = x + width, lppt[1].y = y;
	lpbTypes[2] = PT_LINETO;
	lppt[2].x = x + width, lppt[2].y = y + height;
	lpbTypes[3] = PT_LINETO;
	lppt[3].x = x, lppt[3].y = y + height;
	lpbTypes[4] = PT_LINETO;
	lppt[4].x = x, lppt[4].y = y;
	/* overkill but need better closure */
	lpbTypes[5] = PT_LINETO;
	lppt[5].x = x + width, lppt[5].y = y;
	PolyDraw(dc, lppt, lpbTypes, 6);

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XDrawSegments
 *    draws multiple disjoint lines. Lines do not have to be
 *    connected as each segment is one line.
 */
void XDrawSegments(Display *display, Drawable d, GC gc,
				 XSegment *segs, int numsegs)
{
	int i;
	HDC dc;
	HBITMAP hOldBitmap = NULL;
	POINT *lppt;
	BYTE *lpbTypes;
	int cCount = 0;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	lppt = (POINT*)malloc(numsegs * 2 * sizeof(POINT));
	lpbTypes = (BYTE*)malloc(numsegs * 2 * sizeof(BYTE));

	for (i=0; i<numsegs; i++) {
	  	lpbTypes[cCount] = PT_MOVETO;
	  	lppt[cCount].x = segs[i].x1;
	  	lppt[cCount].y = segs[i].y1;
	  	cCount++;
	  	lpbTypes[cCount] = PT_LINETO;
	  	lppt[cCount].x = segs[i].x2;
	  	lppt[cCount].y = segs[i].y2;
	  	cCount++;		  
	}

	PolyDraw(dc, lppt, lpbTypes, cCount);

	free(lppt);
	free(lpbTypes);

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XDrawString
 *    Draws a string of characters at specified location
 */
void XDrawString(Display *display, Drawable d, GC gc, int x, int y, 
				 char *string, int length)
{
	int prevMode;
	HDC dc;
	HBITMAP hOldBitmap = NULL;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	prevMode = GetBkMode(dc);
	SetBkMode(dc, TRANSPARENT);
	TextOut(dc, x + 3, y - 15, string, length);
	SetBkMode(dc, prevMode);

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XEnableAccessControl
 *    not currently used
 */
void XEnableAccessControl(Display *display)
{
}

/*-
 *  XFillArc
 *    draws a filled arc on screen
 */
void XFillArc(Display *display, Drawable d, GC gc, int x, int y,
			  unsigned int width, unsigned int height,
			  int angle1, int angle2)
{
	int xa1, ya1;
	int xa2, ya2;
	int cx, cy;
	double a1, a2;
	double rx, ry;
	double ang_inc = M_PI * 2.0 / 23040.0;
    HPEN oldPen;
	HDC dc;
	HBITMAP hOldBitmap = NULL;

    /* set the size of the arc a little larger because
	   we won't be drawing a border around it */
	width++;
	height++;

	rx = width/2.0;
	ry = height/2.0;
	cx = x + width/2;
	cy = y + height/2;

	a1 = (23040 - (angle1 % 23040)) * ang_inc;

	xa1 = (int)((rx * cos(a1)) + cx);
	ya1 = (int)((ry * sin(a1)) + cy);

	if (angle2 == 0) {
	  /* Special case - In X this is a point but Arc in Windows would
		 do a complete elipse!  Draw a (thick) line instead.  */
	  
	  XDrawLine(display, d, gc, cx, cy, xa1, ya1);

	} else {
	  if ((dc = GCGetDC(d, gc)) == NULL)
		  return;

	  if (d > 0 && d < NUMBER_BITMAP)
		  if (bmInfo[d].isActive == TRUE)
			  hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	  a2 = (23040 - ((angle1 + angle2) % 23040)) * ang_inc;

	  xa2 = (int)((rx * cos(a2)) + cx);
	  ya2 = (int)((ry * sin(a2)) + cy);

	  /* set the pen draw width to 1, and because its null it won't
		 use it, hence the reason we made the arc larger before */
	  if (noPen == NULL)
		if ((noPen = CreatePen(PS_NULL, 0, RGB(0,0,0))) == NULL)
		{
			xlockmore_set_debug("Failed to create pen");
			return;
		}
	  oldPen = SelectObject(dc, noPen);

	  /* should check for a pie or chord filled arc type, but 
		 currently assuming pie filled arc */
	  Pie(dc, x, y, x + width, y + height, xa1, ya1, xa2, ya2);

	  /* reset back and clean up */
	  SelectObject(dc, oldPen);
	  if (hOldBitmap)
		  SelectObject(dc, hOldBitmap);
	}
}

/*-
 *  XFillArcs
 *    draws a number of filled arcs on screen
 */
void XFillArcs(Display *display, Drawable d, GC gc,
			   XArc *arcs, int narcs)
{
	int i;
	int xa1, ya1;
	int xa2, ya2;
	int cx, cy;
	double a1, a2;
	double rx, ry;
	const double ang_inc = M_PI * 2.0 / 23040.0;
	HDC dc;
	HBITMAP hOldBitmap = NULL;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	for (i=0; i<narcs; i++) {
		rx = arcs[i].width/2.0;
		ry = arcs[i].height/2.0;
		cx = arcs[i].x + arcs[i].width/2;
		cy = arcs[i].y + arcs[i].height/2;

		a1 = (23040 - (arcs[i].angle1 % 23040)) * ang_inc;

		xa1 = (int)((rx * cos(a1)) + cx);
		ya1 = (int)((ry * sin(a1)) + cy);

		if (arcs[i].angle2 == 0) {
		  /* Special case - In X this is a point but Arc in Windows would
			 do a complete elipse!  Draw a (thick) line instead.  */
		  
		  XDrawLine(display, d, gc, cx, cy, xa1, ya1);

		} else {
		  
		  if (d > 0 && d < NUMBER_BITMAP)
			  if (bmInfo[d].isActive == TRUE)
				  hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

		  a2 = (23040 - ((arcs[i].angle1 + arcs[i].angle2) % 23040)) * ang_inc;
		  xa2 = (int)((rx * cos(a2)) + cx);
		  ya2 = (int)((ry * sin(a2)) + cy);

		  /* should check for a pie or chord filled arc type, but 
			 currently assuming pie filled arc */
		  Pie(dc, arcs[i].x, arcs[i].y, arcs[i].x + arcs[i].width, 
			  arcs[i].y + arcs[i].height, xa1, ya1, xa2, ya2);

		  if (hOldBitmap)
			  SelectObject(dc, hOldBitmap);
		}
	}
}


/*-
 *  XFillPolygon
 *    draws a filled polygon on screen. Points can be specified as
 *    being relative to the origin, or relative to the previous
 *    point (with the first point relative to the origin)
 */
void XFillPolygon(Display *display, Drawable d, GC gc, XPoint *points,
				  int npoints, int shape, int mode)
{
	int i;
	int tx, ty;
	POINT *pts;
	HDC dc;
	HBITMAP hOldBitmap = NULL;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (npoints <= 0)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	/* allocate memory */
	pts = calloc(npoints, sizeof(POINT));

	if (mode == CoordModeOrigin) {
		for (i=0; i<npoints; i++) {
			pts[i].x = points[i].x;
			pts[i].y = points[i].y;
		}
	}
	else  if (mode == CoordModePrevious) {
		tx = pts[0].x = points[0].x;
		ty = pts[0].y = points[0].y;
		for (i=1; i<npoints; i++){
			tx += points[i].x;
			ty += points[i].y;
			pts[i].x = tx;
			pts[i].y = ty;
		}
	}
	
	Polygon(dc, pts, npoints);

	/* free memory */
	free(pts);

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XFillRectangle
 *    draws a filled rectangle on screen
 */
void XFillRectangle(Display *display, Drawable d, GC gc, int x, int y, 
					unsigned int width, unsigned int height)
{
	HPEN oldPen;
	HDC dc;
	HBITMAP hOldBitmap = NULL;
	
	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	if (d > 0 && d < NUMBER_BITMAP)
		if (bmInfo[d].isActive == TRUE)
			hOldBitmap = SelectObject(dc, DrawableGetBitmap(d));

	if (width == 1 && height == 1)
	{
		SetPixelV(dc, x, y, RGB(cred, cgreen, cblue));
	}
	else
	{
		if (noPen == NULL)
			if ((noPen = CreatePen(PS_NULL, 0, RGB(0,0,0))) == NULL)
			{
				xlockmore_set_debug("Failed to create pen");
				return;
			}

		oldPen = SelectObject(dc, noPen);
		Rectangle(dc, x, y, x + width + 1, y + height + 1);
		SelectObject(dc, oldPen);
	}

	if (hOldBitmap)
		SelectObject(dc, hOldBitmap);
}

/*-
 *  XFillRectangles
 *    draws a number of filled rectangles on screen
 */
void XFillRectangles(Display *display, Drawable d, GC gc,
					 XRectangle *rectangles, int nrectangles)
{
	int i;

	for (i=0; i<nrectangles; i++) {
		XFillRectangle(display, d, gc, rectangles[i].x, rectangles[i].y,
			rectangles[i].width, rectangles[i].height);
	}
}

/*-
 *  XFlush
 *    not currently used
 */
void XFlush(Display *display)
{
}

/*-
 *  XFree
 *    not currently used
 */
void XFree(void *data)
{
}

/*-
 *  XFreeColormap
 *    not currently used
 */
void XFreeColormap(Display *display, Colormap colormap)
{
}

/*-
 *  XFreeColors
 *    not currently used
 */
void XFreeColors(Display *display, Colormap colormap, 
				 unsigned long pixels[], int npixels, 
				 unsigned long planes)
{
}

/*-
 *  XFreeCursor
 *    not currently used
 */
void XFreeCursor(Display *display, Cursor cursor)
{
}

/*-
 *  XFreeFont
 *    not currently used
 */
int XFreeFont(Display *display, XFontStruct *font_struct)
{
	return 0;
}

/*-
 *  XFreeFontInfo
 *    not currently used
 */
int XFreeFontInfo(char** names, XFontStruct* free_info, int actual_count)
{
	return 0;
}

/*-
 *  XFreeGC
 *    Free the memory for a GC (HDC)
 */
void XFreeGC(Display *display, GC gc)
{
	if (gc >= 0 && gc < numGCInfo && gcInfo[gc].isActive == TRUE)
		gcInfo[gc].isActive = FALSE;
}

/*-
 *  XFreePixmap
 *    Free the memory allocated to a pixmap
 */
void XFreePixmap(Display *display, Pixmap pixmap)
{
	if (pixmap < 0 || pixmap >= NUMBER_BITMAP)
		return;

	if (bmInfo[pixmap].isActive == FALSE)
		return;

	if (bmInfo[pixmap].bitmap != NULL)
 	{
		DeleteObject(bmInfo[pixmap].bitmap);
		bmInfo[pixmap].isActive = FALSE;
		bmInfo[pixmap].bitmap = NULL;
	}
}

/*-
 *  XGContextFromGC
 *    not currently used
 */
GContext XGContextFromGC(GC gc)
{
    return None;
}
/*-
 *  XGetVisualInfo
 *    not currently used
 */
XVisualInfo *XGetVisualInfo(Display *display, long vinfo_mask,
							XVisualInfo *vinfo_template,
							int *nitems_return)
{
	return NULL;
}

/*-
 *  XGetWindowAttributes
 *    retrieves info about the window
 */
Status XGetWindowAttributes(Display *display, Window w,
							XWindowAttributes *window_attr_return)
{
	memset(window_attr_return, 0, sizeof(XWindowAttributes));

	window_attr_return->width = rc.right;
	window_attr_return->height = rc.bottom;
	window_attr_return->depth = 8; /* incorrect, but we'll set it to 8 */
	window_attr_return->root = (int)hwnd;

	return 0;
}

/*-
 *  XGrabKeyboard
 *    not currently used
 */
int XGrabKeyboard(Display *display, Window grab_window, 
				  Bool owner_events, int pointer_mode,
				  int keyboard_mode, Time time)
{
	return 0;
}

/*-
 *  XGrabPointer
 *    not currently used
 */
int XGrabPointer(Display *display, Window grab_window, Bool owner_events,
				 unsigned int event_mask, int pointer_mode, 
				 int keyboard_mode, Window confine_to, Cursor cursor,
				 Time time)
{
	return 0;
}

/*-
 *  XGrabServer
 *    not currently used
 */
void XGrabServer(Display *display)
{
}

/*-
 *  XInstallColormap
 *    not currently used
 */
void XInstallColormap(Display *display, Colormap colormap)
{
}

/*-
 *  XListHosts
 *    not currently used, returns NULL
 */
XHostAddress *XListHosts(Display *display, int *nhosts_return,
						 Bool *state_return)
{
	return NULL;
}

/*-
 *  XLoadQueryFont
 *    not currently used
 */
XFontStruct *XLoadQueryFont(Display *display, char *name)
{
	static XFontStruct fontStruct = { 0 };
	return &fontStruct;
}

/*-
 *  XLookupString
 *    not currently used
 */
int XLookupString(XKeyEvent *event_struct, char *buffer_return,
				  int bytes_buffer, KeySym *keysym_return,
				  XComposeStatus *status_in_out)
{
	return 0;
}

/*-
 *  XMapWindow
 *    not currently used
 */
void XMapWindow(Display *display, Window w)
{
}

/*-
 *  XNextEvent
 *    not currently used
 */
void XNextEvent(Display *display, XEvent *event_return)
{
}

/*-
 *  XOpenDisplay
 *    not currently used
 */
Display *XOpenDisplay(char *display_name)
{
	return NULL;
}

/*-
 *  XParseColor
 *    not currently used
 */
Status XParseColor(Display *display, Colormap colormap, 
				   char *spec, XColor *exact_def_return)
{
	return 0;
}

/*-
 *  XPending
 *    not currently used, return 0
 */
int XPending(Display *display)
{
	return 0;
}

/*-
 *  XPutBackEvent
 *    not currently used
 */
void XPutBackEvent(Display *display, XEvent *event)
{
}

/*-
 *  XPutImage
 *    Draw an image in screen
 */
void XPutImage(Display *display, Drawable d, GC gc, XImage *image, int src_x,
               int src_y, int dest_x, int dest_y, unsigned int width,
               unsigned int height)
{
	HDC hBitmapDC;
	HBITMAP hOldBitmap;
	HBITMAP hBitmap;
	HPALETTE hPalette;
	RGBQUAD rgbQuad = { 0, 0, 0, 0 };
	HDC dc;

	if ((dc = GCGetDC(d, gc)) == NULL)
		return;

	/* set the color table for the bitmap */
	rgbQuad.rgbRed = cred;
	rgbQuad.rgbGreen = cgreen;
	rgbQuad.rgbBlue = cblue;

	if (image->format == XYBitmap)
	{
		if (image->depth != 1)
			return;
	  
		if ((hBitmapDC = CreateCompatibleDC(dc)) == NULL)
			return;

		LCreateBitmap(image, &hBitmap, &hPalette);
		DeleteObject(hPalette); /* we don't need the palette */
		if (hBitmap == NULL)
		{
			DeleteObject(hBitmap);
			DeleteDC(hBitmapDC);
			return;
		}

		if ((hOldBitmap = (HBITMAP)SelectObject(hBitmapDC, hBitmap)) == NULL)
		{
			DeleteObject(hBitmap);
			DeleteDC(hBitmapDC);
			return;
		}

		/* set the correct color table */
		SetDIBColorTable(hBitmapDC, 0, 1, &rgbQuad);

		BitBlt(dc, dest_x, dest_y, width, height, hBitmapDC, 
				src_x, src_y, SRCCOPY);

		SelectObject(hBitmapDC, hOldBitmap);
		DeleteObject(hBitmap);
		DeleteDC(hBitmapDC);
	}
}

/*-
 *  XQueryColor
 *    not currently used
 */
void XQueryColor(Display *display, Colormap colormap, XColor *def_in_out)
{
}

/*-
 *  XQueryPointer
 *    not currently used
 */
Bool XQueryPointer(Display *display, Window w, Window *root_return,
				   Window *child_return, int *root_x_return, int *root_y_return,
				   int *win_x_return, int *win_y_return,
				   unsigned int *mask_return)
{
	return FALSE;
}

/*-
 *  XQueryFont
 *    not currently used
 */
XFontStruct *XQueryFont(Display* display, XID font_ID)
{
	static XFontStruct dummy;
	return &dummy;
}



/*-
 *  XRaiseWindow
 *    not currently used
 */
void XRaiseWindow(Display *display, Window w)
{
}

/*-
 *  XReadBitmapFile
 *    not currently used
 */
int XReadBitmapFile(Display *display, Drawable d, char *filename,
                    unsigned int *width_return, unsigned int *height_return,
					Pixmap *bitmap_return, int *x_hot_return, int *y_hot_return)
{
	char *data;
	unsigned int height;
	int rv;
	unsigned int width;

	/* TODO: implement me */
	return BitmapOpenFailed;

	/* read the file into data structure */

	/* create the bitmap */
	if ((*bitmap_return = XCreateBitmapFromData(display, d, data, width, height)) != None)
	{
		*width_return = width;
		*height_return = height;
		/* TODO:: hot spot */
		*x_hot_return = -1;
		*y_hot_return = -1;
		rv = BitmapSuccess;
	}
	else
		rv = BitmapFileInvalid;

	return rv;
}

/*-
 *  XRemoveHosts
 *    not currently used
 */
void XRemoveHosts(Display *display, XHostAddress *hosts, int num_hosts)
{
}

/*-
 *  XResourceManagerString
 *    not currently used, return NULL
 */
char *XResourceManagerString(Display *display)
{
	return NULL;
}

/*-
 *  XrmDestroyDatabase
 *    not currently used
 */
void XrmDestroyDatabase(XrmDatabase database)
{
}

/*-
 *  XrmGetFileDatabase
 *    not currently used, return NULL
 */
XrmDatabase XrmGetFileDatabase(char *filename)
{
	return NULL;
}

/*-
 *  XrmGetResource
 *    not currently used, return True
 */
Bool XrmGetResource(XrmDatabase database, char *str_name,
					char *str_class, char **str_type_return, 
					XrmValue *value_return)
{
	return True;
}

/*-
 *  XrmGetFileDatabase
 *    not currently used, return NULL
 */
XrmDatabase XrmGetStringDatabase(char *data)
{
	return NULL;
}

/*-
 *  XrmInitialize
 *    not currently used
 */
void XrmInitialize(void)
{
}

/*-
 *  XrmMergeDatabases
 *    not currently used
 */
void XrmMergeDatabases(XrmDatabase source_db, XrmDatabase *target_db)
{
}

/*-
 *  XrmParseCommand
 *    not currently used
 */
void XrmParseCommand(XrmDatabase *database, XrmOptionDescList table,
					 int table_count, char *name, int *argc_in_out,
					 char **argv_in_out)
{
}

/*-
 *  XSetBackground
 *    sets the background color of pens, shapes, etc
 */
void XSetBackground(Display *display, GC gc, unsigned long background)
{
	if (gc >= 0 && gc < numGCInfo && gcInfo[gc].isActive == TRUE)
	{
		gcInfo[gc].needsReset = TRUE;
		/* check that the background is in range */
		if (background > (NUMCOLORS+1))
			gcInfo[gc].color.background = NUMCOLORS-1;
		else
			gcInfo[gc].color.background = background;
	}
}

/*-
 *  XSetFillStyle
 *    sets the fill style
 */
void XSetFillStyle(Display *display, GC gc, int fill_style)
{
	if (gc >= 0 && gc < numGCInfo && gcInfo[gc].isActive == TRUE)
	{
		gcInfo[gc].needsReset = TRUE;
		gcInfo[gc].fillStyle.hasFillStyle = TRUE;
		gcInfo[gc].fillStyle.value = fill_style;
	}
}

/*-
 *  XSetFont
 *    sets the font to be used for text
 */
void XSetFont(Display *display, GC gc, Font font)
{
	if (gc >= 0 && gc < numGCInfo && gcInfo[gc].isActive == TRUE)
	{
		gcInfo[gc].needsReset = TRUE;
		gcInfo[gc].font.hasFont = TRUE;
		gcInfo[gc].font.value = font;
	}
}

/*-
 *  XSetForeground
 *    sets the color of the current color, forground pen and
 *    brush
 */
void XSetForeground(Display *display, GC gc, unsigned long foreground)
{
	if (gc >= 0 && gc < numGCInfo && gcInfo[gc].isActive == TRUE)
	{
		gcInfo[gc].needsReset = TRUE;
		/* check that the foreground is in range */
		if (foreground > (NUMCOLORS+1))
			gcInfo[gc].color.foreground = NUMCOLORS-1;
		else
			gcInfo[gc].color.foreground = foreground;
	}
}

/*-
 *  XSetFunction
 *    sets the logical operation applied between source pixel
 *    and destination pixel
 *  Not fully implemented yet.
 */

void XSetFunction(Display *display, GC gc, int function)
{
	if (gc >= 0 && gc < numGCInfo && gcInfo[gc].isActive == TRUE)
	{
		gcInfo[gc].needsReset = TRUE;
		gcInfo[gc].function.hasFunction = TRUE;
		gcInfo[gc].function.value = function;
	}
}

/*-
 *  XSetGraphicsExposures
 *  Not implemented yet.
 */

int XSetGraphicsExposures(Display *display, GC gc, Bool graphics_exposures)
{
	return 0;
}

/*-
 *  XSetLineAttributes
 *    creates a new pen that has the attributes given
 */
void XSetLineAttributes(Display *display, GC gc, 
						unsigned int line_width, int line_style,
						int cap_style, int join_style)
{
	if (gc >= 0 && gc < numGCInfo && gcInfo[gc].isActive == TRUE)
	{
		gcInfo[gc].needsReset = TRUE;
		gcInfo[gc].lineAttributes.hasLineAttributes = TRUE;
		gcInfo[gc].lineAttributes.width = line_width;
		gcInfo[gc].lineAttributes.lineStyle = line_style;
		gcInfo[gc].lineAttributes.capStyle = cap_style;
		gcInfo[gc].lineAttributes.joinStyle = join_style;
	}
}

/*-
 *  XSetScreenSaver
 *    not currently used
 */
void XSetScreenSaver(Display *display, int timeout, int interval,
					 int prefer_blanking, int allow_exposures)
{
}

/*-
 *  XSetStipple
 *    Set the stipple for filled objects
 */
void XSetStipple(Display *display, GC gc, Pixmap stipple)
{
	if (gc >= 0 && gc < numGCInfo && gcInfo[gc].isActive == TRUE)
	{
		gcInfo[gc].needsReset = TRUE;
		gcInfo[gc].stipple.hasStipple = TRUE;
		gcInfo[gc].stipple.graphic = stipple;
	}
}

/*-
 *  XSetTSOrigin
 *  Not implemented yet.
 */

void XSetTSOrigin(Display *display, GC gc, int ts_x_origin, int ts_y_origin)
{
}

/*-
 *  XSetWindowColormap
 *    not currently used
 */
void XSetWindowColormap(Display *display, Window w, Colormap colormap)
{
}

/*-
 *  XSetWMName
 *    not currently used
 */
void XSetWMName(Display *display, Window w, XTextProperty *text_prop)
{
}

/*-
 *  XStoreColors
 *    not currently used
 */
void XStoreColors(Display *display, Colormap colormap, XColor color[],
                  int ncolors)
{
}

/*-
 *  XStringListToTextProperty
 *    not currently used
 */
Status XStringListToTextProperty(char **list, int count, 
								 XTextProperty *text_prop_return)
{
	return 0;
}

/*-
 *  XSync
 *    not currently used
 */
void XSync(Display *display, Bool discard)
{
}

/*-
 *  XTextWidth
 *    not currently used, returns 0
 */
int XTextWidth(XFontStruct *font_struct, char *string, int count)
{
	return 0;
}

/*-
 *  XTranslateCoordinates
 *     Simple implementation ignores source and destination and just
 *     reports position of global hdc relative to the screen.
 */
Bool XTranslateCoordinates(Display* display, Window src_w, Window dest_w,
						   int src_x, int src_y,
						   int* dest_x_return, int* dest_y_return,
						   Window* child_return)
{
  POINT p;
  /* use global hdc */
  if (GetDCOrgEx(hdc, &p)) {
	*dest_x_return = p.x + src_x;
	*dest_y_return = p.y + src_y;
	
  } else {
	*dest_x_return = src_x;
	*dest_y_return = src_y;
  }
  *child_return = None;
  return 1;
}

/*-
 *  XUngrabKeyboard
 *    not currently used
 */
void XUngrabKeyboard(Display *display, Time time)
{
}

/*-
 *  XUngrabPointer
 *    not currently used
 */
void XUngrabPointer(Display *display, Time time)
{
}

/*-
 *  XUngrabServer
 *    not currently used
 */
void XUngrabServer(Display *display)
{
}

/*-
 *  XUnmapWindow
 *    not currently used
 */
void XUnmapWindow(Display *display, Window w)
{
}

#endif /* WIN32 */

/* -------------------------------------------------------------------- */
