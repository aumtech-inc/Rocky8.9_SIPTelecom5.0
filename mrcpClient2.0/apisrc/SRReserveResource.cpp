/*-----------------------------------------------------------------------------
Program:	SRReserveResource.c
Author:		Aumtech, Inc.
Update:		07/13/06 djb	Ported to mrcpV2.
------------------------------------------------------------------------------*/

#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"
#include "Telecom.h"

using namespace mrcp2client;


/*------------------------------------------------------------------------------
SRReserveResource():
------------------------------------------------------------------------------*/
int SRReserveResource(int zPort)
{
    int      rc = -1;
	char mod[] = "DM_SRReserveResource";

	if( !gSrPort[zPort].IsCallActive() )
	{
       mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL,
            MRCP_2_BASE, WARN, "Call is no longer active.");
		return(TEL_DISCONNECTED);
	}

	if( !gSrPort[zPort].IsTelSRInitCalled() )
	{
       mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, ERR, 
			"IsTelSRInitCalled false, need to be true to continue.");
		return(TEL_FAILURE);
	}

	if (  gSrPort[zPort].IsGrammarAfterVAD() ) 
	{
       mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, 
			"$GRAMMAR_AFTER_VAD is enabled.  Not reserving a resource now.");
		return(TEL_SUCCESS);
	}
	
	if ( (rc = srGetResource(zPort) ) == -1 )
	{
		return(TEL_FAILURE);
	}

	return(0);

} /* END: SRReserveResource() */

