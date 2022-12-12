/* topblock, Copyright (c) 2006-2012 rednuht <topblock.xscreensaver@jumpstation.co.uk>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * 
 *

topBlock - a simple openGL 3D hack of falling blocks
based on jwz's dangerball hack

The proporations of the blocks and their features is not even close to the commercial building block products offered by a variety companies.

information on this hack might be found at 
http://www.jumpstation.co.uk/xscreensaver/topblock/

History
25/02/2006 v1.0 release
29/04/2006 v1.11 updated to better fit with xscreensaver v5
                 colors defaults to 7 (no black)
19/06/2006 v1.2  fixed dropSpeed = 7 bug, added gltrackball support and some code neatening, thanks to Valdis Kletnieks and JWZ for their input.
*/

#include <math.h>

# define release_topBlock 0

#define DEFAULTS	"*delay:	10000       \n" \
			"*count:        30           \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \

#include "xlockmore.h"
#include "topblock.h"
#include "sphere.h"
#include "tube.h"
#include "gltrackball.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#ifndef HAVE_JWXYZ
# include <GL/glu.h>
#endif

typedef struct
{
  GLXContext *glx_context;
  trackball_state *trackball;
  GLfloat rotateSpeed;
  GLfloat dropSpeed;
  int maxFalling;
  int resolution;
  Bool button_down_p;
  int numFallingBlocks;
  GLfloat highest,highestFalling;
  GLfloat eyeLine,eyeX,eyeY,eyeZ;
  int carpetWidth, carpetLength;
  int followMode;
  GLfloat followRadius,followAngle;
  int plusheight;
  GLuint	carpet;
  GLuint	block;
  int carpet_polys, block_polys;
  NODE *blockNodeRoot;
  NODE *blockNodeFollow;
  GLfloat rotation;
} topBlockSTATE;

/* parameter vars */
static Bool override;
static Bool rotate;
static Bool follow;
static Bool drawCarpet;
static Bool drawBlob;
static Bool drawNipples;
static GLfloat rotateSpeed;
static GLfloat camX;
static GLfloat camY;
static GLfloat camZ;
static GLfloat dropSpeed;
static int maxFalling;
static int maxColors;
static int size;
static int spawn;
static int resolution;

static XrmOptionDescRec opts[] = {
  { "-size",        ".size",        XrmoptionSepArg, 0 },
  { "-spawn",       ".spawn",       XrmoptionSepArg, 0 },
  { "-camX",        ".camX",        XrmoptionSepArg, 0 },
  { "-camY",        ".camY",        XrmoptionSepArg, 0 },
  { "-camZ",        ".camZ",        XrmoptionSepArg, 0 },
  { "+rotate",      ".rotate",      XrmoptionNoArg, "False" },
  { "-rotate",      ".rotate",      XrmoptionNoArg, "True" },
  { "+carpet",      ".carpet",      XrmoptionNoArg, "False" },
  { "+nipples",     ".nipples",     XrmoptionNoArg, "False" },
  { "-blob",        ".blob",        XrmoptionNoArg, "True" },
  { "-rotateSpeed", ".rotateSpeed", XrmoptionSepArg, 0 },
  { "-follow",      ".follow",      XrmoptionNoArg, "True" },
  { "-maxFalling",  ".maxFalling",  XrmoptionSepArg, 0 },
  { "-resolution",  ".resolution",  XrmoptionSepArg, 0 },
  { "-maxColors",   ".maxColors",   XrmoptionSepArg, 0 },
  { "-dropSpeed",   ".dropSpeed",   XrmoptionSepArg, 0 },
  { "-override",    ".override",    XrmoptionNoArg, "True" },
};

#define DEF_OVERRIDE      "False"
#define DEF_ROTATE        "True"
#define DEF_FOLLOW        "False"
#define DEF_CARPET   "True"
#define DEF_BLOB     "False"
#define DEF_NIPPLES  "True"
#define DEF_ROTATE_SPEED  "10"
#define DEF_MAX_FALLING   "75"  /* carpet is off screen by then */
#define DEF_MAX_COLORS    "7"
#define DEF_SIZE          "2"
#define DEF_SPAWN         "50"
#define DEF_RESOLUTION    "8"
#define DEF_CAM_X         "1"
#define DEF_CAM_Y         "20"
#define DEF_CAM_Z         "25"
#define DEF_DROP_SPEED    "4"

