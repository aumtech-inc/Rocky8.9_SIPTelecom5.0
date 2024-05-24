/*----------------------------------------------------------------------------
Program:	sc_rule2.c
Purpose:	To implement rule 2.
		Rule 2:
		Fire the application if:
		The point in time denoted by the current date and time is
		anywhere on the continuous time line between the start date and
		time and the end date and time.
Author:		George Wenzel
Date:		02/24/96
Update:		07/20/98 gpw made Y2K compliant
----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef int Integer;

#define SUCCESS	0	/* Return this if schedule criteria are met.    */
#define FAIL	1	/* Return this if schedule criteria aren't met. */
#define INVALID_INPUT 2	/* Return this if input parameters aren't sane. */
#define LOGIC_ERROR 3	/* Return this if you reach code you shouldn't. */
#define NOT_IMPLEMENTED	4 /* Return this if this rule is not implemented. */

Integer 
sc_rule2 (Integer StartDate, Integer StopDate, 
		Integer StartTime, Integer StopTime, 
		Integer Year, Integer Month, Integer Weekday, Integer Mday, 
		Integer Hour, Integer Min, Integer Sec)
{
	int today;
	int cur_time;
	Integer adjustment;

	if (Year < 100)
		adjustment = 19000000;
	else
		adjustment = 20000000;

        if (StartDate < 1000000) StartDate = StartDate + adjustment;
        if (StopDate  < 1000000) StopDate  = StopDate  + adjustment;

	/* Validate StartDate, StopDate, StartTime, StopTime */	

	if (StartDate > StopDate) return(INVALID_INPUT);
	if ( (StartDate == StopDate) && (StartTime > StopTime) )
		return(INVALID_INPUT);

    /*	today = (Year * 10000) + ( (Month +1) * 100 ) + Mday;*/
        today = ( (Year + 1900) * 10000) + ((Month + 1) * 100)  + Mday;

	/* If today is out of range, it is always a failure. */
	if ( (today < StartDate) || (today > StopDate) ) return(FAIL);
	
	/* If today is strictly between the days, it's always a success. */
	if ( ( today > StartDate) && (today < StopDate) ) return(SUCCESS);

	/* At this point, StartDate is strictly less than StopDate, AND */
	/* today is the same as EITHER the_StartDate or StopDate */
	cur_time = (Hour * 10000) + (Min * 100) + Sec;

	if (StartDate == StopDate)
	{
		/* For success, the time must be in range */	
		if ( (cur_time >= StartTime) && (cur_time <= StopTime) )
			return(SUCCESS);
		else
			return(FAIL);
	}

	if ( today == StartDate)
	{
		/* For success, the time must be after the start time. */
	 	if (cur_time >= StartTime) 
			return(SUCCESS);
		else
			return(FAIL);
	}
	
	if ( today == StopDate)
	{
		/* For success, the time must be before the end time. */
	 	if (cur_time <= StopTime) 
			return(SUCCESS);
		else
			return(FAIL);
	}

	/* We should never get here; we've covered all cases. */
	return(LOGIC_ERROR);

}
