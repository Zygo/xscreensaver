/*
 Fireworkx 2.2 - Pyrotechnic explosions simulation,
 an eyecandy, live animating colorful fireworks super-blasts..!
 Copyright (GPL) 1999-2013 Rony B Chandran <ronybc@gmail.com>

 From Kerala, INDIA
 Website: http://www.ronybc.com

 Permission to use, copy, modify, distribute, and sell this software and its
 documentation for any purpose is hereby granted without fee, provided that
 the above copyright notice appear in all copies and that both that
 copyright notice and this permission notice appear in supporting
 documentation.  No representations are made about the suitability of this
 software for any purpose.  It is provided "as is" without express or
 implied warranty.

 2004-OCT: ronybc: Landed on Xscreensaver..!
 2012-DEC: ronybc: Almost rewrite of the last version (>4 years old)
           with SSE2 optimization, colored light flashes,
           HSV color and many visual and speed improvements.

 Additional coding:
 ---------------------------------------------------------------------------------
 Support for different display color modes: put_image()
 Jean-Pierre Demailly <Jean-Pierre.Demailly@ujf-grenoble.fr>

 Fixed array access problems by beating on it with a large hammer.
 Nicholas Miell <nmiell@gmail.com>

 Help 'free'ing up of memory with needed 'XSync's.
 Renuka S <renuka@ronybc.com>
 Rugmini R Chandran <rugmini@ronybc.com>
\
 */

#include "screenhack.h"

#ifdef __SSE2__
# include <emmintrin.h>
#endif

#define FWXVERSION "2.2"

#define WIDTH 1024                /* 888     */
#define HEIGHT 632                /* 548     */
#define SHELLCOUNT 4              /* FIXED NUMBER; for SSE optimization */
#define PIXCOUNT 500              /* 500     */
#define SHELL_LIFE_DEFAULT 32     /* 32      */
#define SHELL_LIFE_RATIO 6        /* 6       */
#define POWDER 5.0                /* 5.0     */
#define FTWEAK 12                 /* 12      */
#define FLASH_ZOOM 0.8            /* 1.0     */
#define G_ACCELERATION 0.001      /* GRAVITY */

typedef struct
{
	unsigned int burn;
	float x, y;
	float xv, yv;
} firepix;

typedef struct
{
	unsigned int cx, cy;
	unsigned int seq_number, life;
	unsigned int bicolor, flies, hshift, vshift;
	unsigned int mortar_fired, explode_y;
	float air_drag, vshift_phase;
	float flash_r, flash_g, flash_b;
	unsigned int h;
	double s, v;
	unsigned char r, g, b;
	firepix *fpix;
} fireshell;

struct state
{
	unsigned int fps_on;
	unsigned int flash_on;
	unsigned int shoot;
	unsigned int verbose;
	unsigned int width;
	unsigned int height;
	unsigned int fullscreen;
	unsigned int max_shell_life;
	unsigned int delay;
	float flash_fade;
	float *light_map;
	unsigned char *palaka1;
	unsigned char *palaka2;
	void *mem1;
	void *mem2;
	fireshell *fireshell_array;

	Display *dpy;
	Window window;
	XImage *xim;
	GC gc;
	XColor *colors;
	int depth;
	int bigendian;
	int ncolors;
	Bool button_down_p;
	int deferred;

};

/*
	will return zero.. divide with care.
*/
static unsigned int rnd(unsigned int x)
{
	return(random() % x);
}

static void fs_roll_rgb(fireshell *fs)
{
	unsigned short r, g, b;
	hsv_to_rgb (fs->h, fs->s, fs->v, &r, &g, &b);
	fs->r = (unsigned char) (r >> 8);
	fs->g = (unsigned char) (g >> 8);
	fs->b = (unsigned char) (b >> 8);
}

static void mix_colors(fireshell *fs)
{
	float flash;
	fs->h = rnd(360);
	fs->s = frand(0.4) + 0.6;
	fs->v = 1.0;
	fs_roll_rgb(fs);

	flash = rnd(444) + 111; /* Mega Jouls ! */
	fs->flash_r = fs->r * flash;
	fs->flash_g = fs->g * flash;
	fs->flash_b = fs->b * flash;
}

