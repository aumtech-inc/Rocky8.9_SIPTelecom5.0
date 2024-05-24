/*-----------------------------------------------------------------------------
Program:	LOGXXCTL.c
Purpose:	ISP common logger control utilities
		FUNCTIONS
		LOGXXCTL()
		Log_Init()
		Log_SetConf() - set logging options from the config file
		Log_LoadMsg()
		Log_TagAppID()
		Log_SetMode()
		Log_Cleanup()
		Set_Log_Options_From_Keyword_List
		ISP_ApplicationId()
-----------------------------------------------------------------------------
Author:		J. Huang
Date:		03/23/96
Update:		05/06/94 J.Huang 1. Add Log_SetConf, Log_TagAppID 
Update:			 Set_Logopt and ISP_ApplicationId functions.
Update: 	06/01/94 J.Huang added Log_SetMode function.
Update:		06/29/94 J.Huang modified Set_Logopt() it does not rely on
Update:			   LOG_x_REMOTE & LOG_x_LOCAL.
Update:			   both Ropt and Lopt has a default string.
Update:		07/14/94 J.Huang use pEsc for process escalation file name.
Update:		07/28/94 J.Huang read blocking and timeout value from config.
Update:		02/27/96 G. Wenzel removed warning msg from Log_LoadMsg()
Update:		03/20/96 G. Wenzel cleaned up code and added comments
Update:		03/20/96 G. Wenzel removed LogDebug var, added #define DEBUG
Update:		03/20/96 G. Wenzel set additional defaults in Log_keyword_list
Update:		04/12/96 G. Wenzel changed ism/lm.h to lm.h 
Update:	11/24/98 mpb Added code to seperate GV_AppId into app name & Port 
Update:	2000/10/07 sja 4 Linux: Added function prototypes
-------------------------------------------------------------------------------
 * Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
 * All Rights Reserved.
 *
 * This is an unpublished work of AT&T ISP which is protected under 
 * the copyright laws of the United States and a trade secret
 * of AT&T. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of AT&T.
------------------------------------------------------------------------------*/
#include "ispinc.h"
#include "loginc.h"
#include "COMmsg.h"
#include "LOGmsg.h"
#include "CDRmsg.h"
#include "CEVmsg.h"
#include "NETmsg.h"
#include <string.h>

#undef DEBUG

static	char	MyModule[]="LOGXXCTL";

/* STDWRITE error messages.  */
static	char	EMSG_INIT[] = "001 - lm_init failed lm option=(%s)";
static	char	EMSG_RLOAD[]= "002 - Message set has been loaded.";
static	char	EMSG_XLOAD[]= "003 - Too many message sets have been loaded.";
static	char	EMSG_COM[]  = "004 - Load COM message set failed.";
static	char	EMSG_LOG[]  = "005 - Load LOG message set failed.";
static	char	EMSG_CDR[]  = "006 - Load CDR message set failed.";
static	char	EMSG_CEV[]  = "007 - Load CEV message set failed.";
static	char	EMSG_OATTR[]= "008 - Can't open Attribute file:%s ";
static	char	EMSG_RATTR[]= "009 - Log attribute read failed from file:%s";
static	char	EMSG_CMD[]  = "010 - Process has not been registered with common logger, invalid log control command - %d";
static	char	EMSG_NET[]  = "011 - Load NET message set failed.";

/*
 * Static Function Prototypes
 */
static int Log_Init(LOG_ReqParms_t *PT_logctl);
static int Log_SetConf(LOG_ReqParms_t *PT_logctl);
static int Set_Log_Options_From_Keyword_List(char *option);
static int Log_LoadMsg(LOG_ReqParms_t *PT_logctl);
static int Log_SetMode(LOG_ReqParms_t *PT_logctl, char *on_or_off);
static int Log_Cleanup(LOG_ReqParms_t *	PT_logctl);

