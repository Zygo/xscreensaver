/* xscreensaver, Copyright © 1998-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This is a kludgy test harness for debugging the password dialog box.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include "blurb.h"
#include "auth.h"

extern Bool test_auth_conv (void *, int, auth_message *, auth_response **);

Bool
test_auth_conv (void *closure,
                int nmsgs,
                auth_message *msg,
                auth_response **resp)
{
  int page = 1;
  int i;
  nmsgs = 0;
  msg = (auth_message *) calloc (100, sizeof(*msg));

  xscreensaver_auth_test_mode();

# define DIALOG()                                                       \
  fprintf (stderr, "\n%s: page %d\n", blurb(), page++);                 \
  xscreensaver_auth_conv (closure, nmsgs, msg, resp);                   \
  if (*resp)                                                            \
    for (i = 0; i < nmsgs; i++)                                         \
      fprintf (stderr, "%s: resp %d = \"%s\"\n", blurb(), i,            \
               ((*resp)[i].response ? (*resp)[i].response : ""));       \
  nmsgs = 0


  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "1/4 Page One";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "2/4 All work and no play makes Jack a dull boy. "
                   "All work and no play makes Jack a dull boy. ";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_ERROR;
  msg[nmsgs].msg = "3/4 Red";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "4/4 Greets to Crash Override.";
  nmsgs++;
  DIALOG();


  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "1/1 Page Two";
  nmsgs++;
  DIALOG();


  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "1/2 Page Three";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_PROMPT_NOECHO;
  msg[nmsgs].msg = "2/2 Greets to Crash Override and also Joey";
  nmsgs++;


  msg[nmsgs].type = AUTH_MSGTYPE_PROMPT_NOECHO;
  msg[nmsgs].msg = "1/3 Page Four";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "2/3 Hack the planet.";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "3/3 Hack the planet.";
  nmsgs++;
  DIALOG();


  msg[nmsgs].type = AUTH_MSGTYPE_PROMPT_ECHO;
  msg[nmsgs].msg = "1/1 Page Five visible text"
    "over several lines, probably like three or four; y and g.";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_PROMPT_ECHO;
  msg[nmsgs].msg = "1/3 Page six visible text";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "2/3 Poop.";
  nmsgs++;

  msg[nmsgs].type = AUTH_MSGTYPE_INFO;
  msg[nmsgs].msg = "3/3 \xF0\x9F\x92\xA9 \xF0\x9F\x92\xA9 \xF0\x9F\x92\xA9"
    " \xE2\x80\x9C"
    " \xE2\xAC\xA4 \xE2\x80\xA2 \xE2\xAD\x98 "
    "\xE2\x80\x9D";
  nmsgs++;
  DIALOG();

  exit(0);
}

