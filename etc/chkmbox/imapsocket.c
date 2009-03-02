/****************************************************************************/
/*
 * imapsocket.c  -- Module to check for mail using an IMAP socket
 *
 *	Functions to logon to an IMAP server and check the user's INBOX for
 *	RECENT or UNSEEN mail.  Errors may be logged to ~/.xsession-errors if
 *	stderr is redirected by a call to RedirectErrLog(), otherwise they are
 *	written to stderr.
 *
 *	It is intended to be used as a set of library functions by a program
 *	that displays and icon, lights a keyboard LED or otherwise notifies
 *	a user that mail is waiting to be read.
 *
 *	Author:	Michael P. Duane	mduane@seanet.com
 *	Date:	August 12, 1997
 *
 * Copyright (c) 1997-98 by Michael P. Duane
 *
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
 * Revision History:
 *
 ****************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef	FALSE
#define	FALSE	0
#define	TRUE	~FALSE
#endif

#define	MAX_BUFFER_SIZE			8192
#define	MAX_BUFFER_LINES		  32

#define	SOCKET_ERROR			(-1)

char	*my_name	= "";

int					fdImap		= (-1);
struct sockaddr_in	addrImap;

int		seq_num				= 0;
char	tag			[ 8];

char	hostname	[64]	= "";
short	port				= 0;

char	user		[64]	= "";
char	passwd		[64]	= "";

char	recv_buf	[MAX_BUFFER_SIZE];
char	*line		[MAX_BUFFER_LINES];

/****************************************************************************/
/*
 * GetProgramName()
 *
 * Extract just the basename from argv[0].  'basename()' doesn't exist
 * on all systems.
 */
char *GetProgramName( char *fullname )
{
char    *name;

    return( name = ( (name = strrchr( fullname, '/' )) ? ++name : fullname ) );

} /* GetProgramName */

/****************************************************************************/
/*
 * RedirectErrLog()
 *
 * Redirect stderr to $HOME/.xsesson-errors.  Create it if it doesn't
 * exist, append to it if it does.
 *
 */
int RedirectErrLog( void )
{
char	*home;
char	xsesserr	[255];
int		mode	= (O_CREAT | O_APPEND | O_WRONLY);
int		fderr;

	if ( (home = getenv( "HOME" )) != NULL ) {
		strcat( strcpy( xsesserr, home ), "/.xsession-errors" );
		if ( (fderr = open( xsesserr, mode, 0600 )) > 0 ) {
			close( STDERR_FILENO );
			if ( dup( fderr ) == STDERR_FILENO )
				close( fderr );
			}
		return( 0 );
		}

	return( -1 );

}/* RedirectErrLog */

/****************************************************************************/
/*
 * LogMessage()
 *
 * Prepend all error messages with my program name and log the corresponding
 * errno description string where appropriate.
 */
void LogMessage( char *msg, int errval )
{

	if ( errval )
		fprintf( stderr, "%s:  %s, %s\n", my_name, msg, strerror( errval ) );
	else
		fprintf( stderr, "%s:  %s\n", my_name, msg );

} /* LogMessage */

/****************************************************************************/
/*
 * ParseToken()
 *
 * Validate the "token = value" sequence to include a known token and
 * a valid assignment operator.  Store the value in a global on success.
 */
static void ParseToken( char *token, char *assign, char *value )
{
char	errmsg	[255];
int		i;

	for ( i=0; i< strlen( token ); i++ )
		*(token+i) = toupper( *(token+i) );

	if ( strcmp( assign, "=" ) ) {
		sprintf( errmsg, "\"%s\" missing assignment", token );
		LogMessage( errmsg, 0 );
		return;
		}

	if ( !strcmp( token, "HOSTNAME" ) )
		strcpy( hostname, value );
	else if ( !strcmp( token, "PORT" ) )
		port = (short)strtol( value, (char **)NULL, 0 );
	else if ( !strcmp( token, "USER" ) )
		strcpy( user, value );
	else if ( !strcmp( token, "PASSWORD" ) )
		strcpy( passwd, value );
	else {
		sprintf( errmsg, "Unexpected configuration token: \"%s\"", token );
		LogMessage( errmsg, 0 );
		}

} /* ParseToken */

/****************************************************************************/
static char *GetNextToken( char *str )
{

	return( strtok( str, " \t\n\r" ) );

} /* GetNextToken */

/****************************************************************************/
/*
 * GetImapCfgInfo()
 *
 * Reads the program configuration file looking for assignments of the
 * form "token = value".  '#' begins a comment that contiues to EOL.
 */
