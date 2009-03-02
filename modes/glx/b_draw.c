#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)b_draw.c  4.11 98/06/16 xlockmore";

#endif

/*-
 * BUBBLE3D (C) 1998 Richard W.M. Jones.
 * b_draw.c: This code creates new bubbles, manages them and controls
 * them as they are drawn on the screen.
 */

#include "bubble3d.h"

typedef struct draw_context {
	/* The list of bubbles currently on the screen. */
	void      **bubble_list;
	int         nr_bubbles;

	/* When was the last time we created a new bubble? */
	int         bubble_count;
} draw_context;

void       *
glb_draw_init(void)
{
	draw_context *c;

	GLfloat     mat_specular[] =
	{1, 1, 1, 1};
	GLfloat     mat_emission[] =
	{0, 0, 0, 1};
	GLfloat     mat_shininess[] =
	{100};
	GLfloat     ambient[] =
	{0.5, 0.5, 0.5, 1.0};
	GLfloat     light_position[][4] =
	{
		{0, -1, 0, 0},
		{1, 1, 0, 0},
		{-1, 0, 1, 0}};
	GLfloat     light_diffuse[][4] =
	{
		{1, 1, 1, 1},
		{1, 1, 1, 1},
		{1, 1, 1, 1}};
	GLfloat     light_specular[][4] =
	{
		{1, 1, 1, 1},
		{1, 1, 1, 1},
		{1, 1, 1, 1}};

	/* Initialize the context. */
	c = (struct draw_context *) malloc(sizeof (struct draw_context));

	if (c == 0)
		return 0;
	c->bubble_list = 0;
	c->nr_bubbles = 0;
	c->bubble_count = glb_config.create_bubbles_every;

	/* Do some GL initialization. */
	glClearColor(glb_config.bg_colour[0],
		     glb_config.bg_colour[1],
		     glb_config.bg_colour[2],
		     glb_config.bg_colour[3]);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, glb_config.bubble_colour);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mat_emission);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

#if GLB_USE_BLENDING
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glEnable(GL_LIGHT2);
#if GLB_USE_BLENDING
	glEnable(GL_BLEND);
#else
	glEnable(GL_DEPTH_TEST);
#endif
	glEnable(GL_AUTO_NORMAL);
	glEnable(GL_NORMALIZE);

#if GLB_USE_BLENDING
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
#endif
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position[0]);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse[0]);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular[0]);
	glLightfv(GL_LIGHT1, GL_POSITION, light_position[1]);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse[1]);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular[1]);
	glLightfv(GL_LIGHT2, GL_POSITION, light_position[2]);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, light_diffuse[2]);
	glLightfv(GL_LIGHT2, GL_SPECULAR, light_specular[2]);

	return c;
}

void
glb_draw_end(void *cc)
{
	draw_context *c = (draw_context *) cc;

	(void) free((void *) c->bubble_list);
	(void) free((void *) c);
}

static int
create_new_bubbles(draw_context * c)
{
	int         n, i;
	double      r = glb_drand();
	GLfloat     size, speed, scale_incr, x, y, z;
	void       *b[4];
	void      **old_bubble_list;

	/* How many bubbles to make? */
	if (r < glb_config.p_bubble_group[0])
		n = 1;
	else if (r < glb_config.p_bubble_group[1])
		n = 2;
	else if (r < glb_config.p_bubble_group[2])
		n = 3;
	else
		n = 4;

	/* Initial position of top-most bubble in group. */
	x = glb_drand() * 4 - 2;
	y = glb_config.screen_bottom;
	z = glb_drand() * 2 - 2;

	/* What size? */
	size = glb_config.min_size
		+ glb_drand() * (glb_config.max_size - glb_config.min_size);

	/* What speed? */
	speed = glb_config.min_speed
		+ glb_drand() * (glb_config.max_speed - glb_config.min_speed);

	/* Work out the scaling increment. Bubbles should increase by scale_factor
	 * as they go from bottom to top of screen.
	 */
	scale_incr = (size * glb_config.scale_factor - size)
		/ ((glb_config.screen_top - glb_config.screen_bottom) / speed);

	/* Create the bubble(s). */
	for (i = 0; i < n; ++i) {
		if ((b[i] = glb_bubble_new(x, y, z, size, speed, scale_incr)) == 0) {
			/* Out of memory - recover. */
			i--;
			while (i >= 0)
				glb_bubble_delete(b[i]);
			return 0;
		}
		/* Create the next bubble below the last bubble. */
		y -= size * 3;
	}

	/* Add the bubbles to the list. */
	c->nr_bubbles += n;
	old_bubble_list = c->bubble_list;
	c->bubble_list = (void **) realloc(c->bubble_list,
					   c->nr_bubbles * sizeof (void *));

	if (c->bubble_list == 0) {
		/* Out of memory - recover. */
		for (i = 0; i < n; ++i)
			glb_bubble_delete(b[i]);
		c->bubble_list = old_bubble_list;
		c->nr_bubbles -= n;
		return 0;
	}
	for (i = 0; i < n; ++i)
		c->bubble_list[c->nr_bubbles - i - 1] = b[i];

	return 1;
}

static void
delete_bubble(draw_context * c, int j)
{
	int         i;

	glb_bubble_delete(c->bubble_list[j]);

	for (i = j; i < c->nr_bubbles - 1; ++i)
		c->bubble_list[i] = c->bubble_list[i + 1];

	c->nr_bubbles--;
}

void
glb_draw_step(void *cc)
{
	draw_context *c = (draw_context *) cc;
	int         i;

	/* Consider creating a new bubble or bubbles. */
	if (c->nr_bubbles < glb_config.max_bubbles &&
	    c->bubble_count++ > glb_config.create_bubbles_every ) {
		if (create_new_bubbles(c))
			c->bubble_count = 0;
	}
	/* Clear the display. */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* XXX Draw the background here ... */

	/* Draw all the bubbles on the display. */
	for (i = 0; i < c->nr_bubbles; ++i) {
		void       *b = c->bubble_list[i];

		glb_bubble_step(b);
		glb_bubble_draw(b);

		/* Has the bubble reached the top of the screen? */
		if (glb_bubble_get_y(b) >= glb_config.screen_top) {
			delete_bubble(c, i);
			i--;
		}
	}
}
