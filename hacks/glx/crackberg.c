/***************************
 ** crackberg; Matus Telgarsky [ catachresis@cmu.edu ] 2005 
 ** */
#include <X11/Intrinsic.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>

extern XtAppContext app;

#define PROGCLASS   "crackberg"
#define HACK_INIT   crackberg_init
#define HACK_DRAW   crackberg_draw
#define HACK_RESHAPE    crackberg_reshape
/* #define HACK_FREE   crackberg_free */
#define HACK_HANDLE_EVENT crackberg_handle_ev
#define EVENT_MASK      (KeyReleaseMask)
#define sws_opts    xlockmore_opts

#define DEFAULTS    "*delay:        20000       \n" \
                    "*showFPS:      False       \n" \

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#ifdef USE_GL /* whole file */
#include <GL/glu.h>



/***************************
 ** macros
 ** */

#define M_RAD7_4        0.661437827766148
#define M_SQRT3_2       0.866025403784439
#define M_PI_180        0.0174532925199433
#define M_180_PI        57.2957795130823
#define MSPEED_SCALE    1.1
#define AVE3(a,b,c)     ( ((a) + (b) + (c)) / 3.0 )
#define MAX_ZDELTA      0.35
#define DISPLACE(h,d)   (h+(random()/(double)RAND_MAX-0.5)*2*MAX_ZDELTA/(1<<d))
#define MEAN(x,y)       ( ((x) + (y)) / 2.0 )
#define TCOORD(x,y)     (heights[(epoints * (y) - ((y)-1)*(y)/2 + (x))])
#define sNCOORD(x,y,p)  (norms[3 * (epoints * (y) - ((y)-1)*(y)/2 + (x)) + (p)])
#define SET_sNCOORD(x,y, down, a,b,c,d,e,f)   \
    sNCOORD(x,y,0) = AVE3(a-d, 0.5 * (b-e), -0.5 * (c-f)); \
    sNCOORD(x,y,1) = ((down) ? -1 : +1) * AVE3(0.0, M_SQRT3_2 * (b-e), M_SQRT3_2 * (c-f)); \
    sNCOORD(x,y,2) = (2*dx)
#define fNCOORD(x,y,w,p)  \
    (norms[3 * (2*(y)*epoints-((y)+1)*((y)+1) + 1 + 2 * ((x)-1) + (w)) + (p)])
#define SET_fNCOORDa(x,y, down, dz00,dz01) \
    fNCOORD(x,y,0,0) = (down) * (dy) * (dz01); \
    fNCOORD(x,y,0,1) = (down) * ((dz01) * (dx) / 2 - (dx) * (dz00)); \
    fNCOORD(x,y,0,2) = (down) * (dx) * (dy)
#define SET_fNCOORDb(x,y, down, dz10,dz11) \
    fNCOORD(x,y,1,0) = (down) * (dy) * (dz10); \
    fNCOORD(x,y,1,1) = (down) * ((dz11) * (dx) - (dx) * (dz10) / 2); \
    fNCOORD(x,y,1,2) = (down) * (dx) * (dy)


/***************************
 ** types
 ** */


typedef struct _cberg_state cberg_state;
typedef struct _Trile Trile;

typedef struct {
    void (*init)(Trile *);
    void (*free)(Trile *);
    void (*draw)(Trile *);
    void (*init_iter)(Trile *, cberg_state *);
    void (*dying_iter)(Trile *, cberg_state *);
} Morph;

typedef struct {
    char *id;
    void (*land)(double);
    void (*water)(double);
    double bg[4];
} Color;

enum { TRILE_NEW, TRILE_INIT, TRILE_STABLE, TRILE_DYING, TRILE_DELETE };

struct _Trile {
    int x,y; /*center coords; points up if (x+y)%2 == 0, else down*/
    short state;
    short visible;
    double *l,*r,*v; /*only edges need saving*/
    GLuint call_list;

    void *morph_data;
    Morph *morph;

    struct _Trile *left, *right, *parent; /* for bst, NOT spatial */
    struct _Trile *next_free; /* for memory allocation */
};

enum { MOTION_AUTO = 0, MOTION_MANUAL = 1, MOTION_LROT= 2,    MOTION_RROT = 4,
       MOTION_FORW = 8,   MOTION_BACK = 16,  MOTION_DEC = 32, MOTION_INC = 64,
       MOTION_LEFT = 128, MOTION_RIGHT = 256 };

struct _cberg_state {
    GLXContext *glx_context;
    Trile *trile_head;
    
    double x,y,z, yaw,roll,pitch, dx,dy,dz, dyaw,droll,dpitch, elapsed;
    double prev_frame;
    int motion_state;
    double mspeed;

    double fovy, aspect, zNear, zFar;

    Color *color;
};


/***************************
 ** globals
 ** */


static unsigned int nsubdivs,
                    epoints, /*number of points to one edge*/
                    tpoints, /*number points total*/
                    ntris,   /*number triangles per trile*/
                    tnorms;  /*number of normals*/
Bool crack, boring, nowater, flat, lit, wire, letterbox;
static char *color;
static cberg_state *cbergs = NULL;
static double *heights = NULL, *norms = NULL, dx;
static float visibility;
static Trile *free_head = NULL; /* for trile_[alloc|free] */

static XrmOptionDescRec opts[] = {
    {"-nsubdivs", ".nsubdivs", XrmoptionSepArg, 0},
    {"-boring", ".boring", XrmoptionNoArg, "True"},
    {"-crack", ".crack", XrmoptionNoArg, "True"},
    {"-nowater", ".nowater", XrmoptionNoArg, "True"},
    {"-flat", ".flat", XrmoptionNoArg, "True"},
    {"-color", ".color", XrmoptionSepArg, 0},
    {"-lit", ".lit", XrmoptionNoArg, "True"},
    {"-wire", ".wire", XrmoptionNoArg, "True"},
    {"-visibility", ".visibility", XrmoptionSepArg, 0},
    {"-letterbox", ".letterbox", XrmoptionNoArg, "True"}
    
};

