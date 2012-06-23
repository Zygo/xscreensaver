/* auth.h --- Providing authentication mechanisms.
 *
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
#ifndef XSS_AUTH_H
#define XSS_AUTH_H

#include "types.h"

#undef Bool
#undef True
#undef False
#define Bool int
#define True 1
#define False 0

struct auth_message {
  enum {
    AUTH_MSGTYPE_INFO,
    AUTH_MSGTYPE_ERROR,
    AUTH_MSGTYPE_PROMPT_NOECHO,
    AUTH_MSGTYPE_PROMPT_ECHO
  } type;
  const char *msg;
};

struct auth_response {
  char *response;
};

int
gui_auth_conv(int num_msg,
	  const struct auth_message auth_msgs[],
	  struct auth_response **resp,
	  saver_info *si);

void
xss_authenticate(saver_info *si, Bool verbose_p);

void
auth_finished_cb (saver_info *si);

#endif
