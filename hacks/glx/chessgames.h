/*
 * endgame -- plays through a chess game ending.  enjoy.
 *
 * Copyright (C) 2002 Blair Tennessy (tennessb@unbc.ca)
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

      whats lacking? 
      castling, en passant, under-promotions.  hack at will.
      more games!  feel free to encode favorites!
      and this moves[40][4] junk.  i love c!
  */
  int moves[40][4];
} ChessGame;

#define GAMES 3
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
  }
};

#endif /* __CHESSGAMES_H__ */