static argtype vars[] = {
    {&nsubdivs, "nsubdivs", "nsubdivs", "4", t_Int},
    {&boring, "boring", "boring", "False", t_Bool},
    {&crack, "crack", "crack", "False", t_Bool},
    {&nowater, "nowater", "nowater", "False", t_Bool},
    {&flat, "flat", "flat", "False", t_Bool},
    {&color, "color", "color", "plain", t_String},
    {&lit, "lit", "lit", "False", t_Bool},
    {&wire, "wire", "wire", "False", t_Bool},
    {&visibility, "visibility", "visibility", "0.6", t_Float},
    {&letterbox, "letterbox", "letterbox", "False", t_Bool}
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};


/***************************
 ** Trile functions. 
 **  first come all are regular trile functions 
 ** */


/* forward decls for trile_new */
static Trile *triles_find(Trile *tr, int x, int y);
static Trile *trile_alloc(void);
static Morph *select_morph(void);
static Color *select_color(void);

static void trile_calc_sides(Trile *new, int x, int y, Trile *root)
{
    unsigned int i,j,k; 
    int dv = ( (x + y) % 2 ? +1 : -1); /* we are pointing down or up*/
    Trile *l, *r, *v; /* v_ertical */


    if (root) {
        l = triles_find(root, x-1, y);
        r = triles_find(root, x+1, y);  
        v = triles_find(root, x,y+dv); 
    } else
        l = r = v = NULL;

    if (v) {
        for (i = 0; i != epoints; ++i)
            new->v[i] = v->v[i];
    } else {
        if (l)          new->v[0] = l->l[0];
        else if (!root) new->v[0] = DISPLACE(0,0);
        else { 
            Trile *tr; /* all of these tests needed.. */
            if ( (tr = triles_find(root, x-1, y + dv)) )
                new->v[0] = tr->l[0];
            else if ( (tr = triles_find(root, x-2, y)) )
                new->v[0] = tr->r[0];
            else if ( (tr = triles_find(root, x-2, y + dv)) )
                new->v[0] = tr->r[0];
            else
                new->v[0] = DISPLACE(0,0);
        }

        if (r)          new->v[epoints-1] = r->l[0];
        else if (!root) new->v[epoints-1] = DISPLACE(0,0);
        else {
            Trile *tr;
            if ( (tr = triles_find(root, x+1, y + dv)) )
                new->v[epoints-1] = tr->l[0];
            else if ( (tr = triles_find(root, x+2, y)) )
                new->v[epoints-1] = tr->v[0];
            else if ( (tr = triles_find(root, x+2, y + dv)) )
                new->v[epoints-1] = tr->v[0];
            else
                new->v[epoints-1] = DISPLACE(0,0);
        }

        for (i = ((1 << nsubdivs) >> 1), k =1; i; i >>= 1, ++k)
            for (j = i; j < epoints; j += i * 2)
                new->v[j] = DISPLACE(MEAN(new->v[j-i], new->v[j+i]), k);
    }
        
    if (l) {
        for (i = 0; i != epoints; ++i)
            new->l[i] = l->r[i];
    } else {
        if (r)          new->l[0] = r->v[0];
        else if (!root) new->l[0] = DISPLACE(0,0);
        else {
            Trile *tr;
            if ( (tr = triles_find(root, x-1, y-dv)) )
                new->l[0] = tr->r[0];
            else if ( (tr = triles_find(root, x+1, y-dv)) )
                new->l[0] = tr->v[0];
            else if ( (tr = triles_find(root, x, y-dv)) )
                new->l[0] = tr->l[0];
            else 
                new->l[0] = DISPLACE(0,0);
        }

        new->l[epoints - 1] = new->v[0];

        for (i = ((1 << nsubdivs) >> 1), k =1; i; i >>= 1, ++k)
            for (j = i; j < epoints; j += i * 2)
                new->l[j] = DISPLACE(MEAN(new->l[j-i], new->l[j+i]), k);
    }

    if (r) {
        for (i = 0; i != epoints; ++i)
            new->r[i] = r->l[i];
    } else {
        new->r[0] = new->v[epoints - 1];
        new->r[epoints - 1] = new->l[0];

        for (i = ((1 << nsubdivs) >> 1), k =1; i; i >>= 1, ++k)
            for (j = i; j < epoints; j += i * 2)
                new->r[j] = DISPLACE(MEAN(new->r[j-i], new->r[j+i]), k);
    }
}

static void trile_calc_heights(Trile *new)
{
    unsigned int i, j, k, h;

    for (i = 0; i < epoints - 1; ++i) { /* copy in sides */
        TCOORD(i,0) = new->v[i];
        TCOORD(epoints - 1 - i, i) = new->r[i];
        TCOORD(0, epoints - 1 - i) = new->l[i];
    }

    for (i = ((1 << nsubdivs) >> 2), k =1; i; i >>= 1, ++k)
        for (j = 1; j < (1 << k); ++j)
            for (h = 1; h <= (1<<k) - j; ++h) {
                TCOORD( i*(2*h - 1), i*(2*j - 1) ) = /*rights*/
                  DISPLACE(MEAN(TCOORD( i*(2*h - 2), i*(2*j + 0) ),
                                TCOORD( i*(2*h + 0), i*(2*j - 2) )), k);

                TCOORD( i*(2*h + 0), i*(2*j - 1) ) = /*lefts*/
                  DISPLACE(MEAN(TCOORD( i*(2*h + 0), i*(2*j - 2) ),
                                TCOORD( i*(2*h + 0), i*(2*j + 0) )), k);

                TCOORD( i*(2*h - 1), i*(2*j + 0) ) = /*verts*/
                  DISPLACE(MEAN(TCOORD( i*(2*h - 2), i*(2*j + 0) ),
                                TCOORD( i*(2*h + 0), i*(2*j + 0) )), k);
            }
}

