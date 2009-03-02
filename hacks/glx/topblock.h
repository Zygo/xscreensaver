/* topblock - openGL based hack  */

static void buildCarpet(ModeInfo *);
static void polygonPlane(int, int, int, int, int ,int);
static void buildBlock(ModeInfo *);
static void generateNewBlock(ModeInfo *);
static void followBlock(ModeInfo *);
static void buildBlobBlock(ModeInfo *);
static double quadrantCorrection(double,int,int,int,int);

/* this structure holds all the attributes about a block */
typedef struct blockNode {
	int color;		/* indexed */
	int rotation;		/* indexed: 0=S-N, 1=W-E, 2=N-S, 3=E-W */
	GLfloat height;
	GLfloat x;
	GLfloat y;
	int falling;		
	struct blockNode *next;
} NODE;


/* some handy macros and definitions */
#define blockHeight 1.49f
#define getHeight(a) (a * blockHeight)

#define getOrientation(a) (a * 90)

#define blockWidth 2.0f
#define getLocation(a) (a * blockWidth)

#define TOLLERANCE 0.1

#define cylSize 0.333334f
#define uddSize 0.4f
#define	singleThick 0.29	/* defines the thickness of the carpet */









