/*-----------------------------------------------------------------------------
Program:SRSetParameter.c
Author:	Aumtech, Inc.
Update:	08/01/2006 djb Ported to mrcpV2.
Update:	01/03/2007 djb Added MRCP SET-PARAMS logic.
------------------------------------------------------------------------------*/
#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"
#include "parseResult.hpp"

using namespace mrcp2client;

extern "C"
{
	#include <stdio.h>
	#include <string.h>
	#include <errno.h>
	#include <unistd.h>
	#include "Telecom.h"
	#include "arcSR.h"
}

string sCreateSetParamsRequest(int zPort, char *zParamName,
                                char *zParamValue);

int SRSetParameter(int zPort, char *zParameterName, char *zParameterValue)
{
	static char mod[] = "SRSetParameter";
	string		lTmpString;
	int			rc;
	char		*pValue;

    if ( ! gSrPort[zPort].IsCallActive() )
    {
       mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL,
            MRCP_2_BASE, WARN, "Call is no longer active.");
        return(TEL_DISCONNECTED);
    }

    if ( ! gSrPort[zPort].IsTelSRInitCalled() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
			MRCP_2_BASE, ERR, 
	   		"SRSetParameter failed: Must call SRInit first.");
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
		char			yBigParamValue[1024];

		if (strncmp(zParameterValue, "FILE:", 5) == 0)
		{
			FILE *fp;
			char yStrFileName[128];
			sprintf(yStrFileName, "%s", &(zParameterValue[5]));
	
			if((fp = fopen(yStrFileName, "r")) == NULL)
			{
				mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
					MRCP_2_BASE, ERR, 
					"Failed to open file (%s) for output. [%d, %s] Unable to set MRCP: parameter.",
					yStrFileName, errno, strerror(errno));
				return(-1);
			}
	
			memset((char *) yBigParamValue, '\0', sizeof(yBigParamValue));
			fscanf(fp, "%s", yBigParamValue);
			fclose(fp);
			unlink(yStrFileName);
			pValue	= yBigParamValue;
		}
		else
		{
			pValue = zParameterValue;
		}


		string ySetParamsMsg = sCreateSetParamsRequest(zPort, 
						&(zParameterName[5]), pValue);
		if ( ySetParamsMsg.empty() )
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
						MRCP_2_BASE, ERR,
						"Empty request generated.  "
						"Unable to send SET-PARAMS request.");
			return(TEL_FAILURE);
		}

		string serverState = "";
		string  recvMsg;
		int     statusCode  = -1;
		int     causeCode   = -1;;
		
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Attempting to set %s=%s directly on MRCP server.",
			&(zParameterName[5]), pValue);
		rc = processMRCPRequest(zPort, ySetParamsMsg, recvMsg,
				serverState, &statusCode, &causeCode);
		if ( statusCode >= 300 )
		{
			return(-1);
		}

		if ( strcmp(&(zParameterName[5]), "DTMF-Interdigit-Timeout") == 0 )
		{
			// MR 4975 - Must do this for interdigit timeout because if 
			// it's not set, it will default to trail-silence timeout. djb

			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
					MRCP_2_BASE, INFO, "Setting %s=%s internally.",
					&(zParameterName[5]), pValue);
			gSrPort[zPort].setInterdigitTimeout(pValue);
		}
		else if ( strcmp(&(zParameterName[5]), "DTMF-Term-Timeout") == 0 )
		{
			// MR 4975 - Must do this for dtmf term timeout because if 
			// it's not set, it will default to trail-silence timeout. djb

			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
					MRCP_2_BASE, INFO, "Setting %s=%s internally.",
					&(zParameterName[5]), pValue);
			gSrPort[zPort].setTermTimeout(pValue);
		}
		else if ( strcmp(&(zParameterName[5]), "Confidence-Threshold") == 0 )
		{
			// MR 4975 - Must do this for Confidence-Threshold because it
			// has a default value which will override this setting. 

			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
					MRCP_2_BASE, INFO, "Setting %s=%s internally.",
					&(zParameterName[5]), pValue);
			gSrPort[zPort].setConfidenceThreshold(pValue);
		}
		else if ( strcmp(&(zParameterName[5]), "Speed-Vs-Accuracy") == 0 )
		{
			// MR 4975 - Must do this for Speed-Vs-Accuracy because it
			// has a default value which will override this setting. 

			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
					MRCP_2_BASE, INFO, "Setting %s=%s internally.",
					&(zParameterName[5]), pValue);
			gSrPort[zPort].setSpeedAccuracy(pValue);
		}

		return(rc);
	}

 	if(strcmp(zParameterName, "$SR_SESSION_RECORD") == 0)
	{
		if(strcmp(zParameterValue, "YES") == 0)
		{
			gSrPort[zPort].setSessionRecord(true);
		}
		else if(strcmp(zParameterValue, "NO") == 0)
		{
			gSrPort[zPort].setSessionRecord(false);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
				"Invalid parameter value: %s. Valid values are YES "
				"and NO.", zParameterValue);
			return(TEL_FAILURE);
		}
	}
	else if(strcmp(zParameterName, "$SR_SESSION_PATH") == 0)
	{
		if(strlen(zParameterValue) > 0)
		{
			gSrPort[zPort].setSessionPath(string(zParameterValue));
		}
		else
		{
			gSrPort[zPort].setSessionPath("");
		}
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Successfully set SR_SESSION_PATH to (%s).",
			gSrPort[zPort].getSessionPath().c_str());
	}
	else if(strcmp(zParameterName, "$SR_GRAMMAR_AFTER_VAD") == 0)
	{
		if(strcmp(zParameterValue, "YES") == 0)
		{
			gSrPort[zPort].setGrammarAfterVAD(true);
		}
		else if(strcmp(zParameterValue, "NO") == 0)
		{
			gSrPort[zPort].setGrammarAfterVAD(false);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, 
				"Invalid value (%s) for parameter (%s). "
				"Valid values are YES and NO.",
				zParameterValue, zParameterName);
            return(TEL_FAILURE);
        }
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Successfully set SR_GRAMMAR_AFTER_VAD to (%d).",
			gSrPort[zPort].IsGrammarAfterVAD());
    }
	else if(strcmp(zParameterName, "$SR_GRAMMAR_NAME") == 0)
	{
		if(strlen(zParameterValue) > 0)
		{
			lTmpString = zParameterValue;
		}
		else
		{
			lTmpString = "";
		}
		gSrPort[zPort].setGrammarName(lTmpString);
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Successfully set SR_GRAMMAR_NAME to (%s).",
			gSrPort[zPort].getGrammarName().c_str());
    }
	else if(strcmp(zParameterName, "$SR_FAIL_ON_GRAMMAR_LOAD") == 0)
	{
		if(strcmp(zParameterValue, "YES") == 0)
		{
			gSrPort[zPort].setFailOnGrammarLoad(true);
		}
		else if(strcmp(zParameterValue, "NO") == 0)
		{
			gSrPort[zPort].setFailOnGrammarLoad(false);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL, 
				MRCP_2_BASE, ERR, "Invalid value (%s) for parameter (%s). "
				"Valid values are YES and NO.",
				zParameterValue, zParameterName);
            return(TEL_FAILURE);
        }
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, 
			"Successfully set SR_FAIL_ON_GRAMMAR_LOAD to %d.",
			gSrPort[zPort].IsFailOnGrammarLoad());
	}
	else if(strcmp(zParameterName, "$SR_UTTERANCE_PATH_64P") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received SR_UTTERANCE_PATH_64P: "
			"Doing nothing.");
	}
	else if(strcmp(zParameterName, "$SR_LANGUAGE") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received SR_LANGUAGE:%s", zParameterValue);

		gSrPort[zPort].setLanguage(zParameterValue);
	}
	else if(strcmp(zParameterName, "$SENSITIVITY_LEVEL") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received SENSITIVITY_LEVEL:%s", zParameterValue);

		gSrPort[zPort].setSensitivity(zParameterValue);
	}
	else if(strcmp(zParameterName, "$RECOGNITION_MODE") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			SR_BASE, INFO, "Received RECOGNITION_MODE:%s", zParameterValue);

		gSrPort[zPort].setRecognitionMode(zParameterValue);
    }
	else if(strcmp(zParameterName, "$CONFIDENCE_LEVEL") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received CONFIDENCE_LEVEL:%s", zParameterValue);

		gSrPort[zPort].setConfidenceThreshold(zParameterValue);
	}
	else if(strcmp(zParameterName, "$SPEED_ACCURACY") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received SPEED_ACCURACY:%s", zParameterValue);

		gSrPort[zPort].setSpeedAccuracy(zParameterValue);
	}
	else if(strcmp(zParameterName, "$MAX_N_BEST") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received MAX_N_BEST:%s", zParameterValue);

		gSrPort[zPort].setMaxNBest(zParameterValue);
	}
	else if(strcmp(zParameterName, "$INTERDIGIT_TIMEOUT") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received INTERDIGIT_TIMEOUT:%s", zParameterValue);

		gSrPort[zPort].setInterdigitTimeout(zParameterValue);
	}
	else if(strcmp(zParameterName, "$INCOMPLETE_TIMEOUT") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received INCOMPLETE_TIMEOUT:%s", zParameterValue);

		gSrPort[zPort].setIncompleteTimeout(zParameterValue);
	}
	else if(strcmp(zParameterName, "$TERMDIGIT_TIMEOUT") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received TERMDIGIT_TIMEOUT:%s", zParameterValue);

		gSrPort[zPort].setTermTimeout(zParameterValue);
	}
	else if(strcmp(zParameterName, "$TERM_CHAR") == 0)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE, 
			MRCP_2_BASE, INFO, "Received TERM_CHAR:%s", zParameterValue);

		gSrPort[zPort].setTermChar(zParameterValue);
	}
    else if(strcmp(zParameterName, "$HIDE_DTMF") == 0)
    {
        if(strcmp(zParameterValue, "0") == 0)
        {
            gSrPort[zPort].setHideDTMF(false);
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
                MRCP_2_BASE, INFO, "Received HIDE_DTMF:%s (false). Recognition results will be logged.", zParameterValue);
        }
        else if(strcmp(zParameterValue, "1") == 0)
        {
            gSrPort[zPort].setHideDTMF(true);
            mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
                MRCP_2_BASE, INFO, "Received HIDE_DTMF:%s (true). Recognition results will not be logged.", zParameterValue);
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

