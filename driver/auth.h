/* auth.h --- Providing authentication mechanisms.
 * Copyright © 1993-2022 Jamie Zawinski <jwz@jwz.org>
 * (c) 2007, Quest Software, Inc. All rights reserved.
 * This file is part of XScreenSaver.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */
#ifndef __XSCREENSAVER_AUTH_H__
#define __XSCREENSAVER_AUTH_H__

#undef Bool
#undef True
#undef False
#define Bool int
#define True 1
#define False 0

extern Bool verbose_p;

typedef struct {
  enum {
    AUTH_MSGTYPE_INFO,
    AUTH_MSGTYPE_ERROR,
    AUTH_MSGTYPE_PROMPT_NOECHO,
    AUTH_MSGTYPE_PROMPT_ECHO
  } type;
  const char *msg;
} auth_message;

typedef struct {
  char *response;
} auth_response;


/* To run all authentication methods.
 */
extern void disavow_privileges (void);
extern Bool lock_priv_init (void);
extern Bool lock_init (void);

/* Returns true if authenticated. */
extern Bool xscreensaver_auth (void *closure,
                               Bool (*conv_fn) (void *closure,
                                                int nmsgs,
                                                const auth_message *msg,
                                                auth_response **resp),
                               void (*finished_fn) (void *closure,
                                                    Bool authenticated_p));


/* The implementations, called by xscreensaver_auth.
 */
#ifdef HAVE_KERBEROS
extern Bool kerberos_lock_init (void);
extern Bool kerberos_passwd_valid_p (void *closure, const char *plaintext);
#endif

#ifdef HAVE_PAM
extern Bool pam_priv_init (void);
extern Bool pam_try_unlock (void *closure,
                            Bool (*conv_fn) (void *closure,
                                             int nmsgs,
                                             const auth_message *msg,
                                             auth_response **resp));
#endif

#ifdef PASSWD_HELPER_PROGRAM
extern Bool ext_priv_init (void);
extern Bool ext_passwd_valid_p (void *closure, const char *plaintext);
#endif

extern Bool pwent_lock_init (void);
extern Bool pwent_priv_init (void);
extern Bool pwent_passwd_valid_p (void *closure, const char *plaintext);

/* GUI conversation function to pass to xscreensaver_auth. */
extern Bool xscreensaver_auth_conv (void *closure,
                                    int num_msg,
                                    const auth_message *msg,
                                    auth_response **resp);
extern void xscreensaver_auth_finished (void *closure, Bool authenticated_p);
extern void xscreensaver_splash (void *root_widget, Bool disable_settings_p);

#endif /* __XSCREENSAVER_AUTH_H__ */

