#ident "@(#)util_sleep 96/07/01 2.2.0 Copyright 1996 Aumtech, Inc."
/*-----------------------------------------------------------------------------
Program:	util_sleep.c
Purpose:	To provide a routine that can sleep for a specified number
		of seconds and milliseconds.
Author:		Unknown
Date:		Unknown
Update: 052300 apj Extract whole seconds from given Milliseconds.
-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

int util_sleep(int Seconds, int Milliseconds)
{
	static struct timeval SleepTime;

	int actualSeconds;
	int actualMilliseconds;
	int secondsFromMilli;

	secondsFromMilli = Milliseconds / 1000;
	actualSeconds = Seconds + secondsFromMilli;
	actualMilliseconds = Milliseconds % 1000;

	SleepTime.tv_sec = actualSeconds;
	SleepTime.tv_usec = actualMilliseconds * 1000;

	select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &SleepTime);

    return 0;
}
