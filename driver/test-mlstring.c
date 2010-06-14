/* 
 * (c) 2007, Quest Software, Inc. All rights reserved.
 *
 * This file is part of XScreenSaver,
 * Copyright (c) 1993-2004 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "mlstring.c"  /* hokey, but whatever */

#define WRAP_WIDTH_PX 100

#undef Bool
#undef True
#undef False
typedef int Bool;
#define True 1
#define False 0

#define SKIPPED -1
#define SUCCESS 0
#define FAILURE 1

#define FAIL(msg, ...)                 \
  do {                                 \
    ++failcount;                       \
    fprintf(stderr, "[FAIL] ");        \
    fprintf(stderr, msg, __VA_ARGS__); \
    putc('\n', stderr);                \
    return FAILURE;                    \
  } while (0)

#define SUCCEED(testname)                          \
  do {                                             \
    fprintf(stderr, "[SUCCESS] %s\n", (testname)); \
  } while (0)

#define SKIP(testname)                             \
  do {                                             \
    fprintf(stderr, "[SKIPPED] %s\n", (testname)); \
  } while (0)

extern mlstring* mlstring_allocate(const char *msg);
extern void mlstring_wrap(mlstring *mstr, XFontStruct *font, Dimension width);

static int failcount = 0;

static char *mlstring_to_cstr(const mlstring *mlstr) {
  char *cstr;
  size_t cstrlen = 0, alloclen = 1024;
  const struct mlstr_line *line;

  cstr = malloc(alloclen);
  if (!cstr)
    return NULL;
  cstr[0] = '\0';

  for (line = mlstr->lines; line; line = line->next_line) {
    /* Extend the buffer if necessary. */
    if (cstrlen + strlen(line->line) + 1 > alloclen) {
      cstr = realloc(cstr, alloclen *= 2);
      if (!cstr)
	return NULL;
    }

    /* If this is not the first line */
    if (line != mlstr->lines) {
      /* Append a newline character */
      cstr[cstrlen] = '\n';
      ++cstrlen;
      cstr[cstrlen] = '\0';
    }

    strcat(cstr, line->line);
    cstrlen += strlen(line->line);
  }
  return cstr;
}

/* Pass -1 for expect_min or expect_exact to not check that value.
 * expect_empty_p means an empty line is expected at some point in the string.
 * Also ensures that the string was not too wide after wrapping. */
static int mlstring_expect_lines(const mlstring *mlstr, int expect_min, int expect_exact, Bool expect_empty_p)
{
  int count;
  Bool got_empty_line = False;
  const struct mlstr_line *line = mlstr->lines;

  for (count = 0; line; line = line->next_line) {
    if (line->line[0] == '\0') {
      if (!expect_empty_p)
	FAIL("Not expecting empty lines, but got one on line %d of [%s]", count + 1, mlstring_to_cstr(mlstr));
      got_empty_line = True;
    }
    ++count;
  }

  if (expect_empty_p && !got_empty_line)
    FAIL("Expecting an empty line, but none found in [%s]", mlstring_to_cstr(mlstr));

  if (expect_exact != -1 && expect_exact != count)
    FAIL("Expected %d lines, got %d", expect_exact, count);
  
  if (expect_min != -1 && count < expect_min)
    FAIL("Expected at least %d lines, got %d", expect_min, count);

  return SUCCESS;
}

static int mlstring_expect(const char *msg, int expect_lines, const mlstring *mlstr, Bool expect_empty_p)
{
  char *str, *str_top;
  const struct mlstr_line *cur;
  int linecount = 0;

  /* Duplicate msg so we can chop it up */
  str_top = strdup(msg);
  if (!str_top)
    return SKIPPED;

  /* Replace all newlines with NUL */
  str = str_top;
  while ((str = strchr(str, '\n')))
    *str++ = '\0';

  /* str is now used to point to the expected string */
  str = str_top;

  for (cur = mlstr->lines; cur; cur = cur->next_line)
    {
      ++linecount;
      if (strcmp(cur->line, str))
	FAIL("lines didn't match; expected [%s], got [%s]", str, cur->line);

      str += strlen(str) + 1; /* Point to the next expected string */
    }

  free(str_top);

  return mlstring_expect_lines(mlstr, -1, expect_lines, expect_empty_p);
}

/* Ensures that the width has been set properly after wrapping */
static int check_width(const char *msg, const mlstring *mlstr) {
  if (mlstr->overall_width == 0)
    FAIL("Overall width was zero for string [%s]", msg);

  if (mlstr->overall_width > WRAP_WIDTH_PX)
    FAIL("Overall width was %hu but the maximum wrap width was %d", mlstr->overall_width, WRAP_WIDTH_PX);

  return SUCCESS;
}

/* FAIL() actually returns the wrong return codes in main, but it
 * prints a message which is what we want. */

