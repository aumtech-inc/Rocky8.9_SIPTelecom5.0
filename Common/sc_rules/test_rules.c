/*----------------------------------------------------------------------------
Program:	test_rules.c
Purpose:	To test the scheduling rules.
		NOTE: As of 2/26/06 this program has not been tested.
Author:		Probably Clyde Hott
Date:		Unknown.
Update:		George Wenzel
Update:		11/07/96 G.Wenzel - added this comment section
Update:		02/26/96 G.Wenzel - changed name from testit.c to test_rules.c
----------------------------------------------------------------------------*/
#include <stdio.h>

main(argc, argv) 
int argc;
char **argv;
{

int StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec;
int ruleno, result = -1;

ruleno=atoi(argv[1]); 	
StartDate=atoi(argv[2]);
StopDate=atoi(argv[3]);	
StartTime=atoi(argv[4]);
StopTime=atoi(argv[5]); 
Year=atoi(argv[6]);		
Month=atoi(argv[7]);
Mday=atoi(argv[8]);
Hour=atoi(argv[9]);
Min=atoi(argv[10]);
Sec=atoi(argv[11]);
Weekday=atoi(argv[12]);


printf("Rule %d applied on %4d/%02d/%02d at %02d:%02d:%02d DayofWeek=%d\n", 
		ruleno,	Year, Month, Mday, Hour, Min, Sec, Weekday);
printf("StartDate=%d, StopDate=%d, StartTime=%d, StopTime=%d\n",
		StartDate, StopDate, StartTime, StopTime);
	Month = Month - 1;
	Year = Year - 1900;
         switch (ruleno)
               {  
                case 0: 
                result = sc_rule0 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 1: 
                result = sc_rule1 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 2: 
                result = sc_rule2 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 3: 
                result = sc_rule3 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 4: 
                result = sc_rule4 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 5: 
                result = sc_rule5 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 6: 
                result = sc_rule6 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 7: 
                result = sc_rule7 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 8: 
                result = sc_rule8 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;

                case 9: 
                result = sc_rule9 (StartDate, StopDate, StartTime, StopTime, Year, Month, Weekday, Mday, Hour, Min, Sec);
                break;
		case 99:
		exit(0);
		break;
                default:
                printf("Wrong input case\n");
                break;

               }

	switch(result)
	{
		case 0: printf("Result: SUCCESS\n"); break;
		case 1: printf("Result: FAIL   \n"); break;
		case 2: printf("Result: INVALID_INPUT\n"); break;
		case 3: printf("Result: LOGIC_ERROR\n"); break;
		case 4: printf("Result: NOT_IMPLEMENTED\n"); break;
		default: printf("Result: Invalid result\n"); break;
	}
	printf("\n");	
}