static argtype vars[] = {
  {&override,     "override",     "Override",     DEF_OVERRIDE,     t_Bool},
  {&rotate,       "rotate",       "Rotate",       DEF_ROTATE,       t_Bool},
  {&drawCarpet,   "carpet",       "Carpet",       DEF_CARPET,   t_Bool},
  {&drawNipples,  "nipples",      "Nipples",      DEF_NIPPLES,  t_Bool},
  {&drawBlob,     "blob",         "Blob",         DEF_BLOB,     t_Bool},
  {&rotateSpeed,  "rotateSpeed",  "RotateSpeed",  DEF_ROTATE_SPEED,  t_Float},
  {&follow,       "follow",       "Follow",       DEF_FOLLOW,       t_Bool},
  {&camX,         "camX",         "camX",         DEF_CAM_X,         t_Float},
  {&camY,         "camY",         "camY",         DEF_CAM_Y,         t_Float},
  {&camZ,         "camZ",         "camZ",         DEF_CAM_Z,         t_Float},
  {&size,         "size",         "size",         DEF_SIZE,         t_Int},
  {&spawn,        "spawn",        "spawn",        DEF_SPAWN,        t_Int},
  {&maxFalling,   "maxFalling",   "maxFalling",   DEF_MAX_FALLING,   t_Int},
  {&resolution,   "resolution",   "resolution",   DEF_RESOLUTION,   t_Int},
  {&maxColors,    "maxColors",    "maxColors",    DEF_MAX_COLORS,    t_Int},
  {&dropSpeed,    "dropSpeed",    "DropSpeed",    DEF_DROP_SPEED,    t_Float},
};

static topBlockSTATE *tbs = NULL;

static ModeSpecOpt topBlock_opts = {countof(opts), opts, countof(vars), vars, NULL};

/* Window management, etc */
ENTRYPOINT void
reshape_topBlock (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width*1.5;
    y = -height*0.2;
    h = height / (GLfloat) width;
  }
  glViewport (0, y, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (60.0, 1/h, 1.0, 1000.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glClear(GL_COLOR_BUFFER_BIT);
}

/* clean up on exit, not required ... */
ENTRYPOINT void
free_topBlock(ModeInfo *mi)
{
  topBlockSTATE *tb = &tbs[MI_SCREEN(mi)];
  NODE *llCurrent, *llOld;

  if (!tb->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *tb->glx_context);

  llCurrent = tb->blockNodeRoot;
  while (llCurrent != NULL) {
          llOld = llCurrent;
          llCurrent = llCurrent->next;
          free(llOld);
  }
  if (tb->trackball) gltrackball_free (tb->trackball);
  if (glIsList(tb->carpet)) glDeleteLists(tb->carpet, 1);
  if (glIsList(tb->block)) glDeleteLists(tb->block, 1);
}

/* setup */
ENTRYPOINT void 
init_topBlock (ModeInfo *mi)
{
  topBlockSTATE *tb;
  int wire = MI_IS_WIREFRAME(mi);

  MI_INIT (mi, tbs);

  tb = &tbs[MI_SCREEN(mi)];

  tb->glx_context = init_GL(mi);

  reshape_topBlock (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

/*	if (wire) { drawNipples=False; }*/
  tb->numFallingBlocks=0;

	if (size>10) { size = 10; }
	if (size<1) { size = 2; }
	tb->carpetWidth = 8 * size;
	tb->carpetLength = tb->carpetWidth;
  
  tb->maxFalling = maxFalling;
  tb->maxFalling*=size;

	if (spawn<4) { spawn=4; }
	if (spawn>1000) { spawn=1000; }

	tb->rotateSpeed = rotateSpeed;
	if (tb->rotateSpeed<1) {tb->rotateSpeed=1; }
	if (tb->rotateSpeed>1000) {tb->rotateSpeed=1000;}
  tb->rotateSpeed /= 100;

	tb->resolution = resolution;
	if (tb->resolution<4) {tb->resolution=4;}
	if (tb->resolution>20) {tb->resolution=20;}
  tb->resolution*=2;

	if (maxColors<1) {maxColors=1;}
	if (maxColors>8) {maxColors=8;}

	tb->dropSpeed = dropSpeed;
	if (tb->dropSpeed<1) {tb->dropSpeed=1;}
	if (tb->dropSpeed>9) {tb->dropSpeed=9;} /* 10+ produces blocks that can pass through each other */
  
	tb->dropSpeed = 80/tb->dropSpeed;
	tb->dropSpeed = (blockHeight/tb->dropSpeed);

  reshape_topBlock (mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  glClearDepth(1.0f);
  if (!wire) {
    GLfloat pos[4] = {10.0, 10.0, 1.0, 0.0};
    GLfloat amb[4] = {0.1, 0.1, 0.1, 1.0};
    GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
    GLfloat spc[4] = {0.0, 1.0, 1.0, 1.0};

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  amb); 
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spc); 
  }
	glDepthFunc(GL_LEQUAL);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE); /* all objects exhibit a reverse side */
  glCullFace(GL_BACK); 

	if (drawBlob) {
    buildBlobBlock(mi); 
	} else {
	  buildBlock(mi);		/* build the display list holding the simple block */
	}
  buildCarpet(mi);		/* build the base */
	tb->highest=0;
	tb->highestFalling=0;
	tb->eyeLine=tb->highest;
	tb->eyeX=0;
	tb->eyeY=0;
	tb->eyeZ=0;
	tb->followMode=0;
  if (follow) {
    tb->plusheight=100;
    camZ=camZ-60;
  } else {
    tb->rotation=random() % 360;
    tb->eyeY=10;
    tb->plusheight=30;
  }
	tb->followRadius=0;
  /* override camera settings */
  if (override) {
    tb->plusheight=100;
    drawCarpet=False;
    camX=0;
    camY=1;
    camZ=0;
    tb->eyeX=-1;
    tb->eyeY=20;
    tb->eyeZ=0;
  }
  tb->trackball = gltrackball_init (False);
}

