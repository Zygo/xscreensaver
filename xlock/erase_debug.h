#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)erase_debug.h	4.14 99/06/17 xlockmore" */

#endif

/*-
 * Erase Debug stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-06-99: Started log. :)
 */

#if HAVE_GETTIMEOFDAY && defined( DEBUG )
   GETTIMEOFDAY(&tp);
   t_prev = (int) (tp.tv_sec * 1000 + tp.tv_usec / 1000);
   (void) printf( "Elapsed time in %s : %d\n" , ERASEMODE , t_prev - t_0 );
#endif
