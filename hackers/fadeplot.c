
/*-
 1) Mode has some problems i.e. the little window
 2) It would be nice if then number of "levels" changed in both x and y.
 3) Not random enough, i.e. always same starting position.
 */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)fadeplot.c  4.00 97/01/17 xlockmore";

#endif

/*-
 * fadeplot.c - fadeplot for xlock, the X Window System lockscreen.
 *
 * Some easy plotting stuff, by Bas van Gaalen, Holland, PD
 *
 * Converted for xlock by Charles Vidal
 * See xlock.c for copying information.
 */

/*- 
<add to mode.h>
extern ModeHook init_fadeplot;
extern ModeHook draw_fadeplot;
extern ModeHook release_fadeplot;
extern ModeHook refresh_fadeplot;
extern ModeSpecOpt fadeplot_opts;

<add to mode.c>
{"fadeplot", init_fadeplot, draw_fadeplot,
release_fadeplot, refresh_fadeplot, init_fadeplot,
NULL, &fadeplot_opts,
17000, 1500, 1, 1, 1.0, "fadeplot", 0, NULL},
 */

#include "xlock.h"

#define MINPTS 1

/* frequency */
#define PERIOD 500

ModeSpecOpt fadeplot_opts =
{0, NULL, 0, NULL, NULL};

typedef struct {
	XPoint      speed, step, factor, st;
	int         temps, maxpts;
	int         width, height;
	int         pix;
	XPoint     *pts;
} fadeplotstruct;

static fadeplotstruct *fadeplots = NULL;

