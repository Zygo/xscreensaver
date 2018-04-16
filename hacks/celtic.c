/* celtic, Copyright (c) 2006 Max Froumentin <max@lapin-bleu.net>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * A celtic pattern programme inspired by "Les Entrelacs Celtes", by
 * Christian Mercat, Dossier Pour La Science, no. 47, april/june 2005.
 * See <http://www.entrelacs.net/>
 */

#include <math.h>
#include "screenhack.h"
#include "erase.h"

#define SQRT_3 1.73205080756887729352
#undef assert
#define assert(EXP) do { if (!((EXP))) abort(); } while(0)

/*-----------------------------------------*/

struct params {
  unsigned long curve_width, shadow_width;
  double shape1, shape2;
  unsigned long margin;

  enum graph_type { polar, tgrid, kennicott, triangle } type;
  unsigned long edge_size;
  unsigned long cluster_size; /* only used if type is kennicott */
  unsigned long delay;        /* controls curve drawing speed (step delay 
			       * in microsecs) */
  unsigned long nsteps; /* only if triangle: number of subdivisions along the side */
  unsigned long nb_orbits;          /* only used if type is polar */
  unsigned long nb_nodes_per_orbit; /* only used if type is polar */

  double angle; /* angle of rotation of the graph around the centre */
};

/*-----------------------------------------*/
typedef enum direction {
  CLOCKWISE=0, ANTICLOCKWISE=1
} Direction;


/*-----------------------------------------*/
typedef struct array {
  int nb_elements;
  int nb_allocated_elements;
  int increment;
  void **elements;
} *Array;

typedef struct graph {
  Array nodes;
  Array edges;
} *Graph;

typedef struct edge_couple {
  int **array;
  int size;
} *EdgeCouple;

typedef struct pattern {
  double shape1, shape2;
  EdgeCouple ec;
  Graph graph;
  Array splines;
  int ncolors;
} *Pattern;

struct state {
  Display *dpy;
  Window window;
  eraser_state *eraser;

  int ncolors;
  XColor *colors;
  GC gc,shadow_gc,gc_graph;

  Bool showGraph;
  Pattern pattern;
  Graph graph;
  XWindowAttributes xgwa;
  int delay2;
  int reset, force_reset;
  double t;

  struct params params;
};




static Array array_new(int increment)
{
  Array new;
  assert(new=(Array)calloc(1,sizeof(struct array)));
  new->nb_elements=0;
  new->nb_allocated_elements=0;
  new->increment=increment;
  return new;
}

static void array_del(Array a, void (*free_element)(void*))
{
  int i;
  if (free_element) 
    for (i=0;i<a->nb_elements;i++) 
      free_element(a->elements[i]);
  free(a->elements);
  free(a);
}

static void array_add_element(Array a, void *element)
{
  if (a->nb_elements == a->nb_allocated_elements) {
    /* we must allocate more */
    a->nb_allocated_elements+=a->increment;
    a->elements=realloc(a->elements,a->nb_allocated_elements*sizeof(void *));
  }
  a->elements[a->nb_elements++]=element;
}
/*-----------------------------------------*/

typedef struct node {
  double x,y;
  Array edges;
} *Node;

typedef struct edge {
  Node node1, node2;
  double angle1, angle2;
} *Edge;

/*-----------------------------------------*/
/* Node functions */

static Node node_new(double x, double y)
{
  Node new;
  assert(new = (Node)calloc(1,sizeof(struct node)));
  new->x=x;
  new->y=y;
  new->edges = array_new(10);
  return new;
}

static void node_del(void *n) 
{ /* not Node * because the function is passed to array_del */
  array_del(((Node)n)->edges,NULL);
  free(n);
}

#if 0
static void node_to_s(Node n, FILE *f) 
{
  fprintf(f,"Node: %g %g\n",n->x,n->y);
}
#endif

static void node_draw(struct state *st, Node n)
{
  XDrawArc(st->dpy,st->window,st->gc_graph,(int)rint(n->x)-5,(int)rint(n->y)-5,10,10,0,360*64);
}
  
static void node_add_edge(Node n, Edge e)
{
  array_add_element(n->edges,e);
}


/*-----------------------------------------*/
/* Edge functions */

static Edge edge_new(Node n1, Node n2)
{
  Edge new; 
  assert(new = (Edge)calloc(1,sizeof(struct edge)));
  new->node1=n1;
  new->node2=n2;
  new->angle1=atan2(new->node2->y - new->node1->y, new->node2->x - new->node1->x);
  if (new->angle1 < 0) new->angle1+=6.28;

  new->angle2=atan2(new->node1->y - new->node2->y, new->node1->x - new->node2->x);
  if (new->angle2 < 0) new->angle2+=6.28;
  return new;
}

