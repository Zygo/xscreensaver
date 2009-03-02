/* -*- Mode: C; tab-width: 4 -*- */
/* solitare --- Shows a pointless game, used in the Mandarin Candidate */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)solitare.cc	5.00 2000/11/01 xlockmore";

#endif

/* Copyright (c) D. Bagley, 1999. */

/*
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * This module is based on Eric Lassauge's text3d and Timothy Budd's
 * C++ program from 1990 contained in the book "An Introduction to
 * Object Oriented Programming"  Addison-Wesley Publishing  1991.
 *
 * Albert H. Morehead and Geoffrey Mott-Smith, The Complete Book of
 * Solitaire and Patience Games, Grosset & Dunlap, New York, 1949.
 *
 * After I started this I found out about some nice related sites:
 *   http://www.delorite.com/store/ace
 *   http://wildsav.idv.uni-linz.ac.at/mfx/psol-cardsets/index.html
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 17-Jan-2000: autoplay, trackmouse, resizing, release
 * 10-Dec-1999: wanted to do some C++, may end up using GL but not for now.
 *
 */

#ifdef STANDALONE
#define PROGCLASS "Solitare"
#define HACK_INIT init_solitare
#define HACK_DRAW draw_solitare
#define solitare_opts xlockmore_opts
#define DEFAULTS "*delay: 2000000 \n" \
 "*ncolors: 64 \n" \
 "*mouse: False \n"

extern "C"
{
#include "xlockmore.h"		/* from the xscreensaver distribution */
}
#else				/* !STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif				/* !STANDALONE */

#ifdef MODE_solitare

/*  Methods overridden in subclasses of class cardPile
           Suit Alternate Deck Discard Deal
initialize         X       X
cleanup                                 X
addCard            X       X      X
display            X
select             X       X      X
canTake     X      X
 */

#define DEF_TRACKMOUSE  "False"

static Bool trackmouse;

#define MAXCOLORS 2
#define MAXSUITS 4
#define MAXRANK 13
#define MAXALTERNATEPILES 7
#define MAXPILES (MAXALTERNATEPILES+MAXSUITS+2)
#define OVERLAP (0.25)

enum Colors {black, red};
enum Suits {spade, diamond, club, heart};

#ifdef FR
// Ace => As, Jack => Valet,
// Queen = Reine (Dame), King = Roi, Joker = Joker
static char *RankNames[] = {NULL, (char *) "A",
	(char *) "2", (char *) "3", (char *) "4", (char *) "5", (char *) "6",
	(char *) "7", (char *) "8", (char *) "9", (char *) "10",
	(char *) "V", (char *) "D", (char *) "R"};
#else
#ifdef NL
// Ace => Aas, Jack => Boer (Farmer),
// Queen = Vrouw (Lady), King = Heer (Gentleman), Joker = Joker
static char *RankNames[] = {NULL, (char *) "A",
	(char *) "2", (char *) "3", (char *) "4", (char *) "5", (char *) "6",
	(char *) "7", (char *) "8", (char *) "9", (char *) "10",
	(char *) "B", (char *) "V", (char *) "H"};
#else
#ifdef RU
// Some may be approximations to Cyrillic
static char *RankNames[] = {NULL, (char *) "T",
	(char *) "2", (char *) "3", (char *) "4", (char *) "5", (char *) "6",
	(char *) "7", (char *) "8", (char *) "9", (char *) "10",
	(char *) "B", (char *) "Q", (char *) "K"};
#else
static char *RankNames[] = {NULL, (char *) "A",
	(char *) "2", (char *) "3", (char *) "4", (char *) "5", (char *) "6",
	(char *) "7", (char *) "8", (char *) "9", (char *) "10",
	(char *) "J", (char *) "Q", (char *) "K"};
#endif
#endif
#endif

static inline int round(double x) { return int(x + 0.5); }

#ifdef DOFONT
// Does this really add anything?
extern XFontStruct *getFont(Display * display);
static XFontStruct *mode_font = None;
static int  char_width[256];

static int
font_width(XFontStruct * font, char ch)
{
	int         dummy;
	XCharStruct xcs;

	(void) XTextExtents(font, &ch, 1, &dummy, &dummy, &dummy, &xcs);
	return xcs.width;
}
#endif

