/* xscreensaver, Copyright (c) 2012-2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Loading URLs and returning the underlying text.
 *
 * This is necessary because iOS and Android don't have Perl installed,
 * so we can't just run "xscreensaver-text" at the end of a pipe to do this.
 */

#include "utils.h"

#if defined(HAVE_IPHONE) || defined(HAVE_ANDROID)  /* whole file */

#include "textclient.h"
#include "resources.h"
#include "utf8wc.h"

#include <stdio.h>

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


extern const char *progname;

struct text_data {

  enum { DATE, LITERAL, URL } mode;
  char *literal, *url;

  Display *dpy;
  int columns;
  int max_lines;
  char *buf;
  int buf_size;
  char *fp;

};


text_data *
textclient_open (Display *dpy)
{
  text_data *d = (text_data *) calloc (1, sizeof (*d));

# ifdef DEBUG
  fprintf (stderr, "%s: textclient: init\n", progname);
# endif

  char *s = get_string_resource (dpy, "textMode", "TextMode");
  if (!s || !*s || !strcasecmp (s, "date") || !strcmp (s, "0"))
    d->mode = DATE;
  else if (!strcasecmp (s, "literal") || !strcmp (s, "1"))
    d->mode = LITERAL;
  else if (!strcasecmp (s, "url") || !strcmp (s, "3"))
    d->mode = URL;
  else
    d->mode = DATE;

  d->dpy = dpy;
  d->literal = get_string_resource (dpy, "textLiteral", "TextLiteral");
  d->url = get_string_resource (dpy, "textURL", "TextURL");

  return d;
}


void
textclient_close (text_data *d)
{
# ifdef DEBUG
  fprintf (stderr, "%s: textclient: free\n", progname);
# endif

  if (d->buf) free (d->buf);
  if (d->literal) free (d->literal);
  if (d->url) free (d->url);
  free (d);
}


/* Returns a copy of the string with some basic HTML entities decoded.
 */
