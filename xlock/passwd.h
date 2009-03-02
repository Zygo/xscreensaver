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
