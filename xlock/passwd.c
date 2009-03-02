#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)passwd.c	4.02 97/04/01 xlockmore";

#endif

/*-
 * passwd.c - passwd stuff.
 *
 * Copyright (c) 1988-91 by Patrick J. Naughton.
 *
 * Revision History:
 *
 * Changes maintained by David Bagley <bagleyd@tux.org>
 * 18-Feb-99: allowroot option no longer ignored for PAM authentication.
 *            Sven Paas <Sven.Paas@t-online.de>
 * 24-Jan-98: Updated PAM support and made it configure-able.
 *            Marc Ewing <marc@redhat.com>  Original PAM support from
 *            25-Jul-96 Michael K. Johnson <johnsonm@redhat.com>
 * 18-Jan-98: Kerberos corrections by RLS <sinkr@cbl.umces.edu>
 * 25-May-96: When xlock is compiled with shadow passwords it will still
 *            work on non shadowed systems.  Marek Michalkiewicz
 *            <marekm@i17linuxb.ists.pwr.wroc.pl>
 * 25-Feb-96: Lewis Muhlenkamp
 *            Added in ability for any of the root accounts to unlock
 *            screen.  Message now gets sent to syslog if root does the
 *            unlocking.
 * 23-Dec-95: Ron Hitchens <ron@idiom.com> reorganized.
 * 10-Dec-95: More context handling stuff for DCE thanks to
 *            Terje Marthinussen <terjem@cc.uit.no>.
 * 01-Sep-95: DCE code added thanks to Heath A. Kehoe
 *            <hakehoe@icaen.uiowa.edu>.
 * 24-Jun-95: Extracted from xlock.c, encrypted passwords are now fetched
 *            on start-up to ensure correct operation (except Ultrix).
 */

#include "xlock.h"
#include "iostuff.h"
#include "passwd.h"

#ifdef VMS
#include <str$routines.h>
#include <starlet.h>
#define ROOT "SYSTEM"
#else
#define ROOT "root"
#endif

extern Bool allowroot;
extern Bool inroot;
extern Bool inwindow;
extern Bool grabmouse;
extern Bool nolock;
extern char *cpasswd;

#if defined( USE_XLOCKRC ) || defined( FALLBACK_XLOCKRC )
#include <sys/stat.h>		/* order of includes matters */
#endif

#if !defined( VMS ) && !defined( SUNOS_ADJUNCT_PASSWD )
#include <pwd.h>
#endif

#if defined( USE_XLOCKRC ) || defined( FALLBACK_XLOCKRC )  || defined( __bsdi__ ) || defined( HAVE_KRB4 ) || defined( HAVE_KRB5 )
#include <sys/param.h>
#endif


#if defined( __bsdi__ ) && _BSDI_VERSION >= 199608
#define       BSD_AUTH
#endif

#ifdef        BSD_AUTH
#include <login_cap.h>
static login_cap_t *lc = NULL;
static login_cap_t *rlc = NULL;

#endif

#if ( HAVE_SYSLOG_H && defined( USE_SYSLOG ))
#include <syslog.h>
#endif

#if ( HAVE_FCNTL_H && (defined( USE_MULTIPLE_ROOT ) || defined( USE_MULTIPLE_USER )))
#include <fcntl.h>
#include <errno.h>
extern int  errno;

void        get_multiple(struct passwd *);
void        set_multiple(int uid);

#define BUFMAX 1024		/* Maximum size of pipe buffer */


/* Linked list to keep track of everyone that's authorized * to unlock the
   screen. */
struct pwln {
	char       *pw_name;
#ifdef        BSD_AUTH
	login_cap_t *pw_lc;
#else
	char       *pw_passwd;
#endif
	struct pwln *next;
};
typedef struct pwln pwlnode;
typedef struct pwln *pwlptr;

pwlptr      pwll, pwllh = (pwlptr) NULL;
extern pwlptr pwllh;

/* Function that creates and initializes a new node that * will be added to
   the linked list. */
pwlptr
new_pwlnode(void)
{
	pwlptr      pwl;

	if ((pwl = (pwlptr) malloc(sizeof (pwlnode))) == 0)
		return ((pwlptr) ENOMEM);

	pwl->pw_name = (char *) NULL;
#ifdef BSD_AUTH
	pwl->pw_lc = NULL;
#else
	pwl->pw_passwd = (char *) NULL;
#endif
	pwl->next = (pwlptr) NULL;

	return (pwl);
}
#endif

#ifdef ultrix
#include <auth.h>

#if defined( HAVE_SETEUID ) || defined(HAVE_SETREUID )
gid_t       rgid, egid;

#endif
#endif

#ifdef OSF1_ENH_SEC
#include <sys/security.h>
#include <prot.h>
#endif

#if defined( __linux__ ) && defined( __ELF__ ) && !defined( HAVE_SHADOW )
/*-
 * Linux may or may not have shadow passwords, so it is best to make the same
 * binary work with both shadow and non-shadow passwords. It's easy with the
 * ELF libc since it has getspnam() and there is no need for any additional
 * libraries like libshadow.a.
 */
#define HAVE_SHADOW
#endif

#if defined( __linux__ ) && defined( HAVE_SHADOW ) && defined( HAVE_PW_ENCRYPT )
/*-
 * Deprecated - long passwords have known weaknesses.  Also, pw_encrypt is
 *  non-standard (requires libshadow.a) while everything else you need to
 *  support shadow passwords is in the standard (ELF) libc.
 */
#define crypt pw_encrypt
#endif

#ifdef HAVE_SHADOW
#ifndef __hpux
#include <shadow.h>
#endif
#endif

#ifdef SUNOS_ADJUNCT_PASSWD
#include <sys/label.h>
#include <sys/audit.h>
#include <pwdadj.h>
#define passwd passwd_adjunct
#define pw_passwd pwa_passwd
#define getpwnam(_s) getpwanam(_s)
#define pw_name pwa_name
#define getpwuid(_s) (((_s)==0)?getpwanam(ROOT):getpwanam(cuserid(NULL)))
#endif /* SUNOS_ADJUNCT_PASSWD */

/* System V Release 4 redefinitions of BSD functions and structures */
#if !defined( SHADOW ) && (defined( SYSV ) || defined( SVR4 ))

#ifdef LESS_THAN_AIX3_2
struct passwd {
	char       *pw_name;
	char       *pw_passwd;
	uid_t       pw_uid;
	gid_t       pw_gid;
	char       *pw_gecos;
	char       *pw_dir;
	char       *pw_shell;
};

#endif /* LESS_THAN_AIX3_2 */

#ifdef HPUX_SECURE_PASSWD
#include <hpsecurity.h>
#include <prot.h>
#define crypt bigcrypt

/* #define seteuid(_eu) setresuid(-1, _eu, -1) */
#define passwd s_passwd
#define getpwnam(_s) getspwnam(_s)
#define getpwuid(_u) getspwuid(_u)
#endif /* HPUX_SECURE_PASSWD */

#endif /* defined( SYSV ) || defined( SVR4 ) */

#ifdef VMS
#include <uaidef.h>
#define VMS_PASSLENGTH 9
static short uai_salt, root_salt;
static char hash_password[VMS_PASSLENGTH], hash_system[VMS_PASSLENGTH];
static char root_password[VMS_PASSLENGTH], root_system[VMS_PASSLENGTH];
static char uai_encrypt, root_encrypt;