static void edge_del(void *e) /* not Edge * because the function is passed to array_del */
{
  free(e);
}

#if 0
static void edge_to_s(Edge e, FILE *f)
{
  fprintf(f,"Edge: (%g, %g), (%g, %g) angles: %g, %g\n",
	  e->node1->x, e->node1->y, e->node2->x, e->node2->y,
	  e->angle1, e->angle2);
}
#endif

static void edge_draw(struct state *st, Edge e)
{
  XDrawLine(st->dpy,st->window,st->gc_graph, e->node1->x, e->node1->y, e->node2->x, e->node2->y);
}

static double edge_angle(Edge e, Node n)
{
  /* returns the angle of the edge at Node n */
  assert(n==e->node1 || n==e->node2);
  if (n==e->node1) return e->angle1; else return e->angle2;
}

static Node edge_other_node(Edge e, Node n)
{
  assert(n==e->node1 || n==e->node2);
  if (n==e->node1) return e->node2; else return e->node1;
}

static double edge_angle_to(Edge e, Edge e2, Node node, Direction direction)
{
  /* returns the absolute angle from this edge to "edge2" around
     "node" following "direction" */
  double a;

  if (direction==CLOCKWISE)
    a=edge_angle(e,node) - edge_angle(e2,node);
  else
    a=edge_angle(e2,node) - edge_angle(e,node);

  if (a<0) return a+2*M_PI; else return a;
}

/*-----------------------------------------*/

static Graph graph_new(struct state *st)
{
  Graph new;
  assert(new = (Graph)calloc(1,sizeof(struct graph)));
  new->nodes = array_new(100);
  new->edges = array_new(100);
  return new;
}

static void graph_del(Graph g)
{
  array_del(g->nodes, &node_del);
  array_del(g->edges, &edge_del);
  free(g);
}


static void graph_add_node(Graph g, Node n)
{
  array_add_element(g->nodes, n);
}

static void graph_add_edge(Graph g, Edge e)
{
  array_add_element(g->edges, e);
  
  /* for each node n of e, add n to pointer e */
  node_add_edge(e->node1, e);
  node_add_edge(e->node2, e);
}

static Edge graph_next_edge_around(Graph g, Node n, Edge e, Direction direction)
{
  /* return the next edge after e around node n clockwise */
  double angle, minangle=20;
  Edge next_edge = e, edge;
  int i;

  for (i=0;i<n->edges->nb_elements;i++) {
    edge=n->edges->elements[i];
    if (edge != e) {
      angle = edge_angle_to(e,edge,n,direction);
      if (angle < minangle) {
	next_edge=edge;
	minangle=angle;
      }
    }
  }
  return next_edge;
}
  
#if 0
static void graph_to_s(Graph g, FILE *f)
{
  int i;
  for (i=0;i<g->nodes->nb_elements;i++) 
    node_to_s(g->nodes->elements[i],f);
  for (i=0;i<g->edges->nb_elements;i++)
    edge_to_s(g->edges->elements[i],f);
}
#endif

static void graph_draw(struct state *st, Graph g)
{
  int i;
  
  for (i=0;i<g->nodes->nb_elements;i++) 
    node_draw(st, g->nodes->elements[i]);
  for (i=0;i<g->edges->nb_elements;i++)
    edge_draw(st, g->edges->elements[i]);
}

static void graph_rotate(Graph g, double angle, int cx, int cy)
{
  /* rotate all the nodes of the graph around the centre */
  int i; 
  float c=cos(angle),s=sin(angle),x,y;
  Node n;
  for (i=0;i<g->nodes->nb_elements;i++) {
    n=g->nodes->elements[i];
    x=n->x; y=n->y;
    n->x = (x-cx)*c-(y-cy)*s + cx;
    n->y = (x-cx)*s+(y-cy)*c + cy;
  }
}


/*---------------------------*/

static Graph make_polar_graph(struct state *st, 
                              int xmin, int ymin, int width, int height, 
		       int nbp, /* number of points on each orbit */
		       int nbo /* number of orbits */)
     /* make a simple grid graph, with edges present or absent randomly */
{
  int cx = width/2+xmin, cy=height/2+ymin; /* centre */
  int os = (width<height?width:height)/(2*nbo); /* orbit height */
  Graph g;
  Node *grid;
  int o,p;

  /* generate nodes */
  assert(grid=(Node*)calloc(1+nbp*nbo,sizeof(Node)));
  assert(g=graph_new(st));
  
  graph_add_node(g, grid[0]=node_new((double)cx,(double)cy));

  for (o=0;o<nbo;o++)
    for (p=0;p<nbp;p++)
      graph_add_node(g,
		     grid[1+o*nbp+p]=node_new(cx+(o+1)*os*sin(p*2*M_PI/nbp), 
					      cy+(o+1)*os*cos(p*2*M_PI/nbp)));


  /* generate edges */
  for (o=0;o<nbo;o++)
    for (p=0;p<nbp;p++) {
      if (o==0) /* link first orbit nodes with centre */
	graph_add_edge(g,edge_new(grid[1+o*nbp+p],grid[0]));
      else /* liink orbit nodes with lower orbit */
      	graph_add_edge(g,edge_new(grid[1+o*nbp+p],grid[1+(o-1)*nbp+p]));
      /* link along orbit */
      graph_add_edge(g,edge_new(grid[1+o*nbp+p],
				grid[1+o*nbp+(p+1)%nbp]));
    }

  free(grid);
  return g;
}