// I am probably not thinking object oriented here...
void
drawSuit(ModeInfo * mi, Suits suit, int x, int y, int width, int height)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	XPoint			polygon[4];

	switch (suit) {
		case spade:
			polygon[0].x =  x + width / 2;
			polygon[0].y =  y;
			polygon[1].x =  short(float(-width) / 3.0);
			polygon[1].y =  height / 3;
			polygon[2].x =  short(float(width) / 3.0);
			polygon[2].y =  height / 3;
			polygon[3].x =  short(float(width) / 3.0) + 1;
			polygon[3].y =  -height / 3;
			XFillPolygon(display, window, gc,
				polygon, 4, Convex, CoordModePrevious);
			XFillArc(display, window, gc,
				x, y + height / 3, width / 2, height / 2, 0, 23040);
			XFillArc(display, window, gc,
				x + width / 2, y + height / 3, width / 2, height / 2, 0, 23040);
			polygon[0].x =  x + width / 2;
			polygon[0].y =  y + height / 2;
			polygon[1].x =  -width / 8;
			polygon[1].y =  height / 2;
			polygon[2].x =  width / 4;
			polygon[2].y =  0;
			XFillPolygon(display, window, gc,
				polygon, 3, Convex, CoordModePrevious);
			break;
		case diamond:
			polygon[0].x =  x + width / 2;
			polygon[0].y =  y;
			polygon[1].x =  -width / 2;
			polygon[1].y =  height / 2;
			polygon[2].x =  width / 2;
			polygon[2].y =  height / 2;
			polygon[3].x =  width / 2;
			polygon[3].y =  -height / 2;
			XFillPolygon(display, window, gc,
				polygon, 4, Convex, CoordModePrevious);
			break;
		case club:
			XFillArc(display, window, gc,
				x, y + short(float(height) / 3.6), width / 2, height / 2, 0, 23040);
			XFillArc(display, window, gc,
				x + width / 2, y + short(float(height) / 3.6), width / 2, height / 2, 0, 23040);
			XFillArc(display, window, gc,
				x + width / 4, y, width / 2, height / 2, 0, 23040);
			polygon[0].x =  x + width / 2;
			polygon[0].y =  y + height / 2 - 1;
			polygon[1].x =  -width / 8;
			polygon[1].y =  height / 2 + 1;
			polygon[2].x =  width / 4;
			polygon[2].y =  0;
			XFillPolygon(display, window, gc,
				polygon, 3, Convex, CoordModePrevious);
			break;
		case heart:
			XFillArc(display, window, gc,
				x + 1, y, width / 2, height / 2, 0, 23040);
			XFillArc(display, window, gc,
				x + width / 2 - 1, y, width / 2, height / 2, 0, 23040);
			polygon[0].x =  x + width / 2;
			polygon[0].y =  y + short(float(height) / 4.1);
			polygon[1].x =  short(float(-width) / 2.7);
			polygon[1].y =  short(float(height) / 4.1);
			polygon[2].x =  short(float(width) / 2.7);
			polygon[2].y =  short(float(height) / 2.05);
			polygon[3].x =  short(float(width) / 2.7);
			polygon[3].y =  short(float(-height) / 2.05);
			XFillPolygon(display, window, gc,
				polygon, 4, Convex, CoordModePrevious);
			break;
	}
}

//card.h
class Card
{
private:
	Suits suit;
	int rank;
public:
	Card(Suits suit, int rank);
	Suits whichSuit();
	int whichRank();
	Colors whichColor();
};
inline Card::Card(Suits a_suit, int a_rank) { this->suit = a_suit; this->rank = a_rank;}
inline Suits Card::whichSuit() { return this->suit;}
inline int Card::whichRank() { return this->rank;}
inline Colors Card::whichColor() { return ((whichSuit() % MAXCOLORS) ? red : black);}


//cardview.h
class CardView
{
private:
	ModeInfo *mi;
	Card * card;
	Bool faceUp;
	int locationX, locationY;
public:
	CardView(ModeInfo * mi, Card * card);
	CardView(ModeInfo *mi, Suits suit, int rank);
	Card * thisCard();
	void draw();
	void erase();
	Bool isFaceUp();
	void flip();
	Bool includes(int, int); // mouse interaction
	int x();
	int y();
	int width();
	int height();
	void moveTo(int, int);
};

inline Card* CardView::thisCard() { return card; }
inline Bool CardView::isFaceUp() { return faceUp; }
inline void CardView::flip() { faceUp = !faceUp; }
inline int CardView::x() { return locationX; }
inline int CardView::y() { return locationY; }
inline void CardView::moveTo(int lx, int ly) { locationX = lx; locationY = ly; }

//cardview.cc
CardView::CardView(ModeInfo * a_mi, Card * a_card)
{
	this->mi = a_mi;
	this->card = a_card;
	faceUp = False;
	locationX = locationY = 0;
}

CardView::CardView(ModeInfo * a_mi, Suits suit, int rank)
{
	this->mi = a_mi;
	if ((this->card = new Card(suit, rank)) == NULL) {
		return;
	}
	faceUp = False;
	locationX = locationY = 0;
}