static char *
decode_entities (const char *html)
{
  char *ret = (char *) malloc ((strlen(html) * 4) + 1);  // room for UTF8
  const char *in = html;
  char *out = ret;
  *out = 0;

  const struct { const char *c; const char *e; } entities[] = {

    { "amp", "&" },
    { "lt",  "<" },
    { "gt",  ">" },

    // Convert Latin1 to UTF8
    { "nbsp", " " },			// � 160
    { "iexcl", "\302\241" },		// � 161
    { "cent", "\302\242" },		// � 162
    { "pound", "\302\243" },		// � 163
    { "curren", "\302\244" },		// � 164
    { "yen", "\302\245" },		// � 165
    { "brvbar", "\302\246" },		// � 166
    { "sect", "\302\247" },		// � 167
    { "uml", "\302\250" },		// � 168
    { "copy", "\302\251" },		// � 169
    { "ordf", "\302\252" },		// � 170
    { "laquo", "\302\253" },		// � 171
    { "not", "\302\254" },		// � 172
    { "shy", "\302\255" },		// � 173
    { "reg", "\302\256" },		// � 174
    { "macr", "\302\257" },		// � 175
    { "deg", "\302\260" },		// � 176
    { "plusmn", "\302\261" },		// � 177
    { "sup2", "\302\262" },		// � 178
    { "sup3", "\302\263" },		// � 179
    { "acute", "\302\264" },		// � 180
    { "micro", "\302\265" },		// � 181
    { "para", "\302\266" },		// � 182
    { "middot", "\302\267" },		// � 183
    { "cedil", "\302\270" },		// � 184
    { "sup1", "\302\271" },		// � 185
    { "ordm", "\302\272" },		// � 186
    { "raquo", "\302\273" },		// � 187
    { "frac14", "\302\274" },		// � 188
    { "frac12", "\302\275" },		// � 189
    { "frac34", "\302\276" },		// � 190
    { "iquest", "\302\277" },		// � 191
    { "Agrave", "\303\200" },		// � 192
    { "Aacute", "\303\201" },		// � 193
    { "Acirc", "\303\202" },		// � 194
    { "Atilde", "\303\203" },		// � 195
    { "Auml", "\303\204" },		// � 196
    { "Aring", "\303\205" },		// � 197
    { "AElig", "\303\206" },		// � 198
    { "Ccedil", "\303\207" },		// � 199
    { "Egrave", "\303\210" },		// � 200
    { "Eacute", "\303\211" },		// � 201
    { "Ecirc", "\303\212" },		// � 202
    { "Euml", "\303\213" },		// � 203
    { "Igrave", "\303\214" },		// � 204
    { "Iacute", "\303\215" },		// � 205
    { "Icirc", "\303\216" },		// � 206
    { "Iuml", "\303\217" },		// � 207
    { "ETH", "\303\220" },		// � 208
    { "Ntilde", "\303\221" },		// � 209
    { "Ograve", "\303\222" },		// � 210
    { "Oacute", "\303\223" },		// � 211
    { "Ocirc", "\303\224" },		// � 212
    { "Otilde", "\303\225" },		// � 213
    { "Ouml", "\303\226" },		// � 214
    { "times", "\303\227" },		// � 215
    { "Oslash", "\303\230" },		// � 216
    { "Ugrave", "\303\231" },		// � 217
    { "Uacute", "\303\232" },		// � 218
    { "Ucirc", "\303\233" },		// � 219
    { "Uuml", "\303\234" },		// � 220
    { "Yacute", "\303\235" },		// � 221
    { "THORN", "\303\236" },		// � 222
    { "szlig", "\303\237" },		// � 223
    { "agrave", "\303\240" },		// � 224
    { "aacute", "\303\241" },		// � 225
    { "acirc", "\303\242" },		// � 226
    { "atilde", "\303\243" },		// � 227
    { "auml", "\303\244" },		// � 228
    { "aring", "\303\245" },		// � 229
    { "aelig", "\303\246" },		// � 230
    { "ccedil", "\303\247" },		// � 231
    { "egrave", "\303\250" },		// � 232
    { "eacute", "\303\251" },		// � 233
    { "ecirc", "\303\252" },		// � 234
    { "euml", "\303\253" },		// � 235
    { "igrave", "\303\254" },		// � 236
    { "iacute", "\303\255" },		// � 237
    { "icirc", "\303\256" },		// � 238
    { "iuml", "\303\257" },		// � 239
    { "eth", "\303\260" },		// � 240
    { "ntilde", "\303\261" },		// � 241
    { "ograve", "\303\262" },		// � 242
    { "oacute", "\303\263" },		// � 243
    { "ocirc", "\303\264" },		// � 244
    { "otilde", "\303\265" },		// � 245
    { "ouml", "\303\266" },		// � 246
    { "divide", "\303\267" },		// � 247
    { "oslash", "\303\270" },		// � 248
    { "ugrave", "\303\271" },		// � 249
    { "uacute", "\303\272" },		// � 250
    { "ucirc", "\303\273" },		// � 251
    { "uuml", "\303\274" },		// � 252
    { "yacute", "\303\275" },		// � 253
    { "thorn", "\303\276" },		// � 254
    { "yuml", "\303\277" },		// � 255

      // And some random others
    { "bdquo", "\342\200\236" },	// ~
    { "bull", "\342\200\242" },		// ~
    { "circ", "\313\206" },		// ~
    { "cong", "\342\211\205" },		// ~
    { "empty", "\342\210\205" },	// ~
    { "emsp", "\342\200\203" },		// ~
    { "ensp", "\342\200\202" },		// ~
    { "equiv", "\342\211\241" },	// ~
    { "frasl", "\342\201\204" },	// ~
    { "ge", "\342\211\245" },		// ~
    { "hArr", "\342\207\224" },		// ~
    { "harr", "\342\206\224" },		// ~
    { "hellip", "\342\200\246" },	// ~
    { "lArr", "\342\207\220" },		// ~
    { "lang", "\342\237\250" },		// ~
    { "larr", "\342\206\220" },		// ~
    { "ldquo", "\342\200\234" },	// ~
    { "le", "\342\211\244" },		// ~
    { "lowast", "\342\210\227" },	// ~
    { "loz", "\342\227\212" },		// ~
    { "lsaquo", "\342\200\271" },	// ~
    { "lsquo", "\342\200\230" },	// ~
    { "mdash", "\342\200\224" },	// ~
    { "minus", "\342\210\222" },	// ~
    { "ndash", "\342\200\223" },	// ~
    { "ne", "\342\211\240" },		// ~
    { "OElig", "\305\222" },		// ~
    { "oelig", "\305\223" },		// ~
    { "prime", "\342\200\262" },	// ~
    { "quot", "\342\200\235" },		// ~
    { "rArr", "\342\207\222" },		// ~
    { "rang", "\342\237\251" },		// ~
    { "rarr", "\342\206\222" },		// ~
    { "rdquo", "\342\200\235" },	// ~
    { "rsaquo", "\342\200\272" },	// ~
    { "rsquo", "\342\200\231" },	// ~
    { "sbquo", "\342\200\232" },	// ~
    { "sim", "\342\210\274" },		// ~
    { "thinsp", "\342\200\211" },	// ~
    { "tilde", "\313\234" },		// ~
    { "trade", "\342\204\242" },	// ~
  };

  while (*in) {
    if (*in == '&') {
      int done = 0;
      if (in[1] == '#' && in[2] == 'x') {			// &#x41;
        unsigned long i = 0;
        in += 2;
        while ((*in >= '0' && *in <= '9') ||
               (*in >= 'A' && *in <= 'F') ||
               (*in >= 'a' && *in <= 'f')) {
          i = (i * 16) + (*in >= 'a' ? *in - 'a' + 16 :
                          *in >= 'A' ? *in - 'A' + 16 :
                          *in - '0');
          in++;
        }
        *out += utf8_encode (i, out, strlen(out));
        done = 1;
      } else if (in[1] == '#') {				// &#65;
        unsigned long i = 0;
        in++;
        while (*in >= '0' && *in <= '9') {
          i = (i * 10) + (*in - '0');
          in++;
        }
        *out += utf8_encode (i, out, strlen(out));
        done = 1;
      } else {
        int i;
        for (i = 0; !done && i < countof(entities); i++) {
          if (!strncmp (in+1, entities[i].c, strlen(entities[i].c))) {
            strcpy (out, entities[i].e);
            in  += strlen(entities[i].c) + 1;
            out += strlen(entities[i].e);
            done = 1;
          }
        }
      }

      if (done) {
        if (*in == ';')
          in++;
      } else {
        *out++ = *in++;
      }
    } else {
      *out++ = *in++;
    }
  }
  *out = 0;

  /* Shrink */
  ret = realloc (ret, out - ret + 1);

  return ret;
}


