/* -*- Mode: C; tab-width: 4 -*- */
/* maze3d --- A recreation of the old 3D maze screensaver from Windows 95.
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
 * Revision History:
 *
 * 03-Apr-2018: Released initial version of "3D Maze"
 * (sudoer@riseup.net)
 */

#undef USE_FLOATING_IMAGES
#undef USE_FRACTAL_IMAGES

#ifdef STANDALONE
#define DEFAULTS	"*delay:			20000   \n"	\
					"*showFPS:			False	\n" \

#define release_maze 0
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */
#include <math.h>

#ifdef USE_GL /* whole file */

#define DEF_ANGULAR_CONVERSION_FACTOR 90
#define DEF_SPEED "1.0"
#define DEF_NUM_ROWS "12"
#define DEF_NUM_COLUMNS "12"
#define DEF_NUM_RATS "1"
#define DEF_NUM_INVERTERS "10"
#define DEF_SHOW_OVERLAY "False"
#define DEF_DROP_ACID "False"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "../images/gen/brick1_png.h"
#include "../images/gen/brick2_png.h"
#include "../images/gen/wood2_png.h"
#include "../images/gen/start_png.h"
#include "../images/gen/bob_png.h"
#include "../images/gen/logo-32_png.h"

#ifdef USE_FLOATING_IMAGES
# include "../images/gen/opengltxt_png.h"
# include "../images/gen/openglbook_png.h"
#endif
#ifdef USE_FRACTAL_IMAGES
# include "../images/gen/fractal1_png.h"
# include "../images/gen/fractal2_png.h"
# include "../images/gen/fractal3_png.h"
# include "../images/gen/fractal4_png.h"
#endif

#include "ximage-loader.h"

static int dropAcid, dropAcidWalls, dropAcidCeiling, dropAcidFloor, numInverters, numRats;
#ifdef USE_FLOATING_IMAGES
static int numGl3dTexts, numGlRedbooks;
#endif
static int shouldDrawOverlay, numRows, numColumns;
static char *wallTexture, *floorTexture, *ceilingTexture;
static GLfloat speed;

static XrmOptionDescRec opts[] = {
	{"-drop-acid", ".maze3d.dropAcid", XrmoptionNoArg, "true"},
	{"-drop-acid-walls", ".maze3d.dropAcidWalls", XrmoptionNoArg, "true"},
	{"-drop-acid-floor", ".maze3d.dropAcidFloor", XrmoptionNoArg, "true"},
	{"-drop-acid-ceiling", ".maze3d.dropAcidCeiling", XrmoptionNoArg, "true"},
	{"-wall-texture", ".maze3d.wallTexture", XrmoptionSepArg, 0},
	{"-floor-texture", ".maze3d.floorTexture", XrmoptionSepArg, 0},
	{"-ceiling-texture", ".maze3d.ceilingTexture", XrmoptionSepArg, 0},
	{"-rows", ".maze3d.numRows", XrmoptionSepArg, 0},
	{"-columns", ".maze3d.numColumns", XrmoptionSepArg, 0},
	{"-inverters", ".maze3d.numInverters", XrmoptionSepArg, 0},
	{"-rats", ".maze3d.numRats", XrmoptionSepArg, 0},
# ifdef USE_FLOATING_IMAGES
	{"-gl-3d-texts", ".maze3d.numGl3dTexts", XrmoptionSepArg, 0},
	{"-gl-redbooks", ".maze3d.numGlRedbooks", XrmoptionSepArg, 0},
# endif
	{"-overlay", ".maze3d.showOverlay", XrmoptionNoArg, "true"},
	{"-speed", ".maze3d.speed", XrmoptionSepArg, 0},
};

static argtype vars[] = {
	{&dropAcid, "dropAcid", "Drop Acid", DEF_DROP_ACID, t_Bool},
	{&dropAcidWalls, "dropAcidWalls", "Drop Acid Walls", "False", t_Bool},
	{&dropAcidFloor, "dropAcidFloor", "Drop Acid Floor", "False", t_Bool},
	{&dropAcidCeiling, "dropAcidCeiling", "Drop Acid Ceiling", "False", t_Bool},
	{&wallTexture, "wallTexture", "Wall Texture", "brick-wall", t_String},
	{&floorTexture, "floorTexture", "Floor Texture", "wood-floor", t_String},
	{&ceilingTexture, "ceilingTexture", "Ceiling Texture", "ceiling-tiles",
		t_String},
	{&numRows, "numRows", "Number of Rows", DEF_NUM_ROWS, t_Int},
	{&numColumns, "numColumns", "Number of Columns", DEF_NUM_COLUMNS, t_Int},
	{&numInverters, "numInverters", "Number of Inverters", DEF_NUM_INVERTERS, t_Int},
	{&numRats, "numRats", "Number of Rats", DEF_NUM_RATS, t_Int},
# ifdef USE_FLOATING_IMAGES
	{&numGl3dTexts, "numGl3dTexts", "Number of GL 3D Texts", "3", t_Int},
	{&numGlRedbooks, "numGlRedbooks", "Number of GL Redbooks", "3", t_Int},
# endif
	{&shouldDrawOverlay, "showOverlay", "Show Overlay", DEF_SHOW_OVERLAY, t_Bool},
	{&speed, "speed", "speed", DEF_SPEED, t_Float},
};

ENTRYPOINT ModeSpecOpt maze_opts = {countof(opts), opts, countof(vars), vars, NULL};

enum cellTypes
{
	WALL, CELL_UNVISITED, CELL, START, FINISH, GL_3D_TEXT, INVERTER_TETRAHEDRON,
	INVERTER_OCTAHEDRON, INVERTER_DODECAHEDRON, INVERTER_ICOSAHEDRON,
	WALL_GL_REDBOOK
};

enum programStates
{
	STARTING, WALKING, TURNING_LEFT, TURNING_RIGHT, TURNING_AROUND, INVERTING,
	FINISHING
};

enum overlayLists
{
	ARROW = 15, SQUARE, STAR, TRIANGLE
};

enum directions
{
	NORTH = 0, EAST = 90, SOUTH = 180, WEST = 270
};

typedef struct
{
	unsigned row, column;
} Tuple;

typedef struct
{
	GLfloat x, z;
} Tuplef;

typedef struct
{
	GLfloat red, green, blue;
} Color;

typedef struct
{
	Tuplef position;
	GLfloat rotation, desiredRotation, inversion, remainingDistanceToTravel;
	unsigned char state, isCamera;
} Rat;


/* structure for holding the maze data */
typedef struct
{
	GLXContext *glx_context;

	unsigned char **mazeGrid;
	Tuple *wallList;
	unsigned wallListSize;
	Tuple startPosition, finishPosition, *inverterPosition,
	*gl3dTextPosition;
    GLuint  wallTexture, floorTexture, ceilingTexture, startTexture,
      finishTexture, ratTexture;
	Rat camera;
	Rat *rats;
# ifdef USE_FLOATING_IMAGES
	GLuint gl3dTextTexture, glTextbookTexture;
# endif
# ifdef USE_FRACTAL_IMAGES
	GLuint fractal1Texture, fractal2Texture, fractal3Texture, fractal4Texture;
# endif
	Color acidColor;
	float acidHue;
	GLfloat wallHeight, inverterRotation;
    Bool button_down_p;
    int numRows, numColumns, numGlRedbooks;
    GLuint dlists[30];  /* ARROW etc index into this */

} maze_configuration;

static maze_configuration *mazes = NULL;

static void newMaze(maze_configuration* maze);
static void constructLists(ModeInfo *);
static void initializeGrid(maze_configuration* maze);
static float roundToNearestHalf(float num);
static unsigned isOdd(unsigned num);
static unsigned isEven(unsigned num);
static void buildMaze(maze_configuration* maze);
static void addWallsToList(Tuple cell, maze_configuration* maze);
static unsigned char isRemovableWall(Tuple coordinates,
		maze_configuration* maze);
static void addCells(Tuple cellToAdd, Tuple currentWall,
		maze_configuration* maze);
static void removeWallFromList(unsigned index, maze_configuration* maze);
static void placeMiscObjects(maze_configuration* maze);
static Tuple placeObject(maze_configuration* maze, unsigned char type);
static void shiftAcidColor(maze_configuration* maze);
static void refreshRemainingDistanceToTravel(ModeInfo * mi);
static void step(Rat* rat, maze_configuration* maze);
static void walk(Rat* rat, char axis, int sign, maze_configuration* maze);
static void turn(Rat* rat, maze_configuration* maze);
static void turnAround(Rat* rat, maze_configuration* maze);
static void invert(maze_configuration* maze);
static void changeState(Rat* rat, maze_configuration* maze);
static void updateInverterRotation(maze_configuration* maze);
static void drawInverter(maze_configuration* maze, Tuple coordinates);
static void drawWalls(ModeInfo * mi);
static void drawWall(Tuple startCoordinates, Tuple endCoordinates,
		maze_configuration* maze);
static void drawCeiling(ModeInfo * mi);
static void drawFloor(ModeInfo * mi);
static void drawPane(ModeInfo *, GLuint texture, Tuple position);
static void drawRat(Tuplef position, maze_configuration* maze);
static void drawOverlay(ModeInfo *);

