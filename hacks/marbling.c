/* marbling, Copyright © 2021-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This generates a random field with Perlin Noise, then permutes it with
 * Fractal Brownian Motion to create images that somewhat resemble clouds,
 * or the striations in marble, depending on the parameters selected and
 * the colors chosen.
 *
 * Perlin Noise, SIGGRAPH 2002:
 *
 *     https://mrl.cs.nyu.edu/~perlin/noise/
 *     https://mrl.cs.nyu.edu/~perlin/paper445.pdf
 *     https://en.wikipedia.org/wiki/Perlin_noise
 *
 * Fractal Brownian Motion:
 *
 *     https://en.wikipedia.org/wiki/Fractional_Brownian_motion
 *     https://thebookofshaders.com/13/
 *     https://www.shadertoy.com/view/Msf3WH
 *
 * These algorithms lend themselves well to SIMD supercomputers, which is to
 * say GPUs.  Ideally, this program would be written in Shader Language, but
 * XScreenSaver still targets OpenGL systems that don't support GLSL, so we
 * are doing the crazy thing here of trying to run this highly parallelizable
 * algorithm on the CPU instead of the GPU.  This sort-of works out because
 * modern CPUs have a fair amount of parallel-computation features on their
 * side of the fence as well.  (Generally speaking, your CPU is a Cray and
 * your GPU is a Connection Machine, except that your phone does not
 * typically require liquid nitrogen cooling and a dedicated power plant).
 *
 * Initial version by jwz; black magic for pthreads and CPU-specific vector
 * ops added by Dave Odell <dmo2118@gmail.com>.  Here be parallel monsters.
 */

#include "screenhack.h"
#include "colors.h"
#include "thread_util.h"
#include "xshm.h"

#if defined __GNUC__ || defined __clang__ || \
  defined __STDC_VERSION__ && __STDC_VERSION__ > 199901L
# define INLINE inline
#else
# define INLINE
#endif

/* Use GCC/Clang's vector SIMD extensions, when possible.
   https://gcc.gnu.org/onlinedocs/gcc/Vector-Extensions.html
 */

#if defined __GNUC__ || defined __clang__
# if defined __x86_64__ || defined __i386__
/* 32-bit x86 doesn't usually have SSE2 enabled in the compiler by default,
   so on that platform this will normally be really slow.

   This doesn't use emmintrin.h/avx2intrin.h because the intrinsics in those
   headers use __m128i or __m256i, which aren't strongly typed, and don't work
   with GCC vector arithmetic.
 */
#  if defined __AVX2__
#   define VSIZE 16
#   define IMUL_HI_OP __builtin_ia32_pmulhw256
#   define MUL_HI_OP __builtin_ia32_pmulhuw256
#  elif defined __SSE2__
#   define VSIZE 8
#   define IMUL_HI_OP __builtin_ia32_pmulhw128
#   define MUL_HI_OP __builtin_ia32_pmulhuw128
#  endif

#  ifdef VSIZE
typedef uint16_t v_uhi __attribute__((vector_size(VSIZE * 2)));
typedef int16_t v_hi __attribute__((vector_size(VSIZE * 2)));

static INLINE v_hi IMUL_HI(v_hi a, v_hi b) { return IMUL_HI_OP(a, b); }
static INLINE v_uhi MUL_HI(v_uhi a, v_uhi b) { return (v_uhi)MUL_HI_OP((v_hi)a, (v_hi)b); }

#  endif
# endif


# if defined __ARM_NEON
#  include <arm_neon.h>
#  define VSIZE 8

/* No unsigned vector multiply on ARM NEON. Closest is vqdmulhq, a.k.a.
   "Vector Saturating Doubling Multiply Returning High Half". (Say that five
   times fast.) The 'doubling' part comes from how in signed multiplication
   the most significant bit always matches the sign bit, so ARM shifts the
   result left by one, preserving an extra bit of precision.

   Which is basically:
   int16_t vqdmulhq_s16(int16_t a, int16_t b)
   {
     return ((int32_t)a * b) >> 15;
   }

   Google has NEON support turned on by default for recent NDK releases.
   Android doesn't require NEON, but it's apparently very uncommon for an ARM
   Android device to not have NEON.

   Apparently iPhones got NEON starting with the 3GS, back in 2009.
 */

