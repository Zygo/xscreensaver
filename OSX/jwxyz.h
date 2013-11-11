/* xscreensaver, Copyright (c) 1991-2012 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* JWXYZ Is Not Xlib.

   But it's a bunch of function definitions that bear some resemblance to
   Xlib and that do Cocoa-ish things that bear some resemblance to the
   things that Xlib might have done.
 */

#ifndef __JWXYZ_H__
#define __JWXYZ_H__

extern void jwxyz_abort(const char *fmt, ...) __dead2;
#define abort() jwxyz_abort("abort in %s:%d", __FUNCTION__, __LINE__)

typedef int Bool;
typedef int Status;
typedef void * XPointer;
typedef unsigned long Time;
typedef unsigned int KeySym;
typedef unsigned int KeyCode;
typedef unsigned int VisualID;

typedef struct jwxyz_Display		Display;
typedef struct jwxyz_Screen		Screen;
typedef struct jwxyz_Visual		Visual;
typedef struct jwxyz_Drawable *		Drawable;
typedef struct jwxyz_Colormap *		Colormap;
typedef struct jwxyz_GC	*		GC;
typedef struct jwxyz_XColor		XColor;
typedef struct jwxyz_XGCValues		XGCValues;
typedef struct jwxyz_XPoint		XPoint;
typedef struct jwxyz_XSegment		XSegment;
typedef struct jwxyz_XRectangle		XRectangle;
typedef struct jwxyz_XArc		XArc;
typedef struct jwxyz_XWindowAttributes	XWindowAttributes;
typedef struct jwxyz_XrmOptionDescRec	XrmOptionDescRec;
typedef struct jwxyz_XrmDatabase *      XrmDatabase;
typedef struct jwxyz_XImage		XImage;
typedef struct jwxyz_XFontStruct	XFontStruct;
typedef struct jwxyz_Font *		Font;
typedef struct jwxyz_XCharStruct	XCharStruct;
typedef struct jwxyz_XComposeStatus	XComposeStatus;
typedef struct jwxyz_XPixmapFormatValues XPixmapFormatValues;

typedef union  jwxyz_XEvent		XEvent;
typedef struct jwxyz_XAnyEvent		XAnyEvent;
typedef struct jwxyz_XKeyEvent		XKeyEvent;
typedef struct jwxyz_XMotionEvent	XMotionEvent;
typedef struct jwxyz_XButtonEvent	XButtonEvent;
typedef XKeyEvent			XKeyPressedEvent;
typedef XKeyEvent			XKeyReleasedEvent;
typedef XMotionEvent			XPointerMovedEvent;
typedef XButtonEvent			XButtonPressedEvent;
typedef XButtonEvent			XButtonReleasedEvent;


/* Not technically Xlib... */
typedef struct jwxyz_GLXContext *	GLXContext;
typedef struct jwxyz_XtAppContext *	XtAppContext;
typedef struct jwxyz_XtIntervalId *	XtIntervalId;
typedef struct jwxyz_XtInputId *	XtInputId;
typedef void *				XtPointer;
typedef unsigned long			XtInputMask;
#define XtInputReadMask			(1L<<0)
#define XtInputWriteMask		(1L<<1)
#define XtInputExceptMask		(1L<<2)
#define XtIMXEvent			1
#define XtIMTimer			2
#define XtIMAlternateInput		4
#define XtIMSignal			8
#define XtIMAll (XtIMXEvent | XtIMTimer | XtIMAlternateInput | XtIMSignal)

#define True 1
#define TRUE 1
#define False 0
#define FALSE 0
#define None 0

#define Window Drawable
#define Pixmap Drawable

#define XrmoptionNoArg  0
#define XrmoptionSepArg 1

#define CoordModeOrigin		0
#define CoordModePrevious       1

#define LineSolid		0
#define LineOnOffDash		1
#define LineDoubleDash		2

#define CapNotLast		0
#define CapButt			1
#define CapRound		2
#define CapProjecting		3

#define JoinMiter		0
#define JoinRound		1
#define JoinBevel		2

