/* barcode, draw some barcodes
 * by Dan Bornstein, danfuzz@milk.com
 * Copyright (c) 2003 Dan Bornstein. All rights reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * See the included man page for more details.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "screenhack.h"
#include <X11/Xutil.h>

#include <time.h>
#include <sys/time.h>
#include <ctype.h>


/* parameters that are user configurable */

/* delay (usec) between iterations */
static int delay;


/* non-user-modifiable immutable definitions */

#define FLOAT double

/* random float in the range (0..1) */
#define RAND_FLOAT_01 \
        (((FLOAT) ((random() >> 8) & 0xffff)) / ((FLOAT) 0x10000))

#define BARCODE_WIDTH (164)
#define BARCODE_HEIGHT (69)
#define MAX_MAG (7)

/* width and height of the window */
static int windowWidth;
static int windowHeight;

static Display *display;        /* the display to draw on */
static Window window;           /* the window to draw on */
static Visual *visual;          /* the visual to use */
static Screen *screen;          /* the screen to draw on */
static Colormap cmap;           /* the colormap of the window */

static GC theGC;                /* GC for drawing */
unsigned long fg_pixel;
static Bool button_down_p;



/* simple bitmap structure */

typedef struct
{
    int width;
    int height;
    int widthBytes;
    char *buf;
}
Bitmap;



/* the model */

typedef struct
{
    int x;          /* x coordinate of the left of the barcode */
    int y;          /* y coordinate of the left of the barcode */
    int mag;        /* magnfication factor */
    Bitmap *bitmap; /* the bitmap */
    char code[128]; /* the barcode string */
    unsigned long pixel;  /* the color */
}
Barcode;

static Barcode *barcodes; /* array of barcodes */
static int barcode_count; /* how many barcodes are currently active */
static int barcode_max;   /* the maximum number of active barcodes */

static XImage *theImage;  /* ginormo image for drawing */
static Bitmap *theBitmap; /* ginormo bitmap for drawing */

static enum { BC_SCROLL, BC_GRID, BC_CLOCK12, BC_CLOCK24 } mode;

/* a bunch of words */
static char *words[] = 
{
  "abdomen",
  "abeyance",
  "abhorrence",
  "abrasion",
  "abstraction",
  "acid",
  "addiction",
  "alertness",
  "Algeria",
  "anxiety",
  "aorta",
  "argyle socks",
  "attrition",
  "axis of evil",
  "bamboo",
  "bangle",
  "bankruptcy",
  "baptism",
  "beer",
  "bellicosity",
  "bells",
  "belly",
  "bliss",
  "bogosity",
  "boobies",
  "boobs",
  "booty",
  "bread",
  "bubba",
  "burrito",
  "California",
  "capybara",
  "cardinality",
  "caribou",
  "carnage",
  "children",
  "chocolate",
  "CLONE",
  "cock",
  "constriction",
  "contrition",
  "cop",
  "corpse",
  "cowboy",
  "crabapple",
  "craziness",
  "cthulhu",
  "Death",
  "decepticon",
  "deception",
  "Decker",
  "decoder",
  "decoy",
  "defenestration",
  "democracy",
  "dependency",
  "despair",
  "desperation",
  "disease",
  "disease",
  "doberman",
  "DOOM",
  "dreams",
  "dreams",
  "drugs",
  "easy",
  "ebony",
  "election",
  "eloquence",
  "emergency",
  "eureka",
  "excommunication",
  "fat",
  "fatherland",
  "Faust",
  "fear",
  "fever",
  "filth",
  "flatulence",
  "fluff",
  "fnord",
  "freedom",
  "fruit",
  "fruit",
  "futility",
  "gerbils",
  "GOD",
  "goggles",
  "goobers",
  "gorilla",
  "halibut",
  "handmaid",
  "happiness",
  "hate",
  "helplessness",
  "hemorrhoid",
  "hermaphrodite",
  "heroin",
  "heroine",
  "hope",
  "hysteria",
  "icepick",
  "identity",
  "ignorance",
  "importance",
  "individuality",
  "inkling",
  "insurrection",
  "intoxicant",
  "ire",
  "irritant",
  "jade",
  "jaundice",
  "Joyce",
  "kidney stone",
  "kitchenette",
  "kiwi",
  "lathe",
  "lattice",
  "lawyer",
  "lemming",
  "liquidation",
  "lobbyist",
  "love",
  "lozenge",
  "magazine",
  "magnesium",
  "malfunction",
  "marmot",
  "marshmallow",
  "merit",
  "merkin",
  "mescaline",
  "milk",
  "mischief",
  "mistrust",
  "money",
  "monkey",
  "monkeybutter",
  "nationalism",
  "nature",
  "neuron",
  "noise",
  "nomenclature",
  "nutria",
  "OBEY",
  "ocelot",
  "offspring",
  "overseer",
  "pain",
  "pajamas",
  "passenger",
  "passion",
  "Passover",
  "peace",
  "penance",
  "persimmon",
  "petticoat",
  "pharmacist",
  "PhD",
  "pitchfork",
  "plague",
  "Poindexter",
  "politician",
  "pony",
  "presidency",
  "prison",
  "prophecy",
  "Prozac",
  "punishment",
  "punk rock",
  "punk",
  "pussy",
  "quagmire",
  "quarantine",
  "quartz",
  "rabies",
  "radish",
  "rage",
  "readout",
  "reality",
  "rectum",
  "reject",
  "rejection",
  "respect",
  "revolution",
  "roadrunner",
  "rule",
  "savor",
  "scab",
  "scalar",
  "Scandinavia",
  "schadenfreude",
  "security",
  "sediment",
  "self worth",
  "sickness",
  "silicone",
  "slack",
  "slander",
  "slavery",
  "sledgehammer",
  "smegma",
  "smelly socks",
  "sorrow",
  "space program",
  "stamen",
  "standardization",
  "stench",
  "subculture",
  "subversion",
  "suffering",
  "surrender",
  "surveillance",
  "synthesis",
  "television",
  "tenant",
  "tendril",
  "terror",
  "terrorism",
  "terrorist",
  "the impossible",
  "the unknown",
  "toast",
  "topography",
  "truism",
  "turgid",
  "underbrush",
  "underling",
  "unguent",
  "unusual",
  "uplink",
  "urge",
  "valor",
  "variance",
  "vaudeville",
  "vector",
  "vegetarian",
  "venom",
  "verifiability",
  "viagra",
  "vibrator",
  "victim",
  "vignette",
  "villainy",
  "W.A.S.T.E.",
  "wagon",
  "waiver",
  "warehouse",
  "waste",
  "waveform",
  "whiffle ball",
  "whorl",
  "windmill",
  "words",
  "worm",
  "worship",
  "worship",
  "Xanax",
  "Xerxes",
  "Xhosa",
  "xylophone",
  "yellow",
  "yesterday",
  "your nose",
  "Zanzibar",
  "zeal",
  "zebra",
  "zest",
  "zinc"
};

#define WORD_COUNT (sizeof(words) / sizeof(char *))



/* ----------------------------------------------------------------------------
 * bitmap manipulation
 */

/* construct a new bitmap */
Bitmap *makeBitmap (int width, int height)
{
    Bitmap *result = malloc (sizeof (Bitmap));
    result->width = width;
    result->height = height;
    result->widthBytes = (width + 7) / 8;
    result->buf = calloc (1, height * result->widthBytes);
    return result;
}

/* clear a bitmap */
void bitmapClear (Bitmap *b)
{
    memset (b->buf, 0, b->widthBytes * b->height);
}