static Graph make_grid_graph(struct state *st, 
                             int xmin, int ymin, int width, int height, int step)
     /* make a simple grid graph */
{
  Graph g;
  int row,col,x,y;
  int size=(width<height?height:width);

  /* empirically, it seems there are 2 curves only if both
     nbcol and nbrow are even, so we round them to even */
  int nbcol=(2+size/step)/2*2, nbrow=(2+size/step)/2*2;

  Node *grid;
  assert(grid=(Node*)calloc(nbrow*nbcol,sizeof(Node)));
  assert(g=graph_new(st));


  /* adjust xmin and xmax so that the grid is centered */
  xmin+=(width-(nbcol-1)*step)/2; 
  ymin+=(height-(nbrow-1)*step)/2;

  /* create node grid */
  for (row=0;row<nbrow;row++)
    for (col=0;col<nbcol;col++) {
      x=col*step+xmin;
      y=row*step+ymin;
      grid[row+col*nbrow]=node_new((double)x, (double)y);
      graph_add_node(g, grid[row+col*nbrow]);
    }

  /* create edges */
  for (row=0;row<nbrow;row++)
    for (col=0;col<nbcol;col++) {
      if (col!=nbcol-1)
	graph_add_edge(g,edge_new(grid[row+col*nbrow],
				  grid[row+(col+1)*nbrow]));
      if (row!=nbrow-1)
	graph_add_edge(g,edge_new(grid[row+col*nbrow],grid[row+1+col*nbrow]));
      if (col!=nbcol-1 && row!=nbrow-1) {
	  graph_add_edge(g,edge_new(grid[row+col*nbrow],
				    grid[row+1+(col+1)*nbrow]));
	  graph_add_edge(g,edge_new(grid[row+1+col*nbrow],
				    grid[row+(col+1)*nbrow]));
      }
    }

  free(grid);
  
  return g;
}


static Graph make_triangle_graph(struct state *st, 
                                 int xmin, int ymin, int width, int height, int edge_size)
{
  Graph g;
  Node *grid;
  int row,col;
  double L=(width<height?width:height)/2.0; /* circumradius of the triangle */
  double cx=xmin+width/2.0, cy=ymin+height/2.0; /* centre of the triangle */
  double p2x=cx-L*SQRT_3/2.0, p2y=cy+L/2.0; /* p2 is the bottom left vertex */
  double x,y;
  int nsteps=3*L/(SQRT_3*edge_size);

  assert(grid=(Node*)calloc((nsteps+1)*(nsteps+1),sizeof(Node)));
  assert(g=graph_new(st));

  /* create node grid */
  for (row=0;row<=nsteps;row++)
    for (col=0;col<=nsteps;col++) 
      if (row+col<=nsteps) {
        x=p2x+col*L*SQRT_3/nsteps + row*L*SQRT_3/(2*nsteps);
        y=p2y-row*3*L/(2*nsteps);
        grid[col+row*(nsteps+1)]=node_new((double)x, (double)y);
        graph_add_node(g, grid[col+row*(nsteps+1)]);
      }

  /* create edges */
  for (row=0;row<nsteps;row++)
    for (col=0;col<nsteps;col++)
      if (row+col<nsteps) { 
          /* horizontal edges */
          graph_add_edge(g,edge_new(grid[row+col*(nsteps+1)],grid[row+(col+1)*(nsteps+1)]));
          /* vertical edges */
          graph_add_edge(g,edge_new(grid[row+col*(nsteps+1)],grid[row+1+col*(nsteps+1)]));
          /* diagonal edges */
          graph_add_edge(g,edge_new(grid[row+1+col*(nsteps+1)],grid[row+(col+1)*(nsteps+1)]));
      }

  free(grid);
  return g;
  
}