/*-----------------------------------------------------------------------------
LOGXXCTL: entry point for all common logger control functions.
Input:	pointer of LOG_ReqParms_t structure
Output:
	ReturnCode:		LOG_ReqParms_t->StatusCode
        -----------             -----------------------
 	ISP_SUCCESS		(unset)
 	ISP_FAIL 		LOG_EINVALID_LOGCMD
 				(or set by individual function)
----------------------------------------------------------------------------*/
int LOGXXCTL(PT_logctl )
LOG_ReqParms_t *	PT_logctl;
{

	int	ret=ISP_SUCCESS;

	switch( PT_logctl->CommandId )
	{
	case	ISP_LOGCMD_INIT:
		ret=Log_Init( PT_logctl );
		break;

	case	ISP_LOGCMD_SETCONF:
		/* This must be called before Log_Init */
		ret=Log_SetConf( PT_logctl );
		break;

	case	ISP_LOGCMD_LOADMSG:
 
		ret=Log_LoadMsg( PT_logctl );
		break;

	case	ISP_LOGCMD_TAGMSG:
		if ( GV_LogInitialized == False )
		{
         		LOGXXPRT( MyModule, REPORT_STDWRITE, PRINTF_MSG,
 				EMSG_CMD, PT_logctl->CommandId );
			return( ISP_FAIL );
		}

		ret=Log_TagAppID( PT_logctl );
		break;

	case	ISP_LOGCMD_SETMODEON:
		if ( GV_LogInitialized == False )
		{
         		LOGXXPRT( MyModule, REPORT_STDWRITE, PRINTF_MSG,
 				EMSG_CMD, PT_logctl->CommandId );
			return( ISP_FAIL );
		}

		ret=Log_SetMode( PT_logctl, "ON" );
		break;

	case	ISP_LOGCMD_SETMODEOFF:
		if ( GV_LogInitialized == False )
		{
         		LOGXXPRT( MyModule, REPORT_STDWRITE, PRINTF_MSG,
 				EMSG_CMD, PT_logctl->CommandId );
			return( ISP_FAIL );
		}

		ret=Log_SetMode( PT_logctl, "OFF" );
		break;

	case	ISP_LOGCMD_SHUTDOWN:
		if ( GV_LogInitialized == False )
		{
         		LOGXXPRT( MyModule, REPORT_STDWRITE, PRINTF_MSG,
 				EMSG_CMD, PT_logctl->CommandId );
			return( ISP_FAIL );
		}

		ret=Log_Cleanup( PT_logctl );
		break;

	default:
		PT_logctl->StatusCode=LOG_EINVALID_LOGCMD;
		return( ISP_FAIL );
	}

	return( ret );
}
	
/*-----------------------------------------------------------------------------
Log_Init   	1. Initialize the log environment via lm_calls
		2. load COMmsg,  LOGmsg, CDRmsg & CEVmsg definitions

Input:
 	LOG_ReqParms_t->CommandId  (ISP_LOGCMD_INIT)
	LOG_ReqParms_t->PT_Object 
	LOG_ReqParms_t->PT_logid 

Output:
	ReturnCode:		LOG_ReqParms_t->StatusCode:
        -----------		------------------------
	ISP_SUCCESS		(unset)
	ISP_FAIL		LOG_EINIT_FAILED
----------------------------------------------------------------------------*/
static int Log_Init(LOG_ReqParms_t *PT_logctl)
{
	int 		i;
	MsgSet_t	mymsg;
	LOG_ReqParms_t	logload;

#if 0
	/* Define the iVIEW log id string */ 
        lm_log_id(GV_LogConfig.LogID);

	/* Set the callback function used to return the transaction id */
	lm_key_function( ISP_ApplicationId, MAX_APPID_LEN );

	/* Initialize the iVIEW logging options */
        if ( lm_init(PT_logctl->PT_LogOpt) == -1 )
	{
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, EMSG_INIT,
			PT_logctl->PT_LogOpt );
		PT_logctl->StatusCode=LOG_EINIT_FAILED;
           	return (ISP_FAIL);
	}
