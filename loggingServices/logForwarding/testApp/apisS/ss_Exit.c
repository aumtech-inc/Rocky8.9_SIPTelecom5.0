/*-----------------------------------------------------------------------------
Program:	ss_Exit.c
Purpose:	Exit routine
Author:		Aumtech, Inc.
Update:		03/07/97 sja	Added this header 
Update:		03/27/97 sja	Re-indented code
Update:		03/31/97 sja	Added send_to_monitor
Update:		04/02/97 sja	Changed #include WSS_Globals.h to WSS_Externs.h
Update: 06/11/98 mpb  Changed GV_Connected to GV_SsConnected.
-----------------------------------------------------------------------------*/

#include "ssHeaders.h"
#include "WSS_Externs.h"

static char ModuleName[]="ss_Exit";

/*----------------------------------------------------------------------
ss_Exit(): This routine close the connection and do cleanup
------------------------------------------------------------------------*/
int ss_Exit()
{
	char	dummy[10];
	char	diag_mesg[100];
	static int	exitCalled = 0;

	sprintf(LAST_API, "%s", ModuleName);

	if(exitCalled == 1)
		return(0);

	exitCalled = 1;


	if (GV_SsConnected == 0)
	{
		Write_Log(ModuleName, "Client disconnected");
	}

	memset(dummy, 0, sizeof(dummy));
	while (read(GV_tfd, dummy, 5) > 0); 	/* Socket cleanup */

	shutdownSocket(ModuleName);

	GV_SsConnected = 0;
	return (ss_SUCCESS);
} /* ss_Exit() */