void CardView::draw()
{
	Display * display = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);
	GC gc = MI_GC(mi);
	unsigned long red_pixel = (MI_NPIXELS(mi) > 2) ? MI_PIXEL(mi,0) : MI_BLACK_PIXEL(mi);
	unsigned long cyan_pixel = (MI_NPIXELS(mi) > 2) ? MI_PIXEL(mi,MI_NPIXELS(mi) / 2) : MI_WHITE_PIXEL(mi);

	if (isFaceUp()) {
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		XFillRectangle(display, window, gc, x(), y(), width(), height());
		if (card->whichColor() == red) {
			XSetForeground(display, gc, red_pixel);
		} else {
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		}
		if (width() > 16 && height() > 34) {
#ifdef CENTERLABEL
			XDrawString(display, window, gc,
				x() + width() / 2 - strlen(RankNames[card->whichRank()]) * 8,
				y() + round(height() * 0.40),
				RankNames[card->whichRank()], strlen(RankNames[card->whichRank()]));
#else
			XDrawString(display, window, gc,
				x() + 8 - strlen(RankNames[card->whichRank()]) * 8 + width() / 8,
				y() + 20,
				RankNames[card->whichRank()], strlen(RankNames[card->whichRank()]));
#endif
		}
#ifdef SUITNAMES
			char *SuitNames[] = {"Spade", "Diamond", "Club", "Heart"};
		XDrawString(display, window, gc, x() + 0.2, y() + round(height() * 0.8),
			SuitNames[card->whichSuit()], strlen(SuitNames[card->whichSuit()]));
#endif
#ifdef CENTERLABEL
		drawSuit(mi, card->whichSuit(),
			x() + round(width() * 0.35), y() + round(height() * 0.55),
			round(0.3 * width()), round(0.3 * height()));
#else
		if (width() > 16 && height() > 34) {
			drawSuit(mi, card->whichSuit(),
				x() + 5, y() + 25,
				round(0.3 * width()), round(0.3 * height()));
		} else {
			drawSuit(mi, card->whichSuit(),
				x() + 2, y() + 2,
				round(0.3 * width()), round(0.3 * height()));
		}
#endif
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XDrawLine(display, window, gc,
			x() + 1, y() + 1, x() + width() - 3, y() + 1);
		XDrawLine(display, window, gc,
			x() + 1, y() + height() - 2, x() + width() - 3, y() + height() - 2);
		XDrawLine(display, window, gc,
			x() + 1, y() + 1, x() + 1, y() + height() - 3);
		XDrawLine(display, window, gc,
			x() + width() - 2, y() + 1, x() + width() - 2, y() + height() - 3);
	} else {
		int n;

		XSetForeground(display, gc, cyan_pixel);
		XFillRectangle(display, window, gc,
			x(), y(), width(), height());
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		n = x() + round(round(width() * 0.3));
		XDrawLine(display, window, gc, n, y() + round(height() * 0.1),
			n, y() + round(height() * 0.9));
		n = x() + round(width() * 0.7);
		XDrawLine(display, window, gc, n, y() + round(height() * 0.1),
			n, y() + round(height() * 0.9));
		n = y() + round(height() * 0.3);
		XDrawLine(display, window, gc, x() + round(height() * 0.1), n,
			x() + round(width() * 0.9), n);
		n = y() + round(height() * 0.7);
		XDrawLine(display, window, gc, x() + round(width() * 0.1), n,
			x() + int(width() * 0.9), n);
	}
	// Shadow
#if 1
	XDrawLine(display, window, gc,
		x() - 1, y() - 1 , x() + width(), y() - 1);
	XDrawLine(display, window, gc,
		x() - 1, y() - 1, x() - 1, y() + height());
#else
	XDrawLine(display, window, gc,
		x() + 1, y() + height(), x() + width(), y() + height());
	XDrawLine(display, window, gc,
		x() + width(), y() + 1, x() + width(), y() + height());
#endif
}

void CardView::erase()
{
	Display * display = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);
	GC gc = MI_GC(mi);

	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	XFillRectangle(display, window, gc,
		x(), y(), width(), height());
}

//pile.h
Bool CardView::includes(int a, int b)
{
	return (a >= x() && (a <= x() + width()) &&
		b >= y() && (b <= y() + height()));
}

class CardLink : public CardView
{
public:
	CardLink(ModeInfo * mi, Suits suit, int rank);
	CardLink(ModeInfo * mi, Card * card);
	CardLink * nextCard();
	void setLink(CardLink * card);

private:
	CardLink * link;
};

inline CardLink::CardLink(ModeInfo *a_mi, Suits suit, int rank) : CardView(a_mi, suit, rank) { ; }
inline CardLink::CardLink(ModeInfo *a_mi, Card * a_card) : CardView(a_mi, a_card) { ; }
inline CardLink * CardLink::nextCard() { return link; }
inline void CardLink::setLink(CardLink * a_card) { link = a_card; }

class CardPile
{
public:
	CardPile(ModeInfo *);

	virtual void updateLocation(ModeInfo *);
	virtual void addCard(CardLink *);
	virtual Bool canTake(Card *);
	Bool contains(int, int); // mouse interaction
	virtual void displayPile();
	virtual Bool initialize();
	virtual void cleanup();
	CardLink * removeCard();
	virtual Bool select(int, int); // mouse interaction
	virtual Bool select(Bool); // generator

protected:
	int x, y;
	ModeInfo * mi;
	CardLink *top;
};

#define nilLink (CardLink *) 0

inline CardPile::CardPile(ModeInfo * a_mi) { this->mi = a_mi; top = nilLink; }

class DealPile : public CardPile
{
public:
	DealPile(ModeInfo *);
	virtual void cleanup();
	Bool shuffle(CardPile *);
};
inline DealPile::DealPile(ModeInfo * a_mi) : CardPile(a_mi) { ; }

class SuitPile : public CardPile
{
public:
	SuitPile(ModeInfo * a_mi, int s) : CardPile(a_mi) { suit = s; }
	virtual void updateLocation(ModeInfo *);
	virtual Bool canTake(Card *);
private:
	int suit;
};