#endif

	for( i=0; i<MAX_MSGSETS; i++ ) { GV_LOG_MsgDB[i].max_defined=0; }

	logload.CommandId=ISP_LOGCMD_LOADMSG;
	logload.PT_MsgSet=&mymsg;
	
	/* Set the object from the passed parameter */
	strcpy(mymsg.object, PT_logctl->PT_Object);

	/* Load COM messages */
	strcpy(mymsg.prefix, "COM");
	mymsg.msgbase=COM_MSG_BASE;
	mymsg.version=COM_MSG_VERSION;
	mymsg.max_defined=GV_COM_msgnum;
	mymsg.msglist=GV_COMmsg;
        if ( LOGXXCTL( &logload ) == ISP_FAIL )
        {
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, EMSG_COM);
		return (ISP_FAIL);
	}

	/* Load LOG messages */
	strcpy(mymsg.prefix, "LOG");
	mymsg.msgbase=LOG_MSG_BASE;
	mymsg.version=LOG_MSG_VERSION;
	mymsg.max_defined=GV_LOG_msgnum;
	mymsg.msglist=GV_LOGmsg;
        if ( LOGXXCTL( &logload ) == ISP_FAIL )
        {
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, EMSG_LOG);
		return (ISP_FAIL);
	}
	
	/* Load CDR messages */
	strcpy(mymsg.prefix, "CDR");
	mymsg.msgbase=CDR_MSG_BASE;
	mymsg.version=CDR_MSG_VERSION;
	mymsg.max_defined=GV_CDR_msgnum;
	mymsg.msglist=GV_CDRmsg;
        if ( LOGXXCTL( &logload ) == ISP_FAIL )
        {
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, EMSG_CDR);
		return (ISP_FAIL);
	}

	/* Load CEV messages */
	strcpy(mymsg.prefix, "CEV");
	mymsg.msgbase=CEV_MSG_BASE;
	mymsg.version=CEV_MSG_VERSION;
	mymsg.max_defined=GV_CEV_msgnum;
	mymsg.msglist=GV_CEVmsg;
        if ( LOGXXCTL( &logload ) == ISP_FAIL )
        {
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, EMSG_CEV);
		return (ISP_FAIL);
	}

	/* Load NET messages */
	strcpy(mymsg.prefix, "NET");
	mymsg.msgbase=NET_MSG_BASE;
	mymsg.version=NET_MSG_VERSION;
	mymsg.max_defined=GV_NET_msgnum;
	mymsg.msglist=GV_NETmsg;
        if ( LOGXXCTL( &logload ) == ISP_FAIL )
        {
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, EMSG_NET);
		return (ISP_FAIL);
	}

	/* Indicate that log initialization is complete */
	GV_LogInitialized=True;
	return (ISP_SUCCESS);
}

/*-----------------------------------------------------------------------------
Log_SetConf	1) Populate the values of the Log_keyword_list by reading
		   the configuration file.
		2) Call Set_Log_Options_From_Keyword_List to
			a) set the values of the GV_LogConfig structure
			b) construct the iVIEW log option string required
			   by Log_Init
 
		Note: You must call this function before Log_Init.
 
Input:
	LOG_ReqParms_t->CommandId  (ISP_LOGCMD_LOADCONF)
	LOG_ReqParms_t->PT_UserData: configuration file path

Output:
	set LOG_ReqParms_t->PT_LogOpt (This is the iVIEW log options string.)

 	ReturnCode:		LOG_ReqParms_t->StatusCode:
        -----------		------------------------
 	ISP_SUCCESS		(unset)
 	ISP_FAIL		LOG_ELOAD_CONFIG
-----------------------------------------------------------------------------*/
enum{
	LOG_CENTRAL_MGR,
	LOG_ID,
	LOG_REPORT_NORMAL,
	LOG_REPORT_VERBOSE,
	LOG_REPORT_DEBUG,
	LOG_REPORT_SPECIAL,
	LOG_x_STDERR,
	LOG_x_STDOUT,
	LOG_x_REMOTE,
	LOG_PRIMARY_SERVER,
	LOG_x_LOCAL,
	LOG_x_LOCAL_FILE,
	LOG_RETENTION,
	LOG_BLOCK_INT,
	LOG_BLOCK_TIMEOUT,
};

static struct Logattr
{
	char Keyword[40];
	char Value[40];
}  Log_keyword_list[]=
	{
		{"LOG_CENTRAL_MGR",	"NO"},
		{"LOG_ID",		DEFAULT_LOG_ID},
		{"LOG_REPORT_NORMAL",	"ON"},
		{"LOG_REPORT_VERBOSE",	"OFF"},
		{"LOG_REPORT_DEBUG",	"OFF"},
		{"LOG_REPORT_SPECIAL",	"ON"},
		{"LOG_x_STDERR",	"OFF"},
		{"LOG_x_STDOUT",	"OFF"},
		{"LOG_x_REMOTE",	"ON"},	/* Must be ON,  we log remote */
		{"LOG_PRIMARY_SERVER",	"N/A"},
		{"LOG_x_LOCAL", 	"OFF"},	/* Must be OFF, we log remote */
		{"LOG_x_LOCAL_FILE", 	DEFAULT_ISP_LOG},
		{"LOG_RETENTION", 	"7"},
		{"LOG_BLOCK_INT", 	"20"},
		{"LOG_BLOCK_TIMEOUT", 	"1000"},
	};

static int num_log_keywords=sizeof( Log_keyword_list ) / (sizeof(struct Logattr));

