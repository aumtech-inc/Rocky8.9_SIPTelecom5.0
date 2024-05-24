/*-----------------------------------------------------------------------------
Program:SRInit.c
Purpose:MRCP SR Initialization.
Author:	Aumtech, Inc.
Update:	06/30/06 djb	Ported to mrcpV2.
------------------------------------------------------------------------------*/
#include <unistd.h>
#include <stdlib.h>
#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"
#include "Telecom.h"

using namespace mrcp2client;

/*------------------------------------------------------------------------------
SRInit():
------------------------------------------------------------------------------*/
int SRInit(int zPort)
{
	static char mod[] = "SRInit";

//	mrcpClient2Log(__FILE__, __LINE__, zPort,
//			mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"SR variables initialized; telSRInitCalled=%d., gSrPort[%d] address=%p",
//			gSrPort[zPort].IsTelSRInitCalled(), zPort, &gSrPort[zPort]);
	
	if ( ! gSrPort[zPort].IsCallActive() )
	{
		return(TEL_DISCONNECTED);
	}

	if ( gSrPort[zPort].IsTelSRInitCalled() )
	{
		return(TEL_SUCCESS);
	}

	gSrPort[zPort].setLicenseReserved(false);
	gSrPort[zPort].setCallActive(true);
	gSrPort[zPort].setGrammarsLoaded(0);
	//gSrPort[zPort].setFailOnGrammarLoad(false);
	gSrPort[zPort].setTelSRInitCalled(true);

//	mrcpClient2Log(__FILE__, __LINE__, zPort,
//			mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
//			"SR variables successfully initialized; telSRInitCalled=%d., gSrPort[%d] address=%p",
//			gSrPort[zPort].IsTelSRInitCalled(), zPort, &gSrPort[zPort]);

	grammarList[zPort].removeAll();

	//gSrPort[zPort].initializeMutex();		DDN 06162011

	return(0);

} /* END: SRInit() */