#define FillSolid		0
#define FillTiled		1
#define FillStippled		2
#define FillOpaqueStippled	3

#define EvenOddRule		0
#define WindingRule		1

#define Complex			0
#define Nonconvex		1
#define Convex			2

#define XYBitmap		0
#define XYPixmap		1
#define ZPixmap			2

#define AllocNone		0
#define AllocAll		1

#define StaticGray		0
#define GrayScale		1
#define StaticColor		2
#define PseudoColor		3
#define TrueColor		4
#define DirectColor		5

#define LSBFirst		0
#define MSBFirst		1

#define DoRed			(1<<0)
#define DoGreen			(1<<1)
#define DoBlue			(1<<2)

#define GCFunction              (1L<<0)
#define GCPlaneMask             (1L<<1)
#define GCForeground            (1L<<2)
#define GCBackground            (1L<<3)
#define GCLineWidth             (1L<<4)
#define GCLineStyle             (1L<<5)
#define GCCapStyle              (1L<<6)
#define GCJoinStyle		(1L<<7)
#define GCFillStyle		(1L<<8)
#define GCFillRule		(1L<<9) 
#define GCTile			(1L<<10)
#define GCStipple		(1L<<11)
#define GCTileStipXOrigin	(1L<<12)
#define GCTileStipYOrigin	(1L<<13)
#define GCFont 			(1L<<14)
#define GCSubwindowMode		(1L<<15)
#define GCGraphicsExposures     (1L<<16)
#define GCClipXOrigin		(1L<<17)
#define GCClipYOrigin		(1L<<18)
#define GCClipMask		(1L<<19)
#define GCDashOffset		(1L<<20)
#define GCDashList		(1L<<21)
#define GCArcMode		(1L<<22)

#define KeyPress		2
#define KeyRelease		3
#define ButtonPress		4
#define ButtonRelease		5
#define MotionNotify		6
#define Expose			12
#define GraphicsExpose		13
#define NoExpose		14
#define VisibilityNotify	15

#define ClipByChildren		0
#define IncludeInferiors	1

#define KeyPressMask		(1L<<0)
#define KeyReleaseMask		(1L<<1)
#define ButtonPressMask		(1L<<2)
#define ButtonReleaseMask	(1L<<3)
#define PointerMotionMask	(1L<<6)

#define Button1			1
#define Button2			2
#define Button3			3
#define Button4			4
#define Button5			5

#define ShiftMask		(1<<0)
#define LockMask		(1<<1)
#define ControlMask		(1<<2)
#define Mod1Mask		(1<<3)
#define Mod2Mask		(1<<4)
#define Mod3Mask		(1<<5)
#define Mod4Mask		(1<<6)
#define Mod5Mask		(1<<7)
#define Button1Mask		(1<<8)
#define Button2Mask		(1<<9)
#define Button3Mask		(1<<10)
#define Button4Mask		(1<<11)
#define Button5Mask		(1<<12)

#define XK_Shift_L		0xFFE1
#define XK_Shift_R		0xFFE2
#define XK_Control_L		0xFFE3
#define XK_Control_R		0xFFE4
#define XK_Caps_Lock		0xFFE5
#define XK_Shift_Lock		0xFFE6
#define XK_Meta_L		0xFFE7
#define XK_Meta_R		0xFFE8
#define XK_Alt_L		0xFFE9
#define XK_Alt_R		0xFFEA
#define XK_Super_L		0xFFEB
#define XK_Super_R		0xFFEC
#define XK_Hyper_L		0xFFED
#define XK_Hyper_R		0xFFEE

#define XK_Home			0xFF50
#define XK_Left			0xFF51
#define XK_Up			0xFF52
#define XK_Right		0xFF53
#define XK_Down			0xFF54
#define XK_Prior		0xFF55
#define XK_Page_Up		0xFF55
#define XK_Next			0xFF56
#define XK_Page_Down		0xFF56
#define XK_End			0xFF57
#define XK_Begin		0xFF58