/* free a bitmap */
void bitmapFree (Bitmap *b)
{
    free (b->buf);
    free (b);
}

/* get the byte value at the given byte-offset coordinates in the given
 * bitmap */
int bitmapGetByte (Bitmap *b, int xByte, int y)
{
    if ((xByte < 0) || 
	(xByte >= b->widthBytes) || 
	(y < 0) || 
	(y >= b->height))
    {
	/* out-of-range get returns 0 */
	return 0;
    }

    return b->buf[b->widthBytes * y + xByte];
}

/* get the bit value at the given coordinates in the given bitmap */
int bitmapGet (Bitmap *b, int x, int y)
{
    int xbyte = x >> 3;
    int xbit = x & 0x7;
    int byteValue = bitmapGetByte (b, xbyte, y);

    return (byteValue & (1 << xbit)) >> xbit;
}

/* set the bit value at the given coordinates in the given bitmap */
void bitmapSet (Bitmap *b, int x, int y, int value)
{
    int xbyte = x >> 3;
    int xbit = x & 0x7;

    if ((x < 0) || 
	(x >= b->width) || 
	(y < 0) || 
	(y >= b->height))
    {
	/* ignore out-of-range set */
	return;
    }

    if (value)
    {
	b->buf[b->widthBytes * y + xbyte] |= 1 << xbit;
    }
    else
    {
	b->buf[b->widthBytes * y + xbyte] &= ~(1 << xbit);
    }
}

/* copy the given rectangle to the given destination from the given source. */
void bitmapCopyRect (Bitmap *dest, int dx, int dy,
		     Bitmap *src, int sx, int sy, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++)
    {
	for (x = 0; x < width; x++)
	{
	    bitmapSet (dest, x + dx, y + dy, bitmapGet (src, x + sx, y + sy));
	}
    }
}

/* draw a vertical line in the given bitmap */
void bitmapVlin (Bitmap *b, int x, int y1, int y2)
{
    while (y1 <= y2)
    {
        bitmapSet (b, x, y1, 1);
        y1++;
    }
}

/* scale a bitmap into another bitmap */
void bitmapScale (Bitmap *dest, Bitmap *src, int mag)
{
    int x, y, x2, y2;

    for (y = 0; y < src->height; y++)
    {
	for (x = 0; x < src->width; x++)
	{
	    int v = bitmapGet (src, x, y);
	    for (x2 = 0; x2 < mag; x2++) 
	    {
		for (y2 = 0; y2 < mag; y2++) 
		{
		    bitmapSet (dest, x * mag + x2, y * mag + y2, v);
		}
	    }
	}
    }
}


/* ----------------------------------------------------------------------------
 * character generation
 */

static unsigned char font5x8Buf[] = 
{
   0x1e, 0x01, 0x06, 0x01, 0x1e, 0x00, 0x1e, 0x01, 0x06, 0x01, 0x1e, 0x00,
   0x1e, 0x01, 0x1e, 0x01, 0x1e, 0x00, 0x01, 0x00, 0x1f, 0x08, 0x04, 0x08,
   0x1f, 0x00, 0x11, 0x1f, 0x11, 0x00, 0x1f, 0x01, 0x01, 0x00, 0x1f, 0x04,
   0x0a, 0x11, 0x00, 0x01, 0x00, 0x0e, 0x11, 0x11, 0x00, 0x0e, 0x11, 0x11,
   0x0e, 0x00, 0x1f, 0x08, 0x04, 0x08, 0x1f, 0x00, 0x44, 0x41, 0x4e, 0x20,
   0x42, 0x4f, 0x52, 0x4e, 0x53, 0x54, 0x45, 0x49, 0x4e, 0x21, 0x21, 0x00,
   0x66, 0x6e, 0x6f, 0x72, 0x64, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00,
   0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00,
   0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00,
   0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00,
   0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00,
   0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00,
   0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00,
   0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x02, 0x00, 0x05, 0x05, 0x05, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x05, 0x05, 0x0f, 0x05, 0x0f, 0x05, 0x05, 0x00,
   0x02, 0x0f, 0x01, 0x0f, 0x08, 0x0f, 0x04, 0x00, 0x0b, 0x0b, 0x08, 0x06,
   0x01, 0x0d, 0x0d, 0x00, 0x03, 0x05, 0x02, 0x05, 0x0d, 0x05, 0x0b, 0x00,
   0x04, 0x04, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x02, 0x02,
   0x02, 0x02, 0x04, 0x00, 0x02, 0x04, 0x04, 0x04, 0x04, 0x04, 0x02, 0x00,
   0x00, 0x09, 0x06, 0x0f, 0x06, 0x09, 0x00, 0x00, 0x00, 0x02, 0x02, 0x07,
   0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x06, 0x00,
   0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x02, 0x00, 0x08, 0x08, 0x04, 0x06, 0x02, 0x01, 0x01, 0x00,
   0x0f, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0f, 0x00, 0x06, 0x04, 0x04, 0x04,
   0x04, 0x04, 0x0f, 0x00, 0x0f, 0x09, 0x08, 0x0f, 0x01, 0x09, 0x0f, 0x00,
   0x0f, 0x08, 0x08, 0x0f, 0x08, 0x08, 0x0f, 0x00, 0x09, 0x09, 0x09, 0x0f,
   0x08, 0x08, 0x08, 0x00, 0x0f, 0x09, 0x01, 0x0f, 0x08, 0x09, 0x0f, 0x00,
   0x03, 0x01, 0x01, 0x0f, 0x09, 0x09, 0x0f, 0x00, 0x0f, 0x09, 0x09, 0x0c,
   0x04, 0x04, 0x04, 0x00, 0x0f, 0x09, 0x09, 0x0f, 0x09, 0x09, 0x0f, 0x00,
   0x0f, 0x09, 0x09, 0x0f, 0x08, 0x08, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00,
   0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x06, 0x00,
   0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x00, 0x00, 0x00, 0x0f, 0x00,
   0x0f, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01, 0x00,
   0x0f, 0x09, 0x08, 0x0e, 0x02, 0x00, 0x02, 0x00, 0x0f, 0x09, 0x0d, 0x0d,
   0x0d, 0x01, 0x0f, 0x00, 0x0f, 0x09, 0x09, 0x0f, 0x09, 0x09, 0x09, 0x00,
   0x07, 0x09, 0x09, 0x07, 0x09, 0x09, 0x07, 0x00, 0x0f, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x0f, 0x00, 0x07, 0x09, 0x09, 0x09, 0x09, 0x09, 0x07, 0x00,
   0x0f, 0x01, 0x01, 0x0f, 0x01, 0x01, 0x0f, 0x00, 0x0f, 0x01, 0x01, 0x0f,
   0x01, 0x01, 0x01, 0x00, 0x0f, 0x01, 0x01, 0x0d, 0x09, 0x09, 0x0f, 0x00,
   0x09, 0x09, 0x09, 0x0f, 0x09, 0x09, 0x09, 0x00, 0x07, 0x02, 0x02, 0x02,
   0x02, 0x02, 0x07, 0x00, 0x0e, 0x04, 0x04, 0x04, 0x04, 0x05, 0x07, 0x00,
   0x09, 0x09, 0x09, 0x07, 0x09, 0x09, 0x09, 0x00, 0x01, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x0f, 0x00, 0x09, 0x0f, 0x0f, 0x0f, 0x09, 0x09, 0x09, 0x00,
   0x09, 0x0b, 0x0d, 0x09, 0x09, 0x09, 0x09, 0x00, 0x0f, 0x09, 0x09, 0x09,
   0x09, 0x09, 0x0f, 0x00, 0x0f, 0x09, 0x09, 0x0f, 0x01, 0x01, 0x01, 0x00,
   0x0f, 0x09, 0x09, 0x09, 0x0b, 0x05, 0x0b, 0x00, 0x07, 0x09, 0x09, 0x07,
   0x09, 0x09, 0x09, 0x00, 0x0f, 0x01, 0x01, 0x0f, 0x08, 0x08, 0x0f, 0x00,
   0x0f, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x09, 0x09, 0x09, 0x09,
   0x09, 0x09, 0x0f, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x05, 0x02, 0x00,
   0x09, 0x09, 0x09, 0x09, 0x0f, 0x0f, 0x09, 0x00, 0x09, 0x09, 0x05, 0x06,
   0x0a, 0x09, 0x09, 0x00, 0x09, 0x09, 0x09, 0x0f, 0x08, 0x08, 0x0f, 0x00,
   0x0f, 0x08, 0x08, 0x06, 0x01, 0x01, 0x0f, 0x00, 0x0e, 0x02, 0x02, 0x02,
   0x02, 0x02, 0x0e, 0x00, 0x01, 0x01, 0x02, 0x06, 0x04, 0x08, 0x08, 0x00,
   0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x00, 0x02, 0x05, 0x05, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00,
   0x02, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x08,
   0x0f, 0x09, 0x0f, 0x00, 0x01, 0x01, 0x0f, 0x09, 0x09, 0x09, 0x0f, 0x00,
   0x00, 0x00, 0x0f, 0x01, 0x01, 0x01, 0x0f, 0x00, 0x08, 0x08, 0x0f, 0x09,
   0x09, 0x09, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x09, 0x0f, 0x01, 0x0f, 0x00,
   0x0e, 0x02, 0x0f, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x0f, 0x09,
   0x09, 0x0f, 0x08, 0x0c, 0x01, 0x01, 0x0f, 0x09, 0x09, 0x09, 0x09, 0x00,
   0x02, 0x00, 0x03, 0x02, 0x02, 0x02, 0x07, 0x00, 0x04, 0x00, 0x04, 0x04,
   0x04, 0x04, 0x05, 0x07, 0x01, 0x01, 0x09, 0x05, 0x03, 0x05, 0x09, 0x00,
   0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00, 0x00, 0x00, 0x09, 0x0f,
   0x0f, 0x09, 0x09, 0x00, 0x00, 0x00, 0x0f, 0x09, 0x09, 0x09, 0x09, 0x00,
   0x00, 0x00, 0x0f, 0x09, 0x09, 0x09, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x09,
   0x09, 0x0f, 0x01, 0x01, 0x00, 0x00, 0x0f, 0x09, 0x09, 0x0f, 0x08, 0x08,
   0x00, 0x00, 0x0f, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x0f, 0x01,
   0x0f, 0x08, 0x0f, 0x00, 0x00, 0x02, 0x0f, 0x02, 0x02, 0x02, 0x0e, 0x00,
   0x00, 0x00, 0x09, 0x09, 0x09, 0x09, 0x0f, 0x00, 0x00, 0x00, 0x09, 0x09,
   0x09, 0x05, 0x02, 0x00, 0x00, 0x00, 0x09, 0x09, 0x0f, 0x0f, 0x09, 0x00,
   0x00, 0x00, 0x09, 0x09, 0x06, 0x09, 0x09, 0x00, 0x00, 0x00, 0x09, 0x09,
   0x09, 0x0f, 0x08, 0x0c, 0x00, 0x00, 0x0f, 0x08, 0x06, 0x01, 0x0f, 0x00,
   0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08, 0x00, 0x02, 0x02, 0x02, 0x02,
   0x02, 0x02, 0x02, 0x00, 0x01, 0x02, 0x02, 0x04, 0x02, 0x02, 0x01, 0x00,
   0x00, 0x0a, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0f, 0x0f, 0x0f, 0x00
};