/* Returns a copy of the HTML string that has been converted to plain text,
   in UTF8 encoding.  HTML tags are stripped, <BR> and <P> are converted
   to newlines, and some basic HTML entities are decoded.
 */
static char *
textclient_strip_html (const char *html)
{
  int tag = 0;
  int comment = 0;
  int white = 0;
  int nl = 0;
  int L = strlen(html);
  char *ret = (char *) malloc ((L * 4) + 1);  // room for UTF8
  char *out = ret;
  *out = 0;

  for (const char *in = html; *in; in++) {
    if (in >= html + L) abort();
    if (comment) {
      if (!strncmp (in, "-->", 3)) {
        comment = 0;
      }
    } else if (tag) {
      if (*in == '>') {
        tag = 0;
      }
    } else if (*in == '<') {
      tag = 1;
      if (!strncmp (in, "<!--", 4)) {
        comment = 1;
        tag = 0;
      } else if (!strncasecmp (in, "<BR", 3)) {
        *out++ = '\n';
        white = 1;
        nl++;
      } else if (!strncasecmp (in, "<P", 2)) {
        if (nl < 2) { *out++ = '\n'; nl++; }
        if (nl < 2) { *out++ = '\n'; nl++; }
        white = 1;
      }
    } else if (*in == ' ' || *in == '\t' || *in == '\r' || *in == '\n') {
      if (!white && out != ret)
        *out++ = ' ';
      white = 1;
    } else {
      *out++ = *in;
      white = 0;
      nl = 0;
    }
  }
  *out = 0;

  {
    char *ret2 = decode_entities (ret);
    free (ret);
    ret = ret2;
  }

  return ret;
}