/* provides the per frame entertainment */
ENTRYPOINT void
draw_topBlock (ModeInfo *mi)
{
  	Display *dpy = MI_DISPLAY(mi);
  	Window window = MI_WINDOW(mi);
  	NODE *llCurrent;
  	NODE *llNode;
  	topBlockSTATE *tb = &tbs[MI_SCREEN(mi)];
  	GLfloat spcN1x,spcN1y,spcN2x,spcN2y;
  	GLfloat spcC1x,spcC1y,spcC2x,spcC2y;
  	int wire = MI_IS_WIREFRAME(mi);
	  GLfloat color[4];	

  if (!tb->glx_context)
    return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *tb->glx_context);
  mi->polygon_count = 0;
  mi->recursion_depth = tb->numFallingBlocks;

	generateNewBlock(mi);

	if (rotate && (!tb->button_down_p)) { tb->rotation += tb->rotateSpeed; }
	if (tb->rotation>=360) { tb->rotation=tb->rotation-360; } 

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);		/* clear current */
	glLoadIdentity();	/* resets directions, do it every time ! */
        glRotatef(current_device_rotation(), 0, 0, 1);

	if (!follow) {
		if (tb->highest>tb->eyeLine) { tb->eyeLine+=((tb->highest-tb->eyeLine)/100);	} /* creates a smooth camera transition */
		gluLookAt(camX, camY+tb->eyeLine, camZ, tb->eyeX, tb->eyeY+tb->eyeLine, tb->eyeZ, 0.0, 1.0, 0.0);		/* setup viewer, xyz cam, xyz looking at and where is up normaly 0,1,0 */
		glRotatef(90, 1.0, 0.0, 0.0);			/* x axis */
	} else {
		glRotatef(90, 0.0, 0.0, 1.0);     /* z axis */
		followBlock(mi);
	}

        /* Rotate the scene around a point that's a little higher up. */
        glTranslatef (0, 0, -5);
        gltrackball_rotate (tb->trackball);
        glTranslatef (0, 0, 5);

	/* rotate the world */
	glRotatef(tb->rotation, 0.0, 0.0, 1.0);		

#if 0  /* This makes the blocks pop into existence already on screen */
       /* We should just make them drop from higher, but it's not obvious
          to me where that is set. */
        {
          GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                       ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                       : 1);
          glScalef (s, s, s);
        }