static void render_light_map(struct state *st, fireshell *fs)
{
	signed int x, y, v = 0;
	for (y = 0, v = fs->seq_number; y < st->height; y += 2)
	{
		for (x = 0; x < st->width; x += 2, v += SHELLCOUNT)
		{
			double f;
			f = sqrt((fs->cx - x) * (fs->cx - x) + (fs->cy - y) * (fs->cy - y)) + 4.0;
			f = FLASH_ZOOM / f;
			f += pow(f,0.1) * frand(0.0001); /* dither */
			st->light_map[v] = f;
		}
	}
}

static void recycle(struct state *st, fireshell *fs, unsigned int x, unsigned int y)
{
	unsigned int n, pixlife;
	firepix *fp = fs->fpix;
	fs->mortar_fired = st->shoot;
	fs->explode_y = y;
	fs->cx = x;
	fs->cy = st->shoot ? st->height : y ;
	fs->life = rnd(st->max_shell_life) + (st->max_shell_life/SHELL_LIFE_RATIO);
	fs->life += !rnd(25) ? st->max_shell_life * 5 : 0;
	fs->air_drag = 1.0 - (float)(rnd(200)) / (10000.0 + fs->life);
	fs->bicolor = !rnd(5) ? 120 : 0;
	fs->flies = !rnd(10) ? 1 : 0; /* flies' motion */
	fs->hshift = !rnd(5) ? 1 : 0; /* hue shifting  */
	fs->vshift = !rnd(10) ? 1 : 0; /* value shifting */
	fs->vshift_phase = M_PI/2.0;
	pixlife = rnd(fs->life) + fs->life / 10 + 1;    /* ! */
	for (n = 0; n < PIXCOUNT; n++)
	{
		fp->burn = rnd(pixlife) + 32;
		fp->xv = frand(2.0) * POWDER - POWDER;
		fp->yv = sqrt(POWDER * POWDER - fp->xv * fp->xv) * (frand(2.0) - 1.0);
		fp->x = x;
		fp->y = y;
		fp++;
	}
	mix_colors(fs);
	render_light_map(st, fs);
}

static void recycle_oldest(struct state *st, unsigned int x, unsigned int y)
{
	unsigned int n;
	fireshell *fs, *oldest;
	fs = oldest = st->fireshell_array;
	for (n = 0; n < SHELLCOUNT; n++)
	{
		if(fs[n].life < oldest->life) oldest = &fs[n];
	}
	recycle(st, oldest, x, y);
}

static void rotate_hue(fireshell *fs, int dh)
{
	fs->h = fs->h + dh;
	fs->s = fs->s - 0.001;
	fs_roll_rgb(fs);
}

static void wave_value(fireshell *fs)
{
	fs->vshift_phase = fs->vshift_phase + 0.008;
	fs->v = fabs(sin(fs->vshift_phase));
	fs_roll_rgb(fs);
}

static int explode(struct state *st, fireshell *fs)
{
	float air_drag;
	unsigned int n;
	unsigned int h = st->height;
	unsigned int w = st->width;
	unsigned char r, g, b;
	unsigned char *prgba;
	unsigned char *palaka = st->palaka1;
	firepix *fp = fs->fpix;
	if (fs->mortar_fired)
	{
		if (--fs->cy == fs->explode_y)
		{
			fs->mortar_fired = 0;
			mix_colors(fs);
			render_light_map(st, fs);
		}
		else
		{
			fs->flash_r =
			    fs->flash_g =
			        fs->flash_b = 50 + (fs->cy - fs->explode_y) * 10;
			prgba = palaka + (fs->cy * w + fs->cx + rnd(5) - 2) * 4;
			prgba[0] = (rnd(32) + 128);
			prgba[1] = (rnd(32) + 128);
			prgba[2] = (rnd(32) + 128);
			return(1);
		}
	}
	if ((fs->bicolor + 1) % 50 == 0) rotate_hue(fs, 180);
	if (fs->bicolor) --fs->bicolor;
	if (fs->hshift) rotate_hue(fs, rnd(8));
	if (fs->vshift) wave_value(fs);
	if (fs->flash_r > 1.0) fs->flash_r *= st->flash_fade;
	if (fs->flash_g > 1.0) fs->flash_g *= st->flash_fade;
	if (fs->flash_b > 1.0) fs->flash_b *= st->flash_fade;
	air_drag = fs->air_drag;
	r = fs->r;
	g = fs->g;
	b = fs->b;
	for (n = 0; n < PIXCOUNT; n++, fp++)
	{
		if (!fp->burn) continue;
		--fp->burn;
		if (fs->flies)
		{
			fp->x += fp->xv = fp->xv * air_drag + frand(0.1) - 0.05;
			fp->y += fp->yv = fp->yv * air_drag + frand(0.1) - 0.05 + G_ACCELERATION;
		}
		else
		{
			fp->x += fp->xv = fp->xv * air_drag + frand(0.01) - 0.005;
			fp->y += fp->yv = fp->yv * air_drag + frand(0.005) - 0.0025 + G_ACCELERATION;
		}
		if (fp->y > h)
		{
			if (rnd(5) == 3)
			{
				fp->yv *= -0.24;
				fp->y = h;
			}
			/* touch muddy ground :) */
			else fp->burn = 0;
		}
		if (fp->x < w && fp->x > 0 && fp->y < h && fp->y > 0)
		{
			prgba = palaka + ((int)fp->y * w + (int)fp->x) * 4;
			prgba[0] = b;
			prgba[1] = g;
			prgba[2] = r;
		}
	}
	return(--fs->life);
}

