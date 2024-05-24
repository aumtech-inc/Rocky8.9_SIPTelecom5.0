/*-----------------------------------------------------------------------------
Program: TTSGetParams.cpp
Author:	Aumtech, Inc.
Update:	03/15/2016 djb Added GET-PARAMS logic.
------------------------------------------------------------------------------*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#include <fstream>
#include <string>
#include "AppMessages_mrcpTTS.h"

#include "mrcpCommon2.hpp"
#include "mrcp2_HeaderList.hpp"
#include "mrcp2_Request.hpp"
#include "mrcp2_Response.hpp"
#include "mrcp2_Event.hpp"
#include "mrcpTTS.hpp"
#include "parseResult.hpp"

using namespace mrcp2client;
using namespace std;

extern "C"
{
    #include <unistd.h>
	#include "Telecom.h"
	#include "arcSR.h"
	void cleanPStr(char *zStr, int zLen);
}

string sCreateGetParamsRequest(int zPort, char *zParamName);

int TTSGetParams(int zPort, char *zParameterName, char *zParameterValue)
{
	static char	mod[] = "TTSGetParams";
	int			rc;

	if(zParameterName[0] == '\0')
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
			"Invalid parameter name. Parameter name cannot be empty."); 
		return(TEL_FAILURE);
	}

	string yGetParamsMsg = sCreateGetParamsRequest(zPort, zParameterName);
	if ( yGetParamsMsg.empty() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, "Empty request generated. Unable to send GET-PARAMS request.");
		return(TEL_FAILURE);
	}

	string		yServerState = "";
	string 		yRecvMsg;
	int         yCompletionCode;
    int         yStatusCode;
    string      yEventName;
	
    rc = processMRCPRequest (zPort, yGetParamsMsg, yRecvMsg,
                yServerState, &yStatusCode, &yCompletionCode, yEventName); 

		
	if ( yStatusCode >= 300 )
	{
		return(-1);
	}
	
	if ( yRecvMsg.size() <= 0 )
	{
//		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, MRCP_2_BASE, ERR,
//			"Received empty response (%s); Parameter (%s) not retrieved.",
//			yRecvMsg.c_str(), zParameterName);
		return(-1);
	}

	Mrcp2_Response      mrcpResponse;
	MrcpHeaderList      headerList;
	mrcpResponse.addMessageBody(yRecvMsg);
	headerList = mrcpResponse.getHeaderList(); // headerList
	zParameterValue[0] = '\0';

	while(!headerList.empty())
	{
		MrcpHeader      header;
		header = headerList.front();
		
		string name = header.getName();
		string value = header.getValue();
		if (name == zParameterName)
		{
			sprintf(zParameterValue, "%s", value.c_str());
		}

		headerList.pop_front();

	} // END: headerList.empty()

	if ( zParameterValue[0] == '\0' )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR,
				"Did not receive parameter (%s) in response.", zParameterName);
		return(-1);
	}

//	cleanPStr(zParameterValue, strlen(zParameterValue));

	return(0);

} /* END: TTSGetParams() */

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

/*------------------------------------------------------------------------------
cleanPStr()
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


