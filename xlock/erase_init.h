#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)erase_init.h	4.14 99/06/17 xlockmore" */

#endif

/*-
 * Erase Initialize stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-06-99: Started log. :)
 */

#if HAVE_GETTIMEOFDAY
	struct timeval tp;
	int         interval, t_prev, t1_prev , t0;
#ifdef DEBUG
	int t_0;
#endif

	GETTIMEOFDAY(&tp);
	t_prev = tp.tv_usec;
	t1_prev = tp.tv_sec;
        t0 = tp.tv_sec;

#ifdef DEBUG
	t_0 = tp.tv_sec * 1000 + tp.tv_usec / 1000;
#endif
#endif
