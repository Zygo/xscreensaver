/* ants.h -- header file containing common ant parameters
 *
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
 * Copyright 2003 Blair Tennessy
*/

/* static GLfloat MaterialRed[] = {0.6, 0.0, 0.0, 1.0}; */
/*static GLfloat MaterialPurple[] = {0.6, 0.2, 0.5, 1.0};*/
/*static GLfloat MaterialOrange[] = {1.0, 0.69, 0.00, 1.0};*/
/*static GLfloat MaterialGreen[] = {0.1, 0.5, 0.2, 0.2};*/
/*static GLfloat MaterialBlue[] = {0.4, 0.4, 0.8, 1.0};*/
/*static GLfloat MaterialCyan[] = {0.2, 0.5, 0.7, 1.0};*/
/*static GLfloat MaterialYellow[] = {0.7, 0.7, 0.0, 1.0};*/
/* static GLfloat MaterialMagenta[] = {0.6, 0.2, 0.5, 1.0}; */
/*static GLfloat MaterialWhite[] = {0.7, 0.7, 0.7, 1.0};*/
static GLfloat MaterialGray[] = {0.2, 0.2, 0.2, 1.0};
static GLfloat MaterialGrayB[] = {0.1, 0.1, 0.1, 0.5};
/*static GLfloat MaterialGray35[] = {0.30, 0.30, 0.30, 1.0};*/

static GLfloat MaterialGray5[] = {0.5, 0.5, 0.5, 1.0};
static GLfloat MaterialGray6[] = {0.6, 0.6, 0.6, 1.0};
/* static GLfloat MaterialGray8[] = {0.8, 0.8, 0.8, 1.0};*/

typedef struct {

  double position[3];
  double goal[3];
  double velocity;
  double direction;
  double step;

  GLfloat *material;

} Ant;
