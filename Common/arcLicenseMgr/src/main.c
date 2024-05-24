/*------------------------------------------------------------------------------
File:		main.c

Purpose:	Main entry point for the arcLicenseMgr application.

Author:		Sandeep Agate, Aumtech Inc.

Usage:		arcLicenseMgr

Update:		2002/05/23 sja 	Created the file.
------------------------------------------------------------------------------*/

#define GLOBALS

/*
** Include Files
*/
#include <arcLicenseMgr.h>

/*
** Static Function Prototypes
*/

/*------------------------------------------------------------------------------
main(): Main entry point of the application.
------------------------------------------------------------------------------*/
int
main (int argc, char *argv[])
{
	static char	yMod[] 			= "main";
	char		yVersion[10]	= "1.0.0";
	char		yBuild[10]		= "1";

	if(argc == 2 && strcmp(argv[1], "-v") == 0)
	{
		fprintf(stdout, 
			"Aumtech's ARC License Manager (%s) version %s, build %s.\n"
			"Compiled on %s at %s.\n",
			argv[0], yVersion, yBuild, __DATE__, __TIME__);

		exit(0);
	}

	arcLog(NORMAL,
			"Starting Aumtech's License Manager (%s) version %s, build %s.",
			argv[0], yVersion, yBuild);

	/*
	** Initialize the application
	*/
	if(appInit(argc, argv) < 0)
	{
		appExit(yMod, 1); // error msg. written in sub-routine
	}

	/*
	** Process client requests
	*/
	processRequests();

	/*
	** Exit the application gracefully.
	*/
	appExit(yMod, 0);

	return (0);

} /* END: main() */


