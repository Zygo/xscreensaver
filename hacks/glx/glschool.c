/* glschool.c, Copyright (c) 2005-2006 David C. Lambert <dcl@panix.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */
#include "xlockmore.h"
#include "glschool.h"

#define sws_opts			xlockmore_opts
#define DEFAULTS    "*delay:		20000       \n" \
                    "*showFPS:      False       \n" \
                    "*wireframe:    False       \n" \

#define refresh_glschool		(0)
#define release_glschool		(0)
#define glschool_handle_event	(0)

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static int			NFish;
static Bool			DoFog;
static Bool			DoDrawBBox;
static Bool			DoDrawGoal;
static int			GoalChgFreq;
static float		MinVel;
static float		MaxVel;
static float		DistExp;
static float		AccLimit;
static float		AvoidFact;
static float		MatchFact;
static float		TargetFact;
static float		CenterFact;
static float		MinRadius;
static float		Momentum;
static float		DistComp;

static XrmOptionDescRec opts[] = {
	{ "-nfish",		".nfish",		XrmoptionSepArg, 0 },
	{ "-fog",		".fog",			XrmoptionNoArg, "True" },
	{ "+fog",		".fog",			XrmoptionNoArg, "False" },
	{ "-drawgoal",	".drawgoal",	XrmoptionNoArg, "True" },
	{ "+drawgoal",	".drawgoal",	XrmoptionNoArg, "False" },
	{ "-drawbbox",	".drawbbox",	XrmoptionNoArg, "True" },
	{ "+drawbbox",	".drawbbox",	XrmoptionNoArg, "False" },
	{ "-goalchgf",	".goalchgf",	XrmoptionSepArg, 0 },
	{ "-maxvel",	".maxvel",		XrmoptionSepArg, 0 },
	{ "-minvel",	".minvel",		XrmoptionSepArg, 0 },
	{ "-acclimit",	".acclimit",	XrmoptionSepArg, 0 },
	{ "-distexp",	".distexp",		XrmoptionSepArg, 0 },
	{ "-avoidfact",	".avoidfact",	XrmoptionSepArg, 0 },
	{ "-matchfact",	".matchfact",	XrmoptionSepArg, 0 },
	{ "-centerfact",".centerfact",	XrmoptionSepArg, 0 },
	{ "-targetfact",".targetfact",	XrmoptionSepArg, 0 },
	{ "-minradius",	".minradius",	XrmoptionSepArg, 0 },
	{ "-distcomp",	".distcomp",	XrmoptionSepArg, 0 },
	{ "-momentum",	".momentum",	XrmoptionSepArg, 0 },
};

static argtype vars[] = {
	{&NFish,		"nfish",		"NFish",		"100", t_Int},
	{&DoFog,		"fog",			"DoFog",		"False", t_Bool},
	{&DoDrawBBox,	"drawbbox",		"DoDrawBBox",	"True", t_Bool},
	{&DoDrawGoal,	"drawgoal",		"DoDrawGoal",	"False", t_Bool},
	{&GoalChgFreq,	"goalchgf",		"GoalChgFreq",	"50",  t_Int},
	{&MaxVel,		"maxvel",		"MaxVel",		"7.0",  t_Float},
	{&MinVel,		"minvel",		"MinVel",		"1.0",	t_Float},
	{&AccLimit,		"acclimit",		"AccLimit",		"8.0",  t_Float},
	{&DistExp,		"distexp",		"DistExp",		"2.2",  t_Float},
	{&AvoidFact,	"avoidfact",	"AvoidFact",	"1.5",  t_Float},
	{&MatchFact,	"matchfact",	"MatchFact",	"0.15",  t_Float},
	{&CenterFact,	"centerfact",	"CenterFact",	"0.1",  t_Float},
	{&TargetFact,	"targetfact",	"TargetFact",	"80",  t_Float},
	{&MinRadius,	"minradius",	"MinRadius",	"30.0",  t_Float},
	{&Momentum,		"momentum",		"Momentum",		"0.9",  t_Float},
	{&DistComp,		"distcomp",		"DistComp",		"10.0",  t_Float},
};

