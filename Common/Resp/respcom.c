/*--------------------------------------------------------------- 
Program:	respcom.c
Purpose:	Function to communcations with the responsibility subsystem 
		resp_get_port(): 
		resp_release_port():
		send_to_responsibility():
		recv_to_responsibility():
Author:  	Mahesh Joshi.
Date:  		06/05/96
Update:  	06/05/96 G. Wenzel added requestor to resp_get_port
Update:		08/02/96 M. Joshi commented "DEBUG:Application Releasing" mesg 
Update:	10/10/98 mpb Changed "NONE" to usage in resp_get_port. 
Update:	04/27/00 apj In recv_from_responsibility, if msgrcv fails with EINTR,
			give it a one more try.
Update:	08/12/00 sja Added memset to struct before sending / receiving
Update:	08/12/00 sja Added loop in recv_from_resp(). 10 times 
		w/ MSGQ_CHECK_SLEEP millisec sleep each time.
Update:	12/20/00 apj changed loop number 20 -> 50 in recv_from_responsibility.
Update:	04/17/01 mpb Added dynmgrType parameter in resp_get_port.
Update:	01/19/02 mpb Added rsm_get_port & rsm_rel_port.
-----------------------------------------------------------------------------*/
#include "BNS.h"
#include "ISPSYS.h"
/* #include "resp.h" */
#define ISP_FOUND	1
#define ISP_NOTFOUND	0
#define	DYNAMIC_MANAGER		5
#define ISP_DEBUG_NORMAL	1	/* log errors only */
#define ISP_DEBUG_DETAIL	2	/* log errors only */

/* milliseconds to sleep before checking on msg q for a message */
#define MSGQ_CHECK_SLEEP	200	

char	log_buf[1024];