static Bitmap font5x8 = { 8, 1024, 1, (char *) font5x8Buf };

/* draw the given 5x8 character at the given coordinates */
void bitmapDrawChar5x8 (Bitmap *b, int x, int y, char c)
{
    bitmapCopyRect (b, x, y, &font5x8, 0, c * 8, 5, 8);
}

/* draw a string of 5x8 characters at the given coordinates */
void bitmapDrawString5x8 (Bitmap *b, int x, int y, char *str)
{
    int origx = x;

    while (*str != '\0')
    {
	char c = *str;
	if (c == '\n')
	{
	    x = origx;
	    y += 8;
	}
	else
	{
	    if (c < ' ')
	    {
		c = ' ';
	    }

	    bitmapDrawChar5x8 (b, x, y, c);
	    x += 5;
	}
	str++;
    }
}



/* ----------------------------------------------------------------------------
 * upc/ean symbologies
 */

/* A quick lesson in UPC and EAN barcodes: 
 *
 * Each digit consists of 2 bars and 2 spaces, taking a total width of 7
 * times the width of the thinnest possible bar or space. There are three
 * different possible representations for each digit, used depending on
 * what side of a two-sided barcode the digit is used on, and to encode
 * checksum or other information in some cases. The three forms are
 * related. Taking as the "base" form the pattern as seen on the right-hand
 * side of a UPC-A barcode, the other forms are the inverse of the base
 * (that is, bar becomes space and vice versa) and the mirror image of the
 * base. Still confused? Here's a complete table, where 0 means space and 1
 * means bar:
 *
 *      Left-A   Left-B   Right
 *      -------  -------  -------
 *   0  0001101  0100111  1110010
 *   1  0011001  0110011  1100110
 *   2  0010011  0011011  1101100
 *   3  0111101  0100001  1000010
 *   4  0100011  0011101  1011100
 *   5  0110001  0111001  1001110
 *   6  0101111  0000101  1010000
 *   7  0111011  0010001  1000100
 *   8  0110111  0001001  1001000
 *   9  0001011  0010111  1110100
 *
 * A UPC-A barcode consists of 6 patterns from Left-A on the left-hand side,
 * 6 patterns from Right on the right-hand side, a guard pattern of 01010
 * in the middle, and a guard pattern of 101 on each end. The 12th digit
 * checksum is calculated as follows: Take the 1st, 3rd, ... 11th digits,
 * sum them and multiplying by 3, and add that to the sum of the other digits.
 * Subtract the final digit from 10, and that is the checksum digit. (If
 * the last digit of the sum is 0, then the check digit is 0.)
 *
 * An EAN-13 barcode is just like a UPC-A barcode, except that the characters
 * on the left-hand side have a pattern of Left-A and Left-B that encodes
 * an extra first digit. Note that an EAN-13 barcode with the first digit
 * of 0 is exactly the same as the UPC-A barcode of the rightmost 12 digits.
 * The patterns to encode the first digit are as follows:
 *
 *      Left-Hand 
 *      Digit Position
 *      1 2 3 4 5 6
 *      - - - - - -
 *   0  a a a a a a
 *   1  a a b a b b
 *   2  a a b b a b
 *   3  a a b b b a
 *   4  a b a a b b
 *   5  a b b a a b
 *   6  a b b b a a
 *   7  a b a b a b
 *   8  a b a b b a
 *   9  a b b a b a
 *
 * The checksum for EAN-13 is just like UPC-A, except the 2nd, 4th, ... 12th
 * digits are multiplied by 3 instead of the other way around.
 *
 * An EAN-8 barcode is just like a UPC-A barcode, except there are only 4
 * digits in each half. Unlike EAN-13, there's no nonsense about different
 * left-hand side patterns, either.
 *
 * A UPC-E barcode contains 6 explicit characters between a guard of 101
 * on the left and 010101 on the right. The explicit characters are the
 * middle six characters of the code. The first and last characters are
 * encoded in the parity pattern of the six characters. There are two
 * sets of parity patterns, one to use if the first digit of the number
 * is 0, and another if it is 1. (UPC-E barcodes may only start with a 0
 * or 1.) The patterns are as follows:
 *
 *      First digit 0     First digit 1
 *      Explicit Digit    Explicit Digit
 *      Position          Position
 *      1 2 3 4 5 6       1 2 3 4 5 6
 *      - - - - - -       - - - - - -
 *   0  b b b a a a       a a a b b b
 *   1  b b a b a a       a a b a b b
 *   2  b b a a b a       a a b b a b
 *   3  b b a a a b       a a b b b a
 *   4  b a b b a a       a b a a b b
 *   5  b a a b b a       a b b a a b
 *   6  b a a a b b       a b b b a a
 *   7  b a b a b a       a b a b a b
 *   8  b a b a a b       a b a b b a
 *   9  b a a b a b       a b b a b a
 *
 * (Note that the two sets are the complements of each other. Also note
 * that the first digit 1 patterns are mostly the same as the EAN-13
 * first digit patterns.) The UPC-E check digit (the final digit encoded in
 * the parity pattern) is the same as the UPC-A check digit for the
 * expanded form of the UPC-E number. The expanstion is as follows, based
 * on the last explicit digit (the second to last digit) in the encoded
 * number:
 *
 *               Corresponding
 *   UPC-E form  UPC-A form
 *   ----------  -------------
 *   XABCDE0Y    XAB00000CDEY
 *   XABCDE1Y    XAB10000CDEY
 *   XABCDE2Y    XAB20000CDEY
 *   XABCDE3Y    XABC00000DEY
 *   XABCDE4Y    XABCD00000EY
 *   XABCDE5Y    XABCDE00005Y
 *   XABCDE6Y    XABCDE00006Y
 *   XABCDE7Y    XABCDE00007Y
 *   XABCDE8Y    XABCDE00008Y
 *   XABCDE9Y    XABCDE00009Y 
 *
 * All UPC/EAN barcodes may have an additional 2- or 5-digit supplemental
 * code just to the right of the main barcode. The supplement starts about
 * one digit-length (that is about 7 times the width of the thinnest bar)
 * to the right of the main code, beginning with the guard pattern 1011.
 * After that comes each digit, with a guard pattern of 01 between each,
 * but not at the end. The digits are encoded using the left A and B
 * characters to encode a parity pattern.
 *
 * For 2-digit supplements, the parity pattern is determined by the
 * lower two bits of the numeric value of the code (e.g., 42 would use
 * pattern 2):
 *
 *   Lower 2 bits  Parity Pattern
 *   ------------  --------------
 *   0 (bin 00)    a a
 *   1 (bin 01)    a b
 *   2 (bin 10)    b a
 *   3 (bin 11)    b b
 *
 * For 5-digit supplements, the parity pattern is calculated in a similar
 * manner to check digit calculation: The first, third, and fifth digits
 * are summed and multiplied by 3; the second and fourth digits are summed
 * and multiplied by nine; the parity digit is the sum of those two numbers,
 * modulo 10. The parity pattern is then the last five patterns from the
 * UPC-E final digit 0 table for the corresponding digit.
 */

