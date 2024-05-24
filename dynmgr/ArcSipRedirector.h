/*------------------------------------------------------------------------------
Program Name:	ArcSipCallMgr/ArcIPDynMgr
File Name:		ArcSipCallMgr.h
Purpose:  		ArcSipCallMgr specific #includes for SIP/2.0
Author:			Aumtech, inc.
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
Update: 07/07/2004 DDN Created the file
------------------------------------------------------------------------------*/

#define MAX_REDIRECTS 20
#ifndef UINT32
#define UINT32 unsigned int
#endif

typedef enum dyn_status
{
	UP,
	DOWN
}dyn_status_;


struct dynMgrStatus
{
	int 	dynMgrId;
	int		status;
	int		port;
	int		pid;
	char	ip[256];
	int		sentCalls;
	char	requestFifo[256];
};

void * readAndProcessDMRequests (void *z);

 /*EOF*/
