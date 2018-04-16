/* xscreensaver, Copyright (c) 1991-2018 Jamie Zawinski <jwz@jwz.org>
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
   Xlib and that do Cocoa-ish or OpenGL-ish things that bear some resemblance
   to the things that Xlib might have done.
 */

#ifndef __JWXYZ_H__
#define __JWXYZ_H__

#include <stdlib.h> /* For abort(). */
#include <stdarg.h>

#if defined __FreeBSD__ || defined __MACH__ && defined __APPLE__
# include <sys/cdefs.h>
#endif

#ifndef __dead2
/* __dead2 is an undocumented FreeBSD-ism (and by extension, an OSX-ism),
   normally #defined in <sys/cdefs.h>.
 */
# if defined __GNUC__ || defined __clang__
#  define __dead2 __attribute__((__noreturn__))
# else
#  define __dead2
# endif
#endif

extern void jwxyz_abort(const char *fmt, ...) __dead2;
#define abort() jwxyz_abort("abort in %s:%d", __FUNCTION__, __LINE__)

typedef int Bool;
typedef int Status;
typedef void * XPointer;
typedef unsigned long Time;
typedef unsigned int KeySym;
typedef unsigned int KeyCode;
typedef unsigned long Atom; /* Must be as large as a char *. */

typedef struct jwxyz_Display		Display;
typedef struct jwxyz_Display		Screen;
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
typedef struct jwxyz_XFontProp          XFontProp;
typedef struct jwxyz_XFontStruct	XFontStruct;
typedef struct jwxyz_Font *		Font;
typedef struct jwxyz_XFontSet *		XFontSet;
typedef struct jwxyz_XCharStruct	XCharStruct;
typedef struct jwxyz_XComposeStatus	XComposeStatus;
typedef struct jwxyz_XPixmapFormatValues XPixmapFormatValues;
typedef struct jwxyz_XChar2b            XChar2b;

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
typedef struct jwxyz_linked_point	linked_point;

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

#define XK_F1			0xFFBE
#define XK_F2			0xFFBF
#define XK_F3			0xFFC0
#define XK_F4			0xFFC1
#define XK_F5			0xFFC2
#define XK_F6			0xFFC3
#define XK_F7			0xFFC4
#define XK_F8			0xFFC5
#define XK_F9			0xFFC6
#define XK_F10			0xFFC7
#define XK_F11			0xFFC8
#define XK_F12			0xFFC9


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

#define XA_FONT                 18

#define DefaultScreen(dpy) (0)
#define BlackPixelOfScreen XBlackPixelOfScreen
#define WhitePixelOfScreen XWhitePixelOfScreen
#define BlackPixel(dpy,n) BlackPixelOfScreen(ScreenOfDisplay(dpy,n))
#define WhitePixel(dpy,n) WhitePixelOfScreen(ScreenOfDisplay(dpy,n))
#define CellsOfScreen XCellsOfScreen
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
#define XWidthOfScreen(s) (DisplayWidth(DisplayOfScreen(s),0))
#define XHeightOfScreen(s) (DisplayHeight(DisplayOfScreen(s),0))
#define XWidthMMOfScreen(s) (XDisplayWidthMM(DisplayOfScreen(s),0))
#define XHeightMMOfScreen(s) (XDisplayHeightMM(DisplayOfScreen(s),0))
#define XDefaultScreenOfDisplay(d) (d)
#define XDisplayOfScreen(s) (s)
#define XDisplayNumberOfScreen(s) 0
#define XScreenNumberOfScreen(s) 0

extern int XDisplayWidth (Display *, int);
extern int XDisplayHeight (Display *, int);
extern int XDisplayWidthMM (Display *, int);
extern int XDisplayHeightMM (Display *, int);

unsigned long XBlackPixelOfScreen(Screen *);
unsigned long XWhitePixelOfScreen(Screen *);
unsigned long XCellsOfScreen(Screen *);

extern int XDrawPoint (Display *, Drawable, GC, int x, int y);

extern int XChangeGC (Display *, GC, unsigned long mask, XGCValues *);

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
extern int jwxyz_XSetAlphaAllowed (Display *, GC, Bool);
extern int jwxyz_XSetAntiAliasing (Display *, GC, Bool);

extern int XFlush (Display *);
extern int XSync (Display *, Bool);
extern int XFreeColors (Display *, Colormap, unsigned long *px, int n,
                        unsigned long planes);
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
extern int XDrawString16 (Display *, Drawable, GC, int x, int y,
                          const XChar2b *, int len);

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
extern XImage *XGetImage (Display *, Drawable, int x, int y,
                          unsigned int w, unsigned int h,
                          unsigned long pm, int fmt);