/* Set up and enable texturing on our object */
static void
setup_png_texture (ModeInfo *mi, const unsigned char *png_data,
                   unsigned long data_size)
{
    XImage *image = image_data_to_ximage (MI_DISPLAY (mi), MI_VISUAL (mi),
                                          png_data, data_size);
	char buf[1024];
	clear_gl_error();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	/* iOS invalid enum:
	glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);
	*/
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
				 image->width, image->height, 0,
				 GL_RGBA,
				 /* GL_UNSIGNED_BYTE, */
				 GL_UNSIGNED_INT_8_8_8_8_REV,
				 image->data);
	sprintf (buf, "builtin texture (%dx%d)", image->width, image->height);
			 check_gl_error(buf);
}


static Bool
setup_file_texture (ModeInfo *mi, char *filename)
{
	Display *dpy = mi->dpy;
	Visual *visual = mi->xgwa.visual;
	char buf[1024];

	XImage *image = file_to_ximage (dpy, visual, filename);
	if (!image) return False;

	clear_gl_error();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
				 image->width, image->height, 0,
				 GL_RGBA,
				 GL_UNSIGNED_BYTE, image->data);
	sprintf (buf, "texture: %.100s (%dx%d)",
			 filename, image->width, image->height);
	check_gl_error(buf);
	return True;
}

static void
setup_textures(ModeInfo * mi)
{
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];
    GLint mag = GL_NEAREST;  /* GL_LINEAR */

	glGenTextures(1, &maze->finishTexture);
	glBindTexture(GL_TEXTURE_2D, maze->finishTexture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, logo_32_png, sizeof(logo_32_png));

	glGenTextures(1, &maze->ratTexture);
	glBindTexture(GL_TEXTURE_2D, maze->ratTexture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, bob_png, sizeof(bob_png));

# ifdef USE_FLOATING_IMAGES
	glGenTextures(1, &maze->glTextbookTexture);
	glBindTexture(GL_TEXTURE_2D, maze->glTextbookTexture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, openglbook_png, sizeof(openglbook_png));

	glGenTextures(1, &maze->gl3dTextTexture);
	glBindTexture(GL_TEXTURE_2D, maze->gl3dTextTexture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, opengltxt_png, sizeof(opengltxt_png));
# endif

# ifdef USE_FRACTAL_IMAGES
	glGenTextures(1, &maze->fractal1Texture);
	glBindTexture(GL_TEXTURE_2D, maze->fractal1Texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, fractal1_png, sizeof(fractal1_png));

	glGenTextures(1, &maze->fractal2Texture);
	glBindTexture(GL_TEXTURE_2D, maze->fractal2Texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, fractal2_png, sizeof(fractal2_png));

	glGenTextures(1, &maze->fractal3Texture);
	glBindTexture(GL_TEXTURE_2D, maze->fractal3Texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, fractal3_png, sizeof(fractal3_png));

	glGenTextures(1, &maze->fractal4Texture);
	glBindTexture(GL_TEXTURE_2D, maze->fractal4Texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, fractal4_png, sizeof(fractal4_png));
