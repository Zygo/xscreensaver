/*
 * endgame.c
 * plays through a chess game ending.  enjoy.
 *
 * version 1.0 - June 6, 2002
 *
 * Copyright (C) 2002-2008 Blair Tennessy (tennessb@unbc.ca)
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/* TODO: correct castling moves */
#ifdef STANDALONE
#define DEFAULTS                                                               \
    "*delay:      20000 \n"                                                    \
    "*showFPS:    False \n"                                                    \
    "*wireframe:  False \n"                                                    \
    "*titleFont:  sans-serif 18\n"                                             \
    "*titleFont2: sans-serif 12\n"                                             \
    "*titleFont3: sans-serif 8\n"

#define release_chess 0
#include "xlockmore.h"

#else
#include "xlock.h"
#endif

#ifdef USE_GL

#define BOARDSIZE 8
#define WHITES 5

#include "gltrackball.h"
#include "chessmodels.h"
#include "chessgames.h"
#include "texfont.h"
#include "easing.h"

#define DEF_ROTATE "True"
#define DEF_REFLECTIONS "True"
#define DEF_SHADOWS "True"
#define DEF_SMOOTH "True"
#define DEF_CLASSIC "False"
#define DEF_LABELS "True"

static XrmOptionDescRec opts[] = {
    {"+rotate", ".chess.rotate", XrmoptionNoArg, "false"},
    {"-rotate", ".chess.rotate", XrmoptionNoArg, "true"},
    {"+reflections", ".chess.reflections", XrmoptionNoArg, "false"},
    {"-reflections", ".chess.reflections", XrmoptionNoArg, "true"},
    {"+shadows", ".chess.shadows", XrmoptionNoArg, "false"},
    {"-shadows", ".chess.shadows", XrmoptionNoArg, "true"},
    {"+smooth", ".chess.smooth", XrmoptionNoArg, "false"},
    {"-smooth", ".chess.smooth", XrmoptionNoArg, "true"},
    {"+classic", ".chess.classic", XrmoptionNoArg, "false"},
    {"-classic", ".chess.classic", XrmoptionNoArg, "true"},
    {"+labels", ".chess.labels", XrmoptionNoArg, "false"},
    {"-labels", ".chess.labels", XrmoptionNoArg, "true"},
};

static int rotate, reflections, smooth, shadows, classic, labels;

static argtype vars[] = {
    {&rotate, "rotate", "Rotate", DEF_ROTATE, t_Bool},
    {&reflections, "reflections", "Reflections", DEF_REFLECTIONS, t_Bool},
    {&shadows, "shadows", "Shadows", DEF_SHADOWS, t_Bool},
    {&smooth, "smooth", "Smooth", DEF_SMOOTH, t_Bool},
    {&classic, "classic", "Classic", DEF_CLASSIC, t_Bool},
    {&labels, "labels", "Labels", DEF_LABELS, t_Bool},
};

ENTRYPOINT ModeSpecOpt chess_opts = {countof(opts), opts, countof(vars), vars,
                                     NULL};

#ifdef USE_MODULES
ModStruct chess_description = {"chess",
                               "init_chess",
                               "draw_chess",
                               NULL,
                               "draw_chess",
                               "init_chess",
                               NULL,
                               &chess_opts,
                               1000,
                               1,
                               2,
                               1,
                               4,
                               1.0,
                               "",
                               "Chess",
                               0,
                               NULL};

#endif

#define checkImageWidth 16
#define checkImageHeight 16
#define CONCURRENT_MOVES 2
#define TICKS_BET_MOVES 50
#define FADE_FACTOR 2
#define END_FACTOR 5
#define NUM_STEPS 80

const double num_steps_f = (double)NUM_STEPS;

enum app_stage {
    CHOSE_GAME = 0,
    FADE_IN,
    DO_MOVE,
    WAIT_FOR_NEXT_MOVE,
    WAIT_FOR_NEXT_GAME,
    FADE_OUT
};

enum color { WHITE = 0, BLACK };

typedef struct {
    int active;        /* Is a piece moving */
    int mpiece;        /* The moving piece */
    int tpiece;        /* Piece taken by this move, if any */
    int promotion;     /* Promotion piece */
    int from[2];       /* Origin case */
    int to[2];         /* destination case */
    int en_passant[2]; /* Is this an en passant capture */
    enum color color;  /* White or black move */
    double dx;         /* Delta x */
    double dz;         /* Delta z */
    int castling;      /* Are we castling */
} Chessmovestate;

typedef struct {
    GLXContext *glx_context;
    Window window;
    trackball_state *trackball;
    Bool button_down_p;

    ChessGame game;
    char game_desc[DESC_STR_LEN + SAN_STR_LEN + 1];
    int move_desc_index;
    int oldwhite;

    /** definition of white/black (orange/gray) colors */
    GLfloat colors[2][3];

    GLubyte checkImage[checkImageWidth][checkImageHeight][3];
    GLuint piecetexture, boardtexture;

    int board[BOARDSIZE][BOARDSIZE];
    Chessmovestate moves[CONCURRENT_MOVES]; /* If castling, two pieces can move
                                               at the same time */
    int steps;
    enum app_stage stage;
    int mc, ticks, wire, abort;
    double theta;

    GLfloat position[4];
    GLfloat position2[4];

    GLfloat mod;

    GLfloat ground[4];

    int cur_game_idx;

    texture_font_data *font1_data, *font2_data, *font3_data;
    int poly_counts[PIECES]; /* polygon count of each type of piece */
} Chesscreen;

static Chesscreen *qs = NULL;

static const GLfloat MaterialShadow[] = {0.0, 0.0, 0.0, 0.3};

/* i prefer silvertip */
static const GLfloat whites[WHITES][3] = {
    {1.0, 0.55, 0.1}, {0.8, 0.52, 0.8},   {0.43, 0.54, 0.76},
    {0.7, 0.7, 0.7},  {0.35, 0.60, 0.35},
};

