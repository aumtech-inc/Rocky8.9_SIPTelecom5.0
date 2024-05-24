/*------------------------------------------------------------------------------
File:		ResourceMgr.c
Author:		Aumtech, Inc.

Purpose:	The Resource Manager (ResourceMgr) manages the allocation of
		resources, communicating to other processes via message queues.
		It performs the following functions:
		1. Tracks, allocates, and frees configured resources per
		   requests from processes
		2. Gives the current status of all resources, free and busy,
		   upon request.

		High level Functionality :
		1. On request from a calling process for a resource, the
		   resource manager allocates the first available resource and
		   marks it as busy.  If no resource is available, it sends
		   the message of RESM_RESOURCE_NOT_AVAIL to the calling
		   process.  If a resource is available, it sends back the
		   device name, and returns 0 (success)
		2. On request to free the resource, the resource manager makes
		   the resource free and marks it as free so that it could be
		   available to other users on request
		3. The Resource manager also provides a mechanism for
		   requesting processes to get the status of all configured
		   resources.

		The resource types managed by the resource manager are :
		1. TTS
		2. FAX
		3. SIR (Speech rec; device names take the form vrxBnCm)

Date:		10/28/98
Update:		06/17/99 gpw now gets RM's config file name from a call.
Update:		06/24/99 gpw fixed msgs; added license checks & log msgs.
Update:		07/27/99 mpb Reset pid,port,appName to when resource was freed.
Update:		08/19/99 gpw made debug message Verbose
Update:		04/17/00 apj added code to start cyclic search for free resource
			     from last given resource index+1.
Update:		05/01/00 apj get/set slot number of a resource in the structure
Update:		05/01/00 gpw Converted to use LogARCMsg.
Update:	01/20/02 djb Added voice port / communication to Resp logic.
Update:	01/20/02 djb Changed all #ifdef DEBUG to log to ISP.cur VERBOSE
Update:	01/22/02 djb Removed added verbose messages.
Update:	12/05/02 apj In sendMsgToQueue, return msgsnd failure irrespective of
					 errno value.
Update:	12/05/02 apj Changed msg send/receive log messages VERBOSE->DETAIL.
------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include "lmclient.h"   /* flexlm v9.5 */
#include "lmpolicy.h"   /* flexlm v9.5 */

#include "ResourceMgr.h"
#include "ispinc.h" 

extern	int	errno;

/*------------------------------------------------------------------------------
 * #define constants 
 *----------------------------------------------------------------------------*/
#define	SLEEP_BEFORE_EXIT	5
#define	MSGQUE_ATTEMPTS		10
#define DEVICE_SIZE		32
#define SUBTYPE_SIZE		32
#define TYPE_SIZE		6
#define	LAST_STATUS_FILE	".ResourceMgr.lastStatus"
#define	GET_FREE_INDEX		0
#define GET_BUSY_INDEX		1

#define FAX_FEATURE "FAX"	/* Licensed feature */
#define SIR_FEATURE "SIR"	/* Licensed feature */
#define TTS_FEATURE "TTS"	/* Licensed feature */

/* resource state */
#define RESM_FREE		0	/* resource is free */
#define RESM_BUSY		1	/* resource is busy */

#define RSM			"RSM" 	/* 3 character "object" id */
#define RM			"ResourceMgr"
#define RDT_RESERVED	1

#define RESM_MSG_BASE	 	7000	/* Log message base */
#define MSG_RECEIVED_REQUEST    RESM_MSG_BASE+1
#define MSG_CANT_FIND_RES       RESM_MSG_BASE+2
#define MSG_INVALID_REQUEST     RESM_MSG_BASE+3
#define MSG_RESOURCE_UNAVAILABLE RESM_MSG_BASE+4
#define MSG_FAILED_TO_OPEN      RESM_MSG_BASE+5
#define MSG_CANT_CREATE_QUEUE   RESM_MSG_BASE+6
#define MSG_NO_RESOURCES_LOADED RESM_MSG_BASE+7
#define MSG_CANT_OPEN_TMP_FILE  RESM_MSG_BASE+8
#define MSG_FAX_LICENSE_FAILURE RESM_MSG_BASE+9
#define MSG_SIR_LICENSE_FAILURE RESM_MSG_BASE+10
#define MSG_TTS_LICENSE_FAILURE RESM_MSG_BASE+11
#define MSG_INVALID_ENTRY       RESM_MSG_BASE+12
#define MSG_INVALID_ENTRY2      RESM_MSG_BASE+13
#define MSG_MALLOC_ERROR        RESM_MSG_BASE+14
#define MSG_DUPLICATE_ENTRY     RESM_MSG_BASE+15
#define MSG_CANT_WRITE          RESM_MSG_BASE+16
#define MSG_SHUTTING_DOWN       RESM_MSG_BASE+17
#define MSG_CANT_REMOVE_QUEUE   RESM_MSG_BASE+18
#define MSG_SIGNAL_RECEIVED     RESM_MSG_BASE+19
#define MSG_FAILED_TO_SEND_MSG  RESM_MSG_BASE+20
#define MSG_NO_RESOURCES_CONFIGED RESM_MSG_BASE+21
#define MSG_LICENSE_WARNING     RESM_MSG_BASE+22
#define MSG_LICENSE_INFO        RESM_MSG_BASE+23
#define MSG_UNABLE_TO_INIT		RESM_MSG_BASE+24
#define MSG_DEBUG				RESM_MSG_BASE+25

/*------------------------------------------------------------------------------
 * ResourceInfo contains all the information needed keep track of
 * the resource usage.
 *----------------------------------------------------------------------------*/
typedef struct
{
	int		status;				/* RESM_FREE or RESM_BUSY */
	pid_t	pid;				/* pid of requesting process */
	int		port;				/* port of requesting process */
	char	device[DEVICE_SIZE+1];		/* device name of resource */
	char	appName[64];			/* application name */ 
	char	slotNumber[10];			/* slot number of a resource */
	int		devicePort;				/* port translated from device */
	int		SRReserved;				/* Is this a SR reserved port */
} ResourceInfo;

/*------------------------------------------------------------------------------
 * ResourceTypes is used to provide mapping between the resource-type/subtype
 * from the resource.cfg file, and the ResourceInfo information.
 *----------------------------------------------------------------------------*/
typedef struct
{
	char		type[TYPE_SIZE+1];	 /* 'FAX', 'TTS', or 'SIR' */
	char		subType[SUBTYPE_SIZE+1]; /* subtype (user-defined) */
	int		numResources;		 /* # of resources available */
	int 		offsetOfLastAllocatedResource;
	ResourceInfo	*rInfo;		 /* array of deviceNames, etc */
} ResourceTypes;

/*------------------------------------------------------------------------------
 * Store the ResourceDefTab information
 *----------------------------------------------------------------------------*/
typedef struct
{
	int			port;
	char		device[32];
} ResourceDefTabInfo;

/*------------------------------------------------------------------------------
 * Global Variables
 *----------------------------------------------------------------------------*/
ResourceTypes			*gvResources; 		/* to be malloc'd */
static int				gvQueId;		/* message queue id  */
static	char			gvMsg[512]; 		/* used for logging only */
ResourceDefTabInfo		gvRDTInfo[97];

LM_HANDLE       *GV_lmHandle;

/*------------------------------------------------------------------------------
 * Function Prototypes
 *----------------------------------------------------------------------------*/
