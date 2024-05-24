/*-----------------------------------------------------------------------------
Program:	LOGXXPRT.c
Purpose:	To log all messages from all systems. 
		This function provides an interface through which all messages
		can be logged. Currently it is dependent upon iVIEW, however,
		in the future this need not be the case.
		Return codes: ISP_SUCCESS, ISP_SERVOFF, ISP_WARN, ISP_FAIL
Author: 	J. Huang
Date:	06/01/96
Update:	07/22/94 J. Huang truncates messages if they are too long
Update:	05/05/94 J. Huang returns ISP_SERVOFF if requested rpt mode off
Update:	05/05/94 J. Huang added REPORT_STDWRITE mode
Update:	06/01/94 J. Huang added  REPORT_DIAGNOSE mode
Update:	04/12/96 G. Wenzel changed ism/lm.h to lm.h
Update:	06/05/96 G. Wenzel removed debug stmt before lm_message
Update:	08/07/97 G. Wenzel added SNMP processing
Update:	08/08/97 G. Wenzel changed name of trap file to match doc.
Update:	08/13/97 G. Wenzel added group_id_override parameter; made 
Update:		 alarm structure and check_and..() not static
Update:	08/14/97 G. Wenzel changed SNMP_SendAlarm SNMP_SendTrap
Update:	01/15/98 mpb added code to check if SNMP is on in Global.cfg file
Update:	11/23/98 mpb Changed strncmp NULL to 0.
Update:	11/23/98 gpw passes requested_mode, not mode to lm_message
Update:	2000/10/07 sja 4 Linux: Added function prototypes
Update: 02/01/2001 mpb Added getDefaultISPBase() routine.
Update: 02/13/2001 mpb Changed err_msg size from 512 to 1024.
Update: 06/12/2001 mpb Removed X25 and SNMPG_X25.
Update: 07/16/2010 rg,ddn Replaced localtime with thread safe localtime_r
-------------------------------------------------------------------------------
Copyright (c) 1996, 1997 Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdarg.h>
// #include <lm.h>
#include "ispinc.h"
#include "loginc.h"
#include "LOGmsg.h"
#include "COMmsg.h"
#include <time.h>
#include "monitor.h"
#include <string.h>

extern int errno;

/*
 * Static Function Prototypes
 */
static int convert_to_upper(char *s);