static char *
copy_rss_field (const char *s)
{
  if (!s) return 0;
  while (*s && *s != '>')			// Skip forward to >
    s++;
  if (! *s) return 0;
  s++;

  if (!strncmp (s, "<![CDATA[", 9)) {		// CDATA quoting
    s += 9;
    char *e = strstr (s, "]]");
    if (e) *e = 0;
    unsigned long L = strlen (s);
    char *s2 = (char *) malloc (L+1);
    memcpy (s2, s, L+1);
    return s2;

  } else {					// Entity-encoded.
    const char *s2;
    for (s2 = s; *s2 && *s2 != '<'; s2++)	// Terminate at <
      ;
    char *s3 = (char *) malloc (s2 - s + 1);
    if (! s3) return 0;
    memcpy (s3, s, s2-s);
    s3[s2-s] = 0;
    char *s4 = textclient_strip_html (s3);
    free (s3);
    return s4;
  }
}


static char *
pick_rss_field (const char *a, const char *b, const char *c, const char *d)
{
  // Pick the longest of the fields.
  char *a2 = copy_rss_field (a);
  char *b2 = copy_rss_field (b);
  char *c2 = copy_rss_field (c);
  char *d2 = copy_rss_field (d);
  unsigned long al = a2 ? strlen(a2) : 0;
  unsigned long bl = b2 ? strlen(b2) : 0;
  unsigned long cl = c2 ? strlen(c2) : 0;
  unsigned long dl = d2 ? strlen(d2) : 0;
  char *ret = 0;

  if      (al > bl && al > cl && al > dl) ret = a2;
  else if (bl > al && bl > cl && bl > dl) ret = b2;
  else if (cl > al && cl > bl && cl > dl) ret = c2;
  else ret = d2;
  if (a2 && a2 != ret) free (a2);
  if (b2 && b2 != ret) free (b2);
  if (c2 && c2 != ret) free (c2);
  if (d2 && d2 != ret) free (d2);
  return ret;
}


/* Strip some Wikipedia formatting from the string to make it more readable.
 */
static void
strip_wiki (char *text)
{
  char *in = text;
  char *out = text;
  while (*in) 
    {
      if (!strncmp (in, "<!--", 4))		/* <!-- ... --> */
        {
          char *e = strstr (in+4, "-->");
          if (e) in = e + 3;
        }
      else if (!strncmp (in, "/*", 2))		/* ... */
        {
          char *e = strstr (in+2, "*/");
          if (e) in = e + 2;
          else *out++ = *in++;
        }
      else if (!strncmp (in, "{{Infobox", 9))	/* {{Infobox ... \n}}\n */
        {
          char *e = strstr (in+2, "\n}}");
          if (e) in = e + 3;
          else *out++ = *in++;
        }
      else if (!strncmp (in, "{{", 2))		/* {{ ...table... }} */
        {
          char *e = strstr (in+2, "}}");
          if (e) in = e + 2;
          else *out++ = *in++;
        }
      else if (!strncmp (in, "{|", 2))		/* {| ...table... |} */
        {
          char *e = strstr (in+2, "|}");
          if (e) in = e + 2;
          else *out++ = *in++;
        }
      else if (!strncmp (in, "|-", 2))		/* |- ...table cell... | */
        {
          char *e = strstr (in+2, "|");
          if (e) in = e + 1;
          else *out++ = *in++;
        }
      else if (!strncmp (in, "<ref", 4))	/* <ref>...</ref> -> "*" */
        {
          char *e1 = strstr (in+4, "/>");
          char *e2 = strstr (in+4, "</ref>");
          if (e1 && e1 < e2) in = e1 + 2;
          else if (e2) in = e2 + 6;
          else *out++ = *in++;

          *out++ = '*';
        }
      else if (!strncmp (in, "<", 1))		/* <...> */
        {
          char *e = strstr (in+1, ">");
          if (e) in = e + 1;
          else *out++ = *in++;
        }
      else if (!strncmp (in, "[[", 2))		/* [[ ... ]] */
        {
          char *e1 = strstr (in+2, "|");
          char *e2 = strstr (in+2, "]]");
          if (e1 && e2 && e1 < e2)		/* [[link|anchor]] */
            {
              long L = e2 - e1 - 1;
              memmove (out, e1+1, L);
              out += L;
              in = e2+2;
            }
          else if (e2)				/* [[link]] */
            {
              long L = e2 - in - 2;
              memmove (out, in+2, L);
              out += L;
              in = e2+2;
            }
          else
            *out++ = *in++;
        }
      else if (!strncmp (in, "[", 1))		/* [ ... ] */
        {
          char *e1 = strstr (in+2, " ");
          char *e2 = strstr (in+2, "]");
          if (e1 && e2 && e1 < e2)		/* [url anchor] */
            {
              long L = e2 - e1 - 1;
              memmove (out, e1+1, L);
              out += L;
              in = e2+2;
            }
          else
            *out++ = *in++;
        }
      else if (!strncmp (in, "''''", 4))	/* omit '''' */
        in += 4;
      else if (!strncmp (in, "'''", 3)) 	/* omit ''' */
        in += 3;
      else if (!strncmp (in, "''", 2) ||	/* '' or `` or "" -> " */
               !strncmp (in, "``", 2) ||
               !strncmp (in, "\"\"", 2))
        {
          *out++ = '"';
          in += 2;
        }
      else
        {
          *out++ = *in++;
        }
    }
  *out = 0;

  /* Collapse newlines */
  in = text;
  out = text;
  while (*in) 
    {
      while (!strncmp(in, "\n\n\n", 3))
        in++;
      *out++ = *in++;
    }
  *out = 0;
}


