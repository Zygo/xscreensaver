/* klondike, Copyright (c) 2024 Joshua Timmons <josh@developerx.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			
#define release_klondike 0

#include "xlockmore.h"
#include <ctype.h>
#include "gltrackball.h"
#include "klondike-game.h"
#include "ximage-loader.h"
#define I_HAVE_XPM
#include "../images/gen/back_png.h"
#include "../images/gen/CA_png.h"
#include "../images/gen/C2_png.h"
#include "../images/gen/C3_png.h"
#include "../images/gen/C4_png.h"
#include "../images/gen/C5_png.h"
#include "../images/gen/C6_png.h"
#include "../images/gen/C7_png.h"
#include "../images/gen/C8_png.h"
#include "../images/gen/C9_png.h"
#include "../images/gen/CT_png.h"
#include "../images/gen/CJ_png.h"
#include "../images/gen/CQ_png.h"
#include "../images/gen/CK_png.h"

#include "../images/gen/DA_png.h"
#include "../images/gen/D2_png.h"
#include "../images/gen/D3_png.h"
#include "../images/gen/D4_png.h"
#include "../images/gen/D5_png.h"
#include "../images/gen/D6_png.h"
#include "../images/gen/D7_png.h"
#include "../images/gen/D8_png.h"
#include "../images/gen/D9_png.h"
#include "../images/gen/DT_png.h"
#include "../images/gen/DJ_png.h"
#include "../images/gen/DQ_png.h"
#include "../images/gen/DK_png.h"

#include "../images/gen/SA_png.h"
#include "../images/gen/S2_png.h"
#include "../images/gen/S3_png.h"
#include "../images/gen/S4_png.h"
#include "../images/gen/S5_png.h"
#include "../images/gen/S6_png.h"
#include "../images/gen/S7_png.h"
#include "../images/gen/S8_png.h"
#include "../images/gen/S9_png.h"
#include "../images/gen/ST_png.h"
#include "../images/gen/SJ_png.h"
#include "../images/gen/SQ_png.h"
#include "../images/gen/SK_png.h"

#include "../images/gen/HA_png.h"
#include "../images/gen/H2_png.h"
#include "../images/gen/H3_png.h"
#include "../images/gen/H4_png.h"
#include "../images/gen/H5_png.h"
#include "../images/gen/H6_png.h"
#include "../images/gen/H7_png.h"
#include "../images/gen/H8_png.h"
#include "../images/gen/H9_png.h"
#include "../images/gen/HT_png.h"
#include "../images/gen/HJ_png.h"
#include "../images/gen/HQ_png.h"
#include "../images/gen/HK_png.h"

#ifdef USE_GL /* whole file */

#define DEF_CAMERA_SPEED "50"
#define DEF_SPEED "60"
#define DEF_DRAW_COUNT "3"
#define DEF_SLOPPY "True"

// global variables
static klondike_configuration *bps = NULL;

static GLuint animation_ticks;
static int draw_count;
static int camera_speed;
static Bool sloppy;

// options
static XrmOptionDescRec opts[] = {
 {"-speed", ".speed", XrmoptionSepArg, 0},
 {"-camera_speed", ".cameraSpeed", XrmoptionSepArg, 0},
 {"-sloppy", ".sloppy", XrmoptionNoArg, "True"},
 {"+sloppy", ".sloppy", XrmoptionNoArg, "False"},
 {"-draw", ".drawCount", XrmoptionSepArg, 0}};

// variables for the options
static argtype vars[] = {
 {&sloppy, "sloppy", "Sloppy", DEF_SLOPPY, t_Bool},
 {&animation_ticks, "speed", "Speed", DEF_SPEED, t_Int},
 {&draw_count, "drawCount", "DrawCount", DEF_DRAW_COUNT, t_Int},
 {&camera_speed, "cameraSpeed", "CameraSpeed", DEF_CAMERA_SPEED, t_Int}};

ENTRYPOINT ModeSpecOpt klondike_opts = {countof(opts), opts, countof(vars), vars, NULL};

