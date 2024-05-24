/*-------------------------------------------------------------------
*
*	messages.h
*
*	This file contains all of the 2.1 SSIO Events.  These messages
*	will be assigned to the NET_ServiceId.
*
*
---------------------------------------------------------------------
*/

#ifndef __MESSAGES_H__
#define __MESSAGES_H__

/*-------------------------------------------------------
  Response
*/

#define ISP_MSG_RESP_PROG_UP 			1000  
/* A program has finished starting */
/* SSIO_BufferedData.NET_Requesterid = Pid */
  
#define ISP_MSG_RESP_LAUNCH			1001		
/* Start an application */
 
#define ISP_MSG_RESP_RELOAD			1002		
/* reload the relational database tables */

#define ISP_MSG_RESP_KILL_APP			1003
/* Kill an application */
/* SSIO_BufferedData.NET_Requesterid = Pid */

#define ISP_MSG_RESP_SHUT_APP			1004
/* Send a shutdown to a particular App*/
/* SSIO_BufferedData.NET_Requesterid = Pid */

#define ISP_MSG_RESP_START_APP			1005
/* A new event has arrived */
/* SSIO_BufferedData.NET_HostName = channel */
/* SSIO_BufferedData.UserDataLen = length */
/* SSIO_BufferedData.UserData = Info */

#define ISP_MSG_RESP_KILL_ALL			1006
/* Stop all running applications */

#define ISP_MSG_RESP_SHUT_ALL			1007
/* Stop all running applications */

#define ISP_MSG_RESP_RESOURCE			1008
/* Request a resource from resp */
/* SSIO_BufferedData.PointData.PointData_val =
   the ResourceUsage being requested */

#define ISP_MSG_RESP_RESOURCE_RELEASE		1009
/* releases a used resource.
   SSIO_BufferedData.PointData.NET_HostName = #
*/

#define ISP_MSG_RESP_DUMP			1010

#define ISP_MSG_RESP_START_MGR			1011
/* A new event has arrived */
/* SSIO_BufferedData.NET_HostName = channel */
/* SSIO_BufferedData.UserDataLen = length */
/* SSIO_BufferedData.UserData = Info */

#define ISP_MSG_RESP_SRV_APP			1012
/* A new event has arrived (from a event server) */
/* SSIO_BufferedData.NET_HostName = channel */
/* SSIO_BufferedData.UserDataLen = length */
/* SSIO_BufferedData.UserData = Info */

#define ISP_MSG_RESP_CHG_USAGE			1013
/* allows an ssio message to change the resource
   usage namename.
*/
/* SSIO_BufferedData.NET_HostName = channel */
/* SSIO_BufferedData.NET_ServiceName21 = New Usage */

#define ISP_MSG_RESP_REPORT			1014
/* A Process wants report Information*/
/* SSIO_BufferedData.EventOrCmdId=Report */

#define ISP_MSG_RESP_ACTIVE_RESOURCE		1015
/* Allows a process to say that it is actvie */
/* SSIO_BufferedData.NET_HostName = resource */

#define ISP_MSG_RESP_MGR_RESOURCE		1016
/* Allows a process to say that it is a dyn mgr */
/* SSIO_BufferedData.NET_HostName = resource */



/*-------------------------------------------------------
  Monitor
*/

				
#define ISP_MSG_MON_APP_START 			2000	
/* An application was started */
/* SSIO_BufferedData.NET_Requesterid = Pid */

#define ISP_MSG_MON_DIAG_ON			2001		
/* Tell an application to turn on its diag */
/* SSIO_BufferedData.NET_Requesterid = Pid */
/* pid of application to notify */

#define ISP_MSG_MON_DIAG_OFF			2002
/* Tell an application to turn off its diag */
/* SSIO_BufferedData.NET_Requesterid = Pid */
/* pid of application to notify */

#define ISP_MSG_MON_APP_HEART			2003
/* An Applications heartbeat 
   SSIO_BufferedData.NET_Requesterid = Pid
   SSIO_BufferedData.EventOrCmdId = Beatlevel
   SSIO_BufferedData.AttributeId=API making call
*/

#define ISP_MSG_MON_APP_DIED			2004		
/* An appliation terminated */
/* SSIO_BufferedData.NET_Requesterid = Pid */

#define ISP_MSG_MON_REPORT			2005
/* A Process wants report Information*/
/* SSIO_BufferedData.EventOrCmdId=Report */

#define ISP_MSG_MON_DUMP			2006
/* A Dump the MCT */

#define ISP_MSG_MON_MGR_START 			2007	
/* An Dynamic Manager was started */
/* SSIO_BufferedData.NET_Requesterid = Pid */

#define ISP_MSG_MON_RESOURCE			2008
/* notifies that a resource is static*/
/* SSIO_BufferedData.NET_HostName = resource */
/* SSIO_BufferedData.PointData.PointData_val = name */