# endif

	glGenTextures(1, &maze->startTexture);
	glBindTexture(GL_TEXTURE_2D, maze->startTexture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	setup_png_texture(mi, start_png, sizeof(start_png));

	glGenTextures(1, &maze->ceilingTexture);
	glBindTexture(GL_TEXTURE_2D, maze->ceilingTexture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	if (!ceilingTexture || !*ceilingTexture
			|| !strcmp(ceilingTexture, "ceiling-tiles")) {
		DEFAULT_CEILING_TEXTURE:
		setup_png_texture(mi, brick2_png, sizeof(brick2_png));
	} else if (!strcmp(ceilingTexture, "brick-wall")) {
		setup_png_texture(mi, brick1_png, sizeof(brick1_png));
	} else if (!strcmp(ceilingTexture, "wood-floor")) {
		setup_png_texture(mi, wood2_png, sizeof(wood2_png));
# ifdef USE_FRACTAL_IMAGES
	} else if (!strcmp(ceilingTexture, "fractal-1")) {
		setup_png_texture(mi, fractal1_png, sizeof(fractal1_png));
		dropAcidCeiling = 1;
	} else if (!strcmp(ceilingTexture, "fractal-2")) {
		setup_png_texture(mi, fractal2_png, sizeof(fractal2_png));
		dropAcidCeiling = 1;
	} else if (!strcmp(ceilingTexture, "fractal-3")) {
		setup_png_texture(mi, fractal3_png, sizeof(fractal3_png));
		dropAcidCeiling = 1;
	} else if (!strcmp(ceilingTexture, "fractal-4")) {
		setup_png_texture(mi, fractal4_png, sizeof(fractal4_png));
		dropAcidCeiling = 1;
# endif
	} else {
		if (!setup_file_texture(mi, ceilingTexture))
			goto DEFAULT_CEILING_TEXTURE;
	}

	glGenTextures(1, &maze->floorTexture);
	glBindTexture(GL_TEXTURE_2D, maze->floorTexture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	if (!floorTexture || !*floorTexture
			|| !strcmp(floorTexture, "wood-floor")) {
		DEFAULT_FLOOR_TEXTURE:
		setup_png_texture(mi, wood2_png, sizeof(wood2_png));
	} else if (!strcmp(floorTexture, "ceiling-tiles")) {
		setup_png_texture(mi, brick2_png, sizeof(brick2_png));
	} else if (!strcmp(floorTexture, "brick-wall")) {
		setup_png_texture(mi, brick1_png, sizeof(brick1_png));
# ifdef USE_FRACTAL_IMAGES
	} else if (!strcmp(floorTexture, "fractal-1")) {
		setup_png_texture(mi, fractal1_png, sizeof(fractal1_png));
		dropAcidFloor = 1;
	} else if (!strcmp(floorTexture, "fractal-2")) {
		setup_png_texture(mi, fractal2_png, sizeof(fractal2_png));
		dropAcidFloor = 1;
	} else if (!strcmp(floorTexture, "fractal-3")) {
		setup_png_texture(mi, fractal3_png, sizeof(fractal3_png));
		dropAcidFloor = 1;
	} else if (!strcmp(floorTexture, "fractal-4")) {
		setup_png_texture(mi, fractal4_png, sizeof(fractal4_png));
		dropAcidFloor = 1;
# endif
	} else {
		if (!setup_file_texture(mi, floorTexture))
			goto DEFAULT_FLOOR_TEXTURE;
	}

	glGenTextures(1, &maze->wallTexture);
	glBindTexture(GL_TEXTURE_2D, maze->wallTexture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mag);
	if (!wallTexture || !*wallTexture || !strcmp(wallTexture, "brick-wall")) {
		DEFAULT_WALL_TEXTURE:
		setup_png_texture(mi, brick1_png, sizeof(brick1_png));
	} else if (!strcmp(wallTexture, "ceiling-tiles")) {
		setup_png_texture(mi, brick2_png, sizeof(brick2_png));
	} else if (!strcmp(wallTexture, "wood-floor")) {
		setup_png_texture(mi, wood2_png, sizeof(wood2_png));
# ifdef USE_FRACTAL_IMAGES
	} else if (!strcmp(wallTexture, "fractal-1")) {
		setup_png_texture(mi, fractal1_png, sizeof(fractal1_png));
		dropAcidWalls = 1;
	} else if (!strcmp(wallTexture, "fractal-2")) {
		setup_png_texture(mi, fractal2_png, sizeof(fractal2_png));
		dropAcidWalls = 1;
	} else if (!strcmp(wallTexture, "fractal-3")) {
		setup_png_texture(mi, fractal3_png, sizeof(fractal3_png));
		dropAcidWalls = 1;
	} else if (!strcmp(wallTexture, "fractal-4")) {
		setup_png_texture(mi, fractal4_png, sizeof(fractal4_png));
		dropAcidWalls = 1;
# endif
	} else {
		if (!setup_file_texture(mi, wallTexture))
			goto DEFAULT_WALL_TEXTURE;
	}
}

static void
initializeGrid(maze_configuration* maze)
{
	unsigned i, j;

	for (i = 0; i < maze->numRows; i++) {
		for (j = 0; j < maze->numColumns; j++) {
			if (isOdd(i) && isOdd(j))
				maze->mazeGrid[i][j] = CELL_UNVISITED;
			else
				maze->mazeGrid[i][j] = WALL;
		}
	}
}

static float
roundToNearestHalf(float num)
{
	return roundf(2.0 * num) / 2.0;
}

static unsigned
isOdd(unsigned num)
{
	return num % 2;
}

static unsigned
isEven(unsigned num)
{
	return !isOdd(num);
}

/*This is the randomized Prim's algorithm.*/
static void
buildMaze(maze_configuration* maze)
{
	Tuple cellToAdd, firstCell = {1, 1};
	maze->mazeGrid[1][1] = CELL;

	addWallsToList(firstCell, maze);

	while (maze->wallListSize > 0) {
		unsigned randomNum = random() % maze->wallListSize;
		Tuple currentWall = maze->wallList[randomNum];

		if (isEven(currentWall.row)) {
			if (maze->mazeGrid[currentWall.row - 1][currentWall.column]
				== CELL
				&& maze->mazeGrid[currentWall.row + 1][currentWall.column]
				== CELL_UNVISITED
			) {
				cellToAdd.row = currentWall.row + 1;
				cellToAdd.column = currentWall.column;
				addCells(cellToAdd, currentWall, maze);
			}
			else if (maze->mazeGrid[currentWall.row + 1][currentWall.column]
				== CELL
				&& maze->mazeGrid[currentWall.row - 1][currentWall.column]
				== CELL_UNVISITED
			) {
				cellToAdd.row = currentWall.row - 1;
				cellToAdd.column = currentWall.column;
				addCells(cellToAdd, currentWall, maze);
			}
		} else {
			if (maze->mazeGrid[currentWall.row][currentWall.column - 1]
				== CELL
				&& maze->mazeGrid[currentWall.row][currentWall.column + 1]
				== CELL_UNVISITED
			) {
				cellToAdd.row = currentWall.row;
				cellToAdd.column = currentWall.column + 1;
				addCells(cellToAdd, currentWall, maze);
			}
			else if (maze->mazeGrid[currentWall.row][currentWall.column + 1]
				== CELL
				&& maze->mazeGrid[currentWall.row][currentWall.column - 1]
				== CELL_UNVISITED
			) {
				cellToAdd.row = currentWall.row;
				cellToAdd.column = currentWall.column - 1;
				addCells(cellToAdd, currentWall, maze);
			}
		}

		removeWallFromList(randomNum, maze);
	}
}

static void
addWallsToList(Tuple cell, maze_configuration* maze)
{
	unsigned i;
	Tuple walls[4];
	walls[0].row = cell.row - 1;
	walls[0].column = cell.column;
	walls[1].row = cell.row + 1;
	walls[1].column = cell.column;
	walls[2].row = cell.row;
	walls[2].column = cell.column - 1;
	walls[3].row = cell.row;
	walls[3].column = cell.column + 1;

	for (i = 0; i < 4; i++) {
		if (isRemovableWall(walls[i], maze)) {
			maze->wallList[maze->wallListSize] = walls[i];
			maze->wallListSize++;
		}
	}
}

static unsigned char
isRemovableWall(Tuple coordinates, maze_configuration* maze)
{
	if (maze->mazeGrid[coordinates.row][coordinates.column] == WALL
		&& coordinates.row > 0
		&& coordinates.row < maze->numRows - 1
		&& coordinates.column > 0
		&& coordinates.column < maze->numColumns - 1
	)
		return 1;
	else
		return 0;
}

static void
addCells(Tuple cellToAdd, Tuple currentWall, maze_configuration* maze)
{
	maze->mazeGrid[currentWall.row][currentWall.column] = CELL;
	maze->mazeGrid[cellToAdd.row][cellToAdd.column] = CELL;
	addWallsToList(cellToAdd, maze);
}

static void
removeWallFromList(unsigned index, maze_configuration* maze)
{
	unsigned i;
	for (i = index + 1; i < maze->wallListSize; i++)
		maze->wallList[i - 1] = maze->wallList[i];

	maze->wallListSize--;
}

static void
placeMiscObjects(maze_configuration* maze)
{
	Rat* rat;
	Tuple temp;
	unsigned char object;
    unsigned numSurroundingWalls = 3;
	unsigned i;

	while (numSurroundingWalls >= 3) {
		numSurroundingWalls = 0;
		maze->startPosition = placeObject(maze, CELL);

		object = maze->mazeGrid[maze->startPosition.row]
			[maze->startPosition.column + 1];
		if (object == WALL || object == WALL_GL_REDBOOK)
			numSurroundingWalls++;
		object = maze->mazeGrid[maze->startPosition.row - 1]
			[maze->startPosition.column];
		if (object == WALL || object == WALL_GL_REDBOOK)
			numSurroundingWalls++;
		object = maze->mazeGrid[maze->startPosition.row]
			[maze->startPosition.column - 1];
		if (object == WALL || object == WALL_GL_REDBOOK)
			numSurroundingWalls++;
		object = maze->mazeGrid[maze->startPosition.row + 1]
			[maze->startPosition.column];
		if (object == WALL || object == WALL_GL_REDBOOK)
			numSurroundingWalls++;
	}
	maze->mazeGrid[maze->startPosition.row][maze->startPosition.column] = START;

	if (maze->mazeGrid[maze->startPosition.row][maze->startPosition.column + 1]
			!= WALL && maze->mazeGrid[maze->startPosition.row]
			[maze->startPosition.column + 1] != WALL_GL_REDBOOK) {
		maze->camera.position.x = (maze->startPosition.column + 1) / 2.0;
		maze->camera.position.z = maze->startPosition.row / 2.0;
		maze->camera.rotation = WEST;
	}
	else if (maze->mazeGrid[maze->startPosition.row - 1]
			[maze->startPosition.column] != WALL
			&& maze->mazeGrid[maze->startPosition.row - 1]
			[maze->startPosition.column] != WALL_GL_REDBOOK) {
		maze->camera.position.x = maze->startPosition.column / 2.0;
		maze->camera.position.z = (maze->startPosition.row - 1) / 2.0;
		maze->camera.rotation = SOUTH;
	}
	else if (maze->mazeGrid[maze->startPosition.row]
			[maze->startPosition.column - 1] != WALL
			&& maze->mazeGrid[maze->startPosition.row]
			[maze->startPosition.column - 1] != WALL_GL_REDBOOK) {
		maze->camera.position.x = (maze->startPosition.column - 1) / 2.0;
		maze->camera.position.z = maze->startPosition.row / 2.0;
		maze->camera.rotation = EAST;
	}
	else {
		maze->camera.position.x = maze->startPosition.column / 2.0;
		maze->camera.position.z = (maze->startPosition.row + 1) / 2.0;
		maze->camera.rotation = NORTH;
	}

	maze->finishPosition = placeObject(maze, FINISH);

	for (i = 0; i < numInverters; i++)
		maze->inverterPosition[i] =
			placeObject(maze, random() % 4 + INVERTER_TETRAHEDRON);

	temp.row = 0;
	temp.column = 0;

# ifdef USE_FLOATING_IMAGES
	for (i = 0; i < numGl3dTexts; i++)
		maze->gl3dTextPosition[i] =
			placeObject(maze, GL_3D_TEXT);
# endif

	for (i = 0; i < numRats; i++) {
		rat = &(maze->rats[i]);
		temp = placeObject(maze, CELL);
		rat->position.x = temp.column / 2.0;
		rat->position.z = temp.row / 2.0;
		rat->state = WALKING;

		if (temp.row == 0 && temp.column == 0) {
			continue;
		}

		if (maze->mazeGrid[(int)(rat->position.z * 2)]
				[(int)(rat->position.x * 2) + 1]
				!= WALL && maze->mazeGrid[(int)(rat->position.z * 2)]
				[(int)(rat->position.x * 2) + 1] != WALL_GL_REDBOOK)
			rat->rotation = EAST;
		else if (maze->mazeGrid[(int)(rat->position.z * 2) - 1]
				[(int)(rat->position.x * 2)]
				!= WALL && maze->mazeGrid[(int)(rat->position.z * 2) - 1]
				[(int)(rat->position.x * 2)] != WALL_GL_REDBOOK)
			rat->rotation = NORTH;
		else if (maze->mazeGrid[(int)(rat->position.z * 2)]
				[(int)(rat->position.x * 2) - 1]
				!= WALL && maze->mazeGrid[(int)(rat->position.z * 2)]
				[(int)(rat->position.x * 2) - 1] != WALL_GL_REDBOOK)
			rat->rotation = WEST;
		else
			rat->rotation = SOUTH;
	}

# ifdef USE_FLOATING_IMAGES
	for (i = 0; i < numGlRedbooks; i++) {
		while (!(((isOdd(temp.row) && isEven(temp.column))
					|| (isEven(temp.row) && isOdd(temp.column)))
					&& maze->mazeGrid[temp.row][temp.column] == WALL)) {
			temp.row = random() % maze->numRows;
			temp.column = random() % maze->numColumns;
		}

		maze->mazeGrid[temp.row][temp.column] = WALL_GL_REDBOOK;
	}
# endif
}

static Tuple
placeObject(maze_configuration* maze, unsigned char type)
{
	Tuple position = {0, 0};

	while (!(maze->mazeGrid[position.row][position.column] == CELL
			&& isOdd(position.row) && isOdd(position.column))) {
		position.row = random() % maze->numRows;
		position.column = random() % maze->numColumns;
	}

	maze->mazeGrid[position.row][position.column] = type;
	return position;
}

ENTRYPOINT void
reshape_maze (ModeInfo *mi, int width, int height)
{
	glViewport(0, 0, (GLint) width, (GLint) height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


ENTRYPOINT Bool
maze_handle_event (ModeInfo *mi, XEvent *event)
{
  maze_configuration *maze = &mazes[MI_SCREEN(mi)];
  if (event->xany.type == ButtonPress)
    {
      maze->button_down_p = True;
      return True;
    }
  else if (event->xany.type == ButtonRelease)
    {
      maze->button_down_p = False;
      return True;
    }
  return False;
}

ENTRYPOINT void
init_maze (ModeInfo * mi)
{
	unsigned i;
	maze_configuration *maze;
	GLfloat ambient[] = {0, 0, 0, 1},
			diffuse[] = {1, 1, 1, 1},
			position[] = {0, 2, 0, 0},
			mcolor[] = {1, 1, 1, 1};

	MI_INIT(mi, mazes);
	maze = &mazes[MI_SCREEN(mi)];

	maze->glx_context = init_GL(mi);

	reshape_maze(mi, MI_WIDTH(mi), MI_HEIGHT(mi));

    for (i = 0; i < countof(maze->dlists); i++)
      maze->dlists[i] = glGenLists (1);

	maze->numRows = (numRows < 2 ? 5 : numRows * 2 + 1);
	maze->numColumns = (numColumns < 2 ? 5 : numColumns * 2 + 1);

	i = (maze->numRows / 2) * (maze->numColumns / 2) - 2;
	if (i < numInverters) {
		numInverters = i;
		i = 0;
	} else i -= numInverters;

	if (i < numRats) {
		numRats = i;
		i = 0;
	} else i -= numRats;
# ifdef USE_FLOATING_IMAGES
	if (i < numGl3dTexts) {
		numGl3dTexts = i;
		i = 0;
	} else i -= numGl3dTexts;

	if (((maze->numRows - 1) + (maze->numColumns - 1)
				+ ((maze->numRows / 2 - 1) * (maze->numColumns / 2 - 1))) < maze->numGlRedbooks)
		maze->numGlRedbooks = (maze->numRows - 1) + (maze->numColumns - 1)
				+ ((maze->numRows / 2 - 1) * (maze->numColumns / 2 - 1));
# endif

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_LIGHT0);

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mcolor);

	glShadeModel(GL_FLAT);

	maze->mazeGrid = calloc(maze->numRows, sizeof(unsigned char*));
	for (i = 0; i < maze->numRows; i++)
		maze->mazeGrid[i] = calloc(maze->numColumns, sizeof(unsigned char));
	maze->wallList = calloc(((maze->numColumns - 2) / 2) * ((maze->numRows - 2) / 2 + 1)
			+ ((maze->numColumns - 2) / 2 + 1) * ((maze->numRows - 2) / 2), sizeof(Tuple));
	maze->inverterPosition = calloc(numInverters, sizeof(Tuple));
	maze->rats = calloc(numRats, sizeof(Rat));
# ifdef USE_FLOATING_IMAGES
	maze->gl3dTextPosition = calloc(numGl3dTexts, sizeof(Tuple));
#endif

	setup_textures(mi);

	newMaze(maze);

	constructLists(mi);
	refreshRemainingDistanceToTravel(mi);

	maze->camera.isCamera = 1;
	for (i = 0; i < numRats; i++)
		maze->rats[i].isCamera = 0;
}

static void
newMaze(maze_configuration* maze)
{
	maze->camera.state = STARTING;
	maze->camera.inversion = 0;
	maze->wallHeight = 0;
	maze->inverterRotation = 0;
	maze->acidHue = 0;

	initializeGrid(maze);
	buildMaze(maze);
	placeMiscObjects(maze);
}

static void
constructLists(ModeInfo *mi)
{
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];

	glNewList(maze->dlists[ARROW], GL_COMPILE);
		glBegin(GL_POLYGON);
		glVertex2f(0, -0.25);
		glVertex2f(0.146946313, 0.202254249);
		glVertex2f(0, 0.125);
		glVertex2f(-0.146946313, 0.202254249);
		glEnd();
	glEndList();

	glNewList(maze->dlists[SQUARE], GL_COMPILE);
		glBegin(GL_QUADS);
		glVertex2f(-0.176776695, -0.176776695);
		glVertex2f(0.176776695, -0.176776695);
		glVertex2f(0.176776695, 0.176776695);
		glVertex2f(-0.176776695, 0.176776695);
		glEnd();
	glEndList();

	glNewList(maze->dlists[STAR], GL_COMPILE);
		glBegin(GL_TRIANGLE_FAN);
		glVertex2f(0, 0);
		glVertex2f(0, -0.25);
		glVertex2f(0.073473157, -0.101127124);
		glVertex2f(0.237764129, -0.077254249);
		glVertex2f(0.118882065, 0.038627124);
		glVertex2f(0.146946313, 0.202254249);
		glVertex2f(0, 0.125);
		glVertex2f(-0.146946313, 0.202254249);
		glVertex2f(-0.118882065, 0.038627124);
		glVertex2f(-0.237764129, -0.077254249);
		glVertex2f(-0.073473157, -0.101127124);
		glVertex2f(0, -0.25);
		glEnd();
	glEndList();

	glNewList(maze->dlists[TRIANGLE], GL_COMPILE);
		glBegin(GL_POLYGON);
		glVertex2f(0, -0.25);
		glVertex2f(0.216506351, 0.125);
		glVertex2f(-0.216506351, 0.125);
		glEnd();
	glEndList();

	glNewList(maze->dlists[INVERTER_TETRAHEDRON], GL_COMPILE);
		glBegin(GL_TRIANGLES);
		glNormal3f(0.471404521, 0.816496581, 0.333333333);
		glVertex3f(0, 0, 0.25);
		glVertex3f(0.23570226, 0, -0.083333333);
		glVertex3f(-0.11785113, 0.204124145, -0.083333333);

		glNormal3f(-0.942809042, 0, 0.333333333);
		glVertex3f(0, 0, 0.25);
		glVertex3f(-0.11785113, 0.204124145, -0.083333333);
		glVertex3f(-0.11785113, -0.204124145, -0.083333333);

		glNormal3f(0.471404521, -0.816496581, 0.333333333);
		glVertex3f(0, 0, 0.25);
		glVertex3f(-0.11785113, -0.204124145, -0.083333333);
		glVertex3f(0.23570226, 0, -0.083333333);

		glNormal3f(0, 0, -1);
		glVertex3f(0.23570226, 0, -0.083333333);
		glVertex3f(-0.11785113, -0.204124145, -0.083333333);
		glVertex3f(-0.11785113, 0.204124145, -0.083333333);
		glEnd();
	glEndList();

	glNewList(maze->dlists[INVERTER_OCTAHEDRON], GL_COMPILE);
		glBegin(GL_TRIANGLES);
		glNormal3f(0.577350269, 0.577350269, 0.577350269);
		glVertex3f(0, 0, 0.25);
		glVertex3f(0.25, 0, 0);
		glVertex3f(0, 0.25, 0);

		glNormal3f(-0.577350269, 0.577350269, 0.577350269);
		glVertex3f(0, 0, 0.25);
		glVertex3f(0, 0.25, 0);
		glVertex3f(-0.25, 0, 0);

		glNormal3f(-0.577350269, -0.577350269, 0.577350269);
		glVertex3f(0, 0, 0.25);
		glVertex3f(-0.25, 0, 0);
		glVertex3f(0, -0.25, 0);

		glNormal3f(0.577350269, -0.577350269, 0.577350269);
		glVertex3f(0, 0, 0.25);
		glVertex3f(0, -0.25, 0);
		glVertex3f(0.25, 0, 0);

		glNormal3f(0.577350269, -0.577350269, -0.577350269);
		glVertex3f(0.25, 0, 0);
		glVertex3f(0, -0.25, 0);
		glVertex3f(0, 0, -0.25);

		glNormal3f(0.577350269, 0.577350269, -0.577350269);
		glVertex3f(0.25, 0, 0);
		glVertex3f(0, 0, -0.25);
		glVertex3f(0, 0.25, 0);

		glNormal3f(-0.577350269, 0.577350269, -0.577350269);
		glVertex3f(0, 0.25, 0);
		glVertex3f(0, 0, -0.25);
		glVertex3f(-0.25, 0, 0);

		glNormal3f(-0.577350269, -0.577350269, -0.577350269);
		glVertex3f(-0.25, 0, 0);
		glVertex3f(0, 0, -0.25);
		glVertex3f(0, -0.25, 0);
		glEnd();
	glEndList();

	glNewList(maze->dlists[INVERTER_DODECAHEDRON], GL_COMPILE);
		glBegin(GL_POLYGON);
		glNormal3f(0.000000000, 0.000000000, 1.000000000);
		glVertex3f(0.122780868, 0.089205522, 0.198663618);
		glVertex3f(-0.046898119, 0.144337567, 0.198663618);
		glVertex3f(-0.151765500, 0.000000000, 0.198663618);
		glVertex3f(-0.046898119, -0.144337567, 0.198663618);
		glVertex3f(0.122780868, -0.089205522, 0.198663618);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(0.894427191, 0.000000000, 0.447213595);
		glVertex3f(0.198663618, -0.144337567, 0.046898119);
		glVertex3f(0.245561737, 0.000000000, -0.046898119);
		glVertex3f(0.198663618, 0.144337567, 0.046898119);
		glVertex3f(0.122780868, 0.089205522, 0.198663618);
		glVertex3f(0.122780868, -0.089205522, 0.198663618);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(0.276393202, 0.850650808, 0.447213595);
		glVertex3f(0.198663618, 0.144337567, 0.046898119);
		glVertex3f(0.075882750, 0.233543090, -0.046898119);
		glVertex3f(-0.075882750, 0.233543090, 0.046898119);
		glVertex3f(-0.046898119, 0.144337567, 0.198663618);
		glVertex3f(0.122780868, 0.089205522, 0.198663618);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(-0.723606798, 0.525731112, 0.447213595);
		glVertex3f(-0.075882750, 0.233543090, 0.046898119);
		glVertex3f(-0.198663618, 0.144337567, -0.046898119);
		glVertex3f(-0.245561737, 0.000000000, 0.046898119);
		glVertex3f(-0.151765500, 0.000000000, 0.198663618);
		glVertex3f(-0.046898119, 0.144337567, 0.198663618);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(-0.723606798, -0.525731112, 0.447213595);
		glVertex3f(-0.245561737, 0.000000000, 0.046898119);
		glVertex3f(-0.198663618, -0.144337567, -0.046898119);
		glVertex3f(-0.075882750, -0.233543090, 0.046898119);
		glVertex3f(-0.046898119, -0.144337567, 0.198663618);
		glVertex3f(-0.151765500, 0.000000000, 0.198663618);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(0.276393202, -0.850650808, 0.447213595);
		glVertex3f(-0.075882750, -0.233543090, 0.046898119);
		glVertex3f(0.075882750, -0.233543090, -0.046898119);
		glVertex3f(0.198663618, -0.144337567, 0.046898119);
		glVertex3f(0.122780868, -0.089205522, 0.198663618);
		glVertex3f(-0.046898119, -0.144337567, 0.198663618);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(0.723606798, 0.525731112, -0.447213595);
		glVertex3f(0.245561737, 0.000000000, -0.046898119);
		glVertex3f(0.151765500, 0.000000000, -0.198663618);
		glVertex3f(0.046898119, 0.144337567, -0.198663618);
		glVertex3f(0.075882750, 0.233543090, -0.046898119);
		glVertex3f(0.198663618, 0.144337567, 0.046898119);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(0.723606798, -0.525731112, -0.447213595);
		glVertex3f(0.198663618, -0.144337567, 0.046898119);
		glVertex3f(0.075882750, -0.233543090, -0.046898119);
		glVertex3f(0.046898119, -0.144337567, -0.198663618);
		glVertex3f(0.151765500, 0.000000000, -0.198663618);
		glVertex3f(0.245561737, 0.000000000, -0.046898119);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(-0.276393202, 0.850650808, -0.447213595);
		glVertex3f(0.075882750, 0.233543090, -0.046898119);
		glVertex3f(0.046898119, 0.144337567, -0.198663618);
		glVertex3f(-0.122780868, 0.089205522, -0.198663618);
		glVertex3f(-0.198663618, 0.144337567, -0.046898119);
		glVertex3f(-0.075882750, 0.233543090, 0.046898119);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(-0.894427191, 0.000000000, -0.447213595);
		glVertex3f(-0.198663618, 0.144337567, -0.046898119);
		glVertex3f(-0.122780868, 0.089205522, -0.198663618);
		glVertex3f(-0.122780868, -0.089205522, -0.198663618);
		glVertex3f(-0.198663618, -0.144337567, -0.046898119);
		glVertex3f(-0.245561737, 0.000000000, 0.046898119);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(-0.276393202, -0.850650808, -0.447213595);
		glVertex3f(-0.198663618, -0.144337567, -0.046898119);
		glVertex3f(-0.122780868, -0.089205522, -0.198663618);
		glVertex3f(0.046898119, -0.144337567, -0.198663618);
		glVertex3f(0.075882750, -0.233543090, -0.046898119);
		glVertex3f(-0.075882750, -0.233543090, 0.046898119);
		glEnd();

		glBegin(GL_POLYGON);
		glNormal3f(0.000000000, 0.000000000, -1.000000000);
		glVertex3f(0.046898119, -0.144337567, -0.198663618);
		glVertex3f(-0.122780868, -0.089205522, -0.198663618);
		glVertex3f(-0.122780868, 0.089205522, -0.198663618);
		glVertex3f(0.046898119, 0.144337567, -0.198663618);
		glVertex3f(0.151765500, 0.000000000, -0.198663618);
		glEnd();
	glEndList();

	glNewList(maze->dlists[INVERTER_ICOSAHEDRON], GL_COMPILE);
		glBegin(GL_TRIANGLES);
		glNormal3f(0.491123473, 0.356822090, 0.794654473);
		glVertex3f(0.000000000, 0.000000000, 0.250000000);
		glVertex3f(0.223606798, 0.000000000, 0.111803399);
		glVertex3f(0.069098301, 0.212662702, 0.111803399);

		glNormal3f(-0.187592474, 0.577350269, 0.794654473);
		glVertex3f(0.000000000, 0.000000000, 0.250000000);
		glVertex3f(0.069098301, 0.212662702, 0.111803399);
		glVertex3f(-0.180901699, 0.131432778, 0.111803399);

		glNormal3f(-0.607061998, 0.000000000, 0.794654473);
		glVertex3f(0.000000000, 0.000000000, 0.250000000);
		glVertex3f(-0.180901699, 0.131432778, 0.111803399);
		glVertex3f(-0.180901699, -0.131432778, 0.111803399);

		glNormal3f(-0.187592474, -0.577350269, 0.794654473);
		glVertex3f(0.000000000, 0.000000000, 0.250000000);
		glVertex3f(-0.180901699, -0.131432778, 0.111803399);
		glVertex3f(0.069098301, -0.212662702, 0.111803399);

		glNormal3f(0.491123473, -0.356822090, 0.794654473);
		glVertex3f(0.000000000, 0.000000000, 0.250000000);
		glVertex3f(0.069098301, -0.212662702, 0.111803399);
		glVertex3f(0.223606798, 0.000000000, 0.111803399);

		glNormal3f(0.794654473, -0.577350269, 0.187592474);
		glVertex3f(0.223606798, 0.000000000, 0.111803399);
		glVertex3f(0.069098301, -0.212662702, 0.111803399);
		glVertex3f(0.180901699, -0.131432778, -0.111803399);

		glNormal3f(0.982246947, 0.000000000, -0.187592474);
		glVertex3f(0.223606798, 0.000000000, 0.111803399);
		glVertex3f(0.180901699, -0.131432778, -0.111803399);
		glVertex3f(0.180901699, 0.131432778, -0.111803399);

		glNormal3f(0.794654473, 0.577350269, 0.187592474);
		glVertex3f(0.223606798, 0.000000000, 0.111803399);
		glVertex3f(0.180901699, 0.131432778, -0.111803399);
		glVertex3f(0.069098301, 0.212662702, 0.111803399);

		glNormal3f(0.303530999, 0.934172359, -0.187592474);
		glVertex3f(0.069098301, 0.212662702, 0.111803399);
		glVertex3f(0.180901699, 0.131432778, -0.111803399);
		glVertex3f(-0.069098301, 0.212662702, -0.111803399);

		glNormal3f(-0.303530999, 0.934172359, 0.187592474);
		glVertex3f(0.069098301, 0.212662702, 0.111803399);
		glVertex3f(-0.069098301, 0.212662702, -0.111803399);
		glVertex3f(-0.180901699, 0.131432778, 0.111803399);

		glNormal3f(-0.794654473, 0.577350269, -0.187592474);
		glVertex3f(-0.180901699, 0.131432778, 0.111803399);
		glVertex3f(-0.069098301, 0.212662702, -0.111803399);
		glVertex3f(-0.223606798, 0.000000000, -0.111803399);

		glNormal3f(-0.982246947, 0.000000000, 0.187592474);
		glVertex3f(-0.180901699, 0.131432778, 0.111803399);
		glVertex3f(-0.223606798, 0.000000000, -0.111803399);
		glVertex3f(-0.180901699, -0.131432778, 0.111803399);

		glNormal3f(-0.794654473, -0.577350269, -0.187592474);
		glVertex3f(-0.180901699, -0.131432778, 0.111803399);
		glVertex3f(-0.223606798, 0.000000000, -0.111803399);
		glVertex3f(-0.069098301, -0.212662702, -0.111803399);

		glNormal3f(-0.303530999, -0.934172359, 0.187592474);
		glVertex3f(-0.180901699, -0.131432778, 0.111803399);
		glVertex3f(-0.069098301, -0.212662702, -0.111803399);
		glVertex3f(0.069098301, -0.212662702, 0.111803399);

		glNormal3f(0.303530999, -0.934172359, -0.187592474);
		glVertex3f(0.069098301, -0.212662702, 0.111803399);
		glVertex3f(-0.069098301, -0.212662702, -0.111803399);
		glVertex3f(0.180901699, -0.131432778, -0.111803399);

		glNormal3f(0.607061998, 0.000000000, -0.794654473);
		glVertex3f(0.180901699, 0.131432778, -0.111803399);
		glVertex3f(0.180901699, -0.131432778, -0.111803399);
		glVertex3f(0.000000000, 0.000000000, -0.250000000);

		glNormal3f(0.187592474, 0.577350269, -0.794654473);
		glVertex3f(0.180901699, 0.131432778, -0.111803399);
		glVertex3f(0.000000000, 0.000000000, -0.250000000);
		glVertex3f(-0.069098301, 0.212662702, -0.111803399);

		glNormal3f(0.187592474, -0.577350269, -0.794654473);
		glVertex3f(0.180901699, -0.131432778, -0.111803399);
		glVertex3f(-0.069098301, -0.212662702, -0.111803399);
		glVertex3f(0.000000000, 0.000000000, -0.250000000);

		glNormal3f(-0.491123473, 0.356822090, -0.794654473);
		glVertex3f(-0.069098301, 0.212662702, -0.111803399);
		glVertex3f(0.000000000, 0.000000000, -0.250000000);
		glVertex3f(-0.223606798, 0.000000000, -0.111803399);

		glNormal3f(-0.491123473, -0.356822090, -0.794654473);
		glVertex3f(-0.223606798, 0.000000000, -0.111803399);
		glVertex3f(0.000000000, 0.000000000, -0.250000000);
		glVertex3f(-0.069098301, -0.212662702, -0.111803399);
		glEnd();
	glEndList();
}

ENTRYPOINT void
draw_maze (ModeInfo * mi)
{
	unsigned i;
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];
	GLfloat h = (GLfloat) MI_HEIGHT(mi) / MI_WIDTH(mi);

    if (!maze->glx_context)
      return;
	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *maze->glx_context);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	gluPerspective(90, 1/h, 0.05, 100);

    if (MI_WIDTH(mi) > 2560)  /* Retina displays */
      glLineWidth (6);
    else