#define GXclear			0x0		/* 0 */
#define GXand			0x1		/* src AND dst */
// #define GXandReverse		0x2		/* src AND NOT dst */
#define GXcopy			0x3		/* src */
// #define GXandInverted	0x4		/* NOT src AND dst */
// #define GXnoop		0x5		/* dst */
#define GXxor			0x6		/* src XOR dst */
#define GXor			0x7		/* src OR dst */
// #define GXnor		0x8		/* NOT src AND NOT dst */
// #define GXequiv		0x9		/* NOT src XOR dst */
// #define GXinvert		0xa		/* NOT dst */
// #define GXorReverse		0xb		/* src OR NOT dst */
// #define GXcopyInverted	0xc		/* NOT src */
// #define GXorInverted		0xd		/* NOT src OR dst */
// #define GXnand		0xe		/* NOT src OR NOT dst */
#define GXset			0xf		/* 1 */

#define DefaultScreen(dpy) (0)
#define BlackPixelOfScreen(s) (0xFF000000)
#define WhitePixelOfScreen(s) (0xFFFFFFFF)
#define BlackPixel(dpy,n) BlackPixelOfScreen(0)
#define WhitePixel(dpy,n) WhitePixelOfScreen(0)
#define CellsOfScreen(s) (0x00FFFFFF)
#define XFree(x) free(x)
#define BitmapPad(dpy) (8)
#define BitmapBitOrder(dpy) (MSBFirst)
#define ImageByteOrder(dpy) (MSBFirst)
#define DisplayOfScreen XDisplayOfScreen
#define DefaultScreenOfDisplay XDefaultScreenOfDisplay
#define ScreenOfDisplay(dpy,n) DefaultScreenOfDisplay(dpy)
#define DefaultVisualOfScreen XDefaultVisualOfScreen
#define DefaultColormapOfScreen(s) (0)
#define RootWindow XRootWindow
#define RootWindowOfScreen(s) RootWindow(DisplayOfScreen(s),0)
#define DisplayWidth XDisplayWidth
#define DisplayHeight XDisplayHeight
#define XMaxRequestSize(dpy) (65535)

extern Display *jwxyz_make_display (void *nsview, void *cgc);
extern void jwxyz_free_display (Display *);
extern void *jwxyz_window_view (Window);
extern void jwxyz_window_resized (Display *, Window,
                                  int, int, int, int,
                                  void *cgc);
extern void jwxyz_mouse_moved (Display *, Window, int x, int y);
extern void jwxyz_flush_context (Display *);

extern Window XRootWindow (Display *, int screen);
extern Screen *XDefaultScreenOfDisplay (Display *);
extern Visual *XDefaultVisualOfScreen (Screen *);
extern Display *XDisplayOfScreen (Screen *);
extern int XDisplayNumberOfScreen (Screen *);
extern int XScreenNumberOfScreen (Screen *);
extern int XDisplayWidth (Display *, int);
extern int XDisplayHeight (Display *, int);

extern int XDrawPoint (Display *, Drawable, GC, int x, int y);
extern int XDrawPoints (Display *, Drawable, GC, XPoint *, int n, int mode);
extern int XDrawSegments (Display *, Drawable, GC, XSegment *, int n);

extern GC XCreateGC (Display *, Drawable, unsigned long mask, XGCValues *);
extern int XChangeGC (Display *, GC, unsigned long mask, XGCValues *);
extern int XFreeGC (Display *, GC);

extern int XClearWindow (Display *, Window);
extern int XClearArea (Display *, Window, int x, int y, int w, int h,Bool exp);
extern int XSetWindowBackground (Display *, Window, unsigned long);
extern Status XGetWindowAttributes (Display *, Window, XWindowAttributes *);
extern Status XGetGeometry (Display *, Drawable, Window *root_ret,
                            int *x_ret, int *y_ret, 
                            unsigned int *w_ret, unsigned int *h_ret,
                            unsigned int *bw_ret, unsigned int *depth_ret);