class AlternatePile : public CardPile
{
public:
	AlternatePile(ModeInfo * a_mi, int p) : CardPile(a_mi) { pile = p; }
	virtual void updateLocation(ModeInfo *);
	virtual void addCard(CardLink *);
	virtual Bool canTake(Card *);
	void copyBuild(CardLink *, CardPile *);
	virtual void displayPile();
	virtual Bool select(int, int);
	virtual Bool select(Bool);
	virtual Bool initialize();
private:
	int pile;
};

class DeckPile : public CardPile
{
public:
	DeckPile(ModeInfo * a_mi) : CardPile(a_mi) { ; }
	virtual void updateLocation(ModeInfo *);
	virtual void addCard(CardLink *);
	virtual Bool initialize();
	virtual Bool select(int, int);
	virtual Bool select(Bool);
};

class DiscardPile : public CardPile
{
public:
	DiscardPile(ModeInfo * a_mi) : CardPile(a_mi) { ; }
	virtual void updateLocation(ModeInfo *);
	virtual void addCard(CardLink *);
	virtual Bool select(int, int);
	virtual Bool select(Bool);
};

//game.h
class GameTable
{
public:
	GameTable(ModeInfo *);
	~GameTable();
	Bool newGame(ModeInfo *);
	Bool suitCanAdd(Card *);
	CardPile * suitAddPile(Card *);
	Bool alternateCanAdd(Card *);
	CardPile * alternateAddPile(Card *);
	void Resize(ModeInfo *);
	void Redraw(ModeInfo *);
	Bool HandleMouse(ModeInfo *);
	Bool HandleGenerate();

	DealPile *dealPile;

	CardPile *deckPile;
	CardPile *discardPile;
protected:
	CardPile * allPiles[MAXALTERNATEPILES + MAXSUITS + 2];
	CardPile * suitPiles[MAXSUITS];
	CardPile * alternatePiles[MAXALTERNATEPILES];
};

// pile.cc

typedef struct {
	Bool        painted;
	GameTable * game;
	int         width, height, cardwidth, cardheight;
	int         showend;
#ifdef DOFONT
	int         fontascent, fontheight;
#endif
} solitarestruct;

static solitarestruct *solitare = (solitarestruct *) NULL;

int CardView::width()
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];

	return bp->cardwidth;
}

int CardView::height()
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];

	return bp->cardheight;
}

void CardPile::addCard(CardLink * card)
{
	if (card != nilLink) {
		card->setLink(top);
		top = card;
		top->moveTo(x, y);
	}
}

void CardPile::updateLocation(ModeInfo * a_mi)
{
	;
}

Bool CardPile::canTake(Card * card)
{
	return False; // from virtual, card unused
}

Bool CardPile::contains(int a, int b)
{
	for (CardLink *p = top; p!= nilLink; p = p->nextCard())
		if (p->includes(a, b))
			return True;
	return False;
}

void CardPile::displayPile()
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	Display * display = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);
	GC gc = MI_GC(mi);

	if (top == nilLink) {
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, window, gc,
			x, y, bp->cardwidth, bp->cardheight);
	} else {
		top->draw();
	}
}

Bool CardPile::initialize()
{
	top = nilLink;
	return True;
}

void CardPile::cleanup()
{
	CardLink *p;

	while (top != nilLink) {
  		p = top;
		top = top->nextCard();
		delete p;
	}
}

CardLink * CardPile::removeCard()
{
	CardLink *p;

	if (top == nilLink)
		return nilLink;
	p = top;
	top = top->nextCard();
	return p;
}

Bool CardPile::select(int a_x, int a_y)
{
	// from virtual, a_x & a_y unused
	return select(True);
}

Bool CardPile::select(Bool first)
{
	// from virtual, first unused
	return False;
}

void DealPile::cleanup()
{
	CardLink *p;

  while (top != nilLink) {
    p = top;
		top = top->nextCard();
		delete p->thisCard();
    delete p;
   }
}

Bool DealPile::shuffle(CardPile * pile)
{
	CardLink *p, *q;
	int max, limit, i;

	// first see how many cards we have
	for (max = 0, p = top; p != nilLink; p = p->nextCard()) {
		max++;
		if (p->isFaceUp())
			p->flip();
	}
	// then pull them out, randomly, one at a time
	for (; max > 0; max--) {
		limit = (int) ((LRAND() >> 3) % max) + 1;
		for ( i= 1, p = top; i <= limit; ) {
			while (p->isFaceUp())
				p = p->nextCard();
			i++;
			if (i <= limit)
				p = p->nextCard();
		}
		if ((q = new CardLink(mi, p->thisCard())) == NULL) {
			return False;
		}
		pile->addCard(q);
		p->flip();
	}
	return True;
}