extern	int	errno;
int	id;
int	flag=0;
/*-----------------------------------------------------------------------------
send_to_responsibility():
-----------------------------------------------------------------------------*/
int	send_to_responsibility(id, request, port_str, token1, token2)
int	id;				/* message queue id */
int	request;			/* REQUEST_APPLICATION / REQUEST_PORT */
char	*port_str;
char	*token1;
char	*token2;
{
char	ModuleName[]="send_to_responsibility";
int	ret_code;
Mesg	msg;
Mesg	*msgptr = &msg;

memset(&msg, 0, sizeof(Mesg));

msg.mesg_type = DYNAMIC_MANAGER;
sprintf(msg.mesg_data, "%d %d %s %s %s", 
				request, getpid(), port_str, token1, token2);

ret_code = msgsnd(id, msgptr, MSIZE-1, IPC_NOWAIT);
if (ret_code != 0)
	{
	sprintf(log_buf, "msgsnd() failed, Unable to send message to responsibility, errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return (-1);
	}
return (0);
} /* send_to_responsibility */

/*-----------------------------------------------------------------------------
recv_to_responsibility():
------------------------------------------------------------------------------*/
int	recv_from_responsibility(id, found, app, customer)
int	id;
int	*found;
char	*app;
char	*customer;
{
char	ModuleName[]="recv_from_responsibility";
Mesg	msg;
Mesg	*msgptr =  &msg;
int 	ret_code;
char	application[256], cust[256];
int	result;
int	index;

memset(&msg, 0, sizeof(Mesg));

msg.mesg_type = (long) getpid();

/* This loop checks the message queue 50 times for a response from 
 * responsibility. If there is no message on the msg queue, the loop sleeps
 * for MSGQ_CHECK_SLEEP milliseconds. If, after 50 iterations (i.e.: 10 second)
 * there is no response received from responsibility, then the loop returns an error. */

for(index = 0; index < 50; index++)
{
	/* Check the message queue for a response. Since IPC_NOWAIT is specified
	 * in the last parameter, the call will return immediately if no 
	 * message of our type is present on the queue.  */

	ret_code = msgrcv(id, msgptr, MSIZE-1, msgptr->mesg_type, IPC_NOWAIT);
	if(ret_code > 0)
	{	 /* Found a message. Get out of the loop.  */
		break;
	}

	/* No message found on the queue. Check the reason with errno. If
	 * errno is set to ENOMSG, then there's no message for us. Continue
 	 * waiting in the loop in this case. Else, break out of the loop.  */
	if((errno == ENOMSG) || (errno == EINTR))
	{
		util_sleep(0, MSGQ_CHECK_SLEEP);
		continue;
	}
	/* Some other error. Get out of the loop.  */
	break;
} /* for index =0 ; index < 50; index++ */

if (ret_code <= 0)
{
	sprintf(log_buf, "msgrcv() failed, Unable to receive message from responsibility, errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return (-1);
}
memset(application, 0, sizeof(application));
memset(cust, 0, sizeof(cust));

sscanf(msg.mesg_data, "%d %s %s", &result, application, cust);
*found = result;
sprintf(app, "%s", application);
sprintf(customer, "%s", cust);
return (0);
} /* recv_from_responsibility */

/*-----------------------------------------------------------------------------
request port from responsibility.
-----------------------------------------------------------------------------*/
int resp_get_port(int requestor, int *port, char *usage, char *dynmgrType)
{
char	ModuleName[]="resp_get_port";
int	id;
char	port_str[10];
char	customer[256];
int	found=-1;
/* extern	int	GV_TEL_PortNumber; */

sprintf(port_str, "%d", requestor);
/* initialise message queue */
if((id = msgget(TEL_DYN_MGR_MQUE, flag))<0)
	{
	sprintf(log_buf, "Can't get Message Queue errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return(-2);
	}
/* make request to responsibility */
if (send_to_responsibility(id, REQUEST_PORT, port_str, usage, "NONE")<0)
	{
	sprintf(log_buf, "failed to send the request to responsibility errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return (-3);
	}
/* wait for response */
if (recv_from_responsibility(id, &found, port_str, customer) < 0)
	{
	sprintf(log_buf, "failed to receive the response from responsibility errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return (-4);
	}
*port = atoi(port_str);
sprintf(dynmgrType, "%s", customer);
if (found == ISP_FOUND)
	return (0);
else
	return (-1);		/* no port available */
} /* resp_get_port */

/*-----------------------------------------------------------------------------
resp_release_port(): Release the port from responsibility.
-----------------------------------------------------------------------------*/
int resp_release_port(int port)
{
char	ModuleName[]="resp_release_port";
int	id;
char	port_str[10];

sprintf(port_str, "%d", port);

/* initialise message queue */
if((id = msgget(TEL_DYN_MGR_MQUE, flag))<0)
	{
	sprintf(log_buf, "Can't get Message Queue errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return(-1);
	}
/* make request to responsibility */
if (send_to_responsibility(id, RELEASE_PORT, port_str, "NONE", "NONE") < 0)
	{
	sprintf(log_buf, "Failed to send the request to responsibility errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return (-2);
	}
return (0);
} /* resp_release_port */
/*-----------------------------------------------------------------------------
rsm_release_port(): Release the port from responsibility.
-----------------------------------------------------------------------------*/
int rsm_release_port(int port)
{
char	ModuleName[]="rsm_release_port";
int	id;
char	port_str[10];

sprintf(port_str, "%d", port);

/* initialise message queue */
if((id = msgget(TEL_DYN_MGR_MQUE, flag))<0)
	{
	sprintf(log_buf, "Can't get Message Queue errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return(-1);
	}
/* make request to responsibility */
if (send_to_responsibility(id, RSM_RELEASE_PORT, port_str, "NONE", "NONE") < 0)
	{
	sprintf(log_buf, "Failed to send the request to responsibility errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return (-2);
	}
return (0);
}
/*-----------------------------------------------------------------------------
request port from responsibility.
-----------------------------------------------------------------------------*/
int rsm_get_port(int port)
{
char	ModuleName[]="rsm_get_port";
int	id;
char	port_str[10];
char	customer[256];
int	found=-1;

sprintf(port_str, "%d", port);
/* initialise message queue */
if((id = msgget(TEL_DYN_MGR_MQUE, flag))<0)
	{
	sprintf(log_buf, "Can't get Message Queue errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return(-2);
	}
/* make request to responsibility */
if (send_to_responsibility(id, RSM_REQUEST_PORT, port_str, "", "NONE")<0)
	{
	sprintf(log_buf, "failed to send the request to responsibility errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return (-3);
	}
/* wait for response */
if (recv_from_responsibility(id, &found, port_str, customer) < 0)
	{
	sprintf(log_buf, "failed to receive the response from responsibility errno = %d", errno);
    	LogMsg(ModuleName, ISP_DEBUG_NORMAL, log_buf);
	return (-4);
	}
if(found == 0)
	return (0);
else
	return (-1);		/* no port available */
} /* rsm_get_port */
