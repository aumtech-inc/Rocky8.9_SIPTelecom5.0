/*------------------------------------------------------------------------------
File:		ResourceMgr.h
Author:		Aumtech, Inc.
Update:	10/28/98 djb	Created.
Update:	06/08/99 mpb	Added port & appName to Free_Resource prototype.
Update:	04/28/00 apj 	Added RESM_SET_SLOT_NUMBER define.
Update:	05/01/00 apj 	Added setSlotNumberInResourceMgr prototype.
------------------------------------------------------------------------------*/
#ifndef RESOURCE_MGR_H	/* Just preventing multiple includes... */
#define RESOURCE_MGR_H

/* message queue related */
#define RESM_MSG_DATASIZE	256	/* Maximum message data size */
#define	RESM_MSG_TYPE		9	/* Request message type */
#define RESM_MSG_QUE		6567L	/* Message queue */
#define RESM_PERMS		0666	/* Permission */

/* resource manager requests */
#define	RESM_REQ_ALLOCATE	1	/* request a resource */
#define RESM_REQ_FREE		2	/* request to free a resource */
#define	RESM_REQ_STATUS		3	/* request status of all resources */
#define	RESM_SET_SLOT_NUMBER	4	/* Set Resource Slot Number */

/* failure return codes */
#define RESM_GENERAL_FAILURE    -1
#define RESM_UNABLE_TO_SEND     -2 /* unable to put message on que */
#define RESM_NO_RESPONSE        -3 /* no response from ResourceMgr */
#define RESM_RESOURCE_NOT_AVAIL -4 /* all resources are busy */
#define RESM_NO_RESOURCES_CONFIG -5 /* resource.cfg is not configured*/

#define RESM_UNINITIALISED_SLOT "99999"

/* Message queue data structure; to be used by resourceMgr */
typedef struct
{
       	long    type;				/* message type */
       	char    data[RESM_MSG_DATASIZE];	/* message data */
} ResourceMsg;

/*----------------------  Function Prototypes  -------------------------------*/
int Request_Resource(
	char	*type,		/* correspond to field1 of resource.cfg */
	char	*subType,	/* correspond to field2 of resource.cfg */
	int	port,           /* port the application is running on */
	char	*appName,       /* application name */
	char	*resourceName,  /* output the device name returned */
	char	*slotNumber, 	/* slot number of a resource */
	char	*logMsg		/* text of error message */
                    );

int Free_Resource(
	char	*type,		/* correspond to field1 of resource.cfg */
	char	*subType,	/* correspond to field2 of resource.cfg */
	int	port,           /* port the application is running on */
	char	*appName,       /* application name */
	char	*resourceName,  /* the device name of the resource */
	char	*logMsg		/* text of error message */
		 );

int Status_Resources(
	char *fileName,		/* returned filename */
	char	*logMsg		/* text of error message */
		);	

int setSlotNumberInResourceMgr(
	char	*type,		/* correspond to field1 of resource.cfg */
	char	*subType,	/* correspond to field2 of resource.cfg */
	int	port,           /* port the application is running on */
	char	*appName,       /* application name */
	char	*resourceName,  /* the device name of the resource */
	char	*slotNumber, 	/* slot number of a resource */
	char	*logMsg		/* text of error message */
		 );
#endif