struct ascid {
	short       len;
	char        dtype;
	char        c_class;
	char       *addr;
};

static struct ascid username, rootuser;

struct itmlst {
	short       buflen;
	short       code;
	long        addr;
	long        retadr;
};

#endif /* VMS */

#ifdef HP_PASSWDETC		/* HAVE_SYS_WAIT_H */
#include <sys/wait.h>
#endif /* HP_PASSWDETC */

#ifdef AFS
#include <afs/kauth.h>
#include <afs/kautils.h>
#endif /* AFS */

char        user[PASSLENGTH];
#ifdef GLOBAL_UNLOCK
char        global_user[PASSLENGTH];
#endif

#ifdef PAM
#include <security/pam_appl.h>
/* used to pass the password to the conversation function */
static char *PAM_password;

/*-
 * PAM conversation function
 * Here we assume (for now, at least) that echo on means login name, and
 * echo off means password.
 */
static int
PAM_conv(int num_msg,
	 /* const */ struct pam_message **msg,
	 struct pam_response **resp,
	 void *appdata_ptr)
{
	int         replies = 0;
	struct pam_response *reply = NULL;


#define COPY_STRING(s) (s) ? strdup(s) : NULL


	reply = (struct pam_response *) malloc(sizeof (struct pam_response) *
					       num_msg);

	if (!reply)
		return PAM_CONV_ERR;


	for (replies = 0; replies < num_msg; replies++) {
		switch (msg[replies]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = COPY_STRING(user);
				/* PAM frees resp */
				break;
			case PAM_PROMPT_ECHO_OFF:
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = COPY_STRING(PAM_password);
				/* PAM frees resp */
				break;
			case PAM_TEXT_INFO:
				/* ignore it... */
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = NULL;
				break;
			case PAM_ERROR_MSG:
				/* ignore it... */
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = NULL;
				break;
			default:
				/* Must be an error of some sort... */
				(void) free((void *) reply);
				return PAM_CONV_ERR;
		}
	}
	*resp = reply;
	return PAM_SUCCESS;
}
static struct pam_conv PAM_conversation =
{
	&PAM_conv,
	NULL
};

#endif /* PAM */

#if !defined( ultrix ) && !defined( PAM )
static char userpass[PASSLENGTH];
static char rootpass[PASSLENGTH];

#ifdef VMS
static char root[] = ROOT;

#endif
#endif /* !ultrix */

#ifdef DCE_PASSWD
static int  usernet, rootnet;
static int  check_dce_net_passwd(char *, char *);

#endif

#if defined( HAVE_KRB4 ) || defined( HAVE_KRB5 )
#ifdef HAVE_KRB4
#include <krb.h>
#else /* HAVE_KRB5 */
#include <krb5.h>
#endif
static int  krb_check_password(struct passwd *, char *);

#endif

#if defined(__cplusplus) || defined(c_plusplus)
#ifdef SunCplusplus
#include <crypt.h>
#else
#if 0
#if 1
extern char *crypt(char *, char *);

#else
extern char *crypt(const char *, const char *);

#endif
#endif
#endif
#endif

#if (defined( USE_XLOCKRC ) || (!defined( OSF1_ENH_SEC ) && !defined( HP_PASSWDETC ) && !defined( VMS )))
static struct passwd *
my_passwd_entry(void)
{
	int         uid;
	struct passwd *pw;

#ifdef HAVE_SHADOW
	struct spwd *spw;

#endif

	uid = (int) getuid();
#ifndef SUNOS_ADJUNCT_PASSWD
	{
		char       *logname = NULL;

		pw = 0;
		logname = getenv("LOGNAME");
		if (!logname)
			logname = getenv("USER");
		if (logname) {
			pw = getpwnam(logname);
			if (pw && (pw->pw_uid != uid))
				pw = 0;
		}
	}
	if (!pw)
#endif
		pw = getpwuid(uid);
	if (!pw)
		return (pw);
#ifdef HAVE_SHADOW
	if ((spw = getspnam(pw->pw_name)) != NULL) {
		char       *tmp;	/* swap */

		tmp = pw->pw_passwd;
		pw->pw_passwd = spw->sp_pwdp;
		spw->sp_pwdp = tmp;
	}
	endspent();
#endif
	return (pw);
}
#endif

#ifdef SAFEWORD
int
checkDynamic()
{
	struct passwd *pw;
	char       *buf;

#ifndef SAFELOG_SWCHECK
#define SAFELOG_SWCHECK "/progs/safelog/swcheck"
#endif
#ifndef SAFELOG_IDENT
#define SAFELOG_IDENT "/progs/safelog/ident"
#endif


	if (!(pw = my_passwd_entry())) {
		/*if ((pw = (struct passwd *) getpwuid(getuid())) == NULL) */
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			     "%s: could not get user shell.\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	return (!(strcmp(pw->pw_shell, SAFELOG_SWCHECK) &&
		  strcmp(pw->pw_shell, SAFELOG_IDENT)));
}
#endif