static Graph make_kennicott_graph(struct state *st, 
                                  int xmin, int ymin, int width, int height, int step,
                           int cluster_size)
     /* make a graph inspired by one of the motifs from the Kennicott bible */
     /* square grid of clusters of the shape  /|\
      *                                       ---
      *                                       \|/
      * cluster_size is the length of an edge of a cluster
      */
{
  Graph g;
  int row,col,x,y;
  int size=width<height?height:width;
  int nbcol=(1+size/step)/2*2, nbrow=(1+size/step)/2*2;
  Node *grid;

  /* there are 5 nodes by for each cluster */
  assert(grid=(Node*)calloc(5*nbrow*nbcol,sizeof(Node)));
  assert(g=graph_new(st));

  /* adjust xmin and xmax so that the grid is centered */
  xmin+=(width-(nbcol-1)*step)/2; 
  ymin+=(height-(nbrow-1)*step)/2;

  /* create node grid */
  for (row=0;row<nbrow;row++)
    for (col=0;col<nbcol;col++) {
      int ci=5*(row+col*nbrow);
      x=col*step+xmin;
      y=row*step+ymin;

      /* create a cluster centred on x,y */
      grid[ci  ]=node_new((double)x, (double)y);
      grid[ci+1]=node_new((double)(x+cluster_size), (double)y);
      grid[ci+2]=node_new((double)x, (double)(y-cluster_size));
      grid[ci+3]=node_new((double)(x-cluster_size), (double)y);
      grid[ci+4]=node_new((double)x, (double)(y+cluster_size));

      graph_add_node(g, grid[ci]);
      graph_add_node(g, grid[ci+1]);
      graph_add_node(g, grid[ci+2]);
      graph_add_node(g, grid[ci+3]);
      graph_add_node(g, grid[ci+4]);

      /* internal edges */
      graph_add_edge(g,edge_new(grid[ci], grid[ci+1]));      
      graph_add_edge(g,edge_new(grid[ci], grid[ci+2]));
      graph_add_edge(g,edge_new(grid[ci], grid[ci+3]));
      graph_add_edge(g,edge_new(grid[ci], grid[ci+4]));
      graph_add_edge(g,edge_new(grid[ci+1], grid[ci+2]));      
      graph_add_edge(g,edge_new(grid[ci+2], grid[ci+3]));
      graph_add_edge(g,edge_new(grid[ci+3], grid[ci+4]));
      graph_add_edge(g,edge_new(grid[ci+4], grid[ci+1]));

    }

  /* create inter-cluster edges */
  for (row=0;row<nbrow;row++)
    for (col=0;col<nbcol;col++) {
      if (col!=nbcol-1)
        /* horizontal edge from edge 1 of cluster (row, col) to edge 3
         * of cluster (row,col+1) */
        graph_add_edge(g,edge_new(grid[5*(row+col*nbrow)+1],grid[5*(row+(col+1)*nbrow)+3]));
      if (row!=nbrow-1)
        /* vertical edge from edge 4 of cluster (row, col) to edge 2
         * of cluster (row+1,col) */
        graph_add_edge(g,edge_new(grid[5*(row+col*nbrow)+4],
                                  grid[5*(row+1+col*nbrow)+2]));
    }
  free(grid);
  return g;
}

/*---------------------------*/
typedef struct spline_segment {
  double x1,y1,x2,y2,x3,y3,x4,y4;
} *SplineSegment;

typedef struct spline {
  Array segments; /* array of SplineSegment */
  int color;
} *Spline;

static Spline spline_new(int color)
{
  Spline new=(Spline)calloc(1,sizeof(struct spline));
  new->segments=array_new(30);
  new->color=color;
  return new;
}

static void spline_del(void *s)
{
  array_del(((Spline)s)->segments,&free);
  free(s);
}

static void spline_add_segment(Spline s,
                        double x1, double y1, double x2, double y2, 
                        double x3, double y3, double x4, double y4)
{
  SplineSegment ss=(SplineSegment)calloc(1,sizeof(struct spline_segment));
  ss->x1=x1;  ss->x2=x2;  ss->x3=x3;  ss->x4=x4;
  ss->y1=y1;  ss->y2=y2;  ss->y3=y3;  ss->y4=y4;
  array_add_element(s->segments,ss);
}

#if 0
static void spline_to_s(Spline s, FILE *f)
{
  int i;
  SplineSegment ss;
  fprintf(f,"Spline: \n");
  for (i=0;i<s->segments->nb_elements;i++) {
    ss=s->segments->elements[i];
    fprintf(f," - segment %d: (%g, %g),(%g, %g),(%g, %g),(%g, %g)\n",
            i,ss->x1,ss->y1,ss->x2,ss->y2,ss->x3,ss->y3,ss->x4,ss->y4);
  }
}
#endif

