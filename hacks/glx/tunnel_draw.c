/* -*- Mode: C; tab-width: 4 -*- */
/* atunnels --- OpenGL Advanced Tunnel Demo */

#if 0
static const char sccsid[] = "@(#)tunnel_draw.c	5.13 2004/05/25 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 2002-2004. */

/*
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * The original code for this mode was written by Roman Podobedov
 * Email: romka@ut.ee
 * WEB: http://romka.demonews.com
 *
 * Eric Lassauge  (May-25-2004) <lassauge@users.sourceforge.net>
 * 				    http://lassauge.free.fr/linux.html
 *
 * REVISION HISTORY:
 * E.Lassauge - 25-May-2004:
 *	- added more texture
 *	- random texture init
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USE_GL /* whole file */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef STANDALONE
# ifndef HAVE_COCOA
#  include <GL/gl.h>
#  include <GL/glu.h>
# endif
#endif

#include "tunnel_draw.h"

#ifdef STANDALONE /* For NRAND() */
#include "xlockmoreI.h"          /* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"              /* in xlockmore distribution */
#endif /* STANDALONE */

typedef struct 
{
  float x, y, z; /* Point coordinates */
} cvPoint;

typedef struct _tnPath
{
	cvPoint p;
	struct _tnPath *next;
} tnPath;


static const cvPoint initpath[]={
{0.000000, 0.000000, 0.000000},
{2.000000, 1.000000, 0.000000},
{4.000000, 0.000000, 0.000000},
{6.000000, 1.000000, 0.000000},
{8.000000, 0.000000, 1.000000},
{10.000000, 1.000000, 1.000000},
{12.000000, 1.500000, 0.000000},
{14.000000, 0.000000, 0.000000},
{16.000000, 1.000000, 0.000000},
{18.000000, 0.000000, 0.000000},
{20.000000, 0.000000, 1.000000},
{22.000000, 1.000000, 0.000000},
{24.000000, 0.000000, 1.000000},
{26.000000, 0.000000, 1.000000},
{28.000000, 1.000000, 0.000000},
{30.000000, 0.000000, 2.000000},
{32.000000, 1.000000, 0.000000},
{34.000000, 0.000000, 2.000000},
{-1.000000, -1.000000, -1.000000}
};


struct tunnel_state {

  tnPath *path;

  float cam_t;					/* Camera variables */
  tnPath *cam_pos;
  float alpha;

  int tFlag;					/* Tunnel Drawing Variables */
  cvPoint prev_points[10];
  int current_texture;

  float ModeX;					/* Modes */
  int ModeXFlag;
};

/*=================== Vector normalization ==================================*/
static void normalize(cvPoint *V)
{
  float d;

  /* Vector length */
  d = (float)sqrt(V->x*V->x + V->y*V->y + V->z*V->z);

  /* Normalization */
  V->x /= d; 
  V->y /= d; 
  V->z /= d; 
}
/*=================== C = A x B  (Vector multiply) ==========================*/
#if 0
static void vect_mult(cvPoint *A, cvPoint *B, cvPoint *C)
{
	/* Vector multiply */
	C->x = A->y*B->z - A->z*B->y;
	C->y = A->z*B->x - A->x*B->z;
	C->z = A->x*B->y - A->y*B->x;
}
#endif

/* Catmull-Rom Curve calculations */
static void cvCatmullRom(cvPoint *p, float t, cvPoint *outp)
{
	float t2, t3, t1;

	t2 = t*t;
	t3 = t*t*t;
	t1 = (1-t)*(1-t);

	outp->x = (-t*t1*p[0].x + (2-5*t2+3*t3)*p[1].x + t*(1+4*t-3*t2)*p[2].x - t2*(1-t)*p[3].x)/2;
	outp->y = (-t*t1*p[0].y + (2-5*t2+3*t3)*p[1].y + t*(1+4*t-3*t2)*p[2].y - t2*(1-t)*p[3].y)/2;
	outp->z = (-t*t1*p[0].z + (2-5*t2+3*t3)*p[1].z + t*(1+4*t-3*t2)*p[2].z - t2*(1-t)*p[3].z)/2;
}

