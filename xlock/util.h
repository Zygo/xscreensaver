#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)util.h	4.14 99/06/17 xlockmore" */

#endif

/*-
 * Utilites stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-06-99: Started log. :)
 */

extern unsigned long seconds(void);

#if ( !HAVE_STRDUP )
extern char *strdup(char *);

#endif