extern Status XAllocColor (Display *, Colormap, XColor *);
extern Status XAllocColorCells (Display *, Colormap, Bool contig,
                                unsigned long *pmret, unsigned int npl,
                                unsigned long *pxret, unsigned int npx);
extern int XStoreColors (Display *, Colormap, XColor *, int n);
extern int XStoreColor (Display *, Colormap, XColor *);
extern Status XParseColor(Display *, Colormap, const char *spec, XColor *ret);
extern Status XAllocNamedColor (Display *, Colormap, char *name,
                                XColor *screen_ret, XColor *exact_ret);
extern int XQueryColor (Display *, Colormap, XColor *);
extern int XQueryColors(Display *, Colormap colormap, XColor *, int ncolors);

extern int XSetForeground (Display *, GC, unsigned long);
extern int XSetBackground (Display *, GC, unsigned long);
extern int XSetFunction (Display *, GC, int);
extern int XSetSubwindowMode (Display *, GC, int);
extern int XSetLineAttributes (Display *, GC, unsigned int line_width,
                               int line_style, int cap_style, int join_style);
extern int XSetClipMask (Display *, GC, Pixmap);
extern int XSetClipOrigin (Display *, GC, int x, int y);
extern int jwxyz_XSetAlphaAllowed (Display *, GC, Bool);
extern int jwxyz_XSetAntiAliasing (Display *, GC, Bool);

extern int XFlush (Display *);
extern int XSync (Display *, Bool);
extern int XFreeColors (Display *, Colormap, unsigned long *px, int n,
                        unsigned long planes);
extern int XFillPolygon (Display *, Drawable, GC, 
                         XPoint * points, int npoints, int shape, int mode);
extern int XCopyArea (Display *, Drawable src, Drawable dest, GC, 
                      int src_x, int src_y, 
                      unsigned int width, unsigned int height, 
                      int dest_x, int dest_y);
extern int XCopyPlane (Display *, Drawable, Drawable, GC,
                       int src_x, int src_y,
                       unsigned width, int height,
                       int dest_x, int dest_y,
                       unsigned long plane);

extern int XDrawLine (Display *, Drawable, GC, int x1, int y1, int x2, int y2);
extern int XDrawLines (Display *, Drawable, GC, XPoint *, int n, int mode);
extern int XDrawArc (Display *, Drawable, GC, int x, int y, 
                     unsigned int width, unsigned int height,
                     int angle1, int angle2);
extern int XFillArc (Display *, Drawable, GC, int x, int y, 
                     unsigned int width, unsigned int height,
                     int angle1, int angle2);
extern int XDrawArcs (Display *, Drawable, GC, XArc *arcs, int narcs);
extern int XFillArcs (Display *, Drawable, GC, XArc *arcs, int narcs);
extern int XDrawRectangle (Display *, Drawable, GC, int x, int y, 
                           unsigned int width, unsigned int height);
extern int XFillRectangle (Display *, Drawable, GC, int x, int y, 
                           unsigned int width, unsigned int height);
extern int XFillRectangles (Display *, Drawable, GC, XRectangle *, int n);

extern int XDrawString (Display *, Drawable, GC, int x, int y, const char *,
                        int len);
extern int XDrawImageString (Display *, Drawable, GC, int x, int y, 
                             const char *, int len);

extern Bool XQueryPointer (Display *, Window, Window *root_ret,
                           Window *child_ret,
                           int *root_x_ret, int *root_y_ret,
                           int *win_x_ret, int *win_y_ret,
                           unsigned int *mask_ret);
extern int XLookupString (XKeyEvent *, char *ret, int size, KeySym *ks_ret,
                          XComposeStatus *);
extern KeySym XKeycodeToKeysym (Display *, KeyCode, int index);

extern Status XInitImage (XImage *);
extern XImage *XCreateImage (Display *, Visual *, unsigned int depth,
                             int format, int offset, char *data,
                             unsigned int width, unsigned int height,
                             int bitmap_pad, int bytes_per_line);
extern XImage *XSubImage (XImage *, int x, int y, 
                          unsigned int w, unsigned int h);