ENTRYPOINT ModeSpecOpt glschool_opts = {countof(opts), opts, countof(vars), vars, NULL};

typedef struct {
	int			nColors;
	int			rotCounter;
	int			goalCounter;
	Bool		drawGoal;
	Bool		drawBBox;			
	GLuint		bboxList;
	GLuint		goalList;
	GLuint		fishList;
	XColor		*colors;
	School		*school;
	GLXContext	*context;
} glschool_configuration;

static glschool_configuration	*scs = NULL;

ENTRYPOINT void
reshape_glschool(ModeInfo *mi, int width, int height)
{
	Bool					wire = MI_IS_WIREFRAME(mi);
	double					aspect = (double)width/(double)height;
	glschool_configuration	*sc = &scs[MI_SCREEN(mi)];

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sc->context));
	if (sc->school != (School *)0) {
		setBBox(sc->school, -aspect*160, aspect*160, -130, 130, -450, -50.0);
		glDeleteLists(sc->bboxList, 1);
		createBBoxList(&SCHOOL_BBOX(sc->school), &sc->bboxList, wire);
	}
	reshape(width, height);
}

ENTRYPOINT void
init_glschool(ModeInfo *mi)
{
	int						width = MI_WIDTH(mi);
	int						height = MI_HEIGHT(mi);
	Bool					wire = MI_IS_WIREFRAME(mi);
	glschool_configuration	*sc;

	if (!scs) {
		scs = (glschool_configuration *)calloc(MI_NUM_SCREENS(mi), sizeof(glschool_configuration));
		if (!scs) {
			perror("init_glschool: ");
			exit(1);
		}
	}
	sc = &scs[MI_SCREEN(mi)];

	sc->drawGoal = DoDrawGoal;
	sc->drawBBox = DoDrawBBox;

	sc->nColors = 360;
	sc->context = init_GL(mi);
	sc->colors = (XColor *)calloc(sc->nColors, sizeof(XColor));
	make_color_ramp(0, 0,
					0.0, 1.0, 1.0,
					359.0, 1.0, 1.0,
					sc->colors, &sc->nColors,
					False, 0, False);

	sc->school = initSchool(NFish, AccLimit, MaxVel, MinVel, DistExp, Momentum,
							MinRadius, AvoidFact, MatchFact, CenterFact, TargetFact,
							DistComp);
	if (sc->school == (School *)0) {
		fprintf(stderr, "couldn't initialize TheSchool, exiting\n");
		exit(1);
	}

	reshape_glschool(mi, width, height);

	initGLEnv(DoFog);
	initFishes(sc->school);
	createDrawLists(&SCHOOL_BBOX(sc->school), &sc->bboxList, &sc->goalList, &sc->fishList, wire);
	computeAccelerations(sc->school);
}

ENTRYPOINT void
draw_glschool(ModeInfo *mi)
{
	Window					window = MI_WINDOW(mi);
	Display					*dpy = MI_DISPLAY(mi);
	glschool_configuration	*sc = &scs[MI_SCREEN(mi)];

	if (!sc->context) {
		fprintf(stderr, "no context\n");
		return;
	}

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sc->context));

	if ((sc->goalCounter % GoalChgFreq) == 0)
		newGoal(sc->school);
	sc->goalCounter++;

	sc->rotCounter++;
	sc->rotCounter = (sc->rotCounter%360);

	applyMovements(sc->school);
	drawSchool(sc->colors, sc->school, sc->bboxList, sc->goalList, sc->fishList, sc->rotCounter, sc->drawGoal, sc->drawBBox);
	computeAccelerations(sc->school);

	if (mi->fps_p)
		do_fps(mi);

	glFinish();
	glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE("GLSchool", glschool)
