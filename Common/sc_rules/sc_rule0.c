/*-----------------------------------------------------------------------------
Program:	sc_rule0.c
Purpose:	Implements scheduling rule 0.
		Rule 0:
		Fire the application if:
		1) Today is between the start day and the stop day,  AND
		2) The current time is between the start time and the stop time
Author:		G. Wenzel
Date:		02/23/96
Update:		03/01/96 G. Wenzel added defines 
Update:		07/20/98 G. Wenzel made Y2k compliant
-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef int Integer;

#define  SUCCESS 0       /* Return this if schedule criteria are met.    */
#define FAIL    1       /* Return this if schedule criteria aren't met. */
#define INVALID_INPUT 2 /* Return this if input parameters aren't sane. */
#define LOGIC_ERROR 3   /* Return this if you reach code you shouldn't. */
#define NOT_IMPLEMENTED 4 /* Return this if this rule is not implemented. */
 
/*----------------------------------------------------------------------------
check_range: 	Return SUCCESS if ( low <= cur <= high)
		Return FAIL otherwise.
		Return FAIL if high < low 	
----------------------------------------------------------------------------*/
static Integer check_range (Integer cur, Integer low, Integer high)
{
	
     	if ((low <= cur) && (cur <= high)) 
		return(SUCCESS);
	else
		return (FAIL);
}

Integer 
sc_rule0 (Integer StartDate, Integer StopDate, Integer StartTime, Integer StopTime, Integer Year, Integer Month, Integer Weekday, Integer Mday, Integer Hour, Integer Min, Integer Sec)
{
	int ret;
	Integer cur_date;
	Integer cur_time;
	Integer adjustment;

	if (Year < 100) 
		adjustment = 19000000;
	else
		adjustment = 20000000;

	if (StartDate < 1000000) StartDate = StartDate + adjustment;
	if (StopDate  < 1000000) StopDate  = StopDate  + adjustment;
	
	if (StartDate > StopDate) return(INVALID_INPUT);
	if (StartTime > StopTime) return(INVALID_INPUT);

     	cur_date = ( (Year + 1900) * 10000) + ((Month + 1) * 100)  + Mday; 
	cur_time = (Hour * 10000) + (Min * 100) + Sec;

	ret = check_range (cur_date, StartDate, StopDate);
      	if (ret == FAIL) return(FAIL);

	/* At this point cur_date is in range */
  	
	ret = check_range (cur_time, StartTime, StopTime);
	return (ret);
}