static int	processRequest(ResourceMsg *msg);
static int	processAllocate(char *type, char *subType,
				long pid, int port, char *appName);
static int	dumpStatus(long pid);
static int 	sendMsgToQueue(long pid, char *message);
static int	initResourceMgr();
static int	callocNPopulateTables();
static void	exitResourceMgr(int retCode);
static void	utilSleep(int seconds, int milliSeconds);
static int	cleanString(char *buf);
static int	deviceAlreadyExists(char *device, int typeIndex);
static void	interruptHandler();
static int	lastStatusStartup();
static int	lastStatusShutdown();
static void	printTables();
static void	freeUnusedBusyResources();
static int	getTableIndexes(int freeOrBusy, char *type, char *subType,
				long pid, char *device,
				int *typeIndex, int *deviceIndex);
static 	int arcValidFeature(char *, char *, int *, char *);
static int log(char *mod, int mode, int id, char *msg);

static int loadResourceDefTab();
static int myGetField(char delimiter, char *buffer,
							int fieldNum, char *fieldValue);
static int isStatusFree(int zType, int zDevice);
static int portToDxxx(int zPort, char *zDxxx);
static int DxxxToPort(char *zDxxx, int *zPort);


/*------------------------------------------------------------------------------
main routine
------------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
	ResourceMsg	msg;
	int		rc;
	static char	mod[]="main";
	int		i, j;
	char		buf[32];
	char		type[TYPE_SIZE+1];
	char		subType[SUBTYPE_SIZE+1];
	int		request;
	long		pid;

	rc=initResourceMgr();
	switch(rc)
	{
		case 0:	
		case RESM_NO_RESOURCES_CONFIG:	
			break;
		default:
			exitResourceMgr(rc);
	}
	printTables();

	/*
	 * If gvResources[0].type == NULL, then no resources are configured.
	 * Just send back RESM_NO_RESOURCES_CONFIG to all requests.
	 */
	if ( rc == RESM_NO_RESOURCES_CONFIG )
	{
		for (;;)
		{
			memset((ResourceMsg *)&msg, 0, sizeof(msg));
			rc=msgrcv(gvQueId, &msg, RESM_MSG_DATASIZE,
							RESM_MSG_TYPE, 0);
			rc=sscanf(msg.data,"%s %s %d %ld",
						type, subType, &request, &pid);
			sprintf(buf, "%d", RESM_NO_RESOURCES_CONFIG);
			if ((rc=sendMsgToQueue(pid, buf)) != 0)
			{
				break;
			}
		}
	}
	else
	{
		/* Wait for a request from the application. */
		for(;;)
		{
			memset((ResourceMsg *)&msg, 0, sizeof(msg));
			rc=msgrcv(gvQueId, &msg, RESM_MSG_DATASIZE,
							RESM_MSG_TYPE, 0);
			sprintf(gvMsg, "Request received: msg.data=<%s>.",
								msg.data);
			log(mod, REPORT_DETAIL, MSG_RECEIVED_REQUEST, gvMsg);
			freeUnusedBusyResources(); 

			if ((rc=processRequest(&msg)) != 0)
			{
				break;
			}
		} /* for */
	}

	exitResourceMgr(rc);
} /* END: main() */

/*-----------------------------------------------------------------------------
processRequest():
	Driver function to process each request received from the message
	queue.
------------------------------------------------------------------------------*/
int	processRequest(ResourceMsg *msg)
{
	static	char	mod[] = "processRequest";
	int		rc;

	char		type[TYPE_SIZE+1];
	char		subType[SUBTYPE_SIZE+1];
	int		request;
	long		pid=0;
	int		port;
	char		appName[64];
	char		device[DEVICE_SIZE+1];
	int		tIndex;
	int		dIndex;
	char		slotNumber[10];

	memset(slotNumber, 0, sizeof(slotNumber));
	rc=sscanf(msg->data,"%s %s %d %ld %d %s %s %s", type, subType, &request,
			&pid, &port, appName, device, slotNumber);
	switch (request)
	{
		case RESM_REQ_ALLOCATE:
			rc=processAllocate(type, subType, pid, port, appName);
			break;
		case RESM_REQ_FREE:
			rc=getTableIndexes(GET_BUSY_INDEX, type, subType, pid,
					device, &tIndex, &dIndex);
			if (rc == 0)
			{
				gvResources[tIndex].rInfo[dIndex].status=
								RESM_FREE;
				gvResources[tIndex].rInfo[dIndex].port=0;
				gvResources[tIndex].rInfo[dIndex].pid=0;
				sprintf(gvResources[tIndex].rInfo[dIndex].appName, "%s", "");
				/* Send release to responsibility, if need be. */
				if (gvResources[tIndex].rInfo[dIndex].SRReserved == RDT_RESERVED)
				{
					sprintf(gvMsg, "Port %d (%s) is RESERVED.  "
								"Sending release to responsibility.",
								gvResources[tIndex].rInfo[dIndex].devicePort,
								gvResources[tIndex].rInfo[dIndex].device);
					log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);

					rc = rsm_release_port(gvResources[tIndex].rInfo[dIndex].devicePort);
					sprintf(gvMsg, "%d = rsm_release_port(%d)",
							rc, gvResources[tIndex].rInfo[dIndex].devicePort);

					log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
				}
				else
				{
					sprintf(gvMsg, "Port %d (%s) is not RESERVED.  ",
						gvResources[tIndex].rInfo[dIndex].devicePort,
						gvResources[tIndex].rInfo[dIndex].device);
					log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
				}
			}
			printTables();
			rc=0;	/* force return code to success */
			break;
		case RESM_REQ_STATUS:
			rc=dumpStatus(pid);
			break;

		case RESM_SET_SLOT_NUMBER:
			rc=getTableIndexes(GET_BUSY_INDEX, type, subType, pid,
					device, &tIndex, &dIndex);
			if (rc == 0)
			{
				sprintf(gvResources[tIndex].rInfo[dIndex].slotNumber, 
							"%s", slotNumber);
			}
			else
			{
				sprintf(gvMsg, "Could not find a resource for request "
						"RESM_SET_SLOT_NUMBER with type<%s>, "
						"subtype<%s>, pid=%ld, device<%s>.", type, 
						subType, pid, device);
				log(mod,REPORT_VERBOSE,MSG_CANT_FIND_RES,gvMsg);

				/* Failure to set the time slot will at the
			  	   most cause the api to get the time slot
				   again for next request. So forcing
				   return code to 0. */
				rc = 0;
			}
			break;

		default:
			sprintf(gvMsg, 
			"Invalid request (%d) received for [%s,%s] from app "
			"<%s> running on port %d. pid=%ld. Request ignored.",
				request, type, subType, appName, port, pid);
			log(mod, REPORT_NORMAL, MSG_INVALID_REQUEST, gvMsg);
			rc=0;	/* force return code to success */
			break;
	}
	return(rc);

} /* END: processRequest() */

