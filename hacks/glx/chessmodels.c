/*
 * models for the xss chess screensavers
 * hacked from:
 *
 * glChess - A 3D chess interface
 *
 * Copyright (C) 2002  Robert  Ancell <bob27@users.sourceforge.net>
 *                     Michael Duelli <duelli@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* chessmodels.c: Contains the code for piece model creation */

#include <math.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "chessmodels.h"

#define ROT 16

#define piece_size 0.095
#define EPSILON 0.001

/* Make a revolved piece */
void revolve_line(double *trace_r, double *trace_h, double max_ih, int rot) {
  double theta, norm_theta, sin_theta, cos_theta;
  double norm_ptheta = 0.0, sin_ptheta = 0.0, cos_ptheta = 1.0;
  double radius, pradius;
  double max_height = max_ih, height, pheight;
  double dx, dy, len;
  int npoints, p;
  double dtheta = (2.0*M_PI) / rot;

  /* Get the number of points */
  for(npoints = 0; 
      fabs(trace_r[npoints]) > EPSILON || fabs(trace_h[npoints]) > EPSILON;
      ++npoints);

  /* If less than two points, can not revolve */
  if(npoints < 2)
    return;

  /* If the max_height hasn't been defined, find it */
  if(max_height < EPSILON)
    for(p = 0; p < npoints; ++p)
      if(max_height < trace_h[p])
	max_height = trace_h[p];

  /* Draw the revolution */
  for(theta = dtheta; rot > 0; --rot) {
    sin_theta = sin(theta);
    cos_theta = cos(theta);
    norm_theta = theta / (2.0 * M_PI);
    pradius = trace_r[0] * piece_size;
    pheight = trace_h[0] * piece_size;
    
    for(p = 1; p < npoints; ++p) {
      radius = trace_r[p] * piece_size;
      height = trace_h[p] * piece_size;

      /* Get the normalized lengths of the normal vector */
      dx = radius - pradius;
      dy = height - pheight;
      len = sqrt(dx*dx + dy*dy);
      dx /= len;
      dy /= len;

      /* If only triangles required */
      if (fabs(radius) < EPSILON) {
 	glBegin(GL_TRIANGLES);

	glNormal3f(dy * sin_ptheta, -dx, dy * cos_ptheta);
	glTexCoord2f(norm_ptheta, pheight / max_height);
	glVertex3f(pradius * sin_ptheta, pheight, pradius * cos_ptheta);
	
	glNormal3f(dy * sin_theta, -dx, dy * cos_theta);
	glTexCoord2f(norm_theta, pheight / max_height);
	glVertex3f(pradius * sin_theta, pheight, pradius * cos_theta);
	
	glTexCoord2f(0.5 * (norm_theta + norm_ptheta),
		     height / max_height);
	glVertex3f(0.0, height, 0.0);
	
	glEnd();
      } 
      
      else {
	glBegin(GL_QUADS);

	glNormal3f(dy * sin_ptheta, -dx, dy * cos_ptheta);
	glTexCoord2f(norm_ptheta, pheight / max_height);
	glVertex3f(pradius * sin_ptheta, pheight, pradius * cos_ptheta);

	glNormal3f(dy * sin_theta, -dx, dy * cos_theta);
	glTexCoord2f(norm_theta, pheight / max_height);
	glVertex3f(pradius * sin_theta, pheight, pradius * cos_theta);

	glTexCoord2f(norm_theta, height / max_height);
	glVertex3f(radius * sin_theta, height, radius * cos_theta);

	glNormal3f(dy * sin_ptheta, -dx, dy * cos_ptheta);
	glTexCoord2f(norm_ptheta, height / max_height);
	glVertex3f(radius * sin_ptheta, height, radius * cos_ptheta);

	glEnd();
      }

      pradius = radius;
      pheight = height;
    }

    sin_ptheta = sin_theta;
    cos_ptheta = cos_theta;
    norm_ptheta = norm_theta;
    theta += dtheta;
  }
}

void gen_model_lists(void) {
  glNewList(KING, GL_COMPILE);
  draw_king();
  glEndList();
  
  glNewList(QUEEN, GL_COMPILE);
  draw_queen();
  glEndList();

  glNewList(BISHOP, GL_COMPILE);
  draw_bishop();
  glEndList();

  glNewList(KNIGHT, GL_COMPILE);
  draw_knight();
  glEndList();

  glNewList(ROOK, GL_COMPILE);
  draw_rook();
  glEndList();

  glNewList(PAWN, GL_COMPILE);
  draw_pawn();
  glEndList();
}