void DeckPile::updateLocation(ModeInfo * a_mi)
{
	solitarestruct *bp = &solitare[MI_SCREEN(a_mi)];

	//set pile
	x = round(((bp->cardwidth + 2.0 * MAXSUITS * bp->cardwidth *
		(MAXALTERNATEPILES + 1.0)) / (2.0 * MAXALTERNATEPILES)) +
		3 * bp->cardwidth / 4.0);
	y = round(0.25 * bp->cardheight);

	// set card
	for (CardLink *p = top; p!= nilLink; p = p->nextCard()) {
		p->moveTo(x, y);
	}
}

Bool DeckPile::initialize()
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];

	CardPile::initialize();
	if (!bp->game->dealPile->shuffle(this))
    	return False;
	return True;
}

void DeckPile::addCard(CardLink *c)
{
	if (c->isFaceUp())
		c->flip();
	CardPile::addCard(c);
}

Bool DeckPile::select(int a_x, int a_y)
{
	// from virtual, a_x & a_y unused
	return select(True);
}

// turn over a new card
Bool DeckPile::select(Bool first)
{
	// from virtual, first unused
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	CardLink *c;
	Bool didSomething = False;

	if (top != nilLink) {
		c = removeCard(); // should not this be 3 at a time
		if (c != nilLink)
			(bp->game->discardPile)->addCard(c);
		didSomething = True;
	}
	displayPile();
	(bp->game->discardPile)->displayPile();
	return didSomething;
}

void DiscardPile::updateLocation(ModeInfo * a_mi)
{
	solitarestruct *bp = &solitare[MI_SCREEN(a_mi)];

	//set pile
	x = round(((bp->cardwidth + 2.0 * MAXSUITS * bp->cardwidth *
		(MAXALTERNATEPILES + 1.0)) / (2.0 * MAXALTERNATEPILES)) +
		2 * bp->cardwidth);
	y = round(0.25 * bp->cardheight);

	// set card
	for (CardLink *p = top; p!= nilLink; p = p->nextCard()) {
		p->moveTo(x, y);
	}
}

void DiscardPile::addCard(CardLink *c)
{
	if (!(c->isFaceUp()))
		c->flip();
	CardPile::addCard(c);
}

// play the current face card
Bool DiscardPile::select(int a_x, int a_y)
{
	// from virtual, a_x & a_y unused
	return select(True);
}

// play the current face card
Bool DiscardPile::select(Bool first)
{
	// from virtual, first unused
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	CardPile * pile;

	if (top == nilLink)
		return False;
	// see if we can move it to a suit pile
	if (bp->game->suitCanAdd(top->thisCard())) {
		pile = bp->game->suitAddPile(top->thisCard());
		pile->addCard(removeCard());
		displayPile();
		pile->displayPile();
		return True;
	}
	if (bp->game->alternateCanAdd(top->thisCard())) {
		pile = bp->game->alternateAddPile(top->thisCard());
		pile->addCard(removeCard());
		displayPile();
		pile->displayPile();
		return True;
	}
	return False;
}

void SuitPile::updateLocation(ModeInfo * a_mi)
{
	solitarestruct *bp = &solitare[MI_SCREEN(a_mi)];

	//set pile
	x = round(((bp->cardwidth + 2.0 * suit * bp->cardwidth *
		(MAXALTERNATEPILES + 1.0)) / (2.0 * MAXALTERNATEPILES)) +
		bp->cardwidth / 2.0);
	y = round(0.25 * bp->cardheight);

	// set card
	for (CardLink *p = top; p!= nilLink; p = p->nextCard()) {
		p->moveTo(x, y);
	}
}

Bool SuitPile::canTake(Card * card)
{
	if (top == nilLink) { // empty so can take ace
		if (card->whichRank() == 1)
			return True;
		return False;
	}
	if ((top->thisCard())->whichSuit() != card->whichSuit())
		return False;
	if (((top->thisCard())->whichRank() + 1) == card->whichRank())
		return True;
	return False;
}

void AlternatePile::updateLocation(ModeInfo * a_mi)
{
	solitarestruct *bp = &solitare[MI_SCREEN(a_mi)];
	int ty;

	//set pile
	x = (round((bp->cardwidth + 2.0 * pile * bp->cardwidth *
	 (MAXALTERNATEPILES + 1.0)) / (2.0 * MAXALTERNATEPILES)));
	y = round(1.5 * bp->cardheight);

	CardLink *bf = nilLink;
	// set card
	for (CardLink *p = top; p!= nilLink; p = p->nextCard()) {
		p->moveTo(x, y);
		// find bottom faceup card
		// not setup to work with STRANGE_LAYOUT
		if (p->isFaceUp())
			bf = p;
	}
	if (bf != nilLink) {
		// ok but this is wrong.  Since we only have a singly link
		// we will do this slightly inefficiently...
		ty = y;
		while (bf != top) {
			CardLink *sp;
			for (sp = top; sp->nextCard() != bf; sp = sp->nextCard());
			bf = sp;
			ty += round(bp->cardheight * OVERLAP);
			sp->moveTo(x, ty);
		}
	}
}

Bool AlternatePile::initialize()
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	int a_pile;

	//put the right number of cards on the alternate
	(void) CardPile::initialize();
	for (a_pile = 0; a_pile <= this->pile; a_pile++)
		addCard((bp->game->deckPile)->removeCard());
	// flip the last one
	if (top != nilLink) {
		top->flip();
	}
	return True;
}