static void trile_calc_flat_norms(Trile *new)
{
    unsigned int x, y;
    int down = (((new->x + new->y) % 2) ? -1 : +1);
    double dz00,dz01,dz10,dz11, a,b,c,d;
    double dy = down * M_SQRT3_2 / (1 << nsubdivs);

    for (y = 0; y < epoints - 1; ++y) {
        a = TCOORD(0,y);
        b = TCOORD(0,y+1);
        for (x = 1; x < epoints - 1 - y; ++x) {
            c = TCOORD(x,y);
            d = TCOORD(x,y+1);

            dz00 = b-c;
            dz01 = a-c;
            dz10 = b-d;
            dz11 = c-d;
            
            SET_fNCOORDa(x,y, down, dz00,dz01);
            SET_fNCOORDb(x,y, down, dz10,dz11);

            a = c;
            b = d;
        }

        c = TCOORD(x,y);
        dz00 = b-c;
        dz01 = a-c;
        SET_fNCOORDa(x,y, down, dz00, dz01);
    }
}

static void trile_calc_smooth_norms(Trile *new)
{
    unsigned int i,j, down = (new->x + new->y) % 2;
    double prev, cur, next;

    /** corners -- assume level (bah) **/
    cur = TCOORD(0,0);
    SET_sNCOORD(0,0, down,
        cur,cur,TCOORD(0,1),TCOORD(1,0),cur,cur);
    cur = TCOORD(epoints-1,0);
    SET_sNCOORD(epoints-1,0, down,
        TCOORD(epoints-2,0),TCOORD(epoints-2,1),cur,cur,cur,cur);
    cur = TCOORD(0,epoints-1);
    SET_sNCOORD(0,epoints-1, down,
        cur,cur,cur,cur,TCOORD(1,epoints-2),TCOORD(0,epoints-2));


    /** sides **/
    /* vert */
    prev = TCOORD(0,0);
    cur = TCOORD(1,0);
    for (i = 1; i < epoints - 1; ++i) {
        next = TCOORD(i+1,0);
        SET_sNCOORD(i,0, down, prev,TCOORD(i-1,1),TCOORD(i,1), next,cur,cur);
        prev = cur;
        cur = next;
    }

    /* right */
    prev = TCOORD(epoints-1,0);
    cur = TCOORD(epoints-2,0);
    for (i = 1; i < epoints - 1; ++i) {
        next = TCOORD(epoints-i-2,i+1);
        SET_sNCOORD(epoints-i-1,i, down, TCOORD(epoints-i-2,i),next,cur,
                                        cur,prev,TCOORD(epoints-i-1,i-1));
        prev = cur;
        cur = next;
    }
        
    /* left */
    prev = TCOORD(0,0);
    cur = TCOORD(0,1);
    for (i = 1; i < epoints - 1; ++i) {
        next = TCOORD(0,i+1);
        SET_sNCOORD(0,i, down, cur,cur,next,TCOORD(1,i),TCOORD(1,i-1),prev);
        prev = cur;
        cur = next;
    }


    /** fill in **/
    for (i = 1; i < epoints - 2; ++i) {
        prev = TCOORD(0,i);
        cur = TCOORD(1,i);
        for (j = 1; j < epoints - i - 1; ++j) {
            next = TCOORD(j+1,i);
            SET_sNCOORD(j,i, down, prev,TCOORD(j-1,i+1),TCOORD(j,i+1),
                            next,TCOORD(j+1,i-1),TCOORD(j,i-1));
            prev = cur;
            cur = next;
        }
    }
}

static inline void trile_light(unsigned int x, unsigned int y, 
                               unsigned int which)
{
    if (flat) {
        if (x) {
            glNormal3d(fNCOORD(x,y,which,0),
                       fNCOORD(x,y,which,1),
                       fNCOORD(x,y,which,2));
        } else { /* I get mesa errors and bizarre glitches without this!! */
            glNormal3d(fNCOORD(1,y,0,0),
                       fNCOORD(1,y,0,1),
                       fNCOORD(1,y,0,2));
        }
    } else {
        glNormal3d(sNCOORD(x,y+which,0),
                   sNCOORD(x,y+which,1),
                   sNCOORD(x,y+which,2));
    }
}

static inline void trile_draw_vertex(cberg_state *cberg, unsigned int ix,
    unsigned int iy, unsigned int which, double x,double y,
    double zcur, double z1, double z2)
{
    glColor3d(0.0, 0.0, 0.0); /* don't ask. my card breaks otherwise. */
    
    if (!nowater && zcur <= 0.0) {
        cberg->color->water(zcur); /* XXX use average-of-3 for color when flat?*/
        if (lit) glNormal3d(0.0,0.0,1.0);
        glVertex3d(x, y, 0.0); 
    } else {
        cberg->color->land(zcur);
        if (lit) trile_light(ix,iy,which);
        glVertex3d(x, y, zcur);
    }
}

static void trile_render(cberg_state *cberg, Trile *new)
{
    double cornerx = 0.5 * new->x - 0.5, cornery;
    double dy = M_SQRT3_2 / (1 << nsubdivs);
    double z0,z1,z2;
    int x,y;

    new->call_list = glGenLists(1);
    glNewList(new->call_list, GL_COMPILE);

        if ((new->x + new->y) % 2) { /*point down*/
            cornery = (new->y + 0.5)*M_SQRT3_2;
            glFrontFace(GL_CW);
            dy = -dy;
        } else
            cornery = (new->y - 0.5) * M_SQRT3_2;

        for (y = 0; y < epoints - 1; ++y) {
            glBegin(GL_TRIANGLE_STRIP);
                /* first three points all part of the same triangle.. */
                z0 = TCOORD(0,y);
                z1 = TCOORD(0,y+1);
                z2 = TCOORD(1,y);
                trile_draw_vertex(cberg, 0,y,0,
                  cornerx,cornery, z0, z1, z2);
                trile_draw_vertex(cberg, 0,y,1,
                  cornerx+0.5*dx,cornery+dy, z1, z0, z2);

                for (x = 1; x < epoints - 1 - y; ++x) {
                    trile_draw_vertex(cberg, x,y,0,
                      cornerx+x*dx,cornery, z2, z1, z0);

                    z0 = TCOORD(x, y+1);

                    trile_draw_vertex(cberg, x,y,1,
                      cornerx+(x+0.5)*dx,cornery+dy, z0, z2, z1);

                    z1 = z0;
                    z0 = z2;
                    z2 = TCOORD(x+1,y);
                }
                trile_draw_vertex(cberg, x,y,0,
                  cornerx + x*dx, cornery, z2, z1, z0);
            glEnd();

            cornerx += dx/2;
            cornery += dy;
        }

        if ((new->x + new->y) % 2) /*point down*/
            glFrontFace(GL_CCW);
    glEndList();
}