/*------------------------------------------------------------------------------
processAllocate():
	Driver routine to process a RESM_REQ_ALLOCATE request, which is
	a request to allocate a resource.
------------------------------------------------------------------------------*/
int	processAllocate(char *type, char *subType,
				long pid, int port, char *appName)
{
	static char	mod[]="processAllocateReq";
	int		rc;
	int		typeIndex;
	int		deviceIndex;
	char		buf[RESM_MSG_DATASIZE];

	rc=getTableIndexes(GET_FREE_INDEX, type, subType, 0, "",
						&typeIndex, &deviceIndex);
	sprintf(gvMsg, "%d=getTableIndexes(%d, %s, %s, 0, %s, %d, %d)",
			rc, GET_FREE_INDEX, type, subType, "", typeIndex, deviceIndex);
	log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
	if (rc == -1)
	{
		sprintf(gvMsg, "Resource [%s,%s] requested by app <%s> "
				"running on port %d with pid=%ld is not available.",
				type, subType, appName, port, pid);
		log(mod, REPORT_NORMAL, MSG_RESOURCE_UNAVAILABLE, gvMsg);

		sprintf(buf, "%d", RESM_RESOURCE_NOT_AVAIL);
		rc=sendMsgToQueue(pid, buf);
		return(rc);
	}

	/*
	 * Send device name back and mark it as busy
	 */
	sprintf(buf, "%s %s", gvResources[typeIndex].rInfo[deviceIndex].device,
			gvResources[typeIndex].rInfo[deviceIndex].slotNumber);
	if ((rc=sendMsgToQueue(pid, buf)) == -1)
	{
		return(rc);
	}
	
	gvResources[typeIndex].rInfo[deviceIndex].status=RESM_BUSY;
	gvResources[typeIndex].rInfo[deviceIndex].port=port;
	gvResources[typeIndex].rInfo[deviceIndex].pid=pid;
	sprintf(gvResources[typeIndex].rInfo[deviceIndex].appName, "%s",
								appName);

	return(0);
} /* END: processAllocate() */

/*------------------------------------------------------------------------------
dumpStatus():
	Dump status of all configured resources to a file.  Return the 
	file to the process which made the request.
------------------------------------------------------------------------------*/
int dumpStatus(long pid)
{
	static	char	mod[] = "dumpStatus";
	char		fileName[50];
	FILE		*fp;
	int		i,  j;
	int		rc;
	char		buf[128];
	char		currentStatus[8];

	sprintf(fileName, "/tmp/%s.%d", mod, pid);
	if ((fp = fopen(fileName, "w")) == NULL)
	{
		sprintf(gvMsg,"Failed to open <%s>. errno=%d.",
							 fileName, errno);
		log(mod, REPORT_NORMAL, MSG_FAILED_TO_OPEN, gvMsg);
		sprintf(buf, "%d", RESM_GENERAL_FAILURE);
		if ((rc=sendMsgToQueue(pid, buf)) != 0)
		{
			return(rc);
		}
		return(0);
	}

	for (i=0; gvResources[i].type[0] != '\0'; i++)
	{
		for (j=0; j<gvResources[i].numResources; j++)
		{
			if (gvResources[i].rInfo[j].status == RESM_BUSY)
			{
				sprintf(currentStatus, "%s", "BUSY");
			}
			else
			{
				sprintf(currentStatus, "%s", "FREE");
			}
			sprintf(buf,"%s %s %s %d %s %d %s\n",
					gvResources[i].type,
					gvResources[i].subType,
					gvResources[i].rInfo[j].device,
					gvResources[i].rInfo[j].pid,
					currentStatus,
					gvResources[i].rInfo[j].port,
					gvResources[i].rInfo[j].appName);
			fprintf(fp, "%s", buf);
		}
	}
	fclose(fp);
	rc=sendMsgToQueue(pid,fileName); 

	return(rc);
} /* dumpStatus() */

/*------------------------------------------------------------------------------
freeUnusedBusyResources():
	This verifies that the owners (pids) of the busy resources
	are still alive (resources are still in use).  If they are not
	alive, this routine:
		1) frees the resource that it held
		2) removes, if any, messages from the message queue,
		   which were sent to the process, but never picked up.

------------------------------------------------------------------------------*/
void	freeUnusedBusyResources()
{
	ResourceMsg	msg;
	long		pid;
	int		i, j;
	int		rc;
	static char	mod[]="freeUnusedBusyResources";

	for (i=0; gvResources[i].type[0] != '\0'; i++)
	{
		for (j=0; j<gvResources[i].numResources; j++)
		{
			if (gvResources[i].rInfo[j].status == RESM_BUSY)
			{
				sprintf(gvMsg, "[%s,%s][%d].rInfo[%d].status=%d, "
					"[%d].rInfo[%d].pid=%d, "
					"[%d].rInfo[%d].port=%d",
					gvResources[i].type,
					gvResources[i].subType,
					i, j, gvResources[i].rInfo[j].status,
					i, j, gvResources[i].rInfo[j].pid,
					i, j, gvResources[i].rInfo[j].port);
				log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
				
				if (kill(gvResources[i].rInfo[j].pid, 0) != 0)
				{
					gvResources[i].rInfo[j].status =
								RESM_FREE;
					pid=gvResources[i].rInfo[j].pid;
					gvResources[i].rInfo[j].port=0;
					gvResources[i].rInfo[j].pid=0;
					sprintf(gvResources[i].rInfo[j].appName, "%s", "");

					/* Send release to responsibility, if need be. */
					if (gvResources[i].rInfo[j].SRReserved == RDT_RESERVED)
					{
						sprintf(gvMsg, "Port %d (%s) is RESERVED.  "
							"Sending release to responsibility.",
							gvResources[i].rInfo[j].devicePort,
							gvResources[i].rInfo[j].device);
						log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);

						rc = rsm_release_port(gvResources[i].rInfo[j].devicePort);
						sprintf(gvMsg, "%d = rsm_release_port(%d)",
							rc, gvResources[i].rInfo[j].devicePort);
						log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
					}
					else
					{
						sprintf(gvMsg, "Port %d (%s) is not RESERVED.  ",
							gvResources[i].rInfo[j].devicePort,
							gvResources[i].rInfo[j].device);
						log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
					}

					/* 
					 * Throw away all stranded messages 
					 * associated of type 'pid'.
					 */
					rc=0;
					while (rc == 0)
					{
						rc=msgrcv(gvQueId,
							&msg,
							RESM_MSG_DATASIZE,
							pid,
							IPC_NOWAIT);


						memset((char *)msg.data, 0, sizeof(msg.data));
						msg.type=0;
						sprintf(gvMsg, "Stranded msg: "
							"%d=msgrcv(%d,(%d,%s), %d, %d, %d). errno=%d",
							rc, gvQueId, msg.type, msg.data,
							RESM_MSG_DATASIZE, pid, IPC_NOWAIT, errno);
						log(mod, REPORT_DETAIL, MSG_DEBUG, gvMsg);
					}
				}
			}
		}
	}
	return;
} /* END: freeUnusedBusyResources() */