static unsigned char SinTab[1001] =	/* 16 - 184 Avg 100 */
{
    100, 100, 100, 100, 100, 100, 99, 99, 99, 99, 98, 98, 98, 97, 97, 96, 96,
  95, 95, 94, 94, 93, 92, 92, 91, 90, 89, 88, 88, 87, 86, 85, 84, 83, 82, 81,
  80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 69, 68, 67, 66, 65, 64, 63, 61, 60,
  59, 58, 57, 55, 54, 53, 52, 51, 50, 48, 47, 46, 45, 44, 43, 42, 41, 39, 38,
  37, 36, 35, 34, 33, 32, 32, 31, 30, 29, 28, 27, 26, 26, 25, 24, 23, 23, 22,
  22, 21, 20, 20, 19, 19, 19, 18, 18, 17, 17, 17, 17, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21,
  22, 22, 23, 24, 24, 25, 26, 27, 27, 28, 29, 30, 31, 32, 33, 33, 34, 35, 36,
  37, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 57,
  58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
  77, 78, 79, 80, 81, 81, 82, 83, 84, 85, 85, 86, 87, 88, 88, 89, 90, 90, 91,
  91, 92, 92, 93, 93, 94, 94, 95, 95, 95, 96, 96, 97, 97, 97, 97, 98, 98, 98,
 98, 99, 99, 99, 99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100,
   100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
   100, 100, 100, 100, 100, 100, 100, 100, 100, 101, 101, 101, 101, 101, 101,
   101, 102, 102, 102, 102, 103, 103, 103, 103, 104, 104, 105, 105, 105, 106,
   106, 107, 107, 108, 108, 109, 109, 110, 110, 111, 112, 112, 113, 114, 115,
   115, 116, 117, 118, 119, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
   129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
   145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 157, 158, 159, 160,
   161, 162, 163, 164, 165, 166, 167, 167, 168, 169, 170, 171, 172, 173, 173,
   174, 175, 176, 176, 177, 178, 178, 179, 179, 180, 180, 181, 181, 182, 182,
   182, 183, 183, 183, 183, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
   184, 184, 183, 183, 183, 183, 182, 182, 181, 181, 181, 180, 180, 179, 178,
   178, 177, 177, 176, 175, 174, 174, 173, 172, 171, 170, 169, 168, 168, 167,
   166, 165, 164, 163, 162, 161, 159, 158, 157, 156, 155, 154, 153, 152, 150,
   149, 148, 147, 146, 145, 143, 142, 141, 140, 139, 137, 136, 135, 134, 133,
   132, 131, 129, 128, 127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117,
   116, 115, 114, 113, 112, 112, 111, 110, 109, 108, 108, 107, 106, 106, 105,
   105, 104, 104, 103, 103, 102, 102, 102, 101, 101, 101, 101, 100, 100, 100,
   100, 100, 100, 100, 100, 100, 100, 100, 101, 101, 101, 101, 102, 102, 102,
   103, 103, 104, 104, 105, 105, 106, 106, 107, 108, 108, 109, 110, 111, 112,
   112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
   127, 128, 129, 131, 132, 133, 134, 135, 136, 137, 139, 140, 141, 142, 143,
   145, 146, 147, 148, 149, 150, 152, 153, 154, 155, 156, 157, 158, 159, 161,
   162, 163, 164, 165, 166, 167, 168, 168, 169, 170, 171, 172, 173, 174, 174,
   175, 176, 177, 177, 178, 178, 179, 180, 180, 181, 181, 181, 182, 182, 183,
   183, 183, 183, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
   183, 183, 183, 183, 182, 182, 182, 181, 181, 180, 180, 179, 179, 178, 178,
   177, 176, 176, 175, 174, 173, 173, 172, 171, 170, 169, 168, 167, 167, 166,
   165, 164, 163, 162, 161, 160, 159, 158, 157, 155, 154, 153, 152, 151, 150,
   149, 148, 147, 146, 145, 143, 142, 141, 140, 139, 138, 137, 136, 135, 134,
   133, 132, 131, 130, 129, 128, 127, 126, 125, 124, 123, 122, 121, 120, 119,
   119, 118, 117, 116, 115, 115, 114, 113, 112, 112, 111, 110, 110, 109, 109,
   108, 108, 107, 107, 106, 106, 105, 105, 105, 104, 104, 103, 103, 103, 103,
   102, 102, 102, 102, 101, 101, 101, 101, 101, 101, 101, 100, 100, 100, 100,
   100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 99,
  99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 97, 97, 97, 97, 96, 96, 95, 95, 95,
  94, 94, 93, 93, 92, 92, 91, 91, 90, 90, 89, 88, 88, 87, 86, 85, 85, 84, 83,
  82, 81, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65,
  64, 63, 62, 61, 60, 59, 58, 57, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45,
  43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 33, 32, 31, 30, 29, 28, 27, 27,
  26, 25, 24, 24, 23, 22, 22, 21, 21, 20, 20, 19, 19, 18, 18, 18, 17, 17, 17,
  17, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18,
  19, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24, 25, 26, 26, 27, 28, 29, 30, 31,
  32, 32, 33, 34, 35, 36, 37, 38, 39, 41, 42, 43, 44, 45, 46, 47, 48, 50, 51,
  52, 53, 54, 55, 57, 58, 59, 60, 61, 63, 64, 65, 66, 67, 68, 69, 71, 72, 73,
  74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 88, 89, 90, 91,
 92, 92, 93, 94, 94, 95, 95, 96, 96, 97, 97, 98, 98, 98, 99, 99, 99, 99, 100,
	100, 100, 100, 100, 100
};