typedef uint16x8_t v_uhi;
typedef int16x8_t v_hi;

# endif
#endif

#ifndef VSIZE
# define VSIZE 1
typedef uint16_t v_uhi;
typedef int16_t v_hi;
# define MUL_HI(a, b) (((uint32_t)(uint16_t)(a) * (uint16_t)(b)) >> 16)
# define IMUL_HI(a, b) (((int32_t)(int16_t)(a) * (int16_t)(b)) >> 16)
# define VEC_INDEX(v, i) (v)
#else
# define VEC_INDEX(v, i) ((v)[i])
#endif

struct state {
  Display *dpy;
  Window window;
  XImage *image;
  XShmSegmentInfo shm_info;
  GC gc;
  int delay;
  Colormap cmap;
  int ncolors;
  XColor *colors;
  unsigned int grid_size, w, h;
  int scale, iterations;
  v_uhi Z;
  struct threadpool threadpool;
};


struct thread {
  struct state *st;
  unsigned thread_id;
};


/* Perlin Noise
 */

const unsigned noise_work_bits = 13; /* Max: 13 */

#if __ARM_NEON
const unsigned lerp_loss = 1;
#else
const unsigned lerp_loss = 2; /* Min: 2 */
#endif

/* == 8 for x86 and scalar. (Nice.) */
/* const unsigned noise_out_bits = noise_work_bits - 3 * lerp_loss + 1; */
#define noise_out_bits ((noise_work_bits - 3) * lerp_loss + 1)
const unsigned noise_in_bits = 8;

static INLINE v_uhi
broadcast (int16_t x)
{
#if VSIZE == 1
  v_hi r = x;
#else
  v_hi r = {0};
  r = r + x;
#endif
  return (v_uhi)r;
}

static INLINE v_uhi
fade (v_uhi t)
{
  const uint16_t F = 256;

  /* This whole thing is playing fast and loose with signdedness, mostly
     because __builtin_ia32_pmulhuw256 is an unsigned op that requires signed
     params, and Android's clang doesn't seem to care about signed vs.
     unsigned vector variables.

     The multiplications below should be unsigned, but the result is the same
     either way, because it's getting the low 16 bits (not the high 16).

     All the v_(u)hi variables are vectors of 16-bit fixed-point ints, maybe
     signed, maybe unsigned.  noise_in_bits and noise_out_bits determine how
     many fractional bits are in use for the v_uhi variables that go into/come
     out of noise().  And noise_out_bits is governed by the big expression at
     the end of noise():

     1. Going in, there's noise_work_bits worth of fraction in c0-c7.
     2. The multiply in LERP() (either vqdmulhq_s16 or IMUL_HI) shifts
        lerp_loss bits of fraction off the end of each int16. Because
        LERP() is nested three deep, it's 3 * lerp_loss.
     3. SCALE() shifts things one bit in the other direction.
        Hence, noise_work_bits - 3 * lerp_loss + 1.
   */
#if __ARM_NEON
  v_uhi ut2 = (t * t) >> 1;
  v_hi it2 = (v_hi)ut2;
  v_hi it = (v_hi)t;
  v_hi iret =
    vqdmulhq_s16(it2, 10 * it - vqdmulhq_s16(it2, (int16_t)(15 * F) - it * 6)) <<
    (noise_work_bits - noise_in_bits + 1);
  return (v_uhi)iret;
#else
  v_uhi t2 = t * t;
  return
    MUL_HI(t2, 10 * t - MUL_HI(t2, (uint16_t)(15 * F) - t * 6)) <<
    (noise_work_bits - noise_in_bits + 1);
#endif
}

#ifdef __ARM_NEON
# define LERP(t, a, b) (((a) >> lerp_loss) + vqdmulhq_s16(t, (b) - (a)))
#else
# define LERP(t, a, b) (((a) >> lerp_loss) + IMUL_HI(t, (b) - (a)))
#endif

#define SCALE(n) ((n) + (uint16_t)(1 << (noise_out_bits - 1)))

/* `a & b | ~a & c`, or `a ? b : c` */
#define PICK(a, b, c) ((((b) ^ (c)) & (a)) ^ (c))

