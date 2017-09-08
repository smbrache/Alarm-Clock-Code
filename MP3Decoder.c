/*
 * MP3Decoder.c
 *
 *  Created on: May 12, 2016
 *      Author: Michael
 */

#include "MP3Decoder.h"

MP3FrameInfo			mp3FrameInfo;
HMP3Decoder				hMP3Decoder;

// Useful service routines for handling ID3 tags.
static uint32_t Mp3ReadId3V2Text(FIL* pInFile, uint32_t unDataLen, char* pszBuffer,
		uint32_t unBufferSize);

uint fill_mp3_buffer(FIL *fp, uint16_t * buffer, uint read_length, bool reset_flag) {

	uint   br;
	uint   btr;
	uint   needed_samples = read_length;
	uint   provided_samples = 0;

	static char    	file_read_buffer[FILE_READ_BUFFER_SIZE];
	static uint16_t local_audio[MAX_FRAME];
	static char *	file_read_ptr = file_read_buffer;

	static uint		file_bytes = 0;
	static uint		audio_samples = 0;


	// Copy of above pointers/indicators for roll back if MP3 frame decode failure.
	char *	copy_file_read_ptr;
	uint	copy_file_bytes;
	bool	need_data;
	FRESULT	res = FR_OK;
	int 	offset,err;
	bool 	out_of_data = false;
	int 	copy_samples;
	int 	i;

	// If we starting a file, initialize decoder.
	if ( reset_flag ) {
		file_bytes		= 0;
		file_read_ptr 	= file_read_buffer;
		audio_samples 	= 0;
		hMP3Decoder 	= MP3InitDecoder();
	}

	if ( audio_samples > 0 ) {

		copy_samples = ( audio_samples > needed_samples ) ? needed_samples : audio_samples;
		memcpy( buffer , local_audio , sizeof(int16_t) * copy_samples );
		memmove(local_audio, local_audio + copy_samples, audio_samples - copy_samples);

		audio_samples 		-= copy_samples;
		needed_samples 		-= copy_samples;
		provided_samples	+= copy_samples;

	}

	need_data = ( needed_samples > 0 );

	do {

		if ( need_data ) {

			// Move samples to start of file read buffer
			memmove(file_read_buffer,file_read_ptr,file_bytes);
			file_read_ptr = file_read_buffer;

			// Fill the file read buffer back up
			btr =      FILE_READ_BUFFER_SIZE - file_bytes;
			res =      f_read(fp, file_read_buffer + file_bytes , btr, &br);
			file_bytes += br;

			if ( res != FR_OK ) {
				// File access failure, dump out.
				trace_printf("Failure!\n");
				out_of_data  = true;
				break;
			}

			// Flag when file is completely read complete.
			out_of_data = f_eof(fp);

			// Clear flag for needing more data.
			need_data = false;
		}

		// Find the next frame in the MP3 file
		offset = MP3FindSyncWord((unsigned char*)file_read_ptr, file_bytes);

		if ( offset < 0 ) {
			// This should never happen unless we have a badly formatted frame.
			// Just exit out and hope the next call gets a good frame.
			need_data = true;
			continue;
		}

		copy_file_bytes    = file_bytes - offset;
		copy_file_read_ptr = file_read_ptr + offset;

		// Decode this frame.
		err = MP3Decode(hMP3Decoder, (unsigned char**)&copy_file_read_ptr,
				(uint*)&copy_file_bytes, local_audio, 0);

		if (err) {
			/* error occurred */
			switch (err) {
			case ERR_MP3_INDATA_UNDERFLOW:
				// Next loop will refill buffer.
				need_data = true;
				continue;
			case ERR_MP3_MAINDATA_UNDERFLOW:
				// Next loop will refill buffer.
				need_data = true;
				continue;
			case ERR_MP3_FREE_BITRATE_SYNC:
			default:
				out_of_data = true;
				break;
			}
		} else {

			file_bytes    = copy_file_bytes;
			file_read_ptr = copy_file_read_ptr;

			// no error
			MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);

//			// Check if we need to convert to stereo to mono.
			if (mp3FrameInfo.nChans == 1) {
//
//				for(i = mp3FrameInfo.outputSamps;i >= 0;i--) 	{
//					local_audio[ i]     = local_audio[ i ] / 4096;
//				}
//
//				audio_samples = mp3FrameInfo.outputSamps * 2;
//
			} else {
//
//				// Only keep the left channel for stereo signals.
//				audio_samples = mp3FrameInfo.outputSamps/2;
//				for ( i = 0;i<audio_samples;i++) {
//					local_audio[i] = local_audio[2*i];
//				}
			}

			audio_samples = mp3FrameInfo.outputSamps;
			copy_samples = ( audio_samples > needed_samples ) ? needed_samples : audio_samples;
			memcpy( buffer + provided_samples, local_audio , sizeof(int16_t) * copy_samples );

			audio_samples 		-= copy_samples;
			memmove(local_audio, local_audio + copy_samples, sizeof(int16_t) * audio_samples  );

			needed_samples 		-= copy_samples;
			provided_samples	+= copy_samples;

		}

	} while( res == FR_OK && !out_of_data && needed_samples > 0 );

	// Return samples created
	return(provided_samples);
}

