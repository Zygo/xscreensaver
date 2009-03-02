#include <stdio.h>
#include <signal.h>
#include <mme/mme_api.h>
#include "mmc_buffers.h"
#include "mmc_cmdlin.h"
#include "mmc_memory.h"
#include "mmc_wave_format.h"
#include "mmc_wave_file.h"
#include "mmc_wave.h"

#define BUFFER_PAD 4

#define DEFAULT_NCHANNELS      	1
#define DEFAULT_ENCODING	WAVE_FORMAT_MULAW
#define DEFAULT_SAMPLESIZE     	8
#define DEFAULT_SAMPLERATE	8000

#define DIV_ROUND_UP_(n,d) ((n + d - 1) / d)
#define ROUND_UP_(n,d) (DIV_ROUND_UP_(n,d) * d)

enum mmov_state 
{
  MMOV_START,
  MMOV_PLAY,
  MMOV_WAITING,
  MMOV_CLOSE
};

#define NUM_DATA 32
struct mmov_soundformat
{
   WAVEFORMATEX wave;
   char extra_data[ NUM_DATA ];
};

static int Verbose;
static mmcWaveFileState_t wavefilestatus = mmcWaveFileStateInitialValue;
static mmcBufferList_t mmov_buffer = mmcBufferListInitialValue;
static enum mmov_state play_state;

static void mmov_cleanup( )
{
   if (mmov_buffer.b) 
     {
	mmcBuffersFree (&mmov_buffer);
     }
   mmcFreeAll ();
    {
       mmcWaveInFileClose (&wavefilestatus);
    }
    return;
}

static void mmov_driver (HANDLE hWaveOut,
			     UINT wMsg,
			     DWORD dwInstance,
			     LPARAM lParam1,
			     LPARAM lParam2)
{
    switch (wMsg)
    {
      case WOM_OPEN:
	play_state = MMOV_PLAY;
	break;
      case WOM_CLOSE:
	play_state = MMOV_CLOSE;
	break;
	
      case WOM_DONE:
      {
	  int buffer_index = mmcWaveOutGotData (&mmov_buffer, lParam1);
	  mmcBuffer_p bp = &mmov_buffer.b[buffer_index];
	  mmcBufferSetStatus (&mmov_buffer, buffer_index, Empty,
			      mmov_buffer.nbytes);
      }
	break;
      default:
	mmcVerboseDisplay(Verbose, "Unknown index %d", wMsg);
	break;
    }
}