#endif

	llCurrent = tb->blockNodeRoot;
	if (drawCarpet) {
		/* center carpet */
		glTranslatef(0.0-(tb->carpetWidth/2),0.0-(tb->carpetLength/2),0.0);
		glCallList(tb->carpet);
                mi->polygon_count += tb->carpet_polys;
		glTranslatef(0.0+(tb->carpetWidth/2),0.0+(tb->carpetLength/2),0.0);
		glTranslatef(0.0,0.0,-0.55);
	}
	tb->highestFalling=0;
	while (llCurrent != NULL) {	/* for each block */
		glPushMatrix();	/* save state */
		/* set color */
		switch (llCurrent->color) { 
			case 0:
				color[0] = 1.0f;	
				color[1] = 0.0f;	
				color[2] = 0.0f;	
				color[3] = 1.0f;	
				break;
			case 1:
				color[0] = 0.0f;	
				color[1] = 1.0f;	
				color[2] = 0.0f;	
				color[3] = 1.0f;	
				break;
			case 2:
				color[0] = 0.0f;	
				color[1] = 0.0f;	
				color[2] = 1.0f;	
				color[3] = 1.0f;	
				break;
			case 3:
				color[0] = 0.95f;	
				color[1] = 0.95f;	
				color[2] = 0.95f;	
				color[3] = 1.0f;	
				break;
			case 4:
				color[0] = 1.0f;	
				color[1] = 0.5f;	
				color[2] = 0.0f;	
				color[3] = 1.0f;	
				break;
			case 5:
				color[0] = 1.0f;	
				color[1] = 1.0f;	
				color[2] = 0.0f;	
				color[3] = 1.0f;	
				break;
			case 6: 
				color[0] = 0.5f;	
				color[1] = 0.5f;	
				color[2] = 0.5f;	
				color[3] = 1.0f;	
				break;
			case 7:
				color[0] = 0.05f;	
				color[1] = 0.05f;	
				color[2] = 0.05f;	
				color[3] = 1.0f;	
				break;
		} 
		if (wire) { glColor3fv(color); }
		else { glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color); }

		if (llCurrent->falling==1) {
    	spcC2x = 0;
	    spcC2y = 0;
	    spcN2x = 0;
	    spcN2y = 0;
			if (llCurrent->height>tb->highestFalling) {tb->highestFalling=llCurrent->height;}
			/* all blocks fall at the same rate to avoid mid air collisions */
			llCurrent->height-=tb->dropSpeed;
			if (llCurrent->height<=0) {
				llCurrent->falling=0;
				if (tb->highest==0) { 
					tb->highest+=blockHeight; 
				}
			} 
			if ( (llCurrent->height<=tb->highest+1) && (llCurrent->falling==1) ) {
				/* check for collision */
				llNode = tb->blockNodeRoot;
				spcC1x = llCurrent->x;
				spcC1y = llCurrent->y;
				switch(llCurrent->rotation) {
					case getOrientation(0):
						spcC2x = spcC1x;
						spcC2y = spcC1y-2;
						break;
					case getOrientation(1):
						spcC2x = spcC1x+2;
						spcC2y = spcC1y;
						break;
					case getOrientation(2):
						spcC2x = spcC1x;
						spcC2y = spcC1y+2;
						break;
					case getOrientation(3):
						spcC2x = spcC1x-2;
						spcC2y = spcC1y;
						break;
				}
				while (llNode != NULL) {
					if ( (llNode->falling==0) && (llCurrent->falling==1) ) {
						spcN1x = llNode->x;
						spcN1y = llNode->y;
						switch(llNode->rotation) {
							case getOrientation(0):
								spcN2x = spcN1x;
								spcN2y = spcN1y-2;
								break;
							case getOrientation(1):
								spcN2x = spcN1x+2;
								spcN2y = spcN1y;
								break;
							case getOrientation(2):
								spcN2x = spcN1x;
								spcN2y = spcN1y+2;
								break;
							case getOrientation(3):
								spcN2x = spcN1x-2;
								spcN2y = spcN1y;
								break;
						}
						if ( 
							( (spcC1x==spcN1x) && (spcC1y==spcN1y) ) ||
							( (spcC1x==spcN2x) && (spcC1y==spcN2y) ) ||
							( (spcC2x==spcN2x) && (spcC2y==spcN2y) ) ||
							( (spcC2x==spcN1x) && (spcC2y==spcN1y) )
						){
              if ( fabs(llCurrent->height-(llNode->height+blockHeight)) <= TOLERANCE) { 

						    llCurrent->falling=0;
							  llCurrent->height=llNode->height+blockHeight; /* if this is missing then small errors build up until the model fails */
                if ( fabs(llCurrent->height-tb->highest) <= TOLERANCE+blockHeight ) {
                 tb->highest+=blockHeight; 
							  }
						  }
					  }
				  }
				  llNode=llNode->next;
			  }				
		  }
	  } 
  	/* set location in space */
	  glTranslatef(llCurrent->x,llCurrent->y,-llCurrent->height);
	  /* rotate */
  	glRotatef(llCurrent->rotation, 0.0f, 0.0f, 1.0f);
  	if ((tb->followMode==0) && (llCurrent->next==NULL)) {
  		tb->blockNodeFollow = llCurrent;
  		tb->followMode=1;
  	} 
  	llCurrent = llCurrent->next;
  	/* draw   */
  	glCallList(tb->block); 
        mi->polygon_count += tb->block_polys;
  	glPopMatrix();	/* restore state */
  } 
  if (mi->fps_p) do_fps (mi);
  glFinish();

	if (tb->highest>(5*tb->maxFalling)) { drawCarpet=False; }
  glXSwapBuffers(dpy, window);
}



