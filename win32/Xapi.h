#ifndef __XLOCK_XAPI_H__
#define __XLOCK_XAPI_H__

/*-
 * @(#)Xapi.h	4.00 98/04/16 xlockmore
 *
 * Xapi.h - X API interface for WIN32 (windows 95/NT) platforms
 *
 * Copyright (c) 1998 by Petey Leinonen.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 *
 * 16-Apr-98: Initially written. Based off code written by myself for
 *            an older version of xlockmore for win95/NT
 *
 */
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN

/*-
 * includes
 */

#include <windows.h>

extern HWND hwnd;                         /* window handle */
extern HDC hdc;                           /* device context */
extern int cred, cgreen, cblue;           /* color reference of the pen */
extern unsigned char *red, *green, *blue; /* holds a list of colors */
extern int colorcount;                    /* number of colors used */
unsigned int randommode;                  /* number of mode to index */
extern RECT rc;                           /* coords of the screen */

/*-
 * defines
 */

/* -------------------------------------------------------------------- */

/*****************************************************************
 * ERROR CODES 
 *****************************************************************/

#define Success            0    /* everything's okay */
#define BadRequest         1    /* bad request code */
#define BadValue           2    /* int parameter out of range */
#define BadWindow          3    /* parameter not a Window */
#define BadPixmap          4    /* parameter not a Pixmap */
#define BadAtom            5    /* parameter not an Atom */
#define BadCursor          6    /* parameter not a Cursor */
#define BadFont            7    /* parameter not a Font */
#define BadMatch           8    /* parameter mismatch */
#define BadDrawable        9    /* parameter not a Pixmap or Window */
#define BadAccess         10    /* depending on context:
                                 - key/button already grabbed
                                 - attempt to free an illegal 
                                   cmap entry 
                                - attempt to store into a read-only 
                                   color map entry.
                                - attempt to modify the access control
                                   list from other than the local host.
                                */
#define BadAlloc          11    /* insufficient resources */
#define BadColor          12    /* no such colormap */
#define BadGC             13    /* parameter not a GC */
#define BadIDChoice       14    /* choice not in range or already used */
#define BadName           15    /* font or color name doesn't exist */
#define BadLength         16    /* Request length incorrect */
#define BadImplementation 17    /* server is defective */

/* redefined functions */
#define XWhitePixelOfScreen WhitePixelOfScreen

/* define False/True */
#define False     (0)
#define True      (!False)

#define None        (0L)
#define CurrentTime (0L)

#define DoRed     (0x1)
#define DoGreen   (0x2)
#define DoBlue    (0x4)

#define XYBitmap  (0x01)
#define XYPixmap  (0x02)
#define ZPixmap   (0x04)

#define LSBFirst  (0x00)
#define MSBFirst  (0x01)

#define AllocNone (0x00)
#define AllocAll  (0x01)

#define BitmapSuccess     (0)
#define BitmapOpenFailed  (1)
#define BitmapFileInvalid (2)
#define BitmapNoMemory    (3)

#define StaticGray  (0)
#define GrayScale   (1)
#define StaticColor (2)
#define PseudoColor (3)
#define TrueColor   (4)
#define DirectColor (5)

/* Key masks. Used as modifiers to GrabButton and GrabKey, results of QueryPointer,
   state in various key-, mouse-, and button-related events. */
#define ShiftMask               (1<<0)
#define LockMask                (1<<1)
#define ControlMask             (1<<2)
#define Mod1Mask                (1<<3)
#define Mod2Mask                (1<<4)
#define Mod3Mask                (1<<5)
#define Mod4Mask                (1<<6)
#define Mod5Mask                (1<<7)



#define VisualNoMask           (0x0)
#define VisualIDMask           (0x1)
#define VisualScreenMask       (0x2)
#define VisualDepthMask        (0x4)
#define VisualClassMask        (0x8)
#define VisualRedMaskMask      (0x10)
#define VisualGreenMaskMask    (0x20)
#define VisualBlueMaskMask     (0x40)
#define VisualColormapMaskMask (0x80)
#define VisualBitsPerRGBMask   (0x100)
#define VisualAllMask          (0x1FF)

#define GrabModeSync  (0)
#define GrabModeAsync (1)

#define GrabSuccess     (0)
#define AlreadyGrabbed  (1)
#define GrabInvalidTime (2)
#define GrabNotViewable (3)
#define GrabFrozen      (4)