/*------------------------------------------------------------------------------
initResourceMgr():
	* Set the signal handling function
	* Initialize the tables;
	* Call setupTables() to load the resourcecfg file;
	* Verify the maximum resources licensed
	* Call lastStatusStartup() to determine if any of the resources which
		were in use when ResourceMgr shut down, are still being used.
		If so, load them in the gvInfoTable and mark them as busy.
	* load the ResourceDefTab to find all the RESERVED ports
	* mark all SR reserved ports in the structure
------------------------------------------------------------------------------*/
int	initResourceMgr()
{
	static char	mod[]="initResourceMgr";
	int		rc;
	int		i;
	int		j;
	int		k;

	sigset( SIGHUP, (void (*)()) interruptHandler );
	sigset( SIGINT, (void (*)()) interruptHandler );
	sigset( SIGQUIT, (void (*)()) interruptHandler );
	sigset( SIGTERM, (void (*)()) interruptHandler );

	rc=callocNPopulateTables();
	switch(rc)
	{
		case 0:	
			(void)lastStatusStartup();
			break;
		case RESM_NO_RESOURCES_CONFIG:	
			break;
		default:
			return(-1);
			break;
	}

	/*
	 * Mark SR Reserved ports 
	 */
	if ((rc = loadResourceDefTab()) == -1)
	{	
		return(-1);		/* message is logged in routine */
	}

	for (i = 0; gvResources[i].type[0] != '\0'; i++)
	{
		if ((strcmp(gvResources[i].type, SIR_FEATURE)) != 0)
		{
			continue;
		}

		for (j=0; j< gvResources[i].numResources; j++)
		{
			for (k=0; gvRDTInfo[k].device[0] != '\0'; k++)
			{
				if ((strcmp(gvResources[i].rInfo[j].device,
									gvRDTInfo[k].device)) != 0)
				{
					continue;
				}
				gvResources[i].rInfo[j].devicePort = gvRDTInfo[k].port;
				gvResources[i].rInfo[j].SRReserved = RDT_RESERVED;
			}
		}
	}

	/*
	 * Create the message queue
	 */
	for (i=0; i<5; i++)
	{
		if((gvQueId = msgget(RESM_MSG_QUE, RESM_PERMS|IPC_CREAT)) < 0)
		{
			sprintf(gvMsg, 
					"Unable to create the message queue. errno=%d.", errno);
			log(mod, REPORT_NORMAL, MSG_CANT_CREATE_QUEUE, gvMsg);
			sleep(60);
			continue;
		}
		return(rc);
	}

	return(-1);
} /* END: initResourceMgr() */

/*------------------------------------------------------------------------------
int loadResourceDefTab():
	Load the RESERVED usage lines from the ResourceDefTab file into the 
	ResourceDefTabInfo structure.

	Translate the port to the device name.
------------------------------------------------------------------------------*/
static int loadResourceDefTab()
{
	FILE			*fp;
	char			record[128];
	char			*ptr;
	char			temp_buf[128];
	char			*baseDirPtr;
	static char		mod[]="loadResourceDefTabInfo";
	char			resource_file[128];
	int				rc;
	int				counter;

	if((baseDirPtr = (char *) getenv("ISPBASE")) == NULL)
	{
		sprintf(gvMsg,
			"Unable to get environment variable ISPBASE. [%d, %s]",errno, 
					strerror(errno));
		log(mod, REPORT_NORMAL, MSG_UNABLE_TO_INIT, gvMsg);
		return(-1);
	}

	sprintf(resource_file, "%s/Telecom/Tables/ResourceDefTab", baseDirPtr);
	if((fp = fopen(resource_file, "r+")) == NULL)
	{
		sprintf(gvMsg, "Failed to open file <%s>. [%d, %s].",
					resource_file, errno, strerror(errno));
		log(mod, REPORT_NORMAL, MSG_UNABLE_TO_INIT, gvMsg);
		return(-1);
	}
	
	memset((ResourceDefTabInfo *) &(gvRDTInfo[0]), 0, sizeof(gvRDTInfo));
	counter = 0;
	while(fgets(record, sizeof(record), fp) != NULL)
	{
		rc = myGetField('|', record, 4, temp_buf);
		if (strcmp(temp_buf, "RESERVED") != 0)
		{
			continue;
		}
		rc = myGetField('|', record, 1, temp_buf);
		gvRDTInfo[counter].port = atoi(temp_buf);

		rc = portToDxxx(gvRDTInfo[counter].port, gvRDTInfo[counter].device);
		counter++;
	}

	fclose(fp);

	for (counter = 0; gvRDTInfo[counter].device[0] != '\0'; counter++)
	{
		sprintf(gvMsg, "Port (%d:%s) in (%s) is RESERVED.",
				gvRDTInfo[counter].port, gvRDTInfo[counter].device,
				resource_file);
		log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
	}

	return(0);
} /* END: loadResourceDefTab() */

/*------------------------------------------------------------------------------
int portToDxxx():
	Translate a port number to a dxxxBxCx.
------------------------------------------------------------------------------*/
static int portToDxxx(int zPort, char *zDxx)
{
	int		yBoardNum;
	int		yChannelNum;

	yBoardNum = zPort / 4 + 1;
	yChannelNum = zPort % 4 + 1;

	sprintf(zDxx, "dxxxB%dC%d", yBoardNum, yChannelNum);

	return(0);
} /* END: portToDxxx() */

/*------------------------------------------------------------------------------
int DxxxToPort():
	Translate a dxxx to a port number.
------------------------------------------------------------------------------*/
static int DxxxToPort(char *zDxxx, int *zPort)
{
	char	yTmpBuf[32];
	int		yCounter;
	int		yTmpCounter;
	int		yBoardNum;
	int		yChannelNum;

	/* Parse the board from the zDxxx string */
	memset((char *)yTmpBuf, 0, sizeof(yTmpBuf));
	yTmpCounter = 0;
	for (yCounter = 4; zDxxx[yCounter] != '\0'; yCounter++)
	{
		if (zDxxx[yCounter] == 'C')
		{
			break;
		}
		yTmpBuf[yTmpCounter] = zDxxx[yCounter];
	}
	yBoardNum = atoi(yTmpBuf);
	
	/* Parse the channel from the zDxxx string */
	memset((char *)yTmpBuf, 0, sizeof(yTmpBuf));
	yTmpCounter = 0;
	for (yCounter = 4; zDxxx[yCounter] != '\0'; yCounter++)
	{
		yTmpBuf[yTmpCounter] = zDxxx[yCounter];
	}
	yChannelNum = atoi(yTmpBuf);
	
	/* Formula is  port = (board - 1) * 4 + channel - 1 */
	*zPort = (yBoardNum - 1) * 4 + yChannelNum - 1;

	return(0);
} /* END: DxxxToPort() */

