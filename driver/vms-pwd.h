/*	@(#)pwd.h 1.7 89/08/24 SMI; from S5R2 1.1	*/

#ifndef	__pwd_h
#define	__pwd_h

#ifdef vax11c
#include <types.h>
#else
#include <sys/types.h>
#endif /* vax11c */

#ifdef PASSWDROUTINES
#define	EXTERN
#else
#define EXTERN extern
#endif /* PASSWDROUTINES */

struct passwd {
	char	*pw_name;
	char	*pw_passwd;
	int	pw_uid;
	int	pw_gid;
	short	pw_salt;
	int	pw_encrypt;
	char	*pw_age;
	char	*pw_comment;
	char	*pw_gecos;
	char	*pw_dir;
	char	*pw_shell;
};


#ifndef	_POSIX_SOURCE
extern struct passwd *getpwent();

struct comment {
        char    *c_dept;
        char    *c_name;
        char    *c_acct;
        char    *c_bin;
};

#endif

EXTERN struct passwd *getpwuid(/* uid_t uid */);
EXTERN struct passwd *getpwnam(/* char *name */);

#endif	/* !__pwd_h */