static int Log_SetConf(LOG_ReqParms_t *PT_logctl)
{

	FILE	*fp;
	int 	i;
	char 	config_file[80];
	char	line[80];	/* config file input line */
	char	*pt_value;	/* pointer to the 2nd field, i.e. the value */
	char 	keyword[40];	/* keyword in the config file */

	/* Configuration file name is passed as the user data */
	strcpy(config_file,PT_logctl->PT_UserData);

        if( (fp = fopen(config_file, "r")) == NULL )
        {
      		LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, 
			EMSG_OATTR, config_file);
  		PT_logctl->StatusCode=LOG_ELOAD_CONFIG;
                return( ISP_FAIL );
        }

	while(fgets(line, sizeof(line), fp) != NULL) 
	{
		/* skip empty line or comments */
		if( (strlen(line) == 0) || (line[0] == '#') ) continue;

		/* get rid of '\n' */
		line[strlen(line)-1] = '\0'; 

		/* if line doesn't have an "=" ignore it */
		if( (pt_value=(char *) strchr( line, (int) '=')) == NULL)
			continue;

		/* we have a keyword to look up */
		strncpy(keyword, line, pt_value - line );
                keyword[pt_value - line]='\0';

#ifdef DEBUG
      		  printf("Log_SetConf keyword: <%s> Value: <%s>\n",keyword,pt_value+1);
#endif
		/* See if the keyword we found matches one in our list */
		/* If it does, update its value in the log keyword list */
		/* Otherwise, just ignore it. */
		for( i=0; i<num_log_keywords; i++ )
		{
			if( !strcmp(Log_keyword_list[i].Keyword, keyword) )
			{
				strcpy(Log_keyword_list[i].Value, ++pt_value);
				break;
			}
		}
	}
	fclose( fp );

	if( Set_Log_Options_From_Keyword_List( PT_logctl->PT_LogOpt )==ISP_FAIL)
	{
		PT_logctl->StatusCode=LOG_ESET_OPTION;
		return( ISP_FAIL );
	}
	return( ISP_SUCCESS );
}


/*----------------------------------------------------------------------------
Set_Log_Options_From_Keyword_List
		Construct the iVIEW log option string and populate 
		the GV_LogConfig structure.
Input:	 pointer to log option string
 
Output: ReturnCode  ISP_SUCCESS
---------------------------------------------------------------------------- */
#include <dirent.h>

#ifdef __hp9000s800
#	include <ism/release.h>
#else
#	include <libgen.h>
#endif