// Local function prototypes
static void initialize_placeholders(klondike_configuration *bp, int width, int height);
static double ease_in_out_quart(double x);
static double ease_out_quart(double x);
static int compare_cards(const void *a, const void *b);
static void animate_board_to_deck(klondike_configuration *bp);
static void animate_initial_board(klondike_configuration *bp);

// Function to load a texture
// Function to load a texture
static GLuint load_texture(ModeInfo *mi, const char *name,
                        const unsigned char *buffer, unsigned length)
{
 GLuint texture_id;
 XImage *image = image_data_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
                                      buffer, length);

 glGenTextures(1, &texture_id);
 glBindTexture(GL_TEXTURE_2D, texture_id);

 // Set texture parameters
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

 glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
 /* glPixelStorei(GL_UNPACK_ROW_LENGTH, image->width); */
 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
              image->width, image->height, 0,
              GL_RGBA, GL_UNSIGNED_BYTE, image->data);
 check_gl_error("load texture");
 check_gl_error("mipmap");
 XDestroyImage(image);
 return texture_id;
}

/* Window management, etc
*/
ENTRYPOINT void
reshape_klondike(ModeInfo *mi, int width, int height)
{
 int y = 0;

 if (width > height * 5)
 { /* tiny window: show middle */
     height = width * 9 / 16;
     y = -height / 2;
 }

 glViewport(0, y, (GLint)width, (GLint)height);

 // Enable blending
 glEnable(GL_BLEND);

 // Set the blending function
 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

 // Enable multisampling
 glEnable(GL_MULTISAMPLE);

 glEnable(GL_DEPTH_TEST);
 glDepthFunc(GL_LESS);

 {
     GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                      ? (MI_WIDTH(mi) / (GLfloat)MI_HEIGHT(mi))
                      : 1);
     glScalef(s, s, s);
 }

 initialize_placeholders(&bps[MI_SCREEN(mi)], width, height);

 glClear(GL_COLOR_BUFFER_BIT);
}

// initialize the placeholders for the foundation and tableau
static void initialize_placeholders(klondike_configuration *bp, int width, int height)
{
 float xscale = width > height ? 0.53 * height / width : 0.53;

 if (width < height)
 {
     xscale *= width / 1280.0f;
 }

 for (int i = 0; i < 4; i++)
 {
     bp->foundation_placeholders[i].x = 0.15 + -0.4f + (0.075 + 0.15 * i * xscale / 0.3f) * bp->scale;
     bp->foundation_placeholders[i].y = 0.7f * bp->scale;
 }

 for (int i = 0; i < 7; i++)
 {
     bp->tableau_placeholders[i].x = 0.15 + -0.55f + 0.15 * i * xscale / 0.3f * bp->scale;
     bp->tableau_placeholders[i].y = 0.3f * bp->scale;
 }

 bp->waste_x = bp->tableau_placeholders[0].x;
 bp->waste_y = -0.65f * bp->scale;
 bp->deck_x = bp->tableau_placeholders[6].x;
 bp->deck_y = -0.65f * bp->scale;
}

// animate the initial board
static void animate_initial_board(klondike_configuration *bp)
{
 int n = 0;
 for (int i = 0; i < 7; i++)
 {
     for (int j = 0; j < 7; j++)
     {
         if (i < bp->game_state->tableau_size[j])
         {
             card_struct *card = &bp->game_state->tableau[j][i];
             card->start_frame = 10 + n * animation_ticks / 4;
             card->end_frame = card->start_frame + animation_ticks;
             card->start_x = bp->deck_x;
             card->start_y = bp->deck_y;
             card->dest_x = bp->tableau_placeholders[j].x;
             card->dest_y = bp->tableau_placeholders[j].y;
             card->angle = 0.0f;
             card->start_angle = 0.0f;
             card->is_face_up = i == bp->game_state->tableau_size[j] - 1;
             card->end_angle = card->is_face_up ? 180.0f : 0.0f;

             n++;
         }
     }
 }
}

ENTRYPOINT Bool
klondike_handle_event(ModeInfo *mi, XEvent *event)
{
  klondike_configuration *bp = &bps[MI_SCREEN(mi)];
  if (gltrackball_event_handler (event, bp->trackball,
                                MI_WIDTH (mi), MI_HEIGHT (mi),
                                &bp->button_down_p))
    return True;
 
  return False;
}