static void build_colors(Chesscreen *cs) {

    /* find new white */
    int newwhite = cs->oldwhite;
    while (newwhite == cs->oldwhite)
        newwhite = random() % WHITES;
    cs->oldwhite = newwhite;

    cs->colors[0][0] = whites[cs->oldwhite][0];
    cs->colors[0][1] = whites[cs->oldwhite][1];
    cs->colors[0][2] = whites[cs->oldwhite][2];
}

/* build piece texture */
static void make_piece_texture(Chesscreen *cs) {
    int i, j, c;

    for (i = 0; i < checkImageWidth; i++) {
        for (j = 0; j < checkImageHeight; j++) {
            c = ((j % 2) == 0 || i % 2 == 0) ? 240 : 180 + random() % 16;
            cs->checkImage[i][j][0] = (GLubyte)c;
            cs->checkImage[i][j][1] = (GLubyte)c;
            cs->checkImage[i][j][2] = (GLubyte)c;
        }
    }

    glGenTextures(1, &cs->piecetexture);
    glBindTexture(GL_TEXTURE_2D, cs->piecetexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth, checkImageHeight, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, &cs->checkImage[0][0]);
}

/* build board texture (uniform noise in [180,180+50]) */
static void make_board_texture(Chesscreen *cs) {
    int i, j, c;

    for (i = 0; i < checkImageWidth; i++) {
        for (j = 0; j < checkImageHeight; j++) {
            c = 180 + random() % 51;
            cs->checkImage[i][j][0] = (GLubyte)c;
            cs->checkImage[i][j][1] = (GLubyte)c;
            cs->checkImage[i][j][2] = (GLubyte)c;
        }
    }

    glGenTextures(1, &cs->boardtexture);
    glBindTexture(GL_TEXTURE_2D, cs->boardtexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth, checkImageHeight, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, &cs->checkImage[0][0]);
}

/** handle X event (trackball) */
ENTRYPOINT Bool chess_handle_event(ModeInfo *mi, XEvent *event) {
    Chesscreen *cs = &qs[MI_SCREEN(mi)];

    if (gltrackball_event_handler(event, cs->trackball, MI_WIDTH(mi),
                                  MI_HEIGHT(mi), &cs->button_down_p))
        return True;
    else if (screenhack_event_helper(MI_DISPLAY(mi), MI_WINDOW(mi), event)) {
        cs->abort = 1;
        return True;
    }

    return False;
}

static const GLfloat diffuse2[] = {1.0, 1.0, 1.0, 1.0};
/*static const GLfloat ambient2[] = {0.7, 0.7, 0.7, 1.0};*/
static const GLfloat shininess[] = {60.0};
static const GLfloat specular[] = {0.4, 0.4, 0.4, 1.0};

/* configure lighting */
static void setup_lights(Chesscreen *cs) {
    glEnable(GL_LIGHTING);
    glLightfv(GL_LIGHT0, GL_POSITION, cs->position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse2);
    glEnable(GL_LIGHT0);

    /*   glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient2); */

    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);

    glLightfv(GL_LIGHT1, GL_SPECULAR, diffuse2);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse2);
    glEnable(GL_LIGHT1);
}

static enum color get_piece_color(int piece) {
    if (piece < BKING)
        return WHITE;
    return BLACK;
}

/* draw pieces */
static void drawPieces(ModeInfo *mi, Chesscreen *cs) {
    int i, j;

    for (i = 0; i < BOARDSIZE; ++i) {
        for (j = 0; j < BOARDSIZE; ++j) {
            if (cs->board[i][j]) {
                int c = cs->board[i][j] / PIECES;
                int piece = cs->board[i][j] % PIECES;
                int side = (cs->board[i][j] == piece ? 1 : -1);
                glColor3fv(cs->colors[c]);
                if (cs->board[i][j] == KNIGHT)
                    glRotatef(180.0, 0.0, 1.0, 0.0);
                else if (piece == BISHOP)
                    glRotatef(90.0 * side, 0.0, 1.0, 0.0);
                glCallList(piece);
                if (cs->board[i][j] == KNIGHT)
                    glRotatef(180.0, 0.0, 1.0, 0.0);
                else if (piece == BISHOP)
                    glRotatef(-90.0 * side, 0.0, 1.0, 0.0);
                mi->polygon_count += cs->poly_counts[cs->board[i][j] % PIECES];
            }
            glTranslatef(1.0, 0.0, 0.0);
        }
        glTranslatef(-1.0 * BOARDSIZE, 0.0, 1.0);
    }
    glTranslatef(0.0, 0.0, -1.0 * BOARDSIZE);
}

/* draw pieces */
static void drawPiecesShadow(ModeInfo *mi, Chesscreen *cs) {
    int i, j;

    for (i = 0; i < BOARDSIZE; ++i) {
        for (j = 0; j < BOARDSIZE; ++j) {
            if (cs->board[i][j]) {
                glColor4f(0.0, 0.0, 0.0, 0.4);
                glCallList(cs->board[i][j] % PIECES);
                mi->polygon_count += cs->poly_counts[cs->board[i][j] % PIECES];
            }
            glTranslatef(1.0, 0.0, 0.0);
        }
        glTranslatef(-1.0 * BOARDSIZE, 0.0, 1.0);
    }
    glTranslatef(0.0, 0.0, -1.0 * BOARDSIZE);
}

/** Helpers */
static int char_to_piece(char piece_char) {
    switch (piece_char) {
    case 'p':
        return BPAWN;
    case 'P':
        return PAWN;
    case 'q':
        return BQUEEN;
    case 'Q':
        return QUEEN;
    case 'k':
        return BKING;
    case 'K':
        return KING;
    case 'b':
        return BBISHOP;
    case 'B':
        return BISHOP;
    case 'n':
        return BKNIGHT;
    case 'N':
        return KNIGHT;
    case 'r':
        return BROOK;
    case 'R':
        return ROOK;
    default:
        return NONE;
    }
    return NONE;
}

static int get_digit(char c) {
    switch (c) {
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    default:
        return 0;
    }
    return 0;
}

