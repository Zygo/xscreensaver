#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)vtlock_proc.c	1.1	98/10/08 xlockmore";
#endif

/* Copyright (c) R. Cohen-Scali, 1998. */

/*
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * <remi.cohenscali@pobox.com>
 *       98/10/08: Eric Lassauge - vtlock_proc renamed from lockvt_proc.
 *				   misc corrections.
 */

#include "xlock.h"	/* lots of include come from here */
#ifdef USE_VTLOCK

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <errno.h>
#if defined( __linux__ )
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/vt.h>
#endif

/* Misc definitions to modify if not applicable on the current system */
#if defined( __linux__ ) && HAVE_DIRENT_H
#define PROCDIR 	"/proc"
#define DEVDIR 		"/dev"
#define TTY		DEVDIR "/tty%c"
#define CONSOLE 	DEVDIR "/console"
#define BASEVTNAME	DEVDIR "/tty%d"
#define XPATH 		"/usr/X11R6/bin" /* default path of X server */
#define XNAME 		"X"		 /* X server name : mandatory ! */
#define MAX_VT		20
#else
#error Sorry ! You must adapt this file to your system !
#endif

/* This struct contains the vt tty refs used for comparison */
struct inode_ref {
    unsigned short n; 
    char	   ref[MAXPATHLEN+1]; 
}; 

/* Static variables used to keep X device, inode and process */
static dev_t xdev =(dev_t)-1; 
static ino_t xino =(ino_t)-1;
static pid_t xproc =(pid_t)-1;
static unsigned short xvt =(unsigned short)0; 
static unsigned short othervt =(unsigned short)0; 

/* Static variables to keep vt devices */
static int n_ttys = -1;
static struct inode_ref ttyinodes[MAX_NR_CONSOLES];

/* Prototypes */
static unsigned short get_active_vt(void);
static ino_t find_x(const char *, const char *, dev_t * );
static int proc_dir_select(const struct dirent *);
static pid_t find_x_proc(int, dev_t, ino_t);
static int find_tty_inodes(struct inode_ref *);
static int scan_x_fds(struct inode_ref *, int, pid_t);

/* use scan_dir from iostuff.c */
extern int  scan_dir(const char *directoryname, struct dirent ***namelist,
                     int         (*specify) (const struct dirent *),
                     int         (*compare) (const struct dirent * const *, const struct dirent * const *));

extern int is_x_vt_active(int display_nr);
extern int set_x_vt_active(int display_nr);
extern int restore_vt_active(void);

/*
 * get_active_vt
 * -------------
 * Find with ioctl on console what is the active vt number.
 * The number found by this will be compared to the X vt number to
 * find if vt locking is possible.
 */
static unsigned short
get_active_vt(void)
{
    struct vt_stat vtstat;
    int fd = -1;

    fd = open( CONSOLE, O_RDONLY );
    if ( fd == -1 ) return( -1 );
    if ( ioctl( fd, VT_GETSTATE,(void *)&vtstat ) == -1 )
    {
        close( fd );
        return( -1 );
    }
    close( fd );
    return vtstat.v_active;
}

/*
 * find_x
 * ------
 * Find X server executable file inode.
 * The inode number found here will be used to find in the X process
 * in the proc fs.
 */
static ino_t
find_x(const  char *path, const char *name, dev_t *pxdev )
{
    struct stat stbuf;
    char xpath[MAXPATHLEN+1]; 

    (void) sprintf( xpath, "%s/%s", path, name ); 
    if ( stat( xpath, &stbuf ) != -1 ) {
        (void) strcpy( xpath, name );
        while ( S_ISLNK(stbuf.st_mode) ) {
            char buf[MAXPATHLEN+1];

            if ( readlink( xpath, buf, MAXPATHLEN ) == -1 || ! *buf )
                return( (ino_t) -1 );

            /* 
	     * Let's try to know if the path is absolute or relative 
             * It is absolute if it begin with '/',
             * else is relative , 
	     * then we need to add the path given as argument
	     */
            if ( buf[0] != '/' )
              (void) sprintf( xpath, "%s/%s", path, buf );
            else
              (void) strcpy( xpath, buf );
            /* Stat linked file */
            if ( stat( xpath, &stbuf ) == -1 ) return( (ino_t) -1 );
        }
    }
    else
      return( (ino_t) -1 );
    if ( pxdev ) *pxdev = stbuf.st_dev; 
    return stbuf.st_ino;  
}

/*
 * proc_dir_select
 * ---------------
 * Callback called for each proc fs dir in order to select all
 * processes directories. Only returns 1 for directory entries
 * with a number [0->9].
 */
static int
proc_dir_select( const struct dirent *entry )
{
    return( entry->d_name[0] >= '0' && entry->d_name[0] <= '9' ); 
}

