/* -*- Mode: C; tab-width: 4 -*- */
/* glhanoi, Copyright (c) 2005, 2009 Dave Atkinson <da@davea.org.uk>
 * except noise function code Copyright (c) 2002 Ken Perlin
 * Modified by Lars Huttar (c) 2010, to generalize to 4 or more poles
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <assert.h>

#include "rotator.h"

#define DEF_LIGHT     "True"
#define DEF_FOG       "False"
#define DEF_TEXTURE   "True"
#define DEF_POLES     "0"   /* choose random value */
#define DEF_SPEED	  "1"

#define DEFAULTS "*delay:     15000\n" \
				 "*count:     0\n" \
				 "*showFPS:   False\n" \
				 "*wireframe: False\n"
				 
# define refresh_glhanoi 0

/* polygon resolution of poles and disks */
#define NSLICE 32
#define NLOOPS 1

/* How long to wait at start and finish. */
#define START_DURATION 1.0
#define FINISH_DURATION 1.0
#define BASE_LENGTH 30.0
#define BOARD_SQUARES 8

#define MAX_CAMERA_RADIUS 250.0
#define MIN_CAMERA_RADIUS 75.0

#define MARBLE_SCALE 1.01

#undef BELLRAND
/* Return a double precision number in [0...n], with bell curve distribution. */
#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

enum {
	MARBLE_TEXURE,
	N_TEXTURES
};

#define MARBLE_TEXTURE_SIZE 256

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include <math.h>
#include "xlockmore.h"

#ifdef USE_GL					/* whole file */

typedef struct timeval glhtime;

static double getTime(void)
{
	struct timeval t;
#ifdef GETTIMEOFDAY_TWO_ARGS
	gettimeofday(&t, NULL);
#else							/* !GETTIMEOFDAY_TWO_ARGS */
	gettimeofday(&t);
#endif							/* !GETTIMEOFDAY_TWO_ARGS */
	return t.tv_sec + t.tv_usec / 1000000.0;
}

typedef enum {
	START,
	MOVE_DISK,
	MOVE_FINISHED,
	FINISHED,
	MONEY_SHOT,
	INVALID = -1
} State;

typedef struct {
	int id;
	GLuint displayList;
	GLfloat position[3];
	GLfloat rotation[3];
	GLfloat color[4];
	GLfloat base0;
	GLfloat base1;
	GLfloat height;
	GLfloat xmin, xmax, ymin;
	GLfloat u1, u2;
	GLfloat t1, t2;
	GLfloat ucostheta, usintheta;
	GLdouble rotAngle;
	GLfloat speed;
    int polys;
} Disk;

typedef struct {
	Disk **data;
	int count;
	int size;
} Pole;

/* A SubProblem is a recursive subdivision of the problem, and means
  "Move nDisks disks from src pole to dst pole, using the poles indicated in 'available'." */
typedef struct {
	int nDisks;
	int src, dst;
	unsigned long available; /* a bitmask of poles that have no smaller disks on them */
} SubProblem;

typedef struct {
	GLXContext *glx_context;
	State state;
	Bool wire;
	Bool fog;
	Bool light;
	double startTime;
	double lastTime;
	double duration;
	int numberOfDisks;
	int numberOfPoles;
	int numberOfMoves;
	int maxDiskIdx;
	int magicNumber;
	Disk *currentDisk;
	int move;
	/* src, tmp, dst: index of pole that is source / storage / destination for
		current move */
	int src;
	int tmp;
	int dst;
	int oldsrc;
	int oldtmp;
	int olddst;
	SubProblem *solveStack;
	int solveStackSize, solveStackIdx;
	Pole *pole;
	float boardSize;
	float baseLength;
	float baseWidth;
	float baseHeight;
	float poleRadius;
	float poleHeight;
	float poleOffset;
	float diskHeight;
	float *diskPos;				/* pre-computed disk positions on rods */
	Disk *disk;
	GLint floorList;
	GLint baseList;
	GLint poleList;
    int floorpolys, basepolys, polepolys;
	GLfloat camera[3];
	GLfloat centre[3];
	rotator *the_rotator;
	Bool button_down_p;
	Bool texture;
	GLuint textureNames[N_TEXTURES];
	int drag_x;
	int drag_y;
	int noise_initted;
	int p[512];
} glhcfg;

static glhcfg *glhanoi_cfg = NULL;
static Bool fog;
static Bool light;
static Bool texture;
static int poles;
static double speed; /* coefficient for how fast the disks move */

static XrmOptionDescRec opts[] = {
	{"-light", ".glhanoi.light", XrmoptionNoArg, "true"},
	{"+light", ".glhanoi.light", XrmoptionNoArg, "false"},
	{"-fog", ".glhanoi.fog", XrmoptionNoArg, "true"},
	{"+fog", ".glhanoi.fog", XrmoptionNoArg, "false"},
	{"-texture", ".glhanoi.texture", XrmoptionNoArg, "true"},
	{"+texture", ".glhanoi.texture", XrmoptionNoArg, "false"},
	{"-poles", ".glhanoi.poles", XrmoptionSepArg, 0 },
	{"-speed", ".glhanoi.speed", XrmoptionSepArg, 0 }
};

static argtype vars[] = {
	{&light, "light", "Light", DEF_LIGHT, t_Bool},
	{&fog, "fog", "Fog", DEF_FOG, t_Bool},
	{&texture, "texture", "Texture", DEF_TEXTURE, t_Bool},
	{&poles, "poles", "Poles", DEF_POLES, t_Int},
	{&speed, "speed", "Speed", DEF_SPEED, t_Float}
};

static OptionStruct desc[] = {
	{"+/-light", "whether to light the scene"},
	{"+/-fog", "whether to apply fog to the scene"},
	{"+/-texture", "whether to apply texture to the scene"},
	{"-poles r", "number of poles to move disks between"},
	{"-speed s", "speed multiplier"}
};

ENTRYPOINT ModeSpecOpt glhanoi_opts = { countof(opts), opts, countof(vars), vars, desc };

#ifdef USE_MODULES

ModStruct glhanoi_description = {
	"glhanoi", "init_glhanoi", "draw_glhanoi", "release_glhanoi",
	"draw_glhanoi", "init_glhanoi", NULL, &glhanoi_opts,
	1000, 1, 2, 1, 4, 1.0, "",
	"Towers of Hanoi", 0, NULL
};

#endif

static const GLfloat cBlack[] = { 0.0, 0.0, 0.0, 1.0 };
static const GLfloat cWhite[] = { 1.0, 1.0, 1.0, 1.0 };
static const GLfloat poleColor[] = { 0.545, 0.137, 0.137 };
static const GLfloat baseColor[] = { 0.34, 0.34, 0.48 };
static const GLfloat fogcolor[] = { 0.5, 0.5, 0.5 };

static const float left[] = { 1.0, 0.0, 0.0 };
static const float up[] = { 0.0, 1.0, 0.0 };
static const float front[] = { 0.0, 0.0, 1.0 };
static const float right[] = { -1.0, 0.0, 0.0 };
static const float down[] = { 0.0, -1.0, 0.0 };
static const float back[] = { 0.0, 0.0, -1.0 };

static const GLfloat pos0[4] = { 50.0, 50.0, 50.0, 0.0 };
static const GLfloat amb0[4] = { 0.0, 0.0, 0.0, 1.0 };
static const GLfloat dif0[4] = { 1.0, 1.0, 1.0, 1.0 };
static const GLfloat spc0[4] = { 0.0, 1.0, 1.0, 1.0 };

