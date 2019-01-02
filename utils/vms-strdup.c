/*
 * strdup.c
 *
 * Simple version of strdup for machines without it (ie DEC Ultrix 4.2)
 * Apparently VMS only got strdup in 1995 (v5.2...)
 *
 * By David Chatterton
 * 29 July 1993
 *
 * You can do anything you like to this... :)
 * I've stolen it from xpilot and added it to the xvmstuils MPJZ ;-)
 */

#if (__VMS_VER < 70000000)
#include <stdlib.h>
#include <string.h>

char* strdup (const char* s1)
{
	char* s2;
	if (s2 = (char*)malloc(strlen(s1)+1))
		strcpy(s2,s1);
	return s2;
}
#endif
