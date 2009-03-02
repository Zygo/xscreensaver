/****************************************************************************/
/*
 * imapsocket.h  -- Module to check for mail using an IMAP socket
 *
 *  Functions to logon to an IMAP server and check the user's INBOX for
 *  RECENT or UNSEEN mail.  Errors may be logged to ~/.xsession-errors if
 *  stderr is redirected by a call to RedirectErrLog(), otherwise they are
 *  written to stderr.
 *
 *  It is intended to be used as a set of library functions by a program
 *  that displays and icon, lights a keyboard LED or otherwise notifies
 *  a user that mail is waiting to be read.
 *
 *  Author: Michael P. Duane    mduane@seanet.com
 *  Date:   August 12, 1997
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

#ifndef IMAPSOCKET_H
#define IMAPSOCKET_H

extern char *my_name;

int		RedirectErrLog( void );
char	*GetProgramName( char *fullname );
int		GetImapCfgInfo( char *cfgfile );
int		InitSocketAddr( void );
int		ServerLogin( void );
void	ServerLogout( void );
int		CheckInbox( void );

#endif /* IMAPSOCKET_H */