static int Set_Log_Options_From_Keyword_List(char *option)
{

	Int32 	mode=0;
	int 	i;
	Bool	RemoteSet=False;
	char 	Mylogid[10], Nopt[80], Vopt[80], Dopt[80], Sopt[80], 
		Ropt[80], Lopt[80], Fopt[80];

	char	NoKeyWord_Msg[]= "No key word found for %s using default.";

	Mylogid[0]=Nopt[0]=Vopt[0]=Dopt[0]=Sopt[0]=Ropt[0]=Lopt[0]='\0';

	strcpy( Ropt, " -R on " ); /* Log to a remote log file, default */
	strcpy( Lopt, " -R on " ); 

	for ( i=0; i<num_log_keywords; i++ )
	{
#ifdef DEBUG
      			printf("Log keyword=%s, value=%s\n", 
				Log_keyword_list[i].Keyword,
				Log_keyword_list[i].Value);
#endif	
		if ( !strcmp(Log_keyword_list[i].Value, "N/A" ) )
		{
		   /* Print out warning message, if key word value is missing */
		   /* Those key words whose initial value is not N/A
		    * are optional key words, e.g. log_x_remote/local
		    */
      		   LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, 
			NoKeyWord_Msg, Log_keyword_list[i].Keyword );
		}
		   
		switch (i) 
		{

		case	LOG_CENTRAL_MGR:
			if ( !strcmp(Log_keyword_list[i].Value, "YES" ) )
				GV_LogConfig.CentralMgr=True;
			else
				GV_LogConfig.CentralMgr=False;
			break;

		case	LOG_ID:

			strncpy(GV_LogConfig.LogID, 
				Log_keyword_list[i].Value, LOG_ID_LEN );

			sprintf(Mylogid, "-I %s", Log_keyword_list[i].Value);
			break;

		case    LOG_REPORT_NORMAL:

			if ( !strcmp(Log_keyword_list[i].Value, "ON" ) )
			{
				strcpy(Nopt, " -N on");
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode|REPORT_NORMAL;
			}
			else
				strcpy(Nopt, " -N off");
			break;
		
		case	LOG_REPORT_VERBOSE:

			if ( !strcmp(Log_keyword_list[i].Value, "ON" ))
			{
				strcpy(Vopt, " -V on");
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode|REPORT_VERBOSE;
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode|REPORT_DIAGNOSE;
			}
			else
			{
				#if LOG_NO_DEFAULT
				strcpy(Vopt, " -V off");
				#endif
			}
			break;

		case	LOG_REPORT_DEBUG:

			if ( !strcmp(Log_keyword_list[i].Value, "ON" ))
			{
				strcpy(Dopt, " -D on");
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode|REPORT_DEBUG;
			}
			else
			{
				#if LOG_NO_DEFAULT
				strcpy(Dopt, " -D off");
				#endif
			}
			break;

		case	LOG_REPORT_SPECIAL:

			if ( !strcmp(Log_keyword_list[i].Value, "ON" ) )
			{
				strcpy(Sopt, " -S on");
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode|REPORT_SPECIAL;
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode|REPORT_CDR;
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode|REPORT_CEV;
			}
			else
			{
				#if LOG_NO_DEFAULT
				strcpy(Sopt, " -S off");
				#endif
			}
			break;

		case	LOG_x_STDERR:
			if( !strcmp(Log_keyword_list[i].Value, "ON" ) )
			{
				GV_LogConfig.Log_x_stderr=True;
			   	if(ReportNormal_ON)  strcat(Nopt, " -E on");
			   	if(ReportVerbose_ON) strcat(Vopt, " -E on");
			   	if(ReportDebug_ON)   strcat(Dopt, " -E on");
			   	if(ReportSpecial_ON) strcat(Sopt, " -E on");
			}
			else
			{
				GV_LogConfig.Log_x_stderr=False;
				#if LOG_NO_DEFAULT
			   	if(ReportNormal_ON)  strcat(Nopt, " -E off");
			   	if(ReportVerbose_ON) strcat(Vopt, " -E off");
			   	if(ReportDebug_ON)   strcat(Dopt, " -E off");
			   	if(ReportSpecial_ON) strcat(Sopt, " -E off");
				#endif
			}
			break;


		case	LOG_x_STDOUT:
			
			if( !strcmp(Log_keyword_list[i].Value, "ON" ) )
			{
				GV_LogConfig.Log_x_stdout=True;
			   	if(ReportNormal_ON)  strcat(Nopt, " -O on");
			   	if(ReportVerbose_ON) strcat(Vopt, " -O on");
			   	if(ReportDebug_ON)   strcat(Dopt, " -O on");
			   	if(ReportSpecial_ON) strcat(Sopt, " -O on");
			}
			else
			{
				GV_LogConfig.Log_x_stdout=False;
				#if LOG_NO_DEFAULT
			   	if(ReportNormal_ON)  strcat(Nopt, " -O off");
			   	if(ReportVerbose_ON) strcat(Vopt, " -O off");
			   	if(ReportDebug_ON)   strcat(Dopt, " -O off");
			   	if(ReportSpecial_ON) strcat(Sopt, " -O off");
				#endif
			}
			break;

		case	LOG_x_REMOTE:
			

			if( !strcmp(Log_keyword_list[i].Value, "ON" ) )
			{
				RemoteSet=True;
				GV_LogConfig.Log_x_remote=True;
				/* use default -R option */
			}
			else
			{
				GV_LogConfig.Log_x_remote=False;
				strcpy( Ropt, " -R off" );
			   	
			}
			break;

		case	LOG_BLOCK_INT:
			{
				int interval;
				char buf[10];
				interval=atoi(Log_keyword_list[i].Value);
				sprintf(buf, " -i %d", interval);
				strcat( Ropt, buf );
				GV_LogConfig.Log_block_int=interval;
				break;
			}

		case	LOG_BLOCK_TIMEOUT:
			{
				int timeout;
				char buf[10];
				timeout=atoi(Log_keyword_list[i].Value);
				sprintf(buf, " -x %d", timeout);
				strcat( Ropt, buf );
				GV_LogConfig.Log_block_timeout=timeout;
				break;
			}

		case	LOG_PRIMARY_SERVER:
			/* assume the value is valid for now */
			strcpy(GV_LogConfig.Log_PrimarySrv, 
					Log_keyword_list[i].Value);

			break;

		case	LOG_x_LOCAL:
			
			/* The iVIEW -L option should not be used for this.
			 -L on with -f (filename) is for single process logging.
			 A file is created for each process and it grows to 
			 a new one when it reaches ITI_LM_RECOVER_MAX_COUNT */

			if( !strcmp(Log_keyword_list[i].Value, "ON" ) )
			{	
				/* use default -R option */
				GV_LogConfig.Log_x_local=True;
			}
			else
			{
				strcpy(Lopt, " -R off" );
				GV_LogConfig.Log_x_local=False;
			}
			
			break;

		case	LOG_x_LOCAL_FILE:
			
			/* This will also be the local escalation file if 
			  LOG_x_REMOTE is ON.  */

			if( strlen( Log_keyword_list[i].Value ) > 0 )
			{
				char *PT_dirname;
				DIR  *PT_dir;
				char filepath[80];

				/* save value, dirname seems to overwrite it */
				strcpy(filepath, Log_keyword_list[i].Value);

#ifndef __hp9000s800				
				PT_dirname=dirname( filepath );
#else
				PT_dirname=iti_dirname( filepath );
#endif					
				/* test if this is a valid dir */
#ifndef __hp9000s800
				if((PT_dir=opendir(PT_dirname))!=(DIR *) NULL )
#else
				if( (PT_dir=opendir(PT_dirname)) != NULL )
#endif
				{
				   strcpy(GV_LogConfig.Log_x_local_file,
				   Log_keyword_list[i].Value);

				   /* replace with process esc. file name */
				   sprintf(Fopt, " -f %s/pEsc", PT_dirname);
				   closedir( PT_dir );
				}
				else
				{
					LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, "Invalid LOG_x_LOCAL_FILE directory %s, use default %s\n", 
					Log_keyword_list[i].Value, GV_LogConfig.Log_x_local_file );
					sprintf(Fopt, " -f %s", 
					GV_LogConfig.Log_x_local_file);
				}
			}
			/* if empty path, use initialized default value */
			else
				sprintf(Fopt, " -f %s", 
					GV_LogConfig.Log_x_local_file);
					
			if ( GV_LogConfig.Log_x_remote == True )
				strcat( Ropt, Fopt );

			break;

		case	LOG_RETENTION:
			{
				int days;
				days=atoi(Log_keyword_list[i].Value);
				if ( (days < 1) || (days > 30) )
					LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, "Invalid LOG_RETENTION value, using default %d\n", 
					GV_LogConfig.Log_Retention );
				else
					GV_LogConfig.Log_Retention = days;

			}
			break;
		} /* end of switch */
	} /* end of for loop */

	if( GV_LogConfig.Log_x_remote == True )
	{
		if(ReportNormal_ON)  strcat(Nopt, Ropt);
		if(ReportVerbose_ON) strcat(Vopt, Ropt);
		if(ReportDebug_ON)   strcat(Dopt, Ropt);
		if(ReportSpecial_ON) strcat(Sopt, Ropt);
	}
	else
	{
		if(ReportNormal_ON)  strcat(Nopt, Lopt);
		if(ReportVerbose_ON) strcat(Vopt, Lopt);
		if(ReportDebug_ON)   strcat(Dopt, Lopt);
		if(ReportSpecial_ON) strcat(Sopt, Lopt);

	}
	sprintf(option, "%s%s%s%s%s", Mylogid, Nopt, Vopt, Dopt, Sopt);