# ifdef HAVE_MOBILE
      glLineWidth (4);
# else
      glLineWidth (2);
# endif

	glRotatef(maze->camera.inversion, 0, 0, 1);
	glRotatef(maze->camera.rotation, 0, 1, 0);
	glTranslatef(-1 * maze->camera.position.x, -0.5,
			-1 * maze->camera.position.z);

	refreshRemainingDistanceToTravel(mi);

	updateInverterRotation(maze);
	shiftAcidColor(maze);
	step(&maze->camera, maze);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawWalls(mi);
	drawCeiling(mi);
	drawFloor(mi);

	for (i = 0; i < numInverters; i++)
		drawInverter(maze, maze->inverterPosition[i]);

# ifdef USE_FLOATING_IMAGES
	for (i = 0; i < numGl3dTexts; i++)
		drawPane(mi, maze->gl3dTextTexture, maze->gl3dTextPosition[i]);
# endif

	for (i = 0; i < numRats; i++) {
		step(&maze->rats[i], maze);
		drawRat(maze->rats[i].position, maze);
	}

	drawPane(mi, maze->finishTexture, maze->finishPosition);
	drawPane(mi, maze->startTexture, maze->startPosition);

	if (shouldDrawOverlay || maze->button_down_p)
      drawOverlay(mi);

	if (mi->fps_p) do_fps(mi);
    glFinish();
	glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}