#define NoEventMask                     0L
#define KeyPressMask                    (1L<<0)  
#define KeyReleaseMask                  (1L<<1)  
#define ButtonPressMask                 (1L<<2)  
#define ButtonReleaseMask               (1L<<3)  
#define EnterWindowMask                 (1L<<4)  
#define LeaveWindowMask                 (1L<<5)  
#define PointerMotionMask               (1L<<6)  
#define PointerMotionHintMask           (1L<<7)  
#define Button1MotionMask               (1L<<8)  
#define Button2MotionMask               (1L<<9)  
#define Button3MotionMask               (1L<<10) 
#define Button4MotionMask               (1L<<11) 
#define Button5MotionMask               (1L<<12) 
#define ButtonMotionMask                (1L<<13) 
#define KeymapStateMask                 (1L<<14) 
#define ExposureMask                    (1L<<15) 
#define VisibilityChangeMask            (1L<<16) 
#define StructureNotifyMask             (1L<<17) 
#define ResizeRedirectMask              (1L<<18) 
#define SubstructureNotifyMask          (1L<<19) 
#define SubstructureRedirectMask        (1L<<20) 
#define FocusChangeMask                 (1L<<21) 
#define PropertyChangeMask              (1L<<22) 
#define ColormapChangeMask              (1L<<23) 
#define OwnerGrabButtonMask             (1L<<24) 

#define KeyPress                2
#define KeyRelease              3
#define ButtonPress             4
#define ButtonRelease           5
#define MotionNotify            6
#define EnterNotify             7
#define LeaveNotify             8
#define FocusIn                 9
#define FocusOut                10
#define KeymapNotify            11
#define Expose                  12
#define GraphicsExpose          13
#define NoExpose                14
#define VisibilityNotify        15
#define CreateNotify            16
#define DestroyNotify           17
#define UnmapNotify             18
#define MapNotify               19
#define MapRequest              20
#define ReparentNotify          21
#define ConfigureNotify         22
#define ConfigureRequest        23
#define GravityNotify           24
#define ResizeRequest           25
#define CirculateNotify         26
#define CirculateRequest        27
#define PropertyNotify          28
#define SelectionClear          29
#define SelectionRequest        30
#define SelectionNotify         31
#define ColormapNotify          32
#define ClientMessage           33
#define MappingNotify           34
#define LASTEvent               35      

#define VisibilityUnobscured        (0)
#define VisibilityPartiallyObscured (1)
#define VisibilityFullyObscured     (2)

#define XC_left_ptr    (68)

#define Button1 (1)
#define Button2 (2)
#define Button3 (3)
#define Button4 (4)
#define Button5 (5)

#define SIGHUP  (0)
#define SIGQUIT (1)
#define SIGBUS  (2)

#define CoordModeOrigin   (0)
#define CoordModePrevious (1)

#define Complex   (0)
#define Nonconvex (1)
#define Convex    (2)

#define LineSolid      (0)
#define LineOnOffDash  (1)
#define LineDoubleDash (2)

#define CapNotLast    (0)
#define CapButt       (1)
#define CapRound      (2)
#define CapProjecting (3)

#define JoinMiter (0)
#define JoinRound (1)
#define JoinBevel (2)


/*
#define   GCFunction                  (1L<<0)
#define   GCPlaneMask                 (1L<<1)
#define   GCForeground                (1L<<2)
#define   GCBackground                (1L<<3)
#define   GCLineWidth                 (1L<<4)
#define   GCLineStyle                 (1L<<5)
#define   GCCapStyle                  (1L<<6)
#define   GCJoinStyle                 (1L<<7)
#define   GCFillStyle                 (1L<<8)
#define   GCFillRule                  (1L<<9)
#define   GCTile                      (1L<<10)
#define   GCStipple                   (1L<<11)
#define   GCTileStipXOrigin           (1L<<12)
#define   GCTileStipYOrigin           (1L<<13)
#define   GCFont                      (1L<<14)
#define   GCSubwindowMode             (1L<<15)
#define   GCGraphicsExposures         (1L<<16)
#define   GCClipXOrigin               (1L<<17)
#define   GCClipYOrigin               (1L<<18)
#define   GCClipMask                  (1L<<19)
#define   GCDashOffset                (1L<<20)
#define   GCDashList                  (1L<<21)
#define   GCArcMode                   (1L<<22)
*/