#ifdef __SSE2__

/* SSE2 optimized versions of glow_blur() and chromo_2x2_light() */

static void glow_blur(struct state *st)
{
	unsigned int n, nn;
	unsigned char *ps = st->palaka1;
	unsigned char *pd = st->palaka2;
	unsigned char *pa = st->palaka1 - (st->width * 4);
	unsigned char *pb = st->palaka1 + (st->width * 4);
	__m128i xmm0, xmm1, xmm2, xmm3, xmm4;

	xmm0 = _mm_setzero_si128();
	nn = st->width * st->height * 4;
	for (n = 0; n < nn; n+=16)
	{
		_mm_prefetch((const void *)&ps[n+16],_MM_HINT_T0);
		_mm_prefetch((const void *)&pa[n+16],_MM_HINT_T0);
		_mm_prefetch((const void *)&pb[n+16],_MM_HINT_T0);

		xmm1 = _mm_load_si128((const __m128i*)&ps[n]);
		xmm2 = xmm1;
		xmm1 = _mm_unpacklo_epi8(xmm1,xmm0);
		xmm2 = _mm_unpackhi_epi8(xmm2,xmm0);
		xmm3 = _mm_loadu_si128((const __m128i*)&ps[n+4]);
		xmm4 = xmm3;
		xmm3 = _mm_unpacklo_epi8(xmm3,xmm0);
		xmm4 = _mm_unpackhi_epi8(xmm4,xmm0);
		xmm3 = _mm_slli_epi16(xmm3,3);
		xmm4 = _mm_slli_epi16(xmm4,3);
		xmm1 = _mm_add_epi16(xmm1,xmm3);
		xmm2 = _mm_add_epi16(xmm2,xmm4);
		xmm3 = _mm_loadu_si128((const __m128i*)&ps[n+8]);
		xmm4 = xmm3;
		xmm3 = _mm_unpacklo_epi8(xmm3,xmm0);
		xmm4 = _mm_unpackhi_epi8(xmm4,xmm0);
		xmm1 = _mm_add_epi16(xmm1,xmm3);
		xmm2 = _mm_add_epi16(xmm2,xmm4);

		xmm3 = _mm_load_si128((const __m128i*)&pa[n]);
		xmm4 = xmm3;
		xmm3 = _mm_unpacklo_epi8(xmm3,xmm0);
		xmm4 = _mm_unpackhi_epi8(xmm4,xmm0);
		xmm1 = _mm_add_epi16(xmm1,xmm3);
		xmm2 = _mm_add_epi16(xmm2,xmm4);
		xmm3 = _mm_loadu_si128((const __m128i*)&pa[n+4]);
		xmm4 = xmm3;
		xmm3 = _mm_unpacklo_epi8(xmm3,xmm0);
		xmm4 = _mm_unpackhi_epi8(xmm4,xmm0);
		xmm1 = _mm_add_epi16(xmm1,xmm3);
		xmm2 = _mm_add_epi16(xmm2,xmm4);
		xmm3 = _mm_loadu_si128((const __m128i*)&pa[n+8]);
		xmm4 = xmm3;
		xmm3 = _mm_unpacklo_epi8(xmm3,xmm0);
		xmm4 = _mm_unpackhi_epi8(xmm4,xmm0);
		xmm1 = _mm_add_epi16(xmm1,xmm3);
		xmm2 = _mm_add_epi16(xmm2,xmm4);

		xmm3 = _mm_load_si128((const __m128i*)&pb[n]);
		xmm4 = xmm3;
		xmm3 = _mm_unpacklo_epi8(xmm3,xmm0);
		xmm4 = _mm_unpackhi_epi8(xmm4,xmm0);
		xmm1 = _mm_add_epi16(xmm1,xmm3);
		xmm2 = _mm_add_epi16(xmm2,xmm4);
		xmm3 = _mm_loadu_si128((const __m128i*)&pb[n+4]);
		xmm4 = xmm3;
		xmm3 = _mm_unpacklo_epi8(xmm3,xmm0);
		xmm4 = _mm_unpackhi_epi8(xmm4,xmm0);
		xmm1 = _mm_add_epi16(xmm1,xmm3);
		xmm2 = _mm_add_epi16(xmm2,xmm4);
		xmm3 = _mm_loadu_si128((const __m128i*)&pb[n+8]);
		xmm4 = xmm3;
		xmm3 = _mm_unpacklo_epi8(xmm3,xmm0);
		xmm4 = _mm_unpackhi_epi8(xmm4,xmm0);
		xmm1 = _mm_add_epi16(xmm1,xmm3);
		xmm2 = _mm_add_epi16(xmm2,xmm4);

		xmm3 = xmm1;
		xmm4 = xmm2;
		xmm1 = _mm_srli_epi16(xmm1,4);
		xmm2 = _mm_srli_epi16(xmm2,4);
		xmm3 = _mm_srli_epi16(xmm3,3);
		xmm4 = _mm_srli_epi16(xmm4,3);
		xmm1 = _mm_packus_epi16(xmm1,xmm2);
		xmm3 = _mm_packus_epi16(xmm3,xmm4);

		_mm_storeu_si128((__m128i*)&ps[n+4], xmm1);
		_mm_storeu_si128((__m128i*)&pd[n+4], xmm3);
	}
}