static int char_to_rowcol(char c) {
    switch (c) {
    case 'a':
    case '8':
        return 0;
    case 'b':
    case '7':
        return 1;
    case 'c':
    case '6':
        return 2;
    case 'd':
    case '5':
        return 3;
    case 'e':
    case '4':
        return 4;
    case 'f':
    case '3':
        return 5;
    case 'g':
    case '2':
        return 6;
    case 'h':
    case '1':
        return 7;
    default:
        return -1;
    }
    return -1;
}

static void apply_fen(Chesscreen *cs) {
    char *row, *saveptr;
    char *fen = cs->game.fen;

    row = strtok_r(fen, "/", &saveptr);
    for (int row_num = 0; row_num < 8; row_num++) {
        int col_num = 0;
        for (int char_num = 0; char_num < strlen(row); char_num++) {
            char c = row[char_num];
            int d = get_digit(c);
            for (int i = 0; i < d; i++) {
                cs->board[row_num][col_num++] = NONE;
            }
            int p = char_to_piece(c);
            if (p != NONE) {
                cs->board[row_num][col_num++] = p;
            }
        }
        row = strtok_r(NULL, "/", &saveptr);
    }
}

static void coord_to_case(char *uci, int chess_case[2]) {
    chess_case[0] = char_to_rowcol(uci[1]);
    chess_case[1] = char_to_rowcol(uci[0]);
}

static int color_piece(int piece, enum color color) {
    if (piece == NONE)
        return piece;
    if (color == WHITE)
        return piece % PIECES;
    return (piece % PIECES) + PIECES;
}

static void setup_move(Chesscreen *cs, char *uci, int move_index) {
    Chessmovestate *movestate = &(cs->moves[move_index]);
    coord_to_case(uci, movestate->from);
    coord_to_case(uci + 2, movestate->to);
    movestate->mpiece = cs->board[movestate->from[0]][movestate->from[1]];
    movestate->color = get_piece_color(movestate->mpiece);
    if (strlen(uci) == 5)
        movestate->promotion =
            color_piece(char_to_piece(uci[4]), movestate->color);
    movestate->dz = (movestate->to[0] - movestate->from[0]) / num_steps_f;
    movestate->dx = (movestate->to[1] - movestate->from[1]) / num_steps_f;
    cs->board[movestate->from[0]][movestate->from[1]] =
        NONE; /* Remove moving piece from board */

    movestate->tpiece = cs->board[movestate->to[0]][movestate->to[1]];
    /* Capture ? */
    if (movestate->tpiece != NONE) {
        cs->board[movestate->to[0]][movestate->to[1]] =
            NONE; /* Remove captured piece from board */
        movestate->en_passant[0] = movestate->en_passant[1] =
            -1; /* Not en passant */
    } else {    /* Destination case is empty. Is this an en passant capture ? */
        /* White en passant ? */
        if ((movestate->mpiece == PAWN) && (movestate->from[0] == 3) &&
            (movestate->from[1] != movestate->to[1])) {
            movestate->tpiece = BPAWN;
            movestate->en_passant[0] = 3;
            movestate->en_passant[1] = movestate->to[1];
            cs->board[movestate->en_passant[0]][movestate->en_passant[1]] =
                NONE; /* Remove captured piece */
        }
        /* Black en passant ? */
        else if ((movestate->mpiece == BPAWN) && (movestate->from[0] == 4) &&
                 (movestate->from[1] != movestate->to[1])) {
            movestate->tpiece = PAWN;
            movestate->en_passant[0] = 4;
            movestate->en_passant[1] = movestate->to[1];
            cs->board[movestate->en_passant[0]][movestate->en_passant[1]] =
                NONE; /* Remove captured piece */
        }
    }
    movestate->active = 1;
}

static void setup_moves(Chesscreen *cs, char *uci) {
    setup_move(cs, uci, 0);         /* General case */
    if (!strncmp(uci, "e1g1", 4)) { /* White king castling */
        setup_move(cs, "h1f1", 1);  /* rook will move at the same time */
        cs->moves[1].castling = 1;
        return;
    }
    if (!strncmp(uci, "e1c1", 4)) { /* White queen castling */
        setup_move(cs, "a1d1", 1);  /* rook will move at the same time */
        cs->moves[1].castling = 1;
        return;
    }
    if (!strncmp(uci, "e8g8", 4)) { /* Black king castling */
        setup_move(cs, "h8f8", 1);  /* rook will move at the same time */
        cs->moves[1].castling = 1;
        return;
    }
    if (!strncmp(uci, "e8c8", 4)) { /* Black queen castling */
        setup_move(cs, "a8d8", 1);  /* rook will move at the same time */
        cs->moves[1].castling = 1;
        return;
    }
}

static int are_moves_active(Chesscreen *cs) {
    for (int i = 0; i < CONCURRENT_MOVES; i++) {
        if (cs->moves[i].active)
            return 1;
    }
    return 0;
}

static int is_a_piece_taken(Chesscreen *cs) {
    for (int i = 0; i < CONCURRENT_MOVES; i++) {
        if (cs->moves[i].tpiece != NONE)
            return 1;
    }
    return 0;
}

static void init_moves(Chesscreen *cs) {
    for (int i = 0; i < CONCURRENT_MOVES; i++) {
        cs->moves[i].active = 0;
        cs->moves[i].tpiece = cs->moves[i].mpiece = NONE;
        cs->moves[i].promotion = NONE;
        cs->moves[i].en_passant[0] = cs->moves[i].en_passant[1] = -1;
        cs->moves[i].color = WHITE;
        cs->moves[i].dx = cs->moves[i].dz = 0;
        cs->moves[i].castling = 0;
    }
}

static double
ease_move (double x, double max)
{
  if (max == 0) return x;
  if (max < 0) max = -max;
  if (x > 0)
    return max * ease (EASE_IN_OUT_CUBIC, x / max);
  else
    return max * -ease (EASE_IN_OUT_CUBIC, -x / max);
}


