/*-----------------------------------------------------------------------------
Program:	monitor.h
Purose:		To provide defines used by the util_heartbeat and 
		send_to_monitor functions.
Author: 	Mahesh Joshi.
Date: 		12/11/96
Update: 	03/14/96 M. Joshi added CUSTOM_API
Update: 	04/12/96 G. Wenzel removed MAX_API - not needed
-----------------------------------------------------------------------------*/

#define MON_API		1	/* Display API including Mnemonic */
#define	MON_DATA	2	/* Data to be displayed (indented) */
#define MON_ERROR	3	/* For sending error string. Shows ERROR: */
#define MON_CUSTOM	4	/* For sending any single integer plus msg */
#define MON_STATUS	5	/* to change monitor status of application */

#define CUSTOM_API	9000	 

#define KEEP_ALIVE_UPDATE 300 /* kill application sec - ISP_MIN_HBEAT */

#ifdef SENDTOMONITOR
int send_to_monitor(int monitor_code, int api, char *api_and_parms);
#else
extern int send_to_monitor(int monitor_code, int api, char *api_and_parms);
#endif
/*
Examples of usage:

send_to_monitor(MON_API, TEL_ANSWERCALL, "msg");
GUI monitor: Translation of TEL_ANSWERCALL
2.0 monitor: msg.

send_to_monitor(MON_DATA, TEL_ANSWERCALL, "msg");
GUI monitor: Nothing
2.0 monitor: msg. (indented)

send_to_monitor(MON_ERROR, TEL_ANSWERCALL, "msg");
GUI monitor: Nothing
2.0 monitor: ERROR: msg.

send_to_monitor(MON_CUSTOM, 2, "msg");
GUI monitor: Nothing
2.0 monitor:  2 msg. 
*/
