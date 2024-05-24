/*------------------------------------------------------------------------------
File:		appExit.c

Purpose:	Exit the application gracefully.

Author:		Sandeep Agate, Aumtech Inc.

Update:		2002/06/05 sja 	Created the file.
------------------------------------------------------------------------------*/

/*
** Include Files
*/
#include <arcLicenseClient.h>

/*
** Static Function Prototypes
*/

/*------------------------------------------------------------------------------
appExit(): Exit the application gracefully.
------------------------------------------------------------------------------*/
int appExit(char *zMod, int zExitCode)
{
	char	yErrorMsg[256];

	if(gAppEnv.fIsSocketConnected == 1)
	{
		if(saExit(gAppEnv.fdSocketClient, yErrorMsg) < 0)
		{
			arcLog(HERE, zMod, REPORT_NORMAL, MSG_ID,
					"Failed to shutdown client socket connection, (%s)",
					yErrorMsg);
		
			// don't return here
		}
	}

	exit(zExitCode);

} /* END: appExit() */