void AlternatePile::addCard(CardLink * card)
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	int tx, ty;

	if (top == nilLink)
		CardPile::addCard(card);
	else {
		tx = top->x();
		ty = top->y();

		// figure out where to place the card
#ifdef STRANGE_LAYOUT
		// This was the original logic... does not look right to me.
		if (!(top->isFaceUp() && top->nextCard() != nilLink &&
			(top->nextCard())->isFaceUp()))
#else
		if (top->isFaceUp())
#endif
			ty += round(bp->cardheight * OVERLAP);
		CardPile::addCard(card);
		top->moveTo(tx, ty);
	}
}

Bool AlternatePile::canTake(Card *card)
{
	if (top == nilLink) { // can take only kings on an empty pile
		if (card->whichRank() == MAXRANK) {
			return True;
		}
		return False;
	}
	// see if it is face up, colors are different, and the number is legal
	if ((top->thisCard())->whichColor() != card->whichColor() &&
	    ((top->thisCard())->whichRank() - 1) == card->whichRank()) {
		return True;
	}
	return False;
}

void AlternatePile::copyBuild(CardLink *card, CardPile * a_pile)
{
	CardLink *tempCard;

	top->erase();
	tempCard = removeCard();
	displayPile();
	if (card != tempCard)
		copyBuild(card, a_pile);
	a_pile->addCard(tempCard);
	a_pile->displayPile();
}

static void stackDisplay(CardLink *p)
{
	if (p->nextCard())
		stackDisplay(p->nextCard());
	p->draw();
}

void AlternatePile::displayPile()
{
	// Zero or one cards, can not do any better
	if (top == nilLink)
		CardPile::displayPile();
	else { // otherwise half display all the covered cards
		stackDisplay(top);
	}
}

Bool AlternatePile::select(int a_x, int a_y)
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	CardLink *c;

	// no cards, do nothing
	if (top == nilLink)
		return False;

	// if top card is not flipped, flip it now
	if (!top->isFaceUp()) {
		top->flip();
		top->draw();
		return True;
	}

	// if it was to top card, see if we can move it
	if (top->includes(a_x, a_y)) {
		// see if we ca move it to a suit pile
		if (bp->game->suitCanAdd(top->thisCard())) {
			copyBuild(top, bp->game->suitAddPile(top->thisCard()));
			return True;
		}
		// else see if we can move a alternate pile but only if it is not part of pile
		if (((top->nextCard() == nilLink) || !((top->nextCard())->isFaceUp())) &&
			bp->game->alternateCanAdd(top->thisCard())) {
			copyBuild(top, bp->game->alternateAddPile(top->thisCard()));
			return True;
		}
	}

	// else see if we can move a pile
	for (c = top->nextCard(); c!= nilLink; c = c->nextCard())
		if (c->isFaceUp() && c->includes(a_x, a_y)) {
			if (bp->game->alternateCanAdd(c->thisCard())) {
				copyBuild(c, bp->game->alternateAddPile(c->thisCard()));
			}
			return True;
		}
	return False;
}

Bool AlternatePile::select(Bool first)
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	CardLink *c;

	// no cards, do nothing
	if (top == nilLink)
		return False;

	// if top card is not flipped, flip it now
	if (!top->isFaceUp()) {
		top->flip();
		top->draw();
		return True;
	}
	// see if we ca move it to a suit pile
	if (bp->game->suitCanAdd(top->thisCard())) {
		copyBuild(top, bp->game->suitAddPile(top->thisCard()));
		return True;
	}

	// If all cards are not flipped it could lead to problems...
	if (first)
		return False;

	// Other moves may be better but this is stupid
	// Bouncing may result if one allows to take away moves
	// find highest faceup card
	c = top;
	while (c->nextCard() != nilLink && (c->nextCard())->isFaceUp())
		c = c->nextCard();

	// if not king see if it can be moved to leave a space (else it will bounce)
	if (((c->nextCard() != nilLink) ||
	    ((c->thisCard())->whichRank() != MAXRANK)) &&
	    bp->game->alternateCanAdd(c->thisCard())) {
				copyBuild(c, bp->game->alternateAddPile(c->thisCard()));
				return True;
	}
	return False;
}