static const GLfloat pos1[4] = { -50.0, 50.0, -50.0, 0.0 };
static const GLfloat amb1[4] = { 0.0, 0.0, 0.0, 1.0 };
static const GLfloat dif1[4] = { 1.0, 1.0, 1.0, 1.0 };
static const GLfloat spc1[4] = { 1.0, 1.0, 1.0, 1.0 };

static float g = 3.0 * 9.80665;		/* hmm, looks like we need more gravity, Scotty... */

static void checkAllocAndExit(Bool item, char *descr) {
	if (!item) {
		fprintf(stderr, "%s: unable to allocate memory for %s\n",
				progname, descr);
		exit(EXIT_FAILURE);
	}
}

#define DOPUSH(X, Y)	(((X)->count) >= ((X)->size)) ? NULL : ((X)->data[(X)->count++] = (Y))
#define DOPOP(X)	(X)->count <= 0 ? NULL : ((X)->data[--((X)->count)])

/* push disk d onto pole idx */
static Disk *push(glhcfg *glhanoi, int idx, Disk * d)
{
	return DOPUSH(&glhanoi->pole[idx], d);
}

/* pop the top disk from pole idx */
static Disk *pop(glhcfg *glhanoi, int idx)
{
	return DOPOP(&glhanoi->pole[idx]);
}

static inline void swap(int *x, int *y)
{
	*x = *x ^ *y;
	*y = *x ^ *y;
	*x = *x ^ *y;
}

/*
 * magic - it's magic...
 *  Return 1 if the number of trailing zeroes on i is even, unless i is 1 or 0.
 */
static int magic(int i)
{
	int count = 0;
	if(i <= 1)
		return 0;
	while((i & 0x01) == 0) {
		i >>= 1;
		count++;
	}
	return count % 2 == 0;
}

#if 0
static float distance(float *p0, float *p1)
{
	float x, y, z;
	x = p1[0] - p0[0];
	y = p1[1] - p0[1];
	z = p1[2] - p0[2];
	return (float)sqrt(x * x + y * y + z * z);
}
#endif

static GLfloat A(GLfloat a, GLfloat b, GLfloat c)
{
	GLfloat sum = a + b;
	return c / (a * b - 0.25 * sum * sum);
}

static void moveSetup(glhcfg *glhanoi, Disk * disk)
{
	float h, ymax;
	float u;
	int src = glhanoi->src;
	int dst = glhanoi->dst;
	GLfloat theta;
	GLfloat sintheta, costheta;
	double absx;

	if(glhanoi->state != FINISHED) {
		double xxx = -180.0 * (dst - src >= 0 ? 1.0 : -1.0);
		if(random() % 6 == 0) {
			disk->rotAngle = xxx * (2 - 2 * random() % 2) * (random() % 3 + 1);
		} else {
			disk->rotAngle = xxx;
		}
		if(random() % 4 == 0) {
			disk->rotAngle = -disk->rotAngle;
		}

	} else {
		disk->rotAngle = -180.0;
	}

	disk->base0 = glhanoi->diskPos[glhanoi->pole[src].count];
	disk->base1 =
		glhanoi->state ==
		FINISHED ? disk->base0 : glhanoi->diskPos[glhanoi->pole[dst].
												  count];

	disk->xmin = glhanoi->poleOffset * (src - (glhanoi->numberOfPoles - 1.0f) * 0.5);
	disk->xmax = glhanoi->poleOffset * (dst - (glhanoi->numberOfPoles - 1.0f) * 0.5);
	disk->ymin = glhanoi->poleHeight;

	absx = fabs(disk->xmax - disk->xmin);
	ymax = glhanoi->poleHeight + absx;
	if(glhanoi->state == FINISHED) {
		ymax += absx * (double)(glhanoi->numberOfDisks - disk->id);
	}
	h = ymax - disk->ymin;
	theta = atan((disk->xmin - disk->xmax) * A(disk->xmin, disk->xmax, h));
	if(theta < 0.0)
		theta += M_PI;
	costheta = cos(theta);
	sintheta = sin(theta);
	u = (float)
		sqrt(fabs
			 (-g /
			  (2.0 * A(disk->xmin, disk->xmax, h) * costheta * costheta)));
	disk->usintheta = u * sintheta;
	disk->ucostheta = u * costheta;
	disk->t1 =
		(-u + sqrt(u * u + 2.0 * g * fabs(disk->ymin - disk->base0))) / g;
	disk->u1 = u + g * disk->t1;
	disk->t2 = 2.0 * disk->usintheta / g;
	disk->u2 = disk->usintheta - g * disk->t2;
}

/* For debugging: show a value as a string of ones and zeroes
static const char *byteToBinary(int x) {
    static char b[9];
	int i, z;

    for (z = 128, i = 0; z > 0; z >>= 1, i++) {
		b[i] = ((x & z) == z) ? '1' : '0';
    }
	b[i] = '\0';

    return b;
}
*/

static void pushMove(glhcfg *glhanoi, int n, int src, int dst, int avail) {
	SubProblem *sp = &(glhanoi->solveStack[glhanoi->solveStackIdx++]);
 
	if (glhanoi->solveStackIdx > glhanoi->solveStackSize) {
		fprintf(stderr, "solveStack overflow: pushed index %d: %d from %d to %d, using %d\n",
		glhanoi->solveStackIdx, n, src, dst, avail);
		exit(EXIT_FAILURE);
	}

	sp->nDisks = n;
	sp->src = src;
	sp->dst = dst;
	sp->available = avail & ~((unsigned long)(1 << src))
		& ~((unsigned long)(1 << dst));
	/*
	fprintf(stderr, "Debug: >  pushed solveStack %d: %d from %d to %d, using %s\n",
		glhanoi->solveStackIdx - 1, n, src, dst, byteToBinary(sp->available));
	*/
}


static Bool solveStackEmpty(glhcfg *glhanoi) {
	return (glhanoi->solveStackIdx < 1);
}

static SubProblem *popMove(glhcfg *glhanoi) {
	SubProblem *sp;
	if (solveStackEmpty(glhanoi)) return (SubProblem *)NULL;
	sp = &(glhanoi->solveStack[--glhanoi->solveStackIdx]);
	/* fprintf(stderr, "Debug:  < popped solveStack %d: %d from %d to %d, using %s\n",
			glhanoi->solveStackIdx, sp->nDisks, sp->src, sp->dst, byteToBinary(sp->available)); */
	return sp;
}

/* Return number of bits set in b */
static int numBits(unsigned long b) {
   int count = 0;
   while (b) {
      count += b & 0x1u;
      b >>= 1;
   }
   return count;
}

/* Return index (power of 2) of least significant 1 bit. */
static int bitScan(unsigned long b) {
	int count;
	for (count = 0; b; count++, b >>= 1) {
		if (b & 0x1u) return count;
	}
	return -1;
}

/* A bit pattern representing all poles */
#define ALL_POLES ((1 << glhanoi->numberOfPoles) - 1)

#define REMOVE_BIT(a, b)  ((a) & ~(1 << (b)))
#define    ADD_BIT(a, b)  ((a) |  (1 << (b)))