static void chromo_2x2_light(struct state *st)
{
	__m128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;
	__m128i xmi4, xmi5, xmi6, xmi7;

	unsigned int x, y, v = 0;
	unsigned int nl = st->width * 4;
	unsigned char *mem = st->palaka2;
	fireshell *fs = st->fireshell_array;

	xmm0 = _mm_setr_ps(fs[0].flash_b, fs[0].flash_g, fs[0].flash_r, 0.0);
	xmm1 = _mm_setr_ps(fs[1].flash_b, fs[1].flash_g, fs[1].flash_r, 0.0);
	xmm2 = _mm_setr_ps(fs[2].flash_b, fs[2].flash_g, fs[2].flash_r, 0.0);
	xmm3 = _mm_setr_ps(fs[3].flash_b, fs[3].flash_g, fs[3].flash_r, 0.0);

	xmi5 = _mm_setzero_si128();

	for (y = st->height/2; y; y--, mem += nl)
	{
		for (x = st->width/4; x; x--, v += 8, mem += 16)
		{
			xmm4 = _mm_set1_ps(st->light_map[v+0]);
			xmm5 = xmm0;
			xmm5 = _mm_mul_ps(xmm5,xmm4);
			xmm4 = _mm_set1_ps(st->light_map[v+1]);
			xmm4 = _mm_mul_ps(xmm4,xmm1);
			xmm5 = _mm_add_ps(xmm5,xmm4);
			xmm4 = _mm_set1_ps(st->light_map[v+2]);
			xmm4 = _mm_mul_ps(xmm4,xmm2);
			xmm5 = _mm_add_ps(xmm5,xmm4);
			xmm4 = _mm_set1_ps(st->light_map[v+3]);
			xmm4 = _mm_mul_ps(xmm4,xmm3);
			xmm5 = _mm_add_ps(xmm5,xmm4);

			xmm4 = _mm_set1_ps(st->light_map[v+4]);
			xmm6 = xmm0;
			xmm6 = _mm_mul_ps(xmm6,xmm4);
			xmm4 = _mm_set1_ps(st->light_map[v+5]);
			xmm4 = _mm_mul_ps(xmm4,xmm1);
			xmm6 = _mm_add_ps(xmm6,xmm4);
			xmm4 = _mm_set1_ps(st->light_map[v+6]);
			xmm4 = _mm_mul_ps(xmm4,xmm2);
			xmm6 = _mm_add_ps(xmm6,xmm4);
			xmm4 = _mm_set1_ps(st->light_map[v+7]);
			xmm4 = _mm_mul_ps(xmm4,xmm3);
			xmm6 = _mm_add_ps(xmm6,xmm4);

			xmi6 = _mm_cvtps_epi32(xmm5);
			xmi7 = _mm_cvtps_epi32(xmm6);
			xmi6 = _mm_packs_epi32(xmi6,xmi6);
			xmi7 = _mm_packs_epi32(xmi7,xmi7);

			xmi4 = _mm_load_si128((const __m128i*) mem);
			xmi5 = _mm_unpacklo_epi8(xmi5,xmi4);
			xmi5 = _mm_srli_epi16(xmi5,8);
			xmi4 = _mm_unpackhi_epi8(xmi4,xmi4);
			xmi4 = _mm_srli_epi16(xmi4,8);
			xmi5 = _mm_add_epi16(xmi5,xmi6);
			xmi4 = _mm_add_epi16(xmi4,xmi7);
			xmi5 = _mm_packus_epi16(xmi5,xmi4);
			_mm_store_si128((__m128i*) mem, xmi5);

			xmi4 = _mm_load_si128((const __m128i*) &mem[nl]);
			xmi5 = _mm_unpacklo_epi8(xmi5,xmi4);
			xmi5 = _mm_srli_epi16(xmi5,8);
			xmi4 = _mm_unpackhi_epi8(xmi4,xmi4);
			xmi4 = _mm_srli_epi16(xmi4,8);
			xmi5 = _mm_add_epi16(xmi5,xmi6);
			xmi4 = _mm_add_epi16(xmi4,xmi7);
			xmi5 = _mm_packus_epi16(xmi5,xmi4);
			_mm_store_si128((__m128i*) &mem[nl], xmi5);
		}
	}
}

