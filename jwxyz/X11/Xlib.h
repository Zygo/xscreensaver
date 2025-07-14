#pragma once

#ifdef HAVE_JWXYZ

// Only define these types when building for jwxyz targets (Android, Web, iOS)
// For native Linux builds, these should not be defined to avoid conflicts

typedef struct jwxyz_XVisualInfo {
  void *visual;  // Use void* instead of Visual* to avoid conflicts
  unsigned long visualid;
  int screen;
  int depth;
  int class;
  unsigned long red_mask;
  unsigned long green_mask;
  unsigned long blue_mask;
  int colormap_size;
  int bits_per_rgb;
} jwxyz_XVisualInfo;

typedef struct jwxyz_XVisualInfo XVisualInfo;
typedef unsigned long VisualID;

#define VisualScreenMask  (1L << 0)
#define VisualIDMask      (1L << 1)

// Dummy function declarations using void* to avoid type conflicts
VisualID XVisualIDFromVisual(void *visual);
XVisualInfo *XGetVisualInfo(void *display, long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return);

#endif /* HAVE_JWXYZ */ 