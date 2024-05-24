/*-----------------------------------------------------------------------------
Program:SRGetResult.c
Author:	Aumtech, Inc.
Update:	08/01/2006 djb Ported to mrcpV2.
------------------------------------------------------------------------------*/
#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"
#include "parseResult.hpp"

using namespace mrcp2client;

extern "C"
{
	#include <stdio.h>
	#include <string.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <errno.h>

	#include "Telecom.h"
	#include "arcSR.h"
}

extern XmlResultInfo           parsedResults[];

int sBuildResult(int zPort, char *zOutputStr);

/*------------------------------------------------------------------------------
SRGetResult():
------------------------------------------------------------------------------*/
int SRGetResult(int zPort, int zAlternativeNumber, int zTokenType,
    char *zDelimiter, char *zResult, int *zOverallConfidence)
{
	static char				mod[] = "DM_SRGetResult";
	int						yCounter;
	int						yRc;
	char					yTmpFile[128];
	int						yTmpFd;
	char					yTokenType[128] = "";

	*zResult = '\0';

	// djb.  zAlternativeNumber is ignored.

	if ( !gSrPort[zPort].IsTelSRInitCalled() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, INFO, "SRGetResult failed.  Must call SRInit first.");
		return(TEL_FAILURE);
	}

	if( !gSrPort[zPort].IsCallActive() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL,
			MRCP_2_BASE, INFO, "Call is no longer active.");
		return(TEL_DISCONNECTED);
	}

	switch(zTokenType)
	{
		case SR_TAG:
			sprintf(yTokenType, "SR_TAG(%d)", SR_TAG);
			break;
		case SR_INPUT_MODE:
			sprintf(yTokenType, "SR_INPUT_MODE(%d)", SR_INPUT_MODE);
			break;
		case SR_SPOKEN_WORD:
			sprintf(yTokenType, "SR_SPOKEN_WORD(%d)", SR_SPOKEN_WORD);
			break;
		case SR_CONCEPT_AND_ATTRIBUTE_PAIR:
			sprintf(yTokenType, "SR_CONCEPT_AND_ATTRIBUTE_PAIR(%d)", SR_CONCEPT_AND_ATTRIBUTE_PAIR);
			break;
		case SR_CONCEPT_AND_WORD:
			sprintf(yTokenType, "SR_CONCEPT_AND_WORD(%d)", SR_CONCEPT_AND_WORD);
			break;
		default:
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, 
				"Invalid token type:%d.  Must be one of: "
				"SR_TAG(%d), SR_INPUT_MODE(%d), SR_SPOKEN_WORD(%d), "
				"SR_CONCEPT_AND_ATTRIBUTE_PAIR(%d), or "
				"SR_CONCEPT_AND_WORD(%d).",
				zTokenType,
				SR_TAG,
				SR_INPUT_MODE,
				SR_SPOKEN_WORD,
 				SR_CONCEPT_AND_ATTRIBUTE_PAIR,
				SR_CONCEPT_AND_WORD);
			return(TEL_FAILURE);
			break;
	}

	if (  parsedResults[zPort].numResults <= 0 )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, INFO, "No recognition results are available.  "
			"Returning failure.");
		return(TEL_FAILURE);
	}
    if ( ! gSrPort[zPort].getHideDTMF() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, 
			"Received token type is %s; "
			"parsedResults[%d].inputMode=(%s); "
			"parsedResults[%d].inputValue=(%s); "
			"parsedResults[%d].instance=(%s); ",
				yTokenType, 
				zPort, parsedResults[zPort].inputMode,
				zPort, parsedResults[zPort].inputValue,
				zPort, parsedResults[zPort].instance);
	}

	if ( zTokenType == SR_INPUT_MODE )
    {
        sprintf(zResult, "%s", parsedResults[zPort].inputMode);
        *zOverallConfidence = 0;
    }
    else
    {
	    *zOverallConfidence = parsedResults[zPort].confidence;

		switch( zTokenType )
		{
			case SR_SPOKEN_WORD:
				sprintf(zResult, "%s", parsedResults[zPort].inputValue);
				break;
			case SR_TAG:
				sprintf(zResult, "%s", parsedResults[zPort].instance);
				break;
			case SR_CONCEPT_AND_ATTRIBUTE_PAIR:
//				sprintf(zResult, "%s!empty|returnValue|%s", 
				sprintf(zResult, "%s|MEANING|%s", 
							parsedResults[zPort].resultGrammar,
							parsedResults[zPort].instance);
				break;
		}
		if ( zResult[0] == '\0' )
		{
			if ( parsedResults[zPort].numInstanceChildren == 0 )
			{
				switch( zTokenType )
				{
#if 0
					case SR_CONCEPT_AND_ATTRIBUTE_PAIR:
//						sprintf(zResult, "%s!empty|returnValue|%s", 
						sprintf(zResult, "%s|MEANING|%s", 
							parsedResults[zPort].resultGrammar,
							parsedResults[zPort].instance);
						break;
#endif
#if 0
					case SR_CONCEPT_AND_TAG:
						sprintf(zResult, "%s!empty|%s", 
									srPort.getResult.resultGrammar,
									srPort.getResult.instance);
						break;
#endif
					case SR_CONCEPT_AND_WORD:
						sprintf(zResult, "%s|%s", 
							parsedResults[zPort].resultGrammar,
							parsedResults[zPort].inputValue);
						break;
				}
			}
			else
			{
				(void) sBuildResult(zPort, zResult);
			}
		}
	}
	sprintf(yTmpFile, "/tmp/%s.%d", mod, zPort);

    if ((yTmpFd = open(yTmpFile, O_CREAT|O_TRUNC|O_WRONLY, 0666)) == -1)
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
					MRCP_2_BASE, INFO, 
        			"Unable to open result file (%s) for writing. [%d, %s]  "
					"Cannot return result.",
	                yTmpFile, errno, strerror(errno));
        return(TEL_FAILURE);
    }

	if ((yRc = write(yTmpFd, zResult, strlen(zResult))) != strlen(zResult))
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
					MRCP_2_BASE, INFO, 
					"Error writing to file (%s), [%d, %s]  ",
					"Cannot return result.",
                    yTmpFile, errno, strerror(errno));
		close(yTmpFd);
		if ((yRc = access(yTmpFile, F_OK)) == 0)
		{
			unlink(yTmpFile);
		}
        return(TEL_FAILURE);
	}

    if ( ! gSrPort[zPort].getHideDTMF() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
			MRCP_2_BASE, INFO, "Successfully wrote %d bytes (%s) to (%s).",
			yRc, zResult, yTmpFile);
	}
	
	close(yTmpFd);
	sprintf(zResult, "%s", yTmpFile);
	return(TEL_SUCCESS);

} /* END: SRGetResult() */

/*------------------------------------------------------------------------------sBuildResult():
------------------------------------------------------------------------------*/
int sBuildResult(int zPort, char *zOutputStr)
{

	char		yTmpOutput[4096] = "";
	int			i;


	for (i = 0; i < parsedResults[zPort].numInstanceChildren; i++)
	{
		if ( i == 0 )
		{
			sprintf(zOutputStr, "%s|%s|%s",
					parsedResults[zPort].resultGrammar,
					parsedResults[zPort].instanceChild[i].instanceName,
					parsedResults[zPort].instanceChild[i].instanceValue);
			continue;
		}

		sprintf(yTmpOutput, "^%s|%s|%s",
					parsedResults[zPort].resultGrammar,
					parsedResults[zPort].instanceChild[i].instanceName,
					parsedResults[zPort].instanceChild[i].instanceValue);
		strcat(zOutputStr, yTmpOutput);
	}
	if ( parsedResults[zPort].literalTimings[0] != '\0' )
	{
		sprintf(yTmpOutput, "^%s|SWI_literalTimings|%s",
					parsedResults[zPort].resultGrammar,
					parsedResults[zPort].literalTimings);
		strcat(zOutputStr, yTmpOutput);
	}

	return(0);
} // END: sBuildResult() 