#else

static void glow_blur(struct state *st)
{
	unsigned int n, q;
	unsigned char *pm = st->palaka1;
	unsigned char *po = st->palaka2;
	unsigned char *pa = pm - (st->width * 4);
	unsigned char *pb = pm + (st->width * 4);
	/*
		unsigned int rgba = 0;
		for (n = st->width*st->height*4; n; n--, pm++, pa++, pb++, po++)
		{
			if(++rgba > 3)
			{
				rgba = 0;
				continue;
			}
			q     = pm[0] + pm[4] * 8 + pm[8] +
			        pa[0] + pa[4] + pa[8] +
			        pb[0] + pb[4] + pb[8];
			pm[4] = q >> 4;
			po[4] = q > 2047 ? 255 : q >> 3;
		}
			--- using unrolled version ------------
	*/
	for (n = st->width*st->height*4; n; n-=4)
	{
		q = pm[0] + pm[4] * 8 + pm[8] +
		    pa[0] + pa[4] + pa[8] +
		    pb[0] + pb[4] + pb[8];
		pm[4] = q >> 4;
		po[4] = q > 2047 ? 255 : q >> 3;
		q = pm[1] + pm[5] * 8 + pm[9] +
		    pa[1] + pa[5] + pa[9] +
		    pb[1] + pb[5] + pb[9];
		pm[5] = q >> 4;
		po[5] = q > 2047 ? 255 : q >> 3;
		q = pm[2] + pm[6] * 8 + pm[10] +
		    pa[2] + pa[6] + pa[10] +
		    pb[2] + pb[6] + pb[10];
		pm[6] = q >> 4;
		po[6] = q > 2047 ? 255 : q >> 3;

		pm+=4, pa+=4, pb+=4, po+=4;
	}
}

static inline unsigned char addbs(unsigned char c, unsigned int i)
{
	i += c;
	return(i > 255 ? 255 : i);
}