extern Pixmap XCreatePixmapFromBitmapData (Display *, Drawable,
                                           const char *data,
                                           unsigned int w, unsigned int h,
                                           unsigned long fg,
                                           unsigned long bg,
                                           unsigned int depth);
extern XPixmapFormatValues *XListPixmapFormats (Display *, int *count_ret);

extern void jwxyz_draw_NSImage_or_CGImage (Display *, Drawable, 
                                           Bool nsimg_p, void *NSImage_arg,
                                           XRectangle *geom_ret, 
                                           int exif_rotation);
extern XImage *jwxyz_png_to_ximage (Display *, Visual *,
                                    const unsigned char *, unsigned long size);

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
extern char * jwxyz_unicode_character_name (Display *, Font, unsigned long uc);
extern int XTextExtents16 (XFontStruct *, const XChar2b *, int length,
                           int *dir_ret, int *ascent_ret, int *descent_ret,
                           XCharStruct *overall_ret);
extern int XTextWidth (XFontStruct *, const char *, int length);
extern int XSetFont (Display *, GC, Font);

extern XFontSet XCreateFontSet (Display *, char *name, 
                                char ***missing_charset_list_return,
                                int *missing_charset_count_return,
                                char **def_string_return);
extern void XFreeFontSet (Display *, XFontSet);
extern void XFreeStringList (char **);
extern int Xutf8TextExtents (XFontSet, const char *, int num_bytes,
                             XRectangle *overall_ink_return,
                             XRectangle *overall_logical_return);
extern void Xutf8DrawString (Display *, Drawable, XFontSet, GC,
                             int x, int y, const char *, int num_bytes);

extern Pixmap XCreatePixmap (Display *, Drawable,
                             unsigned int width, unsigned int height,
                             unsigned int depth);
extern int XFreePixmap (Display *, Pixmap);

extern char *XGetAtomName (Display *, Atom);

extern void set_points_list(XPoint *points, int npoints, linked_point *root);
extern void traverse_points_list(linked_point * root);
extern void draw_three_vertices(linked_point * a, Bool triangle);
extern double compute_edge_length(linked_point * a, linked_point * b);
extern double get_angle(double a, double b, double c);
extern Bool is_same_slope(linked_point * a);
extern Bool is_an_ear(linked_point * a);
extern Bool is_three_point_loop(linked_point * head);

extern int draw_arc_gl(Display *dpy, Drawable d, GC gc, int x, int y,
                   unsigned int width, unsigned int height,
                   int angle1, int angle2, Bool fill_p);

// Log()/Logv(), for debugging JWXYZ. Screenhacks should still use
// fprintf(stderr, ...).
extern void Log(const char *format, ...)
#if defined __GNUC__ || defined __clang__
  __attribute__((format(printf, 1, 2)))
#endif
  ;

extern void jwxyz_logv(Bool error, const char *fmt, va_list args);
#define Logv(format, args) (jwxyz_logv(False, format, args))

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

// Some GLX stuff that also doesn't technically belong here...
// from XScreenSaverGLView.m
extern void glXSwapBuffers (Display *, Window);
extern void glXMakeCurrent (Display *, Window, GLXContext);

// also declared in utils/visual.h
extern int has_writable_cells (Screen *, Visual *);
extern int visual_depth (Screen *, Visual *);
extern int visual_pixmap_depth (Screen *, Visual *);
extern int visual_cells (Screen *, Visual *);
extern int visual_class (Screen *, Visual *);
extern void visual_rgb_masks (Screen *screen, Visual *visual,
                              unsigned long *red_mask,
                              unsigned long *green_mask,
                              unsigned long *blue_mask);
extern int screen_number (Screen *);

// also declared in utils/grabclient.h
extern Bool use_subwindow_mode_p (Screen *, Window);

// also declared in xlockmoreI.h
extern void clear_gl_error (void);
extern void check_gl_error (const char *type);

// A Visual is supposed to be an opaque type, even though Xlib.h defines it.
// Only utils/xft.c uses this, out of necessity.
struct jwxyz_Visual {
  int class;		/* class of screen (monochrome, etc.) */
  unsigned long rgba_masks[4];	/* mask values */
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
  short	lbearing;	/* origin to left edge of ink */
  short	rbearing;	/* origin to right edge of ink */
  short	width;		/* advance to next char's origin */
  short	ascent;		/* baseline to top edge of ink */
  short	descent;	/* baseline to bottom edge of ink */
#if 0
  unsigned short attributes;	/* per char flags (not predefined) */
#endif
};