static INLINE v_hi
grad (v_uhi hash, v_uhi x, v_uhi y, v_uhi z)
{
  v_uhi h = hash & 15;                  /* CONVERT LO 4 BITS OF HASH CODE */
  v_uhi u, v;                           /* INTO 12 GRADIENT DIRECTIONS. */
#if VSIZE == 1
  u = h<8 ? x : y;
  v = h<4 ? y : ((h & ~2) == 12 ? x : z);
  return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
#else
  /* GCC vector comparisons give 0 or -1 instead of 0 or 1. */
  v_uhi v1;
  u = PICK(h<8, x, y);
  v1 = PICK((h & ~2) == 12, x, z);
  v = PICK(h<4, y, v1);
  return (((h&1) != 0) ^ u) + (((h&2) != 0) ^ v);
#endif
}

/* Perlin's code used pre-computed random numbers. */
static INLINE v_uhi
noise_rand (v_uhi x)
{
  /* An 8-bit minimal perfect hash function. This isn't a good source of
     random numbers, but it's good enough here. Applied to a 16-bit value, the
     least significant bits are identical to the 8-bit version of this if the
     upper 8 bits are 0.
   */
  x ^= x >> 3;
  x ^= x << 1;
  return (x << 5) - x;
}

#define P(x) (noise_rand((x) & 0xff))

static INLINE v_uhi
noise (v_uhi x, v_uhi y, v_uhi z)
{
  const v_uhi one = broadcast(1 << noise_work_bits);
  v_uhi X, Y, Z, A, B, AA, AB, BA, BB;
  v_hi u, v, w;
  v_hi c0, c1, c2, c3, c4, c5, c6, c7;
  X = x >> noise_in_bits;			/* FIND UNIT CUBE THAT */
  Y = y >> noise_in_bits;			/* CONTAINS POINT. */
  Z = z >> noise_in_bits;
  x &= (uint16_t)((1 << noise_in_bits) - 1);	/* FIND RELATIVE X,Y,Z */
  y &= (uint16_t)((1 << noise_in_bits) - 1);	/* OF POINT IN CUBE. */
  z &= (uint16_t)((1 << noise_in_bits) - 1);
  u = (v_hi)fade(x);				/* COMPUTE FADE CURVES */
  v = (v_hi)fade(y);				/* FOR EACH OF X,Y,Z. */
  w = (v_hi)fade(z);

  A = noise_rand(X)+Y, AA = P(A)+Z, AB = P(A+1)+Z; /* HASH COORDINATES OF */
  B = P(X+1)+Y,        BA = P(B)+Z, BB = P(B+1)+Z; /* THE 8 CUBE CORNERS, */
  x <<= noise_work_bits - noise_in_bits;
  y <<= noise_work_bits - noise_in_bits;
  z <<= noise_work_bits - noise_in_bits;
  c0 = grad(P(AA  ), x,     y,     z     );
  c1 = grad(P(BA  ), x-one, y,     z     );
  c2 = grad(P(AB  ), x,     y-one, z     );
  c3 = grad(P(BB  ), x-one, y-one, z     );
  c4 = grad(P(AA+1), x,     y,     z-one );
  c5 = grad(P(BA+1), x-one, y,     z-one );
  c6 = grad(P(AB+1), x,     y-one, z-one );
  c7 = grad(P(BB+1), x-one, y-one, z-one );

  return
  (v_uhi)SCALE(LERP(w, LERP(v, LERP(u, c0, c1),  /* AND ADD BLENDED */
                               LERP(u, c2, c3)), /* RESULTS FROM  8 */
                       LERP(v, LERP(u, c4, c5),  /* CORNERS OF CUBE */
                               LERP(u, c6, c7))));
}


/* Fractal Brownian Motion
 */
