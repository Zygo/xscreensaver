/* xscreensaver, Copyright (c) 2002 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_INTL_H__
#define __XSCREENSAVER_INTL_H__

#ifdef ENABLE_NLS

# include <libintl.h>
# define _(String) dgettext(GETTEXT_PACKAGE,(String))
# ifdef gettext_noop
#  define N_(String) gettext_noop((String))
# else  /* !gettext_noop */
#  define N_(String) (String)
# endif /* !gettext_noop */

#else  /* !ENABLE_NLS */

# define _(String) (String)
# define N_(String) (String)
# define textdomain(String) (String)
# define gettext(String) (String)
# define dgettext(Domain,String) (String)
# define dcgettext(Domain,String,Type) (String)
# define bindtextdomain(Domain,Directory) (Domain) 

#endif /* !ENABLE_NLS */

#ifndef HAVE_BINDTEXTDOMAIN_CODESET
# define bindtextdomain_codeset(Domain,Codeset) (Domain)
#endif

#endif /* __XSCREENSAVER_INTL_H__ */