int LOGXXPRT(char *module, int requested_mode, 
			int MessageID, char *bogus, ... )
{
	va_list		ap;
	int			mode, version, i;
	char		*PT_format, *PT_object, *PT_prefix;
	char		msgbuf[MAXMSGBODY];
	char		MyModule[]="LOGXXPRT";
	Bool		logid_chged=False;
	char diag_mesg[1024]; /* gpw added */


#if 0
 
    if(module == NULL || bogus == NULL){
	  if(module){
        fprintf(stderr, " %s() module=%s or fmtstr=%p is NULL cannot continue, \n", __func__, module, bogus);
      } else {
        fprintf(stderr, " %s() module=%p or fmtstr=%p is NULL cannot continue, \n", __func__, module, bogus);
      }
      return 0;
    }

#endif 

	if( (requested_mode!=REPORT_STDWRITE) && (GV_LogInitialized==False) )
	{
		LOGXXPRT( MyModule, REPORT_STDWRITE, PRINTF_MSG, ""
 		"Not registered with common logger; logging to stdout\n");
		/* Change request mode so we can actually get the message */	
		requested_mode = REPORT_STDWRITE; 
	}

	switch ( requested_mode )
	{
	case REPORT_NORMAL:
		if(ReportNormal_ON)
		{
			mode=REPORT_NORMAL;
		}
		else 
		{
			return (ISP_SERVOFF);
		}
		break;
	case REPORT_VERBOSE:
		if(ReportVerbose_ON)
		{
			mode=REPORT_VERBOSE;
		}
		else 
		{
			return (ISP_SERVOFF);
		}
		break;
#if 0
	case REPORT_DEBUG:
		if(ReportDebug_ON)
			mode=REPORT_DEBUG;
		else 
			return (ISP_SERVOFF);
		break;
	   case REPORT_SPECIAL:
		if(ReportSpecial_ON)
			mode=REPORT_SPECIAL;
		else 
			return (ISP_SERVOFF);
		break;
#endif
	case REPORT_CDR:
		if(ReportCDR_ON)
		{
			mode=REPORT_SPECIAL;
//			lm_log_id(CALL_DETAIL_LOG_ID);
			logid_chged=True;
		}
		else 
		{
			return (ISP_SERVOFF);
		}
		break;
	case REPORT_CEV:
	case REPORT_EVENT:
		if(ReportCEV_ON)
		{
//			mode = LM_SPECIAL;
//			lm_log_id(CALL_EVENT_LOG_ID);
			logid_chged = True;
		}
		else 
		{
			return (ISP_SERVOFF);
		}
		break;

	case REPORT_DIAGNOSE:
		if(ReportDiagnose_ON)
		{
//			mode=LM_VERBOSE;
//			lm_log_id(DIAGNOSE_LOG_ID);
			logid_chged=True;
		}
		else 
		{
			return (ISP_SERVOFF);
		}
		break;

	case REPORT_STDWRITE:
		{
			char *PT_format;
			switch( MessageID)
			{
			  case PRINTF_MSG:
				va_start(ap, bogus);
				PT_format=va_arg(ap, char *);
				vsprintf(msgbuf, PT_format, ap);
				va_end(ap);
				return( STDWrite( module, msgbuf) );
			  default:
				return( STDWrite( module, 
				"REPORT_STDWRITE Unknown Message Format %d", MessageID));
			}
		}
		break;

	   default:
		LOGXXPRT(MyModule, REPORT_NORMAL, LOG_EREPORT_MODE, "", requested_mode );
		return (ISP_WARN);
	}


	for( i = 0; i < MAX_MSGSETS; i++ )
	{
	   if ( (MessageID >= GV_LOG_MsgDB[i].msgbase) && 
                (MessageID < (GV_LOG_MsgDB[i].msgbase + 
                              GV_LOG_MsgDB[i].max_defined) ) )
		{
			PT_object=GV_LOG_MsgDB[i].object;
			PT_prefix=GV_LOG_MsgDB[i].prefix;
			PT_format=GV_LOG_MsgDB[i].msglist[ MessageID - GV_LOG_MsgDB[i].msgbase ];
			version=GV_LOG_MsgDB[i].version;
			break;
		}
	}
	
	if ( i == MAX_MSGSETS )
	{
		LOGXXPRT(MyModule, REPORT_NORMAL, LOG_EMESSAGE_ID, "", MessageID );
		return (ISP_WARN);
	}

	va_start(ap, bogus);
		
	if(requested_mode == REPORT_CDR)
	{
//printf("RGDEBUG::%d::%s::requested_mode is REPORT_CDR\n", __LINE__, __FILE__);fflush(stdout);
		int i = 0;
		char tempBuf[512] = "";
		sprintf (msgbuf, "%s", bogus);
		for(i=0; i<6; i++)
		{
			PT_format=va_arg(ap, char *);
			if(PT_format)
			{
				strcat(msgbuf,":");
    			strcat(msgbuf, PT_format);
			}
			else
			{
				break;
			}
		}
	}
	else
	{
//printf("RGDEBUG::%d::%s::requested_mode is not REPORT_CDR\n", __LINE__, __FILE__);fflush(stdout);
    	vsprintf (msgbuf, bogus, ap);
	}

	if ( strlen(msgbuf) > MAXMSGLEN ) msgbuf[MAXMSGLEN] = '\0';

//printf("RGDEBUG::%d::%s::calling lm_message for <%s>\n", __LINE__, __FILE__, msgbuf);fflush(stdout);

	if (lm_message( requested_mode, PT_object, MessageID, module, version, 
                        "%.3s%05d- %s", PT_prefix, MessageID, msgbuf) == -1 )
	{
		return (ISP_FAIL);
	}
	va_end(ap);


	if( logid_chged == True )
	{
		lm_log_id(GV_LogConfig.LogID);
		logid_chged=False;
	}

	return (ISP_SUCCESS);
	
}

int STDWrite(char *module, char *message)
{    
	int  	ret;
    	time_t DateTime;	
    	int    CharCnt;		
    	size_t StrLength;
    	char   LogLine[133];
    	struct tm *PT_Time;
    	struct tm temp_PT_Time;
    	pid_t  ProcessID;

    	DateTime = time(NULL);		/* Obtain local date & time */
    	PT_Time = localtime_r(&DateTime, &temp_PT_Time);	/* Convert to structure */

    	CharCnt = strftime(LogLine,132,"%y.%j-%H:%M:%S-", PT_Time);
    
	ProcessID = getpid();
    	sprintf(LogLine+CharCnt,"PID=%d-%s-", ProcessID , module); 
    	CharCnt = strlen(LogLine);

    	sprintf(LogLine+CharCnt,"%s\n", message); 

    	if((ret = printf(LogLine)) == 0) return(ISP_FAIL);	

   	return(ISP_SUCCESS);
}


static int convert_to_upper(char *s)
{
	while (*s != '\0') 
	{
		*s=(char)toupper(*s);
		s++;
	}
}