static void
shiftAcidColor(maze_configuration* maze)
{
	GLfloat x = 1 - fabs(fmod(maze->acidHue / 60.0, 2) - 1);

	if (0 <= maze->acidHue && maze->acidHue <= 60) {
		maze->acidColor.red = 1;
		maze->acidColor.green = x;
		maze->acidColor.blue = 0;
	} else if (60 <= maze->acidHue && maze->acidHue <= 120) {
		maze->acidColor.red = x;
		maze->acidColor.green = 1;
		maze->acidColor.blue = 0;
	} else if (120 <= maze->acidHue && maze->acidHue <= 180) {
		maze->acidColor.red = 0;
		maze->acidColor.green = 1;
		maze->acidColor.blue = x;
	} else if (180 <= maze->acidHue && maze->acidHue <= 240) {
		maze->acidColor.red = 0;
		maze->acidColor.green = x;
		maze->acidColor.blue = 1;
	} else if (240 <= maze->acidHue && maze->acidHue <= 300) {
		maze->acidColor.red = x;
		maze->acidColor.green = 0;
		maze->acidColor.blue = 1;
	} else {
		maze->acidColor.red = 1;
		maze->acidColor.green = 0;
		maze->acidColor.blue = x;
	}

	maze->acidHue += 75 * maze->camera.remainingDistanceToTravel;
	if (maze->acidHue >= 360) maze->acidHue -= 360;
}

