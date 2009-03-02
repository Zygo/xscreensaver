#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)sound.c	4.00 97/01/01 xlockmore";

#endif

/*-
 * sound.c - xlock.c and vms_play.c
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 26-Oct-01: Allow wild cards on sound-files. (joukj@hrem.stm.tudelft.nl)
 */

#include "xlock.h"
#include "iostuff.h"

#ifdef USE_RPLAY
#include <rplay.h>

static void
play_sound(char *filename, Bool verbose)
{
	int         rplay_fd = rplay_open_default();

	if (rplay_fd >= 0) {
		rplay_sound(rplay_fd, filename);
		rplay_close(rplay_fd);
	}
}
#endif

#ifdef USE_NAS
/* Gives me lots of errors when I compile nas-1.2p5  -- xlock maintainer */

/*-
 * Connect each time, because it might be that the server was not running
 * when xlock first started, but is when next nas_play is called
 */

#include <audio/audio.h>
#include <audio/audiolib.h>

extern Display *dsp;

static void
play_sound(char *filename, Bool verbose)
{
	char       *auservername = DisplayString(dsp);
	char       *fname = filename;
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

#ifdef USE_XAUDIO
  /* Anybody ever get this working? XAudio.007 */
#endif

#ifdef HAS_MMOV
#include <prvdef.h>
#include <ssdef.h>

extern void PLAY_SOUND_MMOV(char* filename, Bool verbose);
#endif

#ifdef USE_VMSPLAY
/*-
 * Jouk Jansen <joukj@hrem.stm.tudelft.nl> contributed this
 * which he found at http://axp616.gsi.de:8080/www/vms/mzsw.html
 *
 * quick hack for sounds in xlockmore on VMS
 * with a the above AUDIO package slightly modified
 */
#include <file.h>
#include <unixio.h>
#include <iodef.h>
#include "vms_amd.h"

static int
play_sound_so(char *filename, Bool verbose)
{
	int         i, j, status;
	char        buffer[2048];
	int         volume = 65;	/* volume is default to 65% */

	/* use the internal speaker(s) */
	int         speaker = SO_INTERNAL /*SO_EXTERNAL */ ;
	int         fp;

	status = AmdInitialize("SO:", volume);	/* Initialize access to AMD */
        if ( status !=0 ) return status;

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
   return 0;
}

#endif

#if defined( HAS_MMOV ) || defined( USE_VMSPLAY )
static void
play_sound(char* filename, Bool verbose)
{
#ifdef USE_VMSPLAY
   if (play_sound_so(filename, verbose) != 0 )
#endif
     {
#ifdef HAS_MMOV
	int pr_status , privs[2] , SYS$SETPRV();

	privs[1] = 0;
	privs[0] = PRV$M_SYSNAM;
	pr_status = SYS$SETPRV ( 1, privs, 0, 0 );

	if ( pr_status == SS$_NORMAL ) PLAY_SOUND_MMOV(filename, verbose);
#endif
     }
}
#endif

#ifdef DEF_PLAY
static void
play_sound(char *filename, Bool verbose)
{
	char        *progrun = NULL;

	if ((progrun = (char *) malloc(strlen(DEF_PLAY) + strlen(filename) + 11)) != NULL) {
		(void) sprintf(progrun, "( %s %s ) 2>&1", DEF_PLAY, filename);
		/*(void) printf("%s\n", progrun); */
		(void) system(progrun);
		free(progrun);
	}
}

#endif

#ifdef USE_ESOUND

#ifndef DEFAULT_SOUND_DIR
#if 1
#define DEFAULT_SOUND_DIR "/usr/lib/X11/xlock/sounds/"
#else
#define DEFAULT_SOUND_DIR "@PREFIX@/lib/X11/xlock/sounds/"
#endif
#endif

#ifdef HAVE_LIBESD

#include <sys/stat.h>
#include <esd.h>

#else

#error Sorry, but you cannot use ESD without ESD !!!?

#endif

typedef struct _esd_sample
{
    char               *file;
    int                 rate;
    int                 format;
    int                 samples;
    unsigned char      *data;
    int                 id;
    struct _esd_sample *next;
} EsdSample_t;

typedef EsdSample_t    *EsdSample_ptr;

static EsdSample_ptr 	EsdSamplesList =(EsdSample_ptr)NULL;
static int	     	sound_fd = -1;

static EsdSample_ptr   	sound_esd_load_sample(char *file);
static void 	     	sound_esd_play(EsdSample_ptr s);
static void	     	sound_esd_destroy_sample(EsdSample_ptr s);
static int	     	sound_esd_init(void);
static void	     	sound_esd_shutdown(void);


/*
 * Public ESOUND sound functions
 * =============================
 */

static void
play_sound(char *filename, Bool verbose)
{
#ifdef DEBUG
    (void) fprintf( stderr, "play_sound %s\n", filename );
#endif
    if ( filename && *filename )
      sound_esd_play( sound_esd_load_sample( filename ) );
}

int init_sound(void)
{
    return( sound_esd_init() );
}

void shutdown_sound(void)
{
    sound_esd_shutdown();
}

/* Found this snippet from modes/glx/xpm-image.c by way of
   Jamie Zawinski <jwz@jwz.org> Copyright (c) 1998 */
static Bool
bigendian (void)
{
  union { int i; char c[sizeof(int)]; } u;
  u.i = 1;
  return !u.c[0];
}


/*
 * Private ESOUND sound functions
 * ==============================
 */

static EsdSample_ptr
sound_esd_load_sample(char *file)
{
   AFfilehandle        in_file;
   struct stat	       stbuf;
   EsdSample_ptr       s;
   int                 in_format, in_width, in_channels, frame_count;
   int                 bytes_per_frame, frames_read;
   double              in_rate;
   char 	      *origfile = strdup( file ? file : "" );
   char 	       fullfile[MAXPATHLEN];

#ifdef DEBUG
   (void) fprintf(stderr, "sound_esd_load_sample: %s\n", origfile);
#endif

   s = EsdSamplesList;
   while ( s && strcmp( file, s->file ) )
     s = s->next;
   if ( s && !strcmp( file, s->file ) )
     return s;

#ifdef DEBUG
   (void) fprintf(stderr, "sound_esd_load_sample: sample not loaded: loading ...\n", origfile);
#endif

   if (file[0] != '/')
   {
       (void) sprintf( fullfile, "%s/%s", DEFAULT_SOUND_DIR, file );
       file = fullfile;
   }
   if (stat(file, &stbuf) < 0)
   {
       (void) fprintf( stderr, "Error ! Cannot find the sound file %s\n", file);
       return NULL;
   }
   if ( !( in_file = afOpenFile(file, "r", NULL) ) )
   {
       (void) fprintf( stderr, "Error ! Cannot open sound sample ! Bad format ?\n" );
       return(NULL);
   }
   s = EsdSamplesList;
   if ( s )
   {
       while ( s && s->next )
         s = s->next;
       s->next = (EsdSample_t *) malloc(sizeof(EsdSample_t));
       if ( !s->next )
       {
           (void) fprintf( stderr, "Error ! cannot allocate sample data !\n" );
           afCloseFile(in_file);
           return NULL;
       }
       s = s->next;
       s->next = NULL;
   }
   else
   {
       s = (EsdSample_t *) malloc(sizeof(EsdSample_t));
       if ( !s )
       {
           (void) fprintf( stderr, "Error ! cannot allocate sample data !\n" );
           afCloseFile(in_file);
           return NULL;
       }
       EsdSamplesList = s;
       s->next = NULL;
   }

   frame_count = afGetFrameCount(in_file, AF_DEFAULT_TRACK);
   in_channels = afGetChannels(in_file, AF_DEFAULT_TRACK);
   in_rate = afGetRate(in_file, AF_DEFAULT_TRACK);
   afGetSampleFormat(in_file, AF_DEFAULT_TRACK, &in_format, &in_width);
   afSetVirtualByteOrder(in_file, AF_DEFAULT_TRACK, (bigendian()) ?
	AF_BYTEORDER_BIGENDIAN : AF_BYTEORDER_LITTLEENDIAN);
   s->file = strdup(origfile);
   s->rate = 44100;
   s->format = ESD_STREAM | ESD_PLAY;
   s->samples = 0;
   s->data = NULL;
   s->id = 0;

   if (in_width == 8)
      s->format |= ESD_BITS8;
   else if (in_width == 16)
      s->format |= ESD_BITS16;
   bytes_per_frame = (in_width * in_channels) / 8;
   if (in_channels == 1)
      s->format |= ESD_MONO;
   else if (in_channels == 2)
      s->format |= ESD_STEREO;
   s->rate = (int)in_rate;

   s->samples = frame_count * bytes_per_frame;
   s->data = (unsigned char *) malloc(frame_count * bytes_per_frame);
   if ( !s->data )
   {
       (void) fprintf( stderr, "Error ! cannot allocate memory for sample !\n" );
       afCloseFile(in_file);
       return NULL;
   }
   frames_read = afReadFrames(in_file, AF_DEFAULT_TRACK, s->data, frame_count);
   afCloseFile(in_file);
   return s;
}

static
void
sound_esd_play(EsdSample_ptr s)
{
   int                 size, confirm = 0;

#ifdef DEBUG
   (void) fprintf( stderr, "sound_esd_play\n" );
#endif

   if ((sound_fd < 0) || (!s))
     return;
   if (!s->id)
     {
	if (sound_fd >= 0)
	  {
	     if (s->data)
	       {
		  size = s->samples;
		  s->id = esd_sample_getid(sound_fd, s->file);
		  if (s->id < 0)
		    {
		       s->id = esd_sample_cache(sound_fd, s->format, s->rate, size, s->file);
		       write(sound_fd, s->data, size);
		       confirm = esd_confirm_sample_cache(sound_fd);
		       if (confirm != s->id)
                         s->id = 0;
		    }
		  free(s->data);
		  s->data = NULL;
	       }
	  }
     }
   if (s->id > 0)
     esd_sample_play(sound_fd, s->id);
   return;
}

static void
sound_esd_destroy_sample(EsdSample_ptr s)
{
#ifdef DEBUG
    (void) fprintf( stderr, "sound_esd_destroy_sample\n" );
#endif

    if ((s->id) && (sound_fd >= 0))
      esd_sample_free(sound_fd, s->id);

    if (s->data)
      free(s->data);
    if (s->file)
      free(s->file);
}

static int
sound_esd_init(void)
{
    int                 fd;

#ifdef DEBUG
    (void) fprintf(stderr, "sound_esd_init\n");
#endif
    if (sound_fd != -1)
      return 0;
    fd = esd_open_sound(NULL);
    if (fd >= 0)
      sound_fd = fd;
    else
    {
	(void) fprintf(stderr, "Error initialising sound\n");
        return -1;
     }
    return 0;
}

static void
sound_esd_shutdown(void)
{
#ifdef DEBUG
    (void) fprintf( stderr, "sound_esd_shutdown\n" );
#endif
    if (sound_fd >= 0)
    {
        EsdSample_ptr	s = EsdSamplesList;

        while ( s )
        {
            EsdSamplesList = s->next;
            sound_esd_destroy_sample( s );
            free( s );
            s = EsdSamplesList;
        }
	close(sound_fd);
	sound_fd = -1;
    }
}

#endif

#ifdef USE_SOUND
void
playSound(char *filename, Bool verbose)
{
	char* actual_sound;

	actual_sound = getSound(filename);
	if (verbose && strcmp(actual_sound, filename) != 0) {
		(void) printf("Requested sound file : %s\n", filename);
		(void) printf("Selected sound file : %s\n" , actual_sound);
	}
	if (actual_sound)
		play_sound(actual_sound, verbose);
}
#endif