/* draw a moving piece */
static void drawMovingPiece(ModeInfo *mi, Chesscreen *cs, int shadow) {
    int piece, side, promotion_piece;

    for (int i = 0; i < CONCURRENT_MOVES; i++) {
        Chessmovestate *movestate = &(cs->moves[i]);
        if (!movestate->active)
            continue;
        piece = movestate->mpiece % PIECES;
        side = (movestate->color == WHITE ? 1 : -1);
        if (piece == NONE)
            continue;
        promotion_piece = movestate->promotion;

        glPushMatrix();

        if (shadow)
            glColor4fv(MaterialShadow);
        else
            glColor3fv(cs->colors[movestate->mpiece / PIECES]);

        if ((movestate->mpiece == PAWN && abs(movestate->to[0]) < 0.01) ||
            (movestate->mpiece == BPAWN &&
             fabs(movestate->to[0] - 7.0) < 0.01)) {
            glTranslatef(movestate->from[1] + cs->steps * movestate->dx, 0.0,
                         movestate->from[0] + cs->steps * movestate->dz);
            glColor4f(shadow ? MaterialShadow[0]
                             : cs->colors[movestate->mpiece / 7][0],
                      shadow ? MaterialShadow[1]
                             : cs->colors[movestate->mpiece / 7][1],
                      shadow ? MaterialShadow[2]
                             : cs->colors[movestate->mpiece / 7][2],
                      fabs((num_steps_f / 2.0) - cs->steps) /
                          (num_steps_f / 2.0));

            piece = cs->steps < NUM_STEPS / 2 ? PAWN : promotion_piece % PIECES;

            /* what a kludge */
            if (cs->steps == NUM_STEPS - 1)
                movestate->mpiece = promotion_piece;
        } else if (movestate->mpiece % PIECES == KNIGHT) {
            /* If there is nothing in the path of a knight, move it by sliding,
               just like the other pieces.  But if there are any pieces on the
               middle two squares in its path, the knight would intersect them,
               so in that case, move it in an airborne arc. */
            GLfloat y;
            int i, j;
            Bool blocked_p = False;
            int fromx = MIN(movestate->from[1], movestate->to[1]);
            int fromy = MIN(movestate->from[0], movestate->to[0]);
            int tox = MAX(movestate->from[1], movestate->to[1]);
            int toy = MAX(movestate->from[0], movestate->to[0]);
            if (fromx == tox - 2)
                fromx = tox = fromx + 1;
            if (fromy == toy - 2)
                fromy = toy = fromy + 1;
            for (i = fromy; i <= toy; i++) {
                for (j = fromx; j <= tox; j++) {
                    if (cs->board[i][j]) {
                        blocked_p = True;
                        break;
                    }
                }
            }

            if (!blocked_p)
                goto SLIDE;

            /* Move by hopping. */
            y = 1.5 * sin(M_PI * cs->steps / num_steps_f);
            glTranslatef(movestate->from[1] +
                         ease_move (cs->steps * movestate->dx,
                                    movestate->to[1] - movestate->from[1]),
                         y,
                         movestate->from[0] +
                         ease_move (cs->steps * movestate->dz,
                                    movestate->to[0] - movestate->from[0]));

        } else if ((movestate->mpiece % PIECES == ROOK) &&
                   movestate->castling) {
            /* Move z in an arc */
            GLfloat offset = 1.5 * sin(M_PI * cs->steps / num_steps_f);
            glTranslatef(movestate->from[1] +
                         ease_move (cs->steps * movestate->dx,
                                    movestate->to[1] - movestate->from[1]),
                         0.0,
                         movestate->from[0] + 
                         ease_move (cs->steps * movestate->dz +
                                    (movestate->color == WHITE
                                     ? offset : -offset),
                                    movestate->to[0] - movestate->from[0]));
        } else {
        SLIDE:
            /* Move by sliding. */
            glTranslatef(movestate->from[1] +
                         ease_move (cs->steps * movestate->dx,
                                    movestate->to[1] - movestate->from[1]),
                         0.0,
                         movestate->from[0] + 
                         ease_move (cs->steps * movestate->dz,
                                    movestate->to[0] - movestate->from[0]));
        }

        if (!cs->wire)
            glEnable(GL_BLEND);

        if ((movestate->mpiece == KNIGHT) ||
            ((promotion_piece == KNIGHT) && (cs->steps >= NUM_STEPS / 2)))
            glRotatef(180.0, 0.0, 1.0, 0.0);
        if ((piece == BISHOP) ||
            ((promotion_piece == BISHOP) && (cs->steps >= NUM_STEPS / 2)))
            glRotatef(90.0 * side, 0.0, 1.0, 0.0);
        glCallList(piece);
        if ((movestate->mpiece == KNIGHT) ||
            ((promotion_piece == KNIGHT) && (cs->steps >= NUM_STEPS / 2)))
            glRotatef(180.0, 0.0, 1.0, 0.0);
        if ((piece == BISHOP) ||
            ((promotion_piece == BISHOP) && (cs->steps >= NUM_STEPS / 2)))
            glRotatef(-90.0 * side, 0.0, 1.0, 0.0);
        mi->polygon_count += cs->poly_counts[movestate->mpiece % PIECES];

        glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);

        glPopMatrix();

        if (!cs->wire)
            glDisable(GL_BLEND);
    }
}

