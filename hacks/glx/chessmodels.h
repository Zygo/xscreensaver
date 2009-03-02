/*
 * models for the xss chess screensavers
 * hacked from:
 *
 * glChess - A 3D chess interface
 *
 * Copyright (C) 2002  Robert  Ancell <bob27@users.sourceforge.net>
 *                     Michael Duelli <duelli@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ugggggggly */
#define PIECES    7
#define NONE      0
#define KING      1
#define QUEEN     2
#define BISHOP    3 
#define KNIGHT    4 
#define ROOK      5
#define PAWN      6 
#define BKING     8
#define BQUEEN    9
#define BBISHOP  10 
#define BKNIGHT  11
#define BROOK    12
#define BPAWN    13 

void revolve_line(double *, double *, double, int);
void gen_model_lists(void);
void draw_pawn(void);
void draw_rook(void);
void draw_rook_old(void);
void draw_knight(void);
void draw_bishop(void);
void draw_queen(void);
void draw_king(void);

