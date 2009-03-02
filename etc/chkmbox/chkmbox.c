/****************************************************************************/
/*
 * xmbox.c  -- Module to check for mail using an IMAP socket
 *
 *  Logon to an IMAP server and check for unread messages.  Return 0 if
 *  no RECENT or UNSEEN mail exists in the user's INBOX or > 0 if messages
 *  exist.
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

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "imapsocket.h"

char	dftname		[]	= "chkmbox.cfg";

/*************************************************************************/
int main( int argc, char **argv )
{
char	*cfgname;
int		msgnbr		= -1;

	my_name = GetProgramName( *argv );

	if ( argc >= 2 )
		cfgname = *(argv+1);
	else
		cfgname = dftname;

	RedirectErrLog();
	GetImapCfgInfo( cfgname );
	if ( !InitSocketAddr() )
		if ( !ServerLogin() ) /* Any errors here it will behave as "no mail" */
			msgnbr = CheckInbox();
	ServerLogout();

	return( msgnbr );

} /* main */