#define   GCFunction                  (1L<<0)
#define   GCForeground                (1L<<2)
#define   GCBackground                (1L<<3)
#define   GCLineWidth                 (1L<<4)
#define   GCLineStyle                 (1L<<5)
#define   GCCapStyle                  (1L<<6)
#define   GCJoinStyle                 (1L<<7)
#define   GCFillStyle                 (1L<<8)
#define   GCStipple                   (1L<<11)
#define   GCFont                      (1L<<14)
#define   GCGraphicsExposures         (1L<<16)

#define FillSolid          (0)
#define FillTiled          (1)
#define FillStippled       (2)
#define FillOpaqueStippled (3)

#define GXclear         (0x0)
#define GXand           (0x1)
#define GXandReverse    (0x2)
#define GXcopy			(0x3)
#define GXandInverted   (0x4)
#define GXnoop			(0x5)
#define GXxor			(0x6)
#define GXor			(0x7)
#define GXnor			(0x8)
#define GXequiv			(0x9)
#define GXinvert		(0xa)
#define GXorReverse		(0xb)
#define GXcopyInverted  (0xc)
#define GXorInverted    (0xd)
#define GXnand			(0xe)
#define GXset			(0xf)

/*-
 * types
 */

/* -------------------------------------------------------------------- */

/* simple */

/* XPointer type: not needed for WIN32 */
typedef void* XPointer;

/* XID type */
typedef unsigned long XID;

/* Atom type: not needed for WIN32*/
typedef unsigned long Atom;

/* Bool type */
typedef int Bool;

/* Colormap type: not needed for WIN32 */
typedef XID Colormap;

/* Cursor type: not needed for WIN32 */
typedef XID Cursor;

/* Display type: not needed for WIN32 */
typedef int Display;

/* Drawable type, can be bitmap or window */
typedef int Drawable;

/* Font type: not needed for WIN32 */
typedef XID Font;

/* GC type: really a HDC */
typedef int GC;

/* GContext type: not needed for WIN32 */
typedef XID GContext;

/* Keysym type: not needed for WIN32 */
typedef XID KeySym;

/* Pixmap type, implemented a bitmap handler for WIN32 */
typedef int Pixmap;

/* Screen type: not needed for WIN32 */
typedef int Screen;

/* Status type: not needed for WIN32 */
typedef int Status;

/* Time type: not needed for WIN32 */
typedef unsigned long Time;

/* VisualID type: not needed for WIN32 */
typedef unsigned long VisualID;

/* Window type: really a HWND but typecasting to int,
 * this is because a Window is also a Drawable.
 * We don't use this anyway */
typedef int Window;

/* XExtData type: used below */
typedef char XExtData;

/* XrmQuark, XrmQuarkList: not needed */
typedef int  XrmQuark, *XrmQuarkList;

/* complex */

/* Visual type: not needed for WIN32 */
typedef struct {
	XExtData *ext_data;
	VisualID visualid;
#if defined(__cplusplus) || defined(c_plusplus)
	int c_class;
#else
	int class;
#endif
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int bits_per_rgb;
	int map_entries;
} Visual;

/* XAnyEvent type: not needed for WIN32 */
typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
} XAnyEvent;

/* XArc */
typedef struct {
	short x,y;
	unsigned short width, height;
	short angle1, angle2;
} XArc;

/* XCharStruct type: not needed for WIN32 */
typedef struct {
	short lbearing;
	short rbearing;
	short width;
	short ascent;
	short descent;
	unsigned short attributes;
} XCharStruct;

/* XClassHint type: not needed for WIN32 */
typedef struct {
	char *res_name;
	char *res_class;
} XClassHint;

/* XComposeStatus type: not needed for WIN32 */
typedef struct {
	XPointer compose_ptr;
	int chars_matched;
} XComposeStatus;

/* XKeyEvent type: */
typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Window root;
	Window subwindow;
	Time time;
	int x, y;
	int x_root, y_root;
	unsigned int state;
	unsigned int keycode;
	Bool same_screen;
} XKeyEvent;

/* XButtonEvent type: */
typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Window root;
	Window subwindow;
	Time time;
	int x, y;
	int x_root, y_root;
	unsigned int state;
	unsigned int button;
	Bool same_screen;
} XButtonEvent;

