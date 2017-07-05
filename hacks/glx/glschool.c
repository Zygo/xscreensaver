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

#define DEF_NFISH	"100"
#define DEF_FOG		"False"
#define DEF_DRAWBBOX	"True"
#define DEF_DRAWGOAL	"False"
#define DEF_GOALCHGF	"50"
#define DEF_MAXVEL	"7.0"
#define DEF_MINVEL	"1.0"
#define DEF_ACCLIMIT	"8.0"
#define DEF_DISTEXP	"2.2"
#define DEF_AVOIDFACT	"1.5"
#define DEF_MATCHFACT	"0.15"
#define DEF_CENTERFACT	"0.1"
#define DEF_TARGETFACT	"80"
#define DEF_MINRADIUS	"30.0"
#define DEF_MOMENTUM	"0.9"
#define DEF_DISTCOMP	"10.0"

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
	{&NFish,		"nfish",		"NFish",		DEF_NFISH, t_Int},
	{&DoFog,		"fog",			"DoFog",		DEF_FOG, t_Bool},
	{&DoDrawBBox,	"drawbbox",		"DoDrawBBox",	DEF_DRAWBBOX, t_Bool},
	{&DoDrawGoal,	"drawgoal",		"DoDrawGoal",	DEF_DRAWGOAL, t_Bool},
	{&GoalChgFreq,	"goalchgf",		"GoalChgFreq",	DEF_GOALCHGF,  t_Int},
	{&MaxVel,		"maxvel",		"MaxVel",		DEF_MAXVEL,  t_Float},
	{&MinVel,		"minvel",		"MinVel",		DEF_MINVEL,	t_Float},
	{&AccLimit,		"acclimit",		"AccLimit",		DEF_ACCLIMIT,  t_Float},
	{&DistExp,		"distexp",		"DistExp",		DEF_DISTEXP,  t_Float},
	{&AvoidFact,	"avoidfact",	"AvoidFact",	DEF_AVOIDFACT,  t_Float},
	{&MatchFact,	"matchfact",	"MatchFact",	DEF_MATCHFACT,  t_Float},
	{&CenterFact,	"centerfact",	"CenterFact",	DEF_CENTERFACT,  t_Float},
	{&TargetFact,	"targetfact",	"TargetFact",	DEF_TARGETFACT,  t_Float},
	{&MinRadius,	"minradius",	"MinRadius",	DEF_MINRADIUS,  t_Float},
	{&Momentum,		"momentum",		"Momentum",		DEF_MOMENTUM,  t_Float},
	{&DistComp,		"distcomp",		"DistComp",		DEF_DISTCOMP,  t_Float},
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
        int		fish_polys, box_polys;
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
		glschool_setBBox(sc->school, -aspect*160, aspect*160, -130, 130, -450, -50.0);
		glDeleteLists(sc->bboxList, 1);
                glschool_createBBoxList(&SCHOOL_BBOX(sc->school),
                                        &sc->bboxList, wire);
	}
	glschool_reshape(width, height);
}

ENTRYPOINT void
init_glschool(ModeInfo *mi)
{
	int						width = MI_WIDTH(mi);
	int						height = MI_HEIGHT(mi);
	Bool					wire = MI_IS_WIREFRAME(mi);
	glschool_configuration	*sc;

	MI_INIT (mi, scs, NULL);
	sc = &scs[MI_SCREEN(mi)];

	sc->drawGoal = DoDrawGoal;
	sc->drawBBox = DoDrawBBox;

	sc->nColors = 360;
	sc->context = init_GL(mi);
	sc->colors = (XColor *)calloc(sc->nColors, sizeof(XColor));
	make_color_ramp(0, 0, 0,
					0.0, 1.0, 1.0,
					359.0, 1.0, 1.0,
					sc->colors, &sc->nColors,
					False, 0, False);

	sc->school = glschool_initSchool(NFish, AccLimit, MaxVel, MinVel, DistExp, Momentum,
							MinRadius, AvoidFact, MatchFact, CenterFact, TargetFact,
							DistComp);
	if (sc->school == (School *)0) {
		fprintf(stderr, "couldn't initialize TheSchool, exiting\n");
		exit(1);
	}

	reshape_glschool(mi, width, height);

	glschool_initGLEnv(DoFog);
	glschool_initFishes(sc->school);
	glschool_createDrawLists(&SCHOOL_BBOX(sc->school), 
                                 &sc->bboxList, &sc->goalList, &sc->fishList,
                                 &sc->fish_polys, &sc->box_polys, wire);
	glschool_computeAccelerations(sc->school);
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

        mi->polygon_count = 0;

	if ((sc->goalCounter % GoalChgFreq) == 0)
		glschool_newGoal(sc->school);
	sc->goalCounter++;

	sc->rotCounter++;
	sc->rotCounter = (sc->rotCounter%360);

	glschool_applyMovements(sc->school);
	glschool_drawSchool(sc->colors, sc->school, sc->bboxList, 
                            sc->goalList, sc->fishList, sc->rotCounter, 
                              sc->drawGoal, sc->drawBBox, 
                            sc->fish_polys, sc->box_polys,
                            &mi->polygon_count);
	glschool_computeAccelerations(sc->school);

	if (mi->fps_p)
		do_fps(mi);

	glFinish();
	glXSwapBuffers(dpy, window);
}

XSCREENSAVER_MODULE("GLSchool", glschool)