/** code to squish a taken piece */
static void drawTakePiece(ModeInfo *mi, Chesscreen *cs, int shadow) {
    if (!cs->wire)
        glEnable(GL_BLEND);

    /* Only the first move can take a piece in case of multiple
     * concurrent moves (castling). So we could only consider the first move
     * for capture... but let's loop anyway over all concurrent moves since it
     * costs almost nothing */
    for (int i = 0; i < CONCURRENT_MOVES; i++) {
        Chessmovestate *movestate = &(cs->moves[i]);
        int side = (movestate->color == BLACK ? 1 : -1);
        if (!movestate->active)
            continue;

        glColor4f(
            shadow ? MaterialShadow[0] : cs->colors[movestate->tpiece / 7][0],
            shadow ? MaterialShadow[1] : cs->colors[movestate->tpiece / 7][1],
            shadow ? MaterialShadow[2] : cs->colors[movestate->tpiece / 7][2],
            (num_steps_f - 1.6 * cs->steps) / num_steps_f);

        if (movestate->en_passant[0] != -1) {
            glTranslatef(movestate->en_passant[1], 0.0,
                         movestate->en_passant[0]);
        } else {
            glTranslatef(movestate->to[1], 0.0, movestate->to[0]);
        }

        if (movestate->tpiece % PIECES == KNIGHT)
            glScalef(1.0 + cs->steps / num_steps_f, 1.0,
                     1.0 + cs->steps / num_steps_f);
        else
            glScalef(1.0,
                     1.0 - cs->steps / num_steps_f / 2 > 0.01
                         ? 1.0 - cs->steps / num_steps_f
                         : 0.01,
                     1.0);

        if (movestate->tpiece == KNIGHT)
            glRotatef(180.0, 0.0, 1.0, 0.0);
        else if ((movestate->tpiece % PIECES) == BISHOP)
            glRotatef(90.0 * side, 0.0, 1.0, 0.0);
        glCallList(movestate->tpiece % 7);
        if (movestate->tpiece == KNIGHT)
            glRotatef(180.0, 0.0, 1.0, 0.0);
        else if ((movestate->tpiece % PIECES) == BISHOP)
            glRotatef(-90.0 * side, 0.0, 1.0, 0.0);
        mi->polygon_count += cs->poly_counts[movestate->tpiece % PIECES];
    }

    if (!cs->wire)
        glDisable(GL_BLEND);
}

/** draw board */
static void drawBoard(ModeInfo *mi, Chesscreen *cs) {
    int i, j;

    glBegin(GL_QUADS);

    for (i = 0; i < BOARDSIZE; ++i)
        for (j = 0; j < BOARDSIZE; ++j) {
            double ma1 = (i + j) % 2 == 0 ? cs->mod * i : 0.0;
            double mb1 = (i + j) % 2 == 0 ? cs->mod * j : 0.0;
            double ma2 = (i + j) % 2 == 0 ? cs->mod * (i + 1.0) : 0.01;
            double mb2 = (i + j) % 2 == 0 ? cs->mod * (j + 1.0) : 0.01;

            /*glColor3fv(colors[(i+j)%2]);*/
            glColor4f(cs->colors[(i + j) % 2][0], cs->colors[(i + j) % 2][1],
                      cs->colors[(i + j) % 2][2], 0.65);

            glNormal3f(0.0, 1.0, 0.0);
            /*       glTexCoord2f(mod*i, mod*(j+1.0)); */
            glTexCoord2f(ma1, mb2);
            glVertex3f(i, 0.0, j + 1.0);
            /*       glTexCoord2f(mod*(i+1.0), mod*(j+1.0)); */
            glTexCoord2f(ma2, mb2);
            glVertex3f(i + 1.0, 0.0, j + 1.0);
            glTexCoord2f(ma2, mb1);
            /*       glTexCoord2f(mod*(i+1.0), mod*j); */
            glVertex3f(i + 1.0, 0.0, j);
            glTexCoord2f(ma1, mb1);
            /*       glTexCoord2f(mod*i, mod*j); */
            glVertex3f(i, 0.0, j);

            mi->polygon_count++;
        }
    glEnd();

    {
        GLfloat off = 0.01;
        GLfloat w = BOARDSIZE;
        GLfloat h = 0.1;

        /* Give the board a slight lip. */
        /* #### oops, normals are wrong here, but you can't tell */

        glColor3f(0.3, 0.3, 0.3);
        glBegin(GL_QUADS);
        glVertex3f(0, 0, 0);
        glVertex3f(0, -h, 0);
        glVertex3f(0, -h, w);
        glVertex3f(0, 0, w);

        glVertex3f(0, 0, w);
        glVertex3f(0, -h, w);
        glVertex3f(w, -h, w);
        glVertex3f(w, 0, w);

        glVertex3f(w, 0, w);
        glVertex3f(w, -h, w);
        glVertex3f(w, -h, 0);
        glVertex3f(w, 0, 0);

        glVertex3f(w, 0, 0);
        glVertex3f(w, -h, 0);
        glVertex3f(0, -h, 0);
        glVertex3f(0, 0, 0);

        glVertex3f(0, -h, 0);
        glVertex3f(w, -h, 0);
        glVertex3f(w, -h, w);
        glVertex3f(0, -h, w);
        glEnd();
        mi->polygon_count += 4;

        /* Fill in the underside of the board with an invisible black box
           to hide the reflections that are not on tiles.  Probably there's
           a way to do this with stencils instead.
         */
        w -= off * 2;
        h = 5;

        glPushMatrix();
        glTranslatef(off, 0, off);
        glDisable(GL_LIGHTING);
        glColor3f(0, 0, 0);
        glBegin(GL_QUADS);
        glVertex3f(0, 0, 0);
        glVertex3f(0, -h, 0);
        glVertex3f(0, -h, w);
        glVertex3f(0, 0, w);

        glVertex3f(0, 0, w);
        glVertex3f(0, -h, w);
        glVertex3f(w, -h, w);
        glVertex3f(w, 0, w);

        glVertex3f(w, 0, w);
        glVertex3f(w, -h, w);
        glVertex3f(w, -h, 0);
        glVertex3f(w, 0, 0);

        glVertex3f(w, 0, 0);
        glVertex3f(w, -h, 0);
        glVertex3f(0, -h, 0);
        glVertex3f(0, 0, 0);

        glVertex3f(0, -h, 0);
        glVertex3f(w, -h, 0);
        glVertex3f(w, -h, w);
        glVertex3f(0, -h, w);
        glEnd();
        mi->polygon_count += 4;
        glPopMatrix();
        if (!cs->wire)
            glEnable(GL_LIGHTING);
    }
}

static void draw_pieces(ModeInfo *mi, Chesscreen *cs, int wire) {
    if (!cs->wire) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, cs->piecetexture);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        glColor4f(0.5, 0.5, 0.5, 1.0);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialShadow);
    }

    drawPieces(mi, cs);
    if (are_moves_active(cs))
        drawMovingPiece(mi, cs, 0);
    if (is_a_piece_taken(cs))
        drawTakePiece(mi, cs, 0);
    glDisable(GL_TEXTURE_2D);
}