static void spline_value_at(Spline s, double *x, double *y, double t, int *segment)
{
  int si;
  double tt;
  SplineSegment ss;
  si = floor(t*s->segments->nb_elements);
  tt = t*s->segments->nb_elements - si;
  assert(tt>=0 && tt<1);
  ss=s->segments->elements[si];

  *x = ss->x1*(1-tt)*(1-tt)*(1-tt)+3*ss->x2*tt*(1-tt)*(1-tt)+3*ss->x3*tt*tt*(1-tt)+ss->x4*tt*tt*tt;
  *y = ss->y1*(1-tt)*(1-tt)*(1-tt)+3*ss->y2*tt*(1-tt)*(1-tt)+3*ss->y3*tt*tt*(1-tt)+ss->y4*tt*tt*tt;

  *segment=si;
}

/*---------------------------*/

static EdgeCouple edge_couple_new(int nb_edges) {
  int i;
  EdgeCouple new = (EdgeCouple)calloc(1,sizeof(struct edge_couple));
  new->array = (int **)calloc(nb_edges, sizeof(int*));
  new->size = nb_edges;

  for (i=0;i<nb_edges;i++) {
    new->array[i]=(int *)calloc(2,sizeof(int));
    new->array[i][CLOCKWISE]=0;
    new->array[i][ANTICLOCKWISE]=0;
  }
  return new;
}

static void edge_couple_del(EdgeCouple e)
{
  int i;
  for (i=0;i<e->size;i++) free(e->array[i]);
  free(e->array);
  free(e);
}
    
/*---------------------------*/

static Pattern pattern_new(struct state *st, Graph g, double shape1, double shape2)
{
  Pattern new;
  assert(new=(Pattern)calloc(1,sizeof(struct pattern)));
  new->shape1=shape1;
  new->shape2=shape2;
  new->graph=g;
  new->ec=edge_couple_new(g->edges->nb_elements);
  new->splines=array_new(10);
  new->ncolors=st->ncolors;
  return new;
}

static void pattern_del(Pattern p)
{
  edge_couple_del(p->ec);
  array_del(p->splines,&spline_del);
  free(p);
}

static void pattern_edge_couple_set(Pattern p, Edge e, Direction d, int value) 
{
  int i;
  for (i=0;i<p->graph->edges->nb_elements;i++)
    if (p->graph->edges->elements[i]==e) {
      p->ec->array[i][d]=value;
      return;
    }
}

static void pattern_draw_spline_direction(Pattern p, Spline s,
                                   Node node, Edge edge1, Edge edge2, 
                                   Direction direction)
{
  double x1=(edge1->node1->x+edge1->node2->x)/2.0;
  double y1=(edge1->node1->y+edge1->node2->y)/2.0;

  /* P2 (x2,y2) is the middle point of edge1 */
  double x4=(edge2->node1->x+edge2->node2->x)/2.0;
  double y4=(edge2->node1->y+edge2->node2->y)/2.0;
  
  double alpha=edge_angle_to(edge1,edge2,node,direction)*p->shape1;
  double beta=p->shape2;
  
  double i1x,i1y,i2x,i2y,x2,y2,x3,y3;
  
  if (direction == ANTICLOCKWISE) {
    /* I1 must stick out to the left of NP1 and I2 to the right of NP4 */
    i1x =  alpha*(node->y-y1)+x1;
    i1y = -alpha*(node->x-x1)+y1;
    i2x = -alpha*(node->y-y4)+x4;
    i2y =  alpha*(node->x-x4)+y4;
    x2 =  beta*(y1-i1y) + i1x;
    y2 = -beta*(x1-i1x) + i1y;
    x3 = -beta*(y4-i2y) + i2x;
    y3 =  beta*(x4-i2x) + i2y;
  }
  else {
    /* I1 must stick out to the left of NP1 and I2 to the right of NP4 */
    i1x = -alpha*(node->y-y1)+x1;
    i1y =  alpha*(node->x-x1)+y1;
    i2x =  alpha*(node->y-y4)+x4;
    i2y = -alpha*(node->x-x4)+y4;
    x2 = -beta*(y1-i1y) + i1x;
    y2 =  beta*(x1-i1x) + i1y;
    x3 =  beta*(y4-i2y) + i2x;
    y3 = -beta*(x4-i2x) + i2y;
  }

  spline_add_segment(s,x1,y1,x2,y2,x3,y3,x4,y4);
}

static int pattern_next_unfilled_couple(Pattern p, Edge *e, Direction *d)
{
  int i;
  for (i=0;i<p->ec->size;i++) {
    if (p->ec->array[i][CLOCKWISE]==0) {
      *e=p->graph->edges->elements[i];
      *d=CLOCKWISE;
      return 1;
    }
    else if (p->ec->array[i][ANTICLOCKWISE]==0) {
      *e=p->graph->edges->elements[i];
      *d=ANTICLOCKWISE;
      return 1;
    }
  }
  return 0;
}