#ifdef DEBUG
         	printf("iVIEW log option string: <%s>\n", option);
#endif

	return( ISP_SUCCESS );
}



/*----------------------------------------------------------------------------
Log_LoadMsg	Load message definition to ISP common logger memory.

Input:	LOG_ReqParms_t->CommandId  (ISP_LOGCMD_LOADMSG)
	LOG_ReqParms_t->PT_MsgSet 

Output:
	ReturnCode:		LOG_ReqParms_t->StatusCode:
	-----------		------------------------
	ISP_SUCCESS		(unset)
	ISP_WARN		LOG_ERELOAD
	ISP_FAIL		LOG_ETOOMANY_MSGSETS
---------------------------------------------------------------------------- */
static int Log_LoadMsg(LOG_ReqParms_t *PT_logctl)
{
	int 		i;
	MsgSet_t *	PT_newset=PT_logctl->PT_MsgSet;

	for( i=0 ; i <= MAX_MSGSETS; i++ )
	{
		/* find an empty slot */
		if( GV_LOG_MsgDB[i].max_defined == 0 )
		{
			strcpy(GV_LOG_MsgDB[i].object, PT_newset->object);
			strcpy(GV_LOG_MsgDB[i].prefix, PT_newset->prefix);
			GV_LOG_MsgDB[i].msgbase=PT_newset->msgbase;
			GV_LOG_MsgDB[i].version=PT_newset->version;
			GV_LOG_MsgDB[i].max_defined=PT_newset->max_defined;
			GV_LOG_MsgDB[i].msglist=PT_newset->msglist;
			return (ISP_SUCCESS);
		}

		if( GV_LOG_MsgDB[i].msgbase == PT_newset->msgbase )
		{
			/* Message set is already loaded; don't log anything */
			return (ISP_WARN);
		}
		continue;
	}

      	LOGXXPRT(MyModule, REPORT_STDWRITE, PRINTF_MSG, EMSG_XLOAD);
	PT_logctl->StatusCode=LOG_ETOOMANY_MSGSETS;
	return (ISP_FAIL);
}