GameTable::GameTable(ModeInfo *mi)
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	int suit, rank, pile;

	bp->width = MI_WIDTH(mi);
	bp->height = MI_HEIGHT(mi);
	bp->showend = 0;
	bp->cardwidth = bp->width / (MAXALTERNATEPILES + 1);
	bp->cardheight = bp->height / 5;
	// Create the original (unshuffled) deck (with no cards) ... dah daaah!
	if ((dealPile = new DealPile(mi)) == NULL) {
		return;
	}
	for (suit = 0; suit < MAXSUITS; suit++)
		for (rank = 1; rank <= MAXRANK; rank++) {
            CardLink * deal;
			// 52 pickup, creating new cards and creating links
			if ((deal = new CardLink(mi, Suits(suit), rank)) == NULL) {
				dealPile->cleanup();
				delete dealPile;
				return;
			}
			dealPile->addCard(deal);
		}
	// create 2 piles the deck and discard piles
	if ((allPiles[0] = deckPile = new DeckPile(mi)) == NULL) {
		dealPile->cleanup();
		delete dealPile;
		return;
	}
	deckPile->updateLocation(mi);
	if ((allPiles[1] = discardPile = new DiscardPile(mi)) == NULL) {
		dealPile->cleanup();
		delete dealPile;
		delete deckPile;
		return;
	}
	discardPile->updateLocation(mi);

	// create suit piles
	for (suit = 0; suit < MAXSUITS; suit++) {
		if ((allPiles[suit + 2] = suitPiles[Suits(suit)] = new SuitPile(mi, suit)) == NULL) {
			delete this;
			return;
		}
		(suitPiles[Suits(suit)])->updateLocation(mi);
	}

	// create alternate piles
	for (pile = 0; pile < MAXALTERNATEPILES; pile++) {
		if ((allPiles[pile + 2 + MAXSUITS] = alternatePiles[pile] =
			new AlternatePile(mi, pile)) == NULL) {
			delete this;
			return;
		}
		(alternatePiles[pile])->updateLocation(mi);
	}
}

GameTable::~GameTable()
{
	for (int pile = 0; pile < MAXPILES; pile++)
		if (allPiles[pile]) {
			allPiles[pile]->cleanup();
			delete allPiles[pile];
		}
	if (dealPile) {
		dealPile->cleanup();
		delete dealPile;
	}
}

void GameTable::Resize(ModeInfo *mi)
{
	solitarestruct *bp = &solitare[MI_SCREEN(mi)];
	int suit, pile;

	bp->width = MI_WIDTH(mi);
	bp->height = MI_HEIGHT(mi);
	bp->cardwidth = bp->width / (MAXALTERNATEPILES + 1);
	bp->cardheight = bp->height / 5;
	deckPile->updateLocation(mi);
	discardPile->updateLocation(mi);

	// move suit piles
	for (suit = 0; suit < MAXSUITS; suit++) {
		(suitPiles[Suits(suit)])->updateLocation(mi);
	}

	// move alternate piles
	for (pile = 0; pile < MAXALTERNATEPILES; pile++) {
		(alternatePiles[pile])->updateLocation(mi);
	}
}

Bool GameTable::newGame(ModeInfo * mi)
{
	int pile;

	// initialize all the piles
	for (pile = 0; pile < MAXPILES; pile++)
		if (!allPiles[pile]->initialize())
			return False;
	// redraw the game window
	Redraw(mi);
	return True;
}

void GameTable::Redraw(ModeInfo * mi)
{
	int pile;

	//first clear the entire playing area
	MI_CLEARWINDOW(mi);

	// then display the piles
	for (pile = 0; pile < MAXPILES; pile++)
		allPiles[pile]->displayPile();
}

// this is where the smarts is/is not
Bool GameTable::HandleMouse(ModeInfo * mi)
{
	Window r, c;
	int cx, cy, rx, ry;
	unsigned int m;
	int pile;

	(void) XQueryPointer(MI_DISPLAY(mi), MI_WINDOW(mi),
	  &r, &c, &rx, &ry, &cx, &cy, &m);

	if (cx <= 0  || cy <= 0 ||
	    cx >= MI_WIDTH(mi) - 1 || cy >= MI_HEIGHT(mi) -1) {
		return HandleGenerate();
	}

	for (pile = 0; pile < MAXPILES; pile++)
		if (allPiles[pile]->contains(cx, cy)) {
			(void) allPiles[pile]->select(cx, cy);
			return True;
		}
	return True;
}

// if done right it looks smart...
Bool GameTable::HandleGenerate()
{
	int pile;

	// Start with the biggest stacks... usually the last ones
	for (pile = MAXALTERNATEPILES - 1; pile >= 0; pile--)
		if (alternatePiles[pile]->select(True))
			return True;
	for (pile = MAXALTERNATEPILES - 1; pile >= 0; pile--)
		if (alternatePiles[pile]->select(False))
			return True;
	// Look for something to do in discard pile
	if (discardPile->select(True))
		return True;

	// Get a new card or end it
	return (deckPile->select(True));
}

// see if any of the suit piles can add a specific card
Bool GameTable::suitCanAdd(Card * card)
{
	int suit;

	for (suit = 0; suit < MAXSUITS; suit++)
		if (suitPiles[Suits(suit)]->canTake(card))
			return True;
	return False;
}

// see if any of the alternate piles can add a specific card
Bool GameTable::alternateCanAdd(Card * card)
{
	int pile;

	for (pile = 0; pile < MAXALTERNATEPILES; pile++)
		if (alternatePiles[pile]->canTake(card))
			return True;
	return False;
}

// return which of the suit piles can add a card
CardPile * GameTable::suitAddPile(Card * card)
{
	int suit;

	for (suit = 0; suit < MAXSUITS; suit++)
		if (suitPiles[Suits(suit)]->canTake(card))
			return suitPiles[Suits(suit)];
	(void) printf("suitAddPile\n");
	return (CardPile *) NULL; // Hopefully we can not get here
}