static void
getUserName(void)
{

#ifdef VMS
	(void) strcpy(user, cuserid(NULL));
#else /* !VMS */
#ifdef HP_PASSWDETC

/*-
 * The PasswdEtc libraries have replacement passwd functions that make
 * queries to DomainOS registries.  Unfortunately, these functions do
 * wierd things to the process (at minimum, signal handlers get changed,
 * there are probably other things as well) that cause xlock to become
 * unstable.
 *
 * As a (really, really sick) workaround, we'll fork() and do the getpw*()
 * calls in the child, and pass the information back through a pipe.
 */
	struct passwd *pw;
	int         pipefd[2], n, total = 0, stat_loc;
	pid_t       pid;
	char       *buf;

	pipe(pipefd);

	if ((pid = fork()) == 0) {
		(void) close(pipefd[0]);
		pw = getpwuid(getuid());
		write(pipefd[1], pw->pw_name, strlen(pw->pw_name));
		(void) close(pipefd[1]);
		_exit(0);
	}
	if (pid < 0) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get user password (fork failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	(void) close(pipefd[1]);

	while ((n = read(pipefd[0], &(user[total]), 50)) > 0)
		total += n;

	wait(&stat_loc);

	if (n < 0) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
		 "%s: could not get user name (read failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	user[total] = 0;

	if (total < 1) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get user name (lookups failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
#else /* !HP_PASSWDETC */
#ifdef OSF1_ENH_SEC
	struct pr_passwd *pw;
	char       *buf;

	/*if ((pw = getprpwuid(getuid())) == NULL) */
	if ((pw = getprpwuid(starting_ruid())) == NULL) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get user name.\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	(void) strcpy(user, pw->ufld.fd_name);

#else /* !OSF1_ENH_SEC */

	struct passwd *pw;
	char       *buf;

	if (!(pw = my_passwd_entry())) {
		/*if ((pw = (struct passwd *) getpwuid(getuid())) == NULL) */
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get user name.\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	(void) strcpy(user, pw->pw_name);

#if ( HAVE_FCNTL_H && (defined( USE_MULTIPLE_ROOT ) || defined( USE_MULTIPLE_USER )))
	get_multiple(pw);
#endif

#endif /* !OSF1_ENH_SEC */
#endif /* !HP_PASSWDETC */
#endif /* !VMS */
}

#if defined( USE_XLOCKRC ) || defined( FALLBACK_XLOCKRC )

#define CPASSLENGTH 14
#define CPASSCHARS 64
static void
gpasskey(char *pass)
{
	static char saltchars[CPASSCHARS + 1] =
	"abcdefghijklmnopwrstuvwxyzABCDEFGHIJKLMNOPWRSTUVWXYZ1234567890./";
	char        pw[9], salt[3], *pw2;

#if 0
	char       *getpass(const char *);

#endif

	(void) strncpy(pw, (char *) getpass("Key: "), sizeof (pw));
	pw2 = (char *) getpass("Again:");
	if (strcmp(pw, pw2) != 0) {
		(void) fprintf(stderr, "%s: Mismatch, try again\n", ProgramName);
		exit(1);
	}
	/* grab a random printable character that is not a colon */
	salt[0] = saltchars[LRAND() & (CPASSCHARS - 1)];
	salt[1] = saltchars[LRAND() & (CPASSCHARS - 1)];
	salt[2] = '\0';
	(void) strncpy(pass, (char *) crypt(pw, salt), CPASSLENGTH);
}

static void
gpass(void)
{
	FILE       *fp;
	char        xlockrc[MAXPATHLEN], *home;

	if (!cpasswd || !*cpasswd) {
		/*
		 * No password given on command line or from database, get from
		 * $HOME/.xlockrc instead.
		 */
		if ((home = getenv("HOME")) == 0) {
			/* struct passwd *p = getpwuid(getuid()); */
			struct passwd *p = my_passwd_entry();

			if (p == 0) {
				char       *buf = (char *) malloc(strlen(ProgramName) + 80);

				(void) sprintf(buf,
					  "%s: Who are you?\n", ProgramName);
				error(buf);
				(void) free((void *) buf);	/* Should never get here */
			}
			home = p->pw_dir;
		}
		(void) strncpy(xlockrc, home, MAXPATHLEN);
		(void) strncat(xlockrc, "/.xlockrc", MAXPATHLEN);
		if ((fp = my_fopen(xlockrc, "r")) == NULL) {
			if ((fp = my_fopen(xlockrc, "w")) != NULL)
				(void) fchmod(fileno(fp), 0600);
#if defined(HAVE_KRB4) || defined(HAVE_KRB5)
			(void) strcpy(userpass, "*");
#else
			gpasskey(userpass);
#endif /* KRB4 || KRB5 */
			if (fp)
				(void) fprintf(fp, "%s\n", userpass);
		} else {
			char        buf[BUFSIZ];

			(void) fchmod(fileno(fp), 0600);
			buf[0] = '\0';
			if ((fgets(buf, sizeof buf, fp) == NULL) ||
			    (!(strlen(buf) == CPASSLENGTH - 1 ||
#if defined(HAVE_KRB4) || defined(HAVE_KRB5)
			       (((strlen(buf) == 1) || strlen(buf) == 2)
				&& buf[0] == '*') ||
#endif
			       (strlen(buf) == CPASSLENGTH && buf[CPASSLENGTH - 1] == '\n')))) {
				(void) fprintf(stderr, "%s: %s crypted password %s\n", xlockrc,
				       buf[0] == '\0' ? "null" : "bad", buf);
#if defined(HAVE_KRB4) || defined(HAVE_KRB5)
				gpasskey(buf);
#else
				exit(1);
#endif
			}
			buf[CPASSLENGTH - 1] = '\0';
			(void) strncpy(userpass, buf, CPASSLENGTH);
		}
		if (fp)
			(void) fclose(fp);
	} else {

		if (strlen(cpasswd) != CPASSLENGTH - 1) {
			(void) fprintf(stderr, "%s: bad crypted password %s\n",
				       ProgramName, cpasswd);
			exit(1);
		} else
			(void) strncpy(userpass, cpasswd, CPASSLENGTH);
	}
}

#endif /* USE_XLOCKRC || FALLBACK_XLOCKRC */

#if !defined( ultrix ) && !defined( DCE_PASSWD ) && !defined( BSD_AUTH )  && !defined ( PAM )
#ifndef USE_XLOCKRC

#if defined( HAVE_SHADOW ) && !defined( AFS )
static int
passwd_invalid(char *passwd)
{
	int         i = strlen(passwd);

	return (i == 1 || i == 2);
}
#endif

static void
getCryptedUserPasswd(void)
{

#ifdef VMS
	struct itmlst il[4];

	il[0].buflen = 2;
	il[0].code = UAI$_SALT;
	il[0].addr = (long) &uai_salt;
	il[0].retadr = 0;
	il[1].buflen = 8;
	il[1].code = UAI$_PWD;
	il[1].addr = (long) &hash_password;
	il[1].retadr = 0;
	il[2].buflen = 1;
	il[2].code = UAI$_ENCRYPT;
	il[2].addr = (long) &uai_encrypt;
	il[2].retadr = 0;
	il[3].buflen = 0;
	il[3].code = 0;
	il[3].addr = 0;
	il[3].retadr = 0;

	username.len = strlen(user);
	username.dtype = 0;
	username.c_class = 0;
	username.addr = user;

	sys$getuai(0, 0, &username, &il, 0, 0, 0);
#else /* !VMS */
#ifdef HP_PASSWDETC

/*-
 * still very sick, see above
 */
	struct passwd *pw;
	int         pipefd[2], n, total = 0, stat_loc;
	pid_t       pid;
	char       *buf;

	pipe(pipefd);

	if ((pid = fork()) == 0) {
		(void) close(pipefd[0]);
		pw = getpwuid(getuid());
		write(pipefd[1], pw->pw_passwd, strlen(pw->pw_passwd));
		(void) close(pipefd[1]);
		_exit(0);
	}
	if (pid < 0) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get user password (fork failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	(void) close(pipefd[1]);

	while ((n = read(pipefd[0], &(userpass[total]), 50)) > 0)
		total += n;

	wait(&stat_loc);

	if (n < 0) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get user password (read failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	user[total] = 0;

	if (total < 1) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get user password (lookups failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
#else /* !HP_PASSWDETC */
#ifdef OSF1_ENH_SEC
	struct pr_passwd *pw;
	char       *buf;

	/*if ((pw = getprpwuid(getuid())) == NULL) */
	if ((pw = getprpwuid(starting_ruid())) == NULL) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
		"%s: could not get encrypted user password.\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	(void) strcpy(userpass, pw->ufld.fd_encrypt);
#else /* !OSF1_ENH_SEC */

	struct passwd *pw;
	char       *buf;

	if (!(pw = my_passwd_entry())) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
		"%s: could not get encrypted user password.\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	/*if ((pw = (struct passwd *) getpwuid(getuid())) == NULL) */
	/* Check if there is any chance of unlocking the display later...  */
	/* Program probably needs to be setuid to root.  */
	(void) strcpy(userpass, pw->pw_passwd);
#ifdef HAVE_SHADOW
#ifndef AFS
	if (passwd_invalid(pw->pw_passwd)) {
		extern Bool verbose, debug;

		buf = (char *) malloc(strlen(ProgramName) + 80);
		if (verbose || debug) {
			(void) fprintf(stderr,
			  "%s: it looks like you have shadow passwording.\n",
				       ProgramName);
			(void) fprintf(stderr, "Contact your administrator to setgid or\n");
			(void) fprintf(stderr, "setuid %s for /etc/shadow read access.\n",
				       ProgramName);
#ifdef FALLBACK_XLOCKRC
			(void) fprintf(stderr, "Falling back on $HOME/.xlockrc password.\n");
#endif
		}
#ifdef FALLBACK_XLOCKRC
		gpass();
#endif
	}
#endif /* !AFS */
#endif /* HAVE_SHADOW */
#if ( HAVE_FCNTL_H && defined( USE_MULTIPLE_USER ))
	set_multiple(getuid());
#endif /* HAVE_FCNTL_H && MULTIPLE_USER */
#endif /* !OSF1_ENH_SEC */
#endif /* !HP_PASSWDETC */
#endif /* !VMS */
}

#endif /* !USE_XLOCKRC */

static void
getCryptedRootPasswd(void)
{

#ifdef VMS
	struct itmlst il[4];

	il[0].buflen = 2;
	il[0].code = UAI$_SALT;
	il[0].addr = (long) &root_salt;
	il[0].retadr = 0;
	il[1].buflen = 8;
	il[1].code = UAI$_PWD;
	il[1].addr = (long) &root_password;
	il[1].retadr = 0;
	il[2].buflen = 1;
	il[2].code = UAI$_ENCRYPT;
	il[2].addr = (long) &root_encrypt;
	il[2].retadr = 0;
	il[3].buflen = 0;
	il[3].code = 0;
	il[3].addr = 0;
	il[3].retadr = 0;

	rootuser.len = strlen(root);
	rootuser.dtype = 0;
	rootuser.c_class = 0;
	rootuser.addr = root;

	sys$getuai(0, 0, &rootuser, &il, 0, 0, 0);
#else /* !VMS */
#ifdef HP_PASSWDETC

/*-
 * Still really, really sick.  See above.
 */
	struct passwd *pw;
	int         pipefd[2], n, total = 0, stat_loc;
	pid_t       pid;
	char       *buf;

	pipe(pipefd);

	if ((pid = fork()) == 0) {
		(void) close(pipefd[0]);
		pw = getpwnam(ROOT);
		write(pipefd[1], pw->pw_passwd, strlen(pw->pw_passwd));
		(void) close(pipefd[1]);
		_exit(0);
	}
	if (pid < 0) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get root password (fork failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	(void) close(pipefd[1]);

	while ((n = read(pipefd[0], &(rootpass[total]), 50)) > 0)
		total += n;

	wait(&stat_loc);

	if (n < 0) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get root password (read failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	rootpass[total] = 0;

	if (total < 1) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: could not get root password (lookups failed)\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
#else /* !HP_PASSWDETC */
#ifdef OSF1_ENH_SEC
	struct pr_passwd *pw;
	char       *buf;

	if ((pw = getprpwnam(ROOT)) == NULL) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
		"%s: could not get encrypted root password.\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	(void) strcpy(rootpass, pw->ufld.fd_encrypt);

#else /* !OSF1_ENH_SEC */
	struct passwd *pw;
	char       *buf;

#ifdef HAVE_SHADOW
	struct spwd *spw;

#endif

	if (!(pw = getpwnam(ROOT)))
		if (!(pw = getpwuid(0))) {
			/*if ((pw = (struct passwd *) getpwuid(0)) == NULL) */
			buf = (char *) malloc(strlen(ProgramName) + 80);
			(void) sprintf(buf,
				       "%s: could not get encrypted root password.\n", ProgramName);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
#ifdef HAVE_SHADOW
	if ((spw = getspnam(pw->pw_name)) != NULL) {
		char       *tmp;	/* swap */

		tmp = pw->pw_passwd;
		pw->pw_passwd = spw->sp_pwdp;
		spw->sp_pwdp = tmp;
	}
	endspent();
#endif
	(void) strcpy(rootpass, pw->pw_passwd);

#if ( HAVE_FCNTL_H && defined( USE_MULTIPLE_ROOT ))
	set_multiple(0);
#endif /* HAVE_FCNTL_H && MULTIPLE_ROOT */

#endif /* !OSF1_ENH_SEC */
#endif /* !HP_PASSWDETC */
#endif /* !VMS */
}

#endif /* !ultrix && !DCE_PASSWD && !BSD_AUTH && !PAM */


#ifdef GLOBAL_UNLOCK
void
checkUser(char *buffer)
{
#ifdef OSF1_ENH_SEC
	struct pr_passwd *pw;
#else /* !OSF1_ENH_SEC */
	struct passwd *pw;
#ifdef HAVE_SHADOW
	struct spwd *spw;

#endif
#endif /* !OSF1_ENH_SEC */
	pw = (struct passwd *)getpwnam(buffer);

	if (pw == NULL)
		(void) strcpy(userpass, "*");
	else {
#ifdef HAVE_SHADOW
	if ((spw = getspnam(pw->pw_name)) != NULL) {
		char       *tmp;	/* swap */

		tmp = pw->pw_passwd;
		pw->pw_passwd = spw->sp_pwdp;
		spw->sp_pwdp = tmp;
	}
	endspent();
#endif
		(void) strcpy(userpass, pw->pw_passwd);
	}

}
#endif



/*-
 * We do not allow for root to have no password, but we handle the case
 * where the user has no password correctly; they have to hit return
 * only.
 */
int
checkPasswd(char *buffer)
{
	int         done = False;

#ifdef VMS
	struct ascid password;

	password.len = strlen(buffer);
	password.dtype = 0;
	password.c_class = 0;
	password.addr = buffer;

	str$upcase(&password, &password);

	sys$hash_password(&password, uai_encrypt, uai_salt,
			  &username, &hash_system);

	hash_password[VMS_PASSLENGTH - 1] = 0;
	hash_system[VMS_PASSLENGTH - 1] = 0;

	done = !strcmp(hash_password, hash_system);
	if (!done && allowroot) {
		sys$hash_password(&password, root_encrypt, root_salt,
				  &rootuser, &root_system);
		root_password[VMS_PASSLENGTH - 1] = 0;
		root_system[VMS_PASSLENGTH - 1] = 0;
		done = !strcmp(root_password, root_system);
	}
#else /* !VMS */

#ifdef DCE_PASSWD
	if (usernet)
		done = check_dce_net_passwd(user, buffer);
	else
		done = !strcmp(userpass, crypt(buffer, userpass));

	if (done)
		return True;
	if (!allowroot)
		return False;

	if (rootnet)
		done = check_dce_net_passwd(ROOT, buffer);
	else
		done = !strcmp(rootpass, crypt(buffer, rootpass));
#else /* !DCE_PASSWD */

#ifdef PAM
/*-
 * Use PAM to do authentication.  No session logging, only authentication.
 * Bail out if there are any errors.
 * For now, don't try to display strings explaining errors.
 * Later, we could integrate PAM more by posting errors to the
 * user.
 * Query: should we be using PAM_SILENT to shut PAM up?
 */
	pam_handle_t *pamh;
	int         pam_error;
#if BAD_PAM
	uid_t       ruid;
#define BAD_PAM_SETUID	seteuid(ruid);
#else
#define BAD_PAM_SETUID
#endif

#define PAM_BAIL if (pam_error != PAM_SUCCESS) { \
    pam_end(pamh, 0); BAD_PAM_SETUID return 0; \
}
#if BAD_PAM
	ruid = getuid();  /* the real user we are running as */
#endif
	PAM_password = buffer;
	pam_error = pam_start("xlock", user, &PAM_conversation, &pamh);
	PAM_BAIL;
#if BAD_PAM
	(void) seteuid(0); /* temporarily go to root so that pam can get shadow password */
#endif
	pam_error = pam_authenticate(pamh, 0);
	if (pam_error != PAM_SUCCESS) {
                if (!allowroot) {
                        pam_end(pamh, 0);
                        return 0;
                }

		/* Try as root; bail if no success there either */
		pam_error = pam_set_item(pamh, PAM_USER, ROOT);
		PAM_BAIL;
		pam_error = pam_authenticate(pamh, 0);
		PAM_BAIL;
	}
#if BAD_PAM
	(void) seteuid(ruid); /* back to user's privileges */
#endif
	/* Do not do account management or credentials; credentials
	 * are not needed and account management would just lock up
	 * a computer and require root to come and unlock it.  Blech.
	 */
	pam_end(pamh, PAM_SUCCESS);
	/* If this point is reached, the user has been authenticated. */
	done = True;

#else /* !PAM */

#ifdef ultrix

#ifdef HAVE_SETEUID
	(void) setegid(egid);
#else
#ifdef HAVE_SETREUID
	(void) setregid(rgid, egid);
#endif
#endif
	done = ((authenticate_user((struct passwd *) getpwnam(user),
				   buffer, NULL) >= 0) || (allowroot &&
			 (authenticate_user((struct passwd *) getpwnam(ROOT),
					    buffer, NULL) >= 0)));
#ifdef HAVE_SETEUID
	(void) setegid(rgid);
#else
#ifdef HAVE_SETREUID
	(void) setregid(egid, rgid);
#endif
#endif

#else /* !ultrix */

#ifdef BSD_AUTH
	char       *pass;
	char       *style;
	char       *name;

#if ( HAVE_FCNTL_H && (defined( USE_MULTIPLE_ROOT ) || defined( USE_MULTIPLE_USER )))
	/* Scan through the linked list until you match a password.  Print
	 * message to log if password match doesn't equal the user.
	 *
	 * This should be changed to allow the user name to be typed in also
	 * to make this more secure.
	 */
	for (pwll = pwllh; done == 0 && pwll->next; pwll = pwll->next) {
		name = pwll->pw_name;
		lc = pwll->pw_lc;
#else
	name = user;
#endif
	if ((pass = strchr(buffer, ':')) != NULL) {
		*pass++ = '\0';
		style = login_getstyle(lc, buffer, "auth-xlock");
		if (auth_response(name, lc->lc_class, style,
				  "response", NULL, "", pass) > 0)
			done = True;
		else if (rlc != NULL) {
			style = login_getstyle(rlc, buffer, "auth-xlock");
			if (auth_response(ROOT, rlc->lc_class, style,
					  "response", NULL, "", pass) > 0)
				done = True;
		}
		pass[-1] = ':';
	}
	if (!done) {
		style = login_getstyle(lc, NULL, "auth-xlock");
		if (auth_response(name, lc->lc_class, style,
				  "response", NULL, "", buffer) > 0)
			done = True;
		else if (rlc != NULL) {
			style = login_getstyle(rlc, NULL, "auth-xlock");
			if (auth_response(ROOT, rlc->lc_class, style,
					  "response", NULL, "", buffer) > 0)
				done = True;
		}
	}
#if ( HAVE_FCNTL_H && (defined( USE_MULTIPLE_ROOT ) || defined( USE_MULTIPLE_USER )))
}
#endif

#else /* !BSD_AUTH */

#ifdef AFS
	{
		char       *reason;

		/* check afs passwd first, then local, then root */
		if (!ka_UserAuthenticate(user, "", 0, buffer, 0, &reason))
			return True;

	}
#endif /* !AFS */
#if defined(HAVE_KRB4) || defined(HAVE_KRB5)
	{
		/* Somehow, buffer gets 'erased' after either doing a krb pw check
		   or a reg pw check */
		char        buffer2[1024];

		(void) strncpy(buffer2, buffer, sizeof (buffer2));
#if 0
		if (!strcmp(userpass, "*"))
#endif
			if (krb_check_password((struct passwd *) getpwuid(getuid()), buffer2))
				return True;
	}
#endif /* !HAVE_KRB4 && !HAVE_KRB5 */
#if ( HAVE_FCNTL_H && (defined( USE_MULTIPLE_ROOT ) || defined( USE_MULTIPLE_USER )))
	if (!done) {
		/* Scan through the linked list until you match a password.  Print
		 * message to log if password match does not equal the user.
		 *
		 * This should be changed to allow the user name to be typed in also
		 * to make this more secure.
		 */
		for (pwll = pwllh; pwll->next; pwll = pwll->next)
			if (!strcmp((char *) crypt(buffer, pwll->pw_passwd), pwll->pw_passwd)) {
#if ( HAVE_SYSLOG_H && defined( USE_SYSLOG ))
				if (strcmp(user, pwll->pw_name) != 0)
					syslog(SYSLOG_NOTICE, "%s: %s unlocked screen",
					       ProgramName, pwll->pw_name);
#endif
				return True;
			}
	}
#endif
	if (!done) {
		done = (!strcmp((char *) crypt(buffer, userpass), userpass));
		/* userpass is used */
		if (!*userpass && *buffer)
			/*
			 * the user has no password, but something was typed anyway.
			 * sounds fishy: don't let him in...
			 */
			done = False;
	}
#if 0
	(void) printf("buffer=%s, encrypt=%s, userpass=%s\n", buffer,
		      (char *) crypt(buffer, userpass), userpass);
#endif
	if (!done) {
		done = (allowroot &&
			!strcmp((char *) crypt(buffer, rootpass), rootpass));
		if (allowroot && !*rootpass && !*buffer)
			/*
			 * root has no password, don't let him in...
			 */
			done = False;
#if ( HAVE_SYSLOG_H && defined( USE_SYSLOG ))
		if (done)
			syslog(SYSLOG_NOTICE, "%s: %s unlocked screen", ProgramName, ROOT);
#endif
	}
#endif /* !BSD_AUTH */
#endif /* !ultrix */
#endif /* !PAM */
#endif /* !DCE_PASSWD */
#endif /* !VMS */

	return done;
}

/*-
 * Functions for DCE authentication
 *
 * Revision History:
 * 21-Aug-95: Added fallback to static password file [HAK]
 * 06-Jul-95: Mods by Heath A. Kehoe <hakehoe@icaen.uiowa.edu> for
 *            inclusion into xlockmore
 * May-95: Created by Phil O'Connell <philo@icaen.uiowa.edu>
 */

#ifdef DCE_PASSWD
#include <pthread.h>
#include <dce/sec_login.h>
#ifdef AIXV3
#include <userpw.h>
#endif

static void
initDCE(void)
{
	sec_login_handle_t login_context;
	error_status_t error_status;
	boolean32   valid;
	struct passwd *pwd;

#ifdef AIXV3
	struct userpw *userpwd;

#endif
	char       *buf;

	/* test to see if this user exists on the network registry */
	valid = sec_login_setup_identity((unsigned_char_p_t) user,
			  sec_login_no_flags, &login_context, &error_status);
	if (!valid) {
		switch (error_status) {
			case sec_rgy_object_not_found:
				break;
			case sec_rgy_server_unavailable:
				(void) fprintf(stderr, "%s: the network registry is not available.\n",
					       ProgramName);
				break;
			case sec_login_s_no_memory:
				buf = (char *) malloc(strlen(ProgramName) + 80);
				(void) sprintf(buf,
					 "%s: out of memory\n", ProgramName);
				error(buf);
				(void) free((void *) buf);	/* Should never get here */
			default:
				(void) fprintf(stderr,
					       "%s: sec_login_setup_identity() returned status %d\n",
					    ProgramName, (int) error_status);
				break;
		}

		pwd = getpwnam(user);
		if (!pwd || strlen(pwd->pw_passwd) < 10) {
			buf = (char *) malloc(strlen(ProgramName) + 80);
			(void) sprintf(buf,
			   "%s: could not get user password\n", ProgramName);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
		usernet = 0;
		(void) strcpy(userpass, pwd->pw_passwd);
	} else
		usernet = 1;

	if (allowroot) {
#ifdef AIXV3
		userpwd = getuserpw("root");
		(void) strcpy(rootpass, userpwd->upw_passwd);
		if (!(strcmp(rootpass, ""))) {
			(void) fprintf(stderr, "%s: could not get root password\n", ProgramName);
			allowroot = 0;
		} else {
			rootnet = 0;
		}
#else
		valid = sec_login_setup_identity((unsigned_char_p_t) ROOT,
			  sec_login_no_flags, &login_context, &error_status);
		if (!valid) {
			switch (error_status) {
				case sec_rgy_object_not_found:
					break;
				case sec_rgy_server_unavailable:
					(void) fprintf(stderr, "%s: the network registry is not available.\n",
						       ProgramName);
					break;
				case sec_login_s_no_memory:
					buf = (char *) malloc(strlen(ProgramName) + 80);
					(void) sprintf(buf,
					 "%s: out of memory\n", ProgramName);
					error(buf);
					(void) free((void *) buf);	/* Should never get here */
				default:
					(void) fprintf(stderr,
						       "%s: sec_login_setup_identity() returned status %d\n",
					    ProgramName, (int) error_status);
					break;
			}

			pwd = getpwuid(0);
			if (!pwd || strlen(pwd->pw_passwd) < 10) {
				(void) fprintf(stderr,
					       "%s: could not get root password\n", ProgramName);
				allowroot = 0;
			}
			rootnet = 0;
			(void) strcpy(rootpass, pwd->pw_passwd);
		} else
			rootnet = 1;
#endif
	}
	pthread_lock_global_np();
}

static char *
error_string(error_status_t error_status)
{
	static char buf[60];

	switch (error_status) {
		case error_status_ok:
			return "no error";
		case sec_rgy_object_not_found:
			return "The principal does not exist";
		case sec_rgy_server_unavailable:
			return "The network registry is not available";
		case sec_login_s_no_memory:
			return "Not enough memory is available to complete the operation";
		case sec_login_s_already_valid:
			return "The login context has already been validated";
		case sec_login_s_default_use:
			return "Can't validate the default context";
		case sec_login_s_acct_invalid:
			return "The account is invalid or has expired";
		case sec_login_s_unsupp_passwd_type:
			return "The password type is not supported";
		case sec_login_s_context_invalid:
			return "The login context itself is not valid";
		default:
			(void) sprintf(buf, "error status #%d", (int) error_status);
			return buf;
	}
}


/*-
 *----------------------------------------------------------------------
 *     Function Created 5/95 to be used with xlock to validate DCE
 *     passwords.  Routine simply returns a (1) if the the variable
 *     PASS is the USER's PASSWORD, else it returns a (0).
 *     Functions used:
 *
 *               sec_login_setup_identity
 *               sec_login_validate_identity
 *               sec_login_certify_identity
 *
 *     where setup_identity obtains the login context for the USER.
 *     This identity is then validated with validate_identity.  Finally,
 *     cerfify_identity is called to make sure that the Security
 *       Server used to set up and validate a login context is legitimate.
 *
 *       Created by Phil O'Connell
 *     philo@icaen.uiowa.edu
 *     Student Programmer
 *
 *-----------------------------------------------------------------------
 */

static int
check_dce_net_passwd(char *usr, char *pass)
{
	sec_login_handle_t login_context;
	error_status_t error_status;
	sec_passwd_rec_t password;
	boolean32   reset_password;
	sec_login_auth_src_t auth_src;
	unsigned_char_p_t principal_name;
	boolean32   valid = 0;
	char       *passcpy;
	boolean32   current_context;


	pthread_unlock_global_np();
	/* -------------------- SETUP IDENTITY--------------------------------- */

	principal_name = (unsigned_char_p_t) usr;

	/*
	 * We would rather like to refresh and existing login context instead of
	 * making a new one.
	 */

	sec_login_get_current_context(&login_context, &error_status);
	if (error_status != error_status_ok) {
		current_context = 0;
		(void) fprintf(stderr,
		       "get_current_context failed! Setting up a new one\n");

		valid = sec_login_setup_identity(principal_name, sec_login_no_flags,
					      &login_context, &error_status);

		if (!valid) {
			(void) fprintf(stderr, "sec_login_setup_identity() failed: %s\n",
				       error_string(error_status));
			pthread_lock_global_np();
			return False;
		}
	} else
		current_context = 1;

/*--------------- VALIDATE IDENTITY ---------------------------------*/

	/* make a copy of pass, because sec_login_validate_identity() will
	   clobber the plaintext password passed to it */
	passcpy = (char *) strdup(pass);

	password.key.key_type = sec_passwd_plain;
	password.key.tagged_union.plain = (unsigned char *) passcpy;
	password.pepper = NULL;
	password.version_number = sec_passwd_c_version_none;

	valid = sec_login_validate_identity(login_context, &password, &reset_password, &auth_src, &error_status);

	/* report unusual error conditions */
	if (error_status != error_status_ok &&
	    error_status != sec_rgy_passwd_invalid &&
	    error_status != sec_login_s_already_valid &&
	    error_status != sec_login_s_null_password) {
		(void) fprintf(stderr, "sec_login_validate_identity failed: %s\n",
			       error_string(error_status));
	}
	/* done with the copy of the password */
	(void) free((void *) passcpy);

	/* Refresh the context if we already have one */
	if (current_context) {
		if (!sec_login_refresh_identity(login_context, &error_status)) {
			(void) fprintf(stderr, "sec_login_refresh_identity failed: %s\n",
				       error_string(error_status));
		} else {
			passcpy = (char *) strdup(pass);

			password.key.key_type = sec_passwd_plain;
			password.key.tagged_union.plain = (unsigned char *) passcpy;
			password.pepper = NULL;
			password.version_number = sec_passwd_c_version_none;

			/* Have to validate the refreshed context */
			valid = sec_login_validate_identity(login_context,
			&password, &reset_password, &auth_src, &error_status);

			/* report unusual error conditions */
			if (error_status != error_status_ok &&
			    error_status != sec_rgy_passwd_invalid &&
			    error_status != sec_login_s_null_password) {
				(void) fprintf(stderr, "sec_login_validate_identity failed: %s\n",
					       error_string(error_status));
			}
		}
		/* done with the copy of the password */
		(void) free((void *) passcpy);
	}
	/* make sure that the authentication service is not an imposter */
	if (valid) {
		if (!sec_login_certify_identity(login_context, &error_status)) {
			(void) fprintf(stderr, "Authentication service is an imposter!\n");
			/* logoutUser(); */
			valid = 0;
		}
	}
	pthread_lock_global_np();
	return valid;
}

#endif /* DCE_PASSWD */

#ifdef HAVE_KRB4
int
krb_check_password(struct passwd *pwd, char *pass)
{
	char        realm[REALM_SZ];
	char        tkfile[MAXPATHLEN];

	/* find local realm */
	if (krb_get_lrealm(realm, 1) != KSUCCESS)
#ifdef KRB_REALM
		/* krb_get_default_realm() may not work well on Solaris */
		(void) strncpy(realm, KRB_REALM, sizeof (realm));
#else
		(void) strncpy(realm, krb_get_default_realm(), sizeof (realm));
#endif
	/* Construct a ticket file */
	(void) sprintf(tkfile, "/tmp/tkt_%d", pwd->pw_uid);

	/* Now, let's make the ticket file named above the _active_ tkt file */
	krb_set_tkt_string(tkfile);

	/* ask the kerberos server for a ticket! */
	if (krb_get_pw_in_tkt(pwd->pw_name, "", realm,
			      "krbtgt", realm,
			      DEFAULT_TKT_LIFE,
			      pass) == INTK_OK)
		return True;
	return False;
}
#endif /* HAVE_KERB4 */

#ifdef HAVE_KRB5
/*-
 * Pretty much all of this was snatched out of the kinit from the Kerberos5b6
 * distribution.  The reason why I felt it was necessary to use kinit was
 * that if someone is locked for a long time, their credentials could expire,
 * so xlock must be able to get a new ticket.  -- dah <rodmur@ecst.csuchico.edu>
 */
#define KRB5_DEFAULT_OPTIONS 0
#define KRB5_DEFAULT_LIFE 60*60*10	/* 10 hours */
static int
krb_check_password(struct passwd *pwd, char *pass)
{
	krb5_context kcontext;
	krb5_timestamp now;
	krb5_ccache ccache = NULL;
	krb5_principal me;
	krb5_principal server;
	int         options = KRB5_DEFAULT_OPTIONS;
	krb5_deltat lifetime = KRB5_DEFAULT_LIFE;
	krb5_error_code code;
	krb5_creds  my_creds;
	char       *client_name;
	krb5_address **addrs = (krb5_address **) 0;
	krb5_preauthtype *preauth = NULL;

	krb5_data   tgtname =
	{
		0,
		KRB5_TGS_NAME_SIZE,
		KRB5_TGS_NAME
	};

	krb5_init_context(&kcontext);
	krb5_init_ets(kcontext);

	if ((code = krb5_timeofday(kcontext, &now))) {
		com_err(ProgramName, code, "while getting time of day");
		return False;	/* seems better to deny access, than just exit, which
				   was what happened in kinit  */
	}
	if ((code = krb5_cc_default(kcontext, &ccache))) {
		com_err(ProgramName, code, "while getting default ccache");
		return False;
	}
	code = krb5_cc_get_principal(kcontext, ccache, &me);
	if (code) {
		if ((code = krb5_parse_name(kcontext, pwd->pw_name, &me))) {
			com_err(ProgramName, code, "when parsing name %s", pwd->pw_name);
			return False;
		}
	}
	if ((code = krb5_unparse_name(kcontext, me, &client_name))) {
		com_err(ProgramName, code, "when unparsing name");
		return False;
	}
	(void) memset((char *) &my_creds, 0, sizeof (my_creds));

	my_creds.client = me;

	if ((code = krb5_build_principal_ext(kcontext, &server,
				      krb5_princ_realm(kcontext, me)->length,
					krb5_princ_realm(kcontext, me)->data,
					     tgtname.length, tgtname.data,
				      krb5_princ_realm(kcontext, me)->length,
					krb5_princ_realm(kcontext, me)->data,
					     0))) {
		com_err(ProgramName, code, "while building server name");
		return False;
	}
	my_creds.server = server;

	my_creds.times.starttime = 0;
	my_creds.times.endtime = now + lifetime;
	my_creds.times.renew_till = 0;

	if (strlen(pass) == 0)
		(void) strcpy(pass, "*");
	/* if pass is NULL, krb5_get_in_tkt_with_password will prompt with
	   krb5_default_pwd_prompt1 for password, you don't want that in
	   this application, most likely the user won't have '*' for a
	   password -- dah <rodmur@ecst.csuchico.edu> */

	code = krb5_get_in_tkt_with_password(kcontext, options, addrs,
					     NULL, preauth, pass, 0,
					     &my_creds, 0);

	(void) memset(pass, 0, sizeof (pass));

	if (code) {
		if (code == KRB5KRB_AP_ERR_BAD_INTEGRITY)
			return False;	/* bad password entered */
		else {
			com_err(ProgramName, code, "while getting initial credentials");
			return False;
		}
	}
	code = krb5_cc_initialize(kcontext, ccache, me);
	if (code != 0) {
		com_err(ProgramName, code, "when initializing cache");
		return False;
	}
	code = krb5_cc_store_cred(kcontext, ccache, &my_creds);
	if (code) {
		com_err(ProgramName, code, "while storing credentials");
		return False;
	}
	krb5_free_principal(kcontext, server);

	krb5_free_context(kcontext);

	return True;		/* success */
}
#endif /* HAVE_KRB5 */

#ifndef VMS
#undef passwd
#undef pw_name
#undef pw_passwd
#ifndef SUNOS_ADJUNCT_PASSWD
#include <pwd.h>
#endif
#endif

#if ( HAVE_FCNTL_H && (defined( USE_MULTIPLE_ROOT ) || defined( USE_MULTIPLE_USER )))

void
get_multiple(struct passwd *pw)
{
	/* This should be the first element on the linked list.
	 * If not, then there could be problems.
	 * Also all memory allocations tend to force an exit of
	 * the program.  This should probably be changed somehow.
	 */
	if (pwllh == (pwlptr) NULL) {
		if ((pwll = new_pwlnode()) == (pwlptr) ENOMEM) {
			perror("new");
			exit(1);
		}
		pwllh = pwll;
	}
	if ((pwll->pw_name = (char *) strdup(pw->pw_name)) == NULL) {
		perror("new");
		exit(1);
	}
#ifdef        BSD_AUTH
	pwll->pw_lc = login_getclass(pw->pw_class);
#else
	if ((pwll->pw_passwd = (char *) strdup(pw->pw_passwd)) == NULL) {
		perror("new");
		exit(1);
	}
#endif
	if ((pwll->next = new_pwlnode()) == (pwlptr) ENOMEM) {
		perror("new");
		exit(1);
	}
}

void
set_multiple(int uid)
{
#ifdef BSD_AUTH
	struct passwd *pw;
	pwlptr      pwll;

	if (pwllh == (pwlptr) NULL) {
		if ((pwll = new_pwlnode()) == (pwlptr) ENOMEM) {
			perror("new");
			exit(1);
		}
		pwllh = pwll;
	}
	for (pwll = pwllh; pwll->next; pwll = pwll->next);

	while ((pw = getpwent()) != (struct passwd *) NULL) {
		if (pw->pw_uid != uid)
			continue;
		if ((pwll->pw_name = (char *) strdup(pw->pw_name)) == NULL) {
			perror("new");
			exit(1);
		}
		pwll->pw_lc = login_getclass(pw->pw_class);

		if ((pwll->next = new_pwlnode()) == (pwlptr) ENOMEM) {
			perror("new");
			exit(1);
		}
	}

	if (pwll->next = new_pwlnode())
		pwll = pwll->next;
#else /* !BSD_AUTH */
	/* If you thought the above was sick, then you will think this is
	 * downright horrific.  This is set up so that a child process
	 * is created to read in the password entries using getpwent(3C).
	 * In the man pages on the HPs, getpwent(3C) has in it the fact
	 * that once getpwent(3C) has opened the password file, it keeps
	 * it open until the process is finished.  Thus, the child
	 * process exits immediately after reading the entire password
	 * file.  Otherwise, the password file remains open the entire
	 * time this program is running.
	 *
	 * I went with getpwent(3C) because it will actually read in
	 * the password entries from the NIS maps as well.
	 */
	struct passwd *pw;
	int         pipefd[2];
	char        buf[BUFMAX], xlen;
	pid_t       cid;

#ifdef HAVE_SHADOW
	struct spwd *spw;

#endif

	if (pipe(pipefd) < 0) {
		perror("Pipe Generation");
		exit(1);
	}
	if ((cid = fork()) < 0) {
		perror("fork");
		exit(1);
	} else if (cid == 0) {
		/* child process.  Used to read in password file.  Also checks to
		 * see if the uid is one we want.  If so, then writes that to the pipe.
		 */
		register int sbuf = 0;
		char       *cbuf, *pbuf;

		(void) close(pipefd[0]);
		while ((pw = getpwent()) != (struct passwd *) NULL) {
			if (pw->pw_uid != uid)
				continue;
#ifdef HAVE_SHADOW
			if ((spw = getspnam(pw->pw_name)) != NULL) {
				char       *tmp;	/* swap */

				tmp = pw->pw_passwd;
				pw->pw_passwd = spw->sp_pwdp;
				spw->sp_pwdp = tmp;
			}
#endif
			if (pw->pw_passwd[0] != '*') {
				xlen = strlen(pw->pw_name);
				if ((sbuf + xlen) >= BUFMAX) {
					if (write(pipefd[1], buf, sbuf) != sbuf)
						perror("write");
					sbuf = 0;
				}
				cbuf = &buf[sbuf];
				*cbuf++ = xlen;
				for (pbuf = pw->pw_name; *pbuf;)
					*cbuf++ = *pbuf++;
				sbuf += xlen + 1;

				xlen = strlen(pw->pw_passwd);
				if ((sbuf + xlen) >= BUFMAX) {
					if (write(pipefd[1], buf, sbuf) != sbuf)
						perror("write");
					sbuf = 0;
				}
				cbuf = &buf[sbuf];
				*cbuf++ = xlen;
				for (pbuf = pw->pw_passwd; *pbuf;)
					*cbuf++ = *pbuf++;
				sbuf += xlen + 1;
			}
		}
#ifdef HAVE_SHADOW
		endspent();
#endif
		cbuf = &buf[sbuf];
		*cbuf = -1;
		sbuf++;
		if (write(pipefd[1], buf, sbuf) != sbuf)
			perror("write");

		(void) close(pipefd[1]);
		exit(0);
	} else {
		/* parent process.  Does the actual creation of the linked list.
		 * It assumes that everything coming through the pipe are password
		 * entries that are authorized to unlock the screen.
		 */
		register int bufsize = BUFMAX, done = 0, sbuf = BUFMAX,
		            i;
		char       *cbuf, *pbuf;
		pwlptr      pwll;

		(void) close(pipefd[1]);

		if (pwllh == (pwlptr) NULL) {
			if ((pwll = new_pwlnode()) == (pwlptr) ENOMEM) {
				perror("new");
				exit(1);
			}
			pwllh = pwll;
		}
		for (pwll = pwllh; pwll->next; pwll = pwll->next);
		while (!done) {
			if (sbuf >= bufsize) {
				if ((bufsize = read(pipefd[0], buf, BUFMAX)) <= 0)
					perror("read");
				sbuf = 0;
			}
			cbuf = &buf[sbuf];
			xlen = *cbuf++;
			if (xlen < 0) {
				done = 1;
				break;
			}
			sbuf++;
			if (sbuf >= bufsize) {
				if ((bufsize = read(pipefd[0], buf, BUFMAX)) <= 0)
					perror("read");
				sbuf = 0;
			}
			if ((pwll->pw_name = (char *) malloc(xlen + 1)) == NULL)
				break;
			pbuf = pwll->pw_name;
			cbuf = &buf[sbuf];
			for (i = 0; i < xlen; i++) {
				*pbuf++ = *cbuf++;
				sbuf++;
				if (sbuf >= bufsize) {
					if ((bufsize = read(pipefd[0], buf, BUFMAX)) <= 0)
						perror("read");
					sbuf = 0;
					cbuf = buf;
				}
			}
			*pbuf = (char) NULL;

			cbuf = &buf[sbuf];
			xlen = *cbuf++;
			sbuf++;
			if (sbuf >= bufsize) {
				if ((bufsize = read(pipefd[0], buf, BUFMAX)) <= 0)
					perror("read");
				sbuf = 0;
			}
			if ((pwll->pw_passwd = (char *) malloc(xlen + 1)) == NULL)
				break;
			pbuf = pwll->pw_passwd;
			cbuf = &buf[sbuf];
			for (i = 0; i < xlen; i++) {
				*pbuf++ = *cbuf++;
				sbuf++;
				if (sbuf >= bufsize) {
					if ((bufsize = read(pipefd[0], buf, BUFMAX)) <= 0)
						perror("read");
					sbuf = 0;
					cbuf = buf;
				}
			}
			*pbuf = (char) NULL;

			if ((pwll->next = new_pwlnode()) == (pwlptr) ENOMEM)
				break;
			pwll = pwll->next;
		}
		(void) close(pipefd[0]);
	}
#endif /* !BSD_AUTH */
}

#endif

void
initPasswd(void)
{
	getUserName();
#if !defined( ultrix ) && !defined( DCE_PASSWD ) && !defined( PAM )
	if (!nolock && !inroot && !inwindow && grabmouse) {
#ifdef BSD_AUTH
		struct passwd *pwd = getpwnam(user);

		lc = login_getclass(pwd->pw_class);
		if (allowroot && (pwd = getpwnam(ROOT)) != NULL)
			rlc = login_getclass(pwd->pw_class);
#else /* !BSD_AUTH */
#ifdef USE_XLOCKRC
		gpass();
#else
#ifdef FALLBACK_XLOCKRC
		if (!cpasswd || !*cpasswd)
			getCryptedUserPasswd();
		else
			gpass();
#else
		getCryptedUserPasswd();
#endif
#endif
		if (allowroot)
			getCryptedRootPasswd();
#endif /* !BSD_AUTH */
	}
#endif /* !ultrix && !DCE_PASSWD && !PAM */
#ifdef DCE_PASSWD
	initDCE();
#endif
}
