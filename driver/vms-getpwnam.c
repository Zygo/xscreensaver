/*
 *	getpwnam(name) - retrieves a UAF entry
 *
 *	Author:		Patrick L. Mahan
 *	Location:	TGV, Inc
 *	Date:		15-Nov-1991
 *
 *	Purpose:	Provides emulation for the UNIX getpwname routine.
 *
 *	Modification History
 *
 *	Date        | Who	| Version	| Reason
 *	------------+-----------+---------------+---------------------------
 *	15-Nov-1991 | PLM	| 1.0		| First Write
 */

#define PASSWDROUTINES

#include <stdio.h>
#include <descrip.h>
#include <uaidef.h>
#include <string.h>
#include <stdlib.h>
#include <starlet.h>
#include "vms-pwd.h"

struct uic {
   unsigned short uid;
   unsigned short gid;
};

#define TEST(ptr, str)	{ if (ptr == NULL) {	\
				fprintf(stderr, "getpwnam: memory allocation failure for \"%s\"\n",	\
					str);	\
				return ((struct passwd *)(NULL));	\
			  } }

struct passwd *getpwnam(name)
char *name;
{
   int  istatus;
   int  UserNameLen;
   int  UserOwnerLen;
   int  UserDeviceLen;
   int  UserDirLen;
   static char UserName[13];
   static char UserOwner[32];
   static char UserDevice[32];
   static char UserDir[64];
   char *cptr, *sptr;
   unsigned long int UserPwd[2];
   unsigned short int UserSalt;
   unsigned long int UserEncrypt;
   struct uic UicValue;
   struct passwd *entry;

   struct dsc$descriptor_s VMSNAME = 
	{strlen(name), DSC$K_DTYPE_T, DSC$K_CLASS_S, name};

   struct itmlist3 {
	unsigned short int length;
	unsigned short int item;
	unsigned long  int addr;
	unsigned long  int retaddr;
   } ItemList[] = {
   	{12, UAI$_USERNAME, (unsigned long)&UserName, (unsigned long)&UserNameLen},
	{8,  UAI$_PWD, (unsigned long)&UserPwd, 0},
	{4,  UAI$_UIC, (unsigned long)&UicValue, 0},
	{32, UAI$_OWNER, (unsigned long)&UserOwner, (unsigned long)&UserOwnerLen},
	{32, UAI$_DEFDEV, (unsigned long)&UserDevice, (unsigned long)&UserDeviceLen},
	{64, UAI$_DEFDIR, (unsigned long)&UserDir, (unsigned long)&UserDirLen},
	{2,  UAI$_SALT, (unsigned long)&UserSalt, 0},
	{4,  UAI$_ENCRYPT, (unsigned long)&UserEncrypt, 0},
	{0, 0, 0, 0}
   };

   UserNameLen = 0;
   istatus = sys$getuai (0, 0, &VMSNAME, &ItemList, 0, 0, 0);

   if (!(istatus & 1)) {
	fprintf (stderr, "getpwnam: unable to retrieve passwd entry for %s\n",
                 name);
	fprintf (stderr, "getpwnam: vms error number is 0x%x\n", istatus);
	return ((struct passwd *)NULL);
   }

   entry = (struct passwd *) calloc (1, sizeof(struct passwd));
   TEST(entry, "PASSWD_ENTRY");

   entry->pw_uid = UicValue.uid;
   entry->pw_gid = UicValue.gid;
   entry->pw_salt = UserSalt;
   entry->pw_encrypt = UserEncrypt;

   sptr = UserName;
   cptr = calloc (UserNameLen+1, sizeof(char));
   TEST(cptr, "USERNAME");
   strncpy (cptr, sptr, UserNameLen);
   cptr[UserNameLen] = '\0';
   entry->pw_name = cptr;

   cptr = calloc(8, sizeof(char));
   TEST(cptr, "PASSWORD");
   memcpy(cptr, UserPwd, 8);
   entry->pw_passwd = cptr;

   sptr = UserOwner; sptr++;
   cptr = calloc ((int)UserOwner[0]+1, sizeof(char));
   TEST(cptr, "FULLNAME");
   strncpy (cptr, sptr, (int)UserOwner[0]);
   cptr[(int)UserOwner[0]] = '\0';
   entry->pw_gecos = cptr;

   cptr = calloc ((int)UserDevice[0]+(int)UserDir[0]+1, sizeof(char));
   TEST(cptr, "HOME");
   sptr = UserDevice; sptr++;
   strncpy (cptr, sptr, (int)UserDevice[0]);
   sptr = UserDir; sptr++;
   strncat (cptr, sptr, (int)UserDir[0]);
   cptr[(int)UserDevice[0]+(int)UserDir[0]] = '\0';
   entry->pw_dir = cptr;

   cptr = calloc (strlen("SYS$SYSTEM:LOGINOUT.EXE")+1, sizeof(char));
   TEST(cptr,"SHELL");
   strcpy (cptr, "SYS$SYSTEM:LOGINOUT.EXE");
   entry->pw_shell = cptr;

   return (entry);
}