static INLINE v_uhi
fbm (v_uhi x, v_uhi y, v_uhi z)
{    
  const float G = exp2f(-0.5);
  int octaves = 2;
  v_uhi f = broadcast(1);
#if __ARM_NEON
  const uint16_t iG = (uint16_t)(G * 0x10000);
  int16_t a = 0x7fff >> (noise_out_bits - noise_in_bits);
#else
  const v_uhi iG = broadcast((int16_t)(G * 0x10000));
  v_uhi a = broadcast((uint16_t)0xffff);
#endif
  v_uhi t = broadcast(0);
  int i;
  for (i = 0; i < octaves; i++)
    {
#if __ARM_NEON
      t += (v_uhi)vqdmulhq_n_s16((v_hi)noise (f*x, f*y, f*z), a);
      a = ((uint32_t)a * iG) >> 16;
#else
      t += MUL_HI(noise (f*x, f*y, f*z), a);
      a = MUL_HI(a, iG);
#endif
      f *= 2;
    }
  return t;
}


static void
marbling_recolor (struct state *st)
{
  XWindowAttributes xgwa;
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->ncolors = 256;
  if (st->colors) free (st->colors);
  st->colors = (XColor *) calloc(st->ncolors, sizeof(*st->colors));
  make_smooth_colormap (xgwa.screen, xgwa.visual, st->cmap,
                        st->colors, &st->ncolors,
                        True, 0, False);
}

static void marbling_thread_run (void *t_raw);

static void
marbling_reset (struct state *st)
{
  XWindowAttributes xgwa;
  unsigned align = thread_memory_alignment (st->dpy) * 8 - 1;
  unsigned bpp;
  unsigned g = st->grid_size;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  bpp = visual_pixmap_depth (xgwa.screen, xgwa.visual);
  if (st->image)
    destroy_xshm_image (st->dpy, st->image, &st->shm_info);
  st->w = ((((xgwa.width + g - 1) / g) + (VSIZE - 1)) & ~(VSIZE - 1));
  st->h = xgwa.height + g - 1;
  st->h = st->h / g;
  st->image = create_xshm_image (st->dpy, xgwa.visual, xgwa.depth, ZPixmap,
                                 &st->shm_info,
                                 ((st->w * g * bpp + align) & ~align) / bpp,
                                 st->h * g);
}


static int
marbling_thread_create (void *t_raw, struct threadpool *threadpool,
                        unsigned int id)
{
  struct thread *t = (struct thread *) t_raw;
  t->st = GET_PARENT_OBJ(struct state, threadpool, threadpool);
  t->thread_id = id;
  return 0;
}


static void
marbling_thread_destroy (void *t_raw)
{
}


static void
marbling_thread_run (void *t_raw)
{
  const struct thread *t = (const struct thread *) t_raw;
  struct state *st = t->st;
  unsigned g = st->grid_size;
  void *scanline = st->image->data +
    st->image->bytes_per_line * t->thread_id * g;
  ptrdiff_t skip = st->image->bytes_per_line * st->threadpool.count * g;
  int i, j, x, y;

  float S = st->scale << noise_in_bits;

  for (y = t->thread_id; y < st->h; y += st->threadpool.count)
    {
      char *scanline1;

      v_uhi Y = broadcast((float) y / st->h * S);

#if VSIZE == 1
      uint32_t X = 0, Xd = 0x10000 / st->w * S;
#else
      v_uhi X, Xd = broadcast((float) VSIZE / st->w * S);
      for (x = 0; x != VSIZE; x++)
        VEC_INDEX(X, x) = (float) x / st->w * S;
#endif

      for (x = 0; x < st->w; x += VSIZE)
        {
          int i;
#if VSIZE == 1
          uint16_t X0 = X >> 16;
#else
          v_uhi X0 = X;
#endif

#if 0
          v_uhi p = noise (X0, Y, st->Z) >> (noise_out_bits - noise_in_bits);
#else
          v_uhi p = broadcast(0);
          for (i = 0; i < st->iterations; i++)
            p = fbm (p+X0, p+Y, p+st->Z);
#endif

          /* Optimizing for 32bpp seems vaguely faster. */
          if (st->image->bits_per_pixel == 32)
            {
              uint32_t *out = (uint32_t *) scanline + x * g;
              for (i = 0; i != VSIZE; ++i)
                {
                  *out =
                    st->colors[((VEC_INDEX(p, i) &
                                 ((1 << noise_in_bits) - 1)) *
                                st->ncolors)
                               >> noise_in_bits].pixel;
                  out += g;
                }

              for (j = 1; j != g; ++j)
                {
                  out = (uint32_t *) scanline + x * g + j;
                  for (i = 0; i != VSIZE; ++i)
                    {
                      out[0] = out[-1];
                      out += g;
                    }
                }
            }
          else
            {
              for (i = 0; i != VSIZE; ++i)
                {
                  int c = st->colors[((VEC_INDEX(p, i) &
                                       ((1 << noise_in_bits) - 1)) *
                                      st->ncolors)
                                     >> noise_in_bits].pixel;
                  for (j = 0; j != g; ++j)
                    XPutPixel (st->image, (x + i) * g + j, y * g, c);
                }
            }

          X += Xd;
        }

      scanline1 = (char *) scanline;
      for (i = 1; i != g; ++i)
        {
          scanline1 += st->image->bytes_per_line;
          memcpy(scanline1, scanline, st->image->bytes_per_line);
        }
      scanline = (uint32_t *)((char *) scanline + skip);
    }
}