static Trile *trile_new(cberg_state *cberg, int x,int y,Trile *parent,Trile *root)
{
    Trile *new;

    new = trile_alloc();

    new->x = x;
    new->y = y;
    new->state = TRILE_NEW;
    new->parent = parent;
    new->left = new->right = NULL;
    new->visible = 1;

    new->morph = select_morph();
    new->morph->init(new);

    trile_calc_sides(new, x, y, root);
    trile_calc_heights(new);

    if (lit) {
        if (flat)   trile_calc_flat_norms(new);
        else        trile_calc_smooth_norms(new);
    }

    trile_render(cberg, new);
    return new;
}

static Trile *trile_alloc(void)
{
    Trile *new;
    static unsigned int count = 0;

    if (free_head) {
        new = free_head;
        free_head = free_head->next_free;
    } else {
        ++count;
        if (!(new = malloc(sizeof(Trile)))
         || !(new->l = (double *) malloc(sizeof(double) * epoints * 3))) {
            perror(progname);
            exit(1);
        }
        new->r = new->l + epoints;
        new->v = new->r + epoints;
#ifdef DEBUG
        printf("needed to alloc; [%d]\n", count);
#endif
    }
    return new;
}

static void trile_free(Trile *tr)
{
    glDeleteLists(tr->call_list, 1);
    tr->morph->free(tr);
    tr->next_free = free_head;
    free_head = tr;
}


static void trile_draw_vanilla(Trile *tr)
{ glCallList(tr->call_list); }

static void trile_draw(Trile *tr, void *ignore)
{
    if (tr->state == TRILE_STABLE)
        trile_draw_vanilla(tr);
    else
        tr->morph->draw(tr);
}


/***************************
 ** Trile morph functions. 
 **  select function at bottom (forward decls sucls) 
 ** */


/*** first the basic growing morph */

static void grow_init(Trile *tr)
{
    tr->morph_data = (void *) malloc(sizeof(double));
    *((double *)tr->morph_data) = 0.02; /* not 0; avoid normals crapping */
}

static void grow_free(Trile *tr)
{
    free(tr->morph_data);
}

static void grow_draw(Trile *tr)
{
    glPushMatrix();
        glScaled(1.0,1.0, *((double *)tr->morph_data));
        trile_draw_vanilla(tr);
    glPopMatrix();
}

static void grow_init_iter(Trile *tr, cberg_state *cberg)
{
    *((double *)(tr->morph_data)) = *((double *)tr->morph_data) + cberg->elapsed;
    if (*((double *)tr->morph_data) >= 1.0)
        tr->state = TRILE_STABLE;
}

static void grow_dying_iter(Trile *tr, cberg_state *cberg)
{
    *((double *)tr->morph_data) = *((double *)tr->morph_data) - cberg->elapsed;
    if (*((double *)tr->morph_data) <= 0.02) /* XXX avoid fast del/cons? */
        tr->state = TRILE_DELETE;
}

/**** falling morph ****/

static void fall_init(Trile *tr)
{
    tr->morph_data = (void *) malloc(sizeof(double));
    *((double *)tr->morph_data) = 0.0;
}

static void fall_free(Trile *tr)
{
    free(tr->morph_data);
}

static void fall_draw(Trile *tr)
{
    glPushMatrix();
        glTranslated(0.0,0.0,(0.5 - *((double *)tr->morph_data)) * 8);
        trile_draw_vanilla(tr);
    glPopMatrix();
}

static void fall_init_iter(Trile *tr, cberg_state *cberg)
{
    *((double *)(tr->morph_data)) = *((double *)tr->morph_data) + cberg->elapsed;
    if (*((double *)tr->morph_data) >= 0.5)
        tr->state = TRILE_STABLE;
}

static void fall_dying_iter(Trile *tr, cberg_state *cberg)
{
    *((double *)tr->morph_data) = *((double *)tr->morph_data) - cberg->elapsed;
    if (*((double *)tr->morph_data) <= 0.0) /* XXX avoid fast del/cons? */
        tr->state = TRILE_DELETE;
}

/**** yeast morph ****/

static void yeast_init(Trile *tr)
{
    tr->morph_data = (void *) malloc(sizeof(double));
    *((double *)tr->morph_data) = 0.02;
}

static void yeast_free(Trile *tr)
{
    free(tr->morph_data);
}

static void yeast_draw(Trile *tr)
{
    double x = tr->x * 0.5,
           y = tr->y * M_SQRT3_2,
           z = *((double *)tr->morph_data);

    glPushMatrix();
        glTranslated(x, y, 0);
        glRotated(z*360, 0,0,1);
        glScaled(z,z,z);
        glTranslated(-x, -y, 0);
        trile_draw_vanilla(tr);
    glPopMatrix();
}

static void yeast_init_iter(Trile *tr, cberg_state *cberg)
{
    *((double *)(tr->morph_data)) = *((double *)tr->morph_data) + cberg->elapsed;
    if (*((double *)tr->morph_data) >= 1.0)
        tr->state = TRILE_STABLE;
}

static void yeast_dying_iter(Trile *tr, cberg_state *cberg)
{
    *((double *)tr->morph_data) = *((double *)tr->morph_data) - cberg->elapsed;
    if (*((double *)tr->morph_data) <= 0.02) /* XXX avoid fast del/cons? */
        tr->state = TRILE_DELETE;
}

/**** identity morph ****/

static void identity_init(Trile *tr)
{ tr->state = TRILE_STABLE; }

static void identity_free(Trile *tr)
{}

static void identity_draw(Trile *tr)
{ trile_draw_vanilla(tr); }

static void identity_init_iter(Trile *tr, cberg_state *cberg)
{}

static void identity_dying_iter(Trile *tr, cberg_state *cberg)
{ tr->state = TRILE_DELETE; }

/** now to handle selection **/