static void
refreshRemainingDistanceToTravel(ModeInfo * mi)
{
	unsigned i;
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];
	maze->camera.remainingDistanceToTravel
		= speed * 1.6 * (MI_DELAY(mi) / 1000000.0);
	for (i = 0; i < numRats; i++)
		maze->rats[i].remainingDistanceToTravel =
			maze->camera.remainingDistanceToTravel;
}

static void
step(Rat* rat, maze_configuration* maze)
{
	GLfloat previousWallHeight = maze->wallHeight;

	if (!rat->isCamera && (maze->wallHeight < 1
				|| (rat->position.x == 0 && rat->position.z == 0)))
		return;

	while (rat->remainingDistanceToTravel > 0) {
		switch(rat->state) {
			case WALKING:
				switch((int)rat->rotation) {
					case NORTH:
						walk(rat, 'z', -1, maze);
						break;
					case EAST:
						walk(rat, 'x', 1, maze);
						break;
					case SOUTH:
						walk(rat, 'z', 1, maze);
						break;
					case WEST:
						walk(rat, 'x', -1, maze);
						break;
					default:
						rat->rotation = 90 * roundf(rat->rotation / 90.0);
						break;
				}
				break;
			case TURNING_LEFT:
				turn(rat, maze);
				break;
			case TURNING_RIGHT:
				turn(rat, maze);
				break;
			case TURNING_AROUND:
				turnAround(rat, maze);
				break;
			case INVERTING:
				invert(maze);
				break;
			case STARTING:
				maze->wallHeight += 0.48 * rat->remainingDistanceToTravel;
				if (maze->wallHeight > 1.0) {
					maze->wallHeight = 1.0;
					rat->remainingDistanceToTravel =
						fabs(previousWallHeight - maze->wallHeight);
					changeState(&maze->camera, maze);
				} else
					rat->remainingDistanceToTravel = 0;
				break;
			case FINISHING:
				if (maze->wallHeight == 0) {
					newMaze(maze);
					rat->remainingDistanceToTravel = 0;
				}
				else if (maze->wallHeight < 0) {
					maze->wallHeight = 0;
					rat->remainingDistanceToTravel =
						fabs(previousWallHeight - maze->wallHeight);
				}
				else {
					maze->wallHeight -= 0.48 * rat->remainingDistanceToTravel;
					rat->remainingDistanceToTravel = 0;
				}
				break;
			default:
				break;
		}
	}
}

static void
walk(Rat* rat, char axis, int sign, maze_configuration* maze)
{
	GLfloat* component = (axis == 'x' ? &rat->position.x : &rat->position.z);
	GLfloat previousPosition = *component;
	int isMultipleOfOneHalf = 0;
	unsigned temp = (unsigned)((*component) * 2.0);

	if (((*component) * 2) == roundf((*component) * 2))
		isMultipleOfOneHalf = 1;
	*component += sign * rat->remainingDistanceToTravel;

	if (!isMultipleOfOneHalf && ((unsigned)((*component) * 2.0)) != temp) {
		*component = roundToNearestHalf(*component);
		rat->remainingDistanceToTravel -=
			fabs((*component) - previousPosition);
		changeState(rat, maze);
	} else
		rat->remainingDistanceToTravel = 0;
	
}

static void
turn(Rat* rat, maze_configuration* maze)
{
	Tuplef rotatingAround;
	GLfloat tangentVectorDirection, previousRotation
		= rat->rotation;

	if (rat->state == TURNING_LEFT) {
		tangentVectorDirection = rat->rotation * (M_PI / 180) + M_PI;
		rotatingAround.x = roundToNearestHalf(rat->position.x
				+ 0.5 * cos(tangentVectorDirection));
		rotatingAround.z = roundToNearestHalf(rat->position.z
				+ 0.5 * sin(tangentVectorDirection));

		rat->rotation -= DEF_ANGULAR_CONVERSION_FACTOR
			* rat->remainingDistanceToTravel;

		if (previousRotation > WEST && rat->rotation <= WEST) {
			rat->rotation = WEST;
			rat->remainingDistanceToTravel -= (M_PI / 180)
				* fabs(previousRotation - rat->rotation);
		} else if (previousRotation > SOUTH && rat->rotation <= SOUTH) {
			rat->rotation = SOUTH;
			rat->remainingDistanceToTravel -= (M_PI / 180)
				* fabs(previousRotation - rat->rotation);
		} else if (previousRotation > EAST && rat->rotation <= EAST) {
			rat->rotation = EAST;
			rat->remainingDistanceToTravel -= (M_PI / 180)
				* fabs(previousRotation - rat->rotation);
		} else if (previousRotation > NORTH && rat->rotation <= NORTH) {
			rat->rotation = NORTH;
			rat->remainingDistanceToTravel -= (M_PI / 180)
				* fabs(previousRotation - rat->rotation);
		} else
			rat->remainingDistanceToTravel = 0;

		tangentVectorDirection = rat->rotation * (M_PI / 180);
	}
	else {
		tangentVectorDirection = rat->rotation * (M_PI / 180);
		rotatingAround.x = roundToNearestHalf(rat->position.x
				+ 0.5 * cos(tangentVectorDirection));
		rotatingAround.z = roundToNearestHalf(rat->position.z
				+ 0.5 * sin(tangentVectorDirection));

		rat->rotation += DEF_ANGULAR_CONVERSION_FACTOR
			* rat->remainingDistanceToTravel;

		if (rat->rotation >= 360) {
			rat->rotation = NORTH;
			rat->remainingDistanceToTravel -= (M_PI / 180)
				* fabs(previousRotation - 360);
		} else if (previousRotation < WEST && rat->rotation >= WEST) {
			rat->rotation = WEST;
			rat->remainingDistanceToTravel -= (M_PI / 180)
				* fabs(previousRotation - rat->rotation);
		} else if (previousRotation < SOUTH && rat->rotation >= SOUTH) {
			rat->rotation = SOUTH;
			rat->remainingDistanceToTravel -= (M_PI / 180)
				* fabs(previousRotation - rat->rotation);
		} else if (previousRotation < EAST && rat->rotation >= EAST) {
			rat->rotation = EAST;
			rat->remainingDistanceToTravel -= (M_PI / 180)
				* fabs(previousRotation - rat->rotation);
		} else
			rat->remainingDistanceToTravel = 0;

		tangentVectorDirection = rat->rotation * (M_PI / 180) + M_PI;
	}

	rat->position.x = rotatingAround.x + 0.5 * cos(tangentVectorDirection);
	rat->position.z = rotatingAround.z + 0.5 * sin(tangentVectorDirection);

	if (rat->rotation < 0)
		rat->rotation += 360;

	if (rat->rotation == NORTH || rat->rotation == EAST
			|| rat->rotation == SOUTH || rat->rotation == WEST) {
		rat->position.x = roundToNearestHalf(rat->position.x);
		rat->position.z = roundToNearestHalf(rat->position.z);
		changeState(rat, maze);
	}
}

