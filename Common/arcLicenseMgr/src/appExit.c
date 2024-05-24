/*------------------------------------------------------------------------------
File:		appExit.c

Purpose:	Exit the application gracefully.

Author:		Sandeep Agate, Aumtech Inc.

Update:		2002/06/05 sja 	Created the file.
------------------------------------------------------------------------------*/

/*
** Include Files
*/
#include <arcLicenseMgr.h>

/*
** Static Function Prototypes
*/

/*------------------------------------------------------------------------------
appExit(): Exit the application gracefully.
------------------------------------------------------------------------------*/
int appExit(char *zMod, int zExitCode)
{
	char	yErrorMsg[256];

	if(gAppEnv.fIsServerConnected == 1)
	{
		if(saExit(gAppEnv.fdSocketServer, yErrorMsg) < 0)
		{
			arcLog(HERE, zMod, REPORT_NORMAL, MSG_ID,
					"Failed to shutdown server socket connection, (%s)",
					yErrorMsg);
		
			// don't return here
		}
	}

	if(gAppEnv.fIsSocketConnected == 1)
	{
		if(saExit(gAppEnv.fdSocketListen, yErrorMsg) < 0)
		{
			arcLog(HERE, zMod, REPORT_NORMAL, MSG_ID,
					"Failed to shutdown listener socket connection, (%s)",
					yErrorMsg);
		
			// don't return here
		}
	}

	arcLog(HERE, zMod, REPORT_NORMAL, MSG_ID,
			"Exiting the application gracefully with exit code (%d).",
			zExitCode);

	exit(zExitCode);

} /* END: appExit() */