/* Returns a copy of the RSS document that has been converted to plain text,
   in UTF8 encoding.  Rougly, it uses the contents of the <description> field
   of each <item>, and decodes HTML within it.
 */
static char *
textclient_strip_rss (const char *rss)
{
  int L = strlen(rss);
  char *ret = malloc (L * 4 + 1);  // room for UTF8
  char *out = ret;
  const char *a = 0, *b = 0, *c = 0, *d = 0, *t = 0;
  int head = 1;
  int done = 0;
  int wiki_p = !!strcasestr (rss, "<generator>MediaWiki");

  *out = 0;
  for (const char *in = rss; *in; in++) {
    if (in >= rss + L) abort();
    if (*in == '<') {
      if (!strncasecmp (in, "<item", 5) ||	// New item, dump.
          !strncasecmp (in, "<entry", 6)) {
      DONE:
        head = 0;
        char *title = copy_rss_field (t);
        char *body  = pick_rss_field (a, b, c, d);

        a = b = c = d = t = 0;

        if (title && body && !strcmp (title, body)) {
          free (title);
          title = 0;
        }

        if (title) {
          strcpy (out, title);
          free (title);
          out += strlen (out);
          strcpy (out, "\n\n");
          out += strlen (out);
        }

        if (body) {
          strcpy (out, body);
          free (body);
          out += strlen (out);
          strcpy (out, "<P>");
          out += strlen (out);
        }

        if (done) break;

      } else if (head) {   // still before first <item>
        ;
      } else if (!strncasecmp (in, "<title", 6)) {
        t = in+6;
      } else if (!strncasecmp (in, "<summary", 8)) {
        d = in+8;
      } else if (!strncasecmp (in, "<description", 12)) {
        a = in+12;
      } else if (!strncasecmp (in, "<content:encoded", 16)) {
        c = in+16;
      } else if (!strncasecmp (in, "<content", 8)) {
        b = in+8;
      }
    }
  }

  if (! done) {		// Finish off the final item.
    done = 1;
    goto DONE;
  }

  char *ret2 = textclient_strip_html (ret);
  free (ret);
  ret = ret2;

  if (wiki_p) {
    strip_wiki (ret);
    ret2 = decode_entities (ret);
    free (ret);
    ret = ret2;
  }

  return ret;
}