static Morph morphs[] = {
    {grow_init, grow_free, grow_draw, grow_init_iter, grow_dying_iter},
    {fall_init, fall_free, fall_draw, fall_init_iter, fall_dying_iter},
    {yeast_init, yeast_free, yeast_draw, yeast_init_iter, yeast_dying_iter},
    {identity_init,  /*always put identity last to skip it..*/
        identity_free, identity_draw, identity_init_iter, identity_dying_iter}
};    
static unsigned int nmorphs = countof(morphs);

static Morph *select_morph()
{ 
    if (crack)
        return &morphs[random() % (nmorphs-1)]; 
    else if (boring)
        return &morphs[nmorphs-1]; 
    else
        return morphs;
}


/***************************
 ** Trile superstructure functions. 
 **  */


static void triles_set_visible(cberg_state *cberg, Trile **root, int x, int y)
{
    Trile *parent = NULL, 
          *iter = *root;
    int goleft;

    while (iter != NULL) {
        parent = iter;
        goleft = (iter->x > x || (iter->x == x && iter->y > y));
        if (goleft)
            iter = iter->left;
        else if (iter->x == x && iter->y == y) {
            iter->visible = 1;
            return;
        } else
            iter = iter->right;
    }

    if (parent == NULL)
        *root = trile_new(cberg, x,y, NULL, NULL);
    else if (goleft)
        parent->left = trile_new(cberg, x,y, parent, *root);
    else
        parent->right = trile_new(cberg, x,y, parent, *root);
}

static unsigned int triles_foreach(Trile *root, void (*f)(Trile *, void *), 
  void *data)
{
    if (root == NULL) 
        return 0;
    
    f(root, data);
    return 1 + triles_foreach(root->left, f, data) 
      + triles_foreach(root->right, f, data);
}

static void triles_update_state(Trile **root, cberg_state *cberg)
{
    int process_current = 1;
    if (*root == NULL)
        return;

    while (process_current) {
        if ( (*root)->visible ) {
            if ( (*root)->state == TRILE_INIT )
                (*root)->morph->init_iter(*root, cberg);
            else if ( (*root)->state == TRILE_DYING ) {
                (*root)->state = TRILE_INIT;
                (*root)->morph->init_iter(*root, cberg);
            } else if ( (*root)->state == TRILE_NEW ) 
                (*root)->state = TRILE_INIT;

            (*root)->visible = 0;
        } else {
            if ( (*root)->state == TRILE_STABLE )
                (*root)->state = TRILE_DYING;
            else if ( (*root)->state == TRILE_INIT ) {
                (*root)->state = TRILE_DYING;
                (*root)->morph->dying_iter(*root, cberg);
            } else if ( (*root)->state == TRILE_DYING )
                (*root)->morph->dying_iter(*root, cberg);
        }

        if ( (*root)->state == TRILE_DELETE ) {
            Trile *splice_me;
            process_current = 1;

            if ((*root)->left == NULL) {
                splice_me = (*root)->right;
                if (splice_me)
                    splice_me->parent = (*root)->parent;
                else 
                    process_current = 0;
            } else if ((*root)->right == NULL) {
                splice_me = (*root)->left;
                splice_me->parent = (*root)->parent;
            } else {
                Trile *tmp;
                for (splice_me = (*root)->right; splice_me->left != NULL; )
                    splice_me = splice_me->left;
                tmp = splice_me->right;

                if (tmp) tmp->parent = splice_me->parent;

                if (splice_me == splice_me->parent->left)
                    splice_me->parent->left = tmp;
                else
                    splice_me->parent->right = tmp;

                splice_me->parent = (*root)->parent;
                splice_me->left = (*root)->left;
                (*root)->left->parent = splice_me;
                splice_me->right = (*root)->right;
                if ((*root)->right)
                    (*root)->right->parent = splice_me;
            }
            trile_free(*root);
            *root = splice_me;
        } else
            process_current = 0;
    }

    if (*root) {
        triles_update_state(&((*root)->left), cberg);
        triles_update_state(&((*root)->right), cberg);
    } 
}

static Trile *triles_find(Trile *tr, int x, int y)
{
    while (tr && !(tr->x == x && tr->y == y))
        if (x < tr->x || (x == tr->x && y < tr->y))
            tr = tr->left;
        else
            tr = tr->right;
    return tr;
}


/***************************
 ** Trile superstructure visibility functions. 
 **  strategy fine, implementation lazy&retarded =/
 **  */

#ifdef DEBUG
static double x_shit, y_shit;
#endif