/*
 * find_x_proc
 * -----------
 * This function scans the /proc dir in order to find the X process
 * for the given display, knowing the X server file inode and device.
 */
static pid_t
find_x_proc(int disp_nr, dev_t lxdev, ino_t lxino)
{
    /*static*/ char xdisp[10]; 
    /*static*/ char xcmd_ref[MAXPATHLEN+1]; 
    struct stat stbuf; 
    pid_t proc = -1;
    struct dirent **namelist = NULL;
    int curn = 0,names = 0; 
    int lencmd ;

    /* These are the display string searched in X cmd running (e.g.: :1) */
    /* and the searched  value of the link (e.g.: "[0301]:286753") */
    (void) sprintf( xdisp, ":%d", disp_nr ); 
    (void) sprintf( xcmd_ref, "[%04x]:%ld", lxdev, lxino );
    lencmd = strlen(xcmd_ref);
    if ( stat( PROCDIR, &stbuf ) == -1 ) return( (pid_t)-1 );
    namelist = (struct dirent **) malloc(sizeof (struct dirent *));
    if ( (names=scan_dir( PROCDIR, &namelist, proc_dir_select, alphasort)) == -1 )
    {
      (void) free((void *) namelist);
      return( (pid_t)-1 );
    }
    while ( curn < names ) {
        char pname[MAXPATHLEN+1];
        char buf[MAXPATHLEN+1]; 

        (void) sprintf( pname, PROCDIR "/%s/exe", namelist[curn]->d_name );
	(void) memset((char *) buf, 0, sizeof (buf));
        if ( readlink( pname, buf, MAXPATHLEN ) <= 0 ) {
            /* This is unreadable, let's continue */
            curn++; 
            continue;
        }
        /* 
	 * If the strings are equals, we found an X process, but is it the one
         * managing the wanted display ? 
         * We are going to try to know it by reading the command line used to
         * invoke the server.
	 */
        if ( !strncmp( buf, xcmd_ref, lencmd ) ) {
            char cmdlinepath[MAXPATHLEN+1];
            char cmdlinebuf[1024];	/* 1k should be enough */
            int cmdlinefd;
            off_t cmdlinesz; 
            char *p; 

            proc =(pid_t)atoi( namelist[curn]->d_name );
            (void) sprintf( cmdlinepath, PROCDIR "/%s/cmdline", namelist[curn]->d_name );
            if ( ( cmdlinefd = open( cmdlinepath, O_RDONLY ) ) == -1 ) {
                curn++; 
                continue;
            }
            /* Ask the kernel what it was (actually do ps)
             * If stat'ed the cmdline proc file as a size of zero
             * No means to dynamically allocate buffer !
	     */
            if ( ( cmdlinesz = read( cmdlinefd, cmdlinebuf, 1023 ) ) == -1) {
                close( cmdlinefd ); 
                curn++; 
                continue;
            }
            /*
	     * The command line proc file contains all command line args 
	     * separated by NULL characters. We are going to replace all ^@ 
	     * with a space, then we'll searched for the display string 
	     * (:0, :1, ..., :N). If a match is found, then we got the good 
	     * process. If no match is found, then we can assume this is the 
	     * good process only if we are searching for the display #0 manager
	     * (the default display). In other case the process is discarded. 
	     */
            p = cmdlinebuf;
            while ( p < cmdlinebuf+cmdlinesz ) {
                if ( !*p ) *p = ' ';
                p++;
            }
            close( cmdlinefd );
            if ( strstr( cmdlinebuf, xdisp ) ) break;
            else if ( !disp_nr )
              break;
            else
              proc =(pid_t)-1; 
        }
        curn++; 
    }
    (void) free((void *) namelist);
    return proc; 
}

/*
 * find_tty_inodes
 * ---------------
 * This function finds all vt console dev and inode.
 * Warning ! The dev is not the console major:minor
 * but the filesystem's major:minor containing the special
 * device file.
 */
static int
find_tty_inodes( struct inode_ref *inotab )
{
    struct stat stbuf;
    int ln_ttys = 0;
    int ix = 0;
    char name[MAXPATHLEN+1];

    for ( ix = 1; ix < MAX_NR_CONSOLES; ix++ ) {
        (void) sprintf( name, BASEVTNAME, ix );
        if ( stat( name, &stbuf ) == -1 )
          continue;
        inotab[ln_ttys].n = ix; 
        (void) sprintf( inotab[ln_ttys].ref, "[%04x]:%ld", stbuf.st_dev, stbuf.st_ino );
        ln_ttys++; 
    }
    return ln_ttys;
}

/*
 * scan_x_fds
 * ----------
 * This function scans all found process file descriptors
 * to find a link towards a tty device special file.
 */