static void
wrap_text (char *body, int columns, int max_lines)
{
  int col = 0, last_col = 0;
  char *last_space = 0;
  int lines = 0;
  if (! body) return;
  for (char *p = body; *p; p++) {
    if (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t') {
      if (col > columns && last_space) {
        *last_space = '\n';
        col = col - last_col;
      }
      last_space = p;
      last_col = col;
    }
    if (*p == '\r' || *p == '\n') {
      col = 0;
      last_col = 0;
      last_space = 0;
      lines++;
      if (max_lines && lines >= max_lines)
        {
          *p = 0;
          break;
        }
    } else {
      col++;
    }
  }
}


static void
rewrap_text (char *body, int columns)
{
  if (! body) return;
  for (char *p = body; *p; p++) {
    if (*p == '\n') {
      if (p[1] == '\n')
        p++;
      else
        *p = ' ';
    }
  }
  wrap_text (body, columns, 0);
}



static void
strip_backslashes (char *s)
{
  char *out = s;
  for (char *in = s; *in; in++) {
    if (*in == '\\') {
      in++;
      if      (*in == 'n') *out++ = '\n';
      else if (*in == 'r') *out++ = '\r';
      else if (*in == 't') *out++ = '\t';
      else *out++ = *in;
    } else {
      *out++ = *in;
    }
  }
  *out = 0;
}


/* Load the raw body of a URL, and convert it to plain text.
 */
static char *
mobile_url_text (Display *dpy, const char *url)
{
  char *body = textclient_mobile_url_string (dpy, url);
  enum { RSS, HTML, TEXT } type;
  if (!body)
    return NULL;

  if (!strncasecmp (body, "<?xml", 5) ||
      !strncasecmp (body, "<!doctype rss", 13))
    type = RSS;
  else if (!strncasecmp (body, "<!doctype html", 14) ||
           !strncasecmp (body, "<html", 5) ||
           !strncasecmp (body, "<head", 5))
    type = HTML;
  else if (strcasestr (body, "<base") ||
           strcasestr (body, "<body") ||
           strcasestr (body, "<script") ||
           strcasestr (body, "<style") ||
           strcasestr (body, "<a href"))
    type = HTML;
  else if (strcasestr (body, "<channel") ||
           strcasestr (body, "<generator") ||
           strcasestr (body, "<description") ||
           strcasestr (body, "<content") ||
           strcasestr (body, "<feed") ||
           strcasestr (body, "<entry"))
    type = RSS;
  else
    type = TEXT;

  char *body2 = 0;

  switch (type) {
  case HTML: body2 = textclient_strip_html (body); break;
  case RSS:  body2 = textclient_strip_rss (body);  break;
  case TEXT: break;
  default: abort(); break;
  }

  if (body2) {
    free (body);
    return body2;
  } else {
    return body;
  }
}


int
textclient_getc (text_data *d)
{
  if (!d->fp || !*d->fp) {
    if (d->buf) {
      free (d->buf);
      d->buf = 0;
      d->fp = 0;
    }
    switch (d->mode) {
    case DATE: DATE:
      d->buf = textclient_mobile_date_string();
      break;
    case LITERAL:
      if (!d->literal || !*d->literal)
        goto DATE;
      d->buf = (char *) malloc (strlen (d->literal) + 3);
      strcpy (d->buf, d->literal);
      strcat (d->buf, "\n");
      strip_backslashes (d->buf);
      d->fp = d->buf;
      break;
    case URL:
      if (!d->url || !*d->url)
        goto DATE;
      d->buf = mobile_url_text (d->dpy, d->url);
      break;
    default:
      abort();
    }
    if (d->columns > 10)
      wrap_text (d->buf, d->columns, d->max_lines);
    d->fp = d->buf;
  }

  if (!d->fp || !*d->fp)
    return -1;

  unsigned char c = (unsigned char) *d->fp++;
  return (int) c;
}


Bool
textclient_putc (text_data *d, XKeyEvent *k)
{
  return False;
}


void
textclient_reshape (text_data *d,
                    int pix_w, int pix_h,
                    int char_w, int char_h,
                    int max_lines)
{
  d->columns = char_w;
  d->max_lines = max_lines;
  rewrap_text (d->buf, d->columns);
}


#endif /* whole file */
