/* xscreensaver, Copyright (c) 2014-2016 Jamie Zawinski <jwz@jwz.org>
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
          ((in[0] & 0x3F) << 12) | /*       01111111----+------- */
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


/* Converts a null-terminated UTF8 string to a null-terminated XChar2b array.
   This only handles characters that can be represented in 16 bits, the
   Basic Multilingual Plane. (No hieroglyphics, Elvish, Klingon or Emoji.)
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


/* Split a UTF8 string into an array of strings, one per character.
   The sub-strings will be null terminated and may be multiple bytes.
 */
char **
utf8_split (const char *string, int *length_ret)
{
  const unsigned char *in = (const unsigned char *) string;
  long len = strlen (string);
  const unsigned char *end = in + len;
  char **ret = (char **) malloc ((len+1) * sizeof(*ret));
  int i = 0;
  int zwjp = 0;
  if (!ret) return 0;

  while (in < end)
    {
      unsigned long uc;
      long len2 = utf8_decode (in, len, &uc);
      char tmp[10];
      strncpy (tmp, (char *) in, len2);
      tmp[len2] = 0;
      ret[i++] = strdup (tmp);
      in += len2;

      /* If this is a Combining Diacritical, append it to the previous
         character. E.g., "y\314\206\314\206" is one string, not three.

         If this is ZWJ, Zero Width Joiner, then we append both this character
         and the following character, e.g. "X ZWJ Y" is one string not three.

         #### Hmmm, should this also include every character in the
         "Symbol, Modifier" category, or does ZWJ get used for those?
         https://www.fileformat.info/info/unicode/category/Sk/list.htm

         Is it intended that "Latin small letter C, 0063" + "Cedilla, 00B8"
         should be a single glyph? Or is that what "Combining Cedilla, 0327"
         is for?  I'm confused by the fact that the skin tones (1F3FB-1F3FF)
         do not seem to be in a readily-identifiable block the way the various
         combining diacriticals are.
       */
      if (i > 1 && 
          ((uc >=   0x300 && uc <=   0x36F) || /* Combining Diacritical */
           (uc >=  0x1AB0 && uc <=  0x1AFF) || /* Combining Diacritical Ext. */
           (uc >=  0x1DC0 && uc <=  0x1DFF) || /* Combining Diacritical Supp. */
           (uc >=  0x20D0 && uc <=  0x20FF) || /* Combining Diacritical Sym. */
           (uc >=  0xFE20 && uc <=  0xFE2F) || /* Combining Half Marks */
           (uc >= 0x1F3FB && uc <= 0x1F3FF) || /* Emoji skin tone modifiers */
           zwjp || uc == 0x200D))              /* Zero Width Joiner */
        {
          long L1 = strlen(ret[i-2]);
          long L2 = strlen(ret[i-1]);
          char *s2 = (char *) malloc (L1 + L2 + 1);
          strncpy (s2,      ret[i-2], L1);
          strncpy (s2 + L1, ret[i-1], L2);
          s2[L1 + L2] = 0;
          free (ret[i-2]);
          ret[i-2] = s2;
          i--;
          zwjp = (uc == 0x200D);  /* Swallow the next character as well */
        }
    }
  ret[i] = 0;

  if (length_ret)
    *length_ret = i;

  /* shrink */
  ret = (char **) realloc (ret, (i+1) * sizeof(*ret));

  return ret;
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
utf8_to_latin1 (const char *string, Bool ascii_p)
{
  long in_len = strlen(string);
  const unsigned char *in = (const unsigned char *) string;
  const unsigned char *in_end = in + in_len;
  unsigned char *ret = (unsigned char *) malloc (in_len + 1);
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

        case 0x2000:	/* EN QUAD */
        case 0x2001:	/* EM QUAD */
        case 0x2002:	/* EN SPACE */
        case 0x2003:	/* EM SPACE */
        case 0x2004:	/* THREE-PER-EM SPACE */
        case 0x2005:	/* FOUR-PER-EM SPACE */
        case 0x2006:	/* SIX-PER-EM SPACE */
        case 0x2007:	/* FIGURE SPACE */
        case 0x2008:	/* PUNCTUATION SPACE */
        case 0x2009:	/* THIN SPACE */
        case 0x200A:	/* HAIR SPACE */
          uc = ' ';
	  break;

        case 0x2010:	/* HYPHEN */
        case 0x2011:	/* NON-BREAKING HYPHEN */
        case 0x2012:	/* FIGURE DASH */
        case 0x2013:	/* EN DASH */
        case 0x2014:	/* EM DASH */
        case 0x2015:	/* HORIZONTAL BAR */
          uc = '-';
	  break;

        case 0x2018:	/* LEFT SINGLE QUOTATION MARK */
        case 0x2019:	/* SINGLE LOW-9 QUOTATION MARK */
        case 0x201A:	/* SINGLE LOW-9 QUOTATION MARK */
        case 0x201B:	/* SINGLE HIGH-REVERSED-9 QUOTATION MARK */
          uc = '\'';
	  break;

        case 0x201C:	/* LEFT DOUBLE QUOTATION MARK */
        case 0x201D:	/* RIGHT DOUBLE QUOTATION MARK */
        case 0x201E:	/* DOUBLE LOW-9 QUOTATION MARK */
        case 0x201F:	/* DOUBLE HIGH-REVERSED-9 QUOTATION MARK */
          uc = '"';
	  break;

        case 0x2022: uc = '\267'; break; /* BULLET */
        case 0x2023: uc = '\273'; break; /* TRIANGULAR BULLET */
        case 0x2027: uc = '\267'; break; /* HYPHENATION POINT */
        case 0x202F: uc = ' ';	  break; /* NARROW NO-BREAK SPACE */
        case 0x2038: uc = '^';	  break; /* CARET */
        case 0x2039: uc = '\253'; break; /* SINGLE LEFT ANGLE QUOTATION MARK */
        case 0x203A: uc = '\273'; break; /* SINGLE RIGHT ANGLE QUOTATION MARK*/
        case 0x2041: uc = '^';	  break; /* CARET INSERTION POINT */
        case 0x2042: uc = '*';	  break; /* ASTERISM */
        case 0x2043: uc = '=';	  break; /* HYPHEN BULLET */
        case 0x2044: uc = '/';	  break; /* FRACTION SLASH */
        case 0x204B: uc = '\266'; break; /* REVERSED PILCROW SIGN */
        case 0x204C: uc = '\267'; break; /* BLACK LEFTWARDS BULLET */
        case 0x204D: uc = '\267'; break; /* BLACK RIGHTWARDS BULLET */
        case 0x204E: uc = '*';	  break; /* LOW ASTERISK */
        case 0x204F: uc = ';';	  break; /* REVERSED SEMICOLON */
        default:
          break;
        }

      if (uc > 0xFF)
        /* "Inverted question mark" looks enough like 0xFFFD,
           the "Unicode Replacement Character". */
        uc = (ascii_p ? '#' : '\277');

      if (ascii_p)	/* Map Latin1 to the closest ASCII versions. */
        {
          const unsigned char latin1_to_ascii[96] =
             " !C##Y|S_C#<=-R_##23'uP.,1o>###?"
             "AAAAAAECEEEEIIIIDNOOOOOx0UUUUYpS"
             "aaaaaaeceeeeiiiionooooo/ouuuuypy";
          if (uc >= 0xA0)
            uc = latin1_to_ascii[uc - 0xA0];
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
                        "se consument dans l'\303\242tre");
    const char *latin1 = ("son \356le int\351rieure, \340 "
                          "c\364t\351 de l'alc\364ve ovo\357de, "
                          "o\371 les b\373ches se consument dans "
                          "l'\342tre");
    const char *ascii = ("son ile interieure, a cote de l'alcove "
                         "ovoide, ou les buches se consument dans "
                         "l'atre");
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