extern unsigned long XGetPixel (XImage *, int x, int y);
extern int XPutPixel (XImage *, int x, int y, unsigned long);
extern int XDestroyImage (XImage *);
extern int XPutImage (Display *, Drawable, GC, XImage *, 
                      int src_x, int src_y, int dest_x, int dest_y,
                      unsigned int w, unsigned int h);
extern XImage *XGetImage (Display *, Drawable, int x, int y,
                          unsigned int w, unsigned int h,
                          unsigned long pm, int fmt);
extern Pixmap XCreatePixmapFromBitmapData (Display *, Drawable,
                                           const char *data,
                                           unsigned int w, unsigned int h,
                                           unsigned long fg,
                                           unsigned int bg,
                                           unsigned int depth);
extern XPixmapFormatValues *XListPixmapFormats (Display *, int *count_ret);

extern void jwxyz_draw_NSImage_or_CGImage (Display *, Drawable, 
                                           Bool nsimg_p, void *NSImage_arg,
                                           XRectangle *geom_ret, 
                                           int exif_rotation);

extern int XSetGraphicsExposures (Display *, GC, Bool);
extern Bool XTranslateCoordinates (Display *, Window src_w, Window dest_w,
                                   int src_x, int src_y,
                                   int *dest_x_ret, int *dest_y_ret,
                                   Window *child_ret);

extern Font XLoadFont (Display *, const char *);
extern XFontStruct * XQueryFont (Display *, Font);
extern XFontStruct * XLoadQueryFont (Display *, const char *);
extern int XFreeFontInfo (char **names, XFontStruct *info, int n);
extern int XFreeFont (Display *, XFontStruct *);
extern int XUnloadFont (Display *, Font);
extern int XTextExtents (XFontStruct *, const char *, int length,
                         int *dir_ret, int *ascent_ret, int *descent_ret,
                         XCharStruct *overall_ret);
extern int XTextWidth (XFontStruct *, const char *, int length);
extern int XSetFont (Display *, GC, Font);

extern Pixmap XCreatePixmap (Display *, Drawable,
                             unsigned int width, unsigned int height,
                             unsigned int depth);
extern int XFreePixmap (Display *, Pixmap);

// Xt timers and fds
extern XtAppContext XtDisplayToApplicationContext (Display *);
typedef void (*XtTimerCallbackProc) (XtPointer closure, XtIntervalId *);
typedef void (*XtInputCallbackProc) (XtPointer closure, int *fd, XtInputId *);
extern XtIntervalId XtAppAddTimeOut (XtAppContext, unsigned long usecs,
                                     XtTimerCallbackProc, XtPointer closure);
extern void XtRemoveTimeOut (XtIntervalId);
extern XtInputId XtAppAddInput (XtAppContext, int fd, XtPointer flags,
                               XtInputCallbackProc, XtPointer closure);
extern void XtRemoveInput (XtInputId);
extern XtInputMask XtAppPending (XtAppContext);
extern void XtAppProcessEvent (XtAppContext, XtInputMask);
extern struct jwxyz_sources_data *display_sources_data (Display *);

// Some GLX stuff that also doesn't technically belong here...
// from XScreenSaverGLView.m
extern void glXSwapBuffers (Display *, Window);
extern void glXMakeCurrent (Display *, Window, GLXContext);

// also declared in utils/visual.h
extern int has_writable_cells (Screen *, Visual *);
extern int visual_depth (Screen *, Visual *);
extern int visual_cells (Screen *, Visual *);
extern int visual_class (Screen *, Visual *);

// also declared in utils/grabclient.h
extern Bool use_subwindow_mode_p (Screen *, Window);


struct jwxyz_Visual {
  VisualID visualid;	/* visual id of this visual */
  int class;		/* class of screen (monochrome, etc.) */
  unsigned long red_mask, green_mask, blue_mask;	/* mask values */
  int bits_per_rgb;	/* log base 2 of distinct color values */
//  int map_entries;	/* color map entries */
};

