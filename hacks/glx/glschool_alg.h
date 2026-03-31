/* glschool_alg.h, Copyright (c) 2005-2006 David C. Lambert <dcl@panix.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */
#ifndef __GLSCHOOL_ALG_H__
#define __GLSCHOOL_ALG_H__

typedef struct {
	double	mins[3];
	double	maxs[3];
} BBox;

#define BBOX_XMIN(b)		((b)->mins[0])
#define BBOX_YMIN(b)		((b)->mins[1])
#define BBOX_ZMIN(b)		((b)->mins[2])
#define BBOX_MINS(b)		((b)->mins)
#define BBOX_IMIN(b, i)		((b)->mins[(i)])

#define BBOX_XMAX(b)		((b)->maxs[0])
#define BBOX_YMAX(b)		((b)->maxs[1])
#define BBOX_ZMAX(b)		((b)->maxs[2])
#define BBOX_MAXS(b)		((b)->maxs)
#define BBOX_IMAX(b, i)		((b)->maxs[(i)])

#define BBOX_IMID(b, i)		(((b)->maxs[(i)] + (b)->mins[(i)])/2.0)
#define BBOX_IRANGE(b, i)	((b)->maxs[(i)] - (b)->mins[(i)])

typedef struct {
	double			pos[3];
	double			vel[3];
	double			accel[3];
	double			oldVel[3];
	double			magic[3];
	double			avgVel[3];
} Fish;

#define FISH_POS(f)			((f)->pos)
#define FISH_X(f)			((f)->pos[0])
#define FISH_Y(f)			((f)->pos[1])
#define FISH_Z(f)			((f)->pos[2])

#define FISH_VEL(f)			((f)->vel)
#define FISH_VX(f)			((f)->vel[0])
#define FISH_VY(f)			((f)->vel[1])
#define FISH_VZ(f)			((f)->vel[2])

#define FISH_ACC(f)			((f)->accel)
#define FISH_MAGIC(f)		((f)->magic)
#define FISH_OLDVEL(f)		((f)->oldVel)
#define FISH_AVGVEL(f)		((f)->avgVel)
#define FISH_IPOS(f, i)		((f)->pos[(i)])
#define FISH_IVEL(f, i)		((f)->vel[(i)])
#define FISH_IACC(f, i)		((f)->accel[(i)])
#define FISH_IMAGIC(f, i)	((f)->magic[(i)])
#define FISH_IOLDVEL(f, i)	((f)->oldVel[(i)])
#define FISH_IAVGVEL(f, i)	((f)->avgVel[(i)])

typedef struct {
	int			nFish;
	double		maxVel;
	double		minVel;
	double		distExp;
	double		momentum;
	double		accLimit;
	double		minRadius;
	double		minRadiusExp;
	double		avoidFact;
	double		matchFact;
	double		centerFact;
	double		targetFact;
	double		distComp;
	double		goal[3];
	double		boxMids[3];
	double		boxRanges[3];
	BBox		theBox;
	Fish		*theFish;
} School;

#define SCHOOL_NFISH(s)			((s)->nFish)
#define SCHOOL_MAXVEL(s)		((s)->maxVel)
#define SCHOOL_MINVEL(s)		((s)->minVel)
#define SCHOOL_DISTEXP(s)		((s)->distExp)
#define SCHOOL_MOMENTUM(s)		((s)->momentum)
#define SCHOOL_ACCLIMIT(s)		((s)->accLimit)
#define SCHOOL_MINRADIUS(s)		((s)->minRadius)
#define SCHOOL_MINRADIUSEXP(s)	((s)->minRadiusExp)
#define SCHOOL_MATCHFACT(s)		((s)->matchFact)
#define SCHOOL_AVOIDFACT(s)		((s)->avoidFact)
#define SCHOOL_CENTERFACT(s)	((s)->centerFact)
#define SCHOOL_TARGETFACT(s)	((s)->targetFact)
#define SCHOOL_DISTCOMP(s)		((s)->distComp)
#define SCHOOL_GOAL(s)			((s)->goal)
#define SCHOOL_IGOAL(s,i)		((s)->goal[(i)])
#define SCHOOL_BBMINS(s)		((s)->bbox.mins)
#define SCHOOL_BBMAXS(s)		((s)->bbox.maxs)
#define SCHOOL_BBMIDS(s)		((s)->boxMids)
#define SCHOOL_IMID(s,i)		((s)->boxMids[(i)])
#define SCHOOL_BBRANGES(s)		((s)->boxRanges)
#define SCHOOL_IRANGE(s,i)		((s)->boxRanges[(i)])
#define SCHOOL_BBOX(s)			((s)->theBox)
#define SCHOOL_FISHES(s)		((s)->theFish)
#define SCHOOL_IFISH(s,i)		((s)->theFish[i])

extern void		glschool_initFishes(School *);
extern void		glschool_initFish(Fish *, double *, double *);

extern void		glschool_applyMovements(School *);
/* extern void		applyFishMovements(Fish *, BBox *, double, double, double); */

extern void		glschool_freeSchool(School *);
extern School		*glschool_initSchool(int, double, double, double, double, double, double, double, double, double, double, double);

extern void		glschool_newGoal(School *);
extern void		glschool_setBBox(School *, double, double, double, double, double, double);

extern void		glschool_computeAccelerations(School *);
extern double		glschool_computeNormalAndThetaToPlusZ(double *, double *);
int			glschool_computeGroupVectors(School *, Fish *, double *, double *, double *);

#endif /* __GLSCHOOL_ALG_H__ */