static void
turnAround(Rat* rat, maze_configuration* maze)
{
	GLfloat previousRotation = rat->rotation;

	rat->rotation -= 1.5 * DEF_ANGULAR_CONVERSION_FACTOR
		* rat->remainingDistanceToTravel;

	if (previousRotation > rat->desiredRotation
			&& rat->rotation <= rat->desiredRotation) {
		rat->rotation = rat->desiredRotation;
		rat->remainingDistanceToTravel -= (M_PI / 180)
			* fabs(previousRotation - rat->rotation);
		changeState(rat, maze);
	}
	else {
		rat->remainingDistanceToTravel = 0;
		if (rat->rotation < 0) rat->rotation += 360;
	}
}

static void
invert(maze_configuration* maze)
{
	GLfloat previousInversion = maze->camera.inversion;
	int shouldChangeState = 0;
	unsigned cameraX = (unsigned)roundf(maze->camera.position.x * 2),
			 cameraZ = (unsigned)roundf(maze->camera.position.z * 2);

	maze->camera.inversion += 1.5 * DEF_ANGULAR_CONVERSION_FACTOR
		* maze->camera.remainingDistanceToTravel;
	if (previousInversion < 180 && maze->camera.inversion >= 180) {
		maze->camera.inversion = 180;
		maze->camera.remainingDistanceToTravel -= (M_PI / 180)
			* fabs(previousInversion - maze->camera.inversion);
		shouldChangeState = 1;
	}
	else if (maze->camera.inversion >= 360) {
		maze->camera.inversion = 0;
		maze->camera.remainingDistanceToTravel -= (M_PI / 180)
			* fabs(previousInversion - maze->camera.inversion);
		shouldChangeState = 1;
	} else
		maze->camera.remainingDistanceToTravel = 0;

	if (shouldChangeState) {
		switch ((int)maze->camera.rotation) {
			case NORTH:
				maze->mazeGrid[cameraZ - 1][cameraX] = CELL;
				break;
			case EAST:
				maze->mazeGrid[cameraZ][cameraX + 1] = CELL;
				break;
			case SOUTH:
				maze->mazeGrid[cameraZ + 1][cameraX] = CELL;
				break;
			case WEST:
				maze->mazeGrid[cameraZ][cameraX - 1] = CELL;
				break;
			default:
				break;
		}

		changeState(&maze->camera, maze);
	}
}

static void
changeState(Rat* rat, maze_configuration* maze)
{
	unsigned char inFrontOfRat, toTheLeft, straightAhead, toTheRight;
	unsigned ratX = (unsigned)roundf(rat->position.x * 2),
			 ratZ = (unsigned)roundf(rat->position.z * 2);

	switch ((int)rat->rotation) {
		case NORTH:
			inFrontOfRat = maze->mazeGrid[ratZ - 1][ratX];
			toTheLeft = maze->mazeGrid[ratZ - 1][ratX - 1];
			straightAhead = maze->mazeGrid[ratZ - 2][ratX];
			toTheRight = maze->mazeGrid[ratZ - 1][ratX + 1];
			break;
		case EAST:
			inFrontOfRat = maze->mazeGrid[ratZ][ratX + 1];
			toTheLeft = maze->mazeGrid[ratZ - 1][ratX + 1];
			straightAhead = maze->mazeGrid[ratZ][ratX + 2];
			toTheRight = maze->mazeGrid[ratZ + 1][ratX + 1];
			break;
		case SOUTH:
			inFrontOfRat = maze->mazeGrid[ratZ + 1][ratX];
			toTheLeft = maze->mazeGrid[ratZ + 1][ratX + 1];
			straightAhead = maze->mazeGrid[ratZ + 2][ratX];
			toTheRight = maze->mazeGrid[ratZ + 1][ratX - 1];
			break;
		case WEST:
			inFrontOfRat = maze->mazeGrid[ratZ][ratX - 1];
			toTheLeft = maze->mazeGrid[ratZ + 1][ratX - 1];
			straightAhead = maze->mazeGrid[ratZ][ratX - 2];
			toTheRight = maze->mazeGrid[ratZ - 1][ratX - 1];
			break;
		default:
			inFrontOfRat = toTheLeft = straightAhead = toTheRight = CELL;
			break;
	}

	if (rat->isCamera && inFrontOfRat == FINISH)
		rat->state = FINISHING;
	else if (rat->isCamera && inFrontOfRat >= INVERTER_TETRAHEDRON
			&& inFrontOfRat <= INVERTER_ICOSAHEDRON)
		rat->state = INVERTING;
	else if (toTheLeft != WALL && toTheLeft != WALL_GL_REDBOOK)
		rat->state = TURNING_LEFT;
	else if (straightAhead != WALL && straightAhead != WALL_GL_REDBOOK)
		rat->state = WALKING;
	else if (toTheRight != WALL && toTheRight != WALL_GL_REDBOOK)
		rat->state = TURNING_RIGHT;
	else {
		rat->state = TURNING_AROUND;

		switch ((int)rat->rotation) {
			case NORTH:
				rat->desiredRotation = SOUTH;
				break;
			case EAST:
				rat->desiredRotation = WEST;
				break;
			case SOUTH:
				rat->desiredRotation = NORTH;
				break;
			case WEST:
				rat->desiredRotation = EAST;
				break;
			default:
				break;
		}
	}
}

static void
updateInverterRotation(maze_configuration* maze)
{
	maze->inverterRotation += 45 * maze->camera.remainingDistanceToTravel;
}

static void drawInverter(maze_configuration* maze, Tuple coordinates)
{
	unsigned char type = maze->mazeGrid[coordinates.row][coordinates.column];

	if (maze->wallHeight < 1 ||
			type < INVERTER_TETRAHEDRON || type > INVERTER_ICOSAHEDRON
			|| (coordinates.row == 0 && coordinates.column == 0))
		return;

	glEnable(GL_LIGHTING);
	glEnable(GL_CULL_FACE);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glTranslatef(coordinates.column / 2.0, 0.25, coordinates.row / 2.0);
	glRotatef(0.618033989 * maze->inverterRotation, 0, 1, 0);
	glRotatef(maze->inverterRotation, 1, 0, 0);

    if (type >= countof(maze->dlists)) abort();
	glCallList(maze->dlists[type]);

	glPopMatrix();

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
}

