#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)erase.h	4.14 99/06/17 xlockmore" */

#endif

/*-
 * Erase stuff
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-06-99: Started log. :)
 */

#if HAVE_GETTIMEOFDAY
		GETTIMEOFDAY(&tp);
		if (tp.tv_sec - t0 >= erasetime ) {
			break;
		}
#endif
		if (actual_delay > 0 && ((LOOPVAR % granularity) == 0))
#if HAVE_GETTIMEOFDAY
		{
			interval = (int) (tp.tv_usec - t_prev +
				1000000 * ( tp.tv_sec - t1_prev ));
			interval = actual_delay * granularity - interval;
			if (interval > 0)
				usleep(interval);
			GETTIMEOFDAY(&tp);
			t_prev = (int) (tp.tv_usec);
			t1_prev = (int) (tp.tv_sec);
		}
#else
			usleep( actual_delay * granularity );
#endif