/* enum to indicate which pattern set to use */
typedef enum
{
    UPC_LEFT_A, UPC_LEFT_B, UPC_RIGHT
}
UpcSet;

/* the Left A patterns */
unsigned int upcLeftA[] = { 
    0x0d, 0x19, 0x13, 0x3d, 0x23, 0x31, 0x2f, 0x3b, 0x37, 0x0b 
};

/* the Left B patterns */
unsigned int upcLeftB[] = { 
    0x27, 0x33, 0x1b, 0x21, 0x1d, 0x39, 0x05, 0x11, 0x09, 0x17
};

/* the Right patterns */
unsigned int upcRight[] = { 
    0x72, 0x66, 0x6c, 0x42, 0x5c, 0x4e, 0x50, 0x44, 0x48, 0x74
};

/* the EAN-13 first-digit patterns */
unsigned int ean13FirstDigit[] = {
    0x00, 0x0b, 0x0d, 0x0e, 0x13, 0x19, 0x1c, 0x15, 0x16, 0x1a
};

/* the UPC-E last-digit patterns for first digit 0 (complement for
 * digit 1); also used for 5-digit supplemental check patterns */
unsigned int upcELastDigit[] = {
    0x38, 0x34, 0x32, 0x31, 0x2c, 0x26, 0x23, 0x2a, 0x29, 0x25
};

/* turn a character into an int representing its digit value; return
 * 0 for things not in the range '0'-'9' */
int charToDigit (char c)
{
    if ((c >= '0') && (c <= '9'))
    {
	return c - '0';
    }
    else
    {
	return 0;
    }
}

/* draw the given digit character at the given coordinates; a '0' is
 * used in place of any non-digit character */
void drawDigitChar (Bitmap *b, int x, int y, char c)
{
  if (mode != BC_CLOCK24 &&
      mode != BC_CLOCK12)
    if ((c < '0') || (c > '9'))
      c = '0';

    bitmapDrawChar5x8 (b, x, y, c);
}

/* draw a upc/ean digit at the given coordinates */
void drawUpcEanDigit (Bitmap *upcBitmap, int x, int y1, int y2, char n, 
		      UpcSet set)
{
    unsigned int bits;
    int i;
    
    n = charToDigit (n);
    switch (set)
    {
	case UPC_LEFT_A: 
	    bits = upcLeftA[(int) n];
	    break;
	case UPC_LEFT_B: 
	    bits = upcLeftB[(int) n];
	    break;
        default /* case UPC_RIGHT */:
	    bits = upcRight[(int) n];
	    break;
    }

    for (i = 6; i >=0; i--)
    {
        if (bits & (1 << i))
	{
            bitmapVlin (upcBitmap, x, y1, y2);
	}
        x++;
    }
}

/* report the width of the given supplemental code or 0 if it is a bad
 * supplement form */
int upcEanSupplementWidth (char *digits)
{
    switch (strlen (digits))
    {
	case 2: return 28; /* 8 + 4 + 2*7 + 1*2 */
	case 5: return 55; /* 8 + 4 + 5*7 + 4*2 */
	default: return 0;
    }
}