void draw_pawn(void) {
  double trace_r[] = 
    { 3.5, 3.5, 2.5, 2.5, 1.5, 1.0, 1.8, 1.0, 2.0, 1.0, 0.0, 0.0 };
  double trace_h[] =
    { 0.0, 2.0, 3.0, 4.0, 6.0, 8.8, 8.8, 9.2, 11.6, 13.4, 13.4, 0.0 };
  
  revolve_line(trace_r, trace_h, 0.0, ROT);
}

void draw_rook(void)
{
  double trace_r[] =
    { 3.8, 3.8, 2.6, 2.0, 2.8, 2.8, 2.2, 2.2, 0.0, 0.0 };
  double trace_h[] =
    { 0.0, 2.0, 5.0, 10.2, 10.2, 13.6, 13.6, 13.0, 13.0, 0.0 };

  revolve_line(trace_r, trace_h, 0.0, ROT);
}

void draw_knight(void)
{
  double trace_r[] = { 4.1, 4.1, 2.0, 2.0, 2.6, 0.0 };
  double trace_h[] = { 0.0, 2.0, 3.6, 4.8, 5.8, 0.0 };

  /* Revolved base */
  revolve_line(trace_r, trace_h, 17.8, ROT);

  /* Non revolved pieces */
  /* Quads */
  glBegin(GL_QUADS);

  /* Square base */
  glNormal3f(0.0, -1.0, 0.0);
  glTexCoord2f(0.0, 5.8 / 17.8 * piece_size);
  glVertex3f(2.6 * piece_size, 5.8 * piece_size, 2.6 * piece_size);
  glTexCoord2f(0.0, 5.8 / 17.8 * piece_size);
  glVertex3f(-2.6 * piece_size, 5.8 * piece_size, 2.6 * piece_size);
  glTexCoord2f(0.0, 5.8 / 17.8 * piece_size);
  glVertex3f(-2.6 * piece_size, 5.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0, 5.8 / 17.8 * piece_size);
  glVertex3f(2.6 * piece_size, 5.8 * piece_size, -0.8 * piece_size);

  /* Upper edge of nose */
  glNormal3f(0.0, 0.707107, 0.707107);
  glTexCoord2f(0.0, 16.2 / 17.8 * piece_size);
  glVertex3f(0.8 * piece_size, 16.2 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 3.4 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 3.4 * piece_size);
  glTexCoord2f(0.0, 16.2 / 17.8 * piece_size);
  glVertex3f(-0.8 * piece_size, 16.2 * piece_size, 4.0 * piece_size);

  /* Above head */
  glNormal3f(0.0, 1.0, 0.0);

  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 3.4 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 3.0 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 3.0 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 3.4 * piece_size);

  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 3.0 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(0.5 * piece_size, 16.8 * piece_size, 1.6 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.5 * piece_size, 16.8 * piece_size, 1.6 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 3.0 * piece_size);

  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(0.5 * piece_size, 16.8 * piece_size, 1.6 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.5 * piece_size, 16.8 * piece_size, 1.6 * piece_size);

  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 0.2 * piece_size);

  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(0.4 * piece_size, 16.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.4 * piece_size, 16.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -0.2 * piece_size);

  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(0.4 * piece_size, 16.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.4 * piece_size, 16.8 * piece_size, -1.1 * piece_size);

  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);

  /* Back of head */
  glNormal3f(0.0, 0.0, -1.0);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0, 15.0 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 15.0 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0, 15.0 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 15.0 * piece_size, -4.4 * piece_size);

  /* Under back */
  glNormal3f(0.0, 0.0, -1.0);
  glTexCoord2f(0.0, 15.0 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 15.0 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0, 15.0 / 17.8 * piece_size);
  glVertex3f(1.0 * piece_size, 15.0 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0, 14.8 / 17.8 * piece_size);
  glVertex3f(0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);
  glTexCoord2f(0.0, 14.8 / 17.8 * piece_size);
  glVertex3f(-0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);

  /* Right side of face */
  glNormal3f(-0.933878, 0.128964, -0.333528);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 3.0 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.5 * piece_size, 16.8 * piece_size, 1.6 * piece_size);
  glTexCoord2f(0.0, 14.0 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 14.0 * piece_size, 1.3 * piece_size);
  glTexCoord2f(0.0, 13.8 / 17.8 * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);

  glNormal3f(-0.966676, 0.150427, 0.207145);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.5 * piece_size, 16.8 * piece_size, 1.6 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.8 * piece_size, 16.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0, 13.8 / 17.8 * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0, 14.0 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 14.0 * piece_size, 1.3 * piece_size);

  /* (above and below eye) */
  glNormal3f(-0.934057, 0.124541, -0.334704);

  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.82666667 * piece_size, 16.6 * piece_size,
	     0.2 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-0.8 * piece_size, 16.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0, 16.6 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.6 * piece_size, -0.38 * piece_size);

  glTexCoord2f(0.0, 13.8 / 17.8 * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0, 16.2 / 17.8 * piece_size);
  glVertex3f(-0.88 * piece_size, 16.2 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0, 16.2 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.2 * piece_size, -0.74 * piece_size);
  glTexCoord2f(0.0, 13.6 / 17.8 * piece_size);
  glVertex3f(-1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);

  glTexCoord2f(0.0, 13.6 / 17.8 * piece_size);
  glVertex3f(-1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0, 16.2 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.2 * piece_size, -0.74 * piece_size);
  glTexCoord2f(0.0, 15.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 15.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0, 14.0 / 17.8 * piece_size);
  glVertex3f(-0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);

  glNormal3f(-0.970801, -0.191698, -0.144213);
  glTexCoord2f(0.0, 16.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);
  glTexCoord2f(0.0, 14.8 / 17.8 * piece_size);
  glVertex3f(-0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);
  glTexCoord2f(0.0, 14.0 / 17.8 * piece_size);
  glVertex3f(-0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);
  glTexCoord2f(0.0, 15.8 / 17.8 * piece_size);
  glVertex3f(-1.0 * piece_size, 15.8 * piece_size, -1.1 * piece_size);

  glNormal3f(-0.975610, 0.219512, 0.0);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.0f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 15.0 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.8f / 17.8f * piece_size);
  glVertex3f(-0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);

  /* Left side of face */
  glNormal3f(0.933878, 0.128964, -0.333528);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 3.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 14.0 * piece_size, 1.3 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.5 * piece_size, 16.8 * piece_size, 1.6 * piece_size);

  glNormal3f(0.966676, 0.150427, 0.207145);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.5 * piece_size, 16.8 * piece_size, 1.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 14.0 * piece_size, 1.3 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.8 * piece_size, 16.8 * piece_size, 0.2 * piece_size);

  /* (above and below eye) */
  glNormal3f(0.934057, 0.124541, -0.334704);

  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(0.82666667 * piece_size, 16.6 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.6 * piece_size, -0.38 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.8 * piece_size, 16.8 * piece_size, 0.2 * piece_size);

  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.6f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.2 * piece_size, -0.74 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(0.88 * piece_size, 16.2 * piece_size, 0.2 * piece_size);

  glTexCoord2f(0.0f * piece_size, 13.6f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 15.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.2 * piece_size, -0.74 * piece_size);

  glNormal3f(0.970801, -0.191698, -0.144213);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 15.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.8f / 17.8f * piece_size);
  glVertex3f(0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);

  glNormal3f(0.975610, -0.219512, 0.0);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.8f / 17.8f * piece_size);
  glVertex3f(0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.0f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 15.0 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -4.4 * piece_size);

  /* Eyes */
  glNormal3f(0.598246, 0.797665, 0.076372);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(0.88 * piece_size, 16.2 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.2 * piece_size, -0.74 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(0.8 * piece_size, 16.4 * piece_size, -0.56 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(0.61333334 * piece_size, 16.4 * piece_size, 0.2 * piece_size);

  glNormal3f(0.670088, -0.714758, 0.200256);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(0.61333334 * piece_size, 16.4 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(0.8 * piece_size, 16.4 * piece_size, -0.56 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.6 * piece_size, -0.38 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(0.82666667 * piece_size, 16.6 * piece_size, 0.2 * piece_size);

  glNormal3f(-0.598246, 0.797665, 0.076372);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(-0.88 * piece_size, 16.2 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(-0.61333334 * piece_size, 16.4 * piece_size,
	     0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(-0.8 * piece_size, 16.4 * piece_size, -0.56 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.2 * piece_size, -0.74 * piece_size);

  glNormal3f(-0.670088, -0.714758, 0.200256);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(-0.61333334 * piece_size, 16.4 * piece_size,
	     0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(-0.82666667 * piece_size, 16.6 * piece_size,
	     0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.6 * piece_size, -0.38 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(-0.8 * piece_size, 16.4 * piece_size, -0.56 * piece_size);

  /* Hair */
  glNormal3f(0.0, 1.0, 0.0);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 17.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 17.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 17.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 17.8 * piece_size, -0.8 * piece_size);

  glNormal3f(1.0, 0.0, 0.0);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 17.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 16.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 17.8 * piece_size, -4.4 * piece_size);

  glNormal3f(-1.0, 0.0, 0.0);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 17.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 17.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 16.8 * piece_size, -0.8 * piece_size);

  glNormal3f(0.0, 0.0, 1.0);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 17.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 17.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 16.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 16.8 * piece_size, -0.8 * piece_size);

  glNormal3f(0.0, 0.0, -1.0);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 17.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.35 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 16.8 * piece_size, -4.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 17.8f / 17.8f * piece_size);
  glVertex3f(-0.35 * piece_size, 17.8 * piece_size, -4.4 * piece_size);

  /* Under chin */
  glNormal3f(0.0, -0.853282, 0.521450);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 14.0 * piece_size, 1.3 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 14.0 * piece_size, 1.3 * piece_size);

  glNormal3f(0.0, -0.983870, -0.178885);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 14.0 * piece_size, 1.3 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 14.0 * piece_size, 1.3 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);

  /* Mane */

  /* Right */
  glNormal3f(-0.788443, 0.043237, -0.613587);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(-2.6 * piece_size, 5.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.8f / 17.8f * piece_size);
  glVertex3f(-0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.0f / 17.8f * piece_size);
  glVertex3f(0.0, 15.0 * piece_size, -3.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 7.8f / 17.8f * piece_size);
  glVertex3f(0.0, 7.8 * piece_size, -4.0 * piece_size);

  /* Left */
  glNormal3f(0.788443, 0.043237, -0.613587);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(2.6 * piece_size, 5.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 7.8f / 17.8f * piece_size);
  glVertex3f(0.0, 7.8 * piece_size, -4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.0f / 17.8f * piece_size);
  glVertex3f(0.0, 15.0 * piece_size, -3.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.8f / 17.8f * piece_size);
  glVertex3f(0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);

  /* Chest */
  /* Front */
  glNormal3f(0.0, 0.584305, 0.811534);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 8.8f / 17.8f * piece_size);
  glVertex3f(-2.0 * piece_size, 8.8 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 8.8f / 17.8f * piece_size);
  glVertex3f(2.0 * piece_size, 8.8 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);

  /* Bottom */
  glNormal3f(0.0, -0.422886, 0.906183);
  glTexCoord2f(0.0f * piece_size, 8.8f / 17.8f * piece_size);
  glVertex3f(-2.0 * piece_size, 8.8 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(-2.6 * piece_size, 5.8 * piece_size, 2.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(2.6 * piece_size, 5.8 * piece_size, 2.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 8.8f / 17.8f * piece_size);
  glVertex3f(2.0 * piece_size, 8.8 * piece_size, 4.0 * piece_size);

  /* Right */
  glNormal3f(-0.969286, 0.231975, -0.081681);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(-1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(-2.6 * piece_size, 5.8 * piece_size, 2.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 8.8f / 17.8f * piece_size);
  glVertex3f(-2.0 * piece_size, 8.8 * piece_size, 4.0 * piece_size);

  glNormal3f(-0.982872, 0.184289, 0.0);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(-1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(-1.1422222222 * piece_size, 12.2 * piece_size,
	     -2.2222222222 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(-2.6 * piece_size, 5.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(-2.6 * piece_size, 5.8 * piece_size, 2.6 * piece_size);

  glTexCoord2f(0.0f * piece_size, 14.8f / 17.8f * piece_size);
  glVertex3f(-0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(-1.1422222222 * piece_size, 12.2 * piece_size,
	     -2.2222222222 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(-1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(-0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);

  /* Left */
  glNormal3f(0.969286, 0.231975, -0.081681);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 8.8f / 17.8f * piece_size);
  glVertex3f(2.0 * piece_size, 8.8 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(2.6 * piece_size, 5.8 * piece_size, 2.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);

  glNormal3f(0.982872, 0.184289, 0.0);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(2.6 * piece_size, 5.8 * piece_size, 2.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(2.6 * piece_size, 5.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(1.1422222222 * piece_size, 12.2 * piece_size,
	     -2.2222222222 * piece_size);

  glTexCoord2f(0.0f * piece_size, 14.8f / 17.8f * piece_size);
  glVertex3f(0.55 * piece_size, 14.8 * piece_size, -2.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(1.1422222222 * piece_size, 12.2 * piece_size,
	     -2.2222222222 * piece_size);
  glEnd();

  /* Triangles */
  glBegin(GL_TRIANGLES);

  /* Under mane */
  glNormal3f(0.819890, -0.220459, -0.528373);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(2.6 * piece_size, 5.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(1.44 * piece_size, 5.8 * piece_size, -2.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 7.8f / 17.8f * piece_size);
  glVertex3f(0.0, 7.8 * piece_size, -4.0 * piece_size);

  glNormal3f(0.0, -0.573462, -0.819232);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(1.44 * piece_size, 5.8 * piece_size, -2.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(-1.44 * piece_size, 5.8 * piece_size, -2.6 * piece_size);
  glTexCoord2f(0.0f * piece_size, 7.8f / 17.8f * piece_size);
  glVertex3f(0.0, 7.8 * piece_size, -4.0 * piece_size);

  glNormal3f(-0.819890, -0.220459, -0.528373);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(-2.6 * piece_size, 5.8 * piece_size, -0.8 * piece_size);
  glTexCoord2f(0.0f * piece_size, 7.8f / 17.8f * piece_size);
  glVertex3f(0.0, 7.8 * piece_size, -4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 5.8f / 17.8f * piece_size);
  glVertex3f(-1.44 * piece_size, 5.8 * piece_size, -2.6 * piece_size);

  /* Nose tip */
  glNormal3f(0.0, 0.0, 1.0);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.0, 14.0 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(0.8 * piece_size, 16.2 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(-0.8 * piece_size, 16.2 * piece_size, 4.0 * piece_size);

  /* Mouth left */
  glNormal3f(-0.752714, -0.273714, 0.598750);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.0, 14.0 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(-0.8 * piece_size, 16.2 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);

  glNormal3f(-0.957338, 0.031911, 0.287202);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(-0.8 * piece_size, 16.2 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 3.4 * piece_size);

  glNormal3f(-0.997785, 0.066519, 0.0);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 3.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, 3.0 * piece_size);

  /* Mouth right */
  glNormal3f(0.752714, -0.273714, 0.598750);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.0, 14.0 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(0.8 * piece_size, 16.2 * piece_size, 4.0 * piece_size);

  glNormal3f(0.957338, 0.031911, 0.287202);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 3.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(0.8 * piece_size, 16.2 * piece_size, 4.0 * piece_size);

  glNormal3f(0.997785, 0.066519, 0.0);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 3.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, 3.4 * piece_size);

  /* Under nose */
  glNormal3f(0.0, -0.992278, 0.124035);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.0, 14.0 * piece_size, 4.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 2.4 * piece_size);

  /* Neck indents */
  glNormal3f(-0.854714, 0.484047, 0.187514);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(-0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(-1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.45 * piece_size, 13.8 * piece_size, -0.2 * piece_size);

  glNormal3f(-0.853747, 0.515805, -0.071146);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.45 * piece_size, 13.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(-1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);

  glNormal3f(0.854714, 0.484047, 0.187514);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.45 * piece_size, 13.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);

  glNormal3f(0.853747, 0.515805, -0.071146);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.45 * piece_size, 13.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 12.2f / 17.8f * piece_size);
  glVertex3f(1.4 * piece_size, 12.2 * piece_size, -0.4 * piece_size);

  /* Under chin */
  glNormal3f(0.252982, -0.948683, -0.189737);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.6f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.45 * piece_size, 13.8 * piece_size, -0.2 * piece_size);

  glNormal3f(0.257603, -0.966012, 0.021467);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.45 * piece_size, 13.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.6f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);

  glNormal3f(0.126745, -0.887214, 0.443607);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.6f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(1.2 * piece_size, 13.8 * piece_size, 0.2 * piece_size);

  glNormal3f(-0.252982, -0.948683, -0.189737);
  glTexCoord2f(0.0f * piece_size, 14.0f / 17.8f * piece_size);
  glVertex3f(-0.6 * piece_size, 14.0 * piece_size, -1.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.45 * piece_size, 13.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.6f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);

  glNormal3f(-0.257603, -0.966012, 0.021467);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.6f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.45 * piece_size, 13.8 * piece_size, -0.2 * piece_size);

  glNormal3f(-0.126745, -0.887214, 0.443607);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-0.5 * piece_size, 13.8 * piece_size, 0.4 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.8f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.8 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 13.6f / 17.8f * piece_size);
  glVertex3f(-1.2 * piece_size, 13.6 * piece_size, -0.2 * piece_size);

  /* Eyes */
  glNormal3f(0.0, 0.0, -1.0);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(0.88 * piece_size, 16.2 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(0.61333334 * piece_size, 16.4 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(0.82666667 * piece_size, 16.6 * piece_size, 0.2 * piece_size);

  glNormal3f(0.000003, -0.668965, 0.743294);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.2 * piece_size, -0.74 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.6 * piece_size, -0.38 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(0.8 * piece_size, 16.4 * piece_size, -0.56 * piece_size);

  glNormal3f(0.0, 0.0, -1.0);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(-0.88 * piece_size, 16.2 * piece_size, 0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(-0.82666667 * piece_size, 16.6 * piece_size,
	     0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(-0.61333334 * piece_size, 16.4 * piece_size,
	     0.2 * piece_size);

  glNormal3f(-0.000003, -0.668965, 0.743294);
  glTexCoord2f(0.0f * piece_size, 16.2f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.2 * piece_size, -0.74 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.4f / 17.8f * piece_size);
  glVertex3f(-0.8 * piece_size, 16.4 * piece_size, -0.56 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.6f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.6 * piece_size, -0.38 * piece_size);

  /* Behind eyes */
  /* Right */
  glNormal3f(-0.997484, 0.070735, 0.004796);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-0.4 * piece_size, 16.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 15.8 * piece_size, -1.1 * piece_size);

  glNormal3f(-0.744437, 0.446663, -0.496292);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 16.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(-0.4 * piece_size, 16.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.8f / 17.8f * piece_size);
  glVertex3f(-1.0 * piece_size, 15.8 * piece_size, -1.1 * piece_size);

  /* Left */
  glNormal3f(0.997484, 0.070735, 0.004796);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.4 * piece_size, 16.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 15.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -2.0 * piece_size);

  glNormal3f(0.744437, 0.446663, -0.496292);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 16.8 * piece_size, -0.2 * piece_size);
  glTexCoord2f(0.0f * piece_size, 15.8f / 17.8f * piece_size);
  glVertex3f(1.0 * piece_size, 15.8 * piece_size, -1.1 * piece_size);
  glTexCoord2f(0.0f * piece_size, 16.8f / 17.8f * piece_size);
  glVertex3f(0.4 * piece_size, 16.8 * piece_size, -1.1 * piece_size);

  glEnd();
}

void draw_bishop(void)
{
  double trace_r[] =
      { 4.0, 4.0, 2.5, 2.5, 1.5, 1.2, 2.5, 1.7, 1.7, 2.2, 2.2,
    1.0, 0.8, 1.2, 0.8, 0.0, 0.0
  };
  double trace_h[] =
      { 0.0, 2.0, 3.0, 4.0, 7.0, 9.4, 9.4, 11.0, 12.2, 13.2,
    14.8, 16.0, 17.0, 17.7, 18.4, 18.4, 0.0
  };

  revolve_line(trace_r, trace_h, 0.0, ROT);
}

void draw_queen(void)
{
  double trace_r[] =
      { 4.8, 4.8, 3.4, 3.4, 1.8, 1.4, 2.9, 1.8, 1.8, 2.0, 2.7,
    2.4, 1.7, 0.95, 0.7, 0.9, 0.7, 0.0, 0.0
  };
  double trace_h[] =
      { 0.0, 2.2, 4.0, 5.0, 8.0, 11.8, 11.8, 13.6, 15.2, 17.8,
    19.2, 20.0, 20.0, 20.8, 20.8, 21.4, 22.0, 22.0, 0.0
  };

  revolve_line(trace_r, trace_h, 0.0, ROT);
}

void draw_king(void)
{
  double trace_r[] =
      { 5.0, 5.0, 3.5, 3.5, 2.0, 1.4, 3.0, 2.0, 2.0, 2.8, 1.6,
    1.6, 0.0, 0.0
  };
  double trace_h[] =
      { 0.0, 2.0, 3.0, 4.6, 7.6, 12.6, 12.6, 14.6, 15.6, 19.1,
    19.7, 20.1, 20.1, 0.0
  };

  revolve_line(trace_r, trace_h, 0.0, ROT);

  glBegin(GL_QUADS);

  /* Cross front */
  glNormal3f(0.0, 0.0, 1.0);

  glVertex3f(-0.3 * piece_size, 20.1 * piece_size, 0.351 * piece_size);
  glVertex3f(0.3 * piece_size, 20.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 23.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 23.1 * piece_size, 0.35 * piece_size);

  glVertex3f(-0.9 * piece_size, 21.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 21.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.9 * piece_size, 22.1 * piece_size, 0.35 * piece_size);

  glVertex3f(0.9 * piece_size, 21.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.9 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 21.1 * piece_size, 0.35 * piece_size);

  /* Cross back */
  glNormal3f(0.0, 0.0, -1.0);

  glVertex3f(0.3 * piece_size, 20.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 20.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 23.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 23.1 * piece_size, -0.35 * piece_size);

  glVertex3f(-0.3 * piece_size, 21.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.9 * piece_size, 21.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.9 * piece_size, 22.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 22.1 * piece_size, -0.35 * piece_size);

  glVertex3f(0.3 * piece_size, 21.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 22.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.9 * piece_size, 22.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.9 * piece_size, 21.1 * piece_size, -0.35 * piece_size);

  /* Cross left */
  glNormal3f(-1.0, 0.0, 0.0);

  glVertex3f(-0.9 * piece_size, 21.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.9 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.9 * piece_size, 22.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.9 * piece_size, 21.1 * piece_size, -0.35 * piece_size);

  glVertex3f(-0.3 * piece_size, 20.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 21.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 21.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 20.1 * piece_size, -0.35 * piece_size);

  glVertex3f(-0.3 * piece_size, 22.1 * piece_size, 0.3 * piece_size);
  glVertex3f(-0.3 * piece_size, 23.1 * piece_size, 0.3 * piece_size);
  glVertex3f(-0.3 * piece_size, 23.1 * piece_size, -0.3 * piece_size);
  glVertex3f(-0.3 * piece_size, 22.1 * piece_size, -0.3 * piece_size);

  /* Cross right */
  glNormal3f(1.0, 0.0, 0.0);

  glVertex3f(0.9 * piece_size, 21.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.9 * piece_size, 22.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.9 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.9 * piece_size, 21.1 * piece_size, 0.35 * piece_size);

  glVertex3f(0.3 * piece_size, 20.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 21.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 21.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 20.1 * piece_size, 0.35 * piece_size);

  glVertex3f(0.3 * piece_size, 22.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 23.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 23.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 22.1 * piece_size, 0.35 * piece_size);

  /* Cross top */
  glNormal3f(0.0, 1.0, 0.0);

  glVertex3f(-0.9 * piece_size, 22.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.9 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 22.1 * piece_size, -0.35 * piece_size);

  glVertex3f(0.3 * piece_size, 22.1 * piece_size, -0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.9 * piece_size, 22.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.9 * piece_size, 22.1 * piece_size, -0.35 * piece_size);

  glVertex3f(-0.3 * piece_size, 23.1 * piece_size, -0.35 * piece_size);
  glVertex3f(-0.3 * piece_size, 23.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 23.1 * piece_size, 0.35 * piece_size);
  glVertex3f(0.3 * piece_size, 23.1 * piece_size, -0.35 * piece_size);

  glEnd();
}
