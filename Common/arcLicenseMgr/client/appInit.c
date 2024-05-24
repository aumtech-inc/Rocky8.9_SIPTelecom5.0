/*------------------------------------------------------------------------------
File:		appInit.c

Purpose:	Initialize the application.

Author:		Sandeep Agate, Aumtech Inc.

Update:		2002/05/23 sja 	Created the file.
------------------------------------------------------------------------------*/

/*
** Include Files
*/
#include <arcLicenseClient.h>

/*
** Static Function Prototypes
*/
static int sSetupSignals(void);
static void sSignalHandler(int zSignalNumber);

/*------------------------------------------------------------------------------
appInit(): Exit the application gracefully.
------------------------------------------------------------------------------*/
int appInit(int argc, char *argv[])
{
	static char	yMod[] = "appInit";
	int			yRc;
	char		yErrorMsg[256];

	/*
	** Clear all buffers
	*/
	memset(&gAppEnv, 0, sizeof(gAppEnv));

	/*
	** Save parameters
	*/
	sprintf(gAppEnv.serverName, "%s", argv[1]);
	gAppEnv.serverPort = atoi(argv[2]);

	/*
	** Setup Signals
	*/
	if(sSetupSignals() < 0)
	{
		return(-1); // error msg. written in sub-routine
	}
	
	return(0);

} /* END: appInit() */

/*------------------------------------------------------------------------------
sSignalHandler(): Generic signal handler routine
------------------------------------------------------------------------------*/
static void sSignalHandler(int zSignalNumber)
{
	static char	yMod[] = "sSignalHandler";
	char		ySignalName[30];
	char		yErrorMsg[256];
	int			yIndex;

	switch(zSignalNumber)
	{
		case SIGTERM:
			sprintf(ySignalName, "%s", "SIGTERM");
			break;

		case SIGHUP:
			sprintf(ySignalName, "%s", "SIGHUP");
			break;

		case SIGINT:
			sprintf(ySignalName, "%s", "SIGINT");
			break;

		case SIGPIPE:
			sprintf(ySignalName, "%s", "SIGPIPE");
			break;

		default:	
			sprintf(ySignalName, "signal %d", zSignalNumber);
			break;
	}

	arcLog(NORMAL,
		"Received (%s). Exiting.", ySignalName);

	/*
	 * Exit the application gracefully.
	 */
	appExit(yMod, 0);

	exit (0);
} /* END: sSignalHandler() */

/*------------------------------------------------------------------------------
sSetupSignals(): Setup all signal handlers
------------------------------------------------------------------------------*/
static int sSetupSignals(void)
{
	static char			yMod[] = "sSetupSignals";
	struct sigaction	ySigAct, ySigOAct;

	/*
	 * Setup signal handlers
	 */
	ySigAct.sa_handler 	= NULL;
	ySigAct.sa_flags 	= SA_NOCLDSTOP | SA_NOCLDWAIT;

	if (sigaction(SIGCHLD, &ySigAct, &ySigOAct) != 0)
	{
		arcLog(NORMAL,
			"sigaction(SIGCHLD,SA_NOCLDWAIT) failed, [%d: %s]",
			errno, strerror(errno));

		return(-1);
	} 

	/*
	** Ignore SIGCHLD
	*/
	if(signal(SIGCHLD,SIG_IGN) == SIG_ERR)
	{
		arcLog(NORMAL,
			"signal(SIGCHLD,SIG_IGN) failed, [%d: %s]",
			errno, strerror(errno));

		return(-1);
	} 

	if(sigset(SIGCLD, SIG_IGN) < 0)
	{
		arcLog(NORMAL,
			"sigset(SIGCHLD,SIG_IGN) failed, [%d: %s]",
			errno, strerror(errno));

		return(-1);
	} 

	/*
	** Setup signal handlers
	*/
	signal(SIGTERM, sSignalHandler);
	signal(SIGINT, 	sSignalHandler);
	signal(SIGHUP, 	sSignalHandler);
	signal(SIGPIPE,	sSignalHandler);
	
	return(0);

} /* END: sSetupSignals() */

