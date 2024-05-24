/*-----------------------------------------------------------------------------
Program:SRUnloadGrammar.cpp
Purpose:remove grammar from grammarList
Author:	Aumtech, Inc.
Update:	07/28/06 yyq	Created the file.
------------------------------------------------------------------------------*/

#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"

using namespace mrcp2client;

extern "C"
{
	#include "Telecom.h"
	#include "arcSR.h"
}


#include <string>
#include "Telecom.h"
using namespace mrcp2client;

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int SRUnloadGrammar(int zPort, char *grammarName)
{
	int			yRc = -1;
	char mod[] = "DM_SRUnloadGrammar";
	string yGrammarName = string(grammarName);

	if( !gSrPort[zPort].IsCallActive() )
	{
		return(TEL_DISCONNECTED);
	}
	
	if ( ! gSrPort[zPort].IsTelSRInitCalled() )
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, mod,
			REPORT_NORMAL, MRCP_2_BASE, WARN,
			"Must call SRInit first." );

		return(TEL_FAILURE);
	}

		
	srUnLoadGrammar(zPort, yGrammarName.c_str());		
	//grammarList[zPort].deactivateGrammar(yGrammarName);
#if 0
	// find grammarName from grammarList and remove the item
	list <GrammarData>::iterator    iter;
	GrammarData                     grammarData;

    iter = grammarList[zPort].begin();

	while ( iter != grammarList[zPort].end() )
    {
        grammarData = *iter;

        mrcpClient2Log(__FILE__, __LINE__, zPort,
            mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "remove grammarList[%d]: type=%d; name=(%s); data=(%s).",
            zPort, grammarData.type,
            grammarData.grammarName.data(), grammarData.data.data());

		if(grammarName == grammarData.grammarName)
		{
			//grammarList[zPort].remove(grammData);
			yRc = grammarList[zPort].deactivateGrammar(grammarName);
			
			if(yRc == 1)
			{
				gSrPort[zPort].setIsGrammarGoogle(false);
			}
		}

        iter++;
    }
#endif

	return(0);

} // END: SRUnloadGrammar() 

