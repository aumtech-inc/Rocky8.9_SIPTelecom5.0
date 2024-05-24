/*-----------------------------------------------------------------------------
Program:	DM_SRReleaseResource.c
Purpose:	
Author:		Aumtech, Inc.
Update:		03/20/03 djb	Created the file.
Update:		10/21/03 djb	Added and removed debug messages.
------------------------------------------------------------------------------*/

#include "arcSR.h"
#include "mrcpSR.hpp"
#include "mrcpCommon2.hpp"
#include "Telecom.h"

using namespace mrcp2client;

extern  int sendReleaseResourceToDM(int zPort);

/*------------------------------------------------------------------------------
SRReleaseResource():
------------------------------------------------------------------------------*/
int SRReleaseResource(const int zPort)
{
	char mod[] ="SRReleaseResource";
	int						rc = -1;

	int callNum = zPort;

	if ( ! gSrPort[callNum].IsTelSRInitCalled() )
	{
		mrcpClient2Log(__FILE__, __LINE__, callNum, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, "%s failed -Must call DM_SRInt() first.", mod);
		return(TEL_FAILURE);
	}

	//rc = sendReleaseResourceToDM(zPort);

	if ( (rc = srReleaseResource(callNum)) != 0 )
	{
		return(TEL_FAILURE);
	}

	if( !gSrPort[callNum].IsCallActive() )
	{
		return(TEL_DISCONNECTED);
	}

	return(TEL_SUCCESS);
} // END: SRReleaseResource() 
