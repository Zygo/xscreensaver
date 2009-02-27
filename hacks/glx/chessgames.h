/*
 * endgame -- plays through a chess game ending.  enjoy.
 *
 * Copyright (C) 2002 Blair Tennessy (tennessy@cs.ubc.ca)
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __CHESSGAMES_H__
#define __CHESSGAMES_H__

/** structure for a chess game */
typedef struct {

  /** original board configuration */
  int board[BOARDSIZE][BOARDSIZE];

  /** total moves */
  int movecount;

  /** 
      moves in game.  this is a slight hack: moves are encoded in
      integer pairs (x,y).  the first pair, _from_, determines the 
      piece to move.  the second pair, _to_, determines where to move.
      
      in case _to_ is held by another piece, that piece is taken.
      (see drawTakePiece(), draw_chess())

      in case the move promotes a pawn, we assume a queening.
      (see drawMovingPiece())

      what's lacking? 
      castling, en passant, under-promotions.
  */
  int moves[40][4];
} ChessGame;

#define GAMES 7
ChessGame games[GAMES] = {
  
  /** 
      game 1:
      
      E. N. Somov-Nasimovitsch
      White to play and win.
      
      "Zadachi I Etiudi"
      1928
  */
  {
    {
      {   0,     0,     0,    0,     0, BKING,    0,    0},
      {   BPAWN, 0, BPAWN,    0, BPAWN,     0,    0,    0},
      {   0,     0, BPAWN,    0, BPAWN,     0,    0,    KNIGHT},
      {   PAWN,  0,  ROOK,    0,     0,     0,    0,    0},
      {   PAWN,  0,     0,    0,  KING,  PAWN,    0,    0},
      {   0,     0,     0,    0,     0,     0,    0,    0},
      {   BPAWN, 0,     0,    0,     0,     0,    0,    PAWN},
      {  BBISHOP,0,     0,    0,     0,     0,    0,    0},
    },
    
    24,
    
    { 
      {3, 2, 6, 2},
      {7, 0, 6, 1},
      {6, 2, 6, 6},
      {0, 5, 0, 4},
      {6, 6, 0, 6},
      {0, 4, 1, 3},
      {2, 7, 1, 5},
      {2, 2, 3, 2},
      {0, 6, 0, 3},
      {1, 3, 2, 2},
      {0, 3, 6, 3},
      {3, 2, 4, 2}, /* pawn to bishop 5 */
      {1, 5, 0, 3}, /* check */
      {2, 2, 3, 2},
      {0, 3, 2, 4}, /* takes pawn */
      {3, 2, 2, 2},
      {2, 4, 0, 3},
      {2, 2, 3, 2},
      {6, 3, 6, 1}, /* rook takes bishop */
      {6, 0, 7, 0},
      {6, 1, 3, 1},
      {3, 2, 2, 3},
      {3, 1, 3, 3},
      {0, 0, 2, 3},
    }
  },

  /** 
      game 2: 
      
      K. A. L. Kubbel
      White to play and win.
      
      "Chess in the USSR"
      1936
  */
  {
    {
      {   0,     0,     0,    0,     0,     0,    0,    0},
      {   0,     0,     0,    0,     0,     0,    0,    BPAWN},
      {   0,     0,     0,    0, BPAWN,  KING,    0,    BKING},
      {   0,     0,     0,    0,     0,  ROOK,    0,    0},
      {   0,     0,     0,    0,     0,     0,    0,    0},
      {  0,BBISHOP,     0,    0, BROOK,     0, PAWN,    0},
      {   0,     0,     0,    0,     0,     0,    0,    0},
      {   0,     0,     0,    0,     0,BISHOP,    0,    0},
    },
    
    10,
    
    { 
      {3, 5, 6, 5},
      {5, 1, 7, 3},
      {6, 5, 6, 7}, /* check */
      {7, 3, 3, 7}, 
      {7, 5, 6, 4}, 
      {5, 4, 6, 4},
      {5, 6, 4, 6}, /* ! */
      {6, 4, 6, 7},
      {4, 6, 3, 6},
      {0, 0, 2, 7}
    }
  },

  /** 
      game 3: 
      
      J. Hasek
      White to play and win.
      
      "Le Strategie"
      1929
  */
  {
    {
      {     0,      0,      0, KNIGHT,      0,      0,      0, KNIGHT},
      {     0,   KING,  BPAWN,  BPAWN,      0,      0,      0,      0},
      {     0,      0,      0,      0,      0,      0,      0,      0},
      {     0,  BKING,      0,      0,      0,      0,      0,      0},
      {     0,   PAWN,      0,      0,      0,  BPAWN,      0,      0},
      {  PAWN,      0,   PAWN,      0,      0,      0,      0,      0},
      {     0,      0,      0,      0,      0,      0,      0,      0},
      {     0,      0,      0,      0,      0,      0,      0,      0},
    },
    
    11,

    { 
      {0, 3, 2, 2},
      {1, 3, 2, 2},
      {0, 7, 2, 6},
      {4, 5, 5, 5}, 
      {2, 6, 3, 4}, 
      {5, 5, 6, 5},
      {3, 4, 5, 3}, /* ! */
      {6, 5, 7, 5},
      {5, 3, 6, 1},
      {0, 0, 0, 0}, /* mull it over... */
      {0, 0, 3, 1}
    }    
  },

  /** 
      game 4: 

      M.B. Newman
      White to play and win.
      
      "Chess Amateur"
      1913
  */
  {
    {
      {      0,      0,      0,      0,  BQUEEN,      0,      0,      0},
      {BKNIGHT,      0,      0,      0,       0,      0,      0,      0},
      {      0,      0,      0,      0,       0,      0,      0,      0},
      {      0,      0,      0,      0,       0,      0,      0,   PAWN},
      {  BKING,      0, BISHOP,      0,  KNIGHT,      0,      0,      0},
      {  PAWN,       0,      0,      0,  KNIGHT,      0,      0,      0},
      {     0,       0,      0,      0,       0,      0,      0,      0},
      {  KING,       0,      0,      0,       0,      0,      0,      0},
    },
    
    15,

    { 
      {4, 2, 3, 1},
      {0, 4, 3, 1}, /* queen wins bishop */
      {4, 4, 5, 2},
      {4, 0, 5, 0}, /* king takes pawn */
      {5, 2, 3, 1}, /* knight takes queen, check */
      {1, 0, 3, 1}, /* knight takes knight */
      {3, 7, 2, 7}, /* pawn advances */
      {3, 1, 2, 3},
      {5, 4, 4, 2},
      {2, 3, 4, 2},
      {2, 7, 1, 7}, /* pawn advances */
      {4, 2, 2, 3},
      {1, 7, 0, 7},
      {0, 0, 0, 0},
      {0, 0, 5, 0}
    }    
  },

  /** 
      game 5:

      V.A. Korolikov
      White to play and win
      
      First Prize - "Truda"
      1935
  */
  {
    {
      {      0,      0, BISHOP,      0,       0,      0,      0,       0},
      {  BPAWN,   ROOK,      0,      0,       0,      0,      0,       0},
      {      0,      0,  BPAWN,   PAWN,       0,  BKING,      0,       0},
      {      0,      0,      0,      0,       0,      0,      0,       0},
      {      0,      0,      0,      0,       0,      0,   KING, BBISHOP},
      {      0,      0,      0,      0,   BPAWN,      0,   PAWN,      0},
      {      0,      0,      0,      0,       0,  BPAWN,      0,      0},
      {      0,      0,      0,      0,       0,      0,      0,      0},
    },
    
    21,

    { 
      {2, 3, 1, 3}, /* pawn to q7 */
      {2, 5, 1, 4}, /* cover with king */
      {1, 1, 0, 1},
      {4, 7, 5, 6}, /* bishop takes pawn */
      {0, 1, 0, 0}, /* r - r8 */
      {6, 5, 7, 5}, /* queened */
      {1, 3, 0, 3}, /* white pawn promoted */
      {1, 4, 0, 3}, /* king takes queen */
      {0, 2, 2, 0}, /* discovered check */
      {5, 6, 0, 1}, /* pull back bishop */
      {2, 0, 7, 5}, /* bishop takes queen */
      {0, 3, 1, 2},
      {7, 5, 2, 0}, /* save rook */
      {5, 4, 6, 4},
      {2, 0, 6, 4}, /* bishop takes pawn */
      {1, 2, 1, 1}, /* king moves in */
      {6, 4, 5, 5},
      {1, 1, 0, 0},
      {5, 5, 2, 2},
      {0, 0, 0, 0},
      {0, 0, 0, 0}
    }    
  },

  /** 
      game 6:

      T.B. Gorgiev
      White to play and win
      
      First Prize - "64"
      1929
  */
  {
    {
      {      0,      0,      0,      0,       0,      0, KNIGHT,       0},
      {  BKNIGHT,    0,      0,      0,       0,      0,      0,       0},
      {      0,      0,      0,  BKING, BKNIGHT,      0,      0,       0},
      {   KING,      0,      0,      0,       0,      0,      0,       0},
      {      0,      0,      0,      0,       0,      0, KNIGHT,       0},
      {      0,      0,      0,      0,       0,      0,      0,      0},
      {      0,      0,      0,      0,       0,      0,      0,      0},
      {      0,      0,      0,      0,  BISHOP,      0,      0,      0},
    },
    
    13,

    { 
      {3, 0, 2, 1}, /* king on move */
      {1, 0, 0, 2}, /* check */
      {2, 1, 1, 1},
      {0, 2, 1, 4}, /* knight moves on */
      {7, 4, 5, 6}, /* bishop puts king in check */
      {2, 3, 1, 3}, /* king moves back */
      {0, 6, 2, 5}, /* knight moves in, check */
      {1, 3, 0, 3}, /* king moves back queen */
      {5, 6, 1, 2}, /* bishop - b7 ch!! */
      {2, 4, 1, 2}, /* black knight takes bishop */
      {4, 6, 3, 4}, /* knight to k5 */
      {0, 0, 0, 0}, /* mate */
      {0, 0, 0, 0}
    }    
  },

  /** 
      game 7:

      K. A. L. Kubbel
      White to play and win
      
      "Schachmatny Listok"
      1922
  */
  {
    {
      {      0, KNIGHT,      0,      0,       0,      0,       0,      0},
      {      0,      0,      0,      0,       0,      0,      0,      0},
      {   KING,      0,      0,      0,       0,      0,      0,      0},
      {      0,      0,      0,  BKING,       0,      0,      0,      0},
      {      0,      0,      0,  BPAWN,       0,      0,      0, BISHOP},
      {  BPAWN,      0,      0,      0,       0,      0,      0,      0},
      {      0,      0,   PAWN,   PAWN,       0,      0,      0,      0},
      {      0,      0,      0,      0,       0,      0,      0,      0},
    },
    
    12,

    { 
      {0, 1, 2, 2}, /* kt-b6 */
      {3, 3, 2, 2}, /* k x kt */
      {4, 7, 2, 5}, /* b-b6 */
      {2, 2, 3, 3}, /* king back to original position */
      {6, 3, 5, 3}, /* p-q3! */
      {5, 0, 6, 0}, /* p-r7 */
      {6, 2, 4, 2}, /* p-b4ch */
      {3, 3, 3, 2}, /* king moves, black cannot capture in passing */
      {2, 0, 1, 1}, /* k-kt7! */
      {6, 0, 7, 0}, /* promo */
      {2, 5, 1, 4}, /* mate */
      {0, 0, 3, 2},
    }    
  },
};

#endif /* __CHESSGAMES_H__ */