/* XMotionEvent type */
typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	Window root;
	Window subwindow;
	Time time;
	int x, y;
	int x_root, y_root;
	unsigned int state;
	char is_hint;
	Bool same_screen;
} XMotionEvent;

/* XExposeEvent */
typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	int x, y;
	int width, height;
	int count;
} XExposeEvent;

/* XVisibilityEvent type */
typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	int state;
} XVisibilityEvent;

/* XConfigureEvent type */
typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window event;
	Window window;
	int x, y;
	int width, height;
	int border_width;
	Window above;
	Bool override_redirect;
} XConfigureEvent;

/* XEvent type: not needed for WIN32 */
typedef union _XEvent {
	int type;
	XAnyEvent xany;
	XKeyEvent xkey;
	XButtonEvent xbutton;
	XMotionEvent xmotion;
//	XCrossingEvent xcrossing;
//	XFocusChangeEvent xfocus;
	XExposeEvent xexpose;
//	XGraphicsExposeEvent xgraphicsexpose;
//	XNoExposeEvent xnoexpose;
	XVisibilityEvent xvisibility;
//	XCreateWindowEvent xcreatewindow;
//	XDestroyWindowEvent xdestroywindow;
//	XUnmapEvent xunmap;
//	XMapEvent xmap;
//	XMapRequestEvent xmaprequest;
//	XReparentEvent xreparent;
	XConfigureEvent xconfigure;
//	XGravityEvent xgravity;
//	XResizeRequestEvent xresizerequest;
//	XConfigureRequestEvent xconfigurerequest;
//	XCirculateEvent xcirculate;
//	XCirculateRequestEvent xcirculaterequest;
//	XPropertyEvent xproperty;
//	XSelectionClearEvent xselectionclear;
//	XSelectionRequestEvent xselectionrequest;
//	XSelectionEvent xselection;
//	XColormapEvent xcolormap;
//	XClientMessageEvent xclient;
//	XMappingEvent xmapping;
//	XErrorEvent xerror;
//	XKeymapEvent xkeymap;
	long pad[24];
} XEvent;

/* XFontProp type: not needed for WIN32 */
typedef struct {
	Atom name;
	unsigned long card32;
} XFontProp;

/* XFontStruct type: not needed for WIN32 */
typedef struct {
	XExtData *ext_data;
	Font fid;
	unsigned direction;
	unsigned min_char_or_byte2;
	unsigned max_char_or_byte2;
	unsigned min_byte1;
	unsigned max_byte1;
	Bool all_chars_exist;
	unsigned default_char;
	int n_properties;
	XFontProp *properties;
	XCharStruct min_bounds;
	XCharStruct max_bounds;
	XCharStruct *per_char;
	int ascent;
	int descent;
} XFontStruct;

/* XColor type */
typedef struct {
	unsigned long pixel;
	unsigned short red, green, blue;
	char flags;
	char pad;
} XColor;

/* XGCValues type */
typedef struct {
	int function;
	unsigned long plane_mask;
	unsigned long foreground;
	unsigned long background;
	int line_width;
	int line_style;
	int cap_style;
	int join_style;
	int fill_style;
	int fill_rule;
	int arc_mode;
	Pixmap tile;
	Pixmap stipple;
	int ts_x_origin;
	int ts_y_origin;
	Font font;
	int subwindow_mode;
	Bool graphics_exposures;
	int clip_x_origin;
	int clip_y_origin;
	Pixmap clip_mask;
	int dash_offset;
	char dashes;
} XGCValues;

/* XHostAddress type: not needed for WIN32 */
typedef struct {
	int family;
	int length;
	char *address;
} XHostAddress;

/* XImage type: not needed for WIN32 */
typedef struct {
	int width, height;
	int xoffset;
	int format;
	char *data;
	int byte_order;
	int bitmap_unit;
	int bitmap_bit_order;
	int bitmap_pad;
	int depth;
	int bytes_per_line;
} XImage;

/* XPoint type */
typedef struct {
	int x, y;
} XPoint;

/* XRectangle type */
typedef struct {
	short x,y;
	unsigned short width, height;
} XRectangle;

/* XrmBinding & XrmBindingList types: not needed for WIN32 */
typedef enum {
	XrmBindTightly, 
	XrmBindLoosely
} XrmBinding, *XrmBindingList;