static void chromo_2x2_light(struct state *st)
{
	unsigned int n, x, y, v = 0;
	unsigned int nl = st->width * 4;
	unsigned char *mem = st->palaka2;
	float r, g, b;
	float rgb[SHELLCOUNT*4];
	fireshell *fs = st->fireshell_array;

	for (n = 0, x = 0; n < SHELLCOUNT; n++, x += 4, fs++)
	{
		rgb[x  ] = fs->flash_r;
		rgb[x+1] = fs->flash_g;
		rgb[x+2] = fs->flash_b;
	}

	for (y = st->height/2; y; y--)
	{
		for (x = st->width/2; x; x--, v += 4)
		{
			r = rgb[0] * st->light_map[v] + rgb[4] * st->light_map[v+1]
			    + rgb[ 8] * st->light_map[v+2] + rgb[12] * st->light_map[v+3];
			g = rgb[1] * st->light_map[v] + rgb[5] * st->light_map[v+1]
			    + rgb[ 9] * st->light_map[v+2] + rgb[13] * st->light_map[v+3];
			b = rgb[2] * st->light_map[v] + rgb[6] * st->light_map[v+1]
			    + rgb[10] * st->light_map[v+2] + rgb[14] * st->light_map[v+3];

			mem[0] = addbs(mem[0], b);
			mem[1] = addbs(mem[1], g);
			mem[2] = addbs(mem[2], r);
			mem[4] = addbs(mem[4], b);
			mem[5] = addbs(mem[5], g);
			mem[6] = addbs(mem[6], r);

			mem += nl;

			mem[0] = addbs(mem[0], b);
			mem[1] = addbs(mem[1], g);
			mem[2] = addbs(mem[2], r);
			mem[4] = addbs(mem[4], b);
			mem[5] = addbs(mem[5], g);
			mem[6] = addbs(mem[6], r);

			mem -= nl - 8;
		}
		mem += nl;
	}
}

#endif

static void resize(struct state *st)
{
	unsigned int n;
	fireshell *fs = st->fireshell_array;
	XWindowAttributes xwa;
	XGetWindowAttributes (st->dpy, st->window, &xwa);
	xwa.width  -= xwa.width % 4;
	xwa.height -= xwa.height % 2;
	st->width  = xwa.width;
	st->height = xwa.height;
	if (st->verbose)
	{
		printf("resolution: %d x %d \n",st->width,st->height);
	}
	XSync(st->dpy, 0);
	if (st->xim)
	{
		/* if (st->xim->data == (char *)st->palaka2) */
                st->xim->data = NULL;
		XDestroyImage(st->xim);
		XSync(st->dpy, 0);
		free(st->mem2);
		free(st->mem1);
                st->xim = 0;
	}
	st->xim = XCreateImage(st->dpy, xwa.visual, xwa.depth, ZPixmap, 0, 0,
	                       st->width, st->height, 8, 0);
	if (!st->xim) return;

#ifdef __SSE2___ABANDONED /* causes __ERROR_use_memset_not_bzero_in_xscreensaver__ */
	st->mem1 = _mm_malloc(((st->height + 2) * st->width + 8)*4, 16);
	bzero(st->mem1, ((st->height + 2) * st->width + 8)*4);
	st->mem2 = _mm_malloc(((st->height + 2) * st->width + 8)*4, 16);
	bzero(st->mem2, ((st->height + 2) * st->width + 8)*4);
#else
	st->mem1 = calloc((st->height + 2) * st->width + 8, 4);
	st->mem2 = calloc((st->height + 2) * st->width + 8, 4);
#endif
	st->palaka1 = (unsigned char *) st->mem1 + (st->width * 4 + 16);
	st->palaka2 = (unsigned char *) st->mem2 + (st->width * 4 + 16);

	if (xwa.depth >= 24)
	{
		st->xim->data = (char *)st->palaka2;
	}
	else
	{
		st->xim->data = calloc(st->height, st->xim->bytes_per_line);
	}

	if (st->light_map) free(st->light_map);
	st->light_map = calloc((st->width * st->height * SHELLCOUNT)/4, sizeof(float));
	for (n = 0; n < SHELLCOUNT; n++, fs++)
	{
		render_light_map(st, fs);
	}
}

