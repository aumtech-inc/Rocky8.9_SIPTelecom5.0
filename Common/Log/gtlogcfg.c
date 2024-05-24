/*-----------------------------------------------------------------------------
Program:	gtlogcfg.c
Purpose:	Utility to get the value of a log key word value from the 
		Global configuration file.
 		It is used called by shell scripts.
 		It only takes one parameter, available parameters are: 
 		
 	Parameters	for the key word value of:	Return:
 	----------	--------------------------	-------
 	iscentralmgr	LOG_CENTRAL_MGR			0/1
 	logid		LOG_ID				gtlogcfg_value:|value|
 	isnormalon	REPORT_NORMAL			0/1
 	isverboseon	REPORT_VERBOSE			0/1
 	isdebugon 	REPORT_DEBUG			0/1
 	isspecialon	REPORT_SPECIAL			0/1
 	islogremote   	LOG_x_REMOTE			0/1
 	logprimserv   	LOG_PRIMARY_SERVER		gtlogcfg_value:|value|
 	logsecserv	LOG_SECONDARY_SERVER		gtlogcfg_value:|value|
 	isloglocal	LOG_x_LOCAL			0/1
 	logfile   	LOG_x_LOCAL_FILE		gtlogcfg_value:|value|
 	islogstderr	LOG_x_STDERR			0/1
 	islogstdout	LOG_x_STDOUT			0/1
 	logretention	LOG_x_RETENTION			gtlogcfg_value:|value|
 	logoption 	log configuration string passed to iVIEW
Author: 	J. Huang
Date:		05/25/94
Update: 	07/27/94 J. Huang Longer log option buffer 
Update:		03/20/96 G. Wenzel changed exit(RETURN_FAIL) to exit(-1)
Update:		04/03/96 G. Wenzel added out_format_int for Log Retention
Update:		04/16/96 G. Wenzel removed R21 references
-----------------------------------------------------------------------------
Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
All Rights Reserved.

This is an unpublished work of AT&T ISP which is protected under 
the copyright laws of the United States and a trade secret
of AT&T. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of AT&T.

-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include "ispinc.h"
#include "loginc.h"

#define MAXOPT	2
#define	config_subpath	"/Global/"
#define	config_filename	".Global.cfg"

main( argc, argv  )
int argc;
char **argv;
{

	char		ISPBASE[]="ISPBASE";
	LOG_ReqParms_t	logctl;
	FILE		*fp;
	char 		config_dir[80];
	char		config_pathname[100];
	char 		my_host_name[16];
	char		*base_dir;
	char		module[]="GTLOGCFG";
	char		parm[20];
	char		lm_opts[300]; /* iVIEW options passed to lm_init */
	char		out_format[]="gtlogcfg_value:|%s|";
	char		out_format_int[]="gtlogcfg_value:|%d|";
	char 		msg[256];

   	char 	GTLOGCFG_000[] = "000 - ISPBASE environment variable missing";
   	char 	Usage[] = "Usage: gtlogcfg <keyword>\n";

	if ( argc != MAXOPT )
	{
      		LOGXXPRT(module, REPORT_STDWRITE, PRINTF_MSG, Usage);
		exit ( -1 );
	}

	/*  Load global log attributes file */

    	if ((base_dir = getenv(ISPBASE)) == NULL) 
	{
      		LOGXXPRT(module, REPORT_STDWRITE, PRINTF_MSG, GTLOGCFG_000);
      		return(ISP_FAIL);
      	}
	sprintf(config_dir, "%s%s", base_dir, config_subpath );
	sprintf(config_pathname, "%s%s", config_dir, config_filename);


	/* Set the Configuration File Parameters - populates GV_LogConfig */

	logctl.CommandId=ISP_LOGCMD_SETCONF;
	logctl.PT_UserData=config_pathname;
	logctl.PT_LogOpt=lm_opts;
        if ( LOGXXCTL( &logctl ) == ISP_FAIL )
        {
		sprintf(msg,"Failed to get parameters from <%s>",
			config_pathname);
      		LOGXXPRT(module, REPORT_STDWRITE, PRINTF_MSG, msg);
		return (ISP_FAIL);
	}

	/* If host specific attribute file exists, over write the defaults */

	gethostname(my_host_name, sizeof(my_host_name)-1 );
        sprintf(config_pathname, "%s%s", config_dir, my_host_name);

	if( (fp = fopen(config_pathname, "r")) != NULL )
        {
		logctl.CommandId=ISP_LOGCMD_SETCONF;
		logctl.PT_UserData=config_pathname;
		logctl.PT_LogOpt[0]='\0'; /* assign the value not address! */

        	if ( LOGXXCTL( &logctl ) == ISP_FAIL )
        	{
			sprintf(msg,
			"Failed to get parameters from <%s>", config_pathname);
      			LOGXXPRT(module, REPORT_STDWRITE, PRINTF_MSG, msg);
			exit (-1);
		}
		fclose( fp );
	}

	strcpy(parm, argv[1]);

	if( !strcmp( parm, "iscentralmgr" ) )
	{
		if( GV_LogConfig.CentralMgr == True )
			exit( 0 );
		else
			exit( 1 );
	}

	if ( !strcmp( parm, "logid" ) )
	{
		printf( out_format, GV_LogConfig.LogID );
		exit( 0 );
	}

	if ( !strcmp( parm, "isnormalon" ) )
	{
		if( GV_LogConfig.ReportingMode & REPORT_NORMAL )
			exit( 0 );
		else
			exit( 1 );
	}

	if( !strcmp( parm, "isverboseon" ) )
	{
		if( GV_LogConfig.ReportingMode & REPORT_VERBOSE )
			exit( 0 );
		else
			exit( 1 );
	}

	if ( !strcmp( parm, "isdebugon" ) )
	{
		if( GV_LogConfig.ReportingMode & REPORT_DEBUG )
			exit( 0 );
		else
			exit( 1 );
	}

	if ( !strcmp( parm, "isspecialon" ) )
	{
		if( GV_LogConfig.ReportingMode & REPORT_SPECIAL )
			exit( 0 );
		else
			exit( 1 );
	}

	if ( !strcmp( parm, "islogremote" ) )
	{
		if( GV_LogConfig.Log_x_remote == True )
			exit( 0 );
		else
			exit( 1 );
	}

	if ( !strcmp( parm, "logprimserv" ) )
	{
		printf(out_format, GV_LogConfig.Log_PrimarySrv );
		exit( 0 );
	}

	if ( !strcmp( parm, "logsecserv" ) )
	{
		printf(out_format, GV_LogConfig.Log_SecondarySrv );
		exit( 0 );
	}

	if ( !strcmp( parm, "isloglocal" ) )
	{
		if( GV_LogConfig.Log_x_local == True )
			exit( 0 );
		else
			exit( 1 );
	}

	if ( !strcmp( parm, "logfile" ) )
	{
		printf(out_format, GV_LogConfig.Log_x_local_file );
		exit( 0 );
	}


	if ( !strcmp( parm, "islogstderr" ) )
	{
		if( GV_LogConfig.Log_x_stderr == True )
			exit( 0 );
		else
			exit( 1 );
	}


	if ( !strcmp( parm, "islogstdout" ) )
	{
		if( GV_LogConfig.Log_x_stdout == True )
			exit( 0 );
		else
			exit( 1 );
	}

	if ( !strcmp( parm, "logretention" ) )
	{
		printf(out_format_int, GV_LogConfig.Log_Retention );
		exit( 0 );
	}

	if ( !strcmp( parm, "logoption" ) )
	{
		printf(out_format, lm_opts );
		exit( 0 );
	}

      	LOGXXPRT(module,REPORT_STDWRITE,PRINTF_MSG,"Unknown request %s", parm);
	exit (-1); 
} 