static void makeMove(glhcfg *glhanoi)
{
	if (glhanoi->numberOfPoles == 3) {
		int fudge = glhanoi->move + 2;
		int magicNumber = magic(fudge);


		glhanoi->currentDisk = pop(glhanoi, glhanoi->src);
		moveSetup(glhanoi, glhanoi->currentDisk);
		push(glhanoi, glhanoi->dst, glhanoi->currentDisk);
		
		fudge = fudge % 2;

		if(fudge == 1 || magicNumber) {
			swap(&glhanoi->src, &glhanoi->tmp);
		}
		if(fudge == 0 || glhanoi->magicNumber) {
			swap(&glhanoi->dst, &glhanoi->tmp);
		}
		glhanoi->magicNumber = magicNumber;
	} else {
		SubProblem sp;
		int tmp = 0;
		
		if (glhanoi->move == 0) {
			/* Initialize the solution stack. Original problem:
				move all disks from pole 0 to furthest pole,
				using all other poles. */
			pushMove(glhanoi, glhanoi->numberOfDisks, 0,
				glhanoi->numberOfPoles - 1,
				REMOVE_BIT(REMOVE_BIT(ALL_POLES, 0), glhanoi->numberOfPoles - 1));
		}
		
		while (!solveStackEmpty(glhanoi)) {
			int k, numAvail;
			sp = *popMove(glhanoi);

			if (sp.nDisks == 1) {
				/* We have a single, concrete move to do. */
				/* moveSetup uses glhanoi->src, dst. */
				glhanoi->src = sp.src;
				glhanoi->dst = sp.dst;
				glhanoi->tmp = tmp; /* Probably unnecessary */

				glhanoi->currentDisk = pop(glhanoi, sp.src);
				moveSetup(glhanoi, glhanoi->currentDisk);
				push(glhanoi, sp.dst, glhanoi->currentDisk);

				return;
			} else {
				/* Divide and conquer, using Frame-Stewart algorithm, until we get to base case */
				if (sp.nDisks == 1) break;
				
				numAvail = numBits(sp.available);
				if (numAvail < 2) k = sp.nDisks - 1;
				else if(numAvail >= sp.nDisks - 2) k = 1;
				/* heuristic for optimal k: sqrt(2n) (see http://www.cs.wm.edu/~pkstoc/boca.pdf) */
				else k = (int)(sqrt(2 * sp.nDisks));
				
				if (k >= sp.nDisks) k = sp.nDisks - 1;
				else if (k < 1) k = 1;
				
				tmp = bitScan(sp.available);
				/* fprintf(stderr, "Debug: k is %d, tmp is %d\n", k, tmp); */
				if (tmp == -1) {
					fprintf(stderr, "Error: n > 1 (%d) and no poles available\n",
						sp.nDisks);
				}
				
				/* Push on moves in reverse order, since this is a stack. */
				pushMove(glhanoi, k, tmp, sp.dst,
					REMOVE_BIT(ADD_BIT(sp.available, sp.src), tmp));
				pushMove(glhanoi, sp.nDisks - k, sp.src, sp.dst,
					REMOVE_BIT(sp.available, tmp));
				pushMove(glhanoi, k, sp.src, tmp,
					REMOVE_BIT(ADD_BIT(sp.available, sp.dst), tmp));
				
				/* Repeat until we've found a move we can make. */
			}
		}
	}
}

static double lerp(double alpha, double start, double end)
{
	return start + alpha * (end - start);
}

static void upfunc(GLdouble t, Disk * d)
{
	d->position[0] = d->xmin;
	d->position[1] = d->base0 + (d->u1 - 0.5 * g * t) * t;

	d->rotation[1] = 0.0;
}

static void parafunc(GLdouble t, Disk * d)
{
	d->position[0] = d->xmin + d->ucostheta * t;
	d->position[1] = d->ymin + (d->usintheta - 0.5 * g * t) * t;

	d->rotation[1] =
	d->rotAngle * (d->position[0] - d->xmin) / (d->xmax - d->xmin);
}

static void downfunc(GLdouble t, Disk * d)
{
	d->position[0] = d->xmax;
	d->position[1] = d->ymin + (d->u2 - 0.5 * g * t) * t;

	d->rotation[1] = 0.0;
}

/* Return true iff move is finished. */
static Bool computePosition(GLfloat t, Disk * d)
{
	Bool finished = False;

	if(t < d->t1) {
		upfunc(t, d);
	} else if(t < d->t1 + d->t2) {
		parafunc(t - d->t1, d);
	} else {
		downfunc(t - d->t1 - d->t2, d);
		if(d->position[1] <= d->base1) {
			d->position[1] = d->base1;
			finished = True;
		}
	}
	return finished;
}

static void updateView(glhcfg *glhanoi)
{
	double longitude, latitude, radius;
	double a, b, c, A, B;

	get_position(glhanoi->the_rotator, NULL, NULL, &radius,
				 !glhanoi->button_down_p);
	get_rotation(glhanoi->the_rotator, &longitude, &latitude, NULL,
				 !glhanoi->button_down_p);
	longitude += glhanoi->camera[0];
	latitude += glhanoi->camera[1];
	radius += glhanoi->camera[2];
	longitude = longitude - floor(longitude);
	latitude = latitude - floor(latitude);
	radius = radius - floor(radius);
	if(latitude > 0.5) {
		latitude = 1.0 - latitude;
	}
	if(radius > 0.5) {
		radius = 1.0 - radius;
	}

	b = glhanoi->centre[1];
	c = (MIN_CAMERA_RADIUS +
		 radius * (MAX_CAMERA_RADIUS - MIN_CAMERA_RADIUS));
	A = M_PI / 4.0 * (1.0 - latitude);
	a = sqrt(b * b + c * c - 2.0 * b * c * cos(A));
	B = asin(sin(A) * b / a);
	glRotatef(-B * 180 / M_PI, 1.0, 0.0, 0.0);

	glTranslatef(0.0f, 0.0f,
				 -(MIN_CAMERA_RADIUS +
				   radius * (MAX_CAMERA_RADIUS - MIN_CAMERA_RADIUS)));
	glRotatef(longitude * 360.0, 0.0f, 1.0f, 0.0f);
	glRotatef(latitude * 180.0, cos(longitude * 2.0 * M_PI), 0.0,
			  sin(longitude * 2.0 * M_PI));
}

static void changeState(glhcfg *glhanoi, State state)
{
	glhanoi->state = state;
	glhanoi->startTime = getTime();
}

static Bool finishedHanoi(glhcfg *glhanoi) {
	/* use different criteria depending on algorithm */
	return (glhanoi->numberOfPoles == 3 ?
		glhanoi->move >= glhanoi->numberOfMoves :
		solveStackEmpty(glhanoi));
}

static void update_glhanoi(glhcfg *glhanoi)
{
	double t = getTime() - glhanoi->startTime;
	int i;
	Bool done;

	switch (glhanoi->state) {
	case START:
		if(t < glhanoi->duration) {
			break;
		}
		glhanoi->move = 0;
		if(glhanoi->numberOfDisks % 2 == 0) {
			swap(&glhanoi->tmp, &glhanoi->dst);
		}
		glhanoi->magicNumber = 1;
		makeMove(glhanoi);
		changeState(glhanoi, MOVE_DISK);
		break;

	case MOVE_DISK:
		if(computePosition(t * glhanoi->currentDisk->speed, glhanoi->currentDisk)) {
			changeState(glhanoi, MOVE_FINISHED);
		}
		break;

	case MOVE_FINISHED:
		++glhanoi->move;
		if(!finishedHanoi(glhanoi)) {
			makeMove(glhanoi);
			changeState(glhanoi, MOVE_DISK);
		} else {
			glhanoi->duration = FINISH_DURATION;
			changeState(glhanoi, FINISHED);
		}
		break;

	case FINISHED:
		while(t < glhanoi->duration) {
			break;
		}
		glhanoi->src = glhanoi->olddst;
		glhanoi->dst = glhanoi->oldsrc;
		for(i = 0; i < glhanoi->numberOfDisks; ++i) {
			Disk *disk = pop(glhanoi, glhanoi->src);
			assert(disk != NULL);
			moveSetup(glhanoi, disk);
		}
		for(i = glhanoi->maxDiskIdx; i >= 0; --i) {
			push(glhanoi, glhanoi->dst, &glhanoi->disk[i]);
		}
		changeState(glhanoi, MONEY_SHOT);
		break;

	case MONEY_SHOT:
		done = True;
		for(i = glhanoi->maxDiskIdx; i >= 0; --i) {
			double delay = 0.25 * i;
			int finished;

			if(t - delay < 0) {
				done = False;
				continue;
			}

			finished = computePosition(t - delay, &glhanoi->disk[i]);
			glhanoi->disk[i].rotation[1] = 0.0;

			if(!finished) {
				done = False;
			}
		}
		if(done) {
			glhanoi->src = glhanoi->oldsrc;
			glhanoi->tmp = glhanoi->oldtmp;
			glhanoi->dst = glhanoi->olddst;
			changeState(glhanoi, START);
		}
		break;

	case INVALID:
	default:
		fprintf(stderr, "Invalid state\n");
		break;
	}
}

