/* config.h.  Generated automatically by configure.  */
/* Define to empty if the keyword does not work.  */
/* #undef const */
 
/* Define to empty if the keyword does not work.  */
/* #undef inline */
 
/* Define to the type of elements in the array set by `getgroups'. Usually
   this is either `int' or `gid_t'.  */
/* #undef GETGROUPS_T */
 
/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */
 
/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
/* #undef HAVE_SYS_WAIT_H */
 
/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */
 
/* Define as the return type of signal handlers (int or void).  */
/* #undef RETSIGTYPE */
 
/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1
 
/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1
 
/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */
 
/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */
 
/* Define if you have the gethostname function.  */
/* #undef HAVE_GETHOSTNAME */
 
/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1
 
/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the seteuid function.  */
#define HAVE_SETEUID 1

/* Define if you have the setreuid function.  */
/* #undef HAVE_SETREUID */

/* Define one of these if they exist (usually not used any more).  */
/* #undef HAVE_NANOSLEEP */
/* #undef HAVE_USLEEP */

/* If left undefined will default to internal Random Number Generator */
#define SRAND srand48
#define LRAND lrand48
#define MAXRAND 2147483648.0

/* Enable use of matherr function */
#define USE_MATHERR 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1
 
/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1
 
/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1
 
/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */
 
/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */
 
/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1
 
/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <syslog.h> header file.  */
#define HAVE_SYSLOG_H 1
 
/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the shadow passwords (or elf).  */
#define HAVE_SHADOW 1

/* Various system defines.  */
#define SYSV 1
#define SVR4 1
/* #undef linux */
/* #undef SOLARIS2 */
/* #undef SUNOS4 */
/* #undef _POSIX_SOURCE */
/* #undef _BSD_SOURCE */
/* #undef _GNU_SOURCE */
/* #undef AIXV3 */
/* #undef LESS_THAN_AIX3_2 */


/* Define if you have XPM (look for it under a X11 dir).  */
/* #undef USE_XPM */

/* Define if you have XPM.  */
#define USE_XPMINC 1

/* Define if you have XMU (Editres).  */
#define USE_XMU 1

/* Define if you have GL (MesaGL).  */
#define USE_GL 1

/* Define if you have DtSaver.  */
/* #undef USE_DTSAVER */

/* Define one of these for sounds.  */
/* #undef USE_RPLAY */
/* #undef USE_NAS */
/* #undef USE_VMSPLAY */
/* #undef DEF_PLAY */

/* Allows xlock to run in root window (some window managers have problems) */
#define USE_VROOT 1

/* Users can not turn off allowroot */
#define ALWAYS_ALLOW_ROOT 1

/* Paranoid administrator option (a check is also done to see if you have it) */
/* #undef USE_SYSLOG */

/* Multiple root users ... security? */
/* #undef USE_MULTIPLE_ROOT */

/* Password screen displayed with mouse motion */
/* #undef USE_MOUSE_MOTION */

/* Some machines may still need this (fd_set errors may be a sign) */
/* #undef USE_OLD_EVENT_LOOP */

/* This patches up old __VMS_VER < 70000000 */
/* #undef USE_VMSUTILS */

/* For personal use you may want to consider: */
/*   Unfriendly paranoid admininistrator or unknown shadow passwd algorithm */
/* #undef USE_XLOCKRC */
 
/* For labs you may want to consider: */

/*   Enable auto-logout code, minutes until auto-logout */
/* #undef USE_AUTO_LOGOUT */

/*   Set default for auto-logout code, hard limit is USE_AUTO_LOGOUT */
/* #undef DEF_AUTO_LOGOUT */

/*   Enable logout button, minutes until button appears */
/* #undef USE_BUTTON_LOGOUT */

/*   Set default for logout button code, hard limit is USE_LOGOUT_BUTTON */
/* #undef DEF_BUTTON_LOGOUT */

/*   Enable automatic logout mode (does not come up in random mode) */
#define USE_BOMB 1

/*   Define one of these with USE_AUTO_LOGOUT,
      USE_LOGOUT_BUTTON, and/or USE_BOMB, if using xdm */
/* #undef CLOSEDOWN_LOGOUT */
/* #undef SESSION_LOGOUT */

/*   File of staff who are exempt */
/* #undef STAFF_FILE */

/*   Netgroup that is exempt */
/* #undef STAFF_NETGROUP */

/*   Enable Digital Unix Enhanced Security */
/* #undef OSF1_ENH_SEC */
