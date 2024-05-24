/*-----------------------------------------------------------------------------
Program:	send_SNMP_trap.c
Purpose:	To send an ARC snmp trap (from the command line). 
Author: 	G. Wenzel
Date:		08/11/97
Update:		08/14/97 G. Wenzel changed SNMP_SendAlarm to SNMP_SendTrap
Update:		11/04/97 D. Barto  changed fprint(stderr to gaVarLog()    
Update:		09/14/98 mpb  changed NULL to "NULL"    
Update:	2000/10/07 sja 4 Linux: Added function prototypes
Update:	2001/06/12 mpb	Removed referances to SNMPG_X25 & X25.
-------------------------------------------------------------------------------
Copyright (c) 1996, 1997 Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include "arc_snmpdefs.h"

/*
 * Static Function Prototypes
 */
static int send_trap(char *group_str, char *object_str, int msg_id, char *msg) ;
static int convert_to_upper(char *s);

/*------------------------------------------------------------------------------
main():
------------------------------------------------------------------------------*/
main(int argc, char *argv[])
{
	int msg_id;
	static char	gaLogDir[256];
	static char	mod[]="main";
	int		rc;

	sprintf(gaLogDir,"%s/LOG", (char *)getenv("ISPBASE"));
	rc=gaSetGlobalString("$PROGRAM_NAME", argv[0]);
	rc=gaSetGlobalString("$LOG_DIR", gaLogDir);
	rc=gaSetGlobalString("$LOG_BASE", argv[0]);

	if ( argc != 5 )
	{
		gaVarLog(mod,0,
		    "Usage: %s <group> <object> <message id> <message text>",
						argv[0]);
		exit(1);
	}
	msg_id = atoi(argv[3]);
	if ( send_trap(argv[1], argv[2], msg_id, argv[4]) == 0) exit(0); 
	exit(1); /*Error */


}

/*-----------------------------------------------------------------------------
This function checks that parameters are valide and send the appropriate 
arc snmp trap.
-----------------------------------------------------------------------------*/
static int send_trap(char *group_str, char *object_str, int msg_id, char *msg) 
{
	#define UNDEFINED -99
	int i, ret;
	int group_id, object_id;
	static char	mod[]="send_trap";

	char *valid_group[]={	"TEL", 		"TCP", 		"SNA", 
				"WSS",		"NET",		"DB",
				"CL",		"CM",		"VENDOR",
				"USER",		"NULL"};

	char *valid_object[]={	"APP", 		"APPL",		"ARC", 
				"NULL"};


	int valid_group_id[] = {SNMPG_TEL, 	SNMPG_TCP,	SNMPG_SNA,
				SNMPG_WSS,	SNMPG_NET,	SNMPG_DB,
				SNMPG_CL,	SNMPG_CM,	SNMPG_VENDOR,
				SNMPG_USER};
	
	int valid_object_id[] = {SNMPO_APPL, 	SNMPO_APPL,	SNMPO_ARC};

	convert_to_upper(group_str);
	convert_to_upper(object_str);

	group_id = UNDEFINED;
	i=0;
	while ( strcmp(valid_group[i], "NULL"))
	{
		if ( !strcmp(group_str, valid_group[i]) )
		{
			group_id = valid_group_id[i];
			break;
		}
		i++;
	}
	if ( group_id == UNDEFINED )
	{
		gaVarLog(mod,0,"Invalid snmp group_id (%s) passed.", 
			group_str);
		return(-1);
	}

	object_id = UNDEFINED;
	i=0;
	while ( strcmp(valid_object[i], "NULL"))
	{
		if ( !strcmp(object_str, valid_object[i]) )
		{
			object_id = valid_object_id[i];
			break;
		}
		i++;
	}
	if ( object_id == UNDEFINED )
	{
		gaVarLog(mod,0,"Invalid snmp object id (%s) passed.", 
			object_str);
		return(-1);
	}


	ret = SNMP_SendTrap(group_id, object_id, msg_id, msg);
	if ( ret != 0 )
	{
		gaVarLog(mod,0,"Unable to send snmp_trap: %s %s %d %s",
			group_str, object_str, msg_id, msg);
	}
	return(ret);
}

static int convert_to_upper(char *s)
{
	while (*s != '\0') 
	{
		*s=(char)toupper(*s);
		s++;
	}
}
