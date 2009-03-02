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
 */


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/vt.h>

#define CONSOLE "/dev/console"

static char *progname = NULL;
static char errmsg[1024]; 

static void usage( int verb )
{
    fprintf( stderr, "%s: usage: %s [-h] <on/off>\n", progname, progname );
    if ( verb )
    {
        fprintf( stderr, "    Allows to enable/disable to VT switching ability\n" ); 
        fprintf( stderr, "      -- %s on -- enable VT switching\n", progname ); 
        fprintf( stderr, "      -- %s off -- disable VT switching\n", progname );
        fprintf( stderr, "    Option -h display this message\n" ); 
    }
    fprintf( stderr, "\n", progname, progname );
}

main( int argc, char **argv )
{
    char *cmd = argv[1];
    int allow = 0, consfd = -1;
    uid_t uid = -1;
    struct stat consstat; 

    /* Program name */
    progname =( progname = strrchr( argv[0], '/' ) ) ? progname +1 : argv[0]; 

    /* Check args */
    if ( argc != 2 && argc != 3 )
    {
        usage( 0 );
        exit( 1 );
    }

    /* Process -h */
    if ( !strcmp( cmd, "-h" ) || !strcmp( cmd, "--help" ) )
    {
        usage( 1 );
        cmd = argv[2]; 
        if ( argc == 2 ) exit( 0 );
    }

    /* Process on/off (case insensitive) */
    if ( strcasecmp( cmd, "on" ) && strcasecmp( cmd, "off" ) )
    {
        usage( 0 );
        exit( 1 );
    }
    allow = !strcasecmp( cmd, "on" );

    /* Check user is allowed to do it */
    uid = getuid();
    if ( stat( CONSOLE, &consstat ) == -1 )
    {
        sprintf( errmsg, "%s: Cannot stat " CONSOLE, progname ); 
        perror( errmsg );
        exit( 1 );
    }
    if ( uid != consstat.st_uid )
    {
        fprintf(stderr,
                "%s: Sorry, you are not allowed to %slock"
                " VT switching: Operation not permitted\n",
                progname,
                allow ? "un" : "" );
        exit( 1 );
    }
    seteuid( 0 ); 

    /* Open console */
    if ( ( consfd = open( CONSOLE, O_RDWR ) ) == -1 )
    {
        sprintf( errmsg, "%s: Cannot open " CONSOLE, progname ); 
        perror( errmsg );
        seteuid( uid ); 
        exit( 1 );
    }

    /* Do it */
    if ( ioctl( consfd, allow?VT_UNLOCKSWITCH:VT_LOCKSWITCH ) == -1 )
    {
        sprintf( errmsg, "%s: Cannot %slock VT switching for " CONSOLE, progname, allow ? "un" : "" ); 
        perror( errmsg );
        seteuid( uid ); 
        exit( 1 );
    }

    /* Terminate */
    close( consfd );
    fprintf( stdout, "VT switching %s\n", allow ? "enabled" : "disabled" ); 
    seteuid( uid ); 
    exit( 0 );
}
