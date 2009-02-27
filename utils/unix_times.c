/*
 *	UNIX-style Time Functions
 *
 */
#include <stdio.h>
#include <signal.h>
#include <starlet.h>
#include <lib$routines.h>
#include <time.h>
#include "unix_time.h"

static struct timeval END_OF_TIME = {100000000, 0};
static VMS_Timer_AST();

static int vms_itimer_eventflg = 0;

/*
 *	gettimeofday(2) - Returns the current time
 *
 *	NOTE: The timezone portion is useless on VMS.
 *	Even on UNIX, it is only provided for backwards
 *	compatibilty and is not guaranteed to be correct.
 */

int gettimeofday(tv, tz)
struct timeval  *tv;
struct timezone *tz;
{
    timeb_t tmp_time;

    ftime(&tmp_time);

    if (tv != NULL)
    {
	tv->tv_sec  = tmp_time.time;
	tv->tv_usec = tmp_time.millitm * 1000;
    }

    if (tz != NULL)
    {
	tz->tz_minuteswest = tmp_time.timezone;
	tz->tz_dsttime = tmp_time.dstflag;
    }

    return (0);

} /*** End gettimeofday() ***/

/*
 *	VMS version of "setitimer"
 */
int setitimer(which, it, null)
int which;
struct itimerval *it;
int null;
{
   register struct timeval *tv = &it->it_value;
   int Delta[2];
   long int m1, a1;
   static int Timer_EFN = -1;
   static int Timer_EFN_Mask;

   /*
    * We only do ITIMER_REAL
    */
   if (which != ITIMER_REAL) {
      /*
       * Check for "no timer"
       */
      if ((tv->tv_sec == 0) && (tv->tv_usec == 0)) return(0);
         return(1);
      }
   /*
    * If there is a timer running, cancel it
    */
   if (vms_itimer_eventflg) {
      vms_itimer_eventflg = 0;
      sys$cantim(VMS_Timer_AST, 0);
   }
   /*
    * If no timer, we are done
    */
   if (tv->tv_sec == END_OF_TIME.tv_sec) return(0);
   /*
    * Setup the timer
    */
   if (tv->tv_sec <= 100) {
      /*
       * Check for values < 20msec
       */
      if ((tv->tv_sec == 0) && (tv->tv_usec < 20000)) {
         /*
          * Check for "disable" timer
          */
         if (tv->tv_usec == 0) return(0);
         /*
          * Make it 20msec (try 10msec later)
          */
         Delta[1] = -1;
         Delta[0] = -200000;
      } else {
         /*
          * We can do it with single precision arithmetic
          */
         Delta[1] = -1;
         Delta[0] = -((10000000 * tv->tv_sec) + (10 * tv->tv_usec));
      }
   } else {
      /*
       * Calculate Delta
       */
      m1 = -10000000;
      a1 = -(10 * tv->tv_usec);
      lib$emul(&m1, &tv->tv_sec, &a1, Delta);
   }
   /*
    * Set the timer EFN to indicate that the timer is running
    */
   if (Timer_EFN < 0) {
      /*
       * Initialize the timer EFN
       * (UGH!!! Hardwired because DECWindows has 24/25 hardwired)
       */
      Timer_EFN = 22;
      /*
       * Calculate the timer EFN mask
       */
      Timer_EFN_Mask = 1 << Timer_EFN;
   }
   vms_itimer_eventflg = Timer_EFN_Mask;
   /*
    * Start the timer
    */
   if (!(sys$setimr(Timer_EFN, Delta, VMS_Timer_AST, VMS_Timer_AST, 0) & 1)) return(1);
   /*
    * Return success
    */
   return(0);
}

/*
 *	VMS Timer AST routine
 */
static VMS_Timer_AST()
{
	static struct sigcontext scp;

	/*
	 *	The timer is no longer running
	 */
	vms_itimer_eventflg = 0;
	/*
	 *	Make like a SIGALRM signal
	 */
	gsignal(SIGALRM);
}

/*
 *	Fake "getitimer" call
 */
int getitimer(which, it)
int which;
register struct itimerval *it;
{
	/*
	 *	We only deal with the virtual timer
	 */
	if (which != ITIMER_VIRTUAL) return(1);
	/*
	 *	Return ZERO time
	 */
	it->it_value.tv_sec = 0;
	it->it_value.tv_usec = 0;
	it->it_interval.tv_sec = 0;
	it->it_interval.tv_usec = 0;
	return(0);
}