/*=================== Point Rotating Around Line ===========================
// p	- original point
// pp	- pivot point
// pl	- pivot line (vector)
// a	- angle to rotate in radians
// outp - output point
//==========================================================================
*/
static void RotateAroundLine(cvPoint *p, cvPoint *pp, cvPoint *pl, float a, cvPoint *outp)
{
	cvPoint p1, p2;
	float l, m, n, ca, sa;

	p1.x = p->x - pp->x;
	p1.y = p->y - pp->y;
	p1.z = p->z - pp->z;

	l = pl->x;
	m = pl->y;
	n = pl->z;

	ca = cos(a);
	sa = sin(a);

	p2.x = p1.x*((l*l)+ca*(1-l*l)) + p1.y*(l*(1-ca)*m+n*sa) + p1.z*(l*(1-ca)*n-m*sa);
	p2.y = p1.x*(l*(1-ca)*m-n*sa) + p1.y*(m*m+ca*(1-m*m)) + p1.z*(m*(1-ca)*n+l*sa);
	p2.z = p1.x*(l*(1-ca)*n+m*sa) + p1.y*(m*(1-ca)*n-l*sa) + p1.z*(n*n+ca*(1-n*n));

	outp->x = p2.x + pp->x;
	outp->y = p2.y + pp->y;
	outp->z = p2.z + pp->z;
}


/*=================== Load camera and tunnel path ==========================*/
static void LoadPath(struct tunnel_state *st)
{
	float x, y, z;
	tnPath *path1=NULL, *path2=NULL;

	cvPoint *f = (cvPoint *)initpath;
	
	while (f->x != -1.0)
	{
		x = f->x;
		y = f->y;
		z = f->z;
		f++;

		if (st->path == NULL)
		{
			st->path = (tnPath *)malloc(sizeof(tnPath));
			path1 = st->path;
		}
		else
		{
			if (!path1) path1 = st->path;
			path2 = (tnPath *)malloc(sizeof(tnPath));
			path1->next = path2;
			path1 = path2;
		}
		
		path1->next = NULL;
		path1->p.x = x;
		path1->p.y = y;
		path1->p.z = z;
	}

	st->cam_pos = st->path;
	st->cam_t = 0;
}

/*=================== Tunnel Initialization ================================*/
struct tunnel_state *
InitTunnel(void)
{
    struct tunnel_state *st = (struct tunnel_state *) calloc (1, sizeof(*st));
	LoadPath(st);
	st->current_texture = NRAND(MAX_TEXTURE);
    return st;
}

