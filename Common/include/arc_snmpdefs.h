/*-----------------------------------------------------------------------------
File:		arc_snmpdefs.h
Purpose:	Group and Object defines for SNMP trap
 		generation.  Used by SNMP_SendAlarm() and any
 		routine which calls it.  
Author:		Dan Barto
Date:		06/13/97
Update:		08/14/97 G. Wenzel changed SNMP_SendAlarm to SNMP_SendTrap
Update:		08/19/02 djb	Added the SNMP_Request defines.
 *---------------------------------------------------------------------------*/

/* Aumtech SNMP Group (Node) Definitions */
#define		SNMPG_TEL		1
#define		SNMPG_TCP		2
#define		SNMPG_SNA		3
#define		SNMPG_WSS		4
#define		SNMPG_NET		5
#define		SNMPG_DB		6
#define		SNMPG_CL		7
#define		SNMPG_CM		8
#define		SNMPG_VENDOR		9
#define		SNMPG_USER		10

#define		NUM_SNMPG		10

/* Aumtech SNMP Object (Leaf) Definitions */
#define		SNMPO_ARC		1
#define		SNMPO_APPL		2

/*---------------------------------------------------------------
 * The following are for the SNMP_Request API()
 *---------------------------------------------------------------*/
#define	SNMP_CLEAR_VAR          1
#define	SNMP_SET_VAR            2
#define	SNMP_GET_VAR            3
#define	SNMP_INCREMENT_VAR      4

#define TC_INBOUND_CALL_PRESENTED       "tcInboundCallsPresented"
#define TC_INBOUND_CALL_ANSWERED        "tcInboundCallsAnswered"
#define TC_INBOUND_CALL_REJECTED        "tcInboundCallsRejected"
#define TC_OUTBOUND_CALL_ATTEMPTED      "tcOutboundCallsAttempted"
#define TC_OUTBOUND_CALL_ANSWERED       "tcOutboundCallsAnswered"

#define SR_ATTEMPTED					"srAttempted" 
#define SR_MATCH						"srMatch"

/*---------------------------------------------------------------
 * Valid Node/Leaf combinations consist of either SNMPO_ARC or
 * SNMPO_APPL combined with any of the SNMPG_xxx #defines.
 * For example,
 *	SNMPG_TEL/SNMPO_ARC,
 *	SNMPG_TEL/SNMPO_APPL,
 *	SNMPG_WSS/SNMPO_APPL, and
 *	SNMPG_VENDOR/SNMPO_ARC are all valid.
 *---------------------------------------------------------------*/

/*	function prototype	*/
int SNMP_SendTrap(int, int, int, char *);
int SNMP_GetRequest(char *zDomain, char *zHost, char *zVariableName, int zPort,
	int *zValue, char *zErrorMsg);
int SNMP_SetRequest(char *zDomain, char *zHost, int zCommand,
	char *zVariableName, int zPort, int *zValue, char *zErrorMsg);
