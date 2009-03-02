extern unsigned long allocPixel(Display * display, Colormap cmap,
				char *name, char *def);

extern void setColormap(Display * display, Window window, Colormap map,
			Bool inwindow);
extern void fixColormap(Display * display, Window window,
			int screen, int ncolors, float saturation,
	  Bool mono, Bool install, Bool inroot, Bool inwindow, Bool verbose);
extern int  preserveColors(unsigned long black, unsigned long white,
			   unsigned long bg, unsigned long fg);

#if defined( USE_XPM ) || defined( USE_XPMINC )
extern void reserveColors(ModeInfo * mi, Colormap cmap,
			  unsigned long *black);

#endif
