/*
 * mono.c
 *
 *  Created on: Feb 22, 2017
 *      Author: brent
 */

#include <stdio.h>
#include <generic.h>
#include <mono.h>


/*
 * Function: AudioToMono
 *
 * Description:
 *
 * Function to convert 16 bit stereo audio data into a mono track for a 12 bit d/a converter using the upper
 *  12 bits of a 16 bit unsigned integer
 *
 */

int16_t AudioToMono( int16_t *Stream, uint16_t StreamSize, uint16_t MergeType, uint16_t NumberOfChannels, int16_t *ReturnStatus )
{
	uint16_t
		Count;

	int16_t
		*Merge;

//
// If mono track then just return. No need to do anything to the data
//
	if ( 1 == NumberOfChannels )
	{
		*ReturnStatus = ANALOG_TO_MONO_STATUS_OK;
		return( PASS );
	}


	if ( 2 == NumberOfChannels )
	{
		Count = 0;
		Merge = Stream;

		switch( MergeType )
		{

//
// Use the left audio channel as the source
//

			case ANALOG_TO_MONO_LEFT_CHANNEL:
			{
//
// Offset the merge pointer and merge
//
				Merge++;
				while ( Count < StreamSize )
				{
					*Stream = *Merge;
					Stream++;
					Merge += 2;
					Count++;
				}
			}
			break;

//
// Use the right audio channel as the source of the stream
//
			case ANALOG_TO_MONO_RIGHT_CHANNEL:
			{
				while ( Count < StreamSize )
				{
					*Stream = *Merge;
					Stream++;
					Merge += 2;
					Count++;
				}
			}
			break;



			case ANALOG_TO_MONO_AVERAGE_CHANNELS:
			{
				while ( Count < StreamSize )
				{

//
// Take the average of the two channels and add an offset
//
					*Stream = (uint16_t) ((*Merge + *(Merge+1)) >> 1 ) + 32768;
					Stream++;
					Merge += 2;
					Count++;
				}
				break;
			}

			default:
			{
				*ReturnStatus  = ANALOG_TO_MONO_STATUS_INVALID_MERGE_TYPE;
				return( FAIL );
			}
		}
	}
	else
	{
		*ReturnStatus  = ANALOG_TO_MONO_STATUS_TOO_MANY_CHANNELS;
		return( FAIL );
	}


	*ReturnStatus = ANALOG_TO_MONO_STATUS_OK;
	return( PASS );

}
