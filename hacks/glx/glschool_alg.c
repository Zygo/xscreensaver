/* school_alg.c, Copyright (c) 2005-2006 David C. Lambert <dcl@panix.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "yarandom.h"
#include "glschool_alg.h"

/* for xscreensaver */
#undef drand48
#define drand48()	frand(1.0)

#define RAD2DEG		(180.0/3.1415926535)


static inline double
norm(double *dv)
{
	return sqrt(dv[0]*dv[0] + dv[1]*dv[1] + dv[2]*dv[2]);
}


static inline void
addVector(double *v, double *d)
{
	v[0] += d[0];
	v[1] += d[1];
	v[2] += d[2];
}


static inline void
clearVector(double *v)
{
	v[0] = v[1] = v[2] = 0.0;
}


static inline void
scaleVector(double *v, double s)
{
	v[0] *= s;
	v[1] *= s;
	v[2] *= s;
}


static inline void
addScaledVector(double *v, double *d, double s)
{
	v[0] += d[0]*s;
	v[1] += d[1]*s;
	v[2] += d[2]*s;
}


static inline void
getDifferenceVector(double *v0, double *v1, double *diff)
{
	diff[0] = v0[0] - v1[0];
	diff[1] = v0[1] - v1[1];
	diff[2] = v0[2] - v1[2];
}


void
glschool_initFish(Fish *f, double *mins, double *ranges)
{
	int i;

	for(i = 0; i < 3; i++) {
		FISH_IPOS(f, i) = mins[i] + drand48()*ranges[i];
		FISH_IACC(f, i) = 0.0;
		FISH_IVEL(f, i) = drand48();
		FISH_IMAGIC(f, i) = 0.70 + 0.60*drand48();
		FISH_IOLDVEL(f, i) = 0.0;
	}
}


void
glschool_initFishes(School *s)
{
	int		i;
	Fish	*f = (Fish *)0;
	int		nFish = SCHOOL_NFISH(s);
	BBox	*bbox = &SCHOOL_BBOX(s);
	double	*mins = BBOX_MINS(bbox);
	double	*ranges = SCHOOL_BBRANGES(s);
	Fish	*theFishes = SCHOOL_FISHES(s);

	for(i = 0, f = theFishes; i < nFish; i++, f++)
		glschool_initFish(f, mins, ranges);
}


static void
applyFishMovements(Fish *f, BBox *bbox, double minVel, double maxVel, double momentum)
{
	int			i;
	int			oob = 0;
	double		vMag = 0.0;

	for(i = 0; i < 3; i++) {
		double	pos = FISH_IPOS(f, i);
		oob = (pos > BBOX_IMAX(bbox, i) || pos < BBOX_IMIN(bbox, i));
		if (oob == 0) FISH_IVEL(f, i) += FISH_IACC(f, i) * FISH_IMAGIC(f, i);
		vMag += (FISH_IVEL(f, i) * FISH_IVEL(f, i));
	}
	vMag = sqrt(vMag);

	if (vMag > maxVel)
		scaleVector(FISH_VEL(f), maxVel/vMag);
	else if (vMag < minVel)
		scaleVector(FISH_VEL(f), minVel/vMag);

	for(i = 0; i < 3; i++) {
		FISH_IVEL(f, i) = momentum * FISH_IOLDVEL(f, i) + (1.0-momentum) * FISH_IVEL(f, i);
		FISH_IPOS(f, i) += FISH_IVEL(f, i);
		FISH_IOLDVEL(f, i) = FISH_IVEL(f, i);

		if (FISH_IPOS(f, i) < BBOX_IMIN(bbox, i))
			FISH_IPOS(f, i) = BBOX_IMAX(bbox, i);
		else if (FISH_IPOS(f, i) > BBOX_IMAX(bbox, i))
			FISH_IPOS(f, i) = BBOX_IMIN(bbox, i);
	}
}