static int
scan_x_fds( struct inode_ref *inotab, int ln_ttys, pid_t proc )
{
    char xfddir[MAXPATHLEN+1]; 
    struct dirent **namelist=NULL;
    int curn = 0;

    (void) sprintf( xfddir, PROCDIR "/%d/fd", proc );
    namelist = (struct dirent **) malloc(sizeof (struct dirent *));
    if ( scan_dir( xfddir, &namelist, NULL, alphasort ) == -1 ) 
    {
        (void) free((void *) namelist);
        return 0;
    }
    while ( namelist[curn] ) {
        char linkname[MAXPATHLEN+1];
        char linkref[MAXPATHLEN+1];
        struct stat stbuf; 
        int ix; 

        (void) sprintf( linkname, "%s/%s", xfddir, namelist[curn]->d_name );
        if ( stat( linkname, &stbuf ) == -1 ) {
            /* If cannot stat it, just discard it */
            curn++;
            continue;
        }
        if ( !S_ISDIR(stbuf.st_mode) ) {
            /*
	     * Let's read the link to get the file device and inode 
	     * (e.g.: [0301]:6203) 
	     */
	    (void) memset((char *) linkref, 0, sizeof (linkref));
            if ( readlink( linkname, linkref, MAXPATHLEN ) <= 0 ) {
                curn++;
                continue;
            }
            for ( ix = 0; ix < ln_ttys; ix++ )
	    {
              if ( !strncmp( linkref, inotab[ix].ref, strlen( inotab[ix].ref ) ) )
              {
      		(void) free((void *) namelist);
                return inotab[ix].n;
              }
            }
        }
        curn++;
    }
    (void) free((void *) namelist);
    return 0; 
}    

/*
 * is_x_vt_active
 * --------------
 * This function is the one called from vtlock.c and which tells
 * if the X vt is active or not. 
 * If the X vt is active it returns 1 else 0.
 * -1 is returned in case of errno or if cannot stat.
 */
int
is_x_vt_active(int display_nr)
{
    int active_vt = 0; 
    char *path = getenv( "PATH" );
    char *envtokenizer =(char *)NULL;
    struct stat stbuf; 

    /* The active VT */
    active_vt = get_active_vt();
/* ERROR ???  comparison between unsigned long and negative */
    if ( xino == -1 )
      if (stat( XPATH, &stbuf ) == -1 ||
          (xino = find_x( XPATH, XNAME, &xdev )) == (ino_t)-1 ) {
          /* No executable at the default location */
          /* Let's try with $PATH */
          if ( !path ) return -1; 
          envtokenizer = strtok( path, ":" );
          while ( envtokenizer ) {
              if ( stat( envtokenizer, &stbuf ) != -1 )
/* ERROR ???  comparison between unsigned long and negative */
                if ( ( xino = find_x( envtokenizer, XNAME, &xdev ) ) != -1 )
                  break; 
              envtokenizer = strtok( (char *)NULL, ":" );
          }
          if ( !envtokenizer ) return -1; 
      }
    if ((xproc ==(pid_t)-1 ) &&
        (xproc = find_x_proc(display_nr, xdev, xino)) == -1)
      return -1; 
    if ((n_ttys == -1) &&
        (n_ttys = find_tty_inodes(ttyinodes))== 0)
      return -1; 
    if ( ! xvt && ( xvt = scan_x_fds( ttyinodes, n_ttys, xproc ) ) == 0 ) return -1; 
    return(active_vt == xvt); 
}

/*
 * set_x_vt_active
 * ---------------
 * This is the core function. It check if the VT of the X server managing
 * display number <display_nr> is active.
 */
int
set_x_vt_active(int display_nr)
{
    int fd = -1;
    
    if ( !othervt ) othervt = get_active_vt(); 
    if ( !xvt ) (void)is_x_vt_active( display_nr ); 
    fd = open( CONSOLE, O_RDONLY );
    if ( fd == -1 ) return( -1 );
    if ( ioctl( fd, VT_ACTIVATE, xvt ) == -1 ) {
        close( fd );
        return( -1 );
    }
    if ( ioctl( fd, VT_WAITACTIVE, xvt ) == -1 ) {
        close( fd );
        return( -1 );
    }
    close( fd );
    return 0; 
}

/*
 * restore_vt_active
 * -----------------
 * This function revert the work of the previous one. Actually it restores
 * the VT which was active when we locked.
 */
int
restore_vt_active(void)
{
    int fd = -1;
    
    if ( !othervt ) return 0;
    fd = open( CONSOLE, O_RDONLY );
    if ( fd == -1 ) return( -1 );
    if ( ioctl( fd, VT_ACTIVATE, othervt ) == -1 ) {
        close( fd );
        return( -1 );
    }
    if ( ioctl( fd, VT_WAITACTIVE, xvt ) == -1 ) {
        /* We can achieve that vt has switched */
        othervt =(unsigned short)0; 
        close( fd );
        return( -1 );
    }
    othervt =(unsigned short)0; 
    close( fd );
    return 0; 
}
#endif