void
init_fadeplot(ModeInfo * mi)
{
	fadeplotstruct *fp;

	if (fadeplots == NULL) {
		if ((fadeplots = (fadeplotstruct *) calloc(MI_NUM_SCREENS(mi),
					   sizeof (fadeplotstruct))) == NULL)
			return;
	}
	fp = &fadeplots[MI_SCREEN(mi)];

	fp->width = MI_WIN_WIDTH(mi);
	fp->height = MI_WIN_HEIGHT(mi);

	fp->speed.x = 8;
	fp->speed.y = 10;
	fp->step.x = 2;
	fp->step.y = 1;
	fp->temps = 0;
	fp->factor.x = MI_WIN_WIDTH(mi) / 320 + 1;
	fp->factor.y = MI_WIN_HEIGHT(mi) / 200 + 1;

	fp->maxpts = MI_BATCHCOUNT(mi);
	if (fp->maxpts < -MINPTS) {
		/* if fp->maxpts is random ... the size can change */
		if (fp->pts != NULL) {
			(void) free((void *) fp->pts);
			fp->pts = NULL;
		}
		fp->maxpts = NRAND(-fp->maxpts - MINPTS + 1) + MINPTS;
	} else if (fp->maxpts < MINPTS)
		fp->maxpts = MINPTS;
	if (fp->pts == NULL)
		fp->pts = (XPoint *) calloc(fp->maxpts, sizeof (XPoint));
	if (MI_NPIXELS(mi) > 2)
		fp->pix = NRAND(MI_NPIXELS(mi));
	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));
}

void
draw_fadeplot(ModeInfo * mi)
{
	fadeplotstruct *fp = &fadeplots[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	int         i, j;
	long        temp;

	XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
	XDrawPoints(display, window, gc, fp->pts, fp->maxpts, CoordModeOrigin);

	if (MI_NPIXELS(mi) > 2) {
		XSetForeground(display, gc, MI_PIXEL(mi, fp->pix));
		if (++fp->pix >= MI_NPIXELS(mi))
			fp->pix = 0;
	} else
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));

#define NBSTEP 10
	if (MI_WIN_IS_ICONIC(mi)) {

		for (temp = NBSTEP - 1; temp >= 0; temp--) {
			j = temp;
			for (i = 0; i < fp->maxpts / NBSTEP; i++) {
				fp->pts[i].x = SinTab[(fp->st.x + fp->speed.x * j + i) % 1000] / 2;
				fp->pts[i].y =
					SinTab[(fp->st.y + fp->speed.y * j + i * fp->step.y) % 1000] / 2;
/* I don't know what is the size of the icon window  ?? */
/* divide by 2 is not a real solution but it works !   */
/* dot is integer type and I can't scale Sintab(integer type) by float */
			}
		}
	} else
		for (temp = NBSTEP - 1; temp >= 0; temp--) {
			j = temp;
			for (i = 0; i < fp->maxpts / NBSTEP; i++) {
				fp->pts[temp * i + i].x =
#if 0
					SinTab[(fp->st.x + fp->speed.x * j + i * fp->step.x) % 1000] *
#else
					SinTab[(fp->st.x + fp->speed.x * j + i) % 1000] *
#endif
					fp->factor.x + 50;
/* here you can scale by factor? bye the size of the screen is greater */
/* than 320,200 , may be it can be a problem ?? */

				fp->pts[temp * i + i].y =
					SinTab[(fp->st.y + fp->speed.y * j + i * fp->step.y) % 1000] *
					fp->factor.y;
			}
		}
	XDrawPoints(display, window, gc, fp->pts, fp->maxpts, CoordModeOrigin);
	XFlush(display);
	fp->st.x = (fp->st.x + fp->speed.x) % 1000;
	fp->st.y = (fp->st.y + fp->speed.y) % 1000;
	fp->temps++;
	if ((fp->temps % PERIOD) == 0) {
		fp->temps = fp->temps % PERIOD * 10;
		if ((fp->temps % PERIOD * 2) == 0)
			fp->speed.y = (fp->speed.y++) % 30 + 1;
		if ((fp->temps % PERIOD * 4) == 0)
			fp->speed.x = (fp->speed.x) % 20;
		if ((fp->temps % PERIOD * 6) == 0)
			fp->step.y = (fp->step.y++) % 2 + 1;
		fp->step.x = (fp->step.x++) % 4;
		XClearWindow(display, window);
	}
}
void
refresh_fadeplot(ModeInfo * mi)
{

}

void
release_fadeplot(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}
