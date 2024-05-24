/*-----------------------------------------------------------------------------
Program:	SRLoadGrammar.c
Author:		Aumtech, Inc.
Update:		07/13/06 djb	Ported to mrcpV2.
------------------------------------------------------------------------------*/
#include <fstream>
#include <string>

#include "mrcpCommon2.hpp"
#include "mrcp2_HeaderList.hpp"
#include "mrcpSR.hpp"
extern "C"
{
	#include <stdio.h>
	#include <string.h>
	#include <unistd.h>
	#include <errno.h>
	#include "Telecom.h"
	#include "arcSR.h"
}

using namespace mrcp2client;

//=====================================
int SRLoadGrammar(int zPort, 
				int zGrammarType, 
			  	const string& zGrammarFile,
				float zWeight)
//=====================================
{
	char 			mod[] ="SRLoadGrammar";
	int				rc;
	int				grammarVADSw;
	string			grammarContent;
	string			grammarName;
	GrammarData		grammarData;
	int				i;


    if ( ! gSrPort[zPort].IsCallActive() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort,
            mod, REPORT_DETAIL, MRCP_2_BASE, WARN,
			"Call is no longer active.");
        return(TEL_DISCONNECTED);
    }

	if ( ! gSrPort[zPort].IsTelSRInitCalled() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort,
            mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
			"%s failed - Must call SRInit first.", mod);
		return(TEL_FAILURE);
	}

	if ( zGrammarFile.empty())
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort,
            mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Invalid input. Empty grammar (%s) received.",
				zGrammarFile.data());
		return(TEL_FAILURE);
	}

    grammarName = gSrPort[zPort].getGrammarName();

    if ( grammarName.empty() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort,
            mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
			"TEL_SRSetParamter(\"$SR_GRAMMAR_NAME\", ...) must be called "
			"first.");
		return(TEL_FAILURE);
	}

	switch (zGrammarType)
	{
		case SR_URI:
		case SR_FILE:
		case SR_STRING:
			break;
        case SR_GRAM_ID:
            rc = srLoadGrammar(zPort, zGrammarType, zGrammarFile.c_str(),
                grammarContent.c_str(), grammarVADSw, zWeight);
            return rc;
		default:
			mrcpClient2Log(__FILE__, __LINE__, zPort,
           		 mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Invalid grammartype (%d). Must be one of SR_STRING(%d), "
				"SR_FILE(%d), or SR_URI(%d).", zGrammarType, SR_STRING, SR_FILE,
				SR_URI);
			return(TEL_FAILURE);
			break;
	}

	mrcpClient2Log(__FILE__, __LINE__, zPort,
           	mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Processing SRLoadGrammar for grammar (%s).", 
			zGrammarFile.data() );

    for (i = 0 ; i < 3; i++ )           // MR 4974
    {   
		if(access(zGrammarFile.c_str(), R_OK) != 0)
		{
			if ( gSrPort[zPort].IsDisconnectReceived() ||
	             gSrPort[zPort].IsExitSentToMrcpThread(zPort) )
			{
				mrcpClient2Log(__FILE__, __LINE__, zPort,
					mod, REPORT_NORMAL, MRCP_2_BASE, WARN,
					"Call is disconnected and grammar file (%s) is not accessible. [%d, %s].",
					zGrammarFile.data(), errno, strerror(errno));
				return(TEL_FAILURE);
			}
			if ( i == 2 )
			{
				mrcpClient2Log(__FILE__, __LINE__, zPort,
					mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
					"SRLoadGrammar failed.  Grammar file (%s) is unreadable. [%d, %s].",
					zGrammarFile.data(), errno, strerror(errno));
				return(TEL_FAILURE);
			}
			usleep(5000);
		}
	}

	ifstream input(zGrammarFile.c_str()); 
	if(!input)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort,
           	 mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
			"SRLoadGrammar failed. Can't open %s.", zGrammarFile.c_str());

		return(TEL_FAILURE);
	}

	// load grammar file content into string
	while(!input.eof() )
	{
		string line;
		getline(input, line);
		grammarContent += line ; // or "\0"  
	}
	input.close();

	mrcpClient2Log(__FILE__, __LINE__, zPort,
         	mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"Processing grammar: [%s:%.*s]; gSrPort[%d].IsGrammarAfterVAD()=%d.",
			grammarName.c_str(), 600, grammarContent.c_str(),
			zPort, gSrPort[zPort].IsGrammarAfterVAD());

	if( ( gSrPort[zPort].IsGrammarAfterVAD() ) && 
	    ( ! gSrPort[zPort].IsLicenseReserved() ) )
	{
		grammarVADSw = LOAD_GRAMMAR_VAD_LATER;
	}
	else
	{
		grammarVADSw = LOAD_GRAMMAR_NOW;
	}

	rc = srLoadGrammar(zPort, zGrammarType, grammarName.c_str(),
					grammarContent.c_str(), grammarVADSw, zWeight);

	return(rc);

} // END: SRLoadGrammar() 





#if 0
//=======================================
static void srPrintGrammarList(int zPort)
//=======================================
{
	static char						mod[] = "srPrintGrammarList";
	list <GrammarData>::iterator	iter;
	GrammarData						yGrammarData;
	int								yCounter;
	
	iter = grammarList[zPort].begin();

	yCounter = 1;
	while ( iter != grammarList[zPort].end() )
    {
        yGrammarData = *iter;
		mrcpClient2Log(__FILE__, __LINE__, zPort,
			mod, REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"grammarList[%d]:%d:type=%d; name=(%s); data=(%.*s).",
			zPort, yCounter, yGrammarData.type,
			yGrammarData.grammarName.c_str(), 100, yGrammarData.data());
        iter++;
		yCounter++;
    }
	
} // END: srPrintGrammarList()
#endif

