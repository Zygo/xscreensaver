/* xscreensaver, Copyright © 2014-2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_JWXYZ
# include "jwxyz.h"
#else /* !HAVE_JWXYZ */
# include <X11/Xlib.h>
#endif

#include "utf8wc.h"


/* "Unicode Replacement Character", displayed in lieu of invalid characters. */
# define INVALID 0xFFFD


/* Mask the number to be within the valid range of unicode characters.
 */
static unsigned long
uc_truncate (unsigned long uc)
{
  uc &= 0x7FFFFFFFL;			/* Unicode is 31 bits */
  if (uc > 0x10FFFF) uc = INVALID;	/* But UTF-8 is 4 bytes */
  if (uc == 0) uc = INVALID;		/* no nulls */

  if (uc >= 0xD800 && uc <= 0xDFFF)
    /* Reserved for use with UTF-16: not a real character. */
    uc = INVALID;

  return uc;
}


/* Parse the first UTF8 character at the front of the string.
   Return the Unicode character, and the number of bytes read.
 */
long
utf8_decode (const unsigned char *in, long length, unsigned long *unicode_ret)
{
  const unsigned char *start = in;
  const unsigned char *end = in + length;
  unsigned long uc = INVALID;
  unsigned long min = 0;
  unsigned char c;

  if (length <= 0) goto DONE;

  c = *in++;

# define PREMATURE_EOF { in = end; goto DONE; }

  if ((c & 0xC0) == 0x80) {        /* 10xxxxxx - lonely continuation byte */
    uc = INVALID;

  } else if ((c & 0x80) == 0) {    /* 0xxxxxxx - 7 bits in 1 byte */
    uc = (c & 0x7F);               /* 01111111 */

  } else if ((c & 0xE0) == 0xC0) { /* 110xxxxx - 11 bits in 2 bytes */
    if (in+1 > end) PREMATURE_EOF;
    min = 1 << 7;
    uc = (((c    & 0x1F) << 6) |   /* 00011111------ */
          (in[0] & 0x3F));         /*       00111111 */
    in += 1;

  } else if ((c & 0xF0) == 0xE0) { /* 1110xxxx - 16 bits in 3 bytes */
    if (in+2 > end) PREMATURE_EOF;
    min = 1 << 11;
    uc = (((c     & 0x0F) << 12) | /* 00001111----+------- */
          ((in[0] & 0x3F) <<  6) | /*       00111111------ */
          ((in[1] & 0x3F)));       /*             00111111 */
    in += 2;

  } else if ((c & 0xF8) == 0xF0) { /* 11110xxx - 21 bits in 4 bytes */
    if (in+3 > end) PREMATURE_EOF;
    min = 1 << 16;
    uc = (((c     & 0x07) << 18) | /* 00000111--+-------+------- */
          ((in[0] & 0x3F) << 12) | /*       00111111----+------- */
          ((in[1] & 0x3F) <<  6) | /*             00111111------ */
          ((in[2] & 0x3F)));       /*                   00111111 */
    in += 3;

  } else if ((c & 0xFC) == 0xF8) { /* 111110xx - 26 bits in 5 bytes */
    if (in+4 > end) PREMATURE_EOF;
    min = 1 << 21;
    uc = (((c     & 0x03) << 24) | /* 00000011--------+-------+------- */
          ((in[0] & 0x3F) << 18) | /*       00111111--+-------+------- */
          ((in[1] & 0x3F) << 12) | /*             00111111----+------- */
          ((in[2] & 0x3F) << 6)  | /*                   00111111------ */
          ((in[3] & 0x3F)));       /*                         00111111 */
    in += 4;

  } else if ((c & 0xFE) == 0xFC) { /* 1111110x - 31 bits in 6 bytes */
    if (in+5 > end) PREMATURE_EOF;
    min = 1 << 26;
    uc = (((c     & 0x01) << 30) | /* 00000001------+-------+-------+------- */
          ((in[0] & 0x3F) << 24) | /*       00111111+-------+-------+------- */
          ((in[1] & 0x3F) << 18) | /*             00111111--+-------+------- */
          ((in[2] & 0x3F) << 12) | /*                   00111111----+------- */
          ((in[3] & 0x3F) << 6)  | /*                         00111111------ */
          ((in[4] & 0x3F)));       /*                               00111111 */
    in += 5;
  } else {
    uc = INVALID;		   /* Unparsable sequence. */
  }

 DONE:

  length = in - start;

  /* If any of the continuation bytes didn't begin with the continuation tag,
     the sequence is invalid; stop at the bad byte, not consuming later ones.
     (It's easier to check this after the fact than up above.) */
  {
    int i;
    for (i = 1; i < length; i++)
      if ((start[i] & 0xC0) != 0x80) {
        uc = INVALID;
        length = i+1;
        break;
      }
  }

  if (uc < min)
    /* A multi-byte sequence encoded a character that could have been
       encoded with a shorter sequence, e.g., hiding ASCII inside a
       multi-byte sequence. Something hinky's going on. Reject it. */
    uc = INVALID;

  uc = uc_truncate (uc);

  if (unicode_ret)
    *unicode_ret = uc;

  return length;
}


/* Converts a Unicode character to a multi-byte UTF8 sequence.
   Returns the number of bytes written.
 */