static void
drawWalls(ModeInfo * mi)
{
	unsigned i, j;
	Tuple startCoordinates, endCoordinates;

	maze_configuration *maze = &mazes[MI_SCREEN(mi)];

	for (i = 0; i < maze->numRows; i++) {
		for (j = 0; j < maze->numColumns; j++) {
			if (maze->mazeGrid[i][j] == WALL
					|| maze->mazeGrid[i][j] == WALL_GL_REDBOOK) {
				if (maze->mazeGrid[i][j] == WALL) {
					glBindTexture(GL_TEXTURE_2D, maze->wallTexture);
					if (dropAcid || dropAcidWalls)
						glColor3f(maze->acidColor.red, maze->acidColor.green,
								maze->acidColor.blue);
# ifdef USE_FLOATING_IMAGES
				} else {
					glBindTexture(GL_TEXTURE_2D, maze->glTextbookTexture);
					glColor3f(1, 1, 1);
# endif
				}

				if (isOdd(i) && isEven(j)) {
					startCoordinates.row = i / 2;
					startCoordinates.column = j / 2;
					endCoordinates.row = i / 2 + 1;
					endCoordinates.column = j / 2;
					drawWall(startCoordinates, endCoordinates, maze);
				} else if (isEven(i) && isOdd(j)) {
					startCoordinates.row = i / 2;
					startCoordinates.column = j / 2;
					endCoordinates.row = i / 2;
					endCoordinates.column = j / 2 + 1;
					drawWall(startCoordinates, endCoordinates, maze);
				}
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glColor3f(1, 1, 1);
}

static void
drawWall(Tuple startCoordinates, Tuple endCoordinates, maze_configuration* maze)
{
	GLfloat wallHeight = maze->wallHeight;

	glBegin(GL_QUADS);
		if (startCoordinates.row == endCoordinates.row) {
			glTexCoord2f(0, 0);
			glVertex3f(startCoordinates.column, 0, startCoordinates.row);
			glTexCoord2f(1, 0);
			glVertex3f(endCoordinates.column, 0, startCoordinates.row);
			glTexCoord2f(1, 1);
			glVertex3f(endCoordinates.column, wallHeight, endCoordinates.row);
			glTexCoord2f(0, 1);
			glVertex3f(startCoordinates.column, wallHeight, endCoordinates.row);
		} else {
			glTexCoord2f(0, 0);
			glVertex3f(startCoordinates.column, 0, startCoordinates.row);
			glTexCoord2f(1, 0);
			glVertex3f(startCoordinates.column, 0, endCoordinates.row);
			glTexCoord2f(1, 1);
			glVertex3f(endCoordinates.column, wallHeight, endCoordinates.row);
			glTexCoord2f(0, 1);
			glVertex3f(endCoordinates.column, wallHeight, startCoordinates.row);
		}
	glEnd();
}

static void
drawCeiling(ModeInfo * mi)
{
	Tuple farRightCorner;
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];
	farRightCorner.row = maze->numRows / 2;
	farRightCorner.column = maze->numColumns / 2;
	glBindTexture(GL_TEXTURE_2D, maze->ceilingTexture);

	glBegin(GL_QUADS);
		if (dropAcid || dropAcidCeiling)
			glColor3f(maze->acidColor.red, maze->acidColor.green,
					maze->acidColor.blue);
		glTexCoord2f(0, 0);
		glVertex3f(0, 1, 0);
		glTexCoord2f(farRightCorner.column, 0);
		glVertex3f(farRightCorner.column, 1, 0);
		glTexCoord2f(farRightCorner.column, farRightCorner.row);
		glVertex3f(farRightCorner.column, 1, farRightCorner.row);
		glTexCoord2f(0, farRightCorner.row);
		glVertex3f(0, 1, farRightCorner.row);
		glColor3f(1, 1, 1);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
}

static void
drawFloor(ModeInfo * mi)
{
	Tuple farRightCorner;
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];
	farRightCorner.row = maze->numRows / 2;
	farRightCorner.column = maze->numColumns / 2;
	glBindTexture(GL_TEXTURE_2D, maze->floorTexture);

	glBegin(GL_QUADS);
		if (dropAcid || dropAcidFloor)
			glColor3f(maze->acidColor.red, maze->acidColor.green,
					maze->acidColor.blue);
		glTexCoord2f(0, 0);
		glVertex3f(0, 0, 0);
		glTexCoord2f(farRightCorner.column, 0);
		glVertex3f(farRightCorner.column, 0, 0);
		glTexCoord2f(farRightCorner.column, farRightCorner.row);
		glVertex3f(farRightCorner.column, 0, farRightCorner.row);
		glTexCoord2f(0, farRightCorner.row);
		glVertex3f(0, 0, farRightCorner.row);
		glColor3f(1, 1, 1);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
}

static void
drawPane(ModeInfo *mi, GLuint texture, Tuple position)
{
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];
	if (position.row == 0 && position.column == 0) return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, texture);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(position.column / 2.0, 0, position.row / 2.0);
	glRotatef(-1 * maze->camera.rotation - 90, 0, 1, 0);

    if (MI_WIDTH(mi) < MI_HEIGHT(mi))
      {
        /* Keep the Start button readable in phone portrait mode. */
        glScalef (0.5, 0.5, 0.5);
        glTranslatef (0, 0.5, 0);
      }

	glBegin(GL_QUADS);
		glColor4f(1, 1, 1, 0.9);
		glTexCoord2f(0, 0);
		glVertex3f(0, 0, 0.5);
		glTexCoord2f(1, 0);
		glVertex3f(0, 0, -0.5);
		glTexCoord2f(1, 1);
		glVertex3f(0, maze->wallHeight, -0.5);
		glTexCoord2f(0, 1);
		glVertex3f(0, maze->wallHeight, 0.5);
		glColor3f(1, 1, 1);
	glEnd();

	glPopMatrix();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
}

static void
drawRat(Tuplef position, maze_configuration* maze)
{
	if (position.x == 0 && position.z == 0) return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, maze->ratTexture);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(position.x, 0, position.z);
	glRotatef(-1 * maze->camera.rotation - 90, 0, 1, 0);

    glScalef (0.25, 0.25, 0.25);
	glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex3f(0, 0, 0.5);
		glTexCoord2f(1, 0);
		glVertex3f(0, 0, -0.5);
		glTexCoord2f(1, 1);
		glVertex3f(0, maze->wallHeight, -0.5);
		glTexCoord2f(0, 1);
		glVertex3f(0, maze->wallHeight, 0.5);
	glEnd();

	glPopMatrix();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
}


static void drawOverlay(ModeInfo *mi)
{
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];
	unsigned i, j;
    GLfloat h = (GLfloat) MI_HEIGHT(mi) / MI_WIDTH(mi);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
    glOrtho(-1/h, 1/h, 1, -1, -1, 1);

	glEnable(GL_BLEND);
	glColor4f(0, 0, 1, 0.75);
	glScalef(0.25, 0.25, 0.25);

	glCallList(maze->dlists[ARROW]);

	glRotatef(maze->camera.inversion, 0, 1, 0);
	glRotatef(maze->camera.rotation, 0, 0, -1);
	glTranslatef(-maze->camera.position.x, -maze->camera.position.z, 0);
	glColor4f(1, 1, 1, 0.75);
	
	glBegin(GL_LINES);
	for (i = 0; i < maze->numRows; i++) {
		for (j = 0; j < maze->numColumns; j++) {
			if (maze->mazeGrid[i][j] == WALL
					|| maze->mazeGrid[i][j] == WALL_GL_REDBOOK) {
				if (isOdd(i) && isEven(j)) {
						glVertex2f(j / 2, i / 2);
						glVertex2f(j / 2, i / 2 + 1);
				} else if (isEven(i) && isOdd(j)) {
						glVertex2f(j / 2, i / 2);
						glVertex2f(j / 2 + 1, i / 2);
				}
			}
		}
	}
	glEnd();

	glColor4f(1, 0, 0, 0.75);

	glPushMatrix();
	glTranslatef(maze->startPosition.column / 2.0,
			maze->startPosition.row / 2.0, 0);
	glCallList(maze->dlists[SQUARE]);
	glPopMatrix();

	glColor4f(1, 1, 0, 0.75);

	glPushMatrix();
	glTranslatef(maze->finishPosition.column / 2.0,
			maze->finishPosition.row / 2.0, 0);
	glCallList(maze->dlists[STAR]);
	glPopMatrix();

	glColor4f(1, 0.607843137, 0, 0.75);

	for (i = 0; i < numRats; i++) {
		if (maze->rats[i].position.x == 0 && maze->rats[i].position.z == 0)
			continue;
		glPushMatrix();
		glTranslatef(maze->rats[i].position.x, maze->rats[i].position.z, 0);
		glRotatef(maze->rats[i].rotation, 0, 0, 1);
		glCallList(maze->dlists[ARROW]);
		glPopMatrix();
	}

	glColor4f(1, 1, 1, 1);

	for (i = 0; i < numInverters; i++) {
		j = maze->mazeGrid[maze->inverterPosition[i].row]
			[maze->inverterPosition[i].column];
		if (j >= INVERTER_TETRAHEDRON && j <= INVERTER_ICOSAHEDRON) {
			glPushMatrix();
			glTranslatef(maze->inverterPosition[i].column / 2.0,
					maze->inverterPosition[i].row / 2.0, 0);
			glRotatef(1.5 * maze->inverterRotation, 0, 0, 1);
			glCallList(maze->dlists[TRIANGLE]);
			glPopMatrix();
		}
	}

	glDisable(GL_BLEND);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

ENTRYPOINT void
free_maze (ModeInfo * mi)
{
	maze_configuration *maze = &mazes[MI_SCREEN(mi)];
    int i;

    if (!maze->glx_context) return;
	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *maze->glx_context);

    glDeleteTextures(1, &maze->wallTexture);
    glDeleteTextures(1, &maze->floorTexture);
    glDeleteTextures(1, &maze->ceilingTexture);
    glDeleteTextures(1, &maze->startTexture);
    glDeleteTextures(1, &maze->ratTexture);
    glDeleteTextures(1, &maze->finishTexture);
# ifdef USE_FLOATING_IMAGES
    glDeleteTextures(1, &maze->glTextbookTexture);
    glDeleteTextures(1, &maze->gl3dTextTexture);
# endif
# ifdef USE_FRACTAL_IMAGES
    glDeleteTextures(1, &maze->fractal1Texture);
    glDeleteTextures(1, &maze->fractal2Texture);
    glDeleteTextures(1, &maze->fractal3Texture);
    glDeleteTextures(1, &maze->fractal4Texture);
# endif

    for (i = 0; i < countof(maze->dlists); i++)
      if (glIsList(maze->dlists[i])) glDeleteLists(maze->dlists[i], 1);

    free(maze->mazeGrid);
    free(maze->wallList);
    free(maze->inverterPosition);
    free(maze->gl3dTextPosition);
    free(maze->rats);
}


XSCREENSAVER_MODULE_2 ("Maze3D", maze3d, maze)

#endif