/* camera is in follow mode, work out where we should be looking */
static void followBlock(ModeInfo *mi)
{
	GLfloat xLen,yLen,cx,cy,rangle,xTarget,yTarget;
  topBlockSTATE *tb = &tbs[MI_SCREEN(mi)];
  cx=0;cy=0;
	if ((tb->blockNodeFollow!=NULL) && (tb->followMode==1)){

		if (tb->highest>tb->eyeLine) { tb->eyeLine+= ((tb->highest-tb->eyeLine)/100);	} 
		  /*tb->blockNodeFollow->color=1;  only noticable if you set the colors to 1 */
		
			if (tb->blockNodeFollow->height > tb->eyeZ) { tb->eyeZ+= ((tb->blockNodeFollow->height - tb->eyeZ)/100); } 
			if (tb->blockNodeFollow->height < tb->eyeZ) { tb->eyeZ-= ((tb->eyeZ - tb->blockNodeFollow->height)/100); } 
		

		/* when the scene is rotated we need to know where the block is in the 2 dimensional coordinates of the carpet area
		   (see http://www.jumpstation.co.uk/rotation/)
		*/

		if (tb->followRadius==0) {		
			xLen = tb->blockNodeFollow->x-cx;
			yLen = tb->blockNodeFollow->y-cy;
			tb->followRadius=sqrt( (xLen*xLen) + (yLen*yLen) );	
			tb->followAngle = (180/M_PI) * asin(xLen/tb->followRadius); 
			tb->followAngle = quadrantCorrection(tb->followAngle,(int)cx,(int)cy,(int)tb->blockNodeFollow->x,(int)tb->blockNodeFollow->y);
		}
		rangle = (tb->followAngle+tb->rotation) * M_PI /180;
		xTarget = cos(rangle) * tb->followRadius + cx;
		yTarget = sin(rangle) * tb->followRadius + cy;
		if (tb->followAngle>360) { tb->followAngle=tb->followAngle-360; }

		if (xTarget < tb->eyeX) { tb->eyeX-= ((tb->eyeX - xTarget)/100); }
		if (xTarget > tb->eyeX) { tb->eyeX+= ((xTarget - tb->eyeX)/100); }

		if (yTarget < tb->eyeY) { tb->eyeY-= ((tb->eyeY - yTarget)/100); }
		if (yTarget > tb->eyeY) { tb->eyeY+= ((yTarget - tb->eyeY)/100); }
		if (!tb->blockNodeFollow->falling) {  
			tb->followMode=0; 
			/*tb->blockNodeFollow->color=2;  only noticable if you set the colors to 1 */
			tb->followRadius=0;
		} 
	}
	gluLookAt(camX, camY, camZ-tb->eyeLine, tb->eyeX, tb->eyeY, -tb->eyeZ,-1.0,0.0,0.0);
}

