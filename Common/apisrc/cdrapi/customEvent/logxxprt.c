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
//#include <stdarg.h>
#include <lm.h>
#include "ispinc.h"
#include "loginc.h"
#include "LOGmsg.h"
#include "COMmsg.h"
#include <time.h>
#include "monitor.h"
#include "arc_snmpdefs.h"
#include <string.h>

/* Defines and structures for sending snmp traps */
#define MAX_SNMP_TRAPS 100
#define SNMP_TRAP_FILE "snmp_arc_traps.cfg"

struct arc_SNMP_trap
{
	int msg_id;
	int group_id;
	int object_id;
} Trap[MAX_SNMP_TRAPS+1]; 

extern int errno;

/*
 * Static Function Prototypes
 */
static int load_snmp_trap_array(void);
static int load_trap(int msg_id, char *group_str);
static int convert_to_upper(char *s);

int logxxprt(char *module, int requested_mode, 
			int MessageID, char *bogus, ... )
{
//	va_list		ap;
	int			mode, version, i;
	char		*PT_format, *PT_object, *PT_prefix;
	char		msgbuf[MAXMSGBODY];
	char		MyModule[]="LOGXXPRT";
	Bool		logid_chged=False;
	char diag_mesg[1024]; /* gpw added */
	char		P[256];
	char		Q[256];

	set_logparm_values(P, Q);

	switch ( requested_mode )
	{
	case REPORT_NORMAL:
		if(ReportNormal_ON)
		{
			mode=LM_NORMAL;
		}
		else 
		{
			return (ISP_SERVOFF);
		}
		break;
	case REPORT_VERBOSE:
		if(ReportVerbose_ON)
		{
			mode=LM_VERBOSE;
		}
		else 
		{
			return (ISP_SERVOFF);
		}
		break;
	case REPORT_DEBUG:
		if(ReportDebug_ON)
			mode=LM_DEBUG;
		else 
			return (ISP_SERVOFF);
		break;
	   case REPORT_SPECIAL:
		if(ReportSpecial_ON)
			mode=LM_SPECIAL;
		else 
			return (ISP_SERVOFF);
		break;
	case REPORT_CDR:
		if(ReportCDR_ON)
		{
			mode=LM_SPECIAL;
			lm_log_id(CALL_DETAIL_LOG_ID);
			logid_chged=True;
		}
		else 
		{
			return (ISP_SERVOFF);
		}
		break;
	case REPORT_CEV:
		mode = LM_SPECIAL;
		lm_log_id(CALL_EVENT_LOG_ID);
		logid_chged = True;
		break;

	case REPORT_DIAGNOSE:
		if(ReportDiagnose_ON)
		{
			mode=LM_VERBOSE;
			lm_log_id(DIAGNOSE_LOG_ID);
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
//				va_start(ap, bogus);
//				PT_format=va_arg(ap, char *);
//				vsprintf(msgbuf, PT_format, ap);
//				va_end(ap);
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
	
#if 0
	if ( i == MAX_MSGSETS )
	{
		LOGXXPRT(MyModule, REPORT_NORMAL, LOG_EMESSAGE_ID, "", MessageID );
		return (ISP_WARN);
	}
#endif
//	va_start(ap, bogus);
		
	if(requested_mode == REPORT_CDR)
	{
//printf("RGDEBUG::%d::%s::requested_mode is REPORT_CDR\n", __LINE__, __FILE__);fflush(stdout);
		int i = 0;
		char tempBuf[512] = "";
		sprintf (msgbuf, "%s", bogus);
		for(i=0; i<6; i++)
		{
//			PT_format=va_arg(ap, char *);
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
    	//vsprintf (msgbuf, bogus, ap);
    	sprintf (msgbuf, "%s", bogus);
	}

	if ( strlen(msgbuf) > MAXMSGLEN ) msgbuf[MAXMSGLEN] = '\0';

	if ( requested_mode == REPORT_NORMAL)
	{
		check_and_send_snmp_trap(MessageID, msgbuf, 0);
	}

//printf("RGDEBUG::%d::%s::calling lm_message for <%s>\n", __LINE__, __FILE__, msgbuf);fflush(stdout);

	if (lm_message( requested_mode, PT_object, MessageID, module, version, 
                        "%.3s%05d- %s", PT_prefix, MessageID, msgbuf) == -1 )
	{
		return (ISP_FAIL);
	}
//	va_end(ap);


	if( logid_chged == True )
	{
		lm_log_id(GV_LogConfig.LogID);
		logid_chged=False;
	}

	return (ISP_SUCCESS);
	
}


/*-----------------------------------------------------------------------------
This function loads the snmp trap array from the ARC snmp trap file. It loads
every line that has a "Y", ignoring blank and commented lines.  It does not
check for duplicate message numbers in the file. All error messages are written
to stdout. The full pathname of the file is a global variable.
Format of the snmp trap file is expected to be:
<message_id> <service> {Y|N}
e.g.
1001 TEL Y
-----------------------------------------------------------------------------*/
static int load_snmp_trap_array()
{
	char module[]="load_snmp_trap_array";
	FILE *fp;
	char *base_dir;
	char environ_var[]="ISPBASE";
	int done_reading=0;
	char buffer[512];
	char trap_path[128];
	int i;
	char  group_str[20], yes_no_str[20];
	int msg_id;

	memset(Trap, 0, sizeof(Trap));

	base_dir = getenv(environ_var);
	if ( base_dir == NULL )
	{
		LOGXXPRT(module, REPORT_NORMAL, COM_ISPDEBUG, "", 
		"Unable to read environment variable ISPBASE. No messages will generate SNMP traps.");
		return(-1);	
	}

	sprintf(trap_path,"%s/Global/Tables/%s", base_dir, SNMP_TRAP_FILE);

	if ((fp = fopen(trap_path, "r")) == NULL)
	{
		printf("LOGXXPRT: Failed to open trap file %s. errno=%d\n", trap_path, errno);
		return(-1);
	}

	while (!done_reading)
	{
		if (feof(fp))
		{
			fclose(fp);
			done_reading = 1;
		}
		if (ferror(fp))
		{
			fclose(fp);
			printf("LOGXXPRT: Error reading trap file %s. errno=%d\n", trap_path, errno);
			return(-1);
		}
		if ( fgets(buffer, 511, fp) == NULL)
			done_reading = 1;
		else
		if ( (buffer[0] != '#') && (buffer[0] != '\n') )
		{
			sscanf(buffer, "%d %s %s", 
				&msg_id, group_str,  yes_no_str);
			convert_to_upper(yes_no_str);
			if ( yes_no_str[0] == 'Y' ) 
			{
				convert_to_upper(group_str);
				load_trap(msg_id, group_str);
			}
		}
	}
	return(0);
}
/*-----------------------------------------------------------------------------
This function finds the first open position in the array and places the
message id, group id, and object id into that slot in the array. If no more
slots are available, a message is written to stdout and the message id is 
discarded. In this case, the discarded message id can never generate a trap.
-----------------------------------------------------------------------------*/

static int load_trap(int msg_id, char *group_str) 
{
	int i, position;

	char *valid_group[]={	"TEL", 		"TCP", 		"SNA", 
				"WSS",		"NET",		"DB",
				"CL",		"CM",		"VENDOR",
				"USER",		""};

	int valid_group_id[] = {SNMPG_TEL, 	SNMPG_TCP,	SNMPG_SNA,
				SNMPG_WSS,	SNMPG_NET,	SNMPG_DB,
				SNMPG_CL,	SNMPG_CM,	SNMPG_VENDOR,
				SNMPG_USER};


	position=0;

	while (Trap[position].msg_id != 0) position++;
	/* Now we have the position to load into */

	if (position >= MAX_SNMP_TRAPS)
	{
		printf("LOGXXPRT: Maximum trap messages (%d) already loaded. No trap will be sent for %s message %d.\n",
		MAX_SNMP_TRAPS, group_str, msg_id);
		return(0);
	}

	/* Set snmp_id */
	Trap[position].msg_id = msg_id;

	/* Sent group_id */ 
	Trap[position].group_id = 99; 	/* Assume unknown */
	i=0;
	while ( strcmp(valid_group[i], ""))
	{
		if ( !strcmp(group_str, valid_group[i]) )
		{
			Trap[position].group_id = valid_group_id[i];
			break;
		}
		i++;
	}

	/* Set object_id - it's always ARC */
	Trap[position].object_id = SNMPO_ARC;
}

static int convert_to_upper(char *s)
{
	while (*s != '\0') 
	{
		*s=(char)toupper(*s);
		s++;
	}
}
