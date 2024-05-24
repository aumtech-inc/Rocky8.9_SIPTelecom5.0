/*-----------------------------------------------------------------------------
Program: sc_rule9.c
Purpose: To implement scheduling rule 9.
		 Rule 9: Always fire the application.
Author:	 George Wenzel
Date:	 02/24/96
-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef int Integer;

#define SUCCESS			0	/* Return this if schedule criteria are met.    */
#define FAIL			1	/* Return this if schedule criteria aren't met. */
#define INVALID_INPUT	1	/* Return this if input parameters aren't sane. */
#define LOGIC_ERROR 	1	/* Return this if you reach code you shouldn't. */
#define NOT_IMPLEMENTED	1	/* Return this if this rule is not implemented. */

Integer 
sc_rule9 (Integer StartDate, Integer StopDate, 
			Integer StartTime, Integer StopTime, 
			Integer Year, Integer Month, Integer Weekday, Integer Mday, 
			Integer Hour, Integer Min, Integer Sec)
{
	return(SUCCESS);
}