static void put_image(struct state *st)
{
	int x,y,i,j;
	unsigned char r, g, b;
	if (!st->xim) return;
	i = 0;
	j = 0;
	if (st->depth==16)
	{
		if(st->bigendian)
			for (y=0; y<st->xim->height; y++)
				for (x=0; x<st->xim->width; x++)
				{
					r = st->palaka2[j++];
					g = st->palaka2[j++];
					b = st->palaka2[j++];
					j++;
					st->xim->data[i++] = (g&224)>>5 | (r&248);
					st->xim->data[i++] = (b&248)>>3 | (g&28)<<3;
				}
		else
			for (y=0; y<st->xim->height; y++)
				for (x=0; x<st->xim->width; x++)
				{
					r = st->palaka2[j++];
					g = st->palaka2[j++];
					b = st->palaka2[j++];
					j++;
					st->xim->data[i++] = (b&248)>>3 | (g&28)<<3;
					st->xim->data[i++] = (g&224)>>5 | (r&248);
				}
	}
	if (st->depth==15)
	{
		if(st->bigendian)
			for (y=0; y<st->xim->height; y++)
				for (x=0; x<st->xim->width; x++)
				{
					r = st->palaka2[j++];
					g = st->palaka2[j++];
					b = st->palaka2[j++];
					j++;
					st->xim->data[i++] = (g&192)>>6 | (r&248)>>1;
					st->xim->data[i++] = (b&248)>>3 | (g&56)<<2;
				}
		else
			for (y=0; y<st->xim->height; y++)
				for (x=0; x<st->xim->width; x++)
				{
					r = st->palaka2[j++];
					g = st->palaka2[j++];
					b = st->palaka2[j++];
					j++;
					st->xim->data[i++] = (b&248)>>3 | (g&56)<<2;
					st->xim->data[i++] = (g&192)>>6 | (r&248)>>1;
				}
	}
	if (st->depth==8)
	{
		for (y=0; y<st->xim->height; y++)
			for (x=0; x<st->xim->width; x++)
			{
				r = st->palaka2[j++];
				g = st->palaka2[j++];
				b = st->palaka2[j++];
				j++;
				st->xim->data[i++] = (((7*g)/256)*36)+(((6*r)/256)*6)+((6*b)/256);
			}
	}
	XPutImage(st->dpy,st->window,st->gc,st->xim,0,0,0,0,st->xim->width,st->xim->height);
}

static void *
fireworkx_init (Display *dpy, Window win)
{
	struct state *st = (struct state *) calloc (1, sizeof(*st));
	unsigned int n;
	Visual *vi;
	Colormap cmap;
	Bool writable;
	XWindowAttributes xwa;
	XGCValues gcv;
	firepix *fp;
	fireshell *fs;

	st->dpy = dpy;
	st->window = win;
	st->xim = NULL;
	st->flash_on = 1;
	st->shoot = 0;
	st->width = 0;
	st->height = 0;
	st->max_shell_life = SHELL_LIFE_DEFAULT;
	st->flash_fade = 0.995;
	st->light_map = NULL;
	st->palaka1 = NULL;
	st->palaka2 = NULL;

	st->flash_on       = get_boolean_resource(st->dpy, "flash"   , "Boolean");
	st->shoot          = get_boolean_resource(st->dpy, "shoot"   , "Boolean");
	st->verbose        = get_boolean_resource(st->dpy, "verbose" , "Boolean");
	st->max_shell_life = get_integer_resource(st->dpy, "maxlife" , "Integer");
        /* transition from xscreensaver <= 5.20 */
	if (st->max_shell_life > 100) st->max_shell_life = 100;

	st->delay          = get_integer_resource(st->dpy, "delay"   , "Integer");

	st->max_shell_life = pow(10.0,(st->max_shell_life/50.0)+2.7);
	if(st->max_shell_life < 1000) st->flash_fade = 0.998;

	if(st->verbose)
	{
		printf("Fireworkx %s - Pyrotechnics explosions simulation \n", FWXVERSION);
		printf("Copyright (GPL) 1999-2013 Rony B Chandran <ronybc@gmail.com> \n\n");
		printf("url: http://www.ronybc.com \n\n");
		printf("Life = %u\n", st->max_shell_life);
#ifdef __SSE2__
		printf("Using SSE2 optimization.\n");
#endif
	}

	XGetWindowAttributes(st->dpy,win,&xwa);
	st->depth = xwa.depth;
	vi        = xwa.visual;
	cmap      = xwa.colormap;
	st->bigendian = (ImageByteOrder(st->dpy) == MSBFirst);

	if(st->depth==8)
	{
		st->colors = (XColor *) calloc(sizeof(XColor),st->ncolors+1);
		writable = False;
		make_smooth_colormap(xwa.screen, vi, cmap,
                                     st->colors, &st->ncolors,
		                     False, &writable, True);
	}
	st->gc = XCreateGC(st->dpy, win, 0, &gcv);

	fs = calloc(SHELLCOUNT, sizeof(fireshell));
	fp = calloc(PIXCOUNT * SHELLCOUNT, sizeof(firepix));
	st->fireshell_array = fs;

	XGetWindowAttributes (st->dpy, st->window, &xwa);
	st->depth = xwa.depth;

	resize(st);   /* initialize palakas */

	for (n = 0; n < SHELLCOUNT; n++, fs++)
	{
		fs->seq_number = n;
		fs->fpix = fp;
		recycle (st, fs, rnd(st->width), rnd(st->height));
		fp += PIXCOUNT;
	}

	return st;
}