/* each quater of the circle has to be adjusted for */
static double quadrantCorrection(double angle,int cx,int cy,int x,int y)
{
	if ((x>=cx) && (y>=cy)) {
		angle +=  (90-(angle-90) * 2); 
	} else if ((x>=cx) && (y<=cy)) {
		angle +=  90; 
	} else if ((x<=cx) && (y<=cy)) {
		angle +=  90; 
	} else if ((x<=cx) && (y>=cy)) {
		angle += (90-(angle-90) * 2); 
	}
	return(angle-180);
}

/* if random chance then create a new falling block */
static void generateNewBlock(ModeInfo *mi)
{
	NODE *llCurrent, *llTail;
	GLfloat startOffx, startOffy;
	int endOffx, endOffy;
  topBlockSTATE *tb = &tbs[MI_SCREEN(mi)];
	if ( ((random() % spawn) == 1) && (tb->highestFalling<getHeight((tb->plusheight-blockHeight)+tb->highest)) ) {	
		startOffx=0;
		endOffx=0;
		startOffy=0;
		endOffy=0;
		tb->numFallingBlocks++;
		llTail = tb->blockNodeRoot; 
		if (llTail == NULL) {
                  llCurrent = ((NODE*) malloc(sizeof(NODE)));
                  if (!llCurrent) abort();
                  llTail = llCurrent;
                  tb->blockNodeRoot = llCurrent; 
		} else {
			if (tb->numFallingBlocks>=tb->maxFalling) {
				/* recycle */
				llCurrent=llTail->next;
				tb->blockNodeRoot=llCurrent->next;
			} else {
                          llCurrent = ((NODE*) malloc(sizeof(NODE)));
                          if (!llCurrent) abort();
			}
			while (llTail->next != NULL) { llTail = llTail->next; } /* find last item in list */
		}
		llCurrent->falling=1;
		llCurrent->rotation=getOrientation(random() % 4); 
		if (llCurrent->rotation==getOrientation(0)) {
			startOffx=1.0;
			endOffx=0;
			startOffy=3.0;
			endOffy=-1;
		} else if (llCurrent->rotation==getOrientation(1)) {
			startOffx=1.0;
			endOffx=-1;
			startOffy=1.0;
			endOffy=0;		
		} else if (llCurrent->rotation==getOrientation(2)) {
			startOffx=1.0;
			endOffx=0;
			startOffy=3.0;
			endOffy=-1;		
		} else if (llCurrent->rotation==getOrientation(3)) { 
			startOffx=5.0;
			endOffx=-1;
			startOffy=1.0;
			endOffy=0;		
		}

		llCurrent->x=(startOffx-(tb->carpetLength/2)) + getLocation(random() % ((tb->carpetLength/2)+endOffx) );
		llCurrent->y=(startOffy-(tb->carpetLength/2)) + getLocation(random() % ((tb->carpetLength/2)+endOffy) );
		llCurrent->color=(random() % maxColors);
		llCurrent->height=getHeight(tb->plusheight+tb->highest); 
		if (tb->numFallingBlocks>=tb->maxFalling) {
			tb->numFallingBlocks--;
			tb->numFallingBlocks--;
		} 
		llTail->next = llCurrent;
		llTail = llCurrent;
		llTail->next = NULL;

	}
}

