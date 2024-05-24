/*----------------------------------------------------------------------------
Program:	sc_rule3.c
Purpose:	To implement scheduling rule 3.
		Rule 3:
		Fire the application if:
		The point in time denoted by the current date and time is
		anywhere on the continuous time line between the start date and
		time and the end date and time.
		The start date and end date are specified as days of the week,
		i.e., 1=Mon, 2=Tues, ... 7=Sun.
		
		NOTE: A starting day of the week can be higher than the stopping
			  day. See the comment in the function below.

		NOTE: The day of week parameter passed may be a 0 (for Sunday). 
		This is probably because it was calculated by a Unix 
		function from the system date. In this case, it is converted
		to 7 before processing.
Author:		George Wenzel
Date:		03/18/96
Date:		04/11/96 G. Wenzel made StartDate, StopDate check 1-7
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

/*----------------------------------------------------------------------------
check_day_range:Verify that today is within the range of start_day, end_day
NOTE: 	end_day can be less than the start_day. For example, start_day=6 
	and end_day=1 implies Saturday thru Monday. In this case today must 
	be => end_day and =< start_day for today to be "in range". 
----------------------------------------------------------------------------*/
static Integer 
check_day_range (Integer today, Integer start_day, Integer end_day)
{
  	if (start_day <= end_day)
    {
	/* For example: start_day 1 to end_day 5 means Mon thru Fri. */
    	if ( (start_day <= today) && (today <= end_day) )
			return(SUCCESS);
		else
			return(FAIL);
    }
	else
	{
	/* For example: start_day 6 to end_day 2 means Saturday thru Tuesday. */
	/* In this case 6,7,1, and 2 are included, e.g., Sat,Sun,Mon,Tues. */
    	if ( (start_day <= today) || (today <= end_day) )
	  		return(SUCCESS);
      	else
  			return(FAIL);
	}
}

Integer 
sc_rule3 (Integer StartDate, Integer StopDate, Integer StartTime, Integer StopTime, Integer Year, Integer Month, Integer Weekday, Integer Mday, Integer Hour, Integer Min, Integer Sec)
{
  	int ret;
	int start_day_of_week;
	int stop_day_of_week;
	int today;
	int cur_time;

	start_day_of_week = StartDate;
	stop_day_of_week  = StopDate;

	if ( (start_day_of_week < 1) || (start_day_of_week > 7) ) 
		return(INVALID_INPUT);
	if ( (stop_day_of_week  < 1) || (stop_day_of_week  > 7) ) 
		return(INVALID_INPUT);

	/* Note: if unix passed a 0, make it a 7 for processing. */
	today = Weekday;
	if (today == 0) today = 7;	

	ret = check_day_range(today, start_day_of_week, stop_day_of_week);
	if (ret != SUCCESS) return(ret);

	/* At this point, today is within range, we have either:
	Case 	1) today = start_day_of_week (see if before start time)
	Case	2) today = stop_day_of_week (see if after end time )
	Case	1) today is BETWEEN the start & stop_day_of_week (success)

	NOTE: Do the checking in this order, because remember that the
		start_day_of_week can be HIGHER than stop_day_of_week */

	/* Calculate current time and check the range */
	cur_time = (Hour * 10000) + (Min * 100) + Sec;

	/* Case 1 */ 	
	if ( today == start_day_of_week)
	{
		
		if  (cur_time < StartTime)
			/* now is before the start time */
			return(FAIL);
		else
			return (SUCCESS);
	}
	/* Case 2 */ 	
	if ( today == stop_day_of_week)
	{
		
		if  (cur_time > StopTime)
			/* now is after the end time */
			return(FAIL);
		else
			return (SUCCESS);
	}
	/* Case 3 */
	return(SUCCESS);
}