/*
 * Taken from
 * http://www.mikrocontroller.net/topic/252319
 */
static uint32_t Mp3ReadId3V2Text(FIL* pInFile, uint32_t unDataLen, char* pszBuffer, uint32_t unBufferSize)
{
	UINT unRead = 0;
	BYTE byEncoding = 0;
	if((f_read(pInFile, &byEncoding, 1, &unRead) == FR_OK) && (unRead == 1))
	{
		unDataLen--;
		if(unDataLen <= (unBufferSize - 1))
		{
			if((f_read(pInFile, pszBuffer, unDataLen, &unRead) == FR_OK) ||
					(unRead == unDataLen))
			{
				if(byEncoding == 0)
				{
					// ISO-8859-1 multibyte
					// just add a terminating zero
					pszBuffer[unDataLen] = 0;
				}
				else if(byEncoding == 1)
				{
					// UTF16LE unicode
					uint32_t r = 0;
					uint32_t w = 0;
					if((unDataLen > 2) && (pszBuffer[0] == 0xFF) && (pszBuffer[1] == 0xFE))
					{
						// ignore BOM, assume LE
						r = 2;
					}
					for(; r < unDataLen; r += 2, w += 1)
					{
						// should be acceptable for 7 bit ascii
						pszBuffer[w] = pszBuffer[r];
					}
					pszBuffer[w] = 0;
				}
			}
			else
			{
				return 1;
			}
		}
		else
		{
			// we won't read a partial text
			if(f_lseek(pInFile, f_tell(pInFile) + unDataLen) != FR_OK)
			{
				return 1;
			}
		}
	}
	else
	{
		return 1;
	}
	return 0;
}

/*
 * Taken from
 * http://www.mikrocontroller.net/topic/252319
 */
uint32_t Mp3ReadId3V2Tag(FIL* pInFile, char* pszArtist, uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize)
{
	pszArtist[0] = 0;
	pszTitle[0] = 0;
	FRESULT	debug_value;
	BYTE id3hd[10];
	UINT unRead = 0;
	if((f_read(pInFile, id3hd, 10, &unRead) != FR_OK) || (unRead != 10))
	{
		return 1;
	}
	else
	{
		uint32_t unSkip = 0;
		if((unRead == 10) &&
				(id3hd[0] == 'I') &&
				(id3hd[1] == 'D') &&
				(id3hd[2] == '3'))
		{
			unSkip += 10;
			unSkip = ((id3hd[6] & 0x7f) << 21) | ((id3hd[7] & 0x7f) << 14) | ((id3hd[8] & 0x7f) << 7) | (id3hd[9] & 0x7f);

			// try to get some information from the tag
			// skip the extended header, if present
			uint8_t unVersion = id3hd[3];
			if(id3hd[5] & 0x40)
			{
				BYTE exhd[4];
				f_read(pInFile, exhd, 4, &unRead);
				size_t unExHdrSkip = ((exhd[0] & 0x7f) << 21) | ((exhd[1] & 0x7f) << 14) | ((exhd[2] & 0x7f) << 7) | (exhd[3] & 0x7f);
				unExHdrSkip -= 4;
				if(f_lseek(pInFile, f_tell(pInFile) + unExHdrSkip) != FR_OK)
				{
					return 1;
				}
			}
			uint32_t nFramesToRead = 2;
			while(nFramesToRead > 0)
			{
				unsigned char frhd[10];
				if(( debug_value =  f_read(pInFile, frhd, 10, &unRead) != FR_OK) || (unRead != 10))
				{
					return 1;
				}
				if((frhd[0] == 0) || (strncmp(frhd, "3DI", 3) == 0))
				{
					break;
				}
				unsigned char szFrameId[5] = {0, 0, 0, 0, 0};
				memcpy(szFrameId, frhd, 4);
				uint32_t unFrameSize = 0;
				uint32_t i = 0;
				for(; i < 4; i++)
				{
					if(unVersion == 3)
					{
						// ID3v2.3
						unFrameSize <<= 8;
						unFrameSize += frhd[i + 4];
					}
					if(unVersion == 4)
					{
						// ID3v2.4
						unFrameSize <<= 7;
						unFrameSize += frhd[i + 4] & 0x7F;
					}
				}

				if(strcmp(szFrameId, "TPE1") == 0)
				{
					// artist
					if(Mp3ReadId3V2Text(pInFile, unFrameSize, pszArtist, unArtistSize) != 0)
					{
						break;
					}
					nFramesToRead--;
				}
				else if(strcmp(szFrameId, "TIT2") == 0)
				{
					// title
					if(Mp3ReadId3V2Text(pInFile, unFrameSize, pszTitle, unTitleSize) != 0)
					{
						break;
					}
					nFramesToRead--;
				}
				else
				{
					if(f_lseek(pInFile, f_tell(pInFile) + unFrameSize) != FR_OK)
					{
						return 1;
					}
				}
			}
		}
		if(f_lseek(pInFile, unSkip) != FR_OK)
		{
			return 1;
		}
	}

	return 0;
}