struct jwxyz_XGCValues {
  int function;		/* logical operation */
#if 0
  unsigned long plane_mask;/* plane mask */
#endif
  unsigned long foreground;/* foreground pixel */
  unsigned long background;/* background pixel */
  int line_width;	/* line width */
#if 0
  int line_style; 	/* LineSolid, LineOnOffDash, LineDoubleDash */
#endif
  int cap_style;	/* CapNotLast, CapButt, CapRound, CapProjecting */
  int join_style;	/* JoinMiter, JoinRound, JoinBevel */
#if 0
  int fill_style;	/* FillSolid, FillTiled, 
			   FillStippled, FillOpaeueStippled */
#endif
  int fill_rule;	/* EvenOddRule, WindingRule */
#if 0
  int arc_mode;		/* ArcChord, ArcPieSlice */
  Pixmap tile;		/* tile pixmap for tiling operations */
  Pixmap stipple;	/* stipple 1 plane pixmap for stipping */
  int ts_x_origin;	/* offset for tile or stipple operations */
  int ts_y_origin;
#endif
  Font font;	        /* default text font for text operations */
  int subwindow_mode;     /* ClipByChildren, IncludeInferiors */
#if 0
  Bool graphics_exposures;/* boolean, should exposures be generated */
#endif
  int clip_x_origin;	/* origin for clipping */
  int clip_y_origin;
  Pixmap clip_mask;	/* bitmap clipping; other calls for rects */
#if 0
  int dash_offset;	/* patterned/dashed line information */
  char dashes;
#endif

  Bool alpha_allowed_p;	/* jwxyz extension: whether pixel values may have
                           a non-opaque alpha component. */
  Bool antialias_p;	/* jwxyz extension: whether Quartz should draw
                           with antialiasing. */
};

struct jwxyz_XWindowAttributes {
    int x, y;			/* location of window */
    int width, height;		/* width and height of window */
    int border_width;		/* border width of window */
    int depth;          	/* depth of window */
    Visual *visual;		/* the associated visual structure */
#if 0
    Window root;        	/* root of screen containing window */
    int class;			/* InputOutput, InputOnly*/
    int bit_gravity;		/* one of bit gravity values */
    int win_gravity;		/* one of the window gravity values */
    int backing_store;		/* NotUseful, WhenMapped, Always */
    unsigned long backing_planes;/* planes to be preserved if possible */
    unsigned long backing_pixel;/* value to be used when restoring planes */
    Bool save_under;		/* boolean, should bits under be saved? */
#endif
    Colormap colormap;		/* color map to be associated with window */
#if 0
    Bool map_installed;		/* boolean, is color map currently installed*/
    int map_state;		/* IsUnmapped, IsUnviewable, IsViewable */
    long all_event_masks;	/* set of events all people have interest in*/
    long your_event_mask;	/* my event mask */
    long do_not_propagate_mask; /* set of events that should not propagate */
    Bool override_redirect;	/* boolean value for override-redirect */
#endif
    Screen *screen;		/* back pointer to correct screen */
};

struct jwxyz_XColor {
  unsigned long pixel;
  unsigned short red, green, blue;
  char flags;  /* do_red, do_green, do_blue */
  char pad;
};

struct jwxyz_XPoint {
  short x, y;
};

struct jwxyz_XSegment {
  short x1, y1, x2, y2;
};

struct jwxyz_XRectangle {
  short x, y;
  unsigned short width, height;
};

struct jwxyz_XArc {
  short x, y;
  unsigned short width, height;
  short angle1, angle2;
};


struct jwxyz_XrmOptionDescRec {
  char *option;
  char *specifier;
  int argKind;
  void *value;
};

struct jwxyz_XAnyEvent {
  int type;
#if 0
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
#endif
};

struct jwxyz_XKeyEvent {
  int type;
#if 0
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
#endif
  unsigned int state;
  unsigned int keycode;
#if 0
  Bool same_screen;
#endif
};