int GetImapCfgInfo( char *cfgfile )
{
FILE	*cfg;
char	txtbuf		[512];
char	*txt;
char	*tok;
char	*assign;
char	*val;

	if ( (cfg = fopen( cfgfile, "r" )) != NULL ) {
		do	{
			if ( (txt = fgets( txtbuf, sizeof( txtbuf ), cfg )) != NULL) {
				if ( (tok = GetNextToken( txt )) ) {
					assign = val = NULL;
					if ( strlen( tok ) ) {
						if ( strchr( tok, '#' ) )
							continue;
						assign	= GetNextToken( NULL );
						val		= GetNextToken( NULL );
						GetNextToken( NULL );	/* strip to eol */
						}
					if ( assign && val )
						ParseToken( tok, assign, val );
					}
				}
			} while( !feof( cfg ) );
		fclose( cfg );
		}
	else {
		LogMessage( cfgfile, errno );
		return( -1 );
		}

	return( 0 );

} /* GetImapCfgInfo */

/****************************************************************************/
/*
 * InitSocketAddr()
 *
 * Setup and validate the host/port address for the IMAP socket
 */

int InitSocketAddr( void )
{
struct hostent	*host_info;
char			addr_str	[ 32];

	if ( (host_info = gethostbyname( hostname )) == NULL ) {
		LogMessage( "Host name error", errno );
		return( -1 );
		}

	sprintf( addr_str,"%u.%u.%u.%u",
			 (unsigned char)host_info->h_addr_list[0][0],
			 (unsigned char)host_info->h_addr_list[0][1],
			 (unsigned char)host_info->h_addr_list[0][2],
			 (unsigned char)host_info->h_addr_list[0][3]
			 );

	addrImap.sin_family			= PF_INET;
	addrImap.sin_addr.s_addr	= inet_addr( addr_str );
	addrImap.sin_port			= htons( port );

	if ( addrImap.sin_addr.s_addr == INADDR_NONE ) {
		LogMessage( "Socket Address Error", errno );
		return( -1 );
		}

	return( 0 );

} /* InitSocketAddr */

/****************************************************************************/
/*
 * ConnectSocket()
 *
 * Open and connect to the IMAP socket
 */

static int ConnectSocket( struct sockaddr_in *addrImap )
{

	if ( addrImap->sin_addr.s_addr == INADDR_NONE ) {
		LogMessage( "Socket Address Error", errno );
		return( -1 );
		}

	if ( (fdImap = socket( AF_INET, SOCK_STREAM, 0 )) == SOCKET_ERROR ) {                 
		LogMessage( "Error opening socket", errno );
		return( -1 );
		}

	if ( connect( fdImap, (struct sockaddr *)addrImap,
            	  sizeof( struct sockaddr )) == SOCKET_ERROR ) {
		close( fdImap );
		fdImap = (-1);
		LogMessage( "Socket Connection error", errno );
		return( -1 );
		}

	return( 0 );

} /* ConnectSocket */

/****************************************************************************/
/*
 * OpenImapSocket()
 *
 * Connect to the IMAP socket and make sure the IMAP service responds.
 */

static int OpenImapSocket( struct sockaddr_in *addrImap )
{
int	i;

	if ( ConnectSocket( addrImap ) )
		return( -1 );

	seq_num = 0;
	memset( recv_buf, 0, sizeof( recv_buf ) );

	for( i=0; i<MAX_BUFFER_LINES; i++ )
		line[i] = NULL;

	if ( recv(  fdImap, (char *)recv_buf,
				sizeof( recv_buf ), 0 ) == SOCKET_ERROR ) {
		close( fdImap );
		fdImap = (-1);
		LogMessage( "Socket revc error", errno );
		return( -1 );
		}

	if ( strncmp( "* OK", recv_buf, 4 ) == 0 ) {
		return( 0 );
		}
	else {	
		close( fdImap );
		LogMessage( "IMAP service timeout", 0 );
		return( -1 );
		}

} /* OpenImapSocket */

/****************************************************************************/
/*
 * ImapCmd()
 *
 * Send an IMAP command to the socket. The "tag" is used by the IMAP
 * protocol to match responses to commands.
 */

static int ImapCmd( char *fmt, ... )
{
char	cmd_buf	[128];
va_list	argp;

	sprintf( tag, "A%3.3d", ++seq_num );
    sprintf( cmd_buf, "%s ", tag );

    va_start( argp, fmt );
	vsprintf( &cmd_buf[ strlen( cmd_buf ) ], fmt, argp );
	va_end( argp );

	if ( send( fdImap, cmd_buf, strlen( cmd_buf ), 0 ) == SOCKET_ERROR ) {
		close( fdImap );
		fdImap = (-1);
		LogMessage( "IMAP send error", errno );
		return( -1 );
		}

	return( 0 );
	 
} /* ImapCmd */

/****************************************************************************/
/*
 * GetImapMsg()
 *
 * Get an IMAP response and check for the tag and for the "OK".
 */