ENTRYPOINT void
init_klondike(ModeInfo *mi)
{
 klondike_configuration *bp;
 int wire = MI_IS_WIREFRAME(mi);

 MI_INIT(mi, bps);
 bp = &bps[MI_SCREEN(mi)];

 bp->glx_context = init_GL(mi);

 reshape_klondike(mi, MI_WIDTH(mi), MI_HEIGHT(mi));

 if (!wire)
 {
     GLfloat pos[4] = {0.0, 0.0, 1.0, 0.0};  // Changed light position to be in front
     GLfloat amb[4] = {0.8, 0.8, 0.8, 1.0};  // Increased ambient light
     GLfloat dif[4] = {1.0, 1.0, 1.0, 1.0};
     GLfloat spc[4] = {0.0, 0.0, 0.0, 1.0};  // Removed specular highlight

     // enable lighting, depth testing, normaliztion, culling, texture mapping, blending
     glEnable(GL_LIGHTING);
     glEnable(GL_LIGHT0);
     glEnable(GL_DEPTH_TEST);
     glEnable(GL_NORMALIZE);
     glEnable(GL_CULL_FACE);
     glEnable(GL_TEXTURE_2D);
     glEnable(GL_BLEND);
     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

     // Set up material properties
     GLfloat mat_ambient[] = {1.0, 1.0, 1.0, 1.0};
     GLfloat mat_diffuse[] = {1.0, 1.0, 1.0, 1.0};
     GLfloat mat_specular[] = {0.0, 0.0, 0.0, 1.0};
     GLfloat mat_shininess[] = {0.0};
     
     // set the material properties
     glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
     glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
     glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
     glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

     // set the light position
     glLightfv(GL_LIGHT0, GL_POSITION, pos);
     glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
     glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
     glLightfv(GL_LIGHT0, GL_SPECULAR, spc);

     // Load textures
     unsigned int n = 0;      
     
     bp->fronts[n++] = load_texture(mi, "DA", DA_png, sizeof(DA_png));
     bp->fronts[n++] = load_texture(mi, "D2", D2_png, sizeof(D2_png));
     bp->fronts[n++] = load_texture(mi, "D3", D3_png, sizeof(D3_png));
     bp->fronts[n++] = load_texture(mi, "D4", D4_png, sizeof(D4_png));
     bp->fronts[n++] = load_texture(mi, "D5", D5_png, sizeof(D5_png));
     bp->fronts[n++] = load_texture(mi, "D6", D6_png, sizeof(D6_png));
     bp->fronts[n++] = load_texture(mi, "D7", D7_png, sizeof(D7_png));
     bp->fronts[n++] = load_texture(mi, "D8", D8_png, sizeof(D8_png));
     bp->fronts[n++] = load_texture(mi, "D9", D9_png, sizeof(D9_png));
     bp->fronts[n++] = load_texture(mi, "DT", DT_png, sizeof(DT_png));
     bp->fronts[n++] = load_texture(mi, "DJ", DJ_png, sizeof(DJ_png));
     bp->fronts[n++] = load_texture(mi, "DQ", DQ_png, sizeof(DQ_png));
     bp->fronts[n++] = load_texture(mi, "DK", DK_png, sizeof(DK_png));
     
     bp->fronts[n++] = load_texture(mi, "CA", CA_png, sizeof(CA_png));
     bp->fronts[n++] = load_texture(mi, "C2", C2_png, sizeof(C2_png));
     bp->fronts[n++] = load_texture(mi, "C3", C3_png, sizeof(C3_png));
     bp->fronts[n++] = load_texture(mi, "C4", C4_png, sizeof(C4_png));
     bp->fronts[n++] = load_texture(mi, "C5", C5_png, sizeof(C5_png));
     bp->fronts[n++] = load_texture(mi, "C6", C6_png, sizeof(C6_png));
     bp->fronts[n++] = load_texture(mi, "C7", C7_png, sizeof(C7_png));
     bp->fronts[n++] = load_texture(mi, "C8", C8_png, sizeof(C8_png));
     bp->fronts[n++] = load_texture(mi, "C9", C9_png, sizeof(C9_png));
     bp->fronts[n++] = load_texture(mi, "CT", CT_png, sizeof(CT_png));
     bp->fronts[n++] = load_texture(mi, "CJ", CJ_png, sizeof(CJ_png));
     bp->fronts[n++] = load_texture(mi, "CQ", CQ_png, sizeof(CQ_png));
     bp->fronts[n++] = load_texture(mi, "CK", CK_png, sizeof(CK_png));
         
     bp->fronts[n++] = load_texture(mi, "HA", HA_png, sizeof(HA_png));
     bp->fronts[n++] = load_texture(mi, "H2", H2_png, sizeof(H2_png));
     bp->fronts[n++] = load_texture(mi, "H3", H3_png, sizeof(H3_png));
     bp->fronts[n++] = load_texture(mi, "H4", H4_png, sizeof(H4_png));
     bp->fronts[n++] = load_texture(mi, "H5", H5_png, sizeof(H5_png));
     bp->fronts[n++] = load_texture(mi, "H6", H6_png, sizeof(H6_png));
     bp->fronts[n++] = load_texture(mi, "H7", H7_png, sizeof(H7_png));
     bp->fronts[n++] = load_texture(mi, "H8", H8_png, sizeof(H8_png));
     bp->fronts[n++] = load_texture(mi, "H9", H9_png, sizeof(H9_png));
     bp->fronts[n++] = load_texture(mi, "HT", HT_png, sizeof(HT_png));
     bp->fronts[n++] = load_texture(mi, "HJ", HJ_png, sizeof(HJ_png));
     bp->fronts[n++] = load_texture(mi, "HQ", HQ_png, sizeof(HQ_png));
     bp->fronts[n++] = load_texture(mi, "HK", HK_png, sizeof(HK_png));

     bp->fronts[n++] = load_texture(mi, "SA", SA_png, sizeof(SA_png));
     bp->fronts[n++] = load_texture(mi, "S2", S2_png, sizeof(S2_png));
     bp->fronts[n++] = load_texture(mi, "S3", S3_png, sizeof(S3_png));
     bp->fronts[n++] = load_texture(mi, "S4", S4_png, sizeof(S4_png));
     bp->fronts[n++] = load_texture(mi, "S5", S5_png, sizeof(S5_png));
     bp->fronts[n++] = load_texture(mi, "S6", S6_png, sizeof(S6_png));
     bp->fronts[n++] = load_texture(mi, "S7", S7_png, sizeof(S7_png));
     bp->fronts[n++] = load_texture(mi, "S8", S8_png, sizeof(S8_png));
     bp->fronts[n++] = load_texture(mi, "S9", S9_png, sizeof(S9_png));
     bp->fronts[n++] = load_texture(mi, "ST", ST_png, sizeof(ST_png));
     bp->fronts[n++] = load_texture(mi, "SJ", SJ_png, sizeof(SJ_png));
     bp->fronts[n++] = load_texture(mi, "SQ", SQ_png, sizeof(SQ_png));
     bp->fronts[n++] = load_texture(mi, "SK", SK_png, sizeof(SK_png));

     bp->back = load_texture(mi, "back", back_png, sizeof(back_png));                
 }

 bp->scale = 1.1f;

 initialize_placeholders(bp, MI_WIDTH(mi), MI_HEIGHT(mi));

 bp->trackball = gltrackball_init (True);

 // initialize the game state
 bp->game_state = (game_state_struct *)malloc(sizeof(game_state_struct));
 bp->tick = 0;
 bp->universe_tick = 0;

 bp->camera_phase = (random() / (float)RAND_MAX) * 0.2 * M_PI;

 bp->redeal = 0;
 bp->final_animation = 0;

 // set the draw count to 3 if it is not 1 or 3
 if (draw_count != 1 && draw_count != 3)
 {
     draw_count = 3;
 }

 bp->animation_ticks = animation_ticks;
 bp->draw_count = draw_count;
 bp->camera_speed = camera_speed;
 bp->sloppy = sloppy;
}