static void calc_points(cberg_state *cberg, double *x1,double *y1, 
        double *x2,double *y2, double *x3,double *y3, double *x4,double *y4)
{
    double zNear, x_nearcenter, y_nearcenter, nhalfwidth, x_center, y_center;


    /* could cache these.. bahhhhhhhhhhhhhh */
    double halfheight = tan(cberg->fovy / 2 * M_PI_180) * cberg->zNear;
    double fovx_2 = atan(halfheight * cberg->aspect / cberg->zNear) * M_180_PI; 
    double zFar = cberg->zFar + M_RAD7_4;
    double fhalfwidth = zFar * tan(fovx_2 * M_PI_180)
                      + M_RAD7_4 / cos(fovx_2 * M_PI_180);
    double x_farcenter = cberg->x + zFar * cos(cberg->yaw * M_PI_180);
    double y_farcenter = cberg->y + zFar * sin(cberg->yaw * M_PI_180);
    *x1 = x_farcenter + fhalfwidth * cos((cberg->yaw - 90) * M_PI_180);
    *y1 = y_farcenter + fhalfwidth * sin((cberg->yaw - 90) * M_PI_180);
    *x2 = x_farcenter - fhalfwidth * cos((cberg->yaw - 90) * M_PI_180);
    *y2 = y_farcenter - fhalfwidth * sin((cberg->yaw - 90) * M_PI_180);

#ifdef DEBUG
    printf("pos (%.3f,%.3f) @ %.3f || fovx: %f || fovy: %f\n", 
            cberg->x, cberg->y, cberg->yaw, fovx_2 * 2, cberg->fovy);
    printf("\tfarcenter: (%.3f,%.3f) || fhalfwidth: %.3f \n"
           "\tp1: (%.3f,%.3f) || p2: (%.3f,%.3f)\n",
            x_farcenter, y_farcenter, fhalfwidth, *x1, *y1, *x2, *y2);
#endif

    if (cberg->z - halfheight <= 0) /* near view plane hits xy */
        zNear = cberg->zNear - M_RAD7_4;
    else /* use bottom of frustum */
        zNear = cberg->z / tan(cberg->fovy / 2 * M_PI_180) - M_RAD7_4;
    nhalfwidth = zNear * tan(fovx_2 * M_PI_180)
               + M_RAD7_4 / cos(fovx_2 * M_PI_180);
    x_nearcenter = cberg->x + zNear * cos(cberg->yaw * M_PI_180);
    y_nearcenter = cberg->y + zNear * sin(cberg->yaw * M_PI_180);
    *x3 = x_nearcenter - nhalfwidth * cos((cberg->yaw - 90) * M_PI_180);
    *y3 = y_nearcenter - nhalfwidth * sin((cberg->yaw - 90) * M_PI_180);
    *x4 = x_nearcenter + nhalfwidth * cos((cberg->yaw - 90) * M_PI_180);
    *y4 = y_nearcenter + nhalfwidth * sin((cberg->yaw - 90) * M_PI_180);

#ifdef DEBUG
    printf("\tnearcenter: (%.3f,%.3f) || nhalfwidth: %.3f\n"
           "\tp3: (%.3f,%.3f) || p4: (%.3f,%.3f)\n",
            x_nearcenter, y_nearcenter, nhalfwidth, *x3, *y3, *x4, *y4);
#endif


    /* center can be average or the intersection of diagonals.. */
#if 0
    {
        double c = nhalfwidth * (zFar -zNear) / (fhalfwidth + nhalfwidth);
        x_center = x_nearcenter + c * cos(cberg->yaw * M_PI_180);
        y_center = y_nearcenter + c * sin(cberg->yaw * M_PI_180);
    }
#else
    x_center = (x_nearcenter + x_farcenter) / 2;
    y_center = (y_nearcenter + y_farcenter) / 2;
#endif
#ifdef DEBUG
    x_shit = x_center;
    y_shit = y_center;
#endif
    
#define VSCALE(p)   *x##p = visibility * *x##p + (1-visibility) * x_center; \
                    *y##p = visibility * *y##p + (1-visibility) * y_center

    VSCALE(1);
    VSCALE(2);
    VSCALE(3);
    VSCALE(4);
#undef VSCALE
}

/* this is pretty stupid.. */
static inline void minmax4(double a, double b, double c, double d, 
  double *min, double *max)
{
    *min = *max = a;

    if (b > *max)       *max = b;
    else if (b < *min)  *min = b;
    if (c > *max)       *max = c;
    else if (c < *min)  *min = c;
    if (d > *max)       *max = d;
    else if (d < *min)  *min = d;
}

typedef struct {
    double min, max, start, dx;
} LS;

