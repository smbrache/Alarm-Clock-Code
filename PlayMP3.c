/*
 * PlayMP3.c
 *
 *  Created on: May 12, 2016
 *      Author: Michael McGuire
 */

#include "PlayMP3.h"
#include "stm32f4xx_hal_dma.h"
#include "mp3dec.h"
#include "mono.h"
#include "main.h"


//
// Global variables for handling MP3 processing.
//
FIL
	file;

int16_t
	audio_read_buffer[MAX_FRAME * NUMBER_BUFFER];

volatile uint
	CurrentBuffer = 0;

volatile bool
	ReadNextBuffer = false;

volatile uint
	buffer_read,
	buffer_write;




/* PlayDirectory(char *drive)
 * Plays all MP3 files in the root directory of the specified drive.
 */

int PlayDirectory(const char* path, unsigned char seek)
{
	FRESULT
		Result;

	FILINFO
		fno;

	DIR
		dir;

	char
		*fn, 			// This function is assuming non-Unicode cfg.
		buffer[200];

	int					// Number of which mp3 is being played
		NowPlaying;

//
// Long file name variables
//
#if _USE_LFN
	static char
		lfn[_MAX_LFN + 1];

	fno.lfname = lfn;
	fno.lfsize = sizeof(lfn);
#endif

	NowPlaying = 0;

	Result = f_opendir(&dir, path); /* Open the directory */
	if (FR_OK == Result )
	{
		while ( TRUE == PlayMusic )
		{
			Result = f_readdir(&dir, &fno); 	// Read a directory item

			if (( FR_OK != Result ) || ( 0 == fno.fname[0] ))
			{
				break;			// Break on error or end of directory
			}

			if ( '.' == fno.fname[0] )
			{
				continue; 		// Ignore dot entry
			}

#if _USE_LFN
			fn = *fno.lfname ? fno.lfname : fno.fname;
#else
			fn = fno.fname;
#endif

//
// Make sure that it is a file and not a directory
//
			if ( !(fno.fattrib & AM_DIR) )
			{
//
// Is there a path then add it to the file name
//
				if ( strnlen( path, sizeof( path )) != 0 )
				{
					sprintf( buffer, "%s/%s", path, fn);
				}
//
// No path
//
				else
				{
					sprintf( buffer, "%s", fn );
				}

//
// Check for mp3 file extension
//
				if (strcasecmp("MP3", get_filename_ext(buffer)) == 0)
				{

//
// Skip "seek" number of mp3 files...
//

					if ( 0 != seek )
					{
						seek--;
						NowPlaying++;
						continue;
					}

					PlayMP3File(buffer);
					NowPlaying++;
				}
			}
		}
	}

	return( NowPlaying );
}

volatile uint32_t
	BlocksLeft;