// return which of the alternate piles can add a card
CardPile * GameTable::alternateAddPile(Card * card)
{
	int pile;

	for (pile = 0; pile < MAXPILES; pile++)
		if (alternatePiles[pile]->canTake(card))
			return alternatePiles[pile];
	(void) printf("alternateAddPile\n");
	return (CardPile *) NULL; // Hopefully we can not get here
}

/* Yes, it's an ugly mix of 'C' and 'C++' functions */
extern "C" { void init_solitare(ModeInfo * mi); }
extern "C" { void draw_solitare(ModeInfo * mi); }
extern "C" { void change_solitare(ModeInfo * mi); }
extern "C" { void release_solitare(ModeInfo * mi); }
extern "C" { void refresh_solitare(ModeInfo * mi); }

#ifndef DISABLE_INTERACTIVE
static XrmOptionDescRec opts[] =
{
	{(char *) "-trackmouse", (char *) ".solitare.trackmouse", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+trackmouse", (char *) ".solitare.trackmouse", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & trackmouse, (char *) "trackmouse", (char *) "TrackMouse", (char *) DEF_TRACKMOUSE, t_Bool}
};

static OptionStruct desc[] =
{
	{(char *) "-/+trackmouse", (char *) "turn on/off the tracking of the mouse"}
};

ModeSpecOpt solitare_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};
#else
ModeSpecOpt solitare_opts =
{0, NULL, 0, NULL, NULL};
#endif

#ifdef USE_MODULES
ModStruct solitare_description =
{(char *) "solitare", (char *) "init_solitare",
 (char *) "draw_solitare", (char *) "release_solitare",
 (char *) "refresh_solitare", (char *) "init_solitare",
 NULL, &solitare_opts,
 2000000, 1, 1, 1, 64, 1.0, (char *) "",
 (char *) "Shows Klondike's game of solitare", 0, NULL};
#endif

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *    Initialize solitare.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

void
init_solitare(ModeInfo * mi)
{
	solitarestruct *bp;

	if (solitare == NULL) {
		if ((solitare = (solitarestruct *) calloc(MI_NUM_SCREENS(mi),
					sizeof(solitarestruct))) == NULL)
		return;
	}
	bp = &solitare[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);
#ifdef DOFONT
	Display    *display = MI_DISPLAY(mi);
	XGCValues gcv;

	 if (mode_font == None)
		mode_font = getFont(display);
	  if (mode_font != None) {
			gcv.font = mode_font->fid;
			XSetFont(display, MI_GC(mi), mode_font->fid);
			gcv.graphics_exposures = False;
			gcv.foreground = MI_WHITE_PIXEL(mi);
			gcv.background = MI_BLACK_PIXEL(mi);
			if ((mp->gc = XCreateGC(display, MI_WINDOW(mi),
			  GCForeground | GCBackground | GCGraphicsExposures | GCFont,
					&gcv)) == None) {
				return;
			}
			mp->ascent = mode_font->ascent;
			mp->height = font_height(mode_font);
			for (i = 0; i < 256; i++)
				char_width[i] = 8;
		}
#endif
	if (bp->game) {
		if (bp->width != MI_WIDTH(mi) || bp->height != MI_HEIGHT(mi)) {
			/* Let us not be creative then... */
			bp->game->Resize(mi);
			bp->painted = True;
			refresh_solitare(mi);
			return;
		}
		delete bp->game;
	}
	bp->painted = False;
	if ((bp->game = new GameTable(mi)) == NULL) {
		return;
	}
	if (!bp->game->newGame(mi)) {
		delete bp->game;
	}
}

/*
 *-----------------------------------------------------------------------------
 *    Called by the mainline code periodically to update the display.
 *-----------------------------------------------------------------------------
 */
void
draw_solitare(ModeInfo * mi)
{
	solitarestruct *bp;

	if (solitare == NULL)
		return;
	bp = &solitare[MI_SCREEN(mi)];
	if (!bp->game)
		return;

	MI_IS_DRAWN(mi) = True;
	bp->painted = True;
	if (bp->showend) {
		bp->showend++;
		if (bp->showend >= 10)
			init_solitare(mi);
	} else if (trackmouse) {
		if (!bp->game->HandleMouse(mi)) {
			bp->showend++;
		}
	} else if (!bp->game->HandleGenerate()) {
		bp->showend++;
	}
}

/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed
 *      memory and X resources that we've alloc'ed.  Only called
 *      once, we must zap everything for every screen.
 *-----------------------------------------------------------------------------
 */

void
release_solitare(ModeInfo * mi)
{
	if (solitare != NULL) {
		for (int screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			solitarestruct *bp = &solitare[screen];

			if (bp->game)
				delete bp->game;
		}
		(void) free((void *) solitare);
		solitare = (solitarestruct *) NULL;
	}
}

void
refresh_solitare(ModeInfo * mi)
{
	solitarestruct *bp;

	if (solitare == NULL)
		return;
	bp = &solitare[MI_SCREEN(mi)];
	if (!bp->game)
		return;

	if (bp->painted) {
		bp->game->Redraw(mi);
	  bp->painted = False;
	}
}

#endif	/* MODE_solitare */
