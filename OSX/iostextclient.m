/* xscreensaver, Copyright (c) 2012 Jamie Zawinski <jwz@jwz.org>
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
 * This is necessary because iOS doesn't have Perl installed, so we can't
 * run "xscreensaver-text" to do this.
 */

#include "utils.h"

#ifdef USE_IPHONE /* whole file -- see utils/textclient.c */

#include "textclient.h"
#include "resources.h"

#include <stdio.h>

extern const char *progname;

struct text_data {

  enum { DATE, LITERAL, URL } mode;
  char *literal, *url;

  int columns;
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


static char *
date_string (void)
{
  UIDevice *dd = [UIDevice currentDevice];
  NSString *name = [dd name];			// My iPhone
  NSString *model = [dd model];			// iPad
  // NSString *system = [dd systemName];	// iPhone OS
  NSString *vers = [dd systemVersion];		// 5.0
  NSString *date =
    [NSDateFormatter
      localizedStringFromDate:[NSDate date]
      dateStyle: NSDateFormatterMediumStyle
      timeStyle: NSDateFormatterMediumStyle];
  NSString *nl = @"\n";

  NSString *result = name;
  result = [result stringByAppendingString: nl];
  result = [result stringByAppendingString: model];
  // result = [result stringByAppendingString: nl];
  // result = [result stringByAppendingString: system];
  result = [result stringByAppendingString: @" "];
  result = [result stringByAppendingString: vers];
  result = [result stringByAppendingString: nl];
  result = [result stringByAppendingString: nl];
  result = [result stringByAppendingString: date];
  result = [result stringByAppendingString: nl];
  result = [result stringByAppendingString: nl];
  return strdup ([result cStringUsingEncoding:NSISOLatin1StringEncoding]);
}


static void
strip_html (char *html)
{
  int tag = 0;
  int comment = 0;
  int white = 0;
  int nl = 0;
  int entity = 0;
  char *out = html;
  for (char *in = html; *in; in++) {
    if (comment) {
      if (!strncmp (in, "-->", 3)) {
        comment = 0;
      }
    } else if (tag) {
      if (*in == '>') {
        tag = 0;
        entity = 0;
      }
    } else if (entity) {
      if (*in == ';') {
        entity = 0;
      }
    } else if (*in == '<') {
      tag = 1;
      entity = 0;
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
    } else if (*in == '&') {
      char *ss = 0;
      entity = 1;

      if      (!strncmp (in, "&amp", 4))    ss = "&";
      else if (!strncmp (in, "&lt", 3))     ss = "<";
      else if (!strncmp (in, "&gt", 3))     ss = ">";
      else if (!strncmp (in, "&nbsp", 5))   ss = " ";

      else if (!strncmp (in, "&AElig", 6))  ss = "AE";
      else if (!strncmp (in, "&aelig", 6))  ss = "ae";
      else if (!strncmp (in, "&bdquo", 6))  ss = "\"";
      else if (!strncmp (in, "&brvbar", 7)) ss = "|";
      else if (!strncmp (in, "&bull", 5))   ss = "*";
      else if (!strncmp (in, "&circ", 5))   ss = "^";
      else if (!strncmp (in, "&cong", 5))   ss = "=~";
      else if (!strncmp (in, "&copy", 5))   ss = "(c)";
      else if (!strncmp (in, "&curren", 7)) ss = "$";
      else if (!strncmp (in, "&deg", 4))    ss = ".";
      else if (!strncmp (in, "&divide", 7)) ss = "/";
      else if (!strncmp (in, "&empty", 6))  ss = "0";
      else if (!strncmp (in, "&emsp", 5))   ss = " ";
      else if (!strncmp (in, "&ensp", 5))   ss = " ";
      else if (!strncmp (in, "&equiv", 6))  ss = "==";
      else if (!strncmp (in, "&frac12", 7)) ss = "1/2";
      else if (!strncmp (in, "&frac14", 7)) ss = "1/4";
      else if (!strncmp (in, "&frac34", 7)) ss = "3/4";
      else if (!strncmp (in, "&frasl", 6))  ss = "/";
      else if (!strncmp (in, "&ge", 3))     ss = ">=";
      else if (!strncmp (in, "&hArr", 5))   ss = "<=>";
      else if (!strncmp (in, "&harr", 5))   ss = "<->";
      else if (!strncmp (in, "&hellip", 7)) ss = "...";
      else if (!strncmp (in, "&iquest", 7)) ss = "?";
      else if (!strncmp (in, "&lArr", 5))   ss = "<=";
      else if (!strncmp (in, "&lang", 5))   ss = "<";
      else if (!strncmp (in, "&laquo", 6))  ss = "<<";
      else if (!strncmp (in, "&larr", 5))   ss = "<-";
      else if (!strncmp (in, "&ldquo", 6))  ss = "\"";
      else if (!strncmp (in, "&le", 3))     ss = "<=";
      else if (!strncmp (in, "&lowast", 7)) ss = "*";
      else if (!strncmp (in, "&loz", 4))    ss = "<>";
      else if (!strncmp (in, "&lsaquo", 7)) ss = "<";
      else if (!strncmp (in, "&lsquo", 6))  ss = "`";
      else if (!strncmp (in, "&macr", 5))   ss = "'";
      else if (!strncmp (in, "&mdash", 6))  ss = "--";
      else if (!strncmp (in, "&micro", 6))  ss = "u";
      else if (!strncmp (in, "&middot", 7)) ss = ".";
      else if (!strncmp (in, "&minus", 6))  ss = "-";
      else if (!strncmp (in, "&ndash", 6))  ss = "-";
      else if (!strncmp (in, "&ne", 3))     ss = "!=";
      else if (!strncmp (in, "&not", 4))    ss = "!";
      else if (!strncmp (in, "&OElig", 6))  ss = "OE";
      else if (!strncmp (in, "&oelig", 6))  ss = "oe";
      else if (!strncmp (in, "&ordf", 5))   ss = "_";
      else if (!strncmp (in, "&ordm", 5))   ss = "_";
      else if (!strncmp (in, "&para", 5))   ss = "PP";
      else if (!strncmp (in, "&plusmn", 7)) ss = "+/-";
      else if (!strncmp (in, "&pound", 6))  ss = "#";
      else if (!strncmp (in, "&prime", 6))  ss = "'";
      else if (!strncmp (in, "&quot", 5))   ss = "\"";
      else if (!strncmp (in, "&rArr", 5))   ss = "=>";
      else if (!strncmp (in, "&rang", 5))   ss = ">";
      else if (!strncmp (in, "&raquo", 6))  ss = ">>";
      else if (!strncmp (in, "&rarr", 5))   ss = "->";
      else if (!strncmp (in, "&rdquo", 6))  ss = "\"";
      else if (!strncmp (in, "&reg", 4))    ss = "(r)";
      else if (!strncmp (in, "&rsaquo", 7)) ss = ">";
      else if (!strncmp (in, "&rsquo", 6))  ss = "'";
      else if (!strncmp (in, "&sbquo", 6))  ss = "'";
      else if (!strncmp (in, "&sect", 5))   ss = "SS";
      else if (!strncmp (in, "&shy", 4))    ss = "";
      else if (!strncmp (in, "&sim", 4))    ss = "~";
      else if (!strncmp (in, "&sup1", 5))   ss = "[1]";
      else if (!strncmp (in, "&sup2", 5))   ss = "[2]";
      else if (!strncmp (in, "&sup3", 5))   ss = "[3]";
      else if (!strncmp (in, "&szlig", 6))  ss = "B";
      else if (!strncmp (in, "&thinsp", 7)) ss = " ";
      else if (!strncmp (in, "&thorn", 6))  ss = "|";
      else if (!strncmp (in, "&tilde", 6))  ss = "!";
      else if (!strncmp (in, "&times", 6))  ss = "x";
      else if (!strncmp (in, "&trade", 6))  ss = "[tm]";
      else if (!strncmp (in, "&uml", 4))    ss = ":";
      else if (!strncmp (in, "&yen", 4))    ss = "Y";

      if (ss) {
        strcpy (out, ss);
        out += strlen(ss);
      } else if (!strncmp (in, "&#", 2)) {	// &#65;
        int i = 0;
        for (char *in2 = in+2; *in2 >= '0' && *in2 <= '9'; in2++)
          i = (i * 10) + (*in2 - '0');
        *out = (i > 255 ? '?' : i);
      } else if (!strncmp (in, "&#x", 3)) {	// &#x41;
        int i = 0;
        for (char *in2 = in+3;
             ((*in2 >= '0' && *in2 <= '9') ||
              (*in2 >= 'A' && *in2 <= 'F') ||
              (*in2 >= 'a' && *in2 <= 'f'));
             in2++)
          i = (i * 16) + (*in2 >= 'a' ? *in2 - 'a' + 16 :
                          *in2 >= 'A' ? *in2 - 'A' + 16 :
                          *in2 - '0');
        *out = (i > 255 ? '?' : i);
      } else {
        *out++ = in[1];    // first character of entity, e.g. &eacute;.
      }
    } else if (*in == ' ' || *in == '\t' || *in == '\r' || *in == '\n') {
      if (!white && out != html)
        *out++ = ' ';
      white = 1;
    } else {
      *out++ = *in;
      white = 0;
      nl = 0;
    }
  }
  *out = 0;
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
    strip_html (s3);
    return s3;
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


static void
strip_rss (char *rss)
{
  char *out = rss;
  const char *a = 0, *b = 0, *c = 0, *d = 0, *t = 0;
  int head = 1;
  int done = 0;

  for (char *in = rss; *in; in++) {
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

  // Now decode it a second time.
  strip_html (rss);
}


static void
wrap_text (char *body, int columns)
{
  int col = 0, last_col = 0;
  char *last_space = 0;
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
  wrap_text (body, columns);
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


static char *
url_string (const char *url)
{
  NSURL *nsurl =
    [NSURL URLWithString:
             [NSString stringWithCString: url
                       encoding:NSISOLatin1StringEncoding]];
  NSString *body =
    [NSString stringWithContentsOfURL: nsurl
              encoding: NSISOLatin1StringEncoding
              error: nil];
  if (! body)
    return 0;

  enum { RSS, HTML, TEXT } type;

  // Only search the first 1/2 K of the document while determining type.

  unsigned long L = [body length];
  if (L > 512) L = 512;
  NSString *head = [[[body substringToIndex: L]
                      stringByTrimmingCharactersInSet:
                        [NSCharacterSet whitespaceAndNewlineCharacterSet]]
                     lowercaseString];
  if ([head hasPrefix:@"<?xml"] ||
      [head hasPrefix:@"<!doctype rss"])
    type = RSS;
  else if ([head hasPrefix:@"<!doctype html"] ||
           [head hasPrefix:@"<html"] ||
           [head hasPrefix:@"<head"])
    type = HTML;
  else if ([head rangeOfString:@"<base"].length ||
           [head rangeOfString:@"<body"].length ||
           [head rangeOfString:@"<script"].length ||
           [head rangeOfString:@"<style"].length ||
           [head rangeOfString:@"<a href"].length)
    type = HTML;
  else if ([head rangeOfString:@"<channel"].length ||
           [head rangeOfString:@"<generator"].length ||
           [head rangeOfString:@"<description"].length ||
           [head rangeOfString:@"<content"].length ||
           [head rangeOfString:@"<feed"].length ||
           [head rangeOfString:@"<entry"].length)
    type = RSS;
  else
    type = TEXT;

  char *body2 = strdup ([body cStringUsingEncoding:NSISOLatin1StringEncoding]);

  switch (type) {
  case HTML: strip_html (body2); break;
  case RSS:  strip_rss (body2);  break;
  case TEXT: break;
  default: abort(); break;
  }

  return body2;
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
      d->buf = date_string();
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
      d->buf = url_string (d->url);
      break;
    default:
      abort();
    }
    if (d->columns > 10)
      wrap_text (d->buf, d->columns);
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
                    int char_w, int char_h)
{
  d->columns = char_w;
  rewrap_text (d->buf, d->columns);
}

#endif /* USE_IPHONE -- whole file */
