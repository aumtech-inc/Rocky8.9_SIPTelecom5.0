/*----------------------------------------------------------------------------
Program:	resp_log.c
Purpose:	This function is probably accessed thru the macro LogMsg.
		It is used by Responsibility to write all its messages. 
Author: 	Madhav Bhide based on work done in '95 by M. Joshi.

Note:		The log_msg routine is very inflexible in that it does not 
		allow the passing of a message number, so that all 
		Responsibility messages are hard coded here to be 3000. 
		This should be changed as soon as possible.

		The changeLogMode function is called only from isp_baseresp.
Date:		05/25/2000 gpw removed unneeded code and wrote this header.
----------------------------------------------------------------------------*/ 
#include <stdio.h>
#include <unistd.h>
#include "ispinc.h" 


void log_msg(int line_no, char *modulename, int debug_mode, char *str)
{
static	int	first_time = 1;
int	ret_code;
char	buf[1024];

buf[0] = '\0';
sprintf(buf, "(%d): %s", line_no, str);
buf[1023] = '\0';		
LogARCMsg(modulename,debug_mode,"0","RES","isp_telresp",3000,buf);
return;
} 

int changeLogMode(char *on_off)
{
int	mode;

if (strcmp(on_off, "ON") == 0)
	GV_LogConfig.ReportingMode = GV_LogConfig.ReportingMode|REPORT_VERBOSE;
else	
	GV_LogConfig.ReportingMode = GV_LogConfig.ReportingMode&REPORT_NORMAL;

return(0);
}