void DrawTunnel(struct tunnel_state *st, 
                int do_texture, int do_light, GLuint *textures)
{
	tnPath *p, *p1, *cmpos;
	cvPoint op, p4[4], T, ppp, ppp1, op1, op2;
	float t;
	int i, j, k, flag;
	cvPoint points[10];
	GLfloat light_position[4];


	/* Select current tunnel texture */
	if (do_texture)
		glBindTexture(GL_TEXTURE_2D, textures[st->current_texture]);
	
	cmpos = st->cam_pos;
	/* Get current curve */
	if (st->cam_pos->next &&
            st->cam_pos->next->next &&
            st->cam_pos->next->next->next)
	{
		p1 = st->cam_pos;
		for (i=0; i<4; i++)
		{
			p4[i].x = p1->p.x;
			p4[i].y = p1->p.y;
			p4[i].z = p1->p.z;
			p1 = p1->next;
		}
	}
	else 
	{
		/* End of tunnel */
		st->ModeX = 1.0;
		st->ModeXFlag = 0;
		return;
	};
		
	/* Get current camera position */
	cvCatmullRom(p4, st->cam_t, &op);

	/* Next camera position */
	st->cam_t += 0.02f;
	if (st->cam_t >= 1)
	{
		st->cam_t = st->cam_t - 1;
		cmpos = st->cam_pos->next;
	}
		
	/* Get curve for next camera position */
	if (cmpos->next->next->next)
	{
		p1 = cmpos;
		for (i=0; i<4; i++)
		{
			p4[i].x = p1->p.x;
			p4[i].y = p1->p.y;
			p4[i].z = p1->p.z;
			p1 = p1->next;
		}
	}
	else 
	{	
		/*  End of tunnel */
		st->ModeX = 1.0;
		st->ModeXFlag = 0;
		return;
	}
	
	/*  Get next camera position */
	cvCatmullRom(p4, st->cam_t, &op1);
	
	/*  Rotate camera */
	glRotatef(st->alpha, 0, 0, -1);
	st->alpha += 1;
	/*  Set camera position */
	gluLookAt(op.x, op.y, op.z, op1.x, op1.y, op1.z, 0, 1, 0);

	/*  Set light position */
	if (do_light)
	{
		light_position[0] = op.x;
		light_position[1] = op.y;
		light_position[2] = op.z;
		light_position[3] = 1;
		glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	}
	
	p = st->cam_pos;
	flag = 0;
	t = 0;
	k = 0;
	/*  Draw tunnel from current curve and next 2 curves */
	glBegin(GL_QUADS);
	while (k < 3)
	{
		if (p->next->next->next)
		{
			p1 = p;
			for (i=0; i<4; i++)
			{
				p4[i].x = p1->p.x;
				p4[i].y = p1->p.y;
				p4[i].z = p1->p.z;
				p1 = p1->next;
			}
		}
		else
		{
			/*  End of tunnel */
			st->ModeX = 1.0;
			st->ModeXFlag = 0;
            glEnd();
			return;
		}
		cvCatmullRom(p4, t, &op);

		ppp.x = op.x;
		ppp.y = op.y;
		ppp.z = op.z + 0.25;

		t += 0.1;
		if (t >= 1.0)
		{
			t = t - 1;
			k++;
			p = p->next;
		}
	
		if (p->next->next->next)
		{
			p1 = p;
			for (i=0; i<4; i++)
			{
				p4[i].x = p1->p.x;
				p4[i].y = p1->p.y;
				p4[i].z = p1->p.z;
				p1 = p1->next;
			}
		} 
		else
		{
			/*  End of tunnel */
			st->ModeX = 1.0;
			st->ModeXFlag = 0;
            glEnd();
			return;
		}
			
		cvCatmullRom(p4, t, &op1);

		ppp1.x = op1.x;
		ppp1.y = op1.y;
		ppp1.z = op1.z + 0.25;

		T.x = op1.x - op.x;
		T.y = op1.y - op.y;
		T.z = op1.z - op.z;
			
		normalize(&T);

		for (i=0; i<10; i++)
		{
			RotateAroundLine(&ppp, &op, &T, ((float)i*36.0*M_PI/180.0), &op2);
			points[i].x = op2.x;
			points[i].y = op2.y;
			points[i].z = op2.z;
			if (!flag)
			{
				st->prev_points[i].x = op2.x;
				st->prev_points[i].y = op2.y;
				st->prev_points[i].z = op2.z;
			}
		}
	
		if (!flag)
		{
			flag = 1;
			continue;
		}
		
		/*  Draw 10 polygons for current point */
		for (i=0; i<10; i++)
		{
			j = i+1;
			if (j > 9) j = 0;
			glNormal3f(0, 0, 1); /*  Normal for lighting */
			glTexCoord2f(0, 0); glVertex3f(st->prev_points[i].x, st->prev_points[i].y, st->prev_points[i].z);
			glNormal3f(0, 0, 1);
			glTexCoord2f(1, 0); glVertex3f(points[i].x, points[i].y, points[i].z);
			glNormal3f(0, 0, 1);
			glTexCoord2f(1, 1); glVertex3f(points[j].x, points[j].y, points[j].z);
			glNormal3f(0, 0, 1);
			glTexCoord2f(0, 1); glVertex3f(st->prev_points[j].x, st->prev_points[j].y, st->prev_points[j].z);
		}
		/*  Save current polygon coordinates for next position */
		for (i=0; i<10; i++)
		{
			st->prev_points[i].x = points[i].x;
			st->prev_points[i].y = points[i].y;
			st->prev_points[i].z = points[i].z;
		}
	}
	glEnd();
	st->cam_pos = cmpos;
}

/* =================== Show splash screen =================================== */
void SplashScreen(struct tunnel_state *st, 
                  int do_wire, int do_texture, int do_light)
{
	if (st->ModeX > 0)
	{
		/*  Reset tunnel and camera position */
		if (!st->ModeXFlag)
		{
			st->cam_pos = st->path;			
			st->cam_t = 0;
			st->tFlag = 0;
			st->ModeXFlag = 1;
			st->current_texture++;
			if (st->current_texture >= MAX_TEXTURE) st->current_texture = 0;
		}
		/*  Now we want to draw splash screen */
		glLoadIdentity();
		/*  Disable all unused features */
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		glDisable(GL_CULL_FACE);

		glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
		glEnable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
		glColor4f(1, 1, 1, st->ModeX);
		
		/*  Draw splash screen (simply quad) */
		glBegin(GL_QUADS);
	        glVertex3f(-10, -10, -1);
	        glVertex3f(10, -10, -1);
	        glVertex3f(10, 10, -1);
	        glVertex3f(-10, 10, -1);
		glEnd();

		st->ModeX -= 0.05;
		if (st->ModeX <= 0)	st->ModeX = 0;

		if (!do_wire)
		{
			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
		}
		if (do_light)
		{
			glEnable(GL_LIGHTING);
			glEnable(GL_FOG);
		}
		if (do_texture)
		{
			glEnable(GL_TEXTURE_2D);
		}
		glDisable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
	}
}

void FreeTunnel(struct tunnel_state *st)
{
  free (st);
}

#endif