/*------------------------------------------------------------------------------
int callocNPopulateTables():
	Populate the gvResource table by reading the configuration file
	This routine first calloc's the space for the table, and thus has 
	to read the entire file to determine the sizes to allocate.  It 
	creates a temporary file which holds only validated records from the 
	configuration file.

	This performs all validation on the configuration file including
	licensing.
------------------------------------------------------------------------------*/
int callocNPopulateTables()
{
	static char mod[] = "callocNPopulateTables";
	static char license_fail_format[300]="Failed to get license for %s. [%s]";
	static char malloc_error_fmt[300]="Failed to allocate memory. errno=%d.";
	char		buf[256];
	int		numTypes=0;
	char		type[TYPE_SIZE+1];
	char		subType[SUBTYPE_SIZE+1];
	char		device[DEVICE_SIZE+1];
	int		i, j, try;
	int		lineNumber;
	int		rc;
	int		licensedFAXResources;	
	int		licensedSIRResources;	
	int		licensedTTSResources;	
	int		numFAXResources=0;	
	int		numSIRResources=0;
	int		numTTSResources=0;

	char		fileName[256];
	char		err_msg[256];
	char		tmpFileName[256];
	FILE		*fp;
	FILE		*fpTmp;

	struct TmpHolder
	{
		char	type[TYPE_SIZE+1];
		char	subType[DEVICE_SIZE+1];
		int	numResources;
	} tmpHolder[16];

	memset((struct TmpHolder *)tmpHolder, 0, sizeof(tmpHolder));

	/* Determine name of ResourceMgr's configuration file. */
	rc =util_get_cfg_filename("RESOURCE_CONFIG", fileName, err_msg);	
	if (rc != 0)
	{
		sprintf(gvMsg, "%s. No resources were loaded.", err_msg);
		log(mod, REPORT_NORMAL, MSG_NO_RESOURCES_LOADED, gvMsg);
		return(RESM_NO_RESOURCES_CONFIG);
	}

	sprintf(tmpFileName, "/tmp/%s.%d", mod, (char *)getpid());

	if ((fp = fopen(fileName, "r")) == NULL)
	{
		sprintf(gvMsg,"Failed to open temporary file <%s>. errno=%d."
			" No resources were loaded.", fileName, errno);
		log(mod, REPORT_NORMAL, MSG_CANT_OPEN_TMP_FILE, gvMsg);
		/*
		 * Create one entry to guard against core dumps
		 */
		gvResources= (ResourceTypes *) calloc(1, sizeof(ResourceTypes));
		gvResources[0].numResources=0;
		return(RESM_NO_RESOURCES_CONFIG);
	}
	if ((fpTmp = fopen(tmpFileName, "w+")) == NULL)
	{
		sprintf(gvMsg,
			"Failed to open temporary file <%s>.  errno=%d. "
			"No resources were loaded.", tmpFileName, errno);
		log(mod, REPORT_NORMAL, MSG_CANT_OPEN_TMP_FILE, gvMsg);
		/*
		 * Create one entry to guard against core dumps
		 */
		gvResources= (ResourceTypes *) calloc(1, sizeof(ResourceTypes));
		gvResources[0].numResources=0;
		return(RESM_NO_RESOURCES_CONFIG);
	}
	rc = arcValidFeature(FAX_FEATURE, "6.1", &licensedFAXResources, err_msg);
	if (rc != 0)
	{
		sprintf(gvMsg, license_fail_format, FAX_FEATURE, err_msg);
		log(mod, REPORT_VERBOSE, MSG_FAX_LICENSE_FAILURE, gvMsg);
	}
	rc = arcValidFeature(TTS_FEATURE, "1.0", &licensedTTSResources, err_msg);
	if (rc != 0)
	{
		sprintf(gvMsg, license_fail_format, TTS_FEATURE, err_msg);
		log(mod, REPORT_VERBOSE, MSG_TTS_LICENSE_FAILURE, gvMsg);
	}

	rc = arcValidFeature(SIR_FEATURE, "1.0", &licensedSIRResources, err_msg);
	if (rc != 0)
	{
		sprintf(gvMsg, license_fail_format, SIR_FEATURE, err_msg);
		log(mod, REPORT_VERBOSE, MSG_SIR_LICENSE_FAILURE, gvMsg);
	}

	memset((char *)buf, 0, sizeof(buf));
	while(fgets(buf, sizeof(buf), fp) != NULL)
	{
		++lineNumber;
		if (buf[0]=='#' || buf[0]=='\n') 
		{
			continue;
		}

		cleanString(buf);
		
		memset((char *)type, 0, sizeof(type));
		memset((char *)subType, 0, sizeof(subType));
		memset((char *)device, 0, sizeof(device));
		rc=sscanf(buf, "%[^|]|%[^|]|%s", type, subType, device);

		if (rc != 3)
		{
			sprintf(gvMsg, "Invalid entry <%s> on line %d in <%s> ignored.",
				buf, lineNumber, fileName);
			log(mod, REPORT_NORMAL, MSG_INVALID_ENTRY, gvMsg);
			memset((char *)buf, 0, sizeof(buf));
			continue;
		}

		if (  (strcmp(FAX_FEATURE, type) != 0) &&
		      (strcmp(TTS_FEATURE, type) != 0) &&
		      (strcmp(SIR_FEATURE, type) != 0) )
		{
			sprintf(gvMsg, "Invalid entry <%s> on line %d in <%s> ignored. "
				"First field must be FAX, TTS, or SIR.",
				buf, lineNumber, fileName);
			log(mod, REPORT_NORMAL, MSG_INVALID_ENTRY2, gvMsg);
			continue;
		}
			
		/*
		 * Check if type/subType has not been calloc'd; if not, set it.
		 */
		for (i=0; i<numTypes; i++)
		{
			if ( (strcmp(type, tmpHolder[i].type) == 0)   &&
			     (strcmp(subType, tmpHolder[i].subType) == 0) )
			{
				break;
			}
		}
		if (i == numTypes)
		{			/* not allocated yet */
			sprintf(tmpHolder[i].type, "%s", type);
			sprintf(tmpHolder[i].subType, "%s", subType);
			numTypes+=1;
		}

		if (strcmp(tmpHolder[i].type, FAX_FEATURE) == 0)
		{
			++numFAXResources;
			if (numFAXResources > licensedFAXResources)
			{
				continue;	/* This resource is ignored */
			}
			tmpHolder[i].numResources+=1;
		}
		else if (strcmp(tmpHolder[i].type, TTS_FEATURE) == 0)
		{
			++numTTSResources;
			if (numTTSResources > licensedTTSResources)
			{
				continue;	/* This resource is ignored */
			}
			tmpHolder[i].numResources+=1;
		} else if (strcmp(tmpHolder[i].type, SIR_FEATURE) == 0)
		{
			++numSIRResources;
			if (numSIRResources > licensedSIRResources)
			{
				continue;	/* This resource is ignored */
			}
			tmpHolder[i].numResources+=1;
		}
		/* 
		 * Write valid entry to temporary output file for later read
		 */
		fprintf(fpTmp, "%s\n",buf);
		fflush(fpTmp);
	}
	fclose(fp);

	/*
	 * Determine License validation; Give warning, if necessary
	 */

//	print_license_msg(FAX_FEATURE, numFAXResources, licensedFAXResources);
//	print_license_msg(TTS_FEATURE, numTTSResources, licensedSIRResources);
//	print_license_msg(SIR_FEATURE, numSIRResources, licensedSIRResources);

	/*
	 * calloc memory resource type and subtype combinations;
	 * allow five attempts
	 */
	for (try=0; try<5; try++)
	{
		gvResources= (ResourceTypes *) calloc(numTypes+1,
							sizeof(ResourceTypes));

		if (gvResources == (ResourceTypes *)NULL)
		{
			sleep(60);
			continue;
		}
			break;
	}
	if ( try == 5 )
	{
		sprintf(gvMsg, malloc_error_fmt, errno);
		log(mod, REPORT_NORMAL, MSG_MALLOC_ERROR, gvMsg);
		return(-1);
	}

	/*
	 * calloc memory for ResourceInfo structures within each type;
	 * need one for each device; populate ResourceTypes fields
	 */
	for (i=0; i<numTypes; i++)
	{
		gvResources[i].offsetOfLastAllocatedResource = -1;
		for (try=0; try<5; try++)
		{
			gvResources[i].rInfo= (ResourceInfo *)
					calloc(tmpHolder[i].numResources+1,
							sizeof(ResourceInfo));
	
			if (gvResources[i].rInfo == (ResourceInfo *)NULL)
			{
				sleep(60);
				continue;
			}
			sprintf(gvResources[i].type, "%s", tmpHolder[i].type);
			sprintf(gvResources[i].subType, "%s",
							tmpHolder[i].subType);
			gvResources[i].numResources=tmpHolder[i].numResources;
			break;
		}

		if ( try == 5 )
		{
			sprintf(gvMsg, malloc_error_fmt, errno);
			log(mod, REPORT_NORMAL, MSG_MALLOC_ERROR, gvMsg);
			return(-1);
		}
	}

	/*
	 * Populate the ResourceInfo arrays with the device names
	 */
	rewind(fpTmp);
	while(fgets(buf, sizeof(buf), fpTmp) != NULL)
	{
		rc=sscanf(buf, "%[^|]|%[^|]|%s", type, subType, device);

		for (i=0; gvResources[i].type[0] != '\0'; i++)
		{
			if ( (strcmp(type, gvResources[i].type) != 0) ||
			     (strcmp(subType, gvResources[i].subType) != 0) )
			{
				continue;
			}
			for (j=0; j<gvResources[i].numResources; j++)
			{
				if (gvResources[i].rInfo[j].device[0]!='\0')
				{
					continue;
				}
				if ((rc=deviceAlreadyExists(device,i)) == -1)
				{	/* initialize fields */
					gvResources[i].rInfo[j].status= RESM_FREE;
					sprintf(gvResources[i].rInfo[j].device, "%s", device);
					sprintf(gvResources[i].rInfo[j].slotNumber, 
						"%s", RESM_UNINITIALISED_SLOT);
					break;
				}
				sprintf(gvMsg, "Warning: Config file <%s> "
					"entry <%s> contains duplicate "
					"device entry <%s> for type <%s> and "
					"subType <%s>.  Entry ignored.", 
					 fileName, buf, device, type, subType);
				log(mod, REPORT_NORMAL, MSG_DUPLICATE_ENTRY, gvMsg);
			}
		}
	}
	fclose(fpTmp);
	remove(tmpFileName);

	return(0);
} /* END: callocNPopulateTables */