static void pattern_make_curves(Pattern p)
{
  Edge current_edge, first_edge, next_edge;
  Node current_node, first_node;
  Direction current_direction, first_direction;
  Spline s;

  while (pattern_next_unfilled_couple(p, &first_edge, &first_direction)) {
    /* start a new loop */
    s=spline_new(random()%(p->ncolors-2)+2);
    array_add_element(p->splines, s);

    current_edge=first_edge;
    current_node=first_node=current_edge->node1;
    current_direction=first_direction;

    do {
      pattern_edge_couple_set(p, current_edge, current_direction, 1);
      next_edge = graph_next_edge_around(p->graph,current_node,current_edge,current_direction);

      /* add the spline segment to the spline */
      pattern_draw_spline_direction(p,s,current_node,
                                    current_edge,next_edge,current_direction);
      
      /* cross the edge */
      current_edge = next_edge;
      current_node = edge_other_node(next_edge, current_node);
      current_direction=1-current_direction;

    } while (current_node!=first_node || current_edge!=first_edge || current_direction!=first_direction);

    if (s->segments->nb_elements==2) /* spline is just one point: remove it */
      p->splines->elements[p->splines->nb_elements-1]=NULL;
      
  }
}

static void pattern_animate(struct state *st)
{
  Pattern p = st->pattern;
  double t = st->t;
  double t2;
  double x,y,x2,y2,x3,y3,x4,y4;
  int i,segment,unused;
  int ticks;
  double step=0.0001; /* TODO: set the step (or the delay) as a
                        * function of the spline length, so that
                        * drawing speed is constant
                        */
  Spline s;

  XSetLineAttributes(st->dpy,st->gc,st->params.curve_width,LineSolid,CapRound,JoinRound);
  XSetLineAttributes(st->dpy,st->shadow_gc,st->params.shadow_width,LineSolid,CapRound,JoinRound);

  for (ticks=0;ticks<100 && t<1;ticks++) {
    for (i=0;i<p->splines->nb_elements;i++) 
      if ((s=p->splines->elements[i])) { /* skip if one-point spline */
        spline_value_at(s, &x, &y, fmod(t,1.0),&segment);
        spline_value_at(s, &x2, &y2, fmod(t+step,1.0),&unused);
        
        /* look ahead for the shadow segment */
        t2=t+step;
        if (t2<=1.0) {
          spline_value_at(s, &x3, &y3, fmod(t2,1.0),&unused);
          while (t2+step<1.0 && (x3-x2)*(x3-x2)+(y3-y2)*(y3-y2) < st->params.shadow_width*st->params.shadow_width) {
            t2+=step;
            spline_value_at(s, &x3, &y3, fmod(t2,1.0),&unused);
          }
          
          spline_value_at(s, &x4, &y4, fmod(t2+step,1.0),&unused);
          
          /* draw shadow line */
          XDrawLine(st->dpy,st->window,st->shadow_gc, 
                    (int)rint(x3),(int)rint(y3), 
                    (int)rint(x4),(int)rint(y4));
        } 
        /* draw line segment */
        if (p->splines->nb_elements==1)
          XSetForeground(st->dpy, st->gc, st->colors[segment%(p->ncolors-3)+2].pixel);
        else
          XSetForeground(st->dpy, st->gc, st->colors[s->color].pixel);
        XDrawLine(st->dpy,st->window,st->gc,
                  (int)rint(x),(int)rint(y),
                  (int)rint(x2),(int)rint(y2));
      }
    t+=step;
  }
  st->t=t;

  if (t>=1) {
    st->reset=1;

    /* at the end we redraw back to remove shadow spillage */
    for (i=0;i<p->splines->nb_elements;i++) {
      if ((s=p->splines->elements[i])) {
	double offset=step;
	XSetForeground(st->dpy, st->gc, st->colors[s->color].pixel);
	spline_value_at(s, &x, &y, fmod(t,1.0),&unused);

	spline_value_at(s, &x2, &y2, fmod(t-offset,1.0),&unused);
      
	while ((x2-x)*(x2-x)+(y2-y)*(y2-y) < st->params.shadow_width*st->params.shadow_width) {
	  offset+=step;
	  spline_value_at(s, &x2, &y2, fmod(t-offset,1.0),&unused);
	}
      
	XDrawLine(st->dpy,st->window,st->gc, (int)rint(x),(int)rint(y), (int)rint(x2),(int)rint(y2));
      }
    }
  }
}

/*======================================================================*/