//	else if(strcmp(zParameterName, "$SR_UTTERANCE_PATH_64P")==0)//not supported 
//	{
//		sprintf(srPort.gUtterancePath64P, "%s", zParameterValue);
//		srVarLog(gMod, zPort, REPORT_VERBOSE, SR_INFO,
//			"Successfully set SR_UTTERANCE_PATH_64P to (%s).",
//			srPort.gUtterancePath64P);
//	}
	return(TEL_SUCCESS);
} /* END: SRSetParameter() */

/*------------------------------------------------------------------------------
sCreateSetParamsRequest():
------------------------------------------------------------------------------*/
string sCreateSetParamsRequest(int zPort, char *zParamName,
                                char *zParamValue)
{
	static char						mod[]="sCreateSetParamsRequest";
    int     rc = 0;
    MrcpHeader                      header;
    MrcpHeaderList                  headerList;
    int                             yCounter;
    string                          yContent = "";
    char                            yContentLength[16];
    string                          ySetParamsMsg;
	string							yParamName;
	string							yParamValue;

	yParamName	= zParamName;
	yParamValue	= zParamValue;

    header.setNameValue("Channel-Identifier", gSrPort[zPort].getChannelId());
    headerList.push_back(header);

	header.setNameValue(yParamName, yParamValue);
	headerList.push_back(header);

    gSrPort[zPort].incrementRequestId();
    ClientRequest setParamsRequest(
        gMrcpInit.getMrcpVersion(),
        "SET-PARAMS",
        gSrPort[zPort].getRequestId(),
        headerList,
		0);

    string setParamsMsg = setParamsRequest.buildMrcpMsg();
    ySetParamsMsg = setParamsMsg + yContent;

	return(ySetParamsMsg);
} // END: sCreateSetParamsRequest()