void play_sound_mmov( char* FileName , int verbose )
{
    HWAVEOUT hwaveout;
    MMRESULT status;
    int mask1;
    int allDone;

    static int uDeviceId = WAVE_MAPPER;
    static int AdpcmBitsPerSample = 16; 
    static int AdpcmSamplesPerBlock = 0;
    static int sizeBuffers = 0;
    static int msBuffers = 0;
    static int numBuffers = 4;
    static struct mmov_soundformat sound_format =
    {
	{
	    DEFAULT_ENCODING,
	    DEFAULT_NCHANNELS,
	    DEFAULT_SAMPLERATE,
    	    0,
	    0,
	    DEFAULT_SAMPLESIZE,
	    NUM_DATA
	}, {
	    0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0
	}
    };

   Verbose = verbose;
   play_state = MMOV_START;
   
    /* Open the Wave In file */
    if (mmcWaveInFileOpen (FileName, &sound_format.wave, &wavefilestatus) != 0) 
    {
	mmcVerboseDisplay(Verbose,"Error opening input file");
	mmov_cleanup();
       return;
    }

    if ( sound_format.wave.wFormatTag == WAVE_FORMAT_PCM)
      mmcVerboseDisplay(Verbose,"PCM file");
    else if ( sound_format.wave.wFormatTag == WAVE_FORMAT_MULAW)
      mmcVerboseDisplay(Verbose,"mu-law file");
    else if ( sound_format.wave.wFormatTag == WAVE_FORMAT_IMA_ADPCM)
      mmcVerboseDisplay(Verbose,"IMA file");
    else
      mmcVerboseDisplay(Verbose,"Unknown sound format %d", 
				sound_format.wave.wFormatTag);
    mmcVerboseDisplay(Verbose,"Channels = %d ",  sound_format.wave.nChannels );
    mmcVerboseDisplay(Verbose,"Sample rate = %d", 
		      sound_format.wave.nSamplesPerSec);
    mmcVerboseDisplay(Verbose,
		      "Sample size = %d", sound_format.wave.wBitsPerSample );
    
    if( sound_format.wave.nChannels == 0)
      sound_format.wave.nChannels = DEFAULT_NCHANNELS;
    if( sound_format.wave.nSamplesPerSec == 0)
      sound_format.wave.nSamplesPerSec = DEFAULT_SAMPLERATE;
    if( sound_format.wave.wBitsPerSample == 0)
      sound_format.wave.wBitsPerSample = DEFAULT_SAMPLESIZE;
    
    if ( sound_format.wave.wFormatTag == WAVE_FORMAT_IMA_ADPCM ) {
      AdpcmSamplesPerBlock = *(Uint16 *)(&sound_format.extra_data[0]);
    }

    if ( ( sound_format.wave.wFormatTag == WAVE_FORMAT_PCM) ||
	( sound_format.wave.wFormatTag == WAVE_FORMAT_MULAW) ) {
      mask1 = WAVE_FORMAT_FIX_BLOCK_ALIGN | WAVE_FORMAT_FIX_AVG_BPS;
      mmcWaveFormatFix((LPPCMWAVEFORMAT)(&sound_format), mask1 );
    }

    if ( sound_format.wave.wFormatTag == WAVE_FORMAT_IMA_ADPCM) {
      sizeBuffers = sound_format.wave.nBlockAlign;
      msBuffers = sizeBuffers * 1000 /
	  (DIV_ROUND_UP_(AdpcmBitsPerSample,8) * 
	   sound_format.wave.nSamplesPerSec * 
	   sound_format.wave.nChannels);


    } else {
      if (!msBuffers && !sizeBuffers)
	msBuffers = 1000/numBuffers;

      if (msBuffers)
	sizeBuffers = 
	  ROUND_UP_(DIV_ROUND_UP_(msBuffers * 
				DIV_ROUND_UP_( sound_format.wave.wBitsPerSample,
					      8) * 
				sound_format.wave.nSamplesPerSec * 
				sound_format.wave.nChannels,1000), BUFFER_PAD);
      else
	{
	  sizeBuffers = ROUND_UP_(sizeBuffers, BUFFER_PAD);
	  msBuffers = sizeBuffers * 1000 /
	    (DIV_ROUND_UP_( sound_format.wave.wBitsPerSample,8) * 
	     sound_format.wave.nSamplesPerSec * 
	     sound_format.wave.nChannels);
	}
    }


    mmcVerboseDisplay(Verbose, "Buffer size = %d bytes or %d milliseconds",
		      sizeBuffers, msBuffers);

    hwaveout = NULL;

    status = mmcWaveOutOpen(&sound_format.wave, uDeviceId, &mmov_driver ,
			    WAVE_OPEN_SHAREABLE, &hwaveout);

    if (status != MMSYSERR_NOERROR)
     {
	mmov_cleanup();
	return;
     }

   if (mmcBuffersCreate (&mmov_buffer, numBuffers, sizeBuffers, BUFFER_PAD)
	!= MMSYSERR_NOERROR)
     {
	mmov_cleanup();
	return;
     }
      
    while (1)
    {
	switch ( play_state )
	{
	  case MMOV_START:
	    break;

	  case MMOV_PLAY:
	    status = mmcWaveOutQueueBufferAll (hwaveout, &mmov_buffer, Verbose,
					       &wavefilestatus, &allDone);
	    if (allDone) 
	      play_state = MMOV_WAITING;
	    else
	     {
		if (status != MMSYSERR_NOERROR)
		  {
		     mmov_cleanup();
		     return;
		  }
	     }
	    break;

	  case MMOV_WAITING:
	    if ((mmcBufferFind(&mmov_buffer,Filling) != mmcBufferNone) ||
		(mmcBufferFind(&mmov_buffer,Full) != mmcBufferNone) ||
		(mmcBufferFind(&mmov_buffer,Playing) != mmcBufferNone))
	      break;
	    play_state = MMOV_CLOSE;
	    
	  case MMOV_CLOSE:
	   status = mmcWaveOutClose (hwaveout, &mmov_buffer);
	   if (status != MMSYSERR_NOERROR)
		  {
		     mmov_cleanup();
		     return;
		  }
	    mmov_cleanup();
	   return;

	  default:
	    mmcVerboseDisplay(Verbose,"Unknown play_state %d", play_state);
	    mmov_cleanup();
	   return;
	}

	mmeWaitForCallbacks (); /* block so we don't hog 100% of the CPU */
	mmeProcessCallbacks ();
    }
}