static const char *celtic_defaults[] = {
    ".background: black",
    ".foreground: #333333",
    "*fpsSolid:	true",
    "*ncolors: 20",
    "*delay: 10000",
    "*delay2: 5",
    "*showGraph: False",
#ifdef HAVE_MOBILE
    "*ignoreRotation: True",
#endif
    0
};

static XrmOptionDescRec celtic_options[] = {
    {"-background", ".background", XrmoptionSepArg, 0},
    {"-foreground", ".foreground", XrmoptionSepArg, 0},
    {"-ncolors", ".ncolors", XrmoptionSepArg, 0},
    {"-delay", ".delay", XrmoptionSepArg, 0},
    {"-delay2", ".delay2", XrmoptionSepArg, 0},
    {"-graph", ".showGraph", XrmoptionNoArg, "True"},
    {0, 0, 0, 0}
};

#if 0
static void params_to_s(FILE *f)
{
  switch (st->params.type) {
  case polar: fprintf(f,"type: polar\n"); 
    fprintf(f,"nb_orbits: %ld\n",st->params.nb_orbits);
    fprintf(f,"nb_nodes_per_orbit: %ld\n",st->params.nb_nodes_per_orbit);
    break;
  case tgrid: fprintf(f,"type: grid\n"); 
    fprintf(f,"edge_size: %ld\n",st->params.edge_size);
    break;
  case triangle: fprintf(f,"type: triangle\n"); 
    fprintf(f,"edge_size: %ld\n",st->params.edge_size);
    break;
  case kennicott: 
    fprintf(f,"type: kennicott\n"); 
    fprintf(f,"edge_size: %ld\n",st->params.edge_size);
    fprintf(f,"cluster_size: %ld\n",st->params.cluster_size);
    break;
  }

  fprintf(f,"curve width: %ld\n",st->params.curve_width);
  fprintf(f,"shadow width: %ld\n",st->params.shadow_width);
  fprintf(f,"shape1: %g\n",st->params.shape1);
  fprintf(f,"shape2: %g\n",st->params.shape2);
  fprintf(f,"margin: %ld\n",st->params.margin);
  fprintf(f,"angle: %g\n",st->params.angle);
  fprintf(f,"delay: %ld\n",st->params.delay);
}
#endif

#if 0
static void colormap_to_s(int ncolors, XColor *colors)
{
  int i;
  printf("-- colormap (%d colors):\n",st->ncolors);
  for (i=0;i<st->ncolors;i++)
    printf("%d: %d %d %d\n", i, st->colors[i].red, st->colors[i].green, st->colors[i].blue);
  printf("----\n");
}
#endif


static void *
celtic_init (Display *d_arg, Window w_arg)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;

  st->dpy=d_arg; st->window=w_arg;
  st->showGraph=get_boolean_resource (st->dpy, "showGraph", "Boolean");

  st->ncolors = get_integer_resource (st->dpy, "ncolors", "Integer");


  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  assert(st->colors = (XColor *) calloc (st->ncolors,sizeof(XColor)));

  if (get_boolean_resource(st->dpy, "mono", "Boolean"))
    {
    MONO:
      st->ncolors = 1;
      st->colors[0].pixel = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                           "foreground", "Foreground");
    }
  else
    {
#if 0
      make_random_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                            st->colors, &st->ncolors, True, True, 0, True);
#else
      make_smooth_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                            st->colors, &st->ncolors, True, 0, True);
#endif
      if (st->ncolors < 2)
        goto MONO;
      else {
	st->colors[0].pixel = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                             "foreground", "Foreground");
	st->colors[1].pixel = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                             "background", "Background");
      }
    }
  
  
  /* graphic context for curves */
  gcv.foreground = st->colors[0].pixel;
  gcv.background = st->colors[1].pixel;
  gcv.line_width = st->params.curve_width;
  gcv.cap_style=CapRound;
  st->gc = XCreateGC (st->dpy, st->window, GCForeground|GCBackground|GCLineWidth|GCCapStyle, &gcv);
  
  /* graphic context for graphs */
  gcv.foreground = st->colors[0].pixel;
  gcv.background = st->colors[1].pixel;
  st->gc_graph = XCreateGC (st->dpy, st->window, GCForeground|GCBackground, &gcv);
  
  /* graphic context for shadows */
  gcv.foreground = st->colors[1].pixel;
  gcv.line_width = st->params.shadow_width;
  gcv.cap_style=CapRound;
  st->shadow_gc = XCreateGC(st->dpy, st->window, GCForeground|GCLineWidth|GCCapStyle, &gcv);

  st->delay2 = 1000000 * get_integer_resource(st->dpy, "delay2", "Delay2");

  return st;
}