/* draw the given supplemental barcode, including the textual digits */
void drawUpcEanSupplementalBars (Bitmap *upcBitmap, char *digits, 
				 int x, int y, int y2, int textAbove)
{
    int len = strlen (digits);
    int i;
    int parity;
    int textY;
    int textX;

    if (textAbove)
    {
	textY = y;
	y += 8;
    }
    else
    {
	y2 -= 8;
	textY = y2 + 2;
    }

    x += 8; /* skip the space between the main and supplemental */

    switch (len)
    {
	case 2: 
	{
	    textX = x + 5;
	    parity = (charToDigit (digits[0]) * 10 + 
		      charToDigit (digits[1])) & 0x3;
	    break;
	}
	case 5:
	{
	    textX = x + 10;
	    parity = 
		((charToDigit (digits[0]) + charToDigit (digits[2]) + 
		  charToDigit (digits[4])) * 3
		 + (charToDigit (digits[1]) + charToDigit (digits[3])) * 9)
		% 10;
	    parity = upcELastDigit[parity];
	    break;
	}
	default:
	{
	    fprintf (stderr, "%s: bad supplement (%d digits)\n",
                     progname, len);
	    exit(1);
	    break;
	}
    }

    /* header */
    bitmapVlin (upcBitmap, x, y, y2);
    bitmapVlin (upcBitmap, x + 2, y, y2);
    bitmapVlin (upcBitmap, x + 3, y, y2);

    for (i = 0; i < len; i++)
    {
	UpcSet lset = 
	    (parity & (1 << (len - 1 - i))) ? UPC_LEFT_B : UPC_LEFT_A;
	int baseX = x + 2 + i * 9;

	/* separator / end of header */
	if (i == 0)
	{
	    bitmapVlin (upcBitmap, baseX, y, y2);
	}
	bitmapVlin (upcBitmap, baseX + 1, y, y2);

        drawUpcEanDigit (upcBitmap,
			 baseX + 2, 
			 y,
			 y2,
			 digits[i], 
			 lset);

        drawDigitChar (upcBitmap, textX + i*6, textY, digits[i]);
    }
}

/* draw the actual barcode part of a UPC-A barcode */
void drawUpcABars (Bitmap *upcBitmap, char *digits, int x, int y, 
		   int barY2, int guardY2)
{
    int i;

    /* header */
    bitmapVlin (upcBitmap, x, y, guardY2);
    bitmapVlin (upcBitmap, x + 2, y, guardY2);

    /* center marker */
    bitmapVlin (upcBitmap, x + 46, y, guardY2);
    bitmapVlin (upcBitmap, x + 48, y, guardY2);

    /* trailer */
    bitmapVlin (upcBitmap, x + 92, y, guardY2);
    bitmapVlin (upcBitmap, x + 94, y, guardY2);

    for (i = 0; i < 6; i++)
    {
        drawUpcEanDigit (upcBitmap,
			 x + 3 + i*7, 
			 y,
			 (i == 0) ? guardY2 : barY2,
			 digits[i], 
			 UPC_LEFT_A);
        drawUpcEanDigit (upcBitmap,
			 x + 50 + i*7, 
			 y, 
			 (i == 5) ? guardY2 : barY2,
			 digits[i+6], 
			 UPC_RIGHT);
    }
}

/* make and return a full-height UPC-A barcode */
int makeUpcAFull (Bitmap *dest, char *digits, int y)
{
    static int baseWidth = 108;
    static int baseHeight = 60;

    int height = baseHeight + y;
    int i;

    bitmapClear (dest);
    drawUpcABars (dest, digits, 6, y, height - 10, height - 4);

    drawDigitChar (dest, 0, height - 14, digits[0]);

    for (i = 0; i < 5; i++)
    {
        drawDigitChar (dest, 18 + i*7, height - 7, digits[i+1]);
        drawDigitChar (dest, 57 + i*7, height - 7, digits[i+6]);
    }

    drawDigitChar (dest, 103, height - 14, digits[11]);

    return baseWidth;
}

/* make and return a UPC-A barcode */
int makeUpcA (Bitmap *dest, char *digits, int y)
{
    int i;
    unsigned int mul = 3;
    unsigned int sum = 0;

    for (i = 0; i < 11; i++)
    {
	sum += charToDigit (digits[i]) * mul;
	mul ^= 2;
    }

    if (digits[11] == '?')
    {
        digits[11] = ((10 - (sum % 10)) % 10) + '0';
    }

    return makeUpcAFull (dest, digits, y);
}

/* draw the actual barcode part of a UPC-E barcode */
void drawUpcEBars (Bitmap *upcBitmap, char *digits, int x, int y, 
		   int barY2, int guardY2)
{
    int i;
    int parityPattern = upcELastDigit[charToDigit(digits[7])];

    int clockp = (mode == BC_CLOCK12 || mode == BC_CLOCK24);

    if (digits[0] == '1')
    {
	parityPattern = ~parityPattern;
    }

    /* header */
    bitmapVlin (upcBitmap, x,     y, guardY2);
    bitmapVlin (upcBitmap, x + 2, y, guardY2);

    /* trailer */
    bitmapVlin (upcBitmap, x + 46 + (clockp?8:0), y, guardY2);
    bitmapVlin (upcBitmap, x + 48 + (clockp?8:0), y, guardY2);
    bitmapVlin (upcBitmap, x + 50 + (clockp?8:0), y, guardY2);

    /* clock kludge -- this draws an extra set of dividers after
       digits 2 and 4.  This makes this *not* be a valid bar code,
       but, it looks pretty for the clock display.
     */
    if (clockp)
      {
        bitmapVlin (upcBitmap, x + 18,     y, guardY2);
        bitmapVlin (upcBitmap, x + 18 + 2, y, guardY2);

        bitmapVlin (upcBitmap, x + 36,     y, guardY2);
        bitmapVlin (upcBitmap, x + 36 + 2, y, guardY2);
      }

    for (i = 0; i < 6; i++)
    {
	UpcSet lset = 
	    (parityPattern & (1 << (5 - i))) ? UPC_LEFT_B : UPC_LEFT_A;
        int off = (clockp
                   ? (i < 2 ? 0 :
                      i < 4 ? 4 :      /* extra spacing for clock bars */
                              8)
                   : 0);
        drawUpcEanDigit (upcBitmap,
			 x + 3 + i*7 + off,
			 y,
			 barY2,
			 digits[i + 1], 
			 lset);
    }
}

/* make and return a full-height UPC-E barcode */
int makeUpcEFull (Bitmap *dest, char *digits, int y)
{
    static int baseWidth = 64;
    static int baseHeight = 60;

    int height = baseHeight + y;
    int i;

    bitmapClear (dest);
    drawUpcEBars (dest, digits, 6, y, height - 10, height - 4);

    drawDigitChar (dest, 0, height - 14, digits[0]);

    for (i = 0; i < 6; i++)
    {
        drawDigitChar (dest, 11 + i*7, height - 7, digits[i+1]);
    }

    drawDigitChar (dest, 59, height - 14, digits[7]);

    return baseWidth;
}

/* expand 8 UPC-E digits into a UPC-A number, storing into the given result
 * array, or just store '\0' into the first element, if the form factor
 * is incorrect; this will also calculate the check digit, if it is
 * specified as '?' */
