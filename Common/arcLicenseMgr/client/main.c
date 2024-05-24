/*------------------------------------------------------------------------------
File:		main.c

Purpose:	Main entry point for the arcLicenseClient application.

Author:		Sandeep Agate, Aumtech Inc.

Usage:		arcLicenseClient serverName serverPort

Update:		2002/05/23 sja 	Created the file.
------------------------------------------------------------------------------*/

#define GLOBALS

/*
** Include Files
*/
#include <arcLicenseClient.h>

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

	if(argc < 3)
	{
		fprintf(stdout, "Usage: %s serverName serverPort\n",
				argv[0]);
		exit(1);
	}

	/*
	** Initialize the application
	*/
	if(appInit(argc, argv) < 0)
	{
		appExit(yMod, 1); // error msg. written in sub-routine
	}

	processRequests();

	/*
	** Exit the application gracefully.
	*/
	appExit(yMod, 0);

	return (0);

} /* END: main() */