/* called at init this creates the 'carpet' display list item */
static void buildCarpet(ModeInfo *mi)
{
	int i,c,x,y;
	GLfloat color[4];
  	int wire = MI_IS_WIREFRAME(mi);
  	topBlockSTATE *tb = &tbs[MI_SCREEN(mi)];
		color[0] = 0.0f;	
		color[1] = 1.0f;	
		color[2] = 0.0f;	
		color[3] = 1.0f;	
	tb->carpet=glGenLists(1);	/* only one */
	glNewList(tb->carpet,GL_COMPILE);
        tb->carpet_polys=0;
	glPushMatrix();	/* save state */
  	x=tb->carpetWidth;
  	y=tb->carpetLength;
	if (wire) { glColor3fv(color); }
	else { glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color); }
	/* draw carpet plane */
	glBegin( wire ? GL_LINE_LOOP : GL_QUADS );
		/* draw top */
		glNormal3f( 0, 0, -1 );
		glVertex3f(0.0,0.0,0.0);
		glVertex3f(x,0.0,0.0);
		glVertex3f(x,y,0.0);
		glVertex3f(0.0,y,0.0);
                tb->carpet_polys++;
	    if (!wire) {
		/* add edge pieces */
		/* side 1 */
		glNormal3f( 0, -1, 0 );
		glVertex3f(0.0,0.0,0.0);
		glVertex3f(x,0.0,0.0);
		glVertex3f(x,0,singleThick);
		glVertex3f(0.0,0,singleThick);
                tb->carpet_polys++;
		/* side 2 */
		glNormal3f( -1, 0, 0 );
		glVertex3f(0.0,0.0,0.0);
		glVertex3f(0,y,0.0);
		glVertex3f(0,y,singleThick);
		glVertex3f(0.0,0,singleThick);
                tb->carpet_polys++;
		/* side 3 */
		glNormal3f( 1, 0, 0 );
		glVertex3f(x,0.0,0.0);
		glVertex3f(x,y,0.0);
		glVertex3f(x,y,singleThick);
		glVertex3f(x,0,singleThick);
                tb->carpet_polys++;
		/* side 4 */
		glNormal3f( 0, 1, 0 );
		glVertex3f(0,y,0.0);
		glVertex3f(x,y,0.0);
		glVertex3f(x,y,singleThick);
		glVertex3f(0,y,singleThick);
                tb->carpet_polys++;
		}
	glEnd();
	/* nipples */
	if (drawNipples) {
		glTranslatef(0.5f,0.5f,-.25);			/* move to the cylinder center */
		for (c=0;c<x;c++) {
			glPushMatrix();	/* save state */
			for (i=0;i<y;i++) {
                          tb->carpet_polys += tube(0, 0, -0.1,
                                                   0, 0, 0.26,
                                                   cylSize, 0,
                                                   tb->resolution, True, True,
                                                   wire);
                          glRotatef(180, 0.0f, 1.0f, 0.0f); /* they are upside down */
                          glRotatef(180, 0.0f, 1.0f, 0.0f); /* recover */
                          glTranslatef(0.0f,1.0f,0.0f);			/* move to the next cylinder center (backward) */
			}
			glPopMatrix();	/* save state */
			glTranslatef(1.0f,0.0f,0.0f);			/* reset   */
		}
	}
	glPopMatrix();	/* restore state */
	glEndList();	
}

/* using the verticies arrays builds the plane, now with normals */
static void polygonPlane(int wire, int a, int b, int c , int d, int i)
{
	GLfloat topBlockNormals[5][3] = { {0,0,-1},	{0,1,0}, {1,0,0}, {0,0,1}, {0,-1,0} };
	GLfloat topBlockVertices[8][3] = { {-0.49,-2.97,-0.99}, {0.99,-2.97,-0.99}, {0.99,0.99,-0.99}  , {-0.49,0.99,-0.99}, {-0.49,-2.97,0.99} , {0.99,-2.97,0.99}, {0.99,0.99,0.99}   , {-0.49,0.99,0.99} };
	glBegin( wire ? GL_LINE_LOOP : GL_POLYGON);
		glNormal3fv(topBlockNormals[i] );
		glVertex3fv(topBlockVertices[a]);
		glVertex3fv(topBlockVertices[b]);
		glVertex3fv(topBlockVertices[c]);
		glVertex3fv(topBlockVertices[d]);
	glEnd();
}