#define TRY_NEW(str, numl, expect_empty)                      \
  do {                                                        \
    mlstr = mlstring_allocate((str));                         \
    if (!mlstr)                                               \
      FAIL("%s", #str);                                       \
    if (SUCCESS == mlstring_expect((str), (numl), mlstr, (expect_empty))) \
      SUCCEED(#str);                                          \
    free(mlstr);                                              \
  } while (0)

/* Expects an XFontStruct* font, and tries to wrap to 100px */
#define TRY_WRAP(str, minl, expect_empty)                        \
  do {                                                           \
    mltest = mlstring_allocate((str));                           \
    if (!mltest)                                                 \
      SKIP(#str);                                                \
    else {                                                       \
      mlstring_wrap(mltest, font, WRAP_WIDTH_PX);                \
      check_width((str), mltest);                                \
      if (SUCCESS == mlstring_expect_lines(mltest, (minl), -1, (expect_empty)))  \
	SUCCEED(#str);                                           \
      free(mltest);                                              \
      mltest = NULL;                                             \
    }                                                            \
  } while (0)


/* Ideally this function would use stub functions rather than real Xlib.
 * Then it would be possible to test for exact line counts, which would be
 * more reliable.
 * It also doesn't handle Xlib errors.
 *
 * Don't print anything based on the return value of this function, it only
 * returns a value so that I can use the FAIL() macro without warning.
 *
 * Anyone who understands this function wins a cookie ;)
 */
static int test_wrapping(void)
{
  Display *dpy = NULL;
  XFontStruct *font = NULL;
  mlstring *mltest = NULL;
  int ok = 0;
  int chars_per_line, chars_first_word, i;
  
  const char *test_short = "a";
  const char *test_hardwrap = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  const char *test_withnewlines = "a\nb";
  char *test_softwrap = NULL;

  dpy = XOpenDisplay(NULL);
  if (!dpy)
    goto end;
  
  font = XLoadQueryFont(dpy, "fixed");
  if (!font)
    goto end;

  TRY_WRAP(test_short, 1, False);
  TRY_WRAP(test_hardwrap, 2, False);
  TRY_WRAP(test_withnewlines, 2, False);

  /* See if wrapping splits on word boundaries like it should */
  chars_per_line = WRAP_WIDTH_PX / font->max_bounds.width;
  if (chars_per_line < 3)
    goto end;

  /* Allocate for 2 lines + \0 */
  test_softwrap = malloc(chars_per_line * 2 + 1);
  if (!test_softwrap)
    goto end;

  /* 2 = strlen(' a'); that is, the minimum space required to start a new word
   * on the same line. */
  chars_first_word = chars_per_line - 2;

  for (i = 0; i < chars_first_word; ++i) {
    test_softwrap[i] = 'a'; /* first word */
    test_softwrap[i + chars_per_line] = 'b'; /* second word */
  }
  /* space between first & second words */
  test_softwrap[chars_first_word] = ' ';
  /* first char of second word (last char of first line) */
  test_softwrap[chars_first_word + 1] = 'b';
  /* after second word */
  test_softwrap[chars_per_line * 2] = '\0';

  mltest = mlstring_allocate(test_softwrap);
  mlstring_wrap(mltest, font, WRAP_WIDTH_PX);

  /* reusing 'i' for a moment here to make freeing mltest easier */
  i = strlen(mltest->lines->line);
  free(mltest);

  if (i != chars_first_word)
    FAIL("Soft wrap failed, expected the first line to be %d chars, but it was %d.", chars_first_word, i);
  SUCCEED("Soft wrap");

  ok = 1;

end:
  if (test_softwrap)
    free(test_softwrap);

  if (font)
    XFreeFont(dpy, font);

  if (dpy)
    XCloseDisplay(dpy);

  if (!ok)
    SKIP("wrapping");

  return ok ? SUCCESS : SKIPPED; /* Unused, actually */
}


int main(int argc, char *argv[])
{
  const char *oneline = "1Foo";
  const char *twolines = "2Foo\nBar";
  const char *threelines = "3Foo\nBar\nWhippet";
  const char *trailnewline = "4Foo\n";
  const char *trailnewlines = "5Foo\n\n";
  const char *embeddednewlines = "6Foo\n\nBar";
  mlstring *mlstr;

  TRY_NEW(oneline, 1, False);
  TRY_NEW(twolines, 2, False);
  TRY_NEW(threelines, 3, False);
  TRY_NEW(trailnewline, 2, True);
  TRY_NEW(trailnewlines, 3, True);
  TRY_NEW(embeddednewlines, 3, True);

  (void) test_wrapping();

  fprintf(stdout, "%d test failures.\n", failcount);

  return !!failcount;
}

/* vim:ts=8:sw=2:noet
 */
