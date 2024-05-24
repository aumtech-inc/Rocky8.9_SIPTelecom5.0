/*---------------------------------------------------------------------------
Program:	LOGINIT.c 
Purpose:	Common Log initialization module to populate the GV_LogConfig 
		structure.
		Derived from original TELIX020.c - Telecom specific 
 		1) locates the $PARMSLIB/parms directory and call 
		LOGXXCTL to read log configuration and populate GV_LogConfig.
		(Currently, GV_LogConfig is associate with each process
		instance. In the future, we might want to re-think this.
		Perhaps we should have GV_LogConfig set only once for the
		entire domain. Then a LOGXXCTL function can be called
		to specify process/object specific configuration.)
		2) Call LOGXXCTL to register itself with ISP common logger.
		3) load Object specific message definitions into LOG memory. 
Author: 	J. Huang
Date:		05/18/94
Update:		03/18/96 G. Wenzel changed REPORT_VERBOSE to REPORT_DIAGNOSE
Update:		04/16/96 G. Wenzel removed R21 references 
Update:		05/06/03 mpb changed REPORT_DIAGNOSE to REPORT_DETAIL
Update:		05/08/03 apj&ddn changed REPORT_DETAIL to REPORT_VERBOSE.
-------------------------------------------------------------------------------
Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
All Rights Reserved.

This is an unpublished work of AT&T ISP which is protected under 
the copyright laws of the United States and a trade secret
of AT&T. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of AT&T.
-----------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdarg.h>
#include "ispinc.h"
#include "loginc.h"
#include "COMmsg.h"

#define	MAXLOGLINE 200
#define	ConfFile_PATH	"/Global/"
#define	ConfFile_NAME	".Global.cfg"

extern int LOGXXCTL(LOG_ReqParms_t *    PT_logctl);
extern int LOGXXPRT(char *module, int requested_mode, int MessageID, char *bogus, ... );


int LOGINIT(char *object, char *prefix, int msgbase, int maxdefs, char **msglist)
// int LOGINIT(char *bogus, ... )
{

	LOG_ReqParms_t	logctl;
    char *stor  = "test";
	MsgSet_t	mymsg;
	va_list		ap;
	char	ParmEnvName[]="ISPBASE";
	FILE	*PT_file;
	char 	ConfigDir[80], ConfigFile[100];
	char 	MyhostName[16];
	char	Logopt[500];
	char	*PT_Parmlib;
	char	MyModule[]="LOGINIT";
	char	MyObject[10];
	char	LogLine[MAXLOGLINE];
   	char	LOGINIT_000[] = "000 - ISPBASE variable missing - FATAL";
	char	LOGINIT_001[]= "001 - Log initialization failed";
	char	LOGINIT_002[]= "001 - Invalid parameter passed - (%s)";

	sprintf(MyObject, "%s", object);
	
	/* Load global log attributes */


    	if ((PT_Parmlib = getenv(ParmEnvName)) == NULL) 
	{
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, "%s", LOGINIT_000);
      		return(ISP_FAIL);
      	}
	sprintf(ConfigDir, "%s%s", PT_Parmlib, ConfFile_PATH);
	sprintf(ConfigFile, "%s%s", ConfigDir, ConfFile_NAME);

	logctl.CommandId=ISP_LOGCMD_SETCONF;
	logctl.PT_Object=MyObject;
	logctl.PT_UserData=ConfigFile;
	logctl.PT_LogOpt=Logopt;

        if ( LOGXXCTL( &logctl ) == ISP_FAIL )
        {
		/* gpw - no error message here */
		return (ISP_FAIL);
	}

	/* If host specific attribute file exist, over write the default */

	gethostname(MyhostName, sizeof(MyhostName)-1 );
        sprintf(ConfigFile, "%s%s", ConfigDir, MyhostName);

	if( (PT_file = fopen(ConfigFile, "r")) != NULL )
        {
		/* optional configuration, if file does not
		 * exist, then don't bother to call LOGXXCTL.
		 */
		logctl.CommandId=ISP_LOGCMD_SETCONF;
		logctl.PT_Object=MyObject;
		logctl.PT_UserData=ConfigFile;
		logctl.PT_LogOpt[0]='\0';

        	if ( LOGXXCTL( &logctl ) == ISP_FAIL )
        	{
			return (ISP_FAIL);
		}
		fclose( PT_file );

	}


	logctl.CommandId=ISP_LOGCMD_INIT;
	/* PT_Object and PT_LogOpt were set in the above LOGXXCTL calls */

        if ( LOGXXCTL( &logctl ) == ISP_FAIL )
        {
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, "%s", LOGINIT_001);
		return (ISP_FAIL);
	}

	logctl.CommandId=ISP_LOGCMD_LOADMSG;
	logctl.PT_MsgSet=&mymsg;
	
	strcpy(mymsg.object, MyObject );

	sprintf(mymsg.prefix,  "%s", prefix);
	mymsg.msgbase= msgbase;
	mymsg.version=1;

	mymsg.max_defined=maxdefs;
	mymsg.msglist=msglist;

	if( mymsg.msglist == NULL )
	{
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, "%s", LOGINIT_001);
		return (ISP_FAIL);
	}


        if ( LOGXXCTL( &logctl ) == ISP_FAIL ) return (ISP_FAIL);
	
	return ( LOGXXPRT( MyModule, REPORT_VERBOSE, COM_ILOGSTART, "%s", "warning") );
}


