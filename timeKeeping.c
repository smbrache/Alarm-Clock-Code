/*
 * timeKeeping.c
 *
 *  Created on: Mar 15, 2017
 *      Author: brent
 */


//This code is used with the AlarmClockMp3 project
//it is called when incrementing the time manually and
//it ensures the time values stay within the bounds of a 12 hour clock

#include "stm32f4xx_hal.h"
#include "timeKeeping.h"

void timeHourCheck(void)
{
	 //9 rolls over to 0
	 if(( ClockTime.Hours & 0x0F) >= 0xA)
	 {
			 ClockTime.Hours = ( ClockTime.Hours & 0x30 ) + 0x10;

	 }

	 //if incrementing passes 12 -> toggles am/pm
	 if( ClockTime.Hours == 0x12 )
	 {
			 ClockTime.TimeFormat ^= RTC_HOURFORMAT12_PM;
	 }

//if incrementing hits hour 13, sets to 1 oclock
	 if( ClockTime.Hours >= 0x13 )
	 {
			 ClockTime.Hours = 0x01;
	 }

}

void timeMinuteCheck(void)
{
        //allows 9 to roll over to 0 and 60 to 0
         if(( ClockTime.Minutes & 0x0F) >= 0xA)
         {
                 ClockTime.Minutes = ( ClockTime.Minutes & 0x70 ) + 0x10;
                 if(( ClockTime.Minutes & 0x70 ) >= 0x60 )
                 {
                         ClockTime.Minutes = 0x00;
                 }
         }
}

void alarmHourCheck(void)
{
//
// 9 rolls over to 0
//
	 if(( ClockAlarm.AlarmTime.Hours & 0x0F) >= 0xA)
	 {
			 ClockAlarm.AlarmTime.Hours = (ClockAlarm.AlarmTime.Hours & 0x30 ) + 0x10;

	 }
//
// if incrementing passes 12 -> toggles am/pm
//
	 if( ClockAlarm.AlarmTime.Hours == 0x12 )
	 {
			 ClockAlarm.AlarmTime.TimeFormat ^= RTC_HOURFORMAT12_PM;
	 }

//
// if incrementing hits hour 13, sets to 1 oclock
//
	 if( ClockAlarm.AlarmTime.Hours >= 0x13 )
	 {
			  ClockAlarm.AlarmTime.Hours = 0x01;
	 }

}

void alarmMinuteCheck(void)
{
         //allows 9 to roll over to 0 and 60 to 0
         if(( ClockAlarm.AlarmTime.Minutes & 0x0F) >= 0xA)
         {
                 ClockAlarm.AlarmTime.Minutes = ( ClockAlarm.AlarmTime.Minutes & 0x70 ) + 0x10;
                 if(( ClockAlarm.AlarmTime.Minutes & 0x70 ) >= 0x60 )
                 {
                         ClockAlarm.AlarmTime.Minutes = 0x00;
                 }
         }
}