/*------------------------------------------------------------------------------
int lastStatusStartup():
	Check if the file .ResourceMgr.lastStatus file exists.  If it does
	exist, read each record.  If the device read exists in the
	configuration, the ResourceInfo fields for that device and pid are
	populated and marked busy.  Once all the records are processed, the
	file is closed and removed.
------------------------------------------------------------------------------*/
int lastStatusStartup()
{
	char		fileName[256];
	FILE		*fp;
	char		buf[256];
	int		rc;
	long		pid;
	int		port;
	char		device[DEVICE_SIZE+1];
	char		appName[64];
	int		i, j;
	int		found;

	sprintf(fileName, "%s/Exec/%s", (char *)getenv("TELECOM"),
					LAST_STATUS_FILE);

	if ((rc=access(fileName, R_OK)) != 0)
	{
		return(0);
	}

	if ((fp = fopen(fileName, "r")) == NULL)
	{
		return(0);
	}

	memset((char *)buf, 0, sizeof(buf));
	while(fgets(buf, sizeof(buf), fp) != NULL)
	{
		rc=sscanf(buf, "%ld %d %s %s", &pid, &port, device, appName);
		if ( rc != 4 )
		{
			memset((char *)buf, 0, sizeof(buf));
			continue;
		}
			
		if ((rc=kill(pid, 0)) != 0)
		{
			memset((char *)buf, 0, sizeof(buf));
			continue;
		}

		/*
		 * Find it, populate it, and mark the resource busy
		 */
		found=0;
		for (i=0; gvResources[i].type[0] != '\0'; i++)
		{
			for (j=0; j<gvResources[i].numResources; j++)
			{
				if (strcmp(device,
					gvResources[i].rInfo[j].device) == 0)
				{
					found=1;
					break;
				}
			}
			if ( found )
			{
				sprintf(gvResources[i].rInfo[j].appName, "%s",
								appName);
				gvResources[i].rInfo[j].pid=pid;
				gvResources[i].rInfo[j].status=RESM_BUSY;
				gvResources[i].rInfo[j].port=port;
				break;
			}
		}
		memset((char *)buf, 0, sizeof(buf));
	}
	fclose(fp);	

	remove(fileName); 
	return(0);

} /* END: lastStatusStartup() */

/*------------------------------------------------------------------------------
int lastStatusShutdown():
	Writes records to the .ResourceInfo.lastStatus file, which are 
	comprised of ResourceInfo fields which exist for each busy resource
	at the time of shutdown.  The fields are space-delimited.
	
	This information, upon startup, is used to determine if any of
	the resources which were in use when ResourceMgr shut down, are
	still being used.
------------------------------------------------------------------------------*/
int lastStatusShutdown()
{
	static char	mod[]="lastStatusShutdown";
	char		fileName[256];
	FILE		*fp;
	char		buf[256];
	int		rc;
	int		counter;
	static int	firstTime=1;
	int		found;
	int		i, j;

	sprintf(fileName, "%s/Exec/%s", (char *)getenv("TELECOM"),
							LAST_STATUS_FILE);

	found=0;
	for (i=0; (gvResources[i].type[0] != '\0') && (! found); i++)
	{
		for (j=0; j<gvResources[i].numResources; j++)
		{
			if (gvResources[i].rInfo[j].status == RESM_FREE)
			{
				continue;
			}

			if (firstTime)
			{
				firstTime=0;
				if ((fp = fopen(fileName, "w"))  == NULL)
				{
					sprintf(gvMsg,
				    	"Unable to open <%s> for write. "
					   "errno=%d. No records written.",
						    fileName, errno);
					log(mod, REPORT_NORMAL, 
						MSG_CANT_WRITE, gvMsg);
					return(0);
				}
			}
			fprintf(fp, "%ld %d %s %s\n", 
					gvResources[i].rInfo[j].pid,
					gvResources[i].rInfo[j].port,
					gvResources[i].rInfo[j].device,
					gvResources[i].rInfo[j].appName);
			fflush(fp);
		}
	}

	if (firstTime == 0)
	{
		fclose(fp);
	}

	return(0);
} /* END: lastStatusShutdown() */

/*------------------------------------------------------------------------------
int getTableIndexes():
	Look up the type, subType, and device in the gvResource table, and	
	return the indexes.
	
	Returns 0 if found, -1 if not found.
------------------------------------------------------------------------------*/
static int	getTableIndexes(int freeOrBusy, char *type, char *subType,
				long pid, char *device,
				int *typeIndex, int *deviceIndex)
{
	static	char	mod[] = "getTableIndexes";
	int		i, j;
	int		found;
	int		rc;

	found=0;
	sprintf(gvMsg, "freeOrBusy: %d, type:%s, subType:%s, pid:%ld, device:%s",
			freeOrBusy, type, subType, pid, device);
	log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);

	for (i=0; gvResources[i].type[0] != '\0'; i++)
	{
		sprintf(gvMsg, "Comparing (%s, %s);  (%s, %s)",
			type, gvResources[i].type,
			subType, gvResources[i].subType);
		log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);

		if ( (strcmp(type, gvResources[i].type) == 0) &&
		     (strcmp(subType, gvResources[i].subType) == 0) )
		{
			found=1;
			break;
		}
	}
	if (! found)
	{
		sprintf(gvMsg, "(%s:%s) is not found; returning -1",
					type, subType);
		log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
		return(-1);
	}
	
	switch(freeOrBusy)
	{
	case GET_BUSY_INDEX:	/* Get busy index associated with device/pid */
		for (j=0; j<gvResources[i].numResources; j++)
		{
		 	if ( (pid == gvResources[i].rInfo[j].pid)  &&
			     (strcmp(device,
				gvResources[i].rInfo[j].device) == 0) )
			{
				*typeIndex=i;
				*deviceIndex=j;
				return(0);
			}
		}
		break;
	case GET_FREE_INDEX:	/* Get next free index */
		for (j=gvResources[i].offsetOfLastAllocatedResource+1;
						j<gvResources[i].numResources; j++)
		{
			if ((rc = isStatusFree(i, j)) == 0)
			{
				*typeIndex=i;
				*deviceIndex=j;
				gvResources[i].offsetOfLastAllocatedResource=j;
				return(0);
			}
		}
		for (j=0; j<gvResources[i].offsetOfLastAllocatedResource+1; j++)
		{
			if ((rc = isStatusFree(i, j)) == 0)
			{
				*typeIndex=i;
				*deviceIndex=j;
				gvResources[i].offsetOfLastAllocatedResource=j;
				return(0);
			}
		}
		break;
	default:
		sprintf(gvMsg, "Invalid freeOrBusy parameter (%d).  Must be either %d "
				"or %d.  Returning failure.", freeOrBusy, GET_BUSY_INDEX,
				GET_FREE_INDEX);
		log(mod, REPORT_NORMAL, MSG_DEBUG, gvMsg);
		return(-1);
	}

	return(-1);
} /* END: getTableIndexes() */