void expandToUpcADigits (char *compressed, char *expanded)
{
    int i;

    if ((compressed[0] != '0') && (compressed[0] != '1'))
    {
	return;
    }

    expanded[0] = compressed[0];
    expanded[6] = '0';
    expanded[7] = '0';
    expanded[11] = compressed[7];

    switch (compressed[6])
    {
	case '0':
	case '1':
	case '2':
	{
	    expanded[1] = compressed[1];
	    expanded[2] = compressed[2];
	    expanded[3] = compressed[6];
	    expanded[4] = '0';
	    expanded[5] = '0';
	    expanded[8] = compressed[3];
	    expanded[9] = compressed[4];
	    expanded[10] = compressed[5];
	    break;
	}
	case '3':
	{
	    expanded[1] = compressed[1];
	    expanded[2] = compressed[2];
	    expanded[3] = compressed[3];
	    expanded[4] = '0';
	    expanded[5] = '0';
	    expanded[8] = '0';
	    expanded[9] = compressed[4];
	    expanded[10] = compressed[5];
	    break;
	}
	case '4':
	{
	    expanded[1] = compressed[1];
	    expanded[2] = compressed[2];
	    expanded[3] = compressed[3];
	    expanded[4] = compressed[4];
	    expanded[5] = '0';
	    expanded[8] = '0';
	    expanded[9] = '0';
	    expanded[10] = compressed[5];
	    break;
	}
	default:
	{
	    expanded[1] = compressed[1];
	    expanded[2] = compressed[2];
	    expanded[3] = compressed[3];
	    expanded[4] = compressed[4];
	    expanded[5] = compressed[5];
	    expanded[8] = '0';
	    expanded[9] = '0';
	    expanded[10] = compressed[6];
	    break;
	}
    }

    if (expanded[11] == '?')
    {
	unsigned int mul = 3;
	unsigned int sum = 0;

	for (i = 0; i < 11; i++)
	{
	    sum += charToDigit (expanded[i]) * mul;
	    mul ^= 2;
	}

        expanded[11] = ((10 - (sum % 10)) % 10) + '0';
    }
}

/* make and return a UPC-E barcode */
int makeUpcE (Bitmap *dest, char *digits, int y)
{
    char expandedDigits[13];
    char compressedDigits[9];

    expandedDigits[0] = '\0';
    compressedDigits[0] = '0';
    strcpy (compressedDigits + 1, digits);

    expandToUpcADigits (compressedDigits, expandedDigits);
    if (expandedDigits[0] == '\0')
    {
	return 0;
    }
    
    compressedDigits[7] = expandedDigits[11];

    return makeUpcEFull (dest, compressedDigits, y);
}

/* draw the actual barcode part of a EAN-13 barcode */
void drawEan13Bars (Bitmap *upcBitmap, char *digits, int x, int y, 
		   int barY2, int guardY2)
{
    int i;
    int leftPattern = ean13FirstDigit[charToDigit (digits[0])];

    /* header */
    bitmapVlin (upcBitmap, x, y, guardY2);
    bitmapVlin (upcBitmap, x + 2, y, guardY2);

    /* center marker */
    bitmapVlin (upcBitmap, x + 46, y, guardY2);
    bitmapVlin (upcBitmap, x + 48, y, guardY2);

    /* trailer */
    bitmapVlin (upcBitmap, x + 92, y, guardY2);
    bitmapVlin (upcBitmap, x + 94, y, guardY2);

    for (i = 0; i < 6; i++)
    {
	UpcSet lset = (leftPattern & (1 << (5 - i))) ? UPC_LEFT_B : UPC_LEFT_A;

        drawUpcEanDigit (upcBitmap,
			 x + 3 + i*7, 
			 y,
			 barY2,
			 digits[i+1], 
			 lset);
        drawUpcEanDigit (upcBitmap,
			 x + 50 + i*7, 
			 y, 
			 barY2,
			 digits[i+7], 
			 UPC_RIGHT);
    }
}

/* make and return a full-height EAN-13 barcode */
int makeEan13Full (Bitmap *dest, char *digits, int y)
{
    static int baseWidth = 102;
    static int baseHeight = 60;

    int height = baseHeight + y;
    int i;

    bitmapClear (dest);
    drawEan13Bars (dest, digits, 6, y, height - 10, height - 4);

    drawDigitChar (dest, 0, height - 7, digits[0]);

    for (i = 0; i < 6; i++)
    {
        drawDigitChar (dest, 11 + i*7, height - 7, digits[i+1]);
        drawDigitChar (dest, 57 + i*7, height - 7, digits[i+7]);
    }

    return baseWidth;
}

/* make and return an EAN-13 barcode */
int makeEan13 (Bitmap *dest, char *digits, int y)
{
    int i;
    unsigned int mul = 1;
    unsigned int sum = 0;

    for (i = 0; i < 12; i++)
    {
	sum += charToDigit (digits[i]) * mul;
	mul ^= 2;
    }

    if (digits[12] == '?')
    {
        digits[12] = ((10 - (sum % 10)) % 10) + '0';
    }

    return makeEan13Full (dest, digits, y);
}

/* draw the actual barcode part of an EAN-8 barcode */
void drawEan8Bars (Bitmap *upcBitmap, char *digits, int x, int y, 
		   int barY2, int guardY2)
{
    int i;

    /* header */
    bitmapVlin (upcBitmap, x, y, guardY2);
    bitmapVlin (upcBitmap, x + 2, y, guardY2);

    /* center marker */
    bitmapVlin (upcBitmap, x + 32, y, guardY2);
    bitmapVlin (upcBitmap, x + 34, y, guardY2);

    /* trailer */
    bitmapVlin (upcBitmap, x + 64, y, guardY2);
    bitmapVlin (upcBitmap, x + 66, y, guardY2);

    for (i = 0; i < 4; i++)
    {
        drawUpcEanDigit (upcBitmap,
			 x + 3 + i*7, 
			 y,
			 barY2,
			 digits[i], 
			 UPC_LEFT_A);
        drawUpcEanDigit (upcBitmap,
			 x + 36 + i*7, 
			 y, 
			 barY2,
			 digits[i+4], 
			 UPC_RIGHT);
    }
}

/* make and return a full-height EAN-8 barcode */
int makeEan8Full (Bitmap *dest, char *digits, int y)
{
    static int baseWidth = 68;
    static int baseHeight = 60;

    int height = baseHeight + y;
    int i;

    bitmapClear (dest);
    drawEan8Bars (dest, digits, 0, y, height - 10, height - 4);

    for (i = 0; i < 4; i++)
    {
        drawDigitChar (dest, 5 + i*7, height - 7, digits[i]);
        drawDigitChar (dest, 37 + i*7, height - 7, digits[i+4]);
    }

    return baseWidth;
}

/* make and return an EAN-8 barcode */
int makeEan8 (Bitmap *dest, char *digits, int y)
{
    int i;
    unsigned int mul = 3;
    unsigned int sum = 0;

    for (i = 0; i < 7; i++)
    {
	sum += charToDigit (digits[i]) * mul;
	mul ^= 2;
    }

    if (digits[7] == '?')
    {
        digits[7] = ((10 - (sum % 10)) % 10) + '0';
    }

    return makeEan8Full (dest, digits, y);
}

/* Dispatch to the right form factor UPC/EAN barcode generator */
void processUpcEan (char *str, Bitmap *dest)
{
    char digits[16];
    int digitCount = 0;
    char supDigits[8];
    int supDigitCount = 0;
    char *instr = str;
    char *banner = NULL; 
    int supplement = 0;
    int vstart = 9;
    int width = 0;

    while ((digitCount < 15) && (supDigitCount < 7))
    {
	char c = *instr;
        if (((c >= '0') && (c <= '9')) || (c == '?'))
        {
	    if (supplement)
	    {
		supDigits[supDigitCount] = *instr;
		supDigitCount++;
	    }
	    else
	    {
		digits[digitCount] = *instr;
		digitCount++;
	    }
        }
	else if (c == ',')
	{
	    supplement = 1;
	}
	else if (c == ':')
	{
	    banner = instr + 1;
	    break;
	}
	else if (c == '\0')
	{
	    break;
	}
	instr++;
    }

    digits[digitCount] = '\0';
    supDigits[supDigitCount] = '\0';

    if (supDigitCount == 0)
    {
	supplement = 0;
    }
    else if ((supDigitCount == 2) || (supDigitCount == 5))
    {
	supplement = upcEanSupplementWidth (supDigits);
    }
    else
    {
	fprintf (stderr, "%s: invalid supplement (must be 2 or 5 digits)\n",
                 progname);
	exit (1);
    }

    if (banner == NULL) 
    {
	banner = "barcode";
    }

    switch (digitCount)
    {
	case 7: 
	{
	    width = makeUpcE (dest, digits, vstart);
	    break;
	}
	case 8: 
	{
	    width = makeEan8 (dest, digits, vstart);
	    break;
	}
	case 12: 
	{
	    width = makeUpcA (dest, digits, vstart);
	    break;
	}
	case 13:
	{
	    width = makeEan13 (dest, digits, vstart);
	    break;
	}
	default:
	{
	    fprintf (stderr, "%s: bad barcode (%d digits)\n",
                     progname, digitCount);
	    exit(1);
	}
    }

    if (supplement)
    {
	drawUpcEanSupplementalBars (dest, supDigits,
				    width,
				    vstart + 1, dest->height - 4, 1);
    }

    if (banner != NULL)
    {
	bitmapDrawString5x8 (dest, 
			     (width + supplement -
			      ((int) strlen (banner) * 5)) / 2,
			     0,
			     banner);
    }
}



