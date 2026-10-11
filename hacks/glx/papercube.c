/* papercube, Copyright © 2023 Ireneusz Szpilewski <irkostka@irkostka.pl>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#define DEFAULTS  "*delay:        30000       \n" \
		  "*count:        30          \n" \
		  "*showFPS:      False       \n" \
		  "*wireframe:    False       \n" \
		  "*suppressRotationAnimation: True\n" \

# define release_cube 0

#include "xlockmore.h"
#include "gltrackball.h"
#include "rotator.h"

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "Y"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"

static char *do_spin;
static GLfloat speed;
static Bool do_wander;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionSepArg, 0 },
  { "+spin",   ".spin",   XrmoptionNoArg, "" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" }
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_String},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
};

ENTRYPOINT ModeSpecOpt cube_opts = { countof(opts), opts,
                                     countof(vars), vars, NULL};

#define MAP_ROWS 6
#define MAP_COLUMNS 5
#define BOTTOM_FIELD_ROW 1
#define BOTTOM_FIELD_COLUMN 1
#define PRESERVED_HEIGHT_TO_WIDTH 1.0
#define SUN_DURATION 2.0
#define FOLD_DURATION 1.0
#define PAUSE_DURATION 1.0
#define SPIN_DURATION 3.0
#define SPIN_RPS 1.0

#define PICTURE_SQUARE_COUNT 8
#define PICTURE_SQUARE_SIZE 16
#define PICTURE_LINE_WIDTH 2
#define PICTURE_BORDER_WIDTH 2

enum orientation
{
  Horizontal,
  Vertical
};

#define ORIENTATION_COUNT 2

enum direction
{
  Left_down,
  Right_up
};

#define DIRECTION_COUNT 2

struct angle
{
  double angle;
  Bool inserting;
};

struct field
{
  int row;
  int column;
  Bool arrow;
};

struct edge
{
  enum orientation orientation;
  enum direction direction;
};

enum move_stage
{
  Before_start,
  Starting,
  During_move,
  Stopping,
  After_stop
};

struct move
{
  enum move_stage move_stage;
  double start;
  double stop;
  double start_value;
  double stop_value;
};

struct move_time
{
  struct timeval big_bang;
  struct timeval now;
  double time;
};

struct field_move
{
  struct move move;
  struct field field;
  Bool inserting;
};

enum stage
{
  Sunrise,
  Fold,
  Spin_and_sunset
};

struct moves
{
  enum stage stage;
  struct move sunrise;
  struct field_move move[17];
  struct move spin;
  struct move sunset;
  struct move_time move_time;
};

struct rgba
{
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
};

struct picture
{
  int square_count;
  int square_size;
  int line_width;
  int border_width;
  int width;
  int height;

  struct rgba *data;
};

struct papercube
{
  const char *map;
  struct angle angles[MAP_ROWS][MAP_COLUMNS];
  struct moves moves;
  double eye_rotation;
  double spin_rotation;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  int spin_sign;
  Bool show_grid;
  GLuint texture_id;
  struct rgba fg, bg;
  GLXContext *glx_context;
};

static struct papercube *cps = NULL;

static enum direction
opposite(enum direction direction)
{
  return 1 - direction;
}

static void
paint_field(const struct field *field, const struct angle *angle,
            GLuint texture_id)
{
  double height;
  Bool draw_arrow;
  double rect_height;
  double arrow_height = 5.0 / 8.0;
  double arrow_width = 1.0 / 8.0;

  if(angle == NULL)
    height = 1.0;
  else
  {
    if(angle->inserting)
    {
      height = 2.0 * cos(M_PI - M_PI * angle->angle / 180);
    }
    else
      height = 1.0;
  }

  if(!field->arrow)
  {
    draw_arrow = False;
    rect_height = height;
  }
  else
  {
    if(height > arrow_height)
    {
      draw_arrow = True;
      rect_height = arrow_height;
    }
    else
    {
      draw_arrow = False;
      rect_height = height;
    }
  }

  glBindTexture(GL_TEXTURE_2D, texture_id);
  glBegin(GL_TRIANGLES);

  glTexCoord2d(0.0, 0.0);
  glVertex3d(field->column, 0.0, -field->row);

  glTexCoord2d(0.0, rect_height);
  glVertex3d(field->column, 0.0, -(field->row + rect_height));

  glTexCoord2d(1.0, 0.0);
  glVertex3d(field->column + 1, 0.0, -field->row);

  glTexCoord2d(0.0, rect_height);
  glVertex3d(field->column, 0.0, -(field->row + rect_height));

  glTexCoord2d(1.0, 0.0);
  glVertex3d(field->column + 1, 0.0, -field->row);

  glTexCoord2d(1.0, rect_height);
  glVertex3d(field->column + 1, 0.0, -(field->row + rect_height));

  if(draw_arrow)
  {
    arrow_width *= (height - arrow_height) / (1 - arrow_height);

    glTexCoord2d(0.0, arrow_height);
    glVertex3d(field->column, 0.0, -(field->row + arrow_height));

    glTexCoord2d(arrow_width, height);
    glVertex3d(field->column + arrow_width, 0.0, -(field->row + height));

    glTexCoord2d(1.0, arrow_height);
    glVertex3d(field->column + 1, 0.0, -(field->row + arrow_height));

    glTexCoord2d(1.0, arrow_height);
    glVertex3d(field->column + 1, 0.0, -(field->row + arrow_height));

    glTexCoord2d(1 - arrow_width, height);
    glVertex3d(field->column + 1 - arrow_width, 0.0, -(field->row + height));

    glTexCoord2d(arrow_width, height);
    glVertex3d(field->column + arrow_width, 0.0, -(field->row + height));
  }

  glEnd();
}

static char
get_field_from_map(const char* map, int row, int column)
{
  return map[2 * (MAP_ROWS - row - 1) * (MAP_COLUMNS * 2 - 1) + column * 2];
}

static char
get_edge_from_map(const char* map, int row, int column,
                  enum orientation orientation, enum direction direction)
{
  int offset;

  if
  (
    (column == 0
     && orientation == Horizontal && direction == Left_down)
    ||
    (column == (MAP_COLUMNS - 1)
     && orientation == Horizontal && direction == Right_up)
    ||
    (row == 0
     && orientation == Vertical && direction == Left_down)
    ||
    (row == (MAP_ROWS - 1)
     && orientation == Vertical && direction == Right_up)
  )
    return ' ';

  switch(orientation)
  {
    case Horizontal:
      offset = 1;
      break;
    case Vertical:
      offset = -(MAP_COLUMNS * 2 - 1);
      break;
    default:
      offset = 1;
  }

  if(direction == Left_down)
    offset *= -1;

  return map[2 * (MAP_ROWS - row - 1) * (MAP_COLUMNS * 2 - 1)
             + column * 2 + offset];
}

static struct field
get_neighbour(const struct field *field, const struct edge *edge)
{
  struct field f = *field;

  switch(edge->orientation)
  {
    case Horizontal:
      switch(edge->direction)
      {
        case Left_down:
          --f.column;
          break;
        case Right_up:
          ++f.column;
          break;
      }

      break;
    case Vertical:
      switch(edge->direction)
      {
        case Left_down:
          --f.row;
          break;
        case Right_up:
          ++f.row;
          break;
      }

      break;
  }

  return f;
}

static void
paint_field_and_neighbours(struct papercube *papercube,
                           struct field *field, const struct edge *entry_edge)
{
  int o;
  int d;
  char symbol = get_field_from_map(papercube->map, field->row, field->column);

  field->arrow = symbol == '^';

  if(entry_edge)
    paint_field(field, &papercube->angles[field->row][field->column],
                papercube->texture_id);
  else
    paint_field(field, NULL, papercube->texture_id);

  for(o = Horizontal; o < ORIENTATION_COUNT; o++)
    for(d = Left_down; d < DIRECTION_COUNT; d++)
    {
      if(!(entry_edge != NULL
           && o == entry_edge->orientation && d == entry_edge->direction))
      {
        if('+' == get_edge_from_map(papercube->map,
                                    field->row, field->column, o, d))
        {
          struct edge edge;
          struct field neighbour;
          double angle;
          int sign;
          int axis;

          glPushMatrix();

          edge.orientation = o;
          edge.direction = d;

          neighbour = get_neighbour(field, &edge);
          edge.direction = opposite(d);
          angle = papercube->angles[neighbour.row][neighbour.column].angle;

          switch(o)
          {
            case Horizontal:
              axis = neighbour.column + edge.direction;
              glTranslated(axis, 0.0, 0.0);

              if(axis <= BOTTOM_FIELD_COLUMN)
                sign = -1;
              else
                sign = 1;

              glRotated(sign * angle, 0.0, 0.0, 1.0);
              glTranslated(-axis, 0.0,  0.0);
              break;
            case Vertical:
              axis = neighbour.row + edge.direction;
              glTranslated(0.0, 0.0, -axis);

              if(axis <= BOTTOM_FIELD_ROW)
                sign = -1;
              else
                sign = 1;

              glRotated(sign * angle, 1.0, 0.0, 0.0);
              glTranslated(0.0, 0.0, axis);
              break;
          }


          paint_field_and_neighbours(papercube, &neighbour, &edge);
          glPopMatrix();
        }
      }
    }
}

static void
paint_papercube(struct papercube *papercube)
{
  struct field start_field = { BOTTOM_FIELD_ROW, BOTTOM_FIELD_COLUMN };
  double cx =  BOTTOM_FIELD_COLUMN + 0.5;
  double cy =  0.5;
  double cz =  -BOTTOM_FIELD_ROW - 0.5;
  double x, y, z;

  glPushMatrix();
  glTranslatef (cx, cy, cz);

  get_position (papercube->rot, &x, &y, &z, !papercube->button_down_p);
  glTranslatef((x - 0.5) * 3,
               (y - 0.5) * 3,
               (z - 0.5) * 3);

  gltrackball_rotate (papercube->trackball);

  get_rotation (papercube->rot, &x, &y, &z, !papercube->button_down_p);
  glRotatef (x * 360, 1.0, 0.0, 0.0);
  glRotatef (y * 360, 0.0, 1.0, 0.0);
  glRotatef (z * 360, 0.0, 0.0, 1.0);

  glRotated(papercube->eye_rotation +
            papercube->spin_sign * papercube->spin_rotation,
            0.0, 1.0, 0.0);
  glTranslatef (-cx, -cy, -cz);

  paint_field_and_neighbours(papercube, &start_field, NULL);

  glPopMatrix();
}

static void
free_texture(ModeInfo *mi)
{
    struct papercube *papercube = &cps[MI_SCREEN(mi)];

    if (!papercube->glx_context) return;

    glXMakeCurrent (MI_DISPLAY(mi), MI_WINDOW(mi), *papercube->glx_context);

    glDeleteTextures(1, &papercube->texture_id);
}

static void
set_move_time_big_bang(struct move_time *move_time)
{
  gettimeofday(&move_time->big_bang, NULL);
}

static void
set_move_time_now(struct move_time *move_time)
{
  double now_time;
  double big_bang_time;

  gettimeofday(&move_time->now, NULL);

  big_bang_time = move_time->big_bang.tv_sec
                  + move_time->big_bang.tv_usec / 1000000.0;
  now_time = move_time->now.tv_sec
             + move_time->now.tv_usec / 1000000.0;

  move_time->time = now_time - big_bang_time;
}

static enum move_stage
get_move_value(struct move *move, double time, double *value)
{
  if(move->move_stage == After_stop)
    return After_stop;

  if(move->stop <= time)
  {
    *value = move->stop_value;
    move->move_stage = After_stop;

    return Stopping;
  }
  else if(move->start <= time)
  {
      *value = move->start_value
               + (move->stop_value - move->start_value)
               * (time - move->start) / (move->stop - move->start);

      if(move->move_stage == Before_start)
      {
        move->move_stage = During_move;
        return Starting;
      }

      return During_move;
  }
  else
  {
    return Before_start;
  }
}

static enum move_stage
move_spin(struct papercube *papercube, struct move *move)
{
  double value;
  enum move_stage move_stage;

  move_stage = get_move_value(move, papercube->moves.move_time.time, &value);

  switch(move_stage)
  {
    case Before_start:
    case After_stop:
      break;
    case Starting:
    case During_move:
    case Stopping:
      papercube->spin_rotation = value;
  };

  return move_stage;
}

static enum move_stage
move_fields(struct papercube *papercube)
{
  int i;
  double value;
  enum move_stage move_stage;
  enum move_stage result = Before_start;

  for(i = 0; i < sizeof(papercube->moves.move)
                 / sizeof(papercube->moves.move[0]); i++)
  {
    struct field_move *field_move = &papercube->moves.move[i];
    struct angle *angle = &papercube->angles[field_move->field.row]
                                            [field_move->field.column];

    move_stage = get_move_value(&field_move->move, 
                                papercube->moves.move_time.time, &value);

    switch(move_stage)
    {
      case Before_start:
      case After_stop:
        break;
      case Starting:
        if(field_move->inserting)
          angle->inserting = True;
      case During_move:
      case Stopping:
        result = During_move;
        angle->angle = value;
    }
  }

  if(move_stage == Stopping)
    result = Stopping;

  return result;
}

static enum move_stage
move_sun(struct papercube *papercube, struct move *move)
{
  double value;
  enum move_stage move_stage;

  move_stage = get_move_value(move, papercube->moves.move_time.time, &value);

  switch(move_stage)
  {
    case Before_start:
    case After_stop:
      break;
    case Starting:
    case During_move:
    case Stopping:
      glColor3f(value, value, value);
  };

  return move_stage;
}

static enum move_stage
move_papercube(struct papercube *papercube)
{
  enum move_stage move_stage;
  enum move_stage result = Before_start;

  set_move_time_now(&papercube->moves.move_time);

  switch(papercube->moves.stage)
  {
    case Sunrise:
      move_stage = move_sun(papercube, &papercube->moves.sunrise);

      switch(move_stage)
      {
        case Before_start:
          break;
        case Starting:
          result = Starting;
          break;
        case During_move:
        case Stopping:
        case After_stop:
          result = During_move;
      };

      if(move_stage == Stopping)
      {
        papercube->moves.stage = Fold;
      }

      break;
    case Fold:
      move_stage = move_fields(papercube);
      result = During_move;

      if(move_stage == Stopping)
          papercube->moves.stage = Spin_and_sunset;
      break;
    case Spin_and_sunset:
      move_spin(papercube, &papercube->moves.spin);
      move_stage = move_sun(papercube, &papercube->moves.sunset);

      if(move_stage == Stopping)
          result = Stopping;
  }

  return result;
}

static void
free_picture(struct picture *picture)
{
  free(picture->data);
}

static void
paint_rectangle(struct picture *picture, int x, int y, int width, int height,
                const struct rgba *color)
{
  struct rgba *base = picture->data + y * picture->width + x;
  int i;
  int j;

  for(i = 0; i < height; i++)
  {
    for(j = 0; j < width; j++)
      *(base + j) = *color;

    base += picture->width;
  }
}

static void
paint_picture_grid(struct picture *picture, struct rgba *color)
{
  int half_line_width = picture->line_width / 2;
  int x;
  int y;

  for(x = 1; x < picture->square_count; x++)
    paint_rectangle(picture, x * picture->square_size - half_line_width, 0,
                    picture->line_width, picture->height, color);

  for(y = 1; y < picture->square_count; y++)
    paint_rectangle(picture, 0, y * picture->square_size - half_line_width,
                    picture->width, picture->line_width, color);
}

static void
paint_picture_border(struct picture *picture)
{
  struct rgba black = { 0, 0, 0, 255 };
  int x;
  int y;

  x = 0;
  y = 0;

  paint_rectangle(picture, x, y, picture->width, picture->border_width,
                  &black);
  paint_rectangle(picture, x, y, picture->border_width, picture->height,
                  &black);

  x = 0;
  y = picture->height - picture->border_width;

  paint_rectangle(picture, x, y, picture->width, picture->border_width,
                  &black);

  x = picture->width - picture->border_width;
  y = 0;

  paint_rectangle(picture, x, y, picture->border_width, picture->height,
                  &black);
}

static void
paint_picture(struct picture *picture, Bool show_grid,
              struct rgba *fg, struct rgba *bg)
{
  paint_rectangle(picture, 0, 0, picture->width, picture->height, fg);

  if(show_grid)
    paint_picture_grid(picture, bg);

  paint_picture_border(picture);
}

static void
initialize_picture(struct picture *picture, Bool show_grid,
                   struct rgba *fg, struct rgba *bg)
{
  int size;

  picture->square_count = PICTURE_SQUARE_COUNT;
  picture->square_size = PICTURE_SQUARE_SIZE;
  picture->line_width = PICTURE_LINE_WIDTH;
  picture->border_width = PICTURE_BORDER_WIDTH;
  picture->width = picture->height =
    picture->square_count * picture->square_size;

  size = picture->width * picture->height * sizeof(struct rgba);

  picture->data = malloc(size);

  if(!picture->data)
  {
    fprintf(stderr, "%s: out of memory", progname);
    exit(1);
  }

  paint_picture(picture, show_grid, fg, bg);
}

static void
initialize_texture(ModeInfo *mi)
{
  struct papercube *papercube = &cps[MI_SCREEN(mi)];
  struct picture picture;

  if (!papercube->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *papercube->glx_context);

  glGenTextures(1, &papercube->texture_id);
  glBindTexture(GL_TEXTURE_2D, papercube->texture_id);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  initialize_picture(&picture, papercube->show_grid,
                     &papercube->fg, &papercube->bg);

  clear_gl_error();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, picture.width, picture.height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, picture.data);
  check_gl_error("texture");

  free_picture(&picture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

static void
set_move(struct move *move, double start, double stop,
         double start_value, double stop_value)
{
  move->move_stage = Before_start;
  move->start = start;
  move->stop = stop;
  move->start_value = start_value;
  move->stop_value = stop_value;
}

static void
set_field_move(struct field_move *field_move, double start, double stop,
               double start_value, double stop_value,
               struct field field, Bool inserting)
{
  set_move(&field_move->move, start, stop, start_value, stop_value);

  field_move->field = field;
  field_move->inserting = inserting;
}

static void
initialize_moves(struct moves *moves)
{
  struct field fields[] =
  {
    { 0, 0 },
    { 2, 0 },
    { 1, 0 },
    { 0, 2 },
    { 0, 1 },
    { 2, 1 },
    { 1, 2 },
    { 1, 3 },
    { 2, 3 },
    { 0, 3 },
    { 1, 4 },
    { 2, 2 },
    { 3, 2 },
    { 4, 2 },
    { 5, 2 }
  };

  double angle = 90.0;
  double fold = FOLD_DURATION / speed;
  double pause = PAUSE_DURATION / speed;
  double sun_d = SUN_DURATION / speed;
  double spin = SPIN_DURATION / speed;
  double spin_rps = SPIN_RPS;
  double time = 0;
  double brightness = 1.0;
  struct field_move *field_move;
  int i;

  set_move(&moves->sunrise, time, sun_d, 0.0, brightness);

  time = moves->sunrise.stop + pause;

  for(i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
  {
    double multi;

    field_move = &moves->move[i];

    if(i == 13)
      multi = 1.0 / 3.0;
    else if(i == 14)
      multi = 4.0 / 3.0;
    else
      multi = 1.0;

    set_field_move(field_move, time, time + fold * multi,
                   0.0, angle * multi, fields[i], False);

    time = field_move->move.stop + pause;
  }

  field_move = &moves->move[15];

  set_field_move(field_move, time, time + fold * 2.0f / 3.0f,
                 30.0, angle, fields[13], False);

  field_move = &moves->move[16];

  set_field_move(field_move, time, time + fold * 2.0f / 3.0f,
                 120.0, angle, fields[14], True);

  time = field_move->move.stop + pause;

  set_move(&moves->spin, time, time + spin + sun_d,
           0.0, (spin + sun_d) * spin_rps * 360);

  time += spin;

  set_move(&moves->sunset, time, time + sun_d, brightness, 0.0);

  moves->stage = Sunrise;
  set_move_time_big_bang(&moves->move_time);
}

static void
initialize_angles(struct angle angles[MAP_ROWS][MAP_COLUMNS])
{
  int r;
  int c;

  for(r = 0; r < MAP_ROWS; r++)
    for(c = 0; c < MAP_COLUMNS; c++)
        {
          angles[r][c].angle = 0.0;
          angles[r][c].inserting = False;
        }
}

static void
initialize_papercube(ModeInfo *mi, Bool first_time)
{
  struct papercube *papercube = &cps[MI_SCREEN(mi)];

  papercube->map =
    "    ^    "
    "    +    "
    "    o    "
    "    +    "
    "    o    "
    "    +    "
    "o o o o  "
    "+ + + +  "
    "o+o+o+o+o"
    "+ +   +  "
    "o o+o o  ";

  if(first_time)
  {
    papercube->eye_rotation = 45.0;
    papercube->show_grid = False;
  }
  else
  {
    papercube->eye_rotation = random() % 360;
    papercube->show_grid = !papercube->show_grid;
  }

  papercube->spin_sign = (random() % 2) ? -1 : 1;
  papercube->spin_rotation = 0.0;

  papercube->fg.r = 255 * (0.5 + frand(0.5));
  papercube->fg.g = 255 * (0.5 + frand(0.5));
  papercube->fg.b = 255 * (0.5 + frand(0.5));
  papercube->fg.a = 255;

  papercube->bg.r = 0.7 * (255 - papercube->fg.r);
  papercube->bg.g = 0.7 * (255 - papercube->fg.g);
  papercube->bg.b = 0.7 * (255 - papercube->fg.b);
  papercube->bg.a = 255;

  initialize_angles(papercube->angles);
  initialize_moves(&papercube->moves);
  initialize_texture(mi);
}

ENTRYPOINT void
reshape_cube (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_LIGHT0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 10.0, 10.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  if((double) height / (double) width > PRESERVED_HEIGHT_TO_WIDTH)
  {
    double scale = (double) width * PRESERVED_HEIGHT_TO_WIDTH
                   / (double) height;
    glScaled(scale, scale, scale);
  }

  glTranslated(-BOTTOM_FIELD_COLUMN - 0.5, -0.5, BOTTOM_FIELD_ROW + 0.5);

  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT Bool
cube_handle_event (ModeInfo *mi, XEvent *event)
{
  struct papercube *papercube = &cps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, papercube->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &papercube->button_down_p))
    return True;

  return False;
}

ENTRYPOINT void
init_cube (ModeInfo *mi)
{
  struct papercube *papercube;

  MI_INIT (mi, cps);
  papercube = &cps[MI_SCREEN(mi)];

  papercube->glx_context = init_GL(mi);

  {
    Bool spinx=False, spiny=False, spinz=False;
    double spin_speed   = 0.5 * speed;
    double spin_accel   = 0.3;
    double wander_speed = 0.01 * speed;

    char *s = do_spin;
    while (*s)
      {
        if      (*s == 'x' || *s == 'X') spinx = True;
        else if (*s == 'y' || *s == 'Y') spiny = True;
        else if (*s == 'z' || *s == 'Z') spinz = True;
        else if (*s == '0') ;
        else
          {
            fprintf (stderr,
         "%s: spin must contain only the characters X, Y, or Z (not \"%s\")\n",
                     progname, do_spin);
            exit (1);
          }
        s++;
      }

    papercube->rot = make_rotator (spinx ? spin_speed : 0,
                                   spiny ? spin_speed : 0,
                                   spinz ? spin_speed : 0,
                                   spin_accel,
                                   do_wander ? wander_speed : 0,
                                   False);
    papercube->trackball = gltrackball_init (True);
  }

  if (speed <= 0.001) speed = 0.001;
  if (speed <= 0.001) speed = 0.001;

  reshape_cube (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  initialize_papercube(mi, True);
}

ENTRYPOINT void
draw_cube (ModeInfo *mi)
{
  struct papercube *papercube = &cps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  if (!papercube->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *papercube->glx_context);


  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if(Stopping == move_papercube(papercube))
  {
    free_texture(mi);
    initialize_papercube(mi, False);
  }

  paint_papercube(papercube);

  if (mi->fps_p)
    do_fps (mi);

  glFinish();
  glXSwapBuffers(dpy, window);
}

ENTRYPOINT void
free_cube (ModeInfo *mi)
{
  free_texture(mi);
}

XSCREENSAVER_MODULE_2 ("PaperCube", papercube, cube)

#endif /* USE_GL */