struct jwxyz_XButtonEvent {
  int type;
#if 0
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
#endif
  int x, y;
#if 0
  int x_root, y_root;
#endif
  unsigned int state;
  unsigned int button;
#if 0
  Bool same_screen;
#endif
};

struct jwxyz_XMotionEvent {
  int type;
#if 0
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
#endif
  int x, y;
#if 0
  int x_root, y_root;
#endif
  unsigned int state;
#if 0
  char is_hint;
  Bool same_screen;
#endif
};

union jwxyz_XEvent {
  int type;
  XAnyEvent xany;
  XKeyEvent xkey;
  XButtonEvent xbutton;
  XMotionEvent xmotion;
};

struct jwxyz_XImage {
    int width, height;		/* size of image */
    int xoffset;		/* number of pixels offset in X direction */
    int format;			/* XYBitmap, XYPixmap, ZPixmap */
    char *data;			/* pointer to image data */
    int byte_order;		/* data byte order, LSBFirst, MSBFirst */
    int bitmap_unit;		/* quant. of scanline 8, 16, 32 */
    int bitmap_bit_order;	/* LSBFirst, MSBFirst */
    int bitmap_pad;		/* 8, 16, 32 either XY or ZPixmap */
    int depth;			/* depth of image */
    int bytes_per_line;		/* accelarator to next line */
    int bits_per_pixel;		/* bits per pixel (ZPixmap) */
    unsigned long red_mask;	/* bits in z arrangment */
    unsigned long green_mask;
    unsigned long blue_mask;
//  XPointer obdata;		/* hook for the object routines to hang on */
    struct funcs {		/* image manipulation routines */
#if 0
      XImage *(*create_image)(
		Display*	/* display */,
		Visual*		/* visual */,
		unsigned int	/* depth */,
		int		/* format */,
		int		/* offset */,
		char*		/* data */,
		unsigned int	/* width */,
		unsigned int	/* height */,
		int		/* bitmap_pad */,
		int		/* bytes_per_line */);
	int (*destroy_image)        (XImage *);
#endif
	unsigned long (*get_pixel)  (XImage *, int, int);
	int (*put_pixel)            (XImage *, int, int, unsigned long);
#if 0
	XImage *(*sub_image)	    (XImage *, int, int, unsigned int, unsigned int);
	int (*add_pixel)            (XImage *, long);
#endif
    } f;
};

struct jwxyz_XCharStruct {
  short	lbearing;	/* origin to left edge of raster */
  short	rbearing;	/* origin to right edge of raster */
  short	width;		/* advance to next char's origin */
  short	ascent;		/* baseline to top edge of raster */
  short	descent;	/* baseline to bottom edge of raster */
#if 0
  unsigned short attributes;	/* per char flags (not predefined) */
#endif
};

struct jwxyz_XFontStruct {
#if 0
  XExtData	*ext_data;	/* hook for extension to hang data */
#endif
  Font          fid;            /* Font id for this font */
#if 0
  unsigned	direction;	/* hint about direction the font is painted */
#endif
  unsigned	min_char_or_byte2;	/* first character */
  unsigned	max_char_or_byte2;	/* last character */
#if 0
  unsigned	min_byte1;	/* first row that exists */
  unsigned	max_byte1;	/* last row that exists */
  Bool	all_chars_exist;	/* flag if all characters have non-zero size*/
#endif
  unsigned	default_char;	/* char to print for undefined character */
#if 0
  int         n_properties;   /* how many properties there are */
  XFontProp	*properties;	/* pointer to array of additional properties*/
#endif
  XCharStruct	min_bounds;	/* minimum bounds over all existing char*/
  XCharStruct	max_bounds;	/* maximum bounds over all existing char*/
  XCharStruct	*per_char;	/* first_char to last_char information */
  int		ascent;		/* log. extent above baseline for spacing */
  int		descent;	/* log. descent below baseline for spacing */
};

struct jwxyz_XComposeStatus {
  char dummy;
};

struct  jwxyz_XPixmapFormatValues {
  int depth;
  int bits_per_pixel;
  int scanline_pad;
};

#endif /* __JWXYZ_H__ */