/*----------------------------------------------------------------------------
Log_SetMode	Overwrite the default ISP reporting mode configuration.
		The configuration changed only affects to this process.
 
Input:	LOG_ReqParms_t->CommandId  (ISP_LOGCMD_SETMODEON/OFF)
 	LOG_ReqParms_t->PT_UserData (reporting mode mnemonic) 
Output: ReturnCode: ISP_SUCCESS or ISP_FAIL (with logged error message)
-----------------------------------------------------------------------------*/
static int Log_SetMode(LOG_ReqParms_t *PT_logctl, char *on_or_off)
{
	Int32	ISPmode=(Int32) PT_logctl->PT_UserData;
	int	value;
	int	rep_mode;
		
	char	LogLine[100];

	sprintf( LogLine, "Log_SetMode: Current reporting mode %o", 
			GV_LogConfig.ReportingMode );

	LOGXXPRT( MyModule, REPORT_DEBUG, COM_ISPDEBUG, LogLine );

#if 0
	if( !strcmp( on_or_off, "ON" ) )
		value=LM_ENABLED;
	else
		value=LM_DISABLED;
#endif

	rep_mode = ISPmode;
#if 0
	switch( ISPmode )
	{
		case REPORT_NORMAL:
			rep_mode=LM_NORMAL;
			break;

		case REPORT_VERBOSE:
			rep_mode=LM_VERBOSE;
			break;

		case REPORT_DIAGNOSE:
			/* if GV_LogConfig.ReportingMode for Verbose is ON
			 * don't issue lm_select to turn it off */
			if( value == LM_DISABLED && (ReportVerbose_ON) )
			{
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode & ~(REPORT_DIAGNOSE);
				return( ISP_SUCCESS );
			}

			rep_mode=LM_VERBOSE;
			break;

		case REPORT_DEBUG:
			rep_mode=LM_DEBUG;
			break;

		case REPORT_SPECIAL:
			rep_mode=LM_SPECIAL;
			break;

		case REPORT_CDR:
			/* if GV_LogConfig.ReportingMode for Special is ON
			 * don't issue lm_select to turn it off */
			if( value == LM_DISABLED && (ReportSpecial_ON) )
			{
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode & ~(REPORT_CDR);
				return( ISP_SUCCESS );
			}
			rep_mode=LM_SPECIAL;
			break;

		case REPORT_CEV:
			/* if GV_LogConfig.ReportingMode for Special is ON
			 * don't issue lm_select to turn it off */
			if( value == LM_DISABLED && (ReportSpecial_ON) )
			{
				GV_LogConfig.ReportingMode=
				GV_LogConfig.ReportingMode & ~(REPORT_CEV);
				return( ISP_SUCCESS );
			}
			rep_mode=LM_SPECIAL;
			break;

		default:
			LOGXXPRT(MyModule, REPORT_NORMAL, LOG_EREPORT_MODE, ISPmode );
			return( ISP_FAIL );
	}

	/* Now change the actual iVIEW option setting */
	/* lm_select is described in the iVIEW Programmer's Reference */
/* NOTE: Our logging option is always LM_REMOTE because LM_LOCAL does not
	provide file synchronization, that is, one process per log.  */

	if ( lm_select( rep_mode, LM_REMOTE, value, (char *) 0 ) == -1 )
	{
		LOGXXPRT(MyModule, REPORT_NORMAL, LOG_ELM_SELECT, 
			rep_mode, LM_REMOTE, value, "n/a" );
		return( ISP_FAIL );
	}

	/* If we log to stderr, apply the override to that logging option */
	if( GV_LogConfig.Log_x_stderr == True )
	{
		if ( lm_select( rep_mode, LM_STDERR, value, (char *) 0 ) == -1 )
		{
			LOGXXPRT(MyModule, REPORT_NORMAL, LOG_ELM_SELECT, 
				rep_mode, LM_STDERR, value, "n/a" );
			return( ISP_FAIL );
		}
	}

	/* If we log to stdout, apply the override to that logging option */
	if( GV_LogConfig.Log_x_stdout == True )
	{
		if ( lm_select( rep_mode, LM_STDOUT, value, (char *) 0 ) == -1 )
		{
			LOGXXPRT(MyModule, REPORT_NORMAL, LOG_ELM_SELECT, 
				rep_mode, LM_STDOUT, value, "n/a" );
			return( ISP_FAIL );
		}
	}

	/* If remote logging fails, we log to our local file, so 
	   apply the override to that logging option */
	if ( lm_select( rep_mode, LM_REMOTE_ESC_FILE, 0, GV_LogConfig.Log_x_local_file ) == -1 )
	{
		LOGXXPRT(MyModule, REPORT_NORMAL, LOG_ELM_SELECT, 
			rep_mode, LM_REMOTE_ESC_FILE, 0, GV_LogConfig.Log_x_local_file );
		return( ISP_FAIL );
	}

	
	/* Now, everything is fine, set ISP ReportingMode accordingly */

	if ( value == LM_ENABLED )
		GV_LogConfig.ReportingMode=GV_LogConfig.ReportingMode | ISPmode;
	else
		GV_LogConfig.ReportingMode=GV_LogConfig.ReportingMode & ~(ISPmode);
#endif
	GV_LogConfig.ReportingMode=GV_LogConfig.ReportingMode | ISPmode;

	sprintf( LogLine, "Log_SetMode: Set reporting mode to %o", 
			GV_LogConfig.ReportingMode );

		LOGXXPRT( MyModule, REPORT_DEBUG, COM_ISPDEBUG, LogLine );

	return ( ISP_SUCCESS );
}