void PlayMP3File(const char *fname)
{

	FRESULT
		res;

	uint
		read_samples;

	HAL_StatusTypeDef
		hresult;

	char
		szArtist[120],
		szTitle[120];

	int16_t
		*SoundPointer;

	int16_t
		AnalogToMonoStatus;

	extern HMP3Decoder
		hMP3Decoder;

	MP3FrameInfo
		MP3FrameData;


//
// Display the current file being played.
//
	trace_printf("Playing File: %s\n",fname);

//
// Open the file
//
	if ( (res =  f_open(&file, fname, FA_OPEN_EXISTING | FA_READ)) == FR_OK )
	{

//
// Read ID3v2 Tag
//

		Mp3ReadId3V2Tag(&file, szArtist, sizeof(szArtist), szTitle, sizeof(szTitle));

//
// Display the title and artist of the track.
//
		trace_printf("Title: %s\n",szTitle);
		trace_printf("Artist: %s\n",szArtist);

//
// Set the initial buffer pointer
//

		CurrentBuffer = 0;

//
// *** Important ***
//
// Do not use trace_printf with semihosting as the song is being played as it will cause DMA transfers to stop as
//	the trace_printf text is being transfered to the host computer
//

//
// Fill the first buffer
//
		SoundPointer = audio_read_buffer + CurrentBuffer * MAX_FRAME;
		read_samples = fill_mp3_buffer(&file, (uint16_t *)SoundPointer, MAX_FRAME, true );

		MP3GetLastFrameInfo( hMP3Decoder, &MP3FrameData );
		AudioToMono( SoundPointer, read_samples, ANALOG_TO_MONO_AVERAGE_CHANNELS, MP3FrameData.nChans, &AnalogToMonoStatus );

//
// Display information about the audio track
//
		trace_printf( "Bit rate: %d\n", MP3FrameData.bitrate );
		trace_printf( "Number of channels: %d\n", MP3FrameData.nChans );
		trace_printf( "Sample Rate: %d\n", MP3FrameData.samprate );
		trace_printf( "Bits per Sample: %d\n", MP3FrameData.bitsPerSample );
		trace_printf( "Layer: %d\n", MP3FrameData.layer );
		trace_printf( "Version: %d\n\n", MP3FrameData.version );

//
// Start the DMA transfer.
//
		hresult = HAL_DAC_Start_DMA( &AudioDac , DAC_CHANNEL_1, (uint32_t *) SoundPointer, MAX_FRAME >> 1, DAC_ALIGN_12B_L );


		if ( hresult != HAL_OK ) {
			return;
		}

//
// Current buffer being transfered start filling the next buffer
//

		ReadNextBuffer = TRUE;

//
// Read in the rest of the file.
//
		while ( read_samples == MAX_FRAME && res == FR_OK )
		{

//
// Only fill buffers that are not being read from.
//
			if ( TRUE == ReadNextBuffer )
			{
				ReadNextBuffer = FALSE;
				BlocksLeft = 1;
				CurrentBuffer = ( CurrentBuffer + 1 ) % NUMBER_BUFFER;

				SoundPointer = audio_read_buffer + CurrentBuffer * MAX_FRAME;
				read_samples = fill_mp3_buffer(&file, (uint16_t *)SoundPointer, MAX_FRAME, false);
				MP3GetLastFrameInfo( hMP3Decoder, &MP3FrameData );

				if ( FALSE == PlayMusic )
				{
					break;
				}

				if ( FALSE == AudioToMono( SoundPointer, read_samples, ANALOG_TO_MONO_AVERAGE_CHANNELS, MP3FrameData.nChans, &AnalogToMonoStatus ))
				{
					trace_printf( "Analog to mono error %d\n", AnalogToMonoStatus );
				}
			}
		}

//
// Wait for the last music block to be transfered
//
		while ( 0 != BlocksLeft )
		{
//
// Wait for interrupt to occur
//
			__asm__ volatile ( "wfi" );

		}



		if ( ( res = f_close(&file) ) != FR_OK )
		{
			//trace_printf("Bad close.\n");
		}
	}

}

const char *get_filename_ext(const char *filename)
{
    const char
		*dot;

    dot = strrchr(filename, '.');

    if( (!dot) || (dot == filename))
    {
    	return("");
    }

    return( dot + 1 );
}

/*
 * HAL_DAC_ConvCpltCallbackCh1
 * Interrupt called when we have emptied the buffer
 */

void HAL_DAC_ConvCpltCallbackCh1 (DAC_HandleTypeDef * hdaci)
{
	HAL_StatusTypeDef hresult;

	if ( 0 != BlocksLeft )
	{
		hresult = HAL_DAC_Start_DMA( &AudioDac , DAC_CHANNEL_1, (uint32_t *) (audio_read_buffer + CurrentBuffer * MAX_FRAME ),
				MAX_FRAME >> 1, DAC_ALIGN_12B_L);

		ReadNextBuffer = TRUE;
		BlocksLeft--;
	}

}
void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *hdaci)
{
//	static unsigned int Status1, Status2;
//
//	Status1 = DMA1->LISR;
//	Status2 = DMA2->LISR;

	int i;
	if ( hdaci == &AudioDac ) {
		for (i=0;i<10;i++);
	}
}

void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef * hdaci) {
	int i;
	if ( hdaci == &AudioDac ) {
		for (i=0;i<10;i++);
	}
}