/*------------------------------------------------------------------------------
int	isStatusFree()
	If the resource status if FREE, then return 0; otherwise, return -1.
------------------------------------------------------------------------------*/
static int	isStatusFree(int zType, int zDevice)
{
	int		rc;
	static char	mod[]="isStatusFree";

	if (gvResources[zType].rInfo[zDevice].status == RESM_FREE)
	{
		if (gvResources[zType].rInfo[zDevice].SRReserved ==
													RDT_RESERVED)
		{
			sprintf(gvMsg, "Port %d (%s) is RESERVED.  "
						"Sending request for port from responsibility.",
						gvResources[zType].rInfo[zDevice].devicePort,
						gvResources[zType].rInfo[zDevice].device);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);

			rc = rsm_get_port(gvResources[zType].rInfo[zDevice].devicePort);

			sprintf(gvMsg, "%d = rsm_get_port(%d).", rc, 
					gvResources[zType].rInfo[zDevice].devicePort);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
			if ( rc == 0 )
			{
				return(0);
			}
		}
		else
		{
			sprintf(gvMsg, "Port %d (%s) is not RESERVED.  ",
						gvResources[zType].rInfo[zDevice].devicePort,
						gvResources[zType].rInfo[zDevice].device);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
			return(0);
		}
	}

	return(-1);
} /* END: isStatusFree() */

/*------------------------------------------------------------------------------
int	deviceAlreadyExists()
	Search the ResourceInfo table for 'device'.  Return either a 0
	(device exists) or -1 (device does not exist).
------------------------------------------------------------------------------*/
int	deviceAlreadyExists(char *device, int typeIndex)
{
	int		rc;
	static char	mod[]="deviceAlreadyExists";
	int		j;

	for (j=0; j<gvResources[typeIndex].numResources; j++);
	{
		if ( (strcmp(device, gvResources[typeIndex].rInfo[j].device))
									== 0)
		{
			return(0);
		}
	}

	return(-1);
} /* END: deviceAlreadyExists() */

/*------------------------------------------------------------------------------
void	exitResourceMgr()
	Log a message, sleep SLEEP_BEFORE_EXIT seconds, then exit.
------------------------------------------------------------------------------*/
void	exitResourceMgr(int retCode)
{
	int		rc;
	static char	mod[]="exitResourceMgr";

	sprintf(gvMsg,"Shutting down ResourceMgr..." );
	log(mod, REPORT_NORMAL, MSG_SHUTTING_DOWN, gvMsg);

	rc=lastStatusShutdown();

	rc = msgctl(gvQueId, IPC_RMID, 0);
	if(rc == -1)
	{
		sprintf(gvMsg,
			"Failed to remove message queue. errno=%d.", errno);
		log(mod, REPORT_NORMAL, MSG_CANT_REMOVE_QUEUE, gvMsg);
	}

	sleep(SLEEP_BEFORE_EXIT);
	exit(retCode);

} /* END: exitResourceMgr() */

/*------------------------------------------------------------------------------
interruptHandler():
	Logs shutdown message and removes the message queue.
------------------------------------------------------------------------------*/
void interruptHandler()
{
	static char	mod[]="interruptHandler";

	sprintf(gvMsg, "Signal received.");
	log(mod, REPORT_VERBOSE, MSG_SIGNAL_RECEIVED, gvMsg);
	exitResourceMgr(0);

} 

/*------------------------------------------------------------------------------
utilSleep():
	Sleep seconds.milliSeconds.
------------------------------------------------------------------------------*/
void	utilSleep(int seconds, int milliSeconds)
{
	static struct timeval sleepTime;

	sleepTime.tv_sec = seconds;
	sleepTime.tv_usec = milliSeconds * 1000;

	select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &sleepTime);

} /* END: utilSleep() */

/*------------------------------------------------------------------------------
cleanString():
        Remove all whitespace from buf, and return it.
------------------------------------------------------------------------------*/
int     cleanString(char *buf)
{
	char	tmpBuf[257];
	char	*tmpPtr;
	char	*bufPtr;
	
	memset((char *)tmpBuf, 0, sizeof(tmpBuf));
	if (strlen(buf) > 256)
	{
		return(-1);     /* string too big */
	}
	
	memset((char *)tmpBuf, 0, sizeof(tmpBuf));
	tmpPtr=tmpBuf;
	bufPtr=buf;

	while ( *bufPtr != '\0' )
	{
		if ( isspace (*bufPtr) )
		{
			++bufPtr;
			continue;
		}
		*tmpPtr++ = *bufPtr++;
	}
	
	sprintf(buf, "%s", tmpBuf);
	return(0);

} /* END: cleanString() */

/*------------------------------------------------------------------------------
printTables():
	Print the entire gvResources table;
------------------------------------------------------------------------------*/
void	printTables()
{
	static char	mod[]="printTables";
	int		i, j;

	for (i = 0; gvResources[i].type[0] != '\0'; i++)
	{
		sprintf(gvMsg, "gvResources[%d].type:<%s>", i,
						gvResources[i].type);
		log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
		sprintf(gvMsg, "gvResources[%d].subType:<%s>", i,
						gvResources[i].subType);
		log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
		sprintf(gvMsg, "gvResources[%d].numResources:%d",
						i, gvResources[i].numResources);
		log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
		for (j=0; j< gvResources[i].numResources; j++)
		{
			sprintf(gvMsg,
				"gvResources[%d].rInfo.[%d].status:%d", 
				i, j, gvResources[i].rInfo[j].status);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
			sprintf(gvMsg,"gvResources[%d].rInfo.[%d].pid:%d", 
				i, j, gvResources[i].rInfo[j].pid);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
			sprintf(gvMsg,"gvResources[%d].rInfo.[%d].port:%d",
				i, j, gvResources[i].rInfo[j].port);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
			sprintf(gvMsg,
				"gvResources[%d].rInfo.[%d].device:<%s>", 
				i, j, gvResources[i].rInfo[j].device);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
			sprintf(gvMsg,
				"gvResources[%d].rInfo.[%d].appName:<%s>", 
				i, j, gvResources[i].rInfo[j].appName);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);

			sprintf(gvMsg,
				"gvResources[%d].rInfo.[%d].devicePort:<%d>", 
				i, j, gvResources[i].rInfo[j].devicePort);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
			sprintf(gvMsg,
				"gvResources[%d].rInfo.[%d].SRReserved:<%d>", 
				i, j, gvResources[i].rInfo[j].SRReserved);
			log(mod, REPORT_VERBOSE, MSG_DEBUG, gvMsg);
		}
	}
} /* END: printTables() */
 