/* XrmDatabase type: not needed for WIN32 */
typedef char** XrmDatabase;

/* XrmOptionKind type: used by XrmOptionDescRec */
typedef enum {
	XrmoptionNoArg,
	XrmoptionIsArg,
	XrmoptionStickyArg,
	XrmoptionSepArg,
	XrmoptionResArg,
	XrmoptionSkipArg,
	XrmoptionSkipLine,
	XrmoptionSkipNArgs
} XrmOptionKind;

/* XrmOptionDescRec type: not needed for WIN32 */
typedef struct {
	char *option;
	char *specifier;
	XrmOptionKind argKind;
	XPointer value;
} XrmOptionDescRec, *XrmOptionDescList;

/* XrmValue type: not needed for WIN32 */
typedef struct {
	unsigned int size;
	XPointer addr;
} XrmValue, *XrmValuePtr;

/* XSegment type */
typedef struct {
	short x1, y1, x2, y2;
} XSegment;

/* XTextProperty type: not needed for WIN32 */
typedef struct {
	unsigned char *value;
	Atom encoding;
	int format;
	unsigned long nitems;
} XTextProperty;

/* XVisualInfo type: not needed for WIN32 */
typedef struct {
	Visual *visual;
	VisualID visualid;
	int screen;
	int depth;
#if defined(__cplusplus) || defined(c_plusplus)
	int c_class;
#else
	int class;
#endif
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int colormap_size;
	int bits_per_rgb;
} XVisualInfo;

/* XWindowAttributes type: not needed for WIN32 */
typedef struct {
	int x, y;
	int width, height;
	int border_width;
	int depth;
	Visual *visual;
	Window root;
	int class;
	int bit_gravity;
	int win_gravity;
	int backing_store;
	unsigned long backing_planes;
	unsigned long backing_pixels;
	Bool save_under;
	Colormap colormap;
	Bool map_installed;
	int map_state;
	long all_events_masks;
	long your_event_mask;
	long do_not_propogate_mask;
	Bool override_redirect;
	Screen *screen;
} XWindowAttributes;

/* XWindowChanges type: not needed for WIN32 */
typedef struct {
	int x, y;
	int width, height;
	int border_width;
	Window sibling;
	int stack_mode;
} XWindowChanges;


/* caddr_t type: char address type */
typedef char *caddr_t;

/* -------------------------------------------------------------------- */

/*-
 * prototypes
 */

int nice(int level);
void sleep(int sec);
int sigmask(int signum);

unsigned long BlackPixel(Display *display, int screen_number);
int BlackPixelOfScreen(Screen *screen);
int CellsOfScreen(Screen *screen);
Colormap DefaultColormap(Display *display, int screen_number);
Colormap DefaultColormapOfScreen(Screen *screen);
Visual *DefaultVisual(Display *display, int screen_number);
int DisplayPlanes(Display *display, int screen_number);
char *DisplayString(Display *display);
Window RootWindow(Display *display, int screen_number);
int ScreenCount(Display *display);
Screen *ScreenOfDisplay(Display *display, int screen_number);
unsigned long WhitePixel(Display *display, int screen_number);
int WhitePixelOfScreen(Screen *screen);



void XAddHosts(Display *display, XHostAddress *hosts, int num_hosts);
Status XAllocColor(Display *display, Colormap colormap, 
				   XColor *screen_in_out);
Status XAllocColorCells(Display *display, Colormap colormap,
					    Bool contig, unsigned longplane_masks_return[],
						unsigned int nplanes, unsigned long pixels_return[],
						unsigned int npixels);
Status XAllocNamedColor(Display *display, Colormap colormap,
						char *color_name, XColor *screen_def_return,
						XColor *exact_def_return);
void XBell(Display *display, int percent);
void XChangeGC(Display *display, GC gc, unsigned long valuemask,
			   XGCValues *values);
Bool XCheckMaskEvent(Display *display, long event_mask, 
					 XEvent *event_return);
void XClearArea(Display *display, Window w, int x, int y,
				unsigned int width, unsigned int height, Bool exposures);
void XClearWindow(Display *display, Window w);
void XCloseDisplay(Display *display);
void XConfigureWindow(Display *display, Window w, 
					  unsigned int value_mask, 
					  XWindowChanges *values);