static void *
marbling_init (Display *dpy, Window window)
{
  static const struct threadpool_class cls = {
    sizeof (struct thread),
    marbling_thread_create,
    marbling_thread_destroy
  };

  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XWindowAttributes xgwa;
  XGCValues gcv;

  st->dpy = dpy;
  st->window = window;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->grid_size = get_integer_resource(dpy, "gridsize", "Integer");
  st->scale = get_integer_resource(dpy, "gridScale", "Integer");
  st->iterations = get_integer_resource(dpy, "iterations", "Integer");
  if (st->delay < 0) st->delay = 0;
  if (! st->gc)
    st->gc = XCreateGC (st->dpy, st->window, 0, &gcv);

  if (st->grid_size < 1) st->grid_size = 1;
  if (st->scale < 1) st->scale = 1;
  if (st->iterations < 1) st->iterations = 1;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->cmap = xgwa.colormap;
  st->Z = broadcast(0);
  marbling_recolor (st);
  threadpool_create (&st->threadpool, &cls, dpy, hardware_concurrency (dpy));
  marbling_reset (st);
  return st;
}


static unsigned long
marbling_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  threadpool_run (&st->threadpool, marbling_thread_run);
  threadpool_wait (&st->threadpool);
  st->Z += (int16_t)(0.01 * (1 << noise_in_bits));

  put_xshm_image (st->dpy, st->window, st->gc, st->image,
                  0, 0, 0, 0, st->image->width, st->image->height,
                  &st->shm_info);

  return st->delay;
}


static const char *marbling_defaults [] = {
  "*delay:	10000",
  "*background:	black",
  "*gridsize:	2",
  "*gridScale:	10",   /* using "scale" screws up fps fonts */
  "*iterations:	5",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
  THREAD_DEFAULTS
  0
};

static XrmOptionDescRec marbling_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-gridsize",	".gridsize",	XrmoptionSepArg, 0 },
  { "-scale",		".gridScale",	XrmoptionSepArg, 0 },
  { "-iterations",	".iterations",	XrmoptionSepArg, 0 },
  THREAD_OPTIONS
  { 0, 0, 0, 0 }
};

static void
marbling_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  marbling_reset (st);
}

static Bool
marbling_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == '+' || c == '=' || keysym == XK_Up)
        {
          st->scale++;
          return True;
        }
      else if (c == '-' || c == '_' || keysym == XK_Down)
        {
          st->scale--;
          if (st->scale <= 0)
            {
              st->scale = 1;
              return False;
            }
          else
            return True;
        }
      else if (c == '>' || c == '.' || keysym == XK_Right)
        {
          st->iterations++;
          return True;
        }
      else if (c == '<' || c == ',' || keysym == XK_Left)
        {
          st->iterations--;
          if (st->iterations < 1)
            {
              st->iterations = 1;
              return False;
            }
          else
            return True;
        }
    }

  if (screenhack_event_helper (dpy, window, event))
    {
      marbling_recolor (st);
      return True;
    }
  return False;
}

static void
marbling_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (st->dpy, st->gc);
  destroy_xshm_image (st->dpy, st->image, &st->shm_info);
  free_colors (DefaultScreenOfDisplay (st->dpy), st->cmap,
               st->colors, st->ncolors);
  threadpool_destroy (&st->threadpool);
  free (st);
}

XSCREENSAVER_MODULE ("Marbling", marbling)