/* ----------------------------------------------------------------------------
 * the screenhack
 */

/*
 * overall setup stuff
 */

/* set up the system */
static void setup (void)
{
    XWindowAttributes xgwa;
    XGCValues gcv;

    XGetWindowAttributes (display, window, &xgwa);

    screen = xgwa.screen;
    visual = xgwa.visual;
    cmap = xgwa.colormap;
    windowWidth = xgwa.width;
    windowHeight = xgwa.height;

    gcv.background = get_pixel_resource ("background", "Background",
					 display, xgwa.colormap);
    gcv.foreground = get_pixel_resource ("foreground", "Foreground",
					 display, xgwa.colormap);
    fg_pixel = gcv.foreground;
    theGC = XCreateGC (display, window, GCForeground|GCBackground, &gcv);

    theBitmap = makeBitmap(BARCODE_WIDTH * MAX_MAG, BARCODE_HEIGHT * MAX_MAG);
    theImage = XCreateImage(display, visual, 1, XYBitmap, 0, theBitmap->buf,
			    theBitmap->width, theBitmap->height, 8,
			    theBitmap->widthBytes);
    theImage->bitmap_bit_order = LSBFirst;
    theImage->byte_order       = LSBFirst;
}



/*
 * the simulation
 */

/* set up the model */
static void setupModel (void)
{
    int i;

    barcode_max = 20;
    barcode_count = 0;
    barcodes = malloc (sizeof (Barcode) * barcode_max);

    for (i = 0; i < barcode_max; i++)
    {
	barcodes[i].bitmap = makeBitmap(BARCODE_WIDTH * MAX_MAG, 
					BARCODE_HEIGHT * MAX_MAG);
    }
}

/* make a new barcode string */
static void makeBarcodeString (char *str)
{
    int dig, i;

    switch ((int) (RAND_FLOAT_01 * 4))
    {
	case 0:  dig = 6;  break;
	case 1:  dig = 7;  break;
	case 2:  dig = 11; break;
	default: dig = 12; break;
    }

    for (i = 0; i < dig; i++)
    {
	str[i] = RAND_FLOAT_01 * 10 + '0';
    }

    str[i] = '?';
    i++;

    switch ((int) (RAND_FLOAT_01 * 3))
    {
	case 0:  dig = 0; break;
	case 1:  dig = 2; break;
	default: dig = 5; break;
    }

    if (dig != 0)
    {
	str[i] = ',';
	i++;
	while (dig > 0) 
	{
	    str[i] = RAND_FLOAT_01 * 10 + '0';
	    i++;
	    dig--;
	}
    }

    str[i] = ':';
    i++;

    strcpy(&str[i], words[(int) (RAND_FLOAT_01 * WORD_COUNT)]);
}

/* update the model for one iteration */
static void scrollModel (void)
{
    int i;

    for (i = 0; i < barcode_count; i++) 
    {
	Barcode *b = &barcodes[i];
	b->x--;
	if ((b->x + BARCODE_WIDTH * b->mag) < 0) 
	{
	    /* fell off the edge */
	    if (i != (barcode_count - 1)) {
		Bitmap *oldb = b->bitmap;
		memmove (b, b + 1, (barcode_count - i - 1) * sizeof (Barcode));
		barcodes[barcode_count - 1].bitmap = oldb;

                XFreeColors (display, cmap, &b->pixel, 1, 0);
	    }

	    i--;
	    barcode_count--;
	}
    }

    while (barcode_count < barcode_max)
    {
	Barcode *barcode = &barcodes[barcode_count];
	barcode->x = (barcode_count == 0) ? 
	    0 : 
	    (barcodes[barcode_count - 1].x + 
	     barcodes[barcode_count - 1].mag * BARCODE_WIDTH);
	barcode->x += RAND_FLOAT_01 * 100;
	barcode->mag = RAND_FLOAT_01 * MAX_MAG;
	barcode->y =
	    RAND_FLOAT_01 * (windowHeight - BARCODE_HEIGHT * barcode->mag);
	if (barcode->y < 0) 
	{
	    barcode->y = 0;
	}
	makeBarcodeString(barcode->code);
	processUpcEan (barcode->code, theBitmap);
	bitmapScale (barcode->bitmap, theBitmap, barcode->mag);

        {
          XColor c;
          int i, ok = 0;
          for (i = 0; i < 100; i++)
            {
              hsv_to_rgb (random() % 360, 1.0, 1.0, &c.red, &c.green, &c.blue);
              ok = XAllocColor (display, cmap, &c);
              if (ok) break;
            }
          if (!ok)
            {
              c.red = c.green = c.blue = 0xFFFF;
              if (!XAllocColor (display, cmap, &c))
                abort();
            }
          barcode->pixel = c.pixel;
        }

	barcode_count++;
    }
}

/* update the model for one iteration */
static void updateGrid (void)
{
    int i, x, y;
    static int grid_w = 0;
    static int grid_h = 0;

    static unsigned long pixel;
    static int alloced_p = 0;

    static char *strings[200] = { 0, };

    if (grid_w == 0 || grid_h == 0 ||
        (! (random() % 400)))
      {
        XClearWindow (display, window);
        grid_w = 1 + (random() % 3);
        grid_h = 1 + (random() % 4);
      }

    if (!alloced_p || (! (random() % 100)))
      {
        XColor c;
        hsv_to_rgb (random() % 360, 1.0, 1.0, &c.red, &c.green, &c.blue);
        if (alloced_p)
          XFreeColors (display, cmap, &pixel, 1, 0);
        XAllocColor (display, cmap, &c);
        pixel = c.pixel;
        alloced_p = 1;
      }

    barcode_count = grid_w * grid_h;
    if (barcode_count > barcode_max) abort();

    for (i = 0; i < barcode_max; i++)
      {
        Barcode *b = &barcodes[i];
        b->x = b->y = 999999;
      }

    i = 0;
    for (y = 0; y < grid_h; y++)
      for (x = 0; x < grid_w; x++, i++)
        {
          Barcode *b = &barcodes[i];
          int digits = 12;

          int cell_w = (windowWidth  / grid_w);
          int cell_h = (windowHeight / grid_h);
          int mag_x  = cell_w / BARCODE_WIDTH;
          int mag_y  = cell_h / BARCODE_HEIGHT;
          int BW = 108 /*BARCODE_WIDTH*/;
          int BH = BARCODE_HEIGHT;

          b->mag = (mag_x < mag_y ? mag_x : mag_y);

          b->x = (x * cell_w) + ((cell_w - b->mag * BW) / 2);
          b->y = (y * cell_h) + ((cell_h - b->mag * BH) / 2);
          b->pixel = pixel;

          if (!strings[i])
            {
              int j;
              char *s = malloc (digits + 10);
              strings[i] = s;
              for (j = 0; j < digits; j++)
                s[j] = (random() % 10) + '0';
              s[j++] = '?';
              s[j++] = ':';
              s[j++] = 0;
            }

          /* change one digit in this barcode */
          strings[i][random() % digits] = (random() % 10) + '0';

          strcpy (b->code, strings[i]);
          processUpcEan (b->code, b->bitmap);
        }
}


