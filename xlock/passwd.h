#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)passwd.h	4.14 99/06/17 xlockmore" */

#endif

/*-
 * Password stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-06-99: Started log. :)
 */

extern void initPasswd(void);
extern int checkPasswd(char *buffer);

#ifdef USE_PAM
#include <security/pam_appl.h>
/*#include <security/pam_misc.h>*/

void PAM_putText( const struct pam_message *msg, struct pam_response *resp, Bool PAM_echokeys );
#endif