void
glschool_applyMovements(School *s)
{
	int		i = 0;
	Fish	*f = (Fish *)0;
	int		nFish = SCHOOL_NFISH(s);
	double	minVel = SCHOOL_MINVEL(s);
	double	maxVel = SCHOOL_MAXVEL(s);
	double	momentum = SCHOOL_MOMENTUM(s);
	BBox	*bbox = &SCHOOL_BBOX(s);
	Fish	*theFishes = SCHOOL_FISHES(s);

	for(i = 0, f = theFishes; i < nFish; i++, f++)
		applyFishMovements(f, bbox, minVel, maxVel, momentum);
}


School *
glschool_initSchool(int nFish, double accLimit, double maxV, double minV, double distExp, double momentum,
		   double minRadius, double avoidFact, double matchFact, double centerFact, double targetFact,
		   double distComp)
{
	School	*s = (School *)0;

	if ((s = (School *)malloc(sizeof(School))) == (School *)0) {
		perror("initSchool School allocation failed: ");
		return s;
	}

	if ((SCHOOL_FISHES(s) = (Fish *)malloc(sizeof(Fish)*nFish)) == (Fish *)0) {
		perror("initSchool Fish array allocation failed: ");
		free(s);
		return (School *)0;
	}

	SCHOOL_NFISH(s) = nFish;
	SCHOOL_ACCLIMIT(s) = accLimit;
	SCHOOL_MAXVEL(s) = maxV;
	SCHOOL_MINVEL(s) = minV;
	SCHOOL_DISTEXP(s) = distExp;
	SCHOOL_MOMENTUM(s) = momentum;
	SCHOOL_MINRADIUS(s) = minRadius;
	SCHOOL_MINRADIUSEXP(s) = pow(minRadius, distExp);
	SCHOOL_MATCHFACT(s) = matchFact;
	SCHOOL_AVOIDFACT(s) = avoidFact;
	SCHOOL_CENTERFACT(s) = centerFact;
	SCHOOL_TARGETFACT(s) = targetFact;
	SCHOOL_DISTCOMP(s) = distComp;

	return s;
}

void
glschool_freeSchool(School *s)
{
	free(SCHOOL_FISHES(s));
	free(s);
}

void
glschool_setBBox(School *s, double xMin, double xMax, double yMin, double yMax, double zMin, double zMax)
{
	int		i;
	BBox	*bbox = &SCHOOL_BBOX(s);

	BBOX_XMIN(bbox) = xMin; BBOX_XMAX(bbox) = xMax;
	BBOX_YMIN(bbox) = yMin; BBOX_YMAX(bbox) = yMax;
	BBOX_ZMIN(bbox) = zMin; BBOX_ZMAX(bbox) = zMax;

	for(i = 0; i < 3; i++) {
		SCHOOL_IMID(s, i) = BBOX_IMID(bbox, i);
		SCHOOL_IRANGE(s, i) = BBOX_IRANGE(bbox, i);
	}
}


void
glschool_newGoal(School *s)
{
	SCHOOL_IGOAL(s,0) = 0.85*(drand48()-0.5)*SCHOOL_IRANGE(s,0) + SCHOOL_IMID(s,0);
	SCHOOL_IGOAL(s,1) = 0.40*(drand48()-0.5)*SCHOOL_IRANGE(s,1) + SCHOOL_IMID(s,1);
	SCHOOL_IGOAL(s,2) = 0.85*(drand48()-0.5)*SCHOOL_IRANGE(s,2) + SCHOOL_IMID(s,2);
}


double
glschool_computeNormalAndThetaToPlusZ(double *v, double *xV)
{
	double	x1 = 0.0;
	double	y1 = 0.0;
	double	z1 = 1.0;
	double	x2 = v[0];
	double	y2 = v[1];
	double	z2 = v[2];
	double	theta = 0.0;
	double	xVNorm = 0.0;
	double	sinTheta = 0.0;
	double	v2Norm = norm(v);

	if (v2Norm == 0.0) {
		xV[1] = 1.0;
		xV[0] = xV[2] = 0.0;
		return theta;
	}
	xV[0] = (y1*z2 - z1*y2);
	xV[1] = -(x1*z2 - z1*x2);
	xV[2] = (x1*y2 - y1*x2);
	xVNorm = norm(xV);

	sinTheta = xVNorm/v2Norm;
	return (asin(sinTheta) * RAD2DEG);
}


