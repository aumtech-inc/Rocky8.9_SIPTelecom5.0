/*------------------------------------------------------------------
LOG_SetMode() :

Description : 
		This api set log mode run time.
		
Author 	: Mahesh Joshi.
created : Mon Mar 11 12:27:18 EST 1996
Update:		04/12/96 G. Wenzel changed ism/lm.h to lm.h
--------------------------------------------------------------------*/
#include <lm.h>
#include "ispinc.h"
#include "loginc.h"

static int LOG_SetMode(char *report_mode, char *on_off)
{
LOG_ReqParms_t 	tmp_logctl;
int	mode;

if (strcmp(on_off, "ON") == 0)
	tmp_logctl.CommandId = ISP_LOGCMD_SETMODEON;
else if (strcmp(on_off, "OFF") == 0)
	tmp_logctl.CommandId = ISP_LOGCMD_SETMODEOFF;
else	
	return (-2);	/* invalid parameter 2 */

if (strcmp(report_mode, "REPORT_NORMAL") == 0)
	tmp_logctl.PT_UserData = (char *) REPORT_NORMAL;
else if (strcmp(report_mode, "REPORT_VERBOSE") == 0)
	tmp_logctl.PT_UserData = (char *) REPORT_VERBOSE;
else if (strcmp(report_mode, "REPORT_DIAGNOSE") == 0)
	tmp_logctl.PT_UserData = (char *) REPORT_DIAGNOSE;
else if (strcmp(report_mode, "REPORT_DEBUG") == 0)
	tmp_logctl.PT_UserData = (char *) REPORT_DEBUG;
else if (strcmp(report_mode, "REPORT_SPECIAL") == 0)
	tmp_logctl.PT_UserData = (char *) REPORT_SPECIAL;
else if (strcmp(report_mode, "REPORT_CDR") == 0)
	tmp_logctl.PT_UserData = (char *) REPORT_CDR;
else if (strcmp(report_mode, "REPORT_CEV") == 0)
	tmp_logctl.PT_UserData = (char *) REPORT_CEV;
else
	return (-1);
if (LOGXXCTL(&tmp_logctl) != ISP_SUCCESS)
	return (-3);
} /* LOG_SetMode */
