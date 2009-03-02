/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)

   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
*/

/* This defines the osc_t object, which generates a stream of
   numbers. It is the heart of the StonerSound/StonerView engine.
    
   The idea is simple; an osc_t represents some function f(), which
   can be evaluated to generate an infinite stream of integers (f(0),
   f(1), f(2), f(3)...). Some of these functions are defined in
   terms of other osc_t functions: f(i) = g(h(i)) or some such thing.
    
   To simplify the code, we don't try to calculate f(i) for any
   arbitrary i. Instead, we start with i=0. Calling osc_get(f)
   returns f(0) for all osc_t's in the system. When we're ready, we
   call osc_increment(), which advances every osc_t to i=1;
   thereafter, calling osc_get(f) returns f(1). When you call
   osc_increment() again, you get f(2). And so on. You can't go
   backwards, or move forwards more than 1 at a time, or move some
   osc_t's without moving others. This is a very restricted model, but
   it's exactly what's needed for this system.
    
   Now, there's an additional complication. To get the rippling
   effect, we don't pull out single values, but *sets* of N elements
   at a time. (N is defined by NUM_ELS below.) So f(i) is really an
   ordered N-tuple (f(i,0), f(i,1), f(i,2), f(i,3), f(i,4)). And f()
   generates an infinite stream of these N-tuples. The osc_get() call
   really has two parameters; you call osc_get(f, n) to find the n'th
   element of the current N-tuple. Remember, n must be between 0 and
   n-1.
    
   (Do *not* try to get an infinite stream f(i) by calling 
   osc_get(f, i) for i ranging to infinity! Use osc_increment() to 
   advance to the next N-tuple in the stream.)
*/

#define NUM_ELS (40) /* Forty polygons at a time. */

#define NUM_PHASES (4) /* Some of the osc functions switch between P 
			  alternatives. We arbitrarily choose P=4. */

/* Here are the functions which are available. 
   Constant: f(i,n) = k. Always the same value. Very simple.
   Wrap: f(i,n) slides up or down as i increases. There's a minimum and maximum
     value, and a step. When the current value reaches the min or max, it jumps
     to the other end and keeps moving the same direction. 
   Bounce: f(i,n) slides up and down as i increases. There's a minimum and
     maximum value, and a step. When the current value reaches the min or max,
     the step is flipped to move the other way.
   Phaser: f(i,n) = floor(i / phaselen) modulo 4. That is, it generates
     phaselen 0 values, and then phaselen 1 values, then phaselen 2 values,
     then phaselen 3 values, then back to 0. (Phaselen is a parameter you
     supply when you create the phaser.) As you see, this is much the same as
     the Wrap function, with a minimum of 0, a maximum of 3, and a step of
     1/phaselen. But since this code uses integer math, fractional steps
     aren't possible; it's easier to write a separate function.
   RandPhaser: The same as Phaser, but the phaselen isn't fixed. It varies
     randomly between a minimum and maximum value you supply.
   Multiplex: There are five subsidiary functions within a multiplex function: 
     g0, g1, g2, g3, and a selector function s. Then:
     f(i,n) = gX(i,n), where X = s(i,n). (Obviously s must generate only values
     in the range 0 to 3. This is what the phaser functions are designed for,
     but you can use anything.)
   Linear: There are two subsidiary functions within this, a and b. Then:
     f(i,n) = a(i,n) + n*b(i,n). This is an easy way to make an N-tuple that
     forms a linear sequence, such as (41, 43, 45, 47, 49).
   Buffer: This takes a subsidiary function g, and computes:
     f(i,n) = g(i-n,0). That is, the 0th element of the N-tuple is the
     *current* value of g; the 1st element is the *previous* value of g; the
     2nd element is the second-to-last value, and so on back in time. This
     is a weird idea, but it causes exactly the rippling-change effect that
     we want.
        
   Note that Buffer only looks up g(i,0) -- it only uses the 0th elements of
     the N-tuples that g generates. This saves time and memory, but it means
     that certain things don't work. For example, if you try to build 
     Buffer(Linear(A,B)), B will have no effect, because Linear computes
     a(i,n) + n*b(i,n), and inside the Buffer, n is always zero. On the other
     hand, Linear(Buffer(A),Buffer(B)) works fine, and is probably what you
     wanted anyway.
   Similarly, Buffer(Buffer(A)) is the same as Buffer(A). Proof left as an
     exercise.
*/

#define otyp_Constant (1)
#define otyp_Bounce (2)
#define otyp_Wrap (3)
#define otyp_Phaser (4)
#define otyp_RandPhaser (5)
#define otyp_VeloWrap (7)
#define otyp_Linear (6)
#define otyp_Buffer (8)
#define otyp_Multiplex (9)

/* The osc_t structure itself. */
typedef struct osc_struct {
  int type; /* An otyp_* constant. */
    
  struct osc_struct *next; /* osc.c uses this to maintain a private linked list
			      of all osc_t objects created. */
    
  /* Union of the data used by all the possible osc_t functions. */
  union {
    struct {
      int val;
    } oconstant;
    struct owrap_struct {
      int min, max, step;
      int val;
    } owrap;
    struct obounce_struct {
      int min, max, step;
      int val;
    } obounce;
    struct omultiplex_struct {
      struct osc_struct *sel;
      struct osc_struct *val[NUM_PHASES];
    } omultiplex;
    struct ophaser_struct {
      int phaselen;
      int count;
      int curphase;
    } ophaser;
    struct orandphaser_struct {
      int minphaselen, maxphaselen;
      int count;
      int curphaselen;
      int curphase;
    } orandphaser;
    struct ovelowrap_struct {
      int min, max;
      struct osc_struct *step;
      int val;
    } ovelowrap;
    struct olinear_struct {
      struct osc_struct *base;
      struct osc_struct *diff;
    } olinear;
    struct obuffer_struct {
      struct osc_struct *val;
      int firstel;
      int el[NUM_ELS];
    } obuffer;
  } u;
} osc_t;

extern osc_t *new_osc_constant(int val);
extern osc_t *new_osc_bounce(int min, int max, int step);
extern osc_t *new_osc_wrap(int min, int max, int step);
extern osc_t *new_osc_phaser(int phaselen);
extern osc_t *new_osc_randphaser(int minphaselen, int maxphaselen);
extern osc_t *new_osc_velowrap(int min, int max, osc_t *step);
extern osc_t *new_osc_linear(osc_t *base, osc_t *diff);
extern osc_t *new_osc_buffer(osc_t *val);
extern osc_t *new_osc_multiplex(osc_t *sel, osc_t *ox0, osc_t *ox1, 
  osc_t *ox2, osc_t *ox3);

extern int osc_get(osc_t *osc, int el);
extern void osc_increment(void);