static void draw_shadow_pieces(ModeInfo *mi, Chesscreen *cs, int wire) {
    if (!cs->wire) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, cs->piecetexture);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    /* use the stencil */
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glClear(GL_STENCIL_BUFFER_BIT);
    glColorMask(0, 0, 0, 0);
    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFFL);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    glPushMatrix();
    glTranslatef(0.0, 0.001, 0.0);

    /* draw the pieces */
    drawPiecesShadow(mi, cs);
    if (are_moves_active(cs))
        drawMovingPiece(mi, cs, shadows);
    if (is_a_piece_taken(cs))
        drawTakePiece(mi, cs, shadows);

    glPopMatrix();

    /* turn on drawing into colour buffer */
    glColorMask(1, 1, 1, 1);

    /* programming with effect */
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_TEXTURE_2D);

    /* now draw the union of the shadows */

    /*
       <todo>
       want to keep alpha values (alpha is involved in transition
       effects of the active pieces).
       </todo>
    */
    glStencilFunc(GL_NOTEQUAL, 0, 0xFFFFFFFFL);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glEnable(GL_BLEND);

    glColor4fv(MaterialShadow);

    /* draw the board generously to fill the shadows */
    glBegin(GL_QUADS);

    glVertex3f(-1.0, 0.0, -1.0);
    glVertex3f(-1.0, 0.0, BOARDSIZE + 1.0);
    glVertex3f(1.0 + BOARDSIZE, 0.0, BOARDSIZE + 1.0);
    glVertex3f(1.0 + BOARDSIZE, 0.0, -1.0);

    glEnd();

    glDisable(GL_STENCIL_TEST);

    /* "pop" attributes */
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_CULL_FACE);
}

enum { X, Y, Z, W };
enum { A, B, C, D };

/* create a matrix that will project the desired shadow */
static void shadowmatrix(GLfloat shadowMat[4][4], GLfloat groundplane[4],
                         GLfloat lightpos[4]) {
    GLfloat dot;

    /* find dot product between light position vector and ground plane normal */
    dot = groundplane[X] * lightpos[X] + groundplane[Y] * lightpos[Y] +
          groundplane[Z] * lightpos[Z] + groundplane[W] * lightpos[W];

    shadowMat[0][0] = dot - lightpos[X] * groundplane[X];
    shadowMat[1][0] = 0.f - lightpos[X] * groundplane[Y];
    shadowMat[2][0] = 0.f - lightpos[X] * groundplane[Z];
    shadowMat[3][0] = 0.f - lightpos[X] * groundplane[W];

    shadowMat[X][1] = 0.f - lightpos[Y] * groundplane[X];
    shadowMat[1][1] = dot - lightpos[Y] * groundplane[Y];
    shadowMat[2][1] = 0.f - lightpos[Y] * groundplane[Z];
    shadowMat[3][1] = 0.f - lightpos[Y] * groundplane[W];

    shadowMat[X][2] = 0.f - lightpos[Z] * groundplane[X];
    shadowMat[1][2] = 0.f - lightpos[Z] * groundplane[Y];
    shadowMat[2][2] = dot - lightpos[Z] * groundplane[Z];
    shadowMat[3][2] = 0.f - lightpos[Z] * groundplane[W];

    shadowMat[X][3] = 0.f - lightpos[W] * groundplane[X];
    shadowMat[1][3] = 0.f - lightpos[W] * groundplane[Y];
    shadowMat[2][3] = 0.f - lightpos[W] * groundplane[Z];
    shadowMat[3][3] = dot - lightpos[W] * groundplane[W];
}

/** reflectionboard */
static void draw_reflections(ModeInfo *mi, Chesscreen *cs) {
    int i, j;

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(0, 0, 0, 0);
    glDisable(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glBegin(GL_QUADS);

    /* only draw white squares */
    for (i = 0; i < BOARDSIZE; ++i) {
        for (j = (BOARDSIZE + i) % 2; j < BOARDSIZE; j += 2) {
            glVertex3f(i, 0.0, j + 1.0);
            glVertex3f(i + 1.0, 0.0, j + 1.0);
            glVertex3f(i + 1.0, 0.0, j);
            glVertex3f(i, 0.0, j);
            mi->polygon_count++;
        }
    }
    glEnd();
    glEnable(GL_DEPTH_TEST);

    glColorMask(1, 1, 1, 1);
    glStencilFunc(GL_EQUAL, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glPushMatrix();
    glScalef(1.0, -1.0, 1.0);
    glTranslatef(0.5, 0.0, 0.5);

    glLightfv(GL_LIGHT0, GL_POSITION, cs->position);
    draw_pieces(mi, cs, cs->wire);
    glPopMatrix();

    glDisable(GL_STENCIL_TEST);
    glLightfv(GL_LIGHT0, GL_POSITION, cs->position);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glColorMask(1, 1, 1, 1);
}

/** draws the scene */
static void display(ModeInfo *mi, Chesscreen *cs) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    mi->polygon_count = 0;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(current_device_rotation(), 0, 0, 1);

    /** setup perspectiv */
    glTranslatef(0.0, 0.0, -1.5 * BOARDSIZE);
    glRotatef(30.0, 1.0, 0.0, 0.0);
    gltrackball_rotate(cs->trackball);

    if (rotate)
        glRotatef(cs->theta * 100, 0.0, 1.0, 0.0);
    glTranslatef(-0.5 * BOARDSIZE, 0.0, -0.5 * BOARDSIZE);

    /*   cs->position[0] = 4.0 + 1.0*-sin(cs->theta*100*M_PI/180.0); */
    /*   cs->position[2] = 4.0 + 1.0* cos(cs->theta*100*M_PI/180.0); */
    /*   cs->position[1] = 5.0; */

    /* this is the lone light that the shadow matrix is generated from */
    cs->position[0] = 1.0;
    cs->position[2] = 1.0;
    cs->position[1] = 16.0;

    cs->position2[0] = 4.0 + 8.0 * -sin(cs->theta * 100 * M_PI / 180.0);
    cs->position2[2] = 4.0 + 8.0 * cos(cs->theta * 100 * M_PI / 180.0);

    if (!cs->wire) {
        glEnable(GL_LIGHTING);
        glLightfv(GL_LIGHT0, GL_POSITION, cs->position);
        glLightfv(GL_LIGHT1, GL_POSITION, cs->position2);
        glEnable(GL_LIGHT0);
    }

    /** draw board, pieces */
    if (!cs->wire) {
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);

        if (reflections && !cs->wire) {
            draw_reflections(mi, cs);
            glEnable(GL_BLEND);
        }

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, cs->boardtexture);
        drawBoard(mi, cs);
        glDisable(GL_TEXTURE_2D);

        if (shadows) {
            /* render shadows */
            GLfloat m[4][4];
            shadowmatrix(m, cs->ground, cs->position);

            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialShadow);
            glEnable(GL_BLEND);
            glDisable(GL_LIGHTING);
            glDisable(GL_DEPTH_TEST);

            /* display shadow */
            glPushMatrix();
            glTranslatef(0.0, 0.001, 0.0);
            glMultMatrixf(m[0]);
            glTranslatef(0.5, 0.01, 0.5);
            draw_shadow_pieces(mi, cs, cs->wire);
            glPopMatrix();

            glEnable(GL_LIGHTING);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        if (reflections)
            glDisable(GL_BLEND);
    } else
        drawBoard(mi, cs);

    glTranslatef(0.5, 0.0, 0.5);
    draw_pieces(mi, cs, cs->wire);

    if (!cs->wire) {
        glDisable(GL_COLOR_MATERIAL);
        glDisable(GL_LIGHTING);
    }

    if (!cs->button_down_p)
        cs->theta += .002;
}