static unsigned long
fireworkx_draw (Display *dpy, Window win, void *closure)
{
	struct state *st = (struct state *) closure;
	fireshell *fs;
	unsigned int n, q;
	for (q = FTWEAK; q; q--)
	{
		fs = st->fireshell_array;
		for (n = 0; n < SHELLCOUNT; n++, fs++)
		{
			if (!explode(st, fs))
			{
				if (st->button_down_p)
                                  st->deferred++;
                                else
				  recycle(st, fs, rnd(st->width), rnd(st->height));
			}
		}
	}

        while (!st->button_down_p && st->deferred) {
          st->deferred--;
          recycle_oldest(st, rnd(st->width), rnd(st->height));
        }

	glow_blur(st);

	if (st->flash_on)
	{
		chromo_2x2_light(st);
	}

	put_image(st);
	return st->delay;
}

static void
fireworkx_reshape (Display *dpy, Window window, void *closure,
                   unsigned int w, unsigned int h)
{
	struct state *st = (struct state *) closure;
	st->width  = w;
	st->height = h;
	resize(st);
}

static Bool
fireworkx_event (Display *dpy, Window window, void *closure, XEvent *event)
{
	struct state *st = (struct state *) closure;
	if (event->type == ButtonPress)
	{
		recycle_oldest(st, event->xbutton.x, event->xbutton.y);
		st->button_down_p = True;
		return True;
	}
	else if (event->type == ButtonRelease)
	{
		st->button_down_p = False;
		return True;
	}

	return False;
}

static void
fireworkx_free (Display *dpy, Window window, void *closure)
{
	struct state *st = (struct state *) closure;
	free(st->mem2);
	free(st->mem1);
	free(st->fireshell_array->fpix);
	free(st->fireshell_array);
        if (st->xim) {
          st->xim->data = NULL;
          XDestroyImage (st->xim);
        }
	if (st->light_map) free(st->light_map);
        if (st->colors) free (st->colors);
        XFreeGC (dpy, st->gc);
        free(st);
}

static const char *fireworkx_defaults [] =
{
	".background: black",
	".foreground: white",
	"*delay: 10000",  /* never default to zero! */
	"*maxlife: 32",
	"*flash: True",
	"*shoot: False",
	"*verbose: False",
        ".lowrez: true",  /* Too slow on macOS Retina screens otherwise */
	0
};

static XrmOptionDescRec fireworkx_options [] =
{
	{ "-delay", ".delay", XrmoptionSepArg, 0 },
	{ "-maxlife", ".maxlife", XrmoptionSepArg, 0 },
	{ "-no-flash", ".flash", XrmoptionNoArg, "False" },
	{ "-shoot", ".shoot", XrmoptionNoArg, "True" },
	{ "-verbose", ".verbose", XrmoptionNoArg, "True" },
	{ 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Fireworkx", fireworkx)
