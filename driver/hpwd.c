/*
 *	VAX/VMS Password hashing routines:
 *
 *	uses the System Service SYS$HASH_PASSWORD
 *
 */

#include <syidef.h>
#include <descrip.h>
#include <string.h>
#include <starlet.h>
/*
 *	Hashing routine
 */
hash_vms_password(output_buf,input_buf,input_length,username,encryption_type,salt)
char *output_buf;
char *input_buf;
int input_length;
char *username;
int encryption_type;
unsigned short salt;
{
	struct dsc$descriptor_s password;
	struct dsc$descriptor_s user;

	/*
	 *  Check the VMS Version. If this is V5.4 or later, then
	 *  we can use the new system service SYS$HASH_PASSWORD.  Else
	 *  fail and return garbage.
	 */

	static char VMS_Version[32];
	struct {
		unsigned short int Size;
		unsigned short int Code;
		char *Buffer;
		unsigned short int *Resultant_Size;
	} Item_List[2]={32, SYI$_VERSION, VMS_Version, 0, 0, 0};
	struct {int Size; char *Ptr;} Descr1;

	/*
	 *	Get the information
	 */
	sys$getsyiw(0,0,0,Item_List,0,0,0);
	/*
	 *	Call the old routine if this isn't V5.4 or later...
	 */
#ifndef __DECC
	if ((VMS_Version[1] < '5') ||
	    ((VMS_Version[1] == '5') && (VMS_Version[3] < '4'))) {
		printf("None supported OS version\n");
	    	return(1);
	}
#endif /* !__DECC */
	/*
	 *	Call the SYS$HASH_PASSWORD system service...
	 */
	password.dsc$b_dtype	= DSC$K_DTYPE_T;
        password.dsc$b_class	= DSC$K_CLASS_S;
        password.dsc$w_length	= input_length;
	password.dsc$a_pointer	= input_buf;
	user.dsc$b_dtype	= DSC$K_DTYPE_T;
	user.dsc$b_class	= DSC$K_CLASS_S;
	user.dsc$w_length	= strlen(username);
	user.dsc$a_pointer	= username;
	sys$hash_password (&password, encryption_type, salt, &user, output_buf);
}