static void load_fonts(ModeInfo *mi) {
    Chesscreen *cs = &qs[MI_SCREEN(mi)];
    cs->font1_data = load_texture_font(mi->dpy, "titleFont");
    cs->font2_data = load_texture_font(mi->dpy, "titleFont2");
    cs->font3_data = load_texture_font(mi->dpy, "titleFont3");
}

static void set_description(Chesscreen *cs) {
    int move_index;

    if (!labels)
        return;

    for (int i = 0; i < DESC_STR_LEN + SAN_STR_LEN + 1; i++)
        cs->game_desc[i] = cs->game.desc[i];
    for (move_index = 0; move_index <= DESC_STR_LEN; move_index++) {
        if (cs->game_desc[move_index] == '\0')
            break;
    }
    cs->game_desc[move_index++] = '\n';
    cs->move_desc_index = move_index;
}

static void manage_labels(ModeInfo *mi) {
    Chesscreen *cs = &qs[MI_SCREEN(mi)];

    if ((cs->stage == DO_MOVE) || (cs->stage == FADE_IN) ||
        (cs->stage == WAIT_FOR_NEXT_MOVE) ||
        (cs->stage == WAIT_FOR_NEXT_GAME)) {
        if (labels) {
            int move_num = (cs->stage == DO_MOVE) ? cs->mc : cs->mc - 1;
            texture_font_data *f;
            int san_index;

            if (cs->stage != FADE_IN) {
                for (san_index = 0; san_index < SAN_STR_LEN; san_index++) {
                    char c = cs->game.moves[move_num].san[san_index];
                    cs->game_desc[cs->move_desc_index + san_index] = c;
                    if (c == '\0')
                        break;
                }
                cs->game_desc[cs->move_desc_index + san_index] = '\0';
            }
            if (MI_WIDTH(mi) >= 500 && MI_HEIGHT(mi) >= 375)
                f = cs->font1_data;
            else if (MI_WIDTH(mi) >= 350 && MI_HEIGHT(mi) >= 260)
                f = cs->font2_data;
            else
                f = cs->font3_data;
            glColor3f(0.8, 0.8, 0);
            print_texture_label(mi->dpy, f, mi->xgwa.width, mi->xgwa.height, 1,
                                cs->game_desc);
        }
    }
}

/** reshape handler */
ENTRYPOINT void reshape_chess(ModeInfo *mi, int width, int height) {
    GLfloat h = (GLfloat)height / (GLfloat)width;
    int y = 0;

    if (width > height * 5) { /* tiny window: show middle */
        height = width * 9 / 16;
        y = -height / 2;
        h = height / (GLfloat)width;
    }

    glViewport(0, y, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, 1 / h, 2.0, 30.0);
    glMatrixMode(GL_MODELVIEW);
}

