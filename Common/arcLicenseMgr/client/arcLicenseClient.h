/*------------------------------------------------------------------------------
File:		arcLicenseMgr.h

Purpose:	Header information used by the arcLicenseMgr program.
		
Author:		Aumtech Inc.

Update:		2002/05/23 sja 	Created the file
------------------------------------------------------------------------------*/
#ifndef arcLicenseMgr__H 		/* Just preventing multiple includes */
#define arcLicenseMgr__H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#include <ISPCommon.h>
#include <saUtils.h>

/*
** Constants and Macros
*/

/*
** Constants for UTL_VarLog()
*/
#define HERE					__FILE__,__LINE__
#define ARC_SERVICE				"LIC"
#define MSG_ID					9999
#define NORMAL					HERE, yMod, REPORT_NORMAL, MSG_ID
// #define VERBOSE					HERE, yMod, REPORT_VERBOSE, MSG_ID
#define VERBOSE					NORMAL

/*
** Typedefs and structs
*/
typedef struct
{
	/*
	** Application Specific Members
	*/
	char	appName[20];
	char	resource[20];
	char	serverName[80];
	int		serverPort;

	/*
	** Socket Related Members
	*/
	int				fdSocketClient;
	unsigned short	fIsSocketConnected;

} AppEnv;

/*
** Global Variables
*/
#ifdef GLOBALS
AppEnv		gAppEnv;
#else
extern AppEnv		gAppEnv;
#endif /* GLOBALS */

/*
** Externs
*/
extern int	errno;

/*
** Function Prototypes
*/
int	processRequests(void);
int arcLog (char *zFile, int zLine, char *zMod, int zReportMode, int zMsgId,
			char *zMsgFormat, ...);

#endif /* arcLicenseMgr__H */