#define check_line(a, b)                     \
    if (fabs(y##a-y##b) > 0.001) {                    \
        ls[count].dx = (x##b-x##a)/(y##b-y##a);               \
        if (y##b > y##a) {                            \
            ls[count].start = x##a;                     \
            ls[count].min = y##a;                       \
            ls[count].max = y##b;                       \
        } else {                                  \
            ls[count].start = x##b;                     \
            ls[count].min = y##b;                       \
            ls[count].max = y##a;                       \
        }                                         \
        ++count;                                    \
    }

static unsigned int build_ls(cberg_state *cberg, 
                      double x1, double y1, double x2, double y2, 
                      double x3, double y3, double x4, double y4, LS *ls,
                      double *trough, double *peak)
{
    unsigned int count = 0;

    check_line(1, 2);
    check_line(2, 3);
    check_line(3, 4);
    check_line(4, 1);

    minmax4(y1, y2, y3, y4, trough, peak);
    return count;
}

#undef check_line

/*needs bullshit to avoid double counts on corners.*/
static void find_bounds(double y, double *left, double *right, LS *ls,
        unsigned int nls)
{
    double x;
    unsigned int i, set = 0;

    for (i = 0; i != nls; ++i)
        if (ls[i].min <= y && ls[i].max >= y) {
            x = (y - ls[i].min) * ls[i].dx + ls[i].start;
            if (!set) {
                *left = x;
                ++set;
            } else if (fabs(x - *left) > 0.001) {
                if (*left < x)
                    *right = x;
                else {
                    *right = *left;
                    *left = x;
                }
                return;
            }
        }

    /* just in case we somehow blew up */
    *left = 3.0;
    *right = -3.0;
}

static void mark_visible(cberg_state *cberg)
{
    double trough, peak, yval, left, right;
    double x1,y1, x2,y2, x3,y3, x4,y4;
    int start, stop, x, y;
    LS ls[4];
    unsigned int nls;

    calc_points(cberg, &x1,&y1, &x2,&y2, &x3,&y3, &x4,&y4);
    nls = build_ls(cberg, x1,y1, x2,y2, x3,y3, x4,y4, ls, &trough, &peak);

    start = (int) ceil(trough / M_SQRT3_2);
    stop = (int) floor(peak / M_SQRT3_2);
    
    for (y = start; y <= stop; ++y) {
        yval = y * M_SQRT3_2;
        find_bounds(yval, &left, &right, ls, nls);
        for (x = (int) ceil(left*2-1); x <= (int) floor(right*2); ++x) 
            triles_set_visible(cberg, &(cberg->trile_head), x, y);
    }
}


/***************************
 ** color schemes
 ** */

static void plain_land(double z)
{ glColor3f(pow((z/0.35),4),  z/0.35, pow((z/0.35),4)); }
static void plain_water(double z)
{ glColor3f(0.0, (z+0.35)*1.6, 0.8); }

static void ice_land(double z)
{ glColor3f((0.35 - z)/0.35, (0.35 - z)/0.35, 1.0); }
static void ice_water(double z)
{ glColor3f(0.0, (z+0.35)*1.6, 0.8); }


static void magma_land(double z)
{ glColor3f(z/0.35, z/0.2,0); }
static void magma_lava(double z)
{ glColor3f((z+0.35)*1.6, (z+0.35), 0.0); }

static double vs0r,vs0g,vs0b, vs1r, vs1g, vs1b,
              vf0r,vf0g,vf0b, vf1r, vf1g, vf1b;
static void vomit_solid(double z)
{
    double norm = fabs(z) / 0.35;
    glColor3f( 
      (1-norm) * vs0r + norm * vs1r, 
      (1-norm) * vs0g + norm * vs1g, 
      (1-norm) * vs0b + norm * vs1b 
    );
}
static void vomit_fluid(double z)
{
    double norm = z / -0.35;
    glColor3f( 
      (1-norm) * vf0r + norm * vf1r, 
      (1-norm) * vf0g + norm * vf1g, 
      (1-norm) * vf0b + norm * vf1b 
    );
}


static Color colors[] = {
    {"plain", plain_land, plain_water, {0.0, 0.0, 0.0, 1.0}},
    {"ice", ice_land, ice_water, {0.0, 0.0, 0.0, 1.0}},
    {"magma", magma_land, magma_lava, {0.3, 0.3, 0.0, 1.0}},
    {"vomit", vomit_solid, vomit_fluid, {0.3, 0.3, 0.0, 1.0}}, /* no error! */
};
static unsigned int ncolors = countof(colors);

static Color *select_color()
{
    int idx = -1;
    if ( ! strcmp(color, "random") )
        idx = random() % ncolors;
    else {
        unsigned int i;
        for (i = 0; i != ncolors; ++i)
            if ( ! strcmp(colors[i].id, color) ) {
                idx = i;
                break;
            }

        if (idx == -1) {
            printf("invalid color scheme selected; valid choices are:\n");
            for (i = 0; i != ncolors; ++i)
                printf("\t%s\n", colors[i].id);
            printf("\t%s\n", "random");
            idx = 0;
        }
    }

    if ( ! strcmp(colors[idx].id, "vomit") ) { /* need to create it (ghetto) */
        vs0r = random()/(double)RAND_MAX;
        vs0g = random()/(double)RAND_MAX;
        vs0b = random()/(double)RAND_MAX; 
        vs1r = random()/(double)RAND_MAX; 
        vs1g = random()/(double)RAND_MAX;
        vs1b = random()/(double)RAND_MAX;
        vf0r = random()/(double)RAND_MAX;
        vf0g = random()/(double)RAND_MAX;
        vf0b = random()/(double)RAND_MAX; 
        vf1r = random()/(double)RAND_MAX; 
        vf1g = random()/(double)RAND_MAX; 
        vf1b = random()/(double)RAND_MAX; 
 
        glClearColor(random()/(double)RAND_MAX,
                     random()/(double)RAND_MAX,
                     random()/(double)RAND_MAX,
                     1.0);
    } else {
        glClearColor(colors[idx].bg[0],
                     colors[idx].bg[1],
                     colors[idx].bg[2],
                     colors[idx].bg[3]);
    }
    return colors + idx;
}


/***************************
 ** misc helper functions
 ** */


/* simple one for now.. */
static inline double drunken_rando(double cur_val, double max, double width)
{
    double r = random() / (double) RAND_MAX * 2;
    if (cur_val > 0)
        if (r >= 1)
            return cur_val + (r-1) * width * (1-cur_val/max);
        else
            return cur_val - r * width; 
    else
        if (r >= 1)
            return cur_val - (r-1) * width * (1+cur_val/max);
        else
            return cur_val + r * width; 
}


/***************************
 ** core crackberg routines
 ** */


void crackberg_init(ModeInfo *mi)
{
    cberg_state *cberg;

    if (!cbergs) {
        nsubdivs %= 16; /* just in case.. */
        epoints = 1 + (1 << nsubdivs);
        tpoints = epoints * (epoints + 1) / 2;
        ntris = (1 << (nsubdivs << 1));
        tnorms = ( (flat) ? ntris : tpoints);
        dx = 1.0 / (1 << nsubdivs);

        if ( !(cbergs = calloc(MI_NUM_SCREENS(mi), sizeof(cberg_state)))
          || !(heights = malloc(tpoints * sizeof(double)))
          || (lit && !(norms = malloc(3 * tnorms * sizeof(double)))) ) {
            perror(progname);
            exit(1);
        }

        if (visibility > 1.0 || visibility < 0.2) {
            printf("visibility must be in range [0.2 .. 1.0]\n");
            visibility = 1.0;
        }
    }

    cberg = cbergs + MI_SCREEN(mi);
    
    cberg->glx_context = init_GL(mi);
    cberg->motion_state = MOTION_AUTO;
    cberg->mspeed = 1.0;
    cberg->z = 0.5;

    cberg->fovy = 60.0;
    cberg->zNear = 0.5;
    cberg->zFar = 5.0;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel((flat) ? GL_FLAT : GL_SMOOTH);
    glPolygonMode(GL_FRONT_AND_BACK, (wire) ? GL_LINE : GL_FILL);

    if (lit) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glEnable(GL_NORMALIZE);
        glEnable(GL_RESCALE_NORMAL); 
    }

    cberg->color = select_color();

    crackberg_reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
}

void crackberg_reshape(ModeInfo *mi, int w, int h)
{
    int h2;
    cberg_state *cberg = cbergs + MI_SCREEN(mi);

    if (letterbox && (h2 = w * 9 / 16) < h) {
        glViewport(0, (h-h2)/2, w, h2);
        cberg->aspect = w/(double)h2;
    } else {
        glViewport (0, 0, w, h);
        cberg->aspect = w/(double)h;
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(cberg->fovy, cberg->aspect, cberg->zNear, cberg->zFar);
    glMatrixMode(GL_MODELVIEW);
}

Bool crackberg_handle_ev(ModeInfo *mi, XEvent *ev)
{
    cberg_state *cberg = cbergs + MI_SCREEN(mi);

    if (ev->xany.type == KeyPress) {
        switch (XKeycodeToKeysym(mi->dpy, ev->xkey.keycode, 0)) {
            case XK_Left:   cberg->motion_state |= MOTION_LROT;  break;
            case XK_Right:  cberg->motion_state |= MOTION_RROT;  break;
            case '1':       cberg->motion_state |= MOTION_DEC;   break; 
            case '2':       cberg->motion_state |= MOTION_INC;   break;
            case 'a':       cberg->motion_state |= MOTION_LEFT;  break;
            case 'd':       cberg->motion_state |= MOTION_RIGHT; break;
            case 's':       cberg->motion_state |= MOTION_BACK;  break;
            case 'w':       cberg->motion_state |= MOTION_FORW;  break;
            default:        return False;
        }
        cberg->motion_state |= MOTION_MANUAL;
    } else if (ev->xany.type == KeyRelease) { 
        XEvent peek_ev;
        if (XPending(mi->dpy)) {
            XPeekEvent(mi->dpy, &peek_ev);
            if (peek_ev.type == KeyPress
             && peek_ev.xkey.keycode == ev->xkey.keycode       
             && peek_ev.xkey.time - ev->xkey.time < 2) {
                XNextEvent(mi->dpy, &peek_ev); /* drop bullshit repeat events */
                return False;
            }
        }

        switch (XKeycodeToKeysym(mi->dpy, ev->xkey.keycode, 0)) {
            case XK_Left:   cberg->motion_state &= ~MOTION_LROT;  break;
            case XK_Right:  cberg->motion_state &= ~MOTION_RROT;  break;
            case '1':       cberg->motion_state &= ~MOTION_DEC;   break; 
            case '2':       cberg->motion_state &= ~MOTION_INC;   break;
            case 'a':       cberg->motion_state &= ~MOTION_LEFT;  break;
            case 'd':       cberg->motion_state &= ~MOTION_RIGHT; break;
            case 's':       cberg->motion_state &= ~MOTION_BACK;  break;
            case 'w':       cberg->motion_state &= ~MOTION_FORW;  break;
            case ' ':       
                if (cberg->motion_state == MOTION_MANUAL)
                    cberg->motion_state = MOTION_AUTO;     
                break;
            default:            return False;
        }
    } else
        return False;
    return True;
}   
 
void crackberg_draw(ModeInfo *mi)
{
    cberg_state *cberg = cbergs + MI_SCREEN(mi);
    struct timeval cur_frame_t;
    double cur_frame;
    static float lpos[] = {2.0,0.0,-0.3,0.0};

    if (!cberg->glx_context) /*XXX does this get externally tweaked? it kinda*/
        return;               /*XXX can't.. check it in crackberg_init*/

    gettimeofday(&cur_frame_t, NULL);
    cur_frame = cur_frame_t.tv_sec + cur_frame_t.tv_usec / 1.0E6;
    if ( cberg->prev_frame ) { /*not first run */
        static double elapsed = 1.0;

        cberg->elapsed = cur_frame - cberg->prev_frame;

        if (cberg->motion_state == MOTION_AUTO) {
            cberg->x += cberg->dx * cberg->elapsed;
            cberg->y += cberg->dy * cberg->elapsed;
            /* cberg->z */
            /* cberg->pitch */
            /* cberg->roll */
            cberg->yaw += cberg->dyaw * cberg->elapsed;

            elapsed += cberg->elapsed;
            if (elapsed >= 0.8) {
                elapsed = 0.0;
                cberg->dx = drunken_rando(cberg->dx, 2.5, 0.8);
                cberg->dy = drunken_rando(cberg->dy, 2.5, 0.8);
                /* cberg->dz */
                /* cberg->dpitch */
                /* cberg->droll */
                cberg->dyaw = drunken_rando(cberg->dyaw, 40.0,  8.0);
            }
        } else {
            double scale = cberg->elapsed * cberg->mspeed;
            if (cberg->motion_state & MOTION_BACK) {
                cberg->x -= cos(cberg->yaw * M_PI_180) * scale;
                cberg->y -= sin(cberg->yaw * M_PI_180) * scale;
            }
            if (cberg->motion_state & MOTION_FORW) {
                cberg->x += cos(cberg->yaw * M_PI_180) * scale;
                cberg->y += sin(cberg->yaw * M_PI_180) * scale;
            }

            if (cberg->motion_state & MOTION_LEFT) {
                cberg->x -= sin(cberg->yaw * M_PI_180) * scale;
                cberg->y += cos(cberg->yaw * M_PI_180) * scale;
            }
            if (cberg->motion_state & MOTION_RIGHT) {
                cberg->x += sin(cberg->yaw * M_PI_180) * scale;
                cberg->y -= cos(cberg->yaw * M_PI_180) * scale;
            }

            if (cberg->motion_state & MOTION_LROT)
                cberg->yaw += 45 * scale;
            if (cberg->motion_state & MOTION_RROT)
                cberg->yaw -= 45 * scale;

            if (cberg->motion_state & MOTION_DEC)
                cberg->mspeed /= pow(MSPEED_SCALE, elapsed);
            if (cberg->motion_state & MOTION_INC)
                cberg->mspeed *= pow(MSPEED_SCALE, elapsed);

        }
    }
    cberg->prev_frame = cur_frame;

    mark_visible(cberg);
    triles_update_state(&(cberg->trile_head), cberg);
        
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(0,0,0, 1,0,0, 0,0,1);
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    /*glRotated(cberg->roll, 1,0,0); / * XXX blah broken and unused for now..* /
    glRotated(cberg->pitch, 0,1,0); */
    glRotated(-cberg->yaw, 0,0,1); /* camera sees ->yaw over */
    glTranslated(-cberg->x, -cberg->y, -cberg->z);

    mi->polygon_count = ntris * 
      triles_foreach(cberg->trile_head, trile_draw,(void *) cberg);
    
    if (mi->fps_p)  
        do_fps(mi);

#ifdef DEBUG
    glBegin(GL_LINES);
        glColor3f(1.0,0.0,0.0);
        glVertex3d(x_shit, y_shit, 0.0);
        glVertex3d(x_shit, y_shit, 1.0);
    glEnd();
#endif

    glFinish();
    glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
}

/* uh */
void crackberg_free(ModeInfo *mi)
{
    if (lit)
        free(norms);

    free(heights);
    free(cbergs);
}

#endif /* USE_GL */
