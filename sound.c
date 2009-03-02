
#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)sound.c	4.00 97/01/01 xlockmore";

#endif

/*-
 * sound.c - xlock.c and vms_play.c
 *
 * See xlock.c for copying information.
 *
 */

#include "xlock.h"

#ifdef USE_RPLAY
#include <rplay.h>

void
play_sound(char *string)
{
	int         rplay_fd = rplay_open_default();

	if (rplay_fd >= 0) {
		rplay_sound(rplay_fd, string);
		rplay_close(rplay_fd);
	}
}
#endif

#ifdef USE_NAS
/*-
 * I connect each time, cos it might be that the server wasn't running
 * when xlock first started, but is when next nas_play is called
 */

#include <audio/audio.h>
#include <audio/audiolib.h>

extern Display *dsp;

void
play_sound(char *string)
{
	char       *auservername = DisplayString(dsp);
	char       *fname = string;
	AuServer   *aud;	/* audio server connection */

	if (!(aud = AuOpenServer(auservername, 0, NULL, 0, NULL, NULL)))
		return;		/*cannot connect - no server? */
	/* 
	 * now play the file at recorded volume (3rd arg is a percentage), 
	 * synchronously
	 */
	AuSoundPlaySynchronousFromFile(aud, fname, 100);
	AuCloseServer(aud);
}
#endif

#ifdef USE_VMSPLAY
/*-
 * Jouk Jansen <joukj@crys.chem.uva.nl> contributed this
 * which he found at http://axp616.gsi.de:8080/www/vms/mzsw.html
 *
 * quick hack for sounds in xlockmore on VMS
 * with a the above AUDIO package slightly modified
 */
#include <file.h>
#include <unixio.h>
#include <iodef.h>
#include "amd.h"

void
play_sound(char *filename)
{
	int         i, j, status;
	char        buffer[2048];
	int         volume = 65;	/* volume is default to 65% */

	/* use the internal speaker(s) */
	int         speaker = SO_INTERNAL /*SO_EXTERNAL */ ;
	int         fp;

	status = AmdInitialize("SO:", volume);	/* Initialize access to AMD */
	AmdSelect(speaker);	/* Select which speaker */
	fp = open(filename, O_RDONLY, 0777);	/* Open the file */
	if (!(fp == -1)) {
		/* Read through it */
		i = read(fp, buffer, 2048);
		while (i) {
			status = AmdWrite(buffer, i);
			if (!(status & 1))
				exit(status);
			i = read(fp, buffer, 1024);
		}
	}
	(void) close(fp);
}

#endif

#ifdef DEF_PLAY
void
play_sound(char *string)
{
	char        progrun[BUFSIZ];

	(void) sprintf(progrun, "( %s%s ) 2>&1", DEF_PLAY, string);
	/*(void) printf("( %s%s ) 2>&1\n", DEF_PLAY, string); */
	(void) system(progrun);
}

#endif