static void HSVtoRGBf(GLfloat h, GLfloat s, GLfloat v,
			   GLfloat * r, GLfloat * g, GLfloat * b)
{
	if(s == 0.0) {
		*r = v;
		*g = v;
		*b = v;
	} else {
		GLfloat i, f, p, q, t;
		if(h >= 360.0) {
			h = 0.0;
		}
		h /= 60.0;				/* h now in [0,6). */
		i = floor((double)h);	/* i now largest integer <= h */
		f = h - i;				/* f is no fractional part of h */
		p = v * (1.0 - s);
		q = v * (1.0 - (s * f));
		t = v * (1.0 - (s * (1.0 - f)));
		switch ((int)i) {
		case 0:
			*r = v;
			*g = t;
			*b = p;
			break;
		case 1:
			*r = q;
			*g = v;
			*b = p;
			break;
		case 2:
			*r = p;
			*g = v;
			*b = t;
			break;
		case 3:
			*r = p;
			*g = q;
			*b = v;
			break;
		case 4:
			*r = t;
			*g = p;
			*b = v;
			break;
		case 5:
			*r = v;
			*g = p;
			*b = q;
			break;
		}
	}
}

static void HSVtoRGBv(GLfloat * hsv, GLfloat * rgb)
{
	HSVtoRGBf(hsv[0], hsv[1], hsv[2], &rgb[0], &rgb[1], &rgb[2]);
}

static void setMaterial(const GLfloat color[3], const GLfloat hlite[3], int shininess)
{
	glColor3fv(color);
	glMaterialfv(GL_FRONT, GL_SPECULAR, hlite);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
	glMateriali(GL_FRONT, GL_SHININESS, shininess);	/* [0,128] */
}

/*
 * drawTube: I know all this stuff is available in gluQuadrics
 * but I'd originally intended to texture the poles with a 3D wood
 * texture, but I was having difficulty getting wood... what? Why
 * are all you Amercians laughing..? Anyway, I don't know if enough
 * people's hardware supports 3D textures, so I didn't bother (xorg
 * ATI server doesn't :-( )
 */
static int drawTube(GLdouble bottomRadius, GLdouble topRadius,
			  GLdouble bottomThickness, GLdouble topThickness,
			  GLdouble height, GLuint nSlice, GLuint nLoop)
{
    int polys = 0;
	GLfloat y;
	GLfloat *cosCache = malloc(sizeof(GLfloat) * nSlice);
	GLfloat *sinCache = malloc(sizeof(GLfloat) * nSlice);
	GLint slice;
	GLuint loop;
	GLint lastSlice = nSlice - 1;
	GLfloat radius;
	GLfloat innerRadius;
	/*GLfloat maxRadius;*/

	if(bottomThickness > bottomRadius) {
		bottomThickness = bottomRadius;
	}
	if(topThickness > topRadius) {
		topThickness = topRadius;
	}
	if(bottomThickness < 0.0) {
		bottomThickness = 0.0;
	}
	if(topThickness < 0.0) {
		topThickness = 0.0;
	}
/*	if(topRadius >= bottomRadius) {
		maxRadius = topRadius;
	} else {
		maxRadius = bottomRadius;
	}*/

	/* bottom */
	y = 0.0;
	radius = bottomRadius;
	innerRadius = bottomRadius - bottomThickness;
	/* innerTexCoordSize = texCoordSize * innerRadius / maxRadius; */
	/* outerTexCoordSize = texCoordSize * radius / maxRadius; */
	/* yTexCoord = minTexCoord; */

	glBegin(GL_QUAD_STRIP);

	glNormal3f(0.0, -1.0, 0.0);

	/* glTexCoord3f(midTexCoord, yTexCoord, midTexCoord + innerTexCoordSize); */
	glVertex3f(0.0, y, innerRadius);

	/* glTexCoord3f(midTexCoord, yTexCoord, midTexCoord + outerTexCoordSize); */
	glVertex3f(0.0, y, radius);

	for(slice = lastSlice; slice >= 0; --slice) {
		GLfloat theta = 2.0 * M_PI * (double)slice / nSlice;

		cosCache[slice] = cos(theta);
		sinCache[slice] = sin(theta);

		/* glTexCoord3f(midTexCoord + sinCache[slice] * innerTexCoordSize, */
		/* yTexCoord, */
		/* midTexCoord + cosCache[slice] * innerTexCoordSize); */
		glVertex3f(innerRadius * sinCache[slice], y,
				   innerRadius * cosCache[slice]);
		/* glTexCoord3f(midTexCoord + sinCache[slice] * outerTexCoordSize, */
		/* yTexCoord, */
		/* midTexCoord + cosCache[slice] * outerTexCoordSize); */
		glVertex3f(radius * sinCache[slice], y, radius * cosCache[slice]);
        polys++;
	}
	glEnd();

	/* middle */
	for(loop = 0; loop < nLoop; ++loop) {
		GLfloat lowerRadius =
			bottomRadius + (topRadius -
							bottomRadius) * (float)loop / (nLoop);
		GLfloat upperRadius =
			bottomRadius + (topRadius - bottomRadius) * (float)(loop +
																1) /
			(nLoop);
		GLfloat lowerY = height * (float)loop / (nLoop);
		GLfloat upperY = height * (float)(loop + 1) / (nLoop);
		GLfloat factor = (topRadius - topThickness) -
			(bottomRadius - bottomThickness);

		/* outside */
		glBegin(GL_QUAD_STRIP);
		for(slice = 0; slice < nSlice; ++slice) {
			glNormal3f(sinCache[slice], 0.0, cosCache[slice]);
			glVertex3f(upperRadius * sinCache[slice], upperY,
					   upperRadius * cosCache[slice]);
			glVertex3f(lowerRadius * sinCache[slice], lowerY,
					   lowerRadius * cosCache[slice]);
            polys++;
		}
		glNormal3f(0.0, 0.0, 1.0);
		glVertex3f(0.0, upperY, upperRadius);
		glVertex3f(0.0, lowerY, lowerRadius);
        polys++;
		glEnd();

		/* inside */
		lowerRadius = bottomRadius - bottomThickness +
			factor * (float)loop / (nLoop);
		upperRadius = bottomRadius - bottomThickness +
			factor * (float)(loop + 1) / (nLoop);

		glBegin(GL_QUAD_STRIP);
		glNormal3f(0.0, 0.0, -1.0);
		glVertex3f(0.0, upperY, upperRadius);
		glVertex3f(0.0, lowerY, lowerRadius);
		for(slice = lastSlice; slice >= 0; --slice) {
			glNormal3f(-sinCache[slice], 0.0, -cosCache[slice]);
			glVertex3f(upperRadius * sinCache[slice], upperY,
					   upperRadius * cosCache[slice]);
			glVertex3f(lowerRadius * sinCache[slice], lowerY,
					   lowerRadius * cosCache[slice]);
            polys++;
		}
		glEnd();
	}

	/* top */
	y = height;
	radius = topRadius;
	innerRadius = topRadius - topThickness;

	glBegin(GL_QUAD_STRIP);
	glNormal3f(0.0, 1.0, 0.0);
	for(slice = 0; slice < nSlice; ++slice) {
		glVertex3f(innerRadius * sinCache[slice], y,
				   innerRadius * cosCache[slice]);

		glVertex3f(radius * sinCache[slice], y, radius * cosCache[slice]);
        polys++;
	}
	glVertex3f(0.0, y, innerRadius);
	glVertex3f(0.0, y, radius);
	glEnd();
    return polys;
}