// ease in out quartic
static double ease_in_out_quart(double x)
{
 if (x < 0.5)
 {
     return 8 * x * x * x * x;
 }
 else
 {
     return 1 - pow(-2 * x + 2, 4) / 2;
 }
}

// ease out quartic
static double ease_out_quart(double x)
{
 return 1 - pow(1 - x, 4);
}

// sort cards by z position then by end_frame
static int compare_cards(const void *a, const void *b)
{
 card_struct **ca = (card_struct **)a;
 card_struct **cb = (card_struct **)b;

 // sort by z position then by end_frame
 if ((*ca)->z != (*cb)->z)
 {
     return ((*ca)->z > (*cb)->z) ? 1 : -1;
 }

 // sort by end_frame
 return (*ca)->end_frame - (*cb)->end_frame;
}

// collect the cards from the board back to the deck to be redealt
static void animate_board_to_deck(klondike_configuration *bp)
{
 int n = 0;
 for (int i = 0; i < 7; i++)
 {
     for (int j = 0; j < bp->game_state->tableau_size[i]; j++)
     {
         card_struct *card = &bp->game_state->tableau[i][j];
         card->start_frame = bp->tick + n * animation_ticks / 12;
         card->end_frame = card->start_frame + animation_ticks;
         card->start_x = card->x;
         card->start_y = card->y;
         card->dest_x = bp->deck_x;
         card->dest_y = bp->deck_y;
         card->start_angle = card->angle;
         card->end_angle = 360.0f;
         card->is_face_up = 0;
         n++;
     }
 }

 for (int i = 0; i < 4; i++)
 {
     for (int j = 0; j < bp->game_state->foundation_size[i]; j++)
     {
         card_struct *card = &bp->game_state->foundation[i][j];
         card->start_frame = bp->tick + n * animation_ticks / 12;
         card->end_frame = card->start_frame + animation_ticks;
         card->start_x = card->x;
         card->start_y = card->y;
         card->dest_x = bp->deck_x;
         card->dest_y = bp->deck_y;
         card->start_angle = card->angle;
         card->end_angle = 360.0f;
         card->is_face_up = 0;
         n++;
     }
 }

 for (int i = 0; i < bp->game_state->waste_size; i++)
 {
     card_struct *card = &bp->game_state->waste[i];
     card->start_frame = bp->tick + n * animation_ticks / 12;
     card->end_frame = card->start_frame + animation_ticks;
     card->start_x = card->x;
     card->start_y = card->y;
     card->dest_x = bp->deck_x;
     card->dest_y = bp->deck_y;
     card->start_angle = card->angle;
     card->end_angle = 360.0f;
     card->is_face_up = 0;
     n++;
 }

 bp->final_animation = bp->tick + n * animation_ticks / 12 + animation_ticks;
}