/* update the model for one iteration.
   This one draws a clock.  By jwz.  */
static void updateClock (void)
{
  Barcode *b = &barcodes[0];
  int BW = 76 /* BARCODE_WIDTH  */;
  int BH = BARCODE_HEIGHT;
  int mag_x, mag_y;
  int i;
  time_t now = time ((time_t *) 0);
  struct tm *tm = localtime (&now);
  XWindowAttributes xgwa;
  int ow = windowWidth;
  int oh = windowHeight;

  XGetWindowAttributes (display, window, &xgwa);
  windowWidth = xgwa.width;
  windowHeight = xgwa.height;

  mag_x  = windowWidth  / BW;
  mag_y  = windowHeight / BH;

  barcode_count = 1;

  b->mag = (mag_x < mag_y ? mag_x : mag_y);

  if (b->mag > MAX_MAG) b->mag = MAX_MAG;
  if (b->mag < 1) b->mag = 1;

  b->x = (windowWidth  - (b->mag * BW      )) / 2;
  b->y = (windowHeight - (b->mag * (BH + 9))) / 2;
  b->pixel = fg_pixel;

  if (!button_down_p)
    sprintf (b->code, "0%02d%02d%02d?:",
             (mode == BC_CLOCK24
              ? tm->tm_hour
              : (tm->tm_hour > 12
                 ? tm->tm_hour - 12
                 : (tm->tm_hour == 0
                    ? 12
                    : tm->tm_hour))),
             tm->tm_min,
             tm->tm_sec);
  else
    sprintf (b->code, "0%02d%02d%02d?:",
             tm->tm_year % 100, tm->tm_mon+1, tm->tm_mday);

  {
    int vstart = 9;
    int hh = BH + vstart;
    char expandedDigits[13];

    expandedDigits[0] = '\0';

    expandToUpcADigits (b->code, expandedDigits);
    if (expandedDigits[0] != '\0')
      b->code[7] = expandedDigits[11];

    bitmapClear (theBitmap);
    drawUpcEBars (theBitmap, b->code, 6, 9, 59, 65);
    for (i = 0; i < 6; i++)
      {
        int off = (i < 2 ? 0 :
                   i < 4 ? 4 :
                   8);
        drawDigitChar (theBitmap, 11 + i*7 + off, hh - 16, b->code[i+1]);
      }

    if (!button_down_p)
      {
#if 0
        char *days[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
        char *s = days[tm->tm_wday];
        bitmapDrawString5x8 (theBitmap, (BW - strlen (s)*5) / 2, 0, s);
#endif
        drawDigitChar (theBitmap,  0, hh - 23, (tm->tm_hour < 12 ? 'A' : 'P'));
        drawDigitChar (theBitmap, 68, hh - 23, 'M');
      }
    else
      {
        char s[20];
        sprintf (s, "%03d", tm->tm_yday);
        bitmapDrawString5x8 (theBitmap, (BW - strlen (s)*5) / 2, 0, s);
      }
  }

  bitmapScale (b->bitmap, theBitmap, b->mag);

  if (ow != windowWidth || oh != windowHeight)
    XClearWindow (display, window);
}



/* render and display the current model */
static void renderFrame (void)
{
    int i;

    for (i = 0; i < barcode_count; i++)
    {
	Barcode *barcode = &barcodes[i];

	if (barcode->x > windowWidth) {
	    break;
	}

	/* bitmapScale (theBitmap, barcode->bitmap, barcode->mag);*/
	theImage->data = barcode->bitmap->buf;

        XSetForeground (display, theGC, barcode->pixel);
	XPutImage (display, window, theGC, theImage, 
		   0, 0, barcode->x, barcode->y, 
		   BARCODE_WIDTH * barcode->mag,
		   BARCODE_HEIGHT * barcode->mag);
    }
}

/* do one iteration */
static void oneIteration (void)
{
    if (mode == BC_SCROLL)
      scrollModel ();
    else if (mode == BC_GRID)
      updateGrid ();
    else if (mode == BC_CLOCK12 || mode == BC_CLOCK24)
      updateClock ();
    else
      abort();

    renderFrame ();
}


static void barcode_handle_events (Display *dpy)
{
  int clockp = (mode == BC_CLOCK12 || mode == BC_CLOCK24);
  while (XPending (dpy))
    {
      XEvent event;
      XNextEvent (dpy, &event);
      if (clockp && event.xany.type == ButtonPress)
        button_down_p = True;
      else if (clockp && event.xany.type == ButtonRelease)
        button_down_p = False;
      else
        screenhack_handle_event (dpy, &event);
    }
}


/* main and options and stuff */

char *progclass = "Barcode";

char *defaults [] = {
    ".background:	black",
    ".foreground:	green",
    "*delay:		10000",
    0
};

XrmOptionDescRec options [] = {
  { "-delay",            ".delay",          XrmoptionSepArg, 0 },
  { "-scroll",           ".mode",           XrmoptionNoArg, "scroll"  },
  { "-grid",             ".mode",           XrmoptionNoArg, "grid"    },
  { "-clock",            ".mode",           XrmoptionNoArg, "clock"   },
  { "-clock12",          ".mode",           XrmoptionNoArg, "clock12" },
  { "-clock24",          ".mode",           XrmoptionNoArg, "clock24" },
  { 0, 0, 0, 0 }
};

/* initialize the user-specifiable params */
static void initParams (void)
{
    int problems = 0;
    char *s;

    delay = get_integer_resource ("delay", "Delay");
    if (delay < 0)
    {
	fprintf (stderr, "%s: delay must be at least 0\n", progname);
	problems = 1;
    }

    s = get_string_resource ("mode", "Mode");
    if (!s || !*s || !strcasecmp (s, "scroll"))
      mode = BC_SCROLL;
    else if (!strcasecmp (s, "grid"))
      mode = BC_GRID;
    else if (!strcasecmp (s, "clock") ||
             !strcasecmp (s, "clock12"))
      mode = BC_CLOCK12;
    else if (!strcasecmp (s, "clock24"))
      mode = BC_CLOCK24;
    else
      {
	fprintf (stderr, "%s: unknown mode \"%s\"\n", progname, s);
	problems = 1;
      }
    free (s);

    if (mode == BC_CLOCK12 || mode == BC_CLOCK24)
      delay = 10000;  /* only update every 1/10th second */

    if (problems)
    {
	exit (1);
    }
}

/* main function */
void screenhack (Display *dpy, Window win)
{
    display = dpy;
    window = win;

    initParams ();
    setup ();
    setupModel ();

    for (;;) 
    {
	oneIteration ();
        XSync (dpy, False);
        barcode_handle_events (dpy);
	usleep (delay);
    }
}