static int drawPole(GLfloat radius, GLfloat length)
{
  return drawTube(radius, radius, radius, radius, length, NSLICE, NLOOPS);
}

static int drawDisk3D(GLdouble inner_radius, GLdouble outer_radius,
                      GLdouble height)
{
  return drawTube(outer_radius, outer_radius, outer_radius - inner_radius,
                  outer_radius - inner_radius, height, NSLICE, NLOOPS);
}

static int drawCuboid(GLfloat length, GLfloat width, GLfloat height)
{
	GLfloat xmin = -length / 2.0f;
	GLfloat xmax = length / 2.0f;
	GLfloat zmin = -width / 2.0f;
	GLfloat zmax = width / 2.0f;
	GLfloat ymin = 0.0f;
	GLfloat ymax = height;
    int polys = 0;

	glBegin(GL_QUADS);
	/* front */
	glNormal3fv(front);
	glVertex3f(xmin, ymin, zmax);	/* 0 */
	glVertex3f(xmax, ymin, zmax);	/* 1 */
	glVertex3f(xmax, ymax, zmax);	/* 2 */
	glVertex3f(xmin, ymax, zmax);	/* 3 */
    polys++;
	/* right */
	glNormal3fv(right);
	glVertex3f(xmax, ymin, zmax);	/* 1 */
	glVertex3f(xmax, ymin, zmin);	/* 5 */
	glVertex3f(xmax, ymax, zmin);	/* 6 */
	glVertex3f(xmax, ymax, zmax);	/* 2 */
    polys++;
	/* back */
	glNormal3fv(back);
	glVertex3f(xmax, ymin, zmin);	/* 5 */
	glVertex3f(xmin, ymin, zmin);	/* 4 */
	glVertex3f(xmin, ymax, zmin);	/* 7 */
	glVertex3f(xmax, ymax, zmin);	/* 6 */
    polys++;
	/* left */
	glNormal3fv(left);
	glVertex3f(xmin, ymin, zmin);	/* 4 */
	glVertex3f(xmin, ymin, zmax);	/* 0 */
	glVertex3f(xmin, ymax, zmax);	/* 3 */
	glVertex3f(xmin, ymax, zmin);	/* 7 */
    polys++;
	/* top */
	glNormal3fv(up);
	glVertex3f(xmin, ymax, zmax);	/* 3 */
	glVertex3f(xmax, ymax, zmax);	/* 2 */
	glVertex3f(xmax, ymax, zmin);	/* 6 */
	glVertex3f(xmin, ymax, zmin);	/* 7 */
    polys++;
	/* bottom */
	glNormal3fv(down);
	glVertex3f(xmin, ymin, zmin);	/* 4 */
	glVertex3f(xmax, ymin, zmin);	/* 5 */
	glVertex3f(xmax, ymin, zmax);	/* 1 */
	glVertex3f(xmin, ymin, zmax);	/* 0 */
    polys++;
	glEnd();
    return polys;
}

static int drawDisks(glhcfg *glhanoi)
{
	int i;
    int polys = 0;

	glPushMatrix();
	glTranslatef(0.0f, glhanoi->baseHeight, 0.0f);
	for(i = glhanoi->maxDiskIdx; i >= 0; i--) {
		Disk *disk = &glhanoi->disk[i];
		GLfloat *pos = disk->position;
		GLfloat *rot = disk->rotation;

		glPushMatrix();
		glTranslatef(pos[0], pos[1], pos[2]);
		if(rot[1] != 0.0) {
			glTranslatef(0.0, glhanoi->diskHeight / 2.0, 0.0);
			glRotatef(rot[1], 0.0, 0.0, 1.0);
			glTranslatef(0.0, -glhanoi->diskHeight / 2.0, 0.0);
		}
		glCallList(disk->displayList);
        polys += disk->polys;
		glPopMatrix();
	}
	glPopMatrix();
    return polys;
}

static GLfloat getDiskRadius(glhcfg *glhanoi, int i)
{
	return ((GLfloat) i + 3.0) * glhanoi->baseLength /
		(2 * 0.95 * glhanoi->numberOfPoles * (glhanoi->numberOfDisks + 3.0));
}

static void initData(glhcfg *glhanoi)
{
	GLfloat maxDiskRadius;
	int i;

	glhanoi->baseLength = BASE_LENGTH;
	maxDiskRadius = getDiskRadius(glhanoi, glhanoi->numberOfDisks);
	glhanoi->poleRadius = maxDiskRadius / (glhanoi->numberOfDisks + 3.0);
	/* fprintf(stderr, "Debug: baseL = %f, maxDiskR = %f, poleR = %f\n",
		glhanoi->baseLength, maxDiskRadius, glhanoi->poleRadius); */
	glhanoi->baseWidth = 2.0 * maxDiskRadius;
	glhanoi->poleOffset = 2.0 * getDiskRadius(glhanoi, glhanoi->maxDiskIdx);
	glhanoi->diskHeight = 2.0 * glhanoi->poleRadius;
	glhanoi->baseHeight = 2.0 * glhanoi->poleRadius;
	glhanoi->poleHeight = glhanoi->numberOfDisks *
		glhanoi->diskHeight + glhanoi->poleRadius;
	/* numberOfMoves only applies if numberOfPoles = 3 */
	glhanoi->numberOfMoves = (1 << glhanoi->numberOfDisks) - 1;
	glhanoi->boardSize = glhanoi->baseLength * 0.5 * (1.0 + sqrt(5.0));

	glhanoi->pole = (Pole *)calloc(glhanoi->numberOfPoles, sizeof(Pole));
	checkAllocAndExit(!!glhanoi->pole, "poles");

	for(i = 0; i < glhanoi->numberOfPoles; i++) {
		checkAllocAndExit(
			!!(glhanoi->pole[i].data = calloc(glhanoi->numberOfDisks, sizeof(Disk *))),
			"disk stack");
		glhanoi->pole[i].size = glhanoi->numberOfDisks;
	}
	checkAllocAndExit(
			!!(glhanoi->diskPos = calloc(glhanoi->numberOfDisks, sizeof(double))),
			"diskPos");

	glhanoi->the_rotator = make_rotator(0.1, 0.025, 0, 1, 0.005, False);
	glhanoi->button_down_p = False;

	glhanoi->src = glhanoi->oldsrc = 0;
	glhanoi->tmp = glhanoi->oldtmp = 1;
	glhanoi->dst = glhanoi->olddst = glhanoi->numberOfPoles - 1;
	
	if (glhanoi->numberOfPoles > 3) {
		glhanoi->solveStackSize = glhanoi->numberOfDisks + 2;
		glhanoi->solveStack = (SubProblem *)calloc(glhanoi->solveStackSize, sizeof(SubProblem));
		checkAllocAndExit(!!glhanoi->solveStack, "solving stack");
		glhanoi->solveStackIdx = 0;
	}
}