/** initialization handler */
ENTRYPOINT void init_chess(ModeInfo *mi) {
    Chesscreen *cs;

    MI_INIT(mi, qs);
    cs = &qs[MI_SCREEN(mi)];

    cs->window = MI_WINDOW(mi);
    cs->wire = MI_IS_WIREFRAME(mi);
    cs->trackball = gltrackball_init(False);

    cs->oldwhite = -1;

    cs->colors[0][0] = 1.0;
    cs->colors[0][1] = 0.5;
    cs->colors[0][2] = 0.0;

    cs->colors[1][0] = 0.3;
    cs->colors[1][1] = 0.3;
    cs->colors[1][2] = 0.3;

    cs->stage = CHOSE_GAME;
    cs->ticks = 0;
    cs->mod = 1.4;
    cs->abort = 0;
    init_moves(cs);

    /*   cs->position[0] = 0.0; */
    /*   cs->position[1] = 5.0; */
    /*   cs->position[2] = 5.0; */
    /*   cs->position[3] = 1.0; */

    cs->position[0] = 0.0;
    cs->position[1] = 24.0;
    cs->position[2] = 2.0;
    cs->position[3] = 1.0;

    cs->position2[0] = 5.0;
    cs->position2[1] = 5.0;
    cs->position2[2] = 5.0;
    cs->position2[3] = 1.0;

    cs->ground[0] = 0.0;
    cs->ground[1] = 1.0;
    cs->ground[2] = 0.0;
    cs->ground[3] = -0.00001;

    cs->cur_game_idx = -1;

    if ((cs->glx_context = init_GL(mi)))
        reshape_chess(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    else
        MI_CLEARWINDOW(mi);

    if (!cs->wire) {
        glDepthFunc(GL_LEQUAL);
        glClearStencil(0);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        make_piece_texture(cs);
        make_board_texture(cs);
    }
    chessmodels_gen_lists(classic, cs->poly_counts);

#ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
    cs->wire = 0;
#endif

    if (!cs->wire) {
        setup_lights(cs);
        glColorMaterial(GL_FRONT, GL_DIFFUSE);
        glShadeModel(smooth ? GL_SMOOTH : GL_FLAT);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
    } else
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    load_fonts(mi);
}

/** does dirty work drawing scene, moving pieces */
ENTRYPOINT void draw_chess(ModeInfo *mi) {
    Chesscreen *cs = &qs[MI_SCREEN(mi)];
    Chessmovestate *movestate;
    Window w = MI_WINDOW(mi);
    Display *disp = MI_DISPLAY(mi);
    int new_game_idx;

    if (!cs->glx_context)
        return;

    glXMakeCurrent(disp, w, *cs->glx_context);

    switch (cs->stage) {
    case CHOSE_GAME:
        new_game_idx = cs->cur_game_idx;
        while (new_game_idx == cs->cur_game_idx)
            new_game_idx = random() % GAMES_NUMBER;

        /* mod the mod */
        cs->mod = 0.6 + (random() % 20) / 10.0;

        cs->cur_game_idx = new_game_idx;
        cs->game = games[cs->cur_game_idx];

        cs->stage = FADE_IN;
        cs->ticks = 1;
        cs->mc = 0;
        build_colors(cs);
        apply_fen(cs);
        set_description(cs);
        init_moves(cs);
        glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION,
                 (double)FADE_FACTOR * TICKS_BET_MOVES);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION,
                 (double)FADE_FACTOR * TICKS_BET_MOVES);
        if (cs->abort)
            cs->abort = 0;
        break;
    case FADE_IN:
        glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION,
                 (double)FADE_FACTOR * TICKS_BET_MOVES / cs->ticks);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION,
                 (double)FADE_FACTOR * TICKS_BET_MOVES / cs->ticks);
        if (cs->abort) {
            cs->abort = 0;
            cs->stage = FADE_OUT;
        } else if (cs->ticks++ == FADE_FACTOR * TICKS_BET_MOVES) {
            cs->stage = WAIT_FOR_NEXT_MOVE;
        }
        break;
    case FADE_OUT:
        glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION,
                 (double)FADE_FACTOR * TICKS_BET_MOVES / cs->ticks);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION,
                 (double)FADE_FACTOR * TICKS_BET_MOVES / cs->ticks);
        if (cs->ticks-- == 0) {
            cs->stage = CHOSE_GAME;
        }
        if (cs->abort)
            cs->abort = 0;
        break;
    case DO_MOVE:
        if (cs->abort) {
            cs->abort = 0;
            cs->ticks = FADE_FACTOR * TICKS_BET_MOVES;
            cs->stage = FADE_OUT;
            break;
        }
        /* End of current move(s) */
        if (are_moves_active(cs) && ++cs->steps == NUM_STEPS) {
            for (int i = 0; i < CONCURRENT_MOVES;
                 i++) { /* Update board with moved piece(s) */
                movestate = &(cs->moves[i]);
                if (movestate->active) {
                    cs->board[movestate->to[0]][movestate->to[1]] =
                        movestate->mpiece;
                }
            }
            /* Reinit stuff for next move */
            cs->ticks = cs->steps = 0;
            init_moves(cs);
            ++cs->mc;
            if (cs->mc == cs->game.movecount) {
                cs->ticks = 0;
                cs->stage = WAIT_FOR_NEXT_GAME;
                cs->mc = 0;
            } else {
                cs->stage = WAIT_FOR_NEXT_MOVE;
            }
        }
        break;
    case WAIT_FOR_NEXT_MOVE:
        if (cs->abort) {
            cs->abort = 0;
            cs->ticks = FADE_FACTOR * TICKS_BET_MOVES;
            cs->stage = FADE_OUT;
            break;
        }
        if (cs->ticks++ >=
            TICKS_BET_MOVES) { /* Wait before processing next move */
            char *uci = cs->game.moves[cs->mc].uci;
            setup_moves(cs, uci);
            cs->steps = 0;
            cs->ticks = 0;
            cs->stage = DO_MOVE;
        }
        break;
    case WAIT_FOR_NEXT_GAME: /* Wait before moving on */
        if (cs->abort) {
            cs->abort = 0;
            cs->ticks = FADE_FACTOR * TICKS_BET_MOVES;
            cs->stage = FADE_OUT;
            break;
        }
        if (cs->ticks++ >= END_FACTOR * TICKS_BET_MOVES) {
            cs->ticks = FADE_FACTOR * TICKS_BET_MOVES;
            cs->stage = FADE_OUT;
        }
        break;
    default:
        break;
    }
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.14);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.14);

    display(mi, cs);
    manage_labels(mi);
    if (mi->fps_p)
        do_fps(mi);
    glFinish();
    glXSwapBuffers(disp, w);
}

ENTRYPOINT void free_chess(ModeInfo *mi) {
    Chesscreen *cs = &qs[MI_SCREEN(mi)];
    int i;
    if (!cs->glx_context)
        return;
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *cs->glx_context);
    gltrackball_free(cs->trackball);
    if (cs->piecetexture)
        glDeleteTextures(1, &cs->piecetexture);
    if (cs->boardtexture)
        glDeleteTextures(1, &cs->boardtexture);
    /* this is horrible! List numbers are hardcoded! */
    for (i = 1; i <= 20; i++)
        if (glIsList(i))
            glDeleteLists(i, 1);
    if (cs->font1_data)
        free_texture_font(cs->font1_data);
    if (cs->font2_data)
        free_texture_font(cs->font2_data);
    if (cs->font3_data)
        free_texture_font(cs->font3_data);
}

XSCREENSAVER_MODULE_2("Endgame", endgame, chess)

#endif