int XCopyArea(Display *display, Drawable src, Drawable dest, GC gc, 
              int src_x, int src_y, unsigned int width, unsigned height,
			  int dest_x, int dest_y);
Colormap XCopyColormapAndFree(Display *display, Colormap colormap);	
Pixmap XCreateBitmapFromData(Display *display, Drawable drawable, 
                             char *data, unsigned int width,
                             unsigned int height);
Colormap XCreateColormap(Display *display, Window w, 
						 Visual *visual, int alloc);
Cursor XCreateFontCursor(Display *display, unsigned int shape);
GC XCreateGC(Display *display, Drawable drawable, 
			 unsigned long valuemask, XGCValues *values);
XImage *XCreateImage(Display *display, Visual *visual, 
					 unsigned int depth, int format, int offset,
					 char *data, unsigned int width,
					 unsigned int height, int bitmap_pad,
					 int bytes_per_line);
Pixmap XCreatePixmap(Display *display, Drawable d, unsigned int width,
					 unsigned int height, unsigned int depth);
Cursor XCreatePixmapCursor(Display *display,
				Pixmap source, Pixmap mask,
				XColor *foreground_color, XColor *background_color,
				unsigned int x_hot, unsigned int y_hot);
Pixmap XCreatePixmapFromBitmapData(Display *display, Drawable drawable,
	    char *data, unsigned int width, unsigned int height,
		unsigned long fg, unsigned long bg, unsigned int depth);
void XDefineCursor(Display *display, Window window, Cursor cursor);
void XDestroyImage(XImage *ximage);
void XDisableAccessControl(Display *display);
void XDrawArc(Display *display, Drawable d, GC gc, int x, int y,
			  unsigned int width, unsigned int height,
			  int angle1, int angle2);
void XDrawImageString(Display *display, Drawable d, GC gc, 
					  int x, int y, char *string, int length);
void XDrawLine(Display *display, Drawable d, GC gc, 
			   int x1, int y1, int x2, int y2);
void XDrawLines(Display *display, Drawable d, GC gc,
				XPoint *points, int npoints, int mode);
void XDrawPoint(Display *display, Drawable d, GC gc, int x, int y);
void XDrawPoints(Display *display, Drawable d, GC gc, 
				 XPoint *pts, int numpts, int mode);
void XDrawRectangle(Display *display, Drawable d, GC gc, int x, int y,
                    unsigned int width, unsigned int height);
void XDrawSegments(Display *display, Drawable d, GC gc,
				 XSegment *segs, int numsegs);
void XDrawString(Display *display, Drawable d, GC gc, int x, int y, 
				 char *string, int length);
void XEnableAccessControl(Display *display);
void XFillArc(Display *display, Drawable d, GC gc, int x, int y,
			  unsigned int width, unsigned int height,
			  int angle1, int angle2);
void XFillArcs(Display *display, Drawable d, GC gc,
			   XArc *arcs, int narcs);
void XFillPolygon(Display *display, Drawable d, GC gc, XPoint *points,
				  int npoints, int shape, int mode);
void XFillRectangle(Display *display, Drawable d, GC gc, int x, int y, 
					unsigned int width, unsigned int height);
void XFillRectangles(Display *display, Drawable d, GC gc,
					 XRectangle *rectangles, int nrectangles);
void XFlush(Display *display);
void XFree(void *data);
void XFreeColormap(Display *display, Colormap colormap);
void XFreeColors(Display *display, Colormap colormap, 
				 unsigned long pixels[], int npixels, 
				 unsigned long planes);
void XFreeCursor(Display *display, Cursor cursor);
int XFreeFont(Display *display, XFontStruct *font_struct);
int XFreeFontInfo(char** names, XFontStruct* free_info, int actual_count);
void XFreeGC(Display *display, GC gc);
void XFreePixmap(Display *display, Pixmap pixmap);
GContext XGContextFromGC(GC gc);
XVisualInfo *XGetVisualInfo(Display *display, long vinfo_mask,
							XVisualInfo *vinfo_template,
							int *nitems_return);
Status XGetWindowAttributes(Display *display, Window w,
							XWindowAttributes *window_attr_return);
int XGrabKeyboard(Display *display, Window grab_window, 
				  Bool owner_events, int pointer_mode,
				  int keyboard_mode, Time time);
