/*-----------------------------------------------------------------------------
Program:SRGetParameter.cpp
Author:	Aumtech, Inc.
Update:	08/01/2006 djb Ported to mrcpV2.
Update:	01/03/2007 djb Added MRCP GET-PARAMS logic.
------------------------------------------------------------------------------*/
#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"
#include "mrcp2_Response.hpp"
#include "mrcp2_HeaderList.hpp"
#include "parseResult.hpp"

using namespace mrcp2client;

extern "C"
{
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include "Telecom.h"
	#include "arcSR.h"
}

string sCreateGetParamsRequest(int zPort, char *zParamName);
void cleanPStr(char *zStr, int zLen);

int SRGetParameter(int zPort, char *zParameterName, char *zParameterValue)
{
	static char	mod[] = "SRGetParameter";
	int			rc;

    if ( ! gSrPort[zPort].IsCallActive() )
    {
       mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL,
            MRCP_2_BASE, WARN, 
			"Call is no longer active.");
        return(TEL_DISCONNECTED);
    }

    if ( ! gSrPort[zPort].IsTelSRInitCalled() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
	   		"SRGetParameter failed: Must call SRInit first.");
		return(TEL_FAILURE);
    }

	if(zParameterName[0] == '\0')
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
			"Invalid parameter name. Parameter name cannot be empty."); 
		return(TEL_FAILURE);
	}

    if (strncmp(zParameterName, "MRCP:", 5) == 0)
    {
        string yGetParamsMsg = sCreateGetParamsRequest(zPort,
                        &(zParameterName[5]));
        if ( yGetParamsMsg.empty() )
        {
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
                        MRCP_2_BASE, ERR,
                        "Empty request generated.  "
                        "Unable to send GET-PARAMS request.");
            return(TEL_FAILURE);
        }

        string serverState = "";
        string  recvMsg;
        int     statusCode  = -1;
        int     causeCode   = -1;;

        rc = processMRCPRequest(zPort, yGetParamsMsg, recvMsg,
                serverState, &statusCode, &causeCode);
		
        if ( statusCode >= 300 )
        {
            return(-1);
        }

        if ( recvMsg.size() <= 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR,
				"Received empty response (%s); Parameter (%s) not retrieved.",
				 recvMsg.c_str(), &(zParameterName[5]));
			return(-1);
		}

		Mrcp2_Response      mrcpResponse;
		MrcpHeaderList      headerList;
		mrcpResponse.addMessageBody(recvMsg);
		headerList = mrcpResponse.getHeaderList(); // headerList
		zParameterValue[0] = '\0';

		while(!headerList.empty())
		{
			MrcpHeader      header;
			header = headerList.front();
			
			string name = header.getName();
			string value = header.getValue();
			if (name == &(zParameterName[5]))
			{
				sprintf(zParameterValue, "%s", value.c_str());
			}

			headerList.pop_front();

		} // END: headerList.empty()

		if ( zParameterValue[0] == '\0' )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR,
				"Did not receive parameter (%s) in response.",
				 &(zParameterName[5]));
			return(-1);
		}

		cleanPStr(zParameterValue, strlen(zParameterValue));

        return(0);
    }

	if(strcmp(zParameterName, "$SR_SESSION_RECORD") == 0)
	{
		if(gSrPort[zPort].IsSessionRecord())
		{
			sprintf(zParameterValue, "%s", "YES"); 
		}
		else
		{
			sprintf(zParameterValue, "%s", "NO"); 
		}
	}
	else if(strcmp(zParameterName, "$SR_SESSION_PATH") == 0)
	{
		sprintf(zParameterValue, "%s", gSrPort[zPort].getSessionPath().c_str());
	}
	else if(strcmp(zParameterName, "$SR_GRAMMAR_AFTER_VAD") == 0)
	{
		if(gSrPort[zPort].IsGrammarAfterVAD())
		{
			sprintf(zParameterValue, "%s", "YES"); 
		}
		else
		{
			sprintf(zParameterValue, "%s", "NO"); 
		}
	}
	else if(strcmp(zParameterName, "$SR_GRAMMAR_NAME") == 0)
	{
		sprintf(zParameterValue, "%s", gSrPort[zPort].getGrammarName().c_str());
	}
	else if(strcmp(zParameterName, "$SR_FAIL_ON_GRAMMAR_LOAD") == 0)
	{
		if(gSrPort[zPort].IsFailOnGrammarLoad())
		{
			sprintf(zParameterValue, "%s", "YES"); 
		}
		else
		{
			sprintf(zParameterValue, "%s", "NO"); 
		}
	}
	else
	{
       mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, ERR, 
			"Invalid parameter name: %s. No such adjustable parameter.", 
			zParameterName);
		return(TEL_FAILURE);
	}

	return(TEL_SUCCESS);
} /* END: SRGetParameter() */

/*------------------------------------------------------------------------------
sCreateGetParamsRequest():
------------------------------------------------------------------------------*/
string sCreateGetParamsRequest(int zPort, char *zParamName)
{
    int     rc = 0;
    MrcpHeader                      header;
    MrcpHeaderList                  headerList;
    int                             yCounter;
    string                          yContent = "";
    char                            yContentLength[16];
    string                          yGetParamsMsg;
    string                          yParamName;
    string                          yParamValue;

    yParamName  = zParamName;

    header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
    headerList.push_back(header);

    header.setNameValue(yParamName, "");
    headerList.push_back(header);

    gSrPort[zPort].incrementRequestId();
    ClientRequest getParamsRequest(
        gMrcpInit.getMrcpVersion(),
        "GET-PARAMS",
        gSrPort[zPort].getRequestId(),
        headerList,
        0);

    string getParamsMsg = getParamsRequest.buildMrcpMsg();
    yGetParamsMsg = getParamsMsg + yContent;

    return(yGetParamsMsg);
} // END: sCreateSetParamsRequest()

/*------------------------------------------------------------------------------cleanPStr()
------------------------------------------------------------------------------*/
void cleanPStr_1(char *zStr, int zLen)
{
    int         i;
    int         spaceFound;
    char        *p;

    if ( (zStr[0] == '\0' ) || (zLen == 0) )
    {
        return;
    }

    for (i = zLen - 1; i >= 0; i--)
    {
        if ( isspace(zStr[i]) )
        {
            zStr[i] = '\0';
        }
        else
        {
            break;
        }
    }

    p = zStr;
    spaceFound = 0;
    while ( (*p != '\0') && ( isspace(*p) ) )
    {
        p++;
        spaceFound = 1;
    }

    if (spaceFound)
    {
        sprintf(zStr, "%s", p);
    }
    return;
} /* END: cleanPStr() */