static int GetImapMsg( char *buf, int size )
{
char	tmp	[16];

	memset( buf, 0, size );

	if ( recv( fdImap, (char *)buf, size, 0 ) == SOCKET_ERROR ) {
		close( fdImap );
		fdImap = (-1);
		LogMessage( "IMAP read error", errno );
		return( -1 );
		}

	sprintf( tmp, "%s OK", tag );

	if ( strstr( buf, tmp ) != NULL )
		return( 0 );
	else {
		LogMessage( "IMAP command error", 0 );
		return( -1 );
    	}

} /* GetImapMsg */

/****************************************************************************/
/*
 * ServerLogin( void )
 *
 * Start the IMAP session to check for new mail
 * RETURN: 0 for success or -1 for failure
 */

int ServerLogin( void )
{
int status = -1;

	if ( !OpenImapSocket( &addrImap ) ) {
		if ( !ImapCmd( "LOGIN %s %s\n", user, passwd ) ) {
			if ( GetImapMsg( recv_buf, sizeof( recv_buf ) ) )
				LogMessage( "IMAP LOGIN error", 0 );
			else
				status = 0;
			}
		}

	return( status );

} /* ServerLogin */

/****************************************************************************/
/*
 * ServerLogout()
 *
 * Close the IMAP session
 * RETURN: none
 */

void ServerLogout( void )
{

	if ( !ImapCmd( "LOGOUT\n" ) )
		GetImapMsg( recv_buf, sizeof( recv_buf ) );

	close( fdImap );

} /* ServerLogout */

/****************************************************************************/
/*
 * ParseBufLines()
 *
 * Parse the response into single lines.
 *
 * ARGS:	b - raw response buffer
 *			l - an array char * for the individual lines
 *
 * RETURN:	the number of lines
 *
 */

static int ParseBufLines( char *b, char *l[] )
{
int	i;
		
	for ( i=0; i<MAX_BUFFER_LINES; i++ )
		l[i] = NULL;

	if ( (l[0] = strtok( b, "\n" )) == NULL )
		return( 0 );

	for ( i=1; i<MAX_BUFFER_LINES; i++ ) {
		if ( (l[i] = strtok( NULL, "\n" )) == NULL )
        	return( i );
		}

	return( MAX_BUFFER_LINES );

} /* ParseBufLines */

/****************************************************************************/
/*
 * CheckRecent()
 *
 * This routine looks through an array of lines for the "RECENT" response
 * and returns the number of "RECENT" message.  If there are no "RECENT"
 * messages it then checks for any "UNSEEN" messages.
 *
 * ARGS:	b - raw response buffer
 *			l - an array char * for the individual lines
 *
 * RETURN:	number.  0 is no messages.  For messages found with the
 *          RECENT command number is the count of RECENT messages.  For
 *          messages found with the UNSEEN command 'number' is the
 *          message number of an UNSEEN message.
 */

static int CheckRecent( char *b, char *l[] )
{
int	i;
int	num_msg	= -1;

/*
 *	Check for new messages that have arrived since the last time we looked
 */
	if ( !GetImapMsg( recv_buf, sizeof( recv_buf ) ) ) {
		if ( ParseBufLines( b, l ) > 0 ) {
			for( i=0; i<MAX_BUFFER_LINES; i++ ) {
				if ( strstr( l[i], "RECENT" ) != NULL )
					break;	
				}
			sscanf( l[i], "%*s %d %*s", &num_msg );
			}
		}
/*
 *	If nothing new has arrived check for any messages that may still be
 *	in the mailbox but have not been read
 */
	if ( num_msg <= 0 ) {
		if ( !ImapCmd( "SEARCH UNSEEN\n" ) ) {
			if ( !GetImapMsg( recv_buf, sizeof( recv_buf ) ) ) {
				if ( ParseBufLines( b, l ) > 0 ) {
					for( i=0; i<MAX_BUFFER_LINES; i++ ) {
						if ( strstr( l[i], "SEARCH" ) != NULL )
							break;	
						}
					sscanf( l[i], "%*s %*s %d", &num_msg );
					}
				}
			}
		}
/*
 *	Any non-zero value means there are messages that have not been read.
 *	This is not a count or an index to the newest message
 */
	return( num_msg );

} /* CheckRecent */

/****************************************************************************/
/*
 * CheckInbox()
 *
 * This is the IMAP session to check for new mail
 *
 * RETURN: 1 for no messages, 0 if messages exist. /* Inverted by DAB */
 */

int CheckInbox( void )
{
int status = -1;

	if ( fdImap < 0 ) {
		if ( ServerLogin() )
			return( !status );  /* Inverted by DAB */
		}

	if ( !ImapCmd( "EXAMINE INBOX\n" ) )
		status = CheckRecent( recv_buf, line );

	return( !status ); /* Inverted by DAB */

} /* CheckInbox */

/****************************************************************************/
/* end of imapsocket.c */