/* called at init this creates the 'block' display list item */
/* the spheres came about originaly as quick way to test the directional lighting/normals */
static void buildBlock(ModeInfo *mi)
{
	int i,c;
  int wire = MI_IS_WIREFRAME(mi);
  topBlockSTATE *tb = &tbs[MI_SCREEN(mi)];
	tb->block=glGenLists(1);	/* only one */
	glNewList(tb->block,GL_COMPILE);
        tb->block_polys=0;
	glPushMatrix();	/* save state */
	glRotatef(90, 0.0f, 1.0f, 0.0f);
	/* base */
	polygonPlane(wire, 0,3,2,1,0); tb->block_polys++;
	polygonPlane(wire, 2,3,7,6,1); tb->block_polys++;
	polygonPlane(wire, 1,2,6,5,2); tb->block_polys++;
	polygonPlane(wire, 4,5,6,7,3); tb->block_polys++;
	polygonPlane(wire, 0,1,5,4,4); tb->block_polys++;
	if (drawNipples) {
		/* nipples */
		/* draw 8 cylinders each with a disk cap */
		glRotatef(90, 0.0f, 1.0f, 0.0f);		/* 'aim' the pointer ready for the cylinder */
		glTranslatef(0.5f,0.5f,0.99f);			/* move to the cylinder center */
		for (c=0;c<2;c++) {
			for (i=0;i<4;i++) {
                          tb->block_polys += tube(0, 0, 0,
                                                  0, 0, 0.25,
                                                  cylSize, 0,
                                                  tb->resolution, True, True,
                                                  wire);
				glTranslatef(0.0f,0.0f,0.25f);			/* move to the cylinder cap  */
				glTranslatef(0.0f,0.0f,-0.25f);			/* move back from the cylinder cap  */
				if (c==0) {	
					glTranslatef(0.0f,-1.0f,0.0f);			/* move to the next cylinder center (forward) */
				} else {
					glTranslatef(0.0f,1.0f,0.0f);			/* move to the next cylinder center (backward) */
				}
			}
			glTranslatef(-1.0f,1.0f,0.0f);			/* move to the cylinder center */
		}
		/* udders */
		/* 3 cylinders on the underside */
		glTranslatef(1.5f,-2.5f,-1.5f);		/* move to the center, under the top of the brick	 */
                if (! wire)
		for (c=0;c<3;c++) {
                  tb->block_polys += tube(0, 0, 0.1,
                                          0, 0, 1.4,
                                          uddSize, 0,
                                          tb->resolution, True, True, wire);
                  glTranslatef(0.0f,-1.0f,0.0f);		/* move to the center */	
		}
	}
	glPopMatrix();	/* restore state */
	glEndList();	
}

/* 
	rip off of the builBlock() function creating the GL compilied pointer "block" but only creates two spheres.
	spheres are created with unit_sphere from spheres.h to allow wire frame 
*/
static void buildBlobBlock(ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  topBlockSTATE *tb = &tbs[MI_SCREEN(mi)];
	tb->block=glGenLists(1);	/* only one */
	glNewList(tb->block,GL_COMPILE);
	glPushMatrix();
  glScalef(1.4,1.4,1.4);
  unit_sphere (tb->resolution/2,tb->resolution, wire);
  glPopMatrix();
  glTranslatef(0.0f,-2.0f,0.0f);
  glScalef(1.4,1.4,1.4);
  unit_sphere (tb->resolution/2,tb->resolution, wire);
	glEndList();	
}


/* handle input events or not if daemon running the show */
ENTRYPOINT Bool 
topBlock_handle_event (ModeInfo *mi, XEvent *event)
{
  topBlockSTATE *tb = &tbs[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, tb->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &tb->button_down_p))
    return True;
  else if (event->xany.type == KeyPress) {
    KeySym keysym;
    char c = 0;
    XLookupString (&event->xkey, &c, 1, &keysym, 0);
    if (c == 'a') {
			tb->eyeX++;
			return True;
		} else if (c == 'z') {
			tb->eyeX--;
			return True;
		} else if (c == 's') {
			tb->eyeY--;
			return True;
		} else if (c == 'x') {
			tb->eyeY++;
			return True;
		} else if (c == 'd') {
			tb->eyeZ++;
			return True;
		} else if (c == 'c') {
			tb->eyeZ--;
			return True;
		} else if (c == 'f') {
			camX++;
			return True;
		} else if (c == 'v') {
			camX--;
			return True;
		} else if (c == 'g') {
			camY++;
			return True;
		} else if (c == 'b') {
			camY--;
			return True;
		} else if (c == 'h') {
			camZ++;
			return True;
		} else if (c == 'n') {
			camZ--;
			return True;
		} else if (c == 'r') {
			tb->rotation++;
			return True;
		}
	}

  return False;
}

/* this is tha main change for v5 compatability and acompanying ENTRYPOINTS */
XSCREENSAVER_MODULE_2 ("TopBlock", topblock, topBlock)

#endif /* USE_GL */