int XGrabPointer(Display *display, Window grab_window, Bool owner_events,
				 unsigned int event_mask, int pointer_mode, 
				 int keyboard_mode, Window confine_to, Cursor cursor,
				 Time time);
void XGrabServer(Display *display);
void XInstallColormap(Display *display, Colormap colormap);
XHostAddress *XListHosts(Display *display, int *nhosts_return,
						 Bool *state_return);
XFontStruct *XLoadQueryFont(Display *display, char *name);
int XLookupString(XKeyEvent *event_struct, char *buffer_return,
				  int bytes_buffer, KeySym *keysym_return,
				  XComposeStatus *status_in_out);
void XMapWindow(Display *display, Window w);
void XNextEvent(Display *display, XEvent *event_return);
Display *XOpenDisplay(char *display_name);
Status XParseColor(Display *display, Colormap colormap, 
				   char *spec, XColor *exact_def_return);
int XPending(Display *display);
void XPutBackEvent(Display *display, XEvent *event);
void XPutImage(Display *display, Drawable d, GC gc, XImage *image, int src_x,
               int src_y, int dest_x, int dest_y, unsigned int width,
               unsigned int height);
void XQueryColor(Display *display, Colormap colormap, XColor *def_in_out);
Bool XQueryPointer(Display *display, Window w, Window *root_return,
				   Window *child_return, int *root_x_return, int *root_y_return,
				   int *win_x_return, int *win_y_return,
				   unsigned int *mask_return);
XFontStruct *XQueryFont(Display* display, XID font_ID);
void XRaiseWindow(Display *display, Window w);
int XReadBitmapFile(Display *display, Drawable d, char *filename,
                    unsigned int *width_return, unsigned int *height_return,
					Pixmap *bitmap_return, int *x_hot_return, int *y_hot_return);
void XRemoveHosts(Display *display, XHostAddress *hosts, int num_hosts);
char *XResourceManagerString(Display *display);
void XrmDestroyDatabase(XrmDatabase database);
XrmDatabase XrmGetFileDatabase(char *filename);
Bool XrmGetResource(XrmDatabase database, char *str_name,
					char *str_class, char **str_type_return, 
					XrmValue *value_return);
XrmDatabase XrmGetStringDatabase(char *data);
void XrmInitialize(void);
void XrmMergeDatabases(XrmDatabase source_db, XrmDatabase *target_db);
void XrmParseCommand(XrmDatabase *database, XrmOptionDescList table,
					 int table_count, char *name, int *argc_in_out,
					 char **argv_in_out);
void XSetBackground(Display *display, GC gc, unsigned long background);
void XSetFillStyle(Display *display, GC gc, int fill_style);
void XSetFont(Display *display, GC gc, Font font);
void XSetForeground(Display *display, GC gc, unsigned long foreground);
void XSetFunction(Display *display, GC gc, int function);
int XSetGraphicsExposures(Display *display, GC gc, Bool graphics_exposures);
void XSetLineAttributes(Display *display, GC gc, 
						unsigned int line_width, int line_style,
						int cap_style, int join_style);
void XSetScreenSaver(Display *display, int timeout, int interval,
					 int prefer_blanking, int allow_exposures);
void XSetStipple(Display *display, GC gc, Pixmap stipple);
void XSetTSOrigin(Display *display, GC gc, int ts_x_origin, int ts_y_origin);
void XSetWindowColormap(Display *display, Window w, Colormap colormap);
void XSetWMName(Display *display, Window w, XTextProperty *text_prop);
Status XStringListToTextProperty(char **list, int count, 
								 XTextProperty *text_prop_return);
void XStoreColors(Display *display, Colormap colormap, XColor color[],
                  int ncolors); 
void XSync(Display *display, Bool discard);
int XTextWidth(XFontStruct *font_struct, char *string, int count);
Bool XTranslateCoordinates(Display* display, Window src_w, Window dest_w,
			   int src_x, int src_y,
			   int* dest_x_return, int* dest_y_return,
			   Window* child_return);
void XUngrabKeyboard(Display *display, Time time);
void XUngrabPointer(Display *display, Time time);
void XUngrabServer(Display *display);
void XUnmapWindow(Display *display, Window w);
/* -------------------------------------------------------------------- */

#endif /* WIN32 */
#endif /* __XLOCK_XAPI_H__ */
