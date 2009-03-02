/*
 * Resource messages for English
 */

#ifndef __RESOURCE_MSG_EN__
#define __RESOURCE_MSG_EN__

#define DEF_NAME		"Name: "
#define DEF_PASS		"Password: "
#define DEF_VALID		"Validating login..."
#define DEF_INVALID		"Invalid login."
#define DEF_INVALIDCAPSLOCK	"Invalid login, Caps Lock on."
#define DEF_INFO		"Enter password to unlock; select icon to lock."

#ifdef HAVE_KRB5
#define DEF_KRBINFO		"Enter Kerberos password to unlock; select icon to lock."
#endif /* HAVE_KRB5 */

#define DEF_COUNT_FAILED	" failed attempt."
#define DEF_COUNTS_FAILED	" failed attempts."

/* string that appears in logout button */
#define DEF_BTN_LABEL		"Logout"

/* this string appears immediately below logout button */
#define DEF_BTN_HELP		"Click here to logout"

/*
 * this string appears in place of the logout button
 * if user could not be logged out
 */
#define DEF_FAIL		"Auto-logout failed"

#endif /* __RESOURCE_MSG_EN__ */
