/*
 *	validate a password for a user
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*
 * Includes
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "vms-pwd.h"
int hash_vms_password(char *output_buf,char *input_buf,int input_length,
                      char *username,int encryption_type,unsigned short salt);

/*
 *
 *	Validate a VMS UserName/Password pair.
 *
 */

int validate_user(name,password)
char *name;
char *password;
{
	char password_buf[64];
	char username_buf[31];
	char encrypt_buf[8];
	register int i;
	register char *cp,*cp1;
	struct passwd *user_entry;

	/*
	 *	Get the users UAF entry
	 */
	user_entry = getpwnam(name);

	/*
	 *	If user_entry == NULL then we got a bad error
	 *	return -1 to indicate a bad error
	 */
	if (user_entry == NULL) return (-1);

	/*
	 *	Uppercase the password
	 */
	cp = password;
	cp1 = password_buf;
	while (*cp)
	   if (islower(*cp))
		*cp1++ = toupper(*cp++);
	   else
		*cp1++ = *cp++;
	/*
	 *	Get the length of the password
	 */
	i = strlen(password);
	/*
	 *	Encrypt the password
	 */
	hash_vms_password(encrypt_buf,password_buf,i,user_entry->pw_name,
			  user_entry->pw_encrypt, user_entry->pw_salt);
	if (memcmp(encrypt_buf,user_entry->pw_passwd,8) == 0)
		return(1);
	else	return(0);
}