struct jwxyz_XFontProp {
  Atom          name;
  unsigned long card32; /* Careful: This holds (32- or 64-bit) pointers. */
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
  int         n_properties;   /* how many properties there are */
  XFontProp	*properties;	/* pointer to array of additional properties*/
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

struct jwxyz_XChar2b {
  unsigned char byte1;
  unsigned char byte2;
};


struct jwxyz_vtbl {
  Window (*root) (Display *);
  Visual *(*visual) (Display *);
  struct jwxyz_sources_data *(*display_sources_data) (Display *);

  unsigned long *(*window_background) (Display *);
  int (*draw_arc) (Display *dpy, Drawable d, GC gc, int x, int y,
                   unsigned int width, unsigned int height,
                   int angle1, int angle2, Bool fill_p);
  void (*fill_rects) (Display *dpy, Drawable d, GC gc,
                      const XRectangle *rectangles,
                      unsigned long nrects, unsigned long pixel);
  XGCValues *(*gc_gcv) (GC gc);
  unsigned int (*gc_depth) (GC gc);
  int (*draw_string) (Display *dpy, Drawable d, GC gc, int x, int y,
                      const char *str, size_t len, Bool utf8);

  void (*copy_area) (Display *dpy, Drawable src, Drawable dst, GC gc,
                     int src_x, int src_y,
                     unsigned int width, unsigned int height,
                     int dst_x, int dst_y);

  int (*DrawPoints) (Display *, Drawable, GC, XPoint *, int n, int mode);
  int (*DrawSegments) (Display *, Drawable, GC, XSegment *, int n);
  GC (*CreateGC) (Display *, Drawable, unsigned long mask, XGCValues *);
  int (*FreeGC) (Display *, GC);
  int (*ClearWindow) (Display *, Window);
  int (*SetClipMask) (Display *, GC, Pixmap);
  int (*SetClipOrigin) (Display *, GC, int x, int y);
  int (*FillPolygon) (Display *, Drawable, GC,
                      XPoint * points, int npoints, int shape, int mode);
  int (*DrawLines) (Display *, Drawable, GC, XPoint *, int n, int mode);

  int (*PutImage) (Display *, Drawable, GC, XImage *,
                    int src_x, int src_y, int dest_x, int dest_y,
                    unsigned int w, unsigned int h);
  XImage *(*GetSubImage) (Display *dpy, Drawable d, int x, int y,
                          unsigned int width, unsigned int height,
                          unsigned long plane_mask, int format,
                          XImage *dest_image, int dest_x, int dest_y);
};

#define JWXYZ_VTBL(dpy) (*(struct jwxyz_vtbl **)(dpy))

#define XRootWindow(dpy, screen) \
  ((dpy) ? JWXYZ_VTBL(dpy)->root(dpy) : 0)
#define XDefaultVisualOfScreen(screen) \
  ((screen) ? JWXYZ_VTBL(screen)->visual(screen) : 0)

#define XDrawPoints(dpy, d, gc, points, n, mode) \
  (JWXYZ_VTBL(dpy)->DrawPoints (dpy, d, gc, points, n, mode))
#define XDrawSegments(dpy, d, gc, segments, n) \
  (JWXYZ_VTBL(dpy)->DrawSegments (dpy, d, gc, segments, n))
#define XCreateGC(dpy, d, mask, gcv) \
  (JWXYZ_VTBL(dpy)->CreateGC (dpy, d, mask, gcv))
#define XFreeGC(dpy, gc) \
  (JWXYZ_VTBL(dpy)->FreeGC (dpy, gc))
#define XClearWindow(dpy, win) \
  (JWXYZ_VTBL(dpy)->ClearWindow(dpy, win))
#define XSetClipMask(dpy, gc, m) \
  (JWXYZ_VTBL(dpy)->SetClipMask (dpy, gc, m))
#define XSetClipOrigin(dpy, gc, x, y) \
  (JWXYZ_VTBL(dpy)->SetClipOrigin (dpy, gc, x, y))
#define XFillPolygon(dpy, d, gc, points, npoints, shape, mode) \
  (JWXYZ_VTBL(dpy)->FillPolygon (dpy, d, gc, points, npoints, shape, mode))
#define XDrawLines(dpy, d, gc, points, n, mode) \
  (JWXYZ_VTBL(dpy)->DrawLines (dpy, d, gc, points, n, mode))
#define XPutImage(dpy, d, gc, image, src_x, src_y, dest_x, dest_y, w, h) \
  (JWXYZ_VTBL(dpy)->PutImage (dpy, d, gc, image, src_x, src_y, \
                              dest_x, dest_y, w, h))
#define XGetSubImage(dpy, d, x, y, width, height, plane_mask, \
                     format, dest_image, dest_x, dest_y) \
  (JWXYZ_VTBL(dpy)->GetSubImage (dpy, d, x, y, width, height, plane_mask, \
                                 format, dest_image, dest_x, dest_y))


#endif /* __JWXYZ_H__ */