ENTRYPOINT void
draw_klondike(ModeInfo *mi)
{
 klondike_configuration *bp = &bps[MI_SCREEN(mi)];
 Display *dpy = MI_DISPLAY(mi);
 Window window = MI_WINDOW(mi);
 card_struct *renderCards[52];

 if (!bp->glx_context)
 {
     fprintf(stderr, "%s: no graphics context\n", progname);
     return;
 }

 glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

 glShadeModel(GL_SMOOTH);
 glEnable(GL_DEPTH_TEST);
 glEnable(GL_NORMALIZE);
 glEnable(GL_CULL_FACE);
 glEnable(GL_TEXTURE_2D);
 glEnable(GL_BLEND);
 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 glEnable(GL_LIGHTING);
 glEnable(GL_LIGHT0);

 glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

 int width = MI_WIDTH(mi);
 int height = MI_HEIGHT(mi);

 glScalef(1.1, 1.1, 1.1);

 float offset_x = 0.0f;
 float offset_y = 0.0f;
 float scale = 1.0f;

 if (bp->tick == 0)
 {
     initialize_placeholders(bp, MI_WIDTH(mi), MI_HEIGHT(mi));

     klondike_initialize_deck(bp);
     klondike_shuffle_deck(bp->game_state->deck);

     klondike_deal_cards(bp);
     animate_initial_board(bp);
 }

 bp->tick++;
 bp->universe_tick++;
 int animatedCardCount = 0;

 int last_animation = 0;

 // render the tableaus
 for (int i = 0; i < 7; i++)
 {
     for (int j = 0; j < bp->game_state->tableau_size[i]; j++)
     {
         card_struct *card = &(bp->game_state->tableau[i][j]);
         renderCards[animatedCardCount++] = card;
     }
 }

 // render the foundations
 for (int i = 0; i < 4; i++)
 {
     for (int j = 0; j < bp->game_state->foundation_size[i]; j++)
     {
         card_struct *card = &(bp->game_state->foundation[i][j]);
         renderCards[animatedCardCount++] = card;

         card->z = j;
     }
 }

 // render the waste pile
 for (int j = 0; j < bp->game_state->waste_size; j++)
 {
     card_struct *card = &(bp->game_state->waste[j]);
     card->z = j;
     renderCards[animatedCardCount++] = card;
 }

 // render the deck
 int ds = klondike_deck_size(bp->game_state);
 for (int j = ds - 1; j >= 0; j--)
 {
     card_struct *card = &(bp->game_state->deck[j]);
     // Only set z position, don't modify x,y here
     card->z = ds - 1 - j;
     renderCards[animatedCardCount + j] = card;
 }

 animatedCardCount += ds;

 for (int i = 0; i < animatedCardCount; i++)
 {
     card_struct *card = renderCards[i];

     card->z = i / 10.0f;

     if (bp->tick >= card->start_frame && bp->tick < card->end_frame)
     {
         float n = ((float)bp->tick - (float)card->start_frame) / (card->end_frame - card->start_frame);
         float eased2 = ease_in_out_quart(n);
         float eased_card_z = card->start_z * (1.0f - eased2);
         card->z += eased_card_z;
         card->z += 8 * sin(n * M_PI);
     }
     else {
         card->start_z = 0;
     }
 
 }



 // qsort the rendercards by animation order
 qsort(renderCards, animatedCardCount, sizeof(card_struct *), compare_cards);

 // camera
 
 float speed_factor = camera_speed / 100.0f;
 float camera_theta = M_PI / 2 + (sin((bp->universe_tick + bp->camera_phase) * 0.0065 * speed_factor) * 0.225);
 float camera_phi =   -0.55 + (sin((bp->universe_tick + bp->camera_phase) * 0.008   * speed_factor) * 0.25);
 float camera_d = 3.5 + (sin((bp->universe_tick + bp->camera_phase) * 0.013 * speed_factor));
 float camera_x = camera_d * cos(camera_theta) * sin(camera_phi);
 float camera_y = camera_d * sin(camera_theta) * sin(camera_phi);
 float camera_z = camera_d * cos(camera_phi);
 
 // Use immediate mode rendering instead of shaders
 glMatrixMode(GL_MODELVIEW);
 glLoadIdentity();
 gluPerspective (30.0, 1.0, 1.0, 200);
 gluLookAt( camera_x, camera_y, camera_z,   // Camera position (eye point)
     0.1, -0.0, 0,     // Look-at point (center)
     0, 0, 1);    // Up vector

 glRotatef (current_device_rotation(), 0, 0, 1);
 gltrackball_rotate (bp->trackball);

 for (int i = 0; i < animatedCardCount; i++)
 {
     glPushMatrix();

     card_struct *card = renderCards[i];

     float tx, ty, tz;

     if (card->end_frame > last_animation)
     {
         last_animation = card->end_frame;
     }

     // card->z = i / 10.0f;

     if (bp->tick >= card->start_frame && bp->tick < card->end_frame)
     {
         float n = ((float)bp->tick - (float)card->start_frame) / (card->end_frame - card->start_frame);
         float eased = ease_out_quart(n);
         float eased2 = ease_in_out_quart(n);

         if (card->dest_x != card->start_x)
         {
             card->x = card->start_x + eased * (card->dest_x - card->start_x);
         }
         else
         {
             card->x = card->start_x;
         }

         if (card->dest_y != card->start_y)
         {
             card->y = card->start_y + eased * (card->dest_y - card->start_y);
         }
         else
         {
             card->y = card->start_y;
         }

         // card->z += 25 * sin(n * M_PI);

         if (card->end_angle != card->start_angle)
         {
             card->angle = card->start_angle + eased2 * (card->end_angle - card->start_angle);
         }
         else
         {
             card->angle = card->start_angle;
         }
     }
     // Make sure card positions are finalized after animation completes
     else if (bp->tick >= card->end_frame)
     {
         card->x = card->dest_x;
         card->y = card->dest_y;
         card->angle = card->end_angle;
     }
     
     float s = 1.0f;
     GLuint current_texture = -1;
     
     if (card->angle > 90.0f && card->angle < 270.0f && (bp->tick > card->start_frame || bp->tick < card->end_frame))
     {
         int n = card->suit * 13 + card->rank - 1;
         current_texture = bp->fronts[n];
         s = -1.0f;
     }
     else
     {
         current_texture = bp->back;
     }

     tx = card->x;
     ty = card->y;
     tz = card->z * 0.025f;

     float translateX = tx + offset_x;
     float translateY = ty + offset_y;
     float translateZ = tz;

     float horiz_card_aspect_scale = 0.53 * height / width;
     float vert_card_aspect_scale = 0.53;

     if (width < height)
     {
         horiz_card_aspect_scale *= width / 1280.0f;
         vert_card_aspect_scale *= width / 1280.0f;
     }

     float scaleX = s * horiz_card_aspect_scale * 0.45f * scale;
     float scaleY = vert_card_aspect_scale * 0.45f * scale;
     float scaleZ = horiz_card_aspect_scale * 0.45f * scale;

     glTranslatef(translateX, translateY, translateZ);
     glRotatef(card->angle, 0.0f, 1.0f, 0.0f);
     glScalef(scaleX, scaleY, scaleZ);

     // Bind the appropriate texture
     glBindTexture(GL_TEXTURE_2D, current_texture);

     // Draw the card quad with proper texture coordinates
     glBegin(GL_QUADS);
     // Front face
     glNormal3f(0.0f, 0.0f, 1.0f);
     glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.75f, 0.0f);
     glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, -0.75f, 0.0f);
     glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, 0.75f, 0.0f);
     glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, 0.75f, 0.0f);
     glEnd();

     glPopMatrix();
 }

 if (mi->fps_p) do_fps (mi);
 
 glFinish();
 glXSwapBuffers(dpy, window);

 if (bp->tick >= last_animation)
 {
     game_state_struct *n = NULL;
     if (bp->redeal)
     {
         n = (game_state_struct *)malloc(sizeof(game_state_struct));
         bp->tick = 0;
         bp->final_animation = 0;
         bp->redeal = 0;
     }
     else if (bp->final_animation == 0)
     {
         n = klondike_next_move(bp);
     }

     if (bp->final_animation != 0 && bp->tick >= bp->final_animation)
     {
         // resetBoard(game_state);
         bp->redeal = 1;
     }
     else if (bp->final_animation == 0 && n == NULL)
     {
         animate_board_to_deck(bp);

         // Check for win
# if 0 // unused            
         int won_game = 0;
         for (int i = 0; i < 4; i++)
         {
             won_game &= (bp->game_state->foundation_size[i] == 13);
         }
# endif                
     }

     if (n != NULL)
     {
         klondike_free_game_state(bp->game_state);
         bp->game_state = n;
     }
 }
}

ENTRYPOINT void
free_klondike(ModeInfo *mi)
{
 klondike_configuration *bp = &bps[MI_SCREEN(mi)];
 if (!bp->glx_context)
     return;
 glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

 // Free the textures
 for (int i = 0; i < 52; i++)
 {
     glDeleteTextures(1, &bp->fronts[i]);
 }

 glDeleteTextures(1, &bp->back);

 klondike_free_game_state(bp->game_state);
}

XSCREENSAVER_MODULE_2("Klondike", klondike, klondike)

#endif /* USE_GL */