/*-----------------------------------------------------

  Dialogic Manager 2500-2999 ( I probably don't need that many)

*/

#define ISP_MSG_DLG_ROUTE			2500
/* notifies the DLG_MGR that an app needs a route request */
/* SSIO_BufferedData.NET_ServiceName21 = route entries separated
                                    by a colon.

   Example:

     dt_route(devh, Channum, tsnum, mode);

     devh is not needed because that is what the Manager is for.

     channum:tsnum:mode would be placed in the SSIO.

     An sprintf of the following should be used:

     sprintf(SSIO_BufferedData.NET_ServiceName21,"%d:%d:%d",
                                            channum,
                                            tsnum,
                                            BOTH);

*/

#define ISP_MSG_DLG_UNROUTE			2501
/* SSIO_BufferedData.NET_ServiceName21 = route entries separated
                                    by a colon.

     sprintf(SSIO_BufferedData.NET_ServiceName21,"%d:%d:%d",
                                            channum,
                                            -1,
                                            BOTH);
*/

/*-------------------------------------------------------
  applications
*/

#define ISP_MSG_APP_SHUT 			4000	
/* Notify an application of shutdown */
  
#define ISP_MSG_APP_NETREG			4001		
/* Sending the registration Number */

#define ISP_MSG_APP_DIAG_ON			4002		
/* Request the App to toggle Diag mode on*/

#define ISP_MSG_APP_DIAG_OFF			4003		
/* Request the App to toggle Diag mode off*/

#define ISP_MSG_APP_START_RESOURCE_GRANT	4004	
/* Grant a START_APP request for resource */

#define ISP_MSG_APP_START_RESOURCE_REJECT	4005 
/* resource is already in use, plesae reject*/

/*-------------------------------------------------------
  Network services
*/

#define ISP_MSG_NET_REG 			5000	
/* Request Registration Number */

#define ISP_MSG_NET_DEREG 			5001	
/* Request DeRegistration */

#define ISP_MSG_NET_REPORT			5002		
/* Request Registration Report*/

#define ISP_MSG_NET_START			5003
/* Request Registration Report*/

/*-------------------------------------------------------
  Generic
*/

#define ISP_MSG_PING 				6000
/* A unacknowledged message */

#define ISP_MSG_REPORT_START			6001
/* First Report Message */

#define ISP_MSG_REPORT_ENTRY			6002
/* a report entry */

#define ISP_MSG_REPORT_STARTEND			6003
/* First and last message */

#define ISP_MSG_REPORT_END			6004		
/* Last report message */

#define ISP_MSG_DONE				6005
/* the requested action was done */	

#define ISP_MSG_BAD				6006
/* had an error performing the request */

/* 8000-8999 is reserved for SIOF */
 

/*-------------------------------------------------------------------
*
*	This section containst the various Heart Beat Levels
*
*
---------------------------------------------------------------------
*/

/* Commands used by Utlheart */

enum BeatLevels { 	

  ISP_HBEAT_START = 100,
  ISP_HBEAT_NORMAL,
  ISP_HBEAT_BLOCK,
  ISP_HBEAT_LONGBLOCK,
  ISP_HBEAT_RAPID,
  ISP_HBEAT_STOP,
  ISP_HBEAT_RESUME,
  ISP_HBEAT_CHANGEINT,
  ISP_HBEAT_SHUTDOWN,
  ISP_HBEAT_NETREG,
  ISP_HBEAT_DUMP,
  ISP_HBEAT_ONEWAY,
  ISP_HBEAT_TWOWAY,
  ISP_HBEAT_CHKMSGS,
  ISP_HBEAT_INITLOG,
  ISP_HBEAT_NEWEVENT

};


/*-------------------------------------------------------------------
*
*	This section containst the various Heart Beat Conditions
*
*
---------------------------------------------------------------------
*/

enum HeartConditions {

  ISP_HEART_HEALTH_START = 200,
  ISP_HEART_HEALTH_NORMAL,
  ISP_HEART_HEALTH_CONCERN,
  ISP_HEART_HEALTH_WARN,
  ISP_HEART_HEALTH_SERIOUS,
  ISP_HEART_HEALTH_CRITICAL,
  ISP_HEART_HEALTH_DEAD

};

#define MAX_HEARTCONDITIONS (ISP_HEART_HEALTH_DEAD - ISP_HEART_HEALTH_START + 1)
#define NTTL(x) ( x / (MAX_HEARTCONDITIONS - 2) )   /* Minutes */
#define ISP_BEAT_PULSE		1	/* Minutes */

/*-------------------------------------------------------------------
*
*	This section containst the default Beat Span
*
*
---------------------------------------------------------------------
*/

#define ISP_BEAT_SPAN_DEFAULT	6	/* Max Number of beats before death
					   under normal conditions 
					*/


#endif