static unsigned long
celtic_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->eraser) {
    st->eraser = erase_window (st->dpy, st->window, st->eraser);
    return 10000;
  }

  if (st->reset || st->force_reset) {
    int delay = (st->force_reset ? 0 : st->delay2);
    st->reset = 0;
    st->force_reset = 0;
    st->t = 1;

    if (st->pattern != NULL) {
      pattern_del(st->pattern);
    }
    st->pattern = NULL;
    graph_del(st->graph);

    /* recolor each time */
    st->ncolors = get_integer_resource (st->dpy, "ncolors", "Integer");
    if (st->ncolors > 2)
      make_smooth_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                            st->colors, &st->ncolors, True, 0, True);

    st->eraser = erase_window (st->dpy, st->window, st->eraser);
    return (delay);
  }

  if (st->pattern == NULL) {
    st->params.curve_width=random()%5+4;
    st->params.shadow_width=st->params.curve_width+4;
    st->params.shape1=(15+random()%15)/10.0 -1.0;
    st->params.shape2=(15+random()%15)/10.0 -1.0;
    st->params.edge_size=10*(random()%5)+20;
    st->params.delay=get_integer_resource(st->dpy, "delay", "Delay");
    st->params.angle=random()%360*2*M_PI/360;
    st->params.margin=(random()%8)*100-600;

    switch (random()%4) {
    case 0:
      st->params.type=tgrid;
      st->params.shape1=(random()%1*2-1.0)*(random()%10+3)/10.0;
      st->params.shape2=(random()%1*2-1.0)*(random()%10+3)/10.0;
      st->params.edge_size=10*(random()%5)+50;
      break;
    case 1:
      st->params.type=kennicott;
      st->params.shape1=(random()%20)/10.0 -1.0;
      st->params.shape2=(random()%20)/10.0 -1.0;
      st->params.edge_size=10*(random()%3)+70;
      st->params.cluster_size=st->params.edge_size/(3.0+random()%10)-1;
      break;
    case 2:
      st->params.type=triangle;
      st->params.edge_size=10*(random()%5)+60;
      st->params.margin=(random()%10)*100-900;
      break;
    case 3:
      st->params.type=polar;
      st->params.nb_orbits=2+random()%10;
      st->params.nb_nodes_per_orbit=4+random()%10;
      break;
    }


/*     st->params.type= polar; */
/*   st->params.nb_orbits= 5; */
/*   st->params.nb_nodes_per_orbit= 19; */
/*   st->params.curve_width= 4; */
/*   st->params.shadow_width= 8; */
/*   st->params.shape1= 0.5; */
/*   st->params.shape2= 1.3; */
/*   st->params.margin= 30; */
/*   st->params.angle= 5.21853; */
/*   st->params.delay= 10000; */

    
/*     params_to_s(stdout); */
    
  /*=======================================================*/
    
    
    switch (st->params.type) {
    case tgrid:
      st->graph=make_grid_graph(st, st->params.margin,st->params.margin,
                        st->xgwa.width-2*st->params.margin, 
			st->xgwa.height-2*st->params.margin, 
			st->params.edge_size);
      break;
    case kennicott:
      st->graph=make_kennicott_graph(st, st->params.margin,st->params.margin,
			     st->xgwa.width-2*st->params.margin, 
			     st->xgwa.height-2*st->params.margin, 
			     st->params.edge_size,
			     st->params.cluster_size);
      break;
    case triangle:
      st->graph=make_triangle_graph(st, st->params.margin,st->params.margin,
                            st->xgwa.width-2*st->params.margin, 
                            st->xgwa.height-2*st->params.margin, 
                            st->params.edge_size);
      break;
    case polar:
      st->graph=make_polar_graph(st, st->params.margin,st->params.margin,
                         st->xgwa.width-2*st->params.margin, 
                         st->xgwa.height-2*st->params.margin, 
                         st->params.nb_nodes_per_orbit, 
                         st->params.nb_orbits);
      break;
    default:
      st->graph=make_grid_graph(st, st->params.margin,st->params.margin,
                        st->xgwa.width-2*st->params.margin, 
                        st->xgwa.height-2*st->params.margin, 
                        st->params.edge_size);
      break;
    }

    graph_rotate(st->graph,st->params.angle,st->xgwa.width/2,st->xgwa.height/2);
    
    if (st->showGraph)
      graph_draw(st, st->graph);
    
    st->pattern=pattern_new(st, st->graph, st->params.shape1, st->params.shape2);
    pattern_make_curves(st->pattern);
    st->t = 0.0;
  }

  pattern_animate(st);

  return st->params.delay;
}


static void
celtic_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
}

static Bool
celtic_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->force_reset = 1;
      return True;
    }
  return False;
}

static void
celtic_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


XSCREENSAVER_MODULE ("Celtic", celtic)