static void initView(glhcfg *glhanoi)
{
	glhanoi->camera[0] = 0.0;
	glhanoi->camera[1] = 0.0;
	glhanoi->camera[2] = 0.0;
	glhanoi->centre[0] = 0.0;
	glhanoi->centre[1] = glhanoi->poleHeight * 3.0;
	glhanoi->centre[2] = 0.0;
}

/*
 * noise_improved.c - based on ImprovedNoise.java
 * JAVA REFERENCE IMPLEMENTATION OF IMPROVED NOISE - COPYRIGHT 2002 KEN PERLIN.
 */
static double fade(double t)
{
	return t * t * t * (t * (t * 6 - 15) + 10);
}

static double grad(int hash, double x, double y, double z)
{
	int h = hash & 15;			/* CONVERT LO 4 BITS OF HASH CODE */
	double u = h < 8 ? x : y,	/* INTO 12 GRADIENT DIRECTIONS. */
		v = h < 4 ? y : h == 12 || h == 14 ? x : z;
	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

static const int permutation[] = { 151, 160, 137, 91, 90, 15,
	131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142,
	8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26,
	197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237,
	149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71,
	134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60,
	211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143,
	54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89,
	18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198,
	173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118,
	126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189,
	28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70,
	221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108,
	110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251,
	34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145,
	235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184,
	84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205,
	93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61,
	156, 180
};

static void initNoise(glhcfg *glhanoi)
{
	int i;
	for(i = 0; i < 256; i++)
		glhanoi->p[256 + i] = glhanoi->p[i] = permutation[i];
}

static double improved_noise(glhcfg *glhanoi, double x, double y, double z)
{
	double u, v, w;
	int A, AA, AB, B, BA, BB;
	int X = (int)floor(x) & 255,	/* FIND UNIT CUBE THAT */
		Y = (int)floor(y) & 255,	/* CONTAINS POINT. */
		Z = (int)floor(z) & 255;
	if(!glhanoi->noise_initted) {
		initNoise(glhanoi);
		glhanoi->noise_initted = 1;
	}
	x -= floor(x);				/* FIND RELATIVE X,Y,Z */
	y -= floor(y);				/* OF POINT IN CUBE. */
	z -= floor(z);
	u  = fade(x),				/* COMPUTE FADE CURVES */
	v  = fade(y),				/* FOR EACH OF X,Y,Z. */
	w  = fade(z);
	A  = glhanoi->p[X] + Y;
	AA = glhanoi->p[A] + Z;
	AB = glhanoi->p[A + 1] + Z,	/* HASH COORDINATES OF */
	B  = glhanoi->p[X + 1] + Y;
	BA = glhanoi->p[B] + Z;
	BB = glhanoi->p[B + 1] + Z;	/* THE 8 CUBE CORNERS, */
	return lerp(w, lerp(v, lerp(u, grad(glhanoi->p[AA], x, y, z),/* AND ADD */
								grad(glhanoi->p[BA], x - 1, y, z)),/* BLENDED */
						lerp(u, grad(glhanoi->p[AB], x, y - 1, z),/* RESULTS */
							 grad(glhanoi->p[BB], x - 1, y - 1, z))),/* FROM 8 CORNERS */
				lerp(v, lerp(u, grad(glhanoi->p[AA + 1], x, y, z - 1), grad(glhanoi->p[BA + 1], x - 1, y, z - 1)),	/* OF CUBE */
					 lerp(u, grad(glhanoi->p[AB + 1], x, y - 1, z - 1),
						  grad(glhanoi->p[BB + 1], x - 1, y - 1, z - 1))));
}

/*
 * end noise_improved.c - based on ImprovedNoise.java
 */

struct tex_col_t {
	GLuint *colours;
	/* GLfloat *points; */
	unsigned int ncols;
};
typedef struct tex_col_t tex_col_t;

static GLubyte *makeTexture(glhcfg *glhanoi, int x_size, int y_size, int z_size,
					 GLuint(*texFunc) (glhcfg *, double, double, double,
									   tex_col_t *), tex_col_t * colours)
{
	int i, j, k;
	GLubyte *textureData;
	GLuint *texturePtr;
	double x, y, z;
	double xi, yi, zi;

	if((textureData =
		calloc(x_size * y_size * z_size, sizeof(GLuint))) == NULL) {
		return NULL;
	}

	xi = 1.0 / x_size;
	yi = 1.0 / y_size;
	zi = 1.0 / z_size;

	z = 0.0;
	texturePtr = (void *)textureData;
	for(k = 0; k < z_size; k++, z += zi) {
		y = 0.0;
		for(j = 0; j < y_size; j++, y += yi) {
			x = 0.0;
			for(i = 0; i < x_size; i++, x += xi) {
				*texturePtr = texFunc(glhanoi, x, y, z, colours);
				++texturePtr;
			}
		}
	}
	return textureData;
}

static void freeTexCols(tex_col_t*p)
{
	free(p->colours);
	free(p);
}

static tex_col_t *makeMarbleColours(void)
{
	tex_col_t *marbleColours;
	int ncols = 2;

	marbleColours = malloc(sizeof(tex_col_t));
	if(marbleColours == NULL) return NULL;
	marbleColours->colours = calloc(sizeof(GLuint), ncols);
	if(marbleColours->colours == NULL) return NULL;
	marbleColours->ncols = ncols;

	marbleColours->colours[0] = 0x3f3f3f3f;
	marbleColours->colours[1] = 0xffffffff;

	return marbleColours;
}

static double turb(glhcfg *glhanoi, double x, double y, double z, int octaves)
{
	int oct, freq = 1;
	double r = 0.0;

	for(oct = 0; oct < octaves; ++oct) {
		r += fabs(improved_noise(glhanoi, freq * x, freq * y, freq * z)) / freq;
		freq <<= 1;
	}
	return r / 2.0;
}

static void perturb(glhcfg *glhanoi, double *x, double *y, double *z, double scale)
{
	double t = scale * turb(glhanoi, *x, *y, *z, 4);
	*x += t;
	*y += t;
	*z += t;
}

static double f_m(double x, double y, double z)
{
	return sin(3.0 * M_PI * x);
}

static GLuint C_m(double x, const tex_col_t * tex_cols)
{
	int r = tex_cols->colours[0] & 0xff;
	int g = tex_cols->colours[0] >> 8 & 0xff;
	int b = tex_cols->colours[0] >> 16 & 0xff;
	double factor;
	int r1, g1, b1;
	x = x - floor(x);

	factor = (1.0 + sin(2.0 * M_PI * x)) / 2.0;

	r1 = (tex_cols->colours[1] & 0xff);
	g1 = (tex_cols->colours[1] >> 8 & 0xff);
	b1 = (tex_cols->colours[1] >> 16 & 0xff);

	r += (int)(factor * (r1 - r));
	g += (int)(factor * (g1 - g));
	b += (int)(factor * (b1 - b));

	return 0xff000000 | (b << 16) | (g << 8) | r;
}


static GLuint makeMarbleTexture(glhcfg *glhanoi, double x, double y, double z, tex_col_t * colours)
{
	perturb(glhanoi, &x, &y, &z, MARBLE_SCALE);
	return C_m(f_m(x, y, z), colours);
}

static void setTexture(glhcfg *glhanoi, int n)
{
	glBindTexture(GL_TEXTURE_2D, glhanoi->textureNames[n]);
}

/* returns 1 on failure, 0 on success */
static int makeTextures(glhcfg *glhanoi)
{
	GLubyte *marbleTexture;
	tex_col_t *marbleColours;

	glGenTextures(N_TEXTURES, glhanoi->textureNames);

	if((marbleColours = makeMarbleColours()) == NULL) {
		return 1;
	}
	if((marbleTexture =
		makeTexture(glhanoi, MARBLE_TEXTURE_SIZE, MARBLE_TEXTURE_SIZE, 1,
					makeMarbleTexture, marbleColours)) == NULL) {
		return 1;
	}

	glBindTexture(GL_TEXTURE_2D, glhanoi->textureNames[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
				 MARBLE_TEXTURE_SIZE, MARBLE_TEXTURE_SIZE, 0,
				 GL_RGBA, GL_UNSIGNED_BYTE, marbleTexture);
	free(marbleTexture);
	freeTexCols(marbleColours);

	return 0;
}

static void initFloor(glhcfg *glhanoi)
{
	int i, j;
	float tileSize = glhanoi->boardSize / BOARD_SQUARES;
	float x0, x1, z0, z1;
	float tx0, tx1, tz0, tz1;
	const float *col = cWhite;
	float texIncr = 1.0 / BOARD_SQUARES;

    glhanoi->floorpolys = 0;
	checkAllocAndExit(!!(glhanoi->floorList = glGenLists(1)), "floor display list");
	glNewList(glhanoi->floorList, GL_COMPILE);
	x0 = -glhanoi->boardSize / 2.0;
	tx0 = 0.0f;
	setMaterial(col, cWhite, 128);
	setTexture(glhanoi, 0);
	glNormal3fv(up);
	for(i = 0; i < BOARD_SQUARES; i++, x0 += tileSize, tx0 += texIncr) {
		x1 = x0 + tileSize;
		tx1 = tx0 + texIncr;
		z0 = -glhanoi->boardSize / 2.0;
		tz0 = 0.0f;
		for(j = 0; j < BOARD_SQUARES; j++, z0 += tileSize, tz0 += texIncr) {
			int colIndex = (i + j) & 0x1;

			z1 = z0 + tileSize;
			tz1 = tz0 + texIncr;

			if(colIndex)
				col = cWhite;
			else
				col = cBlack;

			setMaterial(col, cWhite, 100);

			glBegin(GL_QUADS);

			glTexCoord2d(tx0, tz0);
			glVertex3f(x0, 0.0, z0);

			glTexCoord2d(tx0, tz1);
			glVertex3f(x0, 0.0, z1);

			glTexCoord2d(tx1, tz1);
			glVertex3f(x1, 0.0, z1);

			glTexCoord2d(tx1, tz0);
			glVertex3f(x1, 0.0, z0);
            glhanoi->floorpolys++;
			glEnd();
		}
	}
	glEndList();
}

static void initTowers(glhcfg *glhanoi)
{
	int i;
	
	checkAllocAndExit(!!(glhanoi->baseList = glGenLists(1)), "tower bases display list");

	glNewList(glhanoi->baseList, GL_COMPILE);
	setMaterial(baseColor, cWhite, 50);
    glhanoi->basepolys = drawCuboid(glhanoi->baseLength, glhanoi->baseWidth,
                                    glhanoi->baseHeight);
	glEndList();

	checkAllocAndExit(!!(glhanoi->poleList = glGenLists(1)), "poles display list\n");

	glNewList(glhanoi->poleList, GL_COMPILE);
	glPushMatrix();
	glTranslatef(-glhanoi->poleOffset * (glhanoi->numberOfPoles - 1.0f) * 0.5f, glhanoi->baseHeight, 0.0f);
	setMaterial(poleColor, cWhite, 50);
	for (i = 0; i < glhanoi->numberOfPoles; i++) {		
		glhanoi->polepolys = drawPole(glhanoi->poleRadius, glhanoi->poleHeight);
		/* poleOffset is based on disk radius. */
		glTranslatef(glhanoi->poleOffset, 0.0, 0.0);
	}
	glPopMatrix();
	glEndList();
}

/* Parameterized hue based on input 0.0 - 1.0. */
static double cfunc(double x)
{
#define COMP <
	if(x < 2.0 / 7.0) {
		return (1.0 / 12.0) / (1.0 / 7.0) * x;
	}
	if(x < 3.0 / 7.0) {
		/* (7x - 1) / 6 */
		return (1.0 + 1.0 / 6.0) * x - 1.0 / 6.0;
	}
	if(x < 4.0 / 7.0) {
		return (2.0 + 1.0 / 3.0) * x - 2.0 / 3.0;
	}
	if(x < 5.0 / 7.0) {
		return (1.0 / 12.0) / (1.0 / 7.0) * x + 1.0 / 3.0;
	}
	return (1.0 / 12.0) / (1.0 / 7.0) * x + 1.0 / 3.0;
}

static void initDisks(glhcfg *glhanoi)
{
	int i;
	glhanoi->disk = (Disk *) calloc(glhanoi->numberOfDisks, sizeof(Disk));
	checkAllocAndExit(!!glhanoi->disk, "disks");

	for(i = glhanoi->maxDiskIdx; i >= 0; i--) {
		GLfloat height = (GLfloat) (glhanoi->maxDiskIdx - i);
		double f = cfunc((GLfloat) i / (GLfloat) glhanoi->numberOfDisks);
		GLfloat diskColor = f * 360.0;
		GLfloat color[3];
		Disk *disk = &glhanoi->disk[i];

		disk->id = i;
		disk->position[0] = -glhanoi->poleOffset * (glhanoi->numberOfPoles - 1.0f) * 0.5;
		disk->position[1] = glhanoi->diskHeight * height;
		disk->position[2] = 0.0;
		disk->rotation[0] = 0.0;
		disk->rotation[1] = 0.0;
		disk->rotation[2] = 0.0;
		disk->polys = 0;

		/* make smaller disks move faster */
		disk->speed = lerp(((double)glhanoi->numberOfDisks - i) / glhanoi->numberOfDisks, 1.0, speed);
		/* fprintf(stderr, "disk id: %d, alpha: %0.2f, speed: %0.2f\n", disk->id,
			((double)(glhanoi->maxDiskIdx - i)) / glhanoi->numberOfDisks, disk->speed); */

		color[0] = diskColor;
		color[1] = 1.0f;
		color[2] = 1.0f;
		HSVtoRGBv(color, color);

		checkAllocAndExit(!!(disk->displayList = glGenLists(1)), "disk display list");
		glNewList(disk->displayList, GL_COMPILE);
		setMaterial(color, cWhite, 100.0);
		disk->polys += drawDisk3D(glhanoi->poleRadius, 
                                  getDiskRadius(glhanoi, i),
                                  glhanoi->diskHeight);
		/*fprintf(stderr, "Debug: disk %d has radius %f\n", i,
			getDiskRadius(glhanoi, i)); */
		glEndList();
	}
	for(i = glhanoi->maxDiskIdx; i >= 0; --i) {
		GLfloat height = (GLfloat) (glhanoi->maxDiskIdx - i);
		int h = glhanoi->maxDiskIdx - i;
		glhanoi->diskPos[h] = glhanoi->diskHeight * height;
		push(glhanoi, glhanoi->src, &glhanoi->disk[i]);
	}
}

static void initLights(Bool state)
{
	if(state) {
		glLightfv(GL_LIGHT0, GL_POSITION, pos0);
		glLightfv(GL_LIGHT0, GL_AMBIENT, amb0);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, dif0);
		glLightfv(GL_LIGHT0, GL_SPECULAR, spc0);

		glLightfv(GL_LIGHT1, GL_POSITION, pos1);
		glLightfv(GL_LIGHT1, GL_AMBIENT, amb1);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, dif1);
		glLightfv(GL_LIGHT1, GL_SPECULAR, spc1);

		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);
	} else {
		glDisable(GL_LIGHTING);
	}
}