/*------------------------------------------------------------------------------
sendMsgToQueue():
	Send a message to the message queue, based on the pid.

	If msgsnd() fails with a return code of -1 and errno == EAGAIN,
	a sleep(0.3) will be performed before trying again.  If after
	MSGQUE_ATTEMPTS retries a successful attempt has not been made,
	a message is logged, and a 0 is returned.  This means the message
	is discarded.

	If msgsnd() fails with a return code of -1 and errno != EAGAIN,
	there is probably a serious problem: a message is logged, and
	a -1 is returned.
------------------------------------------------------------------------------*/
int 	sendMsgToQueue(long pid, char *message) 
{
	static	char	mod[] = "sendMsgToQueue";
	ResourceMsg	msg;
	int		rc;
	int		try;

	sprintf(msg.data, "%s", message);
	msg.type = pid;
	for (try=0; try<MSGQUE_ATTEMPTS; try++)
	{
		if((rc=msgsnd(gvQueId, &msg, RESM_MSG_DATASIZE, IPC_NOWAIT))
									== 0) 
		{
			sprintf(gvMsg, "%d=msgsnd(%d,(%d,%s), %d, %d)",
						rc,gvQueId, msg.type, msg.data,
						RESM_MSG_DATASIZE, IPC_NOWAIT);
			log(mod, REPORT_DETAIL, MSG_DEBUG, gvMsg);
			return(0);
		}
		utilSleep(0, 300);
	}

	sprintf(gvMsg, "Failed to send message <%s> to pid=%d. errno=%d.",
			message, pid, errno);
	log(mod, REPORT_NORMAL, MSG_FAILED_TO_SEND_MSG, gvMsg);
	return(-1);

} /* END: sendMsgToQueue() */


static int arcValidFeature(char *feature, char *version, int *feature_count,
			char *msg)
{
	static char		mod[]="arcValidFeature";
	char			yFeature[32];
	char			yVersion[32];
	int				rc; 
	char			failureReason[256];

	*feature_count = 0;
	sprintf(yFeature, "%s", feature);
	sprintf(yVersion, "%s", version);
	memset((char *)failureReason, '\0', sizeof(failureReason));

	if ((rc = flexLMCheckOut(yFeature, yVersion, 1,
                        &GV_lmHandle, failureReason)) != 0)
    {
        return(-1);
    }

	memset((char *)failureReason, '\0', sizeof(failureReason));
	(void) flexLMCheckIn(&GV_lmHandle, feature, failureReason);

	*feature_count = 50;  	// djb - to.  Get the number of licenses.

	return(0);
} // END: arcValidFeature()

#if 0
/*---------------------------------------------------------------------------- 
This function checks the mac address of the machine, then looks for and
counts the requested feature. 
----------------------------------------------------------------------------*/
static int arcValidFeature(feature, feature_count, msg)
char * 	feature;
int  * 	feature_count;
char * 	msg;
{
	int rc;
	int count, mac_tries;
	char err_msg[512];

	/* Initialize return values */
	*feature_count = 0;
	msg[0]='\0';

	rc = lic_get_license(feature, err_msg);
	switch(rc)
	{
	case 0:
		sprintf(msg,"License granted (%s).", feature);
		break;
	case -1:
		strcpy(msg, err_msg);
		return(-1);
		break;
	default:
		sprintf(msg,"Unknown license return code %d.", rc);
		return(-1);
		break;
	}

	rc = lic_count_licenses(feature, &count, err_msg);
	if (rc == -1)
	{
		strcpy(msg,err_msg);
	 	return(-1);
	}
	*feature_count = count;
	return(0);
}

/*----------------------------------------------------------------------------
Print license warnings and info. 
----------------------------------------------------------------------------*/
int print_license_msg(char *feature, int num_configed, int licensed)
{
	char format[128];
	char mod[]="print_license_msg";
	static char license_warn[]="Warning: %d %s resources are configured. You are only licensed for %d. Using %d.";
	static char license_info[]="%d %s resources are configured. You are licensed for %d. Using %d.";

	if (num_configed == 0) 
	{
		sprintf(gvMsg, "No %s resources are configured.", feature);
		log(mod, REPORT_NORMAL, MSG_NO_RESOURCES_CONFIGED, gvMsg);
		return;
	}

	if (num_configed > licensed)
	{
		strcpy(format, license_warn);
		sprintf(gvMsg, format, num_configed, feature,
				licensed, min(num_configed,licensed));
		log(mod, REPORT_NORMAL, MSG_LICENSE_WARNING, gvMsg);
	}
	else
	{
		strcpy(format, license_info);
		sprintf(gvMsg, format, num_configed, feature,
				licensed, min(num_configed,licensed));
		log(mod, REPORT_NORMAL, MSG_LICENSE_INFO, gvMsg);
	}
	return;
}
#endif


int min(int x,int y)
{
	if (x < y) return(x);
	return(y);
}

static int log(char *mod, int mode, int id, char *msg)
{
	LogARCMsg(mod, mode, "0", "RSM", "ResourceMgr", id, msg);
}

/*------------------------------------------------------------------------------
 * myGetField(): 	This routine extracts the value of the desired
 *			fieldValue from the data record.
 * 
 * Dependencies: 	None.
 * 
 * Input:
 * 	delimiter	: that separates fieldValues in the buffer.
 * 	buffer		: containing the line to be parsed.
 * 	fieldNum	: Field # to be extracted from the buffer.
 * 	fieldValue	: Return buffer containing extracted field.
 * 
 * Output:
 * 	-1	: Error
 * 	>= 0	: Length of fieldValue extracted
 *----------------------------------------------------------------------------*/
static int myGetField(char delimiter, char *buffer, int fieldNum, char *fieldValue)
{
	static char	mod[] = "gaGetField";
	register int	i;
	int		fieldLength, state, wc;

	fieldLength = state = wc = 0;

	fieldValue[0] = '\0';

	if(fieldNum <= 0)
	{
		/* gaVarLog(mod, 0, "Field number (%d) must be > 0", fieldNum); */
        return (-1);
	}

	for(i=0;i<(int)strlen(buffer);i++) 
        {
		/*
		 * If the current character is a delimiter
		 */
        	if(buffer[i] == delimiter || buffer[i] == '\n')
                {
			/*
			 * Set state to 0 indicating that we have hit a 
			 * field boundary.
			 */
                	state = 0;

			/*
			 * If the current character and the previous character
			 * are delimiters, increment the word count.
			 */
                	if(buffer[i] == delimiter)
			{
				if(i == 0 || buffer[i-1] == delimiter)
				{
					++wc;
				}
			}
                }
        	else if (state == 0)
                {
                	state = 1;
                	++wc;
                }
        	if (fieldNum == wc && state == 1)
		{
                	fieldValue[fieldLength++] = buffer[i];
		}
        	if (fieldNum == wc && state == 0)
		{
			fieldValue[fieldLength] = '\0';
                	break;
		}
        } /* for */

	if(fieldLength > 0)
        {
        	fieldValue[fieldLength] = '\0';
        	return (fieldLength);
        }

	return(-1);
} /* END: myGetField() */