int
utf8_encode (unsigned long uc, char *out, long length)
{
  const char *old = out;

  uc = uc_truncate (uc);

  if (uc < 0x80 && length >= 1)			/* 7 bits in 1 byte */
    {
      *out++ = uc;				/* 0xxxxxxx */
    }
  else if (uc < 0x800 && length >= 2)		/* 11 bits in 2 bytes */
    {
      *out++ = (0xC0 | ((uc >> 6)  & 0x1F));	/* 110xxxxx */
      *out++ = (0x80 |  (uc        & 0x3F));	/* 10xxxxxx */
    }
  else if (uc < 0x10000L && length >= 3)	/* 16 bits in 3 bytes */
    {
      *out++ = (0xE0 | ((uc >> 12) & 0x0F));	/* 1110xxxx */
      *out++ = (0x80 | ((uc >>  6) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 |  (uc        & 0x3F));	/* 10xxxxxx */
    }
  else if (uc < 0x200000L && length >= 4)	/* 21 bits in 4 bytes */
    {
      *out++ = (0xF0 | ((uc >> 18) & 0x07));	/* 11110xxx */
      *out++ = (0x80 | ((uc >> 12) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 | ((uc >>  6) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 |  (uc        & 0x3F));	/* 10xxxxxx */
    } 
  else if (uc < 0x4000000L && length >= 5)	/* 26 bits in 5 bytes */
    {
      *out++ = (0xF8 | ((uc >> 24) & 0x03));	/* 111110xx */
      *out++ = (0x80 | ((uc >> 18) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 | ((uc >> 12) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 | ((uc >>  6) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 |  (uc        & 0x3F));	/* 10xxxxxx */
    }
  else if (length >= 6)				/* 31 bits in 6 bytes */
    {
      *out++ = (0xFC | ((uc >> 30) & 0x01));	/* 1111110x */
      *out++ = (0x80 | ((uc >> 24) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 | ((uc >> 18) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 | ((uc >> 12) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 | ((uc >>  6) & 0x3F));	/* 10xxxxxx */
      *out++ = (0x80 |  (uc        & 0x3F));	/* 10xxxxxx */
    }

  return (int) (out - old);
}


/* Whether the Unicode character should be combined with the preceding
   character (Combining Diacritical or Zero Width Joiner). */
int
uc_is_combining (unsigned long uc)
{
  /* If this is a Combining Diacritical, append it to the previous
     character. E.g., "y\314\206\314\206" is one string, not three.

     If this is ZWJ, Zero Width Joiner, then we append both this character
     and the following character, e.g. "X ZWJ Y" is one string not three.

     Is it intended that "Latin small letter C, 0063" + "Cedilla, 00B8"
     should be a single glyph? Or is that what "Combining Cedilla, 0327"
     is for?  I'm confused by the fact that the skin tones (1F3FB-1F3FF)
     do not seem to be in a readily-identifiable block the way the various
     combining diacriticals are.

     So I'm going to guess that this should also include every character
     in the "Symbol, Modifier" category:
     https://www.fileformat.info/info/unicode/category/Sk/list.htm
   */
  return ((uc >=   0x300 && uc <=   0x36F) || /* Combining Diacritical */
          (uc >=  0x1AB0 && uc <=  0x1AFF) || /* Combining Diacritical Ext. */
          (uc >=  0x1DC0 && uc <=  0x1DFF) || /* Combining Diacritical Supp. */
          (uc >=  0x20D0 && uc <=  0x20FF) || /* Combining Diacritical Sym. */
          (uc >=  0xFE20 && uc <=  0xFE2F) || /* Combining Half Marks */
          (uc >= 0x1F3FB && uc <= 0x1F3FF) || /* Emoji skin tone modifiers */

          /* Symbol, Modifier
           */
           uc == 0x00A8 ||			/* Diaeresis */
           uc == 0x00AF ||			/* Macron */
           uc == 0x00B4 ||			/* Acute Accent */
           uc == 0x00B8 ||			/* Cedilla */
          (uc >= 0x02C2 && uc <= 0x02C5) ||	/* Modifier Letter Left... */
          (uc >= 0x02D2 && uc <= 0x02DF) ||	/* Modifier Letter * */
          (uc >= 0x02E5 && uc <= 0x02EB) ||	/* Modifier Letter * */
           uc == 0x02ED ||			/* Modifier Letter Aspirated */
          (uc >= 0x02EF && uc <= 0x02FF) ||	/* Modifier Letter Low */
           uc == 0x0375 ||			/* Greek Lower Numeral Sign */
           uc == 0x0384 ||			/* Greek Tonos */
           uc == 0x0385 ||			/* Greek Dialytika Tonos */
           uc == 0x0888 ||			/* Arabic Raised Round Dot */
           uc == 0x1FBD ||			/* Greek Koronis */
          (uc >= 0x1FBF && uc <= 0x1FC1) ||	/* Greek Psili - Dialytika */
          (uc >= 0x1FCD && uc <= 0x1FCF) ||	/* Greek Psili - Perispomeni */
          (uc >= 0x1FDD && uc <= 0x1FDF) ||	/* Greek Dasia - Perispomeni */
          (uc >= 0x1FED && uc <= 0x1FEF) ||	/* Greek Dialytika - Varia */
           uc == 0x1FFD ||			/* Greek Oxia */
           uc == 0x1FFE ||			/* Greek Dasia */
           uc == 0x309B ||			/* Katakana-Hiragana Voiced */
           uc == 0x309C ||			/* Katakana-Hiragana Semi */
          (uc >= 0xA700 && uc <= 0xA716) ||	/* Modifier Letter Chinese...*/
           uc == 0xA720 ||			/* Modifier Letter Stress */
           uc == 0xA721 ||			/* Modifier Letter Stress */
           uc == 0xA789 ||			/* Modifier Letter Colon */
           uc == 0xA78A ||			/* Modifier Letter Short Eq */
           uc == 0xAB5B ||			/* Modifier Breve With... */
           uc == 0xAB6A ||			/* Modifier Letter Left Tack */
           uc == 0xAB6B ||			/* Modifier Letter Right Tack*/
          (uc >= 0xFBB2 && uc <= 0xFBC2) ||	/* Arabic Symbol Dot Above...*/
           uc == 0xFF3E ||			/* Fullwidth Circumflex */
           uc == 0xFF40 ||			/* Fullwidth Grave */
           uc == 0xFFE3 ||			/* Fullwidth Macron */

           uc == 0x200D);			/* Zero Width Joiner */
}

/* Like utf8_decode() except that if the following character is a
   Combining Diacritical or Zero Width Joiner, the appropriate number
   of following characters will be swallowed as well.
   Return the *first* Unicode character, and the number of bytes read.
 */
long
utf8_decode_combining (const unsigned char *in, long length,
                       unsigned long *unicode_ret)
{
  const unsigned char *start = in;
  const unsigned char *end = in + length;
  int i = 0;
  int zwjp = 0;
  
 for (i = 0; in < end; i++)
    {
      unsigned long uc;
      long len2 = utf8_decode (in, length, &uc);

      if (i == 0 && unicode_ret)
        *unicode_ret = uc;

      if (i > 0 && (zwjp || uc_is_combining (uc)))
        {
          i--;
          zwjp = (uc == 0x200D);  /* Swallow the next character as well */
        }
      else if (i > 0)
        break;

      in     += len2;
      length -= len2;
    }

  return (in - start);
}


/* Whether the Unicode character is whitespace, for word-wrapping purposes. */
int
uc_isspace (unsigned long uc)
{
  return (uc == 0x0009 ||	/* TABULATION */
          uc == 0x0020 ||	/* SPACE */
       /* uc == 0x00A0 ||	   NO-BREAK SPACE */
          uc == 0x1680 ||	/* OGHAM SPACE MARK */
          uc == 0x2000 ||	/* EN QUAD */
          uc == 0x2001 ||	/* EM QUAD */
          uc == 0x2002 ||	/* EN SPACE */
          uc == 0x2003 ||	/* EM SPACE */
          uc == 0x2004 ||	/* THREE-PER-EM SPACE */
          uc == 0x2005 ||	/* FOUR-PER-EM SPACE */
          uc == 0x2006 ||	/* SIX-PER-EM SPACE */
          uc == 0x2007 ||	/* FIGURE SPACE */
          uc == 0x2008 ||	/* PUNCTUATION SPACE */
          uc == 0x2009 ||	/* THIN SPACE */
          uc == 0x200A ||	/* HAIR SPACE */
          uc == 0x200B ||	/* ZERO WIDTH SPACE */
       /* uc == 0x202F ||	   NARROW NO-BREAK SPACE */
          uc == 0x205F ||	/* MEDIUM MATHEMATICAL SPACE */
          uc == 0x3000);	/* IDEOGRAPHIC SPACE */
}


/* Whether the Unicode character is punctuation, for word-wrapping purposes. */
int
uc_ispunct (unsigned long uc)
{
  /* "Unicode Standard Annex #14, Line Breaking Algorithm"
     https://www.unicode.org/reports/tr14/
     ...is a lot.  But this is probably good enough?
   */
  return (ispunct((char) uc) ||	/* Posix: !"#$%&'()*+,-./:;<=>?@[\]^_`{|}~ */
          uc == 0x002D ||	/* HYPHEN-MINUS */
          uc == 0x058A ||	/* ARMENIAN HYPHEN */
          uc == 0x05BE ||	/* HEBREW PUNCTUATION MAQAF */
          uc == 0x1400 ||	/* CANADIAN SYLLABICS HYPHEN */
          uc == 0x1806 ||	/* MONGOLIAN TODO SOFT HYPHEN */
          uc == 0x2010 ||	/* HYPHEN */
          uc == 0x2011 ||	/* NON-BREAKING HYPHEN */
          uc == 0x2012 ||	/* FIGURE DASH */
          uc == 0x2013 ||	/* EN DASH */
          uc == 0x2014 ||	/* EM DASH */
          uc == 0x2015 ||	/* HORIZONTAL BAR */
          uc == 0x2053 ||	/* SWUNG DASH */
          uc == 0x207B ||	/* SUPERSCRIPT MINUS */
          uc == 0x208B ||	/* SUBSCRIPT MINUS */
          uc == 0x2212 ||	/* MINUS SIGN */
          uc == 0x2E17 ||	/* DOUBLE OBLIQUE HYPHEN */
          uc == 0x2E1A ||	/* HYPHEN WITH DIAERESIS */
          uc == 0x2E3A ||	/* TWO-EM DASH */
          uc == 0x2E3B ||	/* THREE-EM DASH */
          uc == 0x2E40 ||	/* DOUBLE HYPHEN */
          uc == 0x2E5D ||	/* OBLIQUE HYPHEN */
          uc == 0x301C ||	/* WAVE DASH */
          uc == 0x3030 ||	/* WAVY DASH */
          uc == 0x30A0 ||	/* KATAKANA-HIRAGANA DOUBLE HYPHEN */
          uc == 0xFE31 ||	/* PRESENTATION FORM FOR VERTICAL EM DASH */
          uc == 0xFE32 ||	/* PRESENTATION FORM FOR VERTICAL EN DASH */
          uc == 0xFE58 ||	/* SMALL EM DASH */
          uc == 0xFE63 ||	/* SMALL HYPHEN-MINUS */
          uc == 0xFF0D ||	/* FULLWIDTH HYPHEN-MINUS */
          uc == 0x10EAD);	/* YEZIDI HYPHENATION MARK */
}


/* Converts a null-terminated UTF8 string to a null-terminated XChar2b array.
   This only handles characters that can be represented in 16 bits, the
   Basic Multilingual Plane. (No hieroglyphics, Elvish, Klingon or Emoji.)

   #### These XChar2b routines are only used in two places:
        - fontglide, but only in debugMetrics-mode.
        - jwxyz's implementation of XDrawString16 and XTextExtents16.
          But XDrawString16 is only used by:
            - fontglide debugMetrics (again)
            - xft.c if both Xft and Xutf8DrawString are unavailable,
              which is never, because Xft is required on X11, so xft.c
              is used only on macOS / iOS, which has Xutf8DrawString.
 */
XChar2b *
utf8_to_XChar2b (const char *string, int *length_ret)
{
  long in_len = strlen(string);
  const unsigned char *in = (const unsigned char *) string;
  const unsigned char *in_end = in + in_len;
  XChar2b *c2b = (XChar2b *) malloc ((in_len + 1) * sizeof(*c2b));
  XChar2b *out = c2b;
  if (! out) return 0;

  while (in < in_end)
    {
      unsigned long uc = 0;
      long L = utf8_decode (in, in_end - in, &uc);
      in += L;

      /* If it can't be represented in a 16-bit XChar2b,
         use "Unicode Replacement Character". */
      if (uc > 0xFFFF) uc = INVALID;

      out->byte1 = (uc >> 8) & 0xFF;
      out->byte2 = uc & 0xFF;
      out++;
    }

  out->byte1 = 0;
  out->byte2 = 0;

  if (length_ret)
    *length_ret = (int) (out - c2b);

  /* shrink */
  c2b = (XChar2b *) realloc (c2b, (out - c2b + 1) * sizeof(*c2b));

  return c2b;
}


/* Converts a null-terminated XChar2b array to a null-terminated UTF8 string.
 */
char *
XChar2b_to_utf8 (const XChar2b *in, int *length_ret)
{
  int in_len = 0;
  const XChar2b *in_end;
  int out_len;
  char *utf8, *out;
  const char *out_end;

  /* Find the null termination on the XChar2b. */
  for (in_end = in; in_end->byte1 || in_end->byte2; in_end++, in_len++)
    ;

  out_len = (in_len + 1) * 3;		   /* 16 bit chars = 3 bytes max */
  utf8 = out = (char *) malloc (out_len + 1);
  if (! out) return 0;
  out_end = out + out_len;

  while (in < in_end)
    {
      unsigned long uc = (in->byte1 << 8) | in->byte2;
      int wrote = utf8_encode (uc, out, out_end - out);
      if (wrote > 3) abort();  /* Can't happen with 16 bit input */
      out += wrote;
      in++;
    }
  *out = 0;

  out_len = (int) (out - utf8 + 1);

  if (length_ret)
    *length_ret = out_len;

  /* shrink */
  utf8 = (char *) realloc (utf8, out_len);

  return utf8;
}


/* Converts a UTF8 string to the closest Latin1 or ASCII equivalent.
 */
char *
utf8_to_latin1 (const char *string, int ascii_p)
{
  long in_len = strlen(string);
  const unsigned char *in = (const unsigned char *) string;
  const unsigned char *in_end = in + in_len;
  unsigned char *ret = (unsigned char *) malloc ((in_len * 4) + 1);
  unsigned char *out = ret;

  if (! ret) return 0;

  while (in < in_end)
    {
      unsigned long uc = 0;
      long len2 = utf8_decode (in, in_end - in, &uc);
      in += len2;

      if (uc == '\240')	/* &nbsp; */
        uc = ' ';
      else if (uc >= 0x300 && uc <= 0x36F)
        uc = 0;		/* Discard "Combining Diacritical Marks" */
      else if (uc >= 0x1AB0 && uc <= 0x1AFF)
        uc = 0;		/* Discard "Combining Diacritical Marks Extended" */
      else if (uc >= 0x1DC0 && uc <= 0x1DFF)
        uc = 0;		/* Discard "Combining Diacritical Marks Supplement" */
      else if (uc >= 0x20D0 && uc <= 0x20FF)
        uc = 0;		/* Discard "Combining Diacritical Marks for Symbols" */
      else if (uc >= 0xFE20 && uc <= 0xFE2F)
        uc = 0;		/* Discard "Combining Half Marks" */

      else if (uc > 0xFF)
        switch (uc) {

        /* Map "Unicode General Punctuation Block" to Latin1 equivalents. */

        case 0x2000:	/*  	EN QUAD */
        case 0x2001:	/*  	EM QUAD */
        case 0x2002:	/*  	EN SPACE */
        case 0x2003:	/*  	EM SPACE */
        case 0x2004:	/*  	THREE-PER-EM SPACE */
        case 0x2005:	/*  	FOUR-PER-EM SPACE */
        case 0x2006:	/*  	SIX-PER-EM SPACE */
        case 0x2007:	/*  	FIGURE SPACE */
        case 0x2008:	/*  	PUNCTUATION SPACE */
        case 0x2009:	/*  	THIN SPACE */
        case 0x200A:	/*  	HAIR SPACE */
        case 0x3000:	/* 　	IDEOGRAPHIC SPACE */
          uc = ' ';
	  break;

        case 0x00A0:	/*  	NO-BREAK SPACE */
        case 0x202F:	/*  	NARROW NO-BREAK SPACE */
          uc = '\240';
          break;

        case 0x058A:	/* ֊	ARMENIAN HYPHEN */
        case 0x05BE:	/* ־	HEBREW PUNCTUATION MAQAF */
        case 0x1400:	/* ᐀	CANADIAN SYLLABICS HYPHEN */
        case 0x1806:	/* ᠆	MONGOLIAN TODO SOFT HYPHEN */
        case 0x2010:	/* ‐	HYPHEN */
        case 0x2011:	/* ‑	NON-BREAKING HYPHEN */
        case 0x2012:	/* ‒	FIGURE DASH */
        case 0x2013:	/* –	EN DASH */
        case 0x2014:	/* —	EM DASH */
        case 0x2015:	/* ―	HORIZONTAL BAR */
        case 0x2053:	/* ⁓	SWUNG DASH */
        case 0x207B:	/* ⁻	SUPERSCRIPT MINUS */
        case 0x208B:	/* ₋	SUBSCRIPT MINUS */
        case 0x2212:	/* −	MINUS SIGN */
        case 0x2E17:	/* ⸗	DOUBLE OBLIQUE HYPHEN */
        case 0x2E1A:	/* ⸚	HYPHEN WITH DIAERESIS */
        case 0x2E3A:	/* ⸺	TWO-EM DASH */
        case 0x2E3B:	/* ⸻	THREE-EM DASH */
        case 0x2E40:	/* ⹀	DOUBLE HYPHEN */
        case 0x2E5D:	/* ⹝	OBLIQUE HYPHEN */
        case 0x301C:	/* 〜	WAVE DASH */
        case 0x3030:	/* 〰	WAVY DASH */
        case 0x30A0:	/* ゠	KATAKANA-HIRAGANA DOUBLE HYPHEN */
        case 0xFE31:	/* ︱	PRESENTATION FORM FOR VERTICAL EM DASH */
        case 0xFE32:	/* ︲	PRESENTATION FORM FOR VERTICAL EN DASH */
        case 0xFE58:	/* ﹘	SMALL EM DASH */
        case 0xFE63:	/* ﹣	SMALL HYPHEN-MINUS */
        case 0xFF0D:	/* －	FULLWIDTH HYPHEN-MINUS */
        case 0x10EAD:	/* 𐺭	YEZIDI HYPHENATION MARK */
          uc = '-';
	  break;

        case 0x2018:	/* ‘	LEFT SINGLE QUOTATION MARK */
        case 0x2019:	/* ’	SINGLE LOW-9 QUOTATION MARK */
        case 0x201A:	/* ‚	SINGLE LOW-9 QUOTATION MARK */
        case 0x201B:	/* ‛	SINGLE HIGH-REVERSED-9 QUOTATION MARK */
          uc = '\'';
	  break;

        case 0x201C:	/* “	LEFT DOUBLE QUOTATION MARK */
        case 0x201D:	/* ”	RIGHT DOUBLE QUOTATION MARK */
        case 0x201E:	/* „	DOUBLE LOW-9 QUOTATION MARK */
        case 0x201F:	/* ‟	DOUBLE HIGH-REVERSED-9 QUOTATION MARK */
          uc = '"';
	  break;

        case 0x2022: uc = '\267'; break; /* •	BULLET */
        case 0x2023: uc = '\273'; break; /* ‣	TRIANGULAR BULLET */
        case 0x2027: uc = '\267'; break; /* ‧	HYPHENATION POINT */
        case 0x2038: uc = '^';	  break; /* ‸	CARET */
        case 0x2039: uc = '\253'; break; /* ‹	SINGLE LEFT ANGLE QUOTATION MARK */
        case 0x203A: uc = '\273'; break; /* ›	SINGLE RIGHT ANGLE QUOTATION MARK*/
        case 0x2041: uc = '^';	  break; /* ⁁	CARET INSERTION POINT */
        case 0x2042: uc = '*';	  break; /* ⁂	ASTERISM */
        case 0x2043: uc = '=';	  break; /* ⁃	HYPHEN BULLET */
        case 0x2044: uc = '/';	  break; /* ⁄	FRACTION SLASH */
        case 0x204B: uc = '\266'; break; /* ⁋	REVERSED PILCROW SIGN */
        case 0x204C: uc = '\267'; break; /* ⁌	BLACK LEFTWARDS BULLET */
        case 0x204D: uc = '\267'; break; /* ⁍	BLACK RIGHTWARDS BULLET */
        case 0x204E: uc = '*';	  break; /* ⁎	LOW ASTERISK */
        case 0x204F: uc = ';';	  break; /* ⁏	REVERSED SEMICOLON */

        /* Latin Extended-A */

	case 0x0100: uc = 'A';	break; /* Ā	CAPITAL LETTER A WITH MACRON */
	case 0x0101: uc = 'a';	break; /* ā	SMALL LETTER A WITH MACRON */
	case 0x0102: uc = 'A';	break; /* Ă	CAPITAL LETTER A WITH BREVE */
	case 0x0103: uc = 'a';	break; /* ă	SMALL LETTER A WITH BREVE */
	case 0x0104: uc = 'A';	break; /* Ą	CAPITAL LETTER A WITH OGONEK */
	case 0x0105: uc = 'a';	break; /* ą	SMALL LETTER A WITH OGONEK */
	case 0x0106: uc = 'C';	break; /* Ć	CAPITAL LETTER C WITH ACUTE */
	case 0x0107: uc = 'c';	break; /* ć	SMALL LETTER C WITH ACUTE */
	case 0x0108: uc = 'C';	break; /* Ĉ	CAPITAL LETTER C WITH CIRCUMFLEX */
	case 0x0109: uc = 'c';	break; /* ĉ	SMALL LETTER C WITH CIRCUMFLEX */
	case 0x010A: uc = 'C';	break; /* Ċ	CAPITAL LETTER C WITH DOT ABOVE */
	case 0x010B: uc = 'c';	break; /* ċ	SMALL LETTER C WITH DOT ABOVE */
	case 0x010C: uc = 'C';	break; /* Č	CAPITAL LETTER C WITH CARON */
	case 0x010D: uc = 'c';	break; /* č	SMALL LETTER C WITH CARON */
	case 0x010E: uc = 'D';	break; /* Ď	CAPITAL LETTER D WITH CARON */
	case 0x010F: uc = 'd';	break; /* ď	SMALL LETTER D WITH CARON */
	case 0x0110: uc = 'D';	break; /* Đ	CAPITAL LETTER D WITH STROKE */
	case 0x0111: uc = 'd';	break; /* đ	SMALL LETTER D WITH STROKE */
	case 0x0112: uc = 'E';	break; /* Ē	CAPITAL LETTER E WITH MACRON */
	case 0x0113: uc = 'e';	break; /* ē	SMALL LETTER E WITH MACRON */
	case 0x0114: uc = 'E';	break; /* Ĕ	CAPITAL LETTER E WITH BREVE */
	case 0x0115: uc = 'e';	break; /* ĕ	SMALL LETTER E WITH BREVE */
	case 0x0116: uc = 'E';	break; /* Ė	CAPITAL LETTER E WITH DOT ABOVE */
	case 0x0117: uc = 'e';	break; /* ė	SMALL LETTER E WITH DOT ABOVE */
	case 0x0118: uc = 'E';	break; /* Ę	CAPITAL LETTER E WITH OGONEK */
	case 0x0119: uc = 'e';	break; /* ę	SMALL LETTER E WITH OGONEK */
	case 0x011A: uc = 'E';	break; /* Ě	CAPITAL LETTER E WITH CARON */
	case 0x011B: uc = 'e';	break; /* ě	SMALL LETTER E WITH CARON */
	case 0x011C: uc = 'G';	break; /* Ĝ	CAPITAL LETTER G WITH CIRCUMFLEX */
	case 0x011D: uc = 'g';	break; /* ĝ	SMALL LETTER G WITH CIRCUMFLEX */
	case 0x011E: uc = 'G';	break; /* Ğ	CAPITAL LETTER G WITH BREVE */
	case 0x011F: uc = 'g';	break; /* ğ	SMALL LETTER G WITH BREVE */
	case 0x0120: uc = 'G';	break; /* Ġ	CAPITAL LETTER G WITH DOT ABOVE */
	case 0x0121: uc = 'g';	break; /* ġ	SMALL LETTER G WITH DOT ABOVE */
	case 0x0122: uc = 'G';	break; /* Ģ	CAPITAL LETTER G WITH CEDILLA */
	case 0x0123: uc = 'g';	break; /* ģ	SMALL LETTER G WITH CEDILLA */
	case 0x0124: uc = 'H';	break; /* Ĥ	CAPITAL LETTER H WITH CIRCUMFLEX */
	case 0x0125: uc = 'h';	break; /* ĥ	SMALL LETTER H WITH CIRCUMFLEX */
	case 0x0126: uc = 'H';	break; /* Ħ	CAPITAL LETTER H WITH STROKE */
	case 0x0127: uc = 'h';	break; /* ħ	SMALL LETTER H WITH STROKE */
	case 0x0128: uc = 'I';	break; /* Ĩ	CAPITAL LETTER I WITH TILDE */
	case 0x0129: uc = 'i';	break; /* ĩ	SMALL LETTER I WITH TILDE */
	case 0x012A: uc = 'I';	break; /* Ī	CAPITAL LETTER I WITH MACRON */
	case 0x012B: uc = 'i';	break; /* ī	SMALL LETTER I WITH MACRON */
	case 0x012C: uc = 'I';	break; /* Ĭ	CAPITAL LETTER I WITH BREVE */
	case 0x012D: uc = 'i';	break; /* ĭ	SMALL LETTER I WITH BREVE */
	case 0x012E: uc = 'I';	break; /* Į	CAPITAL LETTER I WITH OGONEK */
	case 0x012F: uc = 'i';	break; /* į	SMALL LETTER I WITH OGONEK */
	case 0x0130: uc = 'I';	break; /* İ	CAPITAL LETTER I WITH DOT ABOVE */
	case 0x0131: uc = 'd';	break; /* ı	SMALL LETTER DOTLESS I */
	case 0x0132: uc = 'J';	break; /* Ĳ	CAPITAL LIGATURE IJ */
	case 0x0133: uc = 'j';	break; /* ĳ	SMALL LIGATURE IJ */
	case 0x0134: uc = 'J';	break; /* Ĵ	CAPITAL LETTER J WITH CIRCUMFLEX */
	case 0x0135: uc = 'j';	break; /* ĵ	SMALL LETTER J WITH CIRCUMFLEX */
	case 0x0136: uc = 'K';	break; /* Ķ	CAPITAL LETTER K WITH CEDILLA */
	case 0x0137: uc = 'k';	break; /* ķ	SMALL LETTER K WITH CEDILLA */
	case 0x0138: uc = 'k';	break; /* ĸ	SMALL LETTER KRA */
	case 0x0139: uc = 'L';	break; /* Ĺ	CAPITAL LETTER L WITH ACUTE */
	case 0x013A: uc = 'l';	break; /* ĺ	SMALL LETTER L WITH ACUTE */
	case 0x013B: uc = 'L';	break; /* Ļ	CAPITAL LETTER L WITH CEDILLA */
	case 0x013C: uc = 'l';	break; /* ļ	SMALL LETTER L WITH CEDILLA */
	case 0x013D: uc = 'L';	break; /* Ľ	CAPITAL LETTER L WITH CARON */
	case 0x013E: uc = 'l';	break; /* ľ	SMALL LETTER L WITH CARON */
	case 0x013F: uc = 'L';	break; /* Ŀ	CAPITAL LETTER L WITH MIDDLE DOT */
	case 0x0140: uc = 'l';	break; /* ŀ	SMALL LETTER L WITH MIDDLE DOT */
	case 0x0141: uc = 'L';	break; /* Ł	CAPITAL LETTER L WITH STROKE */
	case 0x0142: uc = 'l';	break; /* ł	SMALL LETTER L WITH STROKE */
	case 0x0143: uc = 'N';	break; /* Ń	CAPITAL LETTER N WITH ACUTE */
	case 0x0144: uc = 'n';	break; /* ń	SMALL LETTER N WITH ACUTE */
	case 0x0145: uc = 'N';	break; /* Ņ	CAPITAL LETTER N WITH CEDILLA */
	case 0x0146: uc = 'n';	break; /* ņ	SMALL LETTER N WITH CEDILLA */
	case 0x0147: uc = 'N';	break; /* Ň	CAPITAL LETTER N WITH CARON */
	case 0x0148: uc = 'n';	break; /* ň	SMALL LETTER N WITH CARON */
	case 0x0149: uc = 'n';	break; /* ŉ	SMALL LETTER N PRECEDED BY APOSTROPHE */
	case 0x014A: uc = 'E';	break; /* Ŋ	CAPITAL LETTER ENG */
	case 0x014B: uc = 'e';	break; /* ŋ	SMALL LETTER ENG */
	case 0x014C: uc = 'O';	break; /* Ō	CAPITAL LETTER O WITH MACRON */
	case 0x014D: uc = 'o';	break; /* ō	SMALL LETTER O WITH MACRON */
	case 0x014E: uc = 'O';	break; /* Ŏ	CAPITAL LETTER O WITH BREVE */
	case 0x014F: uc = 'o';	break; /* ŏ	SMALL LETTER O WITH BREVE */
	case 0x0150: uc = 'O';	break; /* Ő	CAPITAL LETTER O WITH DOUBLE ACUTE */
	case 0x0151: uc = 'o';	break; /* ő	SMALL LETTER O WITH DOUBLE ACUTE */
	case 0x0152: uc = 'O';	break; /* Œ	CAPITAL LIGATURE OE */
	case 0x0153: uc = 'o';	break; /* œ	SMALL LIGATURE OE */
	case 0x0154: uc = 'R';	break; /* Ŕ	CAPITAL LETTER R WITH ACUTE */
	case 0x0155: uc = 'r';	break; /* ŕ	SMALL LETTER R WITH ACUTE */
	case 0x0156: uc = 'R';	break; /* Ŗ	CAPITAL LETTER R WITH CEDILLA */
	case 0x0157: uc = 'r';	break; /* ŗ	SMALL LETTER R WITH CEDILLA */
	case 0x0158: uc = 'R';	break; /* Ř	CAPITAL LETTER R WITH CARON */
	case 0x0159: uc = 'r';	break; /* ř	SMALL LETTER R WITH CARON */
	case 0x015A: uc = 'S';	break; /* Ś	CAPITAL LETTER S WITH ACUTE */
	case 0x015B: uc = 's';	break; /* ś	SMALL LETTER S WITH ACUTE */
	case 0x015C: uc = 'S';	break; /* Ŝ	CAPITAL LETTER S WITH CIRCUMFLEX */
	case 0x015D: uc = 's';	break; /* ŝ	SMALL LETTER S WITH CIRCUMFLEX */
	case 0x015E: uc = 'S';	break; /* Ş	CAPITAL LETTER S WITH CEDILLA */
	case 0x015F: uc = 's';	break; /* ş	SMALL LETTER S WITH CEDILLA */
	case 0x0160: uc = 'S';	break; /* Š	CAPITAL LETTER S WITH CARON */
	case 0x0161: uc = 's';	break; /* š	SMALL LETTER S WITH CARON */
	case 0x0162: uc = 'T';	break; /* Ţ	CAPITAL LETTER T WITH CEDILLA */
	case 0x0163: uc = 't';	break; /* ţ	SMALL LETTER T WITH CEDILLA */
	case 0x0164: uc = 'T';	break; /* Ť	CAPITAL LETTER T WITH CARON */
	case 0x0165: uc = 't';	break; /* ť	SMALL LETTER T WITH CARON */
	case 0x0166: uc = 'T';	break; /* Ŧ	CAPITAL LETTER T WITH STROKE */
	case 0x0167: uc = 't';	break; /* ŧ	SMALL LETTER T WITH STROKE */
	case 0x0168: uc = 'U';	break; /* Ũ	CAPITAL LETTER U WITH TILDE */
	case 0x0169: uc = 'u';	break; /* ũ	SMALL LETTER U WITH TILDE */
	case 0x016A: uc = 'U';	break; /* Ū	CAPITAL LETTER U WITH MACRON */
	case 0x016B: uc = 'u';	break; /* ū	SMALL LETTER U WITH MACRON */
	case 0x016C: uc = 'U';	break; /* Ŭ	CAPITAL LETTER U WITH BREVE */
	case 0x016D: uc = 'u';	break; /* ŭ	SMALL LETTER U WITH BREVE */
	case 0x016E: uc = 'U';	break; /* Ů	CAPITAL LETTER U WITH RING ABOVE */
	case 0x016F: uc = 'u';	break; /* ů	SMALL LETTER U WITH RING ABOVE */
	case 0x0170: uc = 'U';	break; /* Ű	CAPITAL LETTER U WITH DOUBLE ACUTE */
	case 0x0171: uc = 'u';	break; /* ű	SMALL LETTER U WITH DOUBLE ACUTE */
	case 0x0172: uc = 'U';	break; /* Ų	CAPITAL LETTER U WITH OGONEK */
	case 0x0173: uc = 'u';	break; /* ų	SMALL LETTER U WITH OGONEK */
	case 0x0174: uc = 'W';	break; /* Ŵ	CAPITAL LETTER W WITH CIRCUMFLEX */
	case 0x0175: uc = 'w';	break; /* ŵ	SMALL LETTER W WITH CIRCUMFLEX */
	case 0x0176: uc = 'Y';	break; /* Ŷ	CAPITAL LETTER Y WITH CIRCUMFLEX */
	case 0x0177: uc = 'y';	break; /* ŷ	SMALL LETTER Y WITH CIRCUMFLEX */
	case 0x0178: uc = 'Y';	break; /* Ÿ	CAPITAL LETTER Y WITH DIAERESIS */
	case 0x0179: uc = 'Z';	break; /* Ź	CAPITAL LETTER Z WITH ACUTE */
	case 0x017A: uc = 'z';	break; /* ź	SMALL LETTER Z WITH ACUTE */
	case 0x017B: uc = 'Z';	break; /* Ż	CAPITAL LETTER Z WITH DOT ABOVE */
	case 0x017C: uc = 'z';	break; /* ż	SMALL LETTER Z WITH DOT ABOVE */
	case 0x017D: uc = 'Z';	break; /* Ž	CAPITAL LETTER Z WITH CARON */
	case 0x017E: uc = 'z';	break; /* ž	SMALL LETTER Z WITH CARON */
	case 0x017F: uc = 'l';	break; /* ſ	SMALL LETTER LONG S */

        /* Latin Extended-B */

	case 0x0180: uc = 'b';	break; /* ƀ	SMALL LETTER B WITH STROKE */
	case 0x0181: uc = 'B';	break; /* Ɓ	CAPITAL LETTER B WITH HOOK */
	case 0x0182: uc = 'B';	break; /* Ƃ	CAPITAL LETTER B WITH TOPBAR */
	case 0x0183: uc = 'b';	break; /* ƃ	SMALL LETTER B WITH TOPBAR */
	case 0x0184: uc = 'T';	break; /* Ƅ	CAPITAL LETTER TONE SIX */
	case 0x0185: uc = 't';	break; /* ƅ	SMALL LETTER TONE SIX */
	case 0x0186: uc = 'O';	break; /* Ɔ	CAPITAL LETTER OPEN O */
	case 0x0187: uc = 'C';	break; /* Ƈ	CAPITAL LETTER C WITH HOOK */
	case 0x0188: uc = 'c';	break; /* ƈ	SMALL LETTER C WITH HOOK */
	case 0x0189: uc = 'A';	break; /* Ɖ	CAPITAL LETTER AFRICAN D */
	case 0x018A: uc = 'D';	break; /* Ɗ	CAPITAL LETTER D WITH HOOK */
	case 0x018B: uc = 'D';	break; /* Ƌ	CAPITAL LETTER D WITH TOPBAR */
	case 0x018C: uc = 'd';	break; /* ƌ	SMALL LETTER D WITH TOPBAR */
	case 0x018D: uc = 't';	break; /* ƍ	SMALL LETTER TURNED DELTA */
	case 0x018E: uc = 'R';	break; /* Ǝ	CAPITAL LETTER REVERSED E */
	case 0x018F: uc = 'S';	break; /* Ə	CAPITAL LETTER SCHWA */
	case 0x0190: uc = 'O';	break; /* Ɛ	CAPITAL LETTER OPEN E */
	case 0x0191: uc = 'F';	break; /* Ƒ	CAPITAL LETTER F WITH HOOK */
	case 0x0192: uc = 'f';	break; /* ƒ	SMALL LETTER F WITH HOOK */
	case 0x0193: uc = 'G';	break; /* Ɠ	CAPITAL LETTER G WITH HOOK */
	case 0x0194: uc = 'G';	break; /* Ɣ	CAPITAL LETTER GAMMA */
	case 0x0195: uc = 'h';	break; /* ƕ	SMALL LETTER HV */
	case 0x0196: uc = 'I';	break; /* Ɩ	CAPITAL LETTER IOTA */
	case 0x0197: uc = 'I';	break; /* Ɨ	CAPITAL LETTER I WITH STROKE */
	case 0x0198: uc = 'K';	break; /* Ƙ	CAPITAL LETTER K WITH HOOK */
	case 0x0199: uc = 'k';	break; /* ƙ	SMALL LETTER K WITH HOOK */
	case 0x019A: uc = 'l';	break; /* ƚ	SMALL LETTER L WITH BAR */
	case 0x019B: uc = 'l';	break; /* ƛ	SMALL LETTER LAMBDA WITH STROKE */
	case 0x019C: uc = 'T';	break; /* Ɯ	CAPITAL LETTER TURNED M */
	case 0x019D: uc = 'N';	break; /* Ɲ	CAPITAL LETTER N WITH LEFT HOOK */
	case 0x019E: uc = 'n';	break; /* ƞ	SMALL LETTER N WITH LONG RIGHT LEG */
	case 0x019F: uc = 'O';	break; /* Ɵ	CAPITAL LETTER O WITH MIDDLE TILDE */
	case 0x01A0: uc = 'O';	break; /* Ơ	CAPITAL LETTER O WITH HORN */
	case 0x01A1: uc = 'o';	break; /* ơ	SMALL LETTER O WITH HORN */
	case 0x01A2: uc = 'O';	break; /* Ƣ	CAPITAL LETTER OI */
	case 0x01A3: uc = 'o';	break; /* ƣ	SMALL LETTER OI */
	case 0x01A4: uc = 'P';	break; /* Ƥ	CAPITAL LETTER P WITH HOOK */
	case 0x01A5: uc = 'p';	break; /* ƥ	SMALL LETTER P WITH HOOK */
	case 0x01A6: uc = 'R';	break; /* Ʀ	LETTER YR */
	case 0x01A7: uc = 'T';	break; /* Ƨ	CAPITAL LETTER TONE TWO */
	case 0x01A8: uc = 't';	break; /* ƨ	SMALL LETTER TONE TWO */
	case 0x01A9: uc = 'E';	break; /* Ʃ	CAPITAL LETTER ESH */
	case 0x01AA: uc = 'S';	break; /* ƪ	LETTER REVERSED ESH LOOP */
	case 0x01AB: uc = 't';	break; /* ƫ	SMALL LETTER T WITH PALATAL HOOK */
	case 0x01AC: uc = 'T';	break; /* Ƭ	CAPITAL LETTER T WITH HOOK */
	case 0x01AD: uc = 't';	break; /* ƭ	SMALL LETTER T WITH HOOK */
	case 0x01AE: uc = 'T';	break; /* Ʈ	CAPITAL LETTER T WITH RETROFLEX HOOK */
	case 0x01AF: uc = 'U';	break; /* Ư	CAPITAL LETTER U WITH HORN */
	case 0x01B0: uc = 'u';	break; /* ư	SMALL LETTER U WITH HORN */
	case 0x01B1: uc = 'U';	break; /* Ʊ	CAPITAL LETTER UPSILON */
	case 0x01B2: uc = 'V';	break; /* Ʋ	CAPITAL LETTER V WITH HOOK */
	case 0x01B3: uc = 'Y';	break; /* Ƴ	CAPITAL LETTER Y WITH HOOK */
	case 0x01B4: uc = 'y';	break; /* ƴ	SMALL LETTER Y WITH HOOK */
	case 0x01B5: uc = 'Z';	break; /* Ƶ	CAPITAL LETTER Z WITH STROKE */
	case 0x01B6: uc = 'z';	break; /* ƶ	SMALL LETTER Z WITH STROKE */
	case 0x01B7: uc = 'E';	break; /* Ʒ	CAPITAL LETTER EZH */
	case 0x01B8: uc = 'E';	break; /* Ƹ	CAPITAL LETTER EZH REVERSED */
	case 0x01B9: uc = 'e';	break; /* ƹ	SMALL LETTER EZH REVERSED */
	case 0x01BA: uc = 'e';	break; /* ƺ	SMALL LETTER EZH WITH TAIL */
	case 0x01BB: uc = 0xB2;	break; /* ƻ	LETTER TWO WITH STROKE */
	case 0x01BC: uc = 'T';	break; /* Ƽ	CAPITAL LETTER TONE FIVE */
	case 0x01BD: uc = 't';	break; /* ƽ	SMALL LETTER TONE FIVE */
	case 0x01BE: uc = 'S';	break; /* ƾ	LETTER INVERTED GLOTTAL STOP WITH STROKE */
	case 0x01BF: uc = 'P';	break; /* ƿ	LETTER WYNN */
	case 0x01C0: uc = '!';	break; /* ǀ	LETTER DENTAL CLICK */
	case 0x01C1: uc = '!';	break; /* ǁ	LETTER LATERAL CLICK */
	case 0x01C2: uc = '=';	break; /* ǂ	LETTER ALVEOLAR CLICK */
	case 0x01C3: uc = '!';	break; /* ǃ	LETTER RETROFLEX CLICK */
	case 0x01C4: uc = 'D';	break; /* Ǆ	CAPITAL LETTER DZ WITH CARON */
	case 0x01C5: uc = 'D';	break; /* ǅ	CAPITAL LETTER D WITH SMALL LETTER Z WITH CARON */
	case 0x01C6: uc = 'z';	break; /* ǆ	SMALL LETTER DZ WITH CARON */
	case 0x01C7: uc = 'L';	break; /* Ǉ	CAPITAL LETTER LJ */
	case 0x01C8: uc = 'L';	break; /* ǈ	CAPITAL LETTER L WITH SMALL LETTER J */
	case 0x01C9: uc = 'j';	break; /* ǉ	SMALL LETTER LJ */
	case 0x01CA: uc = 'N';	break; /* Ǌ	CAPITAL LETTER NJ */
	case 0x01CB: uc = 'N';	break; /* ǋ	CAPITAL LETTER N WITH SMALL LETTER J */
	case 0x01CC: uc = 'j';	break; /* ǌ	SMALL LETTER NJ */
	case 0x01CD: uc = 'A';	break; /* Ǎ	CAPITAL LETTER A WITH CARON */
	case 0x01CE: uc = 'a';	break; /* ǎ	SMALL LETTER A WITH CARON */
	case 0x01CF: uc = 'I';	break; /* Ǐ	CAPITAL LETTER I WITH CARON */
	case 0x01D0: uc = 'i';	break; /* ǐ	SMALL LETTER I WITH CARON */
	case 0x01D1: uc = 'O';	break; /* Ǒ	CAPITAL LETTER O WITH CARON */
	case 0x01D2: uc = 'o';	break; /* ǒ	SMALL LETTER O WITH CARON */
	case 0x01D3: uc = 'U';	break; /* Ǔ	CAPITAL LETTER U WITH CARON */
	case 0x01D4: uc = 'u';	break; /* ǔ	SMALL LETTER U WITH CARON */
	case 0x01D5: uc = 0xDC;	break; /* Ǖ	CAPITAL LETTER U WITH DIAERESIS AND MACRON */
	case 0x01D6: uc = 0xFC;	break; /* ǖ	SMALL LETTER U WITH DIAERESIS AND MACRON */
	case 0x01D7: uc = 0xDC;	break; /* Ǘ	CAPITAL LETTER U WITH DIAERESIS AND ACUTE */
	case 0x01D8: uc = 0xFC;	break; /* ǘ	SMALL LETTER U WITH DIAERESIS AND ACUTE */
	case 0x01D9: uc = 0xDC;	break; /* Ǚ	CAPITAL LETTER U WITH DIAERESIS AND CARON */
	case 0x01DA: uc = 0xFC;	break; /* ǚ	SMALL LETTER U WITH DIAERESIS AND CARON */
	case 0x01DB: uc = 0xDC;	break; /* Ǜ	CAPITAL LETTER U WITH DIAERESIS AND GRAVE */
	case 0x01DC: uc = 0xFC;	break; /* ǜ	SMALL LETTER U WITH DIAERESIS AND GRAVE */
	case 0x01DD: uc = 't';	break; /* ǝ	SMALL LETTER TURNED E */
	case 0x01DE: uc = 0xC4;	break; /* Ǟ	CAPITAL LETTER A WITH DIAERESIS AND MACRON */
	case 0x01DF: uc = 0xE4;	break; /* ǟ	SMALL LETTER A WITH DIAERESIS AND MACRON */
	case 0x01E0: uc = 'A';	break; /* Ǡ	CAPITAL LETTER A WITH DOT ABOVE AND MACRON */
	case 0x01E1: uc = 'a';	break; /* ǡ	SMALL LETTER A WITH DOT ABOVE AND MACRON */
	case 0x01E2: uc = 0xC6;	break; /* Ǣ	CAPITAL LETTER AE WITH MACRON */
	case 0x01E3: uc = 0xE6;	break; /* ǣ	SMALL LETTER AE WITH MACRON */
	case 0x01E4: uc = 'G';	break; /* Ǥ	CAPITAL LETTER G WITH STROKE */
	case 0x01E5: uc = 'g';	break; /* ǥ	SMALL LETTER G WITH STROKE */
	case 0x01E6: uc = 'G';	break; /* Ǧ	CAPITAL LETTER G WITH CARON */
	case 0x01E7: uc = 'g';	break; /* ǧ	SMALL LETTER G WITH CARON */
	case 0x01E8: uc = 'K';	break; /* Ǩ	CAPITAL LETTER K WITH CARON */
	case 0x01E9: uc = 'k';	break; /* ǩ	SMALL LETTER K WITH CARON */
	case 0x01EA: uc = 'O';	break; /* Ǫ	CAPITAL LETTER O WITH OGONEK */
	case 0x01EB: uc = 'o';	break; /* ǫ	SMALL LETTER O WITH OGONEK */
	case 0x01EC: uc = 'O';	break; /* Ǭ	CAPITAL LETTER O WITH OGONEK AND MACRON */
	case 0x01ED: uc = 'o';	break; /* ǭ	SMALL LETTER O WITH OGONEK AND MACRON */
	case 0x01EE: uc = 'E';	break; /* Ǯ	CAPITAL LETTER EZH WITH CARON */
	case 0x01EF: uc = 'e';	break; /* ǯ	SMALL LETTER EZH WITH CARON */
	case 0x01F0: uc = 'j';	break; /* ǰ	SMALL LETTER J WITH CARON */
	case 0x01F1: uc = 'D';	break; /* Ǳ	CAPITAL LETTER DZ */
	case 0x01F2: uc = 'D';	break; /* ǲ	CAPITAL LETTER D WITH SMALL LETTER Z */
	case 0x01F3: uc = 'z';	break; /* ǳ	SMALL LETTER DZ */
	case 0x01F4: uc = 'G';	break; /* Ǵ	CAPITAL LETTER G WITH ACUTE */
	case 0x01F5: uc = 'g';	break; /* ǵ	SMALL LETTER G WITH ACUTE */
	case 0x01F6: uc = 'H';	break; /* Ƕ	CAPITAL LETTER HWAIR */
	case 0x01F7: uc = 'W';	break; /* Ƿ	CAPITAL LETTER WYNN */
	case 0x01F8: uc = 'N';	break; /* Ǹ	CAPITAL LETTER N WITH GRAVE */
	case 0x01F9: uc = 'n';	break; /* ǹ	SMALL LETTER N WITH GRAVE */
	case 0x01FA: uc = 0xC5;	break; /* Ǻ	CAPITAL LETTER A WITH RING ABOVE AND ACUTE */
	case 0x01FB: uc = 0xE5;	break; /* ǻ	SMALL LETTER A WITH RING ABOVE AND ACUTE */
	case 0x01FC: uc = 0xC6;	break; /* Ǽ	CAPITAL LETTER AE WITH ACUTE */
	case 0x01FD: uc = 0xE6;	break; /* ǽ	SMALL LETTER AE WITH ACUTE */
	case 0x01FE: uc = 0xD8;	break; /* Ǿ	CAPITAL LETTER O WITH STROKE AND ACUTE */
	case 0x01FF: uc = 0xF8;	break; /* ǿ	SMALL LETTER O WITH STROKE AND ACUTE */
	case 0x0200: uc = 0xC0;	break; /* Ȁ	CAPITAL LETTER A WITH DOUBLE GRAVE */
	case 0x0201: uc = 0xC0;	break; /* ȁ	SMALL LETTER A WITH DOUBLE GRAVE */
	case 0x0202: uc = 'A';	break; /* Ȃ	CAPITAL LETTER A WITH INVERTED BREVE */
	case 0x0203: uc = 'a';	break; /* ȃ	SMALL LETTER A WITH INVERTED BREVE */
	case 0x0204: uc = 0xC8;	break; /* Ȅ	CAPITAL LETTER E WITH DOUBLE GRAVE */
	case 0x0205: uc = 0xE0;	break; /* ȅ	SMALL LETTER E WITH DOUBLE GRAVE */
	case 0x0206: uc = 'E';	break; /* Ȇ	CAPITAL LETTER E WITH INVERTED BREVE */
	case 0x0207: uc = 'e';	break; /* ȇ	SMALL LETTER E WITH INVERTED BREVE */
	case 0x0208: uc = 'I';	break; /* Ȉ	CAPITAL LETTER I WITH DOUBLE GRAVE */
	case 0x0209: uc = 0xCC;	break; /* ȉ	SMALL LETTER I WITH DOUBLE GRAVE */
	case 0x020A: uc = 0xEC;	break; /* Ȋ	CAPITAL LETTER I WITH INVERTED BREVE */
	case 0x020B: uc = 'i';	break; /* ȋ	SMALL LETTER I WITH INVERTED BREVE */
	case 0x020C: uc = 0xD2;	break; /* Ȍ	CAPITAL LETTER O WITH DOUBLE GRAVE */
	case 0x020D: uc = 0xF2;	break; /* ȍ	SMALL LETTER O WITH DOUBLE GRAVE */
	case 0x020E: uc = 'O';	break; /* Ȏ	CAPITAL LETTER O WITH INVERTED BREVE */
	case 0x020F: uc = 'o';	break; /* ȏ	SMALL LETTER O WITH INVERTED BREVE */
	case 0x0210: uc = 'R';	break; /* Ȑ	CAPITAL LETTER R WITH DOUBLE GRAVE */
	case 0x0211: uc = 'r';	break; /* ȑ	SMALL LETTER R WITH DOUBLE GRAVE */
	case 0x0212: uc = 'R';	break; /* Ȓ	CAPITAL LETTER R WITH INVERTED BREVE */
	case 0x0213: uc = 'r';	break; /* ȓ	SMALL LETTER R WITH INVERTED BREVE */
	case 0x0214: uc = 'U';	break; /* Ȕ	CAPITAL LETTER U WITH DOUBLE GRAVE */
	case 0x0215: uc = 'u';	break; /* ȕ	SMALL LETTER U WITH DOUBLE GRAVE */
	case 0x0216: uc = 'U';	break; /* Ȗ	CAPITAL LETTER U WITH INVERTED BREVE */
	case 0x0217: uc = 'u';	break; /* ȗ	SMALL LETTER U WITH INVERTED BREVE */
	case 0x0218: uc = 'S';	break; /* Ș	CAPITAL LETTER S WITH COMMA BELOW */
	case 0x0219: uc = 's';	break; /* ș	SMALL LETTER S WITH COMMA BELOW */
	case 0x021A: uc = 'T';	break; /* Ț	CAPITAL LETTER T WITH COMMA BELOW */
	case 0x021B: uc = 't';	break; /* ț	SMALL LETTER T WITH COMMA BELOW */
	case 0x021C: uc = 'Y';	break; /* Ȝ	CAPITAL LETTER YOGH */
	case 0x021D: uc = 'y';	break; /* ȝ	SMALL LETTER YOGH */
	case 0x021E: uc = 'H';	break; /* Ȟ	CAPITAL LETTER H WITH CARON */
	case 0x021F: uc = 'h';	break; /* ȟ	SMALL LETTER H WITH CARON */
	case 0x0220: uc = 'N';	break; /* Ƞ	CAPITAL LETTER N WITH LONG RIGHT LEG */
	case 0x0221: uc = 'd';	break; /* ȡ	SMALL LETTER D WITH CURL */
	case 0x0222: uc = 'O';	break; /* Ȣ	CAPITAL LETTER OU */
	case 0x0223: uc = 'o';	break; /* ȣ	SMALL LETTER OU */
	case 0x0224: uc = 'Z';	break; /* Ȥ	CAPITAL LETTER Z WITH HOOK */
	case 0x0225: uc = 'z';	break; /* ȥ	SMALL LETTER Z WITH HOOK */
	case 0x0226: uc = 'A';	break; /* Ȧ	CAPITAL LETTER A WITH DOT ABOVE */
	case 0x0227: uc = 'a';	break; /* ȧ	SMALL LETTER A WITH DOT ABOVE */
	case 0x0228: uc = 'E';	break; /* Ȩ	CAPITAL LETTER E WITH CEDILLA */
	case 0x0229: uc = 'e';	break; /* ȩ	SMALL LETTER E WITH CEDILLA */
	case 0x022A: uc = 'O';	break; /* Ȫ	CAPITAL LETTER O WITH DIAERESIS AND MACRON */
	case 0x022B: uc = 'o';	break; /* ȫ	SMALL LETTER O WITH DIAERESIS AND MACRON */
	case 0x022C: uc = 'O';	break; /* Ȭ	CAPITAL LETTER O WITH TILDE AND MACRON */
	case 0x022D: uc = 'o';	break; /* ȭ	SMALL LETTER O WITH TILDE AND MACRON */
	case 0x022E: uc = 'O';	break; /* Ȯ	CAPITAL LETTER O WITH DOT ABOVE */
	case 0x022F: uc = 'o';	break; /* ȯ	SMALL LETTER O WITH DOT ABOVE */
	case 0x0230: uc = 'O';	break; /* Ȱ	CAPITAL LETTER O WITH DOT ABOVE AND MACRON */
	case 0x0231: uc = 'o';	break; /* ȱ	SMALL LETTER O WITH DOT ABOVE AND MACRON */
	case 0x0232: uc = 'Y';	break; /* Ȳ	CAPITAL LETTER Y WITH MACRON */
	case 0x0233: uc = 'y';	break; /* ȳ	SMALL LETTER Y WITH MACRON */
	case 0x0234: uc = 'l';	break; /* ȴ	SMALL LETTER L WITH CURL */
	case 0x0235: uc = 'n';	break; /* ȵ	SMALL LETTER N WITH CURL */
	case 0x0236: uc = 't';	break; /* ȶ	SMALL LETTER T WITH CURL */
	case 0x0237: uc = 'd';	break; /* ȷ	SMALL LETTER DOTLESS J */
	case 0x0238: uc = 'd';	break; /* ȸ	SMALL LETTER DB DIGRAPH */
	case 0x0239: uc = 'q';	break; /* ȹ	SMALL LETTER QP DIGRAPH */
	case 0x023A: uc = 'A';	break; /* Ⱥ	CAPITAL LETTER A WITH STROKE */
	case 0x023B: uc = 'C';	break; /* Ȼ	CAPITAL LETTER C WITH STROKE */
	case 0x023C: uc = 'c';	break; /* ȼ	SMALL LETTER C WITH STROKE */
	case 0x023D: uc = 'L';	break; /* Ƚ	CAPITAL LETTER L WITH BAR */
	case 0x023E: uc = 'T';	break; /* Ⱦ	CAPITAL LETTER T WITH DIAGONAL STROKE */
	case 0x023F: uc = 's';	break; /* ȿ	SMALL LETTER S WITH SWASH TAIL */
	case 0x0240: uc = 'z';	break; /* ɀ	SMALL LETTER Z WITH SWASH TAIL */
	case 0x0241: uc = 'G';	break; /* Ɂ	CAPITAL LETTER GLOTTAL STOP */
	case 0x0242: uc = 'g';	break; /* ɂ	SMALL LETTER GLOTTAL STOP */
	case 0x0243: uc = 'B';	break; /* Ƀ	CAPITAL LETTER B WITH STROKE (BITCOIN) */
	case 0x0244: uc = 'U';	break; /* Ʉ	CAPITAL LETTER U BAR */
	case 0x0245: uc = 'T';	break; /* Ʌ	CAPITAL LETTER TURNED V */
	case 0x0246: uc = 'E';	break; /* Ɇ	CAPITAL LETTER E WITH STROKE */
	case 0x0247: uc = 'e';	break; /* ɇ	SMALL LETTER E WITH STROKE */
	case 0x0248: uc = 'J';	break; /* Ɉ	CAPITAL LETTER J WITH STROKE */
	case 0x0249: uc = 'j';	break; /* ɉ	SMALL LETTER J WITH STROKE */
	case 0x024A: uc = 'S';	break; /* Ɋ	CAPITAL LETTER SMALL Q WITH HOOK TAIL */
	case 0x024B: uc = 'q';	break; /* ɋ	SMALL LETTER Q WITH HOOK TAIL */
	case 0x024C: uc = 'R';	break; /* Ɍ	CAPITAL LETTER R WITH STROKE */
	case 0x024D: uc = 'r';	break; /* ɍ	SMALL LETTER R WITH STROKE */
	case 0x024E: uc = 'Y';	break; /* Ɏ	CAPITAL LETTER Y WITH STROKE */
	case 0x024F: uc = 'y';	break; /* ɏ	SMALL LETTER Y WITH STROKE */

	/* Latin Extended Additional */

	case 0x1E00: uc = 0xC5;	break; /* Ḁ	CAPITAL LETTER A WITH RING BELOW */
	case 0x1E01: uc = 0xE5;	break; /* ḁ	SMALL LETTER A WITH RING BELOW */
	case 0x1E02: uc = 'B';	break; /* Ḃ	CAPITAL LETTER B WITH DOT ABOVE */
	case 0x1E03: uc = 'b';	break; /* ḃ	SMALL LETTER B WITH DOT ABOVE */
	case 0x1E04: uc = 'B';	break; /* Ḅ	CAPITAL LETTER B WITH DOT BELOW */
	case 0x1E05: uc = 'b';	break; /* ḅ	SMALL LETTER B WITH DOT BELOW */
	case 0x1E06: uc = 'B';	break; /* Ḇ	CAPITAL LETTER B WITH LINE BELOW */
	case 0x1E07: uc = 'b';	break; /* ḇ	SMALL LETTER B WITH LINE BELOW */
	case 0x1E08: uc = 0xC7;	break; /* Ḉ	CAPITAL LETTER C WITH CEDILLA AND ACUTE */
	case 0x1E09: uc = 0xE7;	break; /* ḉ	SMALL LETTER C WITH CEDILLA AND ACUTE */
	case 0x1E0A: uc = 'D';	break; /* Ḋ	CAPITAL LETTER D WITH DOT ABOVE */
	case 0x1E0B: uc = 'd';	break; /* ḋ	SMALL LETTER D WITH DOT ABOVE */
	case 0x1E0C: uc = 'D';	break; /* Ḍ	CAPITAL LETTER D WITH DOT BELOW */
	case 0x1E0D: uc = 'd';	break; /* ḍ	SMALL LETTER D WITH DOT BELOW */
	case 0x1E0E: uc = 'D';	break; /* Ḏ	CAPITAL LETTER D WITH LINE BELOW */
	case 0x1E0F: uc = 'd';	break; /* ḏ	SMALL LETTER D WITH LINE BELOW */
	case 0x1E10: uc = 'D';	break; /* Ḑ	CAPITAL LETTER D WITH CEDILLA */
	case 0x1E11: uc = 'd';	break; /* ḑ	SMALL LETTER D WITH CEDILLA */
	case 0x1E12: uc = 'D';	break; /* Ḓ	CAPITAL LETTER D WITH CIRCUMFLEX BELOW */
	case 0x1E13: uc = 'd';	break; /* ḓ	SMALL LETTER D WITH CIRCUMFLEX BELOW */
	case 0x1E14: uc = 0xC8;	break; /* Ḕ	CAPITAL LETTER E WITH MACRON AND GRAVE */
	case 0x1E15: uc = 0xE8;	break; /* ḕ	SMALL LETTER E WITH MACRON AND GRAVE */
	case 0x1E16: uc = 0xC9;	break; /* Ḗ	CAPITAL LETTER E WITH MACRON AND ACUTE */
	case 0x1E17: uc = 0xE9;	break; /* ḗ	SMALL LETTER E WITH MACRON AND ACUTE */
	case 0x1E18: uc = 0xCA;	break; /* Ḙ	CAPITAL LETTER E WITH CIRCUMFLEX BELOW */
	case 0x1E19: uc = 0xEA;	break; /* ḙ	SMALL LETTER E WITH CIRCUMFLEX BELOW */
	case 0x1E1A: uc = 'E';	break; /* Ḛ	CAPITAL LETTER E WITH TILDE BELOW */
	case 0x1E1B: uc = 'e';	break; /* ḛ	SMALL LETTER E WITH TILDE BELOW */
	case 0x1E1C: uc = 'E';	break; /* Ḝ	CAPITAL LETTER E WITH CEDILLA AND BREVE */
	case 0x1E1D: uc = 'e';	break; /* ḝ	SMALL LETTER E WITH CEDILLA AND BREVE */
	case 0x1E1E: uc = 'F';	break; /* Ḟ	CAPITAL LETTER F WITH DOT ABOVE */
	case 0x1E1F: uc = 'f';	break; /* ḟ	SMALL LETTER F WITH DOT ABOVE */
	case 0x1E20: uc = 'G';	break; /* Ḡ	CAPITAL LETTER G WITH MACRON */
	case 0x1E21: uc = 'g';	break; /* ḡ	SMALL LETTER G WITH MACRON */
	case 0x1E22: uc = 'H';	break; /* Ḣ	CAPITAL LETTER H WITH DOT ABOVE */
	case 0x1E23: uc = 'h';	break; /* ḣ	SMALL LETTER H WITH DOT ABOVE */
	case 0x1E24: uc = 'H';	break; /* Ḥ	CAPITAL LETTER H WITH DOT BELOW */
	case 0x1E25: uc = 'h';	break; /* ḥ	SMALL LETTER H WITH DOT BELOW */
	case 0x1E26: uc = 'H';	break; /* Ḧ	CAPITAL LETTER H WITH DIAERESIS */
	case 0x1E27: uc = 'h';	break; /* ḧ	SMALL LETTER H WITH DIAERESIS */
	case 0x1E28: uc = 'H';	break; /* Ḩ	CAPITAL LETTER H WITH CEDILLA */
	case 0x1E29: uc = 'h';	break; /* ḩ	SMALL LETTER H WITH CEDILLA */
	case 0x1E2A: uc = 'H';	break; /* Ḫ	CAPITAL LETTER H WITH BREVE BELOW */
	case 0x1E2B: uc = 'h';	break; /* ḫ	SMALL LETTER H WITH BREVE BELOW */
	case 0x1E2C: uc = 'I';	break; /* Ḭ	CAPITAL LETTER I WITH TILDE BELOW */
	case 0x1E2D: uc = 'i';	break; /* ḭ	SMALL LETTER I WITH TILDE BELOW */
	case 0x1E2E: uc = 0xCF;	break; /* Ḯ	CAPITAL LETTER I WITH DIAERESIS AND ACUTE */
	case 0x1E2F: uc = 0xEF;	break; /* ḯ	SMALL LETTER I WITH DIAERESIS AND ACUTE */
	case 0x1E30: uc = 'K';	break; /* Ḱ	CAPITAL LETTER K WITH ACUTE */
	case 0x1E31: uc = 'k';	break; /* ḱ	SMALL LETTER K WITH ACUTE */
	case 0x1E32: uc = 'K';	break; /* Ḳ	CAPITAL LETTER K WITH DOT BELOW */
	case 0x1E33: uc = 'k';	break; /* ḳ	SMALL LETTER K WITH DOT BELOW */
	case 0x1E34: uc = 'K';	break; /* Ḵ	CAPITAL LETTER K WITH LINE BELOW */
	case 0x1E35: uc = 'k';	break; /* ḵ	SMALL LETTER K WITH LINE BELOW */
	case 0x1E36: uc = 'L';	break; /* Ḷ	CAPITAL LETTER L WITH DOT BELOW */
	case 0x1E37: uc = 'l';	break; /* ḷ	SMALL LETTER L WITH DOT BELOW */
	case 0x1E38: uc = 'L';	break; /* Ḹ	CAPITAL LETTER L WITH DOT BELOW AND MACRON */
	case 0x1E39: uc = 'l';	break; /* ḹ	SMALL LETTER L WITH DOT BELOW AND MACRON */
	case 0x1E3A: uc = 'L';	break; /* Ḻ	CAPITAL LETTER L WITH LINE BELOW */
	case 0x1E3B: uc = 'l';	break; /* ḻ	SMALL LETTER L WITH LINE BELOW */
	case 0x1E3C: uc = 'L';	break; /* Ḽ	CAPITAL LETTER L WITH CIRCUMFLEX BELOW */
	case 0x1E3D: uc = 'l';	break; /* ḽ	SMALL LETTER L WITH CIRCUMFLEX BELOW */
	case 0x1E3E: uc = 'M';	break; /* Ḿ	CAPITAL LETTER M WITH ACUTE */
	case 0x1E3F: uc = 'm';	break; /* ḿ	SMALL LETTER M WITH ACUTE */
	case 0x1E40: uc = 'M';	break; /* Ṁ	CAPITAL LETTER M WITH DOT ABOVE */
	case 0x1E41: uc = 'm';	break; /* ṁ	SMALL LETTER M WITH DOT ABOVE */
	case 0x1E42: uc = 'M';	break; /* Ṃ	CAPITAL LETTER M WITH DOT BELOW */
	case 0x1E43: uc = 'm';	break; /* ṃ	SMALL LETTER M WITH DOT BELOW */
	case 0x1E44: uc = 'N';	break; /* Ṅ	CAPITAL LETTER N WITH DOT ABOVE */
	case 0x1E45: uc = 'n';	break; /* ṅ	SMALL LETTER N WITH DOT ABOVE */
	case 0x1E46: uc = 'N';	break; /* Ṇ	CAPITAL LETTER N WITH DOT BELOW */
	case 0x1E47: uc = 'n';	break; /* ṇ	SMALL LETTER N WITH DOT BELOW */
	case 0x1E48: uc = 'N';	break; /* Ṉ	CAPITAL LETTER N WITH LINE BELOW */
	case 0x1E49: uc = 'n';	break; /* ṉ	SMALL LETTER N WITH LINE BELOW */
	case 0x1E4A: uc = 'N';	break; /* Ṋ	CAPITAL LETTER N WITH CIRCUMFLEX BELOW */
	case 0x1E4B: uc = 'n';	break; /* ṋ	SMALL LETTER N WITH CIRCUMFLEX BELOW */
	case 0x1E4C: uc = 'O';	break; /* Ṍ	CAPITAL LETTER O WITH TILDE AND ACUTE */
	case 0x1E4D: uc = 'o';	break; /* ṍ	SMALL LETTER O WITH TILDE AND ACUTE */
	case 0x1E4E: uc = 'O';	break; /* Ṏ	CAPITAL LETTER O WITH TILDE AND DIAERESIS */
	case 0x1E4F: uc = 'o';	break; /* ṏ	SMALL LETTER O WITH TILDE AND DIAERESIS */
	case 0x1E50: uc = 'O';	break; /* Ṑ	CAPITAL LETTER O WITH MACRON AND GRAVE */
	case 0x1E51: uc = 'o';	break; /* ṑ	SMALL LETTER O WITH MACRON AND GRAVE */
	case 0x1E52: uc = 'O';	break; /* Ṓ	CAPITAL LETTER O WITH MACRON AND ACUTE */
	case 0x1E53: uc = 'o';	break; /* ṓ	SMALL LETTER O WITH MACRON AND ACUTE */
	case 0x1E54: uc = 'P';	break; /* Ṕ	CAPITAL LETTER P WITH ACUTE */
	case 0x1E55: uc = 'p';	break; /* ṕ	SMALL LETTER P WITH ACUTE */
	case 0x1E56: uc = 'P';	break; /* Ṗ	CAPITAL LETTER P WITH DOT ABOVE */
	case 0x1E57: uc = 'p';	break; /* ṗ	SMALL LETTER P WITH DOT ABOVE */
	case 0x1E58: uc = 'R';	break; /* Ṙ	CAPITAL LETTER R WITH DOT ABOVE */
	case 0x1E59: uc = 'r';	break; /* ṙ	SMALL LETTER R WITH DOT ABOVE */
	case 0x1E5A: uc = 'R';	break; /* Ṛ	CAPITAL LETTER R WITH DOT BELOW */
	case 0x1E5B: uc = 'r';	break; /* ṛ	SMALL LETTER R WITH DOT BELOW */
	case 0x1E5C: uc = 'R';	break; /* Ṝ	CAPITAL LETTER R WITH DOT BELOW AND MACRON */
	case 0x1E5D: uc = 'r';	break; /* ṝ	SMALL LETTER R WITH DOT BELOW AND MACRON */
	case 0x1E5E: uc = 'R';	break; /* Ṟ	CAPITAL LETTER R WITH LINE BELOW */
	case 0x1E5F: uc = 'r';	break; /* ṟ	SMALL LETTER R WITH LINE BELOW */
	case 0x1E60: uc = 'S';	break; /* Ṡ	CAPITAL LETTER S WITH DOT ABOVE */
	case 0x1E61: uc = 's';	break; /* ṡ	SMALL LETTER S WITH DOT ABOVE */
	case 0x1E62: uc = 'S';	break; /* Ṣ	CAPITAL LETTER S WITH DOT BELOW */
	case 0x1E63: uc = 's';	break; /* ṣ	SMALL LETTER S WITH DOT BELOW */
	case 0x1E64: uc = 'S';	break; /* Ṥ	CAPITAL LETTER S WITH ACUTE AND DOT ABOVE */
	case 0x1E65: uc = 's';	break; /* ṥ	SMALL LETTER S WITH ACUTE AND DOT ABOVE */
	case 0x1E66: uc = 'S';	break; /* Ṧ	CAPITAL LETTER S WITH CARON AND DOT ABOVE */
	case 0x1E67: uc = 's';	break; /* ṧ	SMALL LETTER S WITH CARON AND DOT ABOVE */
	case 0x1E68: uc = 'S';	break; /* Ṩ	CAPITAL LETTER S WITH DOT BELOW AND DOT ABOVE */
	case 0x1E69: uc = 's';	break; /* ṩ	SMALL LETTER S WITH DOT BELOW AND DOT ABOVE */
	case 0x1E6A: uc = 'T';	break; /* Ṫ	CAPITAL LETTER T WITH DOT ABOVE */
	case 0x1E6B: uc = 't';	break; /* ṫ	SMALL LETTER T WITH DOT ABOVE */
	case 0x1E6C: uc = 'T';	break; /* Ṭ	CAPITAL LETTER T WITH DOT BELOW */
	case 0x1E6D: uc = 't';	break; /* ṭ	SMALL LETTER T WITH DOT BELOW */
	case 0x1E6E: uc = 'T';	break; /* Ṯ	CAPITAL LETTER T WITH LINE BELOW */
	case 0x1E6F: uc = 't';	break; /* ṯ	SMALL LETTER T WITH LINE BELOW */
	case 0x1E70: uc = 'T';	break; /* Ṱ	CAPITAL LETTER T WITH CIRCUMFLEX BELOW */
	case 0x1E71: uc = 't';	break; /* ṱ	SMALL LETTER T WITH CIRCUMFLEX BELOW */
	case 0x1E72: uc = 0xDC;	break; /* Ṳ	CAPITAL LETTER U WITH DIAERESIS BELOW */
	case 0x1E73: uc = 0xFC;	break; /* ṳ	SMALL LETTER U WITH DIAERESIS BELOW */
	case 0x1E74: uc = 'U';	break; /* Ṵ	CAPITAL LETTER U WITH TILDE BELOW */
	case 0x1E75: uc = 'u';	break; /* ṵ	SMALL LETTER U WITH TILDE BELOW */
	case 0x1E76: uc = 'U';	break; /* Ṷ	CAPITAL LETTER U WITH CIRCUMFLEX BELOW */
	case 0x1E77: uc = 'u';	break; /* ṷ	SMALL LETTER U WITH CIRCUMFLEX BELOW */
	case 0x1E78: uc = 'U';	break; /* Ṹ	CAPITAL LETTER U WITH TILDE AND ACUTE */
	case 0x1E79: uc = 'u';	break; /* ṹ	SMALL LETTER U WITH TILDE AND ACUTE */
	case 0x1E7A: uc = 'U';	break; /* Ṻ	CAPITAL LETTER U WITH MACRON AND DIAERESIS */
	case 0x1E7B: uc = 'u';	break; /* ṻ	SMALL LETTER U WITH MACRON AND DIAERESIS */
	case 0x1E7C: uc = 'V';	break; /* Ṽ	CAPITAL LETTER V WITH TILDE */
	case 0x1E7D: uc = 'v';	break; /* ṽ	SMALL LETTER V WITH TILDE */
	case 0x1E7E: uc = 'V';	break; /* Ṿ	CAPITAL LETTER V WITH DOT BELOW */
	case 0x1E7F: uc = 'v';	break; /* ṿ	SMALL LETTER V WITH DOT BELOW */
	case 0x1E80: uc = 'W';	break; /* Ẁ	CAPITAL LETTER W WITH GRAVE */
	case 0x1E81: uc = 'w';	break; /* ẁ	SMALL LETTER W WITH GRAVE */
	case 0x1E82: uc = 'W';	break; /* Ẃ	CAPITAL LETTER W WITH ACUTE */
	case 0x1E83: uc = 'w';	break; /* ẃ	SMALL LETTER W WITH ACUTE */
	case 0x1E84: uc = 'W';	break; /* Ẅ	CAPITAL LETTER W WITH DIAERESIS */
	case 0x1E85: uc = 'w';	break; /* ẅ	SMALL LETTER W WITH DIAERESIS */
	case 0x1E86: uc = 'W';	break; /* Ẇ	CAPITAL LETTER W WITH DOT ABOVE */
	case 0x1E87: uc = 'w';	break; /* ẇ	SMALL LETTER W WITH DOT ABOVE */
	case 0x1E88: uc = 'W';	break; /* Ẉ	CAPITAL LETTER W WITH DOT BELOW */
	case 0x1E89: uc = 'w';	break; /* ẉ	SMALL LETTER W WITH DOT BELOW */
	case 0x1E8A: uc = 'X';	break; /* Ẋ	CAPITAL LETTER X WITH DOT ABOVE */
	case 0x1E8B: uc = 'x';	break; /* ẋ	SMALL LETTER X WITH DOT ABOVE */
	case 0x1E8C: uc = 'X';	break; /* Ẍ	CAPITAL LETTER X WITH DIAERESIS */
	case 0x1E8D: uc = 'x';	break; /* ẍ	SMALL LETTER X WITH DIAERESIS */
	case 0x1E8E: uc = 'Y';	break; /* Ẏ	CAPITAL LETTER Y WITH DOT ABOVE */
	case 0x1E8F: uc = 'y';	break; /* ẏ	SMALL LETTER Y WITH DOT ABOVE */
	case 0x1E90: uc = 'Z';	break; /* Ẑ	CAPITAL LETTER Z WITH CIRCUMFLEX */
	case 0x1E91: uc = 'z';	break; /* ẑ	SMALL LETTER Z WITH CIRCUMFLEX */
	case 0x1E92: uc = 'Z';	break; /* Ẓ	CAPITAL LETTER Z WITH DOT BELOW */
	case 0x1E93: uc = 'z';	break; /* ẓ	SMALL LETTER Z WITH DOT BELOW */
	case 0x1E94: uc = 'Z';	break; /* Ẕ	CAPITAL LETTER Z WITH LINE BELOW */
	case 0x1E95: uc = 'z';	break; /* ẕ	SMALL LETTER Z WITH LINE BELOW */
	case 0x1E96: uc = 'h';	break; /* ẖ	SMALL LETTER H WITH LINE BELOW */
	case 0x1E97: uc = 't';	break; /* ẗ	SMALL LETTER T WITH DIAERESIS */
	case 0x1E98: uc = 'w';	break; /* ẘ	SMALL LETTER W WITH RING ABOVE */
	case 0x1E99: uc = 'y';	break; /* ẙ	SMALL LETTER Y WITH RING ABOVE */
	case 0x1E9A: uc = 'a';	break; /* ẚ	SMALL LETTER A WITH RIGHT HALF RING */
	case 0x1E9B: uc = 'l';	break; /* ẛ	SMALL LETTER LONG S WITH DOT ABOVE */
	case 0x1E9C: uc = 'l';	break; /* ẜ	SMALL LETTER LONG S WITH DIAGONAL STROKE */
	case 0x1E9D: uc = 'l';	break; /* ẝ	SMALL LETTER LONG S WITH HIGH STROKE */
	case 0x1E9E: uc = 'S';	break; /* ẞ	CAPITAL LETTER SHARP S */
	case 0x1E9F: uc = 'd';	break; /* ẟ	SMALL LETTER DELTA */
	case 0x1EA0: uc = 'A';	break; /* Ạ	CAPITAL LETTER A WITH DOT BELOW */
	case 0x1EA1: uc = 'a';	break; /* ạ	SMALL LETTER A WITH DOT BELOW */
	case 0x1EA2: uc = 'A';	break; /* Ả	CAPITAL LETTER A WITH HOOK ABOVE */
	case 0x1EA3: uc = 'a';	break; /* ả	SMALL LETTER A WITH HOOK ABOVE */
	case 0x1EA4: uc = 0xC2;	break; /* Ấ	CAPITAL LETTER A WITH CIRCUMFLEX AND ACUTE */
	case 0x1EA5: uc = 0xE2;	break; /* ấ	SMALL LETTER A WITH CIRCUMFLEX AND ACUTE */
	case 0x1EA6: uc = 0xC2;	break; /* Ầ	CAPITAL LETTER A WITH CIRCUMFLEX AND GRAVE */
	case 0x1EA7: uc = 0xE2;	break; /* ầ	SMALL LETTER A WITH CIRCUMFLEX AND GRAVE */
	case 0x1EA8: uc = 0xC2;	break; /* Ẩ	CAPITAL LETTER A WITH CIRCUMFLEX AND HOOK ABOVE */
	case 0x1EA9: uc = 0xE2;	break; /* ẩ	SMALL LETTER A WITH CIRCUMFLEX AND HOOK ABOVE */
	case 0x1EAA: uc = 0xC2;	break; /* Ẫ	CAPITAL LETTER A WITH CIRCUMFLEX AND TILDE */
	case 0x1EAB: uc = 0xE2;	break; /* ẫ	SMALL LETTER A WITH CIRCUMFLEX AND TILDE */
	case 0x1EAC: uc = 0xC2;	break; /* Ậ	CAPITAL LETTER A WITH CIRCUMFLEX AND DOT BELOW */
	case 0x1EAD: uc = 'a';	break; /* ậ	SMALL LETTER A WITH CIRCUMFLEX AND DOT BELOW */
	case 0x1EAE: uc = 0xC1;	break; /* Ắ	CAPITAL LETTER A WITH BREVE AND ACUTE */
	case 0x1EAF: uc = 0xE1;	break; /* ắ	SMALL LETTER A WITH BREVE AND ACUTE */
	case 0x1EB0: uc = 0xC0;	break; /* Ằ	CAPITAL LETTER A WITH BREVE AND GRAVE */
	case 0x1EB1: uc = 0xE0;	break; /* ằ	SMALL LETTER A WITH BREVE AND GRAVE */
	case 0x1EB2: uc = 'A';	break; /* Ẳ	CAPITAL LETTER A WITH BREVE AND HOOK ABOVE */
	case 0x1EB3: uc = 'a';	break; /* ẳ	SMALL LETTER A WITH BREVE AND HOOK ABOVE */
	case 0x1EB4: uc = 'A';	break; /* Ẵ	CAPITAL LETTER A WITH BREVE AND TILDE */
	case 0x1EB5: uc = 'a';	break; /* ẵ	SMALL LETTER A WITH BREVE AND TILDE */
	case 0x1EB6: uc = 'A';	break; /* Ặ	CAPITAL LETTER A WITH BREVE AND DOT BELOW */
	case 0x1EB7: uc = 'a';	break; /* ặ	SMALL LETTER A WITH BREVE AND DOT BELOW */
	case 0x1EB8: uc = 'E';	break; /* Ẹ	CAPITAL LETTER E WITH DOT BELOW */
	case 0x1EB9: uc = 'e';	break; /* ẹ	SMALL LETTER E WITH DOT BELOW */
	case 0x1EBA: uc = 'E';	break; /* Ẻ	CAPITAL LETTER E WITH HOOK ABOVE */
	case 0x1EBB: uc = 'e';	break; /* ẻ	SMALL LETTER E WITH HOOK ABOVE */
	case 0x1EBC: uc = 'E';	break; /* Ẽ	CAPITAL LETTER E WITH TILDE */
	case 0x1EBD: uc = 'e';	break; /* ẽ	SMALL LETTER E WITH TILDE */
	case 0x1EBE: uc = 0xCA;	break; /* Ế	CAPITAL LETTER E WITH CIRCUMFLEX AND ACUTE */
	case 0x1EBF: uc = 0xEA;	break; /* ế	SMALL LETTER E WITH CIRCUMFLEX AND ACUTE */
	case 0x1EC0: uc = 0xCA;	break; /* Ề	CAPITAL LETTER E WITH CIRCUMFLEX AND GRAVE */
	case 0x1EC1: uc = 0xEA;	break; /* ề	SMALL LETTER E WITH CIRCUMFLEX AND GRAVE */
	case 0x1EC2: uc = 0xCA;	break; /* Ể	CAPITAL LETTER E WITH CIRCUMFLEX AND HOOK ABOVE */
	case 0x1EC3: uc = 0xEA;	break; /* ể	SMALL LETTER E WITH CIRCUMFLEX AND HOOK ABOVE */
	case 0x1EC4: uc = 0xCA;	break; /* Ễ	CAPITAL LETTER E WITH CIRCUMFLEX AND TILDE */
	case 0x1EC5: uc = 0xEA;	break; /* ễ	SMALL LETTER E WITH CIRCUMFLEX AND TILDE */
	case 0x1EC6: uc = 0xCA;	break; /* Ệ	CAPITAL LETTER E WITH CIRCUMFLEX AND DOT BELOW */
	case 0x1EC7: uc = 0xEA;	break; /* ệ	SMALL LETTER E WITH CIRCUMFLEX AND DOT BELOW */
	case 0x1EC8: uc = 'I';	break; /* Ỉ	CAPITAL LETTER I WITH HOOK ABOVE */
	case 0x1EC9: uc = 'i';	break; /* ỉ	SMALL LETTER I WITH HOOK ABOVE */
	case 0x1ECA: uc = 'I';	break; /* Ị	CAPITAL LETTER I WITH DOT BELOW */
	case 0x1ECB: uc = 'i';	break; /* ị	SMALL LETTER I WITH DOT BELOW */
	case 0x1ECC: uc = 'O';	break; /* Ọ	CAPITAL LETTER O WITH DOT BELOW */
	case 0x1ECD: uc = 'o';	break; /* ọ	SMALL LETTER O WITH DOT BELOW */
	case 0x1ECE: uc = 'O';	break; /* Ỏ	CAPITAL LETTER O WITH HOOK ABOVE */
	case 0x1ECF: uc = 'o';	break; /* ỏ	SMALL LETTER O WITH HOOK ABOVE */
	case 0x1ED0: uc = 0xD4;	break; /* Ố	CAPITAL LETTER O WITH CIRCUMFLEX AND ACUTE */
	case 0x1ED1: uc = 0xF4;	break; /* ố	SMALL LETTER O WITH CIRCUMFLEX AND ACUTE */
	case 0x1ED2: uc = 0xD4;	break; /* Ồ	CAPITAL LETTER O WITH CIRCUMFLEX AND GRAVE */
	case 0x1ED3: uc = 0xF4;	break; /* ồ	SMALL LETTER O WITH CIRCUMFLEX AND GRAVE */
	case 0x1ED4: uc = 0xD4;	break; /* Ổ	CAPITAL LETTER O WITH CIRCUMFLEX AND HOOK ABOVE */
	case 0x1ED5: uc = 0xF4;	break; /* ổ	SMALL LETTER O WITH CIRCUMFLEX AND HOOK ABOVE */
	case 0x1ED6: uc = 0xD4;	break; /* Ỗ	CAPITAL LETTER O WITH CIRCUMFLEX AND TILDE */
	case 0x1ED7: uc = 0xF4;	break; /* ỗ	SMALL LETTER O WITH CIRCUMFLEX AND TILDE */
	case 0x1ED8: uc = 0xD4;	break; /* Ộ	CAPITAL LETTER O WITH CIRCUMFLEX AND DOT BELOW */
	case 0x1ED9: uc = 0xF4;	break; /* ộ	SMALL LETTER O WITH CIRCUMFLEX AND DOT BELOW */
	case 0x1EDA: uc = 0xD3;	break; /* Ớ	CAPITAL LETTER O WITH HORN AND ACUTE */
	case 0x1EDB: uc = 0xF3;	break; /* ớ	SMALL LETTER O WITH HORN AND ACUTE */
	case 0x1EDC: uc = 0xD2;	break; /* Ờ	CAPITAL LETTER O WITH HORN AND GRAVE */
	case 0x1EDD: uc = 0xF2;	break; /* ờ	SMALL LETTER O WITH HORN AND GRAVE */
	case 0x1EDE: uc = 'O';	break; /* Ở	CAPITAL LETTER O WITH HORN AND HOOK ABOVE */
	case 0x1EDF: uc = 'o';	break; /* ở	SMALL LETTER O WITH HORN AND HOOK ABOVE */
	case 0x1EE0: uc = 'O';	break; /* Ỡ	CAPITAL LETTER O WITH HORN AND TILDE */
	case 0x1EE1: uc = 'o';	break; /* ỡ	SMALL LETTER O WITH HORN AND TILDE */
	case 0x1EE2: uc = 'O';	break; /* Ợ	CAPITAL LETTER O WITH HORN AND DOT BELOW */
	case 0x1EE3: uc = 'o';	break; /* ợ	SMALL LETTER O WITH HORN AND DOT BELOW */
	case 0x1EE4: uc = 'U';	break; /* Ụ	CAPITAL LETTER U WITH DOT BELOW */
	case 0x1EE5: uc = 'u';	break; /* ụ	SMALL LETTER U WITH DOT BELOW */
	case 0x1EE6: uc = 'U';	break; /* Ủ	CAPITAL LETTER U WITH HOOK ABOVE */
	case 0x1EE7: uc = 'u';	break; /* ủ	SMALL LETTER U WITH HOOK ABOVE */
	case 0x1EE8: uc = 0xDA;	break; /* Ứ	CAPITAL LETTER U WITH HORN AND ACUTE */
	case 0x1EE9: uc = 0xFA;	break; /* ứ	SMALL LETTER U WITH HORN AND ACUTE */
	case 0x1EEA: uc = 0xD9;	break; /* Ừ	CAPITAL LETTER U WITH HORN AND GRAVE */
	case 0x1EEB: uc = 0xF9;	break; /* ừ	SMALL LETTER U WITH HORN AND GRAVE */
	case 0x1EEC: uc = 'U';	break; /* Ử	CAPITAL LETTER U WITH HORN AND HOOK ABOVE */
	case 0x1EED: uc = 'u';	break; /* ử	SMALL LETTER U WITH HORN AND HOOK ABOVE */
	case 0x1EEE: uc = 'U';	break; /* Ữ	CAPITAL LETTER U WITH HORN AND TILDE */
	case 0x1EEF: uc = 'u';	break; /* ữ	SMALL LETTER U WITH HORN AND TILDE */
	case 0x1EF0: uc = 'U';	break; /* Ự	CAPITAL LETTER U WITH HORN AND DOT BELOW */
	case 0x1EF1: uc = 'u';	break; /* ự	SMALL LETTER U WITH HORN AND DOT BELOW */
	case 0x1EF2: uc = 'Y';	break; /* Ỳ	CAPITAL LETTER Y WITH GRAVE */
	case 0x1EF3: uc = 'y';	break; /* ỳ	SMALL LETTER Y WITH GRAVE */
	case 0x1EF4: uc = 'Y';	break; /* Ỵ	CAPITAL LETTER Y WITH DOT BELOW */
	case 0x1EF5: uc = 'y';	break; /* ỵ	SMALL LETTER Y WITH DOT BELOW */
	case 0x1EF6: uc = 'Y';	break; /* Ỷ	CAPITAL LETTER Y WITH HOOK ABOVE */
	case 0x1EF7: uc = 'y';	break; /* ỷ	SMALL LETTER Y WITH HOOK ABOVE */
	case 0x1EF8: uc = 'Y';	break; /* Ỹ	CAPITAL LETTER Y WITH TILDE */
	case 0x1EF9: uc = 'y';	break; /* ỹ	SMALL LETTER Y WITH TILDE */
	case 0x1EFA: uc = 'M';	break; /* Ỻ	CAPITAL LETTER MIDDLE-WELSH LL */
	case 0x1EFB: uc = 'l';	break; /* ỻ	SMALL LETTER MIDDLE-WELSH LL */
	case 0x1EFC: uc = 'V';	break; /* Ỽ	CAPITAL LETTER MIDDLE-WELSH V */
	case 0x1EFD: uc = 'v';	break; /* ỽ	SMALL LETTER MIDDLE-WELSH V */
	case 0x1EFE: uc = 'Y';	break; /* Ỿ	CAPITAL LETTER Y WITH LOOP */
	case 0x1EFF: uc = 'y';	break; /* ỿ	SMALL LETTER Y WITH LOOP */

        default:
          break;
        }

      if (uc > 0xFF)
        /* "Inverted question mark" looks enough like 0xFFFD,
           the "Unicode Replacement Character". */
        uc = (ascii_p ? '#' : '\277');

      if (ascii_p)	/* Map Latin1 to the closest ASCII versions. */
        {
          const char * const latin1_to_ascii[96] = {
            " ",  "!",   "C",  "#",  "#",   "Y",   "|",   "SS",
            "_",  "(c)", "#",  "<",  "=",   "-",   "(r)", "_",
            "#",  "+-",  "2",  "3",  "'",   "u",   "PP",  ".",
            ",",  "1",   "o",  ">",  "1/4", "1/2", "3/4", "?",
            "A",  "A",   "A",  "A",  "A",   "A",   "AE",  "C",
            "E",  "E",   "E",  "E",  "I",   "I",   "I",   "I",
            "D",  "N",   "O",  "O",  "O",   "O",   "O",   "x",
            "0",  "U",   "U",  "U",  "U",   "Y",   "p",   "S",
            "a",  "a",   "a",  "a",  "a",   "a",   "ae",  "c",
            "e",  "e",   "e",  "e",  "i",   "i",   "i",   "i",
            "o",  "n",   "o",  "o",  "o",   "o",   "o",   "/",
            "o",  "u",   "u",  "u",  "u",   "y",   "p",   "y" };
          if (uc >= 0xA0)
            {
              const char *c2 = latin1_to_ascii[uc - 0xA0];
              while (*c2) { *out++ = *c2++; }
              uc = 0;
            }
        }

      if (uc > 0)
        *out++ = (unsigned char) uc;
    }
  *out = 0;

  /* shrink */
  ret = (unsigned char *) realloc (ret, (out - ret + 1) * sizeof(*ret));

  return (char *) ret;
}


/*************************************************************************

 cd ../hacks ; make test-utf8wc

 *************************************************************************/

#ifdef SELFTEST

/* Convert a UTF8 string to Unicode and back again.
 */
static char *
split_and_join (const char *string)
{
  const unsigned char *in = (const unsigned char *) string;
  int len = strlen (string);
  const unsigned char *end = in + len;
  unsigned long *unicode = (unsigned long *)
    malloc((len + 1) * sizeof(*unicode));
  int i = 0;
  char *ret, *out, *out_end;

  while (in < end)
    {
      long len2 = utf8_decode (in, len, &unicode[i]);
      i++;
      in += len2;
    }
  unicode[i] = 0;

  i = i*6 + 1;
  out = ret = (char *) malloc(i);
  out_end = out + i;
  i = 0;
  while (unicode[i])
    {
      int len2 = utf8_encode (unicode[i], out, out_end - out);
      out += len2;
      i++;
    }
  *out = 0;
  free (unicode);

  return ret;
}


static void
LOG (FILE *out, const char *prefix, const char *s)
{
  fprintf (out, "%6s: \"", prefix);
  while (*s)
    {
      unsigned char c = *s;
      if (c == '"' || c == '\\') fprintf(out, "\\%c", c);
      else if (c < 32 || c >= 127) fprintf(out, "\\%03o", c);
      else fprintf (out, "%c", c);
      s++;
    }
  fprintf (out, "\"\n");
}


int
main (int argc, char **argv)
{
  /* Adapted from http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
   */

#  define URC "\357\277\275"   /* 0xFFFD, "Unicode Replacement Character" */

  static const struct { const char *name, *in, *target, *target2; } tests[] = {
    /* 1  Some correct UTF-8 text */

    /* The Greek word 'kosme': */
    { "1", "\316\272\341\275\271\317\203\316\274\316\265" },


    /* 2  Boundary condition test cases */

    /* 2.1  First possible sequence of a certain length */

    { "2.1.1", /*  1 byte  (U-00000000): */  "\000" },
    { "2.1.2", /*  2 bytes (U-00000080): */  "\302\200" },
    { "2.1.3", /*  3 bytes (U-00000800): */  "\340\240\200" },
    { "2.1.4", /*  4 bytes (U-00010000): */  "\360\220\200\200", 0, URC },
    { "2.1.5", /*  5 bytes (U-00200000): */  "\370\210\200\200\200", URC },
    { "2.1.6", /*  6 bytes (U-04000000): */  "\374\204\200\200\200\200", URC },

    /* 2.2  Last possible sequence of a certain length */

    { "2.2.1", /*  1 byte  (U-0000007F): */  "\177" },
    { "2.2.2", /*  2 bytes (U-000007FF): */  "\337\277" },
    { "2.2.3", /*  3 bytes (U-0000FFFF): */  "\357\277\277" },
    { "2.2.4", /*  4 bytes (U-001FFFFF): */  "\367\277\277\277", URC },
    { "2.2.5", /*  5 bytes (U-03FFFFFF): */  "\373\277\277\277\277", URC },
    { "2.2.6", /*  6 bytes (U-7FFFFFFF): */  "\375\277\277\277\277\277", URC },

    /* 2.3  Other boundary conditions */

    { "2.3.1", /*  U-0000D7FF = ed 9f bf = */	 "\355\237\277" },
    { "2.3.2", /*  U-0000E000 = ee 80 80 = */	 "\356\200\200" },
    { "2.3.3", /*  U-0000FFFD = ef bf bd = */	 URC },
    { "2.3.4", /*  U-0010FFFF = f4 8f bf bf = */ "\364\217\277\277", 0, URC },
    { "2.3.5", /*  U-00110000 = f4 90 80 80 = */ "\364\220\200\200", URC },


    /* 3  Malformed sequences */

    /* 3.1  Unexpected continuation bytes */

    /* Each unexpected continuation byte should be separately signalled as a
       malformed sequence of its own. */

    { "3.1.1", /*  First continuation byte 0x80: */ "\200", URC },
    { "3.1.2", /*  Last  continuation byte 0xbf: */ "\277", URC },
    { "3.1.3", /*  2 continuation bytes: */ "\200\277",     URC URC },
    { "3.1.4", /*  3 continuation bytes: */ "\200\277\200", URC URC URC },
    { "3.1.5", /*  4 continuation bytes: */ "\200\277\200\277",
      URC URC URC URC },
    { "3.1.6", /*  5 continuation bytes: */ "\200\277\200\277\200",
      URC URC URC URC URC },
    { "3.1.7", /*  6 continuation bytes: */ "\200\277\200\277\200\277",
      URC URC URC URC URC URC },
    { "3.1.8", /*  7 continuation bytes: */ "\200\277\200\277\200\277\200",
      URC URC URC URC URC URC URC },

    { "3.1.9", /* Sequence of all 64 possible continuation bytes (0x80-0xbf):*/

      "\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217"
      "\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237"
      "\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257"
      "\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277",
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC },

    /* 3.2  Lonely start characters */

    { "3.2.1", /*  All 32 first bytes of 2-byte sequences (0xc0-0xdf),
                   each followed by a space character: */

      "\300 \301 \302 \303 \304 \305 \306 \307 \310 \311 \312 \313 \314 "
      "\315 \316 \317 \320 \321 \322 \323 \324 \325 \326 \327 \330 \331 "
      "\332 \333 \334 \335 \336 \337 ",
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC },

    { "3.2.2", /*  All 16 first bytes of 3-byte sequences (0xe0-0xef),
                   each followed by a space character: */
      "\340 \341 \342 \343 \344 \345 \346 \347 "
      "\350 \351 \352 \353 \354 \355 \356 \357 ",
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC },

    { "3.2.3", /*  All 8 first bytes of 4-byte sequences (0xf0-0xf7),
                   each followed by a space character: */
      URC URC URC URC URC URC URC URC },

    { "3.2.4", /*  All 4 first bytes of 5-byte sequences (0xf8-0xfb),
                   each followed by a space character: */
      "\370 \371 \372 \373 ",
      URC URC URC URC },

    { "3.2.5", /*  All 2 first bytes of 6-byte sequences (0xfc-0xfd),
                   each followed by a space character: */
      "\374 \375 ", URC URC },

    /* 3.3  Sequences with last continuation byte missing */

    /* All bytes of an incomplete sequence should be signalled as a single
       malformed sequence, i.e., you should see only a single replacement
       character in each of the next 10 tests. (Characters as in section 2) */

    { "3.3.1", /*  2-byte sequence with last byte missing (U+0000): */
      "\300", URC },
    { "3.3.2", /*  3-byte sequence with last byte missing (U+0000): */
      "\340\200", URC },
    { "3.3.3", /*  4-byte sequence with last byte missing (U+0000): */
      "\360\200\200", URC },
    { "3.3.4", /*  5-byte sequence with last byte missing (U+0000): */
      "\370\200\200\200", URC },
    { "3.3.5", /*  6-byte sequence with last byte missing (U+0000): */
      "\374\200\200\200\200", URC },
    { "3.3.6", /*  2-byte sequence with last byte missing (U-000007FF): */
      "\337", URC },
    { "3.3.7", /*  3-byte sequence with last byte missing (U-0000FFFF): */
      "\357\277", URC },
    { "3.3.8", /*  4-byte sequence with last byte missing (U-001FFFFF): */
      "\367\277\277", URC },
    { "3.3.9", /*  5-byte sequence with last byte missing (U-03FFFFFF): */
      "\373\277\277\277", URC },
    { "3.3.10", /* 6-byte sequence with last byte missing (U-7FFFFFFF): */
      "\375\277\277\277\277", URC },

    /* 3.4  Concatenation of incomplete sequences */

    /* All the 10 sequences of 3.3 concatenated, you should see 10 malformed
       sequences being signalled: */

    { "3.4",   "\300\340\200\360\200\200\370\200\200\200\374\200\200\200\200"
      "\337\357\277\367\277\277\373\277\277\277\375\277\277\277\277",
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC },

    /* 3.5  Impossible bytes */

    /* The following two bytes cannot appear in a correct UTF-8 string */

    { "3.5.1", /*  fe = */	"\376", URC },
    { "3.5.2", /*  ff = */	"\377", URC },
    { "3.5.3", /*  fe fe ff ff = */	"\376\376\377\377", URC URC URC URC },


    /* 4  Overlong sequences */

    /* 4.1  Examples of an overlong ASCII character */

    { "4.1.1", /* U+002F = c0 af             = */ "\300\257", URC },
    { "4.1.2", /* U+002F = e0 80 af          = */ "\340\200\257", URC },
    { "4.1.3", /* U+002F = f0 80 80 af       = */ "\360\200\200\257", URC },
    { "4.1.4", /* U+002F = f8 80 80 80 af    = */ "\370\200\200\200\257",
      URC },
    { "4.1.5", /* U+002F = fc 80 80 80 80 af = */ "\374\200\200\200\200\257",
      URC },

    /* 4.2  Maximum overlong sequences */

    { "4.2.1", /*  U-0000007F = c1 bf             = */ "\301\277", URC },
    { "4.2.2", /*  U-000007FF = e0 9f bf          = */ "\340\237\277", URC },
    { "4.2.3", /*  U-0000FFFF = f0 8f bf bf       = */ "\360\217\277\277",
      URC },
    { "4.2.4", /*  U-001FFFFF = f8 87 bf bf bf    = */ "\370\207\277\277\277",
      URC },
    { "4.2.5", /*  U-03FFFFFF = fc 83 bf bf bf bf = */  URC },

    /* 4.3  Overlong representation of the NUL character */

    { "4.3.1", /*  U+0000 = c0 80             = */  "\300\200", URC },
    { "4.3.2", /*  U+0000 = e0 80 80          = */  "\340\200\200", URC },
    { "4.3.3", /*  U+0000 = f0 80 80 80       = */  "\360\200\200\200", URC },
    { "4.3.4", /*  U+0000 = f8 80 80 80 80    = */  "\370\200\200\200\200",
      URC },
    { "4.3.5", /*  U+0000 = fc 80 80 80 80 80 = */  "\374\200\200\200\200\200",
      URC },


    /* 5  Illegal code positions */

    /* 5.1 Single UTF-16 surrogates */

    { "5.1.1", /*  U+D800 = ed a0 80 = */	"\355\240\200", URC },
    { "5.1.2", /*  U+DB7F = ed ad bf = */	"\355\255\277", URC },
    { "5.1.3", /*  U+DB80 = ed ae 80 = */	"\355\256\200", URC },
    { "5.1.4", /*  U+DBFF = ed af bf = */	"\355\257\277", URC },
    { "5.1.5", /*  U+DC00 = ed b0 80 = */	"\355\260\200", URC },
    { "5.1.6", /*  U+DF80 = ed be 80 = */	"\355\276\200", URC },
    { "5.1.7", /*  U+DFFF = ed bf bf = */	"\355\277\277", URC },

    /* 5.2 Paired UTF-16 surrogates */

    { "5.2.1", /*  U+D800 U+DC00 = ed a0 80 ed b0 80 = */ URC URC },
    { "5.2.2", /*  U+D800 U+DFFF = ed a0 80 ed bf bf = */ URC URC },
    { "5.2.3", /*  U+DB7F U+DC00 = ed ad bf ed b0 80 = */ URC URC },
    { "5.2.4", /*  U+DB7F U+DFFF = ed ad bf ed bf bf = */ URC URC },
    { "5.2.5", /*  U+DB80 U+DC00 = ed ae 80 ed b0 80 = */ URC URC },
    { "5.2.6", /*  U+DB80 U+DFFF = ed ae 80 ed bf bf = */ URC URC },
    { "5.2.7", /*  U+DBFF U+DC00 = ed af bf ed b0 80 = */ URC URC },
    { "5.2.8", /*  U+DBFF U+DFFF = ed af bf ed bf bf = */ URC URC },

    /* 5.3 Other illegal code positions */

    { "5.3.1", /*  U+FFFE = ef bf be = */	"\357\277\276" },
    { "5.3.2", /*  U+FFFF = ef bf bf = */	"\357\277\277" },


    /* 6 Some other junk */

    { "6.0", "" },
    { "6.1", "\001\002\003\004\005 ABC" },
    { "6.2", /* every non-ASCII Latin1 character */
      "\302\241\302\242\302\243\302\244\302\245\302\246\302\247\302\250"
      "\302\251\302\252\302\253\302\254\302\255\302\256\302\257\302\260"
      "\302\261\302\262\302\263\302\264\302\265\302\266\302\267\302\270"
      "\302\271\302\272\302\273\302\274\302\275\302\276\302\277\303\200"
      "\303\201\303\202\303\203\303\204\303\205\303\206\303\207\303\210"
      "\303\211\303\212\303\213\303\214\303\215\303\216\303\217\303\220"
      "\303\221\303\222\303\223\303\224\303\225\303\226\303\227\303\230"
      "\303\231\303\232\303\233\303\234\303\235\303\236\303\237\303\240"
      "\303\241\303\242\303\243\303\244\303\245\303\246\303\247\303\250"
      "\303\251\303\252\303\253\303\254\303\255\303\256\303\257\303\260"
      "\303\261\303\262\303\263\303\264\303\265\303\266\303\267\303\270"
      "\303\271\303\272\303\273\303\274\303\275\303\276\303\277" },

    { "6.3", /* Christmas tree */
      "\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020"
      "\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\040"
      "\041\042\043\044\045\046\047\050\051\052\053\054\055\056\057\060"
      "\061\062\063\064\065\066\067\070\071\072\073\074\075\076\077\100"
      "\101\102\103\104\105\106\107\110\111\112\113\114\115\116\117\120"
      "\121\122\123\124\125\126\127\130\131\132\133\134\135\136\137\140"
      "\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157\160"
      "\161\162\163\164\165\166\167\170\171\172\173\174\175\176\177\200"
      "\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220"
      "\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240"
      "\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260"
      "\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277\300"
      "\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\320"
      "\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337\340"
      "\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357\360"
      "\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377",

      "\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020"
      "\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037"
      " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\177"
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC URC
      URC URC URC URC URC URC URC URC URC URC URC URC },

    { "7.0", /* RFC 9839 */
        /* "\u0089\uDEAD\uD9BF\uDFFF" */
        "\211\355\272\255\355\246\277\355\277\277",
        URC URC URC URC },

    { "8.0", /* "Java Modified UTF-8 Encoding".
                Yes that's right, Java uses non-standard UTF-8! In FFI or
                serialization it might start with a 2-byte length; but beyond
                that, it encodes \000 as C0 80, and some other bytes weirdly.
                https://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8

                This test harness can't test an embedded null, since it takes
                null-terminated strings as input.
              */
        "\360\240\234\216",
        "\360\240\234\216",		/* This is correct */
     /* "\241\201\355\274\216"		   This is how Java encodes it. */

        "\357\277\275"	  /* ...and it can't be represented as XChar2b. */
    },
  };

  int i;
  int ok = 1;
  for (i = 0; i < sizeof(tests)/sizeof(*tests); i++)
    {
      const char *name = tests[i].name;
      const char *in   = tests[i].in;
      const char *target = (tests[i].target ? tests[i].target : in);
      const char *target2 = (tests[i].target2 ? tests[i].target2 : target);
      char *out = split_and_join (in);
      XChar2b *out16 = utf8_to_XChar2b (in, 0);
      char *out2 = XChar2b_to_utf8 (out16, 0);
      if (strcmp (out, target))
        {
          LOG (stderr, name, target);
          LOG (stderr, "FAIL", out);
          fprintf (stderr, "\n");
          ok = 0;
        }
      if (strcmp (out2, target2))
        {
          LOG (stderr, name, target2);
          LOG (stderr, "FAIL2", out2);
          fprintf (stderr, "\n");
          ok = 0;
        }
      free (out);
      free (out2);
      free (out16);
    }

  /* Check conversion from UTF8 to Latin1 and ASCII. */
  {
    const char *utf8 = ("son \303\256le int\303\251rieure, \303\240 "
                        "c\303\264t\303\251 de l'alc\303\264ve "
                        "ovo\303\257de, o\303\271 les b\303\273ches "
                        "se consument dans l'\303\242tre "
                        "\302\251\302\256\302\261\302\274\302\275\302\276"
                        "\303\206\303\246");
    const char *latin1 = ("son \356le int\351rieure, \340 "
                          "c\364t\351 de l'alc\364ve ovo\357de, "
                          "o\371 les b\373ches se consument dans "
                          "l'\342tre "
                          "\251\256\261\274\275\276\306\346");
    const char *ascii = ("son ile interieure, a cote de l'alcove "
                         "ovoide, ou les buches se consument dans "
                         "l'atre "
                         "(c)(r)+-1/41/23/4AEae");
    char *latin1b = utf8_to_latin1 (utf8, False);
    char *ascii2  = utf8_to_latin1 (utf8, True);
    if (strcmp (latin1, latin1b))
      {
        LOG (stderr, "LATIN1", utf8);
        LOG (stderr, "FAIL3", latin1b);
        fprintf (stderr, "\n");
        ok = 0;
      }
    if (strcmp (ascii, ascii2))
      {
        LOG (stderr, "ASCII", utf8);
        LOG (stderr, "FAIL4", ascii2);
        fprintf (stderr, "\n");
        ok = 0;
      }
    free (latin1b);
    free (ascii2);
  }

  /* Check de-composition of emoji that should all be treated as a unit
     for measurement and display purposes. */
  {
    static const char * const tests[] = { 

      /* 0: "Man" */
      " \360\237\221\250 ",

      /* 1: "Blackula" = "Vampire, dark skin tone" = 1F9DB 1F3FF */
      " \360\237\247\233\360\237\217\277 ",

      /* 2: "Black male teacher" = "Man, dark skin tone, ZWJ, school" =
            1F468 1F3FF 200D 1F3EB
       */
      " \360\237\221\250\360\237\217\277\342\200\215\360\237\217\253 ",

      /* 3: "Female runner" = "Runner, ZWJ, female sign" = 1F3C3 200D 2640 */
      " \360\237\217\203\342\200\215\342\231\200 ",

      /* 4: "Woman astronaut" = "Woman, ZWJ, rocket ship" = 1F3C3 200D 1F680 */
      " \360\237\217\203\342\200\215\360\237\232\200 ",

      /* 5:
         Group of people displayed as a single glyph:
           Woman, dark skin tone, ZWJ,   1F469 1F3FF 200D
           Man, light skin tone, ZWJ,    1F468 1F3FB 200D
           Boy, medium skin tone, ZWJ,   1F466 1F3FD 200D
           Girl, dark skin tone.         1F467 1F3FF
       */
      " \360\237\221\251\360\237\217\277\342\200\215"
       "\360\237\221\250\360\237\217\273\342\200\215"
       "\360\237\221\246\360\237\217\275\342\200\215"
       "\360\237\221\247\360\237\217\277 ",

        /* 6: Symbol, Modifiers */
        " c\302\270 ",	/* 0063 C + 00B8 Cedilla */
        " c\314\247 ",	/* 0063 C + 0327 Combining Cedilla */
    };
    int i;
    for (i = 0; i < sizeof(tests)/sizeof(*tests); i++)
      {
        int L = 0;
        char **out = utf8_split (tests[i], &L);
        char name[100];
        int j;
        sprintf (name, "SPLIT %d: %d glyphs", i, L-2);
        if (L != 3)
          {
            LOG (stderr, name, tests[i]);
            ok = 0;
          }
        for (j = 0; j < L; j++)
          free (out[j]);
        free (out);
      }
  }

  if (ok) fprintf (stderr, "OK\n");
  return (ok == 0);
}

#endif /* SELFTEST */
