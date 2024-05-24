/*-----------------------------------------------------------------------------
Program:SRExit.c
Author:	Aumtech, Inc.
Update:	08/01/2006 djb Ported to mrcpV2.
------------------------------------------------------------------------------*/
#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"

using namespace mrcp2client;

extern "C"
{
	#include "Telecom.h"
	#include "arcSR.h"
}

extern int gCanProcessSipThread;


/*------------------------------------------------------------------------------
SRExit():
------------------------------------------------------------------------------*/
int SRExit(int zPort)
{
	static char		mod[] = "SRExit";
	int				yRc;

	if ( !gSrPort[zPort].IsTelSRInitCalled() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
           MRCP_2_BASE, INFO, "SRExit failed.  Must call SRInit first.");
		return(TEL_FAILURE);
	}

	if(gCanProcessSipThread)
	{
		yRc = cleanUp(zPort);
	}
	else
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
           MRCP_2_BASE, INFO, "gCanProcessSipThread is reset, not calling cleanup.");
	}

	if( !gSrPort[zPort].IsCallActive() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL,
			MRCP_2_BASE, WARN, "Call is no longer active.");
		return(TEL_DISCONNECTED);
	}

	return(yRc);
} /* END: SRExit() */