int
glschool_computeGroupVectors(School *s, Fish *ref, double *avoidance, double *centroid, double *avgVel)
{
	int		i;
	double	dist;
	double	adjDist;
	double	diffVect[3];
	int		neighborCount = 0;
	Fish	*test = (Fish *)0;
	int		nFish = SCHOOL_NFISH(s);
	double	distExp = SCHOOL_DISTEXP(s);
	double	distComp = SCHOOL_DISTCOMP(s);
	double	minRadiusExp = SCHOOL_MINRADIUSEXP(s);
	Fish	*fishes = SCHOOL_FISHES(s);

	for(i = 0, test = fishes; i < nFish; i++, test++) {
		if (test == ref) continue;

		getDifferenceVector(FISH_POS(ref), FISH_POS(test), diffVect);

		dist = norm(diffVect) - distComp;
		if (dist < 0.0) dist = 0.1;

		adjDist = pow(dist, distExp);
		if (adjDist > minRadiusExp) continue;

		neighborCount++;

		addVector(avgVel, FISH_VEL(test));
		addVector(centroid, FISH_POS(test));

		addScaledVector(avoidance, diffVect, 1.0/adjDist);
	}
	if (neighborCount > 0) {
		scaleVector(avgVel, 1.0/neighborCount);
		scaleVector(centroid, 1.0/neighborCount);
	}
	return neighborCount;
}


void
glschool_computeAccelerations(School *s)
{
	int		i;
	int		j;
	int		neighborCount;
	double	dist;
	double	adjDist;
	double	accMag;
	double	avgVel[3];
	double	diffVect[3];
	double	centroid[3];
	double	avoidance[3];
	Fish	*ref = (Fish *)0;
	int		nFish = SCHOOL_NFISH(s);
	double	*goal = SCHOOL_GOAL(s);
	double	distExp = SCHOOL_DISTEXP(s);
	double	distComp = SCHOOL_DISTCOMP(s);
	double	avoidFact = SCHOOL_AVOIDFACT(s);
	double	matchFact = SCHOOL_MATCHFACT(s);
	double	centerFact = SCHOOL_CENTERFACT(s);
	double	targetFact = SCHOOL_TARGETFACT(s);
	double	accLimit = SCHOOL_ACCLIMIT(s);
	double	minRadius = SCHOOL_MINRADIUS(s);
	Fish	*fishes = SCHOOL_FISHES(s);

	for(i = 0, ref = fishes; i < nFish; i++, ref++) {
		clearVector(avgVel);
		clearVector(centroid);
		clearVector(avoidance);
		clearVector(FISH_ACC(ref));
		neighborCount = glschool_computeGroupVectors(s, ref, avoidance, centroid, avgVel);

		/* avoidanceAccel[] = avoidance[] * AvoidFact */
		scaleVector(avoidance, avoidFact);
		addVector(FISH_ACC(ref), avoidance);

		accMag = norm(FISH_ACC(ref));
		if (neighborCount > 0 && accMag < accLimit) {
			for(j = 0; j < 3; j++) {
				FISH_IAVGVEL(ref, j) = avgVel[j];
				FISH_IACC(ref, j) += ((avgVel[j] - FISH_IVEL(ref, j)) * matchFact);
			}

			accMag = norm(FISH_ACC(ref));
			if (accMag < accLimit) {
				for(j = 0; j < 3; j++)
					FISH_IACC(ref, j) += ((centroid[j] - FISH_IPOS(ref, j)) * centerFact);
			}
		}

		accMag = norm(FISH_ACC(ref));
		if (accMag < accLimit) {
			getDifferenceVector(goal, FISH_POS(ref), diffVect);

			dist = norm(diffVect) - distComp;
			if (dist < 0.0) dist = 0.1;

			/*adjDist = pow(dist, distExp);*/
			if (dist > minRadius) {
				adjDist = pow(dist, distExp);
				for(j = 0; j < 3; j++)
					FISH_IACC(ref, j) += (diffVect[j]*targetFact/adjDist);
			}
		}
	}
}