/*----------------------------------------------------------------------------
Log_Cleanup	Clean up log environment before shutdown.
		This routine must be called to release resource allocated for
 		this object before terminate the object.

Input:
 	LOG_ReqParms_t->CommandId  (ISP_LOGCMD_SHUTDOWN)

	Output:
	ReturnCode:		LOG_ReqParms_t->StatusCode:
        -----------		------------------------
 	ISP_SUCCESS		(unset)
 	ISP_FAIL		
---------------------------------------------------------------------------- */
static int Log_Cleanup(LOG_ReqParms_t *	PT_logctl)
{
	lm_exit();
	return (ISP_SUCCESS);
}


/*-----------------------------------------------------------------------------
Log_TagAppID	Tag subsequent log messages with identifier.
 *		The identifier is specified in LOG_ReqParms_t->
 *		PT_UserData with the format of <application_name>.<resource>
 
Input:	LOG_ReqParms_t->CommandId  (ISP_LOGCMD_TAGMSG)
	LOG_ReqParms_t->PT_UserData

Output: Always returns ISP_SUCCESS, doesn't set LOG_ReqParms_t->StatusCode
---------------------------------------------------------------------------- */
int Log_TagAppID(PT_logctl)
LOG_ReqParms_t *PT_logctl;
{
char	*ptr1, *ptr2;

strncpy( GV_AppId, PT_logctl->PT_UserData, MAX_APPID_LEN );

ptr2 = GV_AppId;
ptr1 = strchr(GV_AppId, '.');
if(ptr1 != NULL)
	{
	strncpy(GV_Application_Name, GV_AppId, strlen(ptr2) - strlen(ptr1));
	ptr1++;
	sprintf(GV_Resource_Name, "%s", ptr1);
	}
else
	{
	sprintf(GV_Application_Name, "%s", GV_AppId);
	GV_Resource_Name[0] = '\0';
	}

	return( ISP_SUCCESS );
}

/*-----------------------------------------------------------------------------
ISP_ApplicationId	This is the callback function for iview to obtain 
			application instance specific information for the 
		 	"transaction ID" header field. The callback function
 			is specified in the Log_Init. GV_AppId global variable 
			is set, when an LOGXXCTL() function is called with 
			ISP_LOGCMD_TAGMSG.
			- ID_buf space is managed by iVIEW API.
----------------------------------------------------------------------------- */
void ISP_ApplicationId( ID_buf )
char *ID_buf;
{
	sprintf(ID_buf, "%s", GV_AppId );
}