static int drawFloor(glhcfg *glhanoi)
{
	glCallList(glhanoi->floorList);
    return glhanoi->floorpolys;
}

static int drawTowers(glhcfg *glhanoi)
{
	glCallList(glhanoi->baseList);
	glCallList(glhanoi->poleList);
    return glhanoi->basepolys + glhanoi->polepolys;
}

/* Window management, etc
 */
ENTRYPOINT void reshape_glhanoi(ModeInfo * mi, int width, int height)
{
	glViewport(0, 0, (GLint) width, (GLint) height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(30.0, (GLdouble) width / (GLdouble) height, 1.0,
				   2 * MAX_CAMERA_RADIUS);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT void init_glhanoi(ModeInfo * mi)
{
	glhcfg *glhanoi;
	if(!glhanoi_cfg) {
		glhanoi_cfg =
			(glhcfg *) calloc(MI_NUM_SCREENS(mi), sizeof(glhcfg));
		checkAllocAndExit(!!glhanoi_cfg, "configuration");
	}

	glhanoi = &glhanoi_cfg[MI_SCREEN(mi)];
	glhanoi->glx_context = init_GL(mi);
	glhanoi->numberOfDisks = MI_BATCHCOUNT(mi);
	
    if (glhanoi->numberOfDisks <= 1)
      glhanoi->numberOfDisks = 3 + (int) BELLRAND(9);

	/* magicnumber is a bitfield, so we can't have more than 31 discs
	   on a system with 4-byte ints. */
	if (glhanoi->numberOfDisks >= 8 * sizeof(int))
		glhanoi->numberOfDisks = (8 * sizeof(int)) - 1;

	glhanoi->maxDiskIdx = glhanoi->numberOfDisks - 1;

	speed = get_float_resource(MI_DISPLAY(mi), "speed", "float");
	/* fprintf(stderr, "speed: %0.2f\n", speed); */
	
	glhanoi->numberOfPoles = get_integer_resource(MI_DISPLAY(mi), "poles", "Integer");
	/* Set a number of poles from 3 to numberOfDisks + 1, biased toward lower values,
		with probability decreasing linearly. */
    if (glhanoi->numberOfPoles <= 2)
      glhanoi->numberOfPoles = 3 +
		(int)((1 - sqrt(frand(1.0))) * (glhanoi->numberOfDisks - 1));
	/* fprintf(stderr, "Debug: Num disks: %d; num poles: %d\n", glhanoi->numberOfDisks, glhanoi->numberOfPoles); */
	
	glhanoi->wire = MI_IS_WIREFRAME(mi);
	glhanoi->light = light;
	glhanoi->fog = fog;
	glhanoi->texture = texture;

	reshape_glhanoi(mi, MI_WIDTH(mi), MI_HEIGHT(mi));

	if(glhanoi->wire) {
		glhanoi->light = False;
		glhanoi->fog = False;
		glhanoi->texture = False;
	}

	initLights(!glhanoi->wire && glhanoi->light);
	checkAllocAndExit(!makeTextures(glhanoi), "textures\n");

	initData(glhanoi);
	initView(glhanoi);
	initFloor(glhanoi);
	initTowers(glhanoi);
	initDisks(glhanoi);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	if(glhanoi->fog) {
		glClearColor(fogcolor[0], fogcolor[1], fogcolor[2], 1.0);
		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogfv(GL_FOG_COLOR, fogcolor);
		glFogf(GL_FOG_DENSITY, 0.35f);
		glHint(GL_FOG_HINT, GL_NICEST);
		glFogf(GL_FOG_START, MIN_CAMERA_RADIUS);
		glFogf(GL_FOG_END, MAX_CAMERA_RADIUS / 1.9);
		glEnable(GL_FOG);
	}

	glhanoi->duration = START_DURATION;
	changeState(glhanoi, START);
}

ENTRYPOINT void draw_glhanoi(ModeInfo * mi)
{
	glhcfg *glhanoi = &glhanoi_cfg[MI_SCREEN(mi)];
	Display *dpy = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);

	if(!glhanoi->glx_context)
		return;

    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(glhanoi->glx_context));

	glPolygonMode(GL_FRONT, glhanoi->wire ? GL_LINE : GL_FILL);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    mi->polygon_count = 0;

	glLoadIdentity();

	update_glhanoi(glhanoi);
	updateView(glhanoi);

	if(!glhanoi->wire && glhanoi->texture) {
		glEnable(GL_TEXTURE_2D);
	}
    mi->polygon_count += drawFloor(glhanoi);
	glDisable(GL_TEXTURE_2D);

	mi->polygon_count += drawTowers(glhanoi);
	mi->polygon_count += drawDisks(glhanoi);

	if(mi->fps_p) {
		do_fps(mi);
	}
	glFinish();

	glXSwapBuffers(dpy, window);
}

