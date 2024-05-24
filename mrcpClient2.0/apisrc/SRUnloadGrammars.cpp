/*-----------------------------------------------------------------------------
Program:	SRUnloadGrammars.cpp
Purpose:    clear grammarList for application port.
Author:		Aumtech, Inc.
Update:		07/28/06 yyq	Modified the file for Mrcp2.0client system.
Update:		10/21/03 djb	Added and removed debug messages.
Update:		03/20/03 djb	Created the file.
------------------------------------------------------------------------------*/
#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"

using namespace mrcp2client;

extern "C"
{
	#include "Telecom.h"
	#include "arcSR.h"
}

int SRUnloadGrammars(int zPort)
{
	int					yRc;
	char				mod[] = "SRUnloadGrammars";

	if( ! gSrPort[zPort].IsCallActive() )
	{
        mrcpClient2Log(__FILE__, __LINE__, zPort,
            mod, REPORT_DETAIL, MRCP_2_BASE, WARN,
            "%s failed - Call is no longer active.", mod);
		return(TEL_DISCONNECTED);
	}
	
	if ( ! gSrPort[zPort].IsTelSRInitCalled() )
	{
        mrcpClient2Log(__FILE__, __LINE__, zPort,
            mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
            "%s failed - Must call SRInit first.", mod);
		return(TEL_FAILURE);
	}

	srUnLoadGrammars(zPort);		
	//grammarList[zPort].removeAll();
	//grammarList[zPort].deactivateAllGrammars();

	return(0);

} // END: SRUnloadGrammars()