ENTRYPOINT Bool glhanoi_handle_event(ModeInfo * mi, XEvent * event)
{
	glhcfg *glhanoi = &glhanoi_cfg[MI_SCREEN(mi)];

	if(event->xany.type == ButtonPress && event->xbutton.button == Button1) {
		glhanoi->button_down_p = True;
		glhanoi->drag_x = event->xbutton.x;
		glhanoi->drag_y = event->xbutton.y;
		return True;
	} else if(event->xany.type == ButtonRelease
			  && event->xbutton.button == Button1) {
		glhanoi->button_down_p = False;
		return True;
	} else if(event->xany.type == ButtonPress &&
			  (event->xbutton.button == Button4
			   || event->xbutton.button == Button5)) {
		switch (event->xbutton.button) {
		case Button4:
			glhanoi->camera[2] += 0.01;
			break;
		case Button5:
			glhanoi->camera[2] -= 0.01;
			break;
		default:
			fprintf(stderr,
					"glhanoi: unknown button in mousewheel handler\n");
		}
		return True;
	} else if(event->xany.type == MotionNotify
			  && glhanoi_cfg->button_down_p) {
		int x_diff, y_diff;

		x_diff = event->xbutton.x - glhanoi->drag_x;
		y_diff = event->xbutton.y - glhanoi->drag_y;

		glhanoi->camera[0] = (float)x_diff / (float)MI_WIDTH(mi);
		glhanoi->camera[1] = (float)y_diff / (float)MI_HEIGHT(mi);

		return True;
	}
	return False;
}

ENTRYPOINT void release_glhanoi(ModeInfo * mi)
{
	if(glhanoi_cfg != NULL) {
		int screen;
		for(screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			int i;
			int j;
			glhcfg *glh = &glhanoi_cfg[screen];
			glDeleteLists(glh->floorList, 1);
			glDeleteLists(glh->baseList, 1);
			glDeleteLists(glh->poleList, 1);
                        glDeleteLists(glh->textureNames[0], 2);
			for(j = 0; j < glh->numberOfDisks; ++j) {
				glDeleteLists(glh->disk[j].displayList, 1);
			}
			free(glh->disk);
			for(i = 0; i < glh->numberOfPoles; i++) {
				if(glh->pole[i].data != NULL) {
					free(glh->pole[i].data);
				}
			}
		}
	}
	free(glhanoi_cfg);
	glhanoi_cfg = NULL;
}

XSCREENSAVER_MODULE ("GLHanoi", glhanoi)

#endif							/* USE_GL */
