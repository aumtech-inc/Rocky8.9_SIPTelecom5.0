/*-----------------------------------------------------------------------------
Program:        TEL_SRGetResult.c 
Purpose:        To get the speech recognition results.
Author:         Anand Joglekar
Date:			10/01/2002
Update: 07/27/2005 djb  Added a null character to the result after it is read
                        from the file.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, int *zAlternativeNumber, int zTokenType );

int TEL_SRGetResult(int zAlternativeNumber, int zTokenType, char *zDelimiter, 
	char *zResult, int *zpOverallConfidence)
{
	static char mod[]="TEL_SRGetResult";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;
	int		yAlternativeNumber;

	struct MsgToApp response;
	struct Msg_SRGetResult lMsg;
	struct MsgToDM msg; 

	if ( GV_SRType == GOOGLE_SR )
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR is enabled. Returning with no action.");
		return(0);
	}

	sprintf(apiAndParms,"%s(%d, %s, %s, Result, &OverallConfidence)",
		mod, zAlternativeNumber, convertTokenType(zTokenType), zDelimiter);

	yAlternativeNumber = zAlternativeNumber;
	rc = apiInit(mod, TEL_SRGETRESULT, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if (!parametersAreGood(mod, &yAlternativeNumber, zTokenType)) 
		HANDLE_RETURN(-1);

 	*zpOverallConfidence = 0;
	zResult[0] = '\0';

	lMsg.opcode = DMOP_SRGETRESULT;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;

	lMsg.alternativeNumber = yAlternativeNumber;
	lMsg.tokenType = zTokenType;
	sprintf(lMsg.delimiter, "%s", zDelimiter);

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRGetResult));

#ifdef MRCP_SR
	rc = sendRequestToSRClient(mod, &msg);
#else
	rc = sendRequestToDynMgr(mod, &msg); 
#endif

	if (rc == -1) HANDLE_RETURN(-1);

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,60,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Timeout waiting for response from Dynamic Manager.");
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_SRGETRESULT:
			if(response.returnCode == 0)
			{
				char *pChar;
				int lFd;
				char lFileName[256];

				memset(lFileName, 0, sizeof(lFileName));

				sscanf(response.message, "%d^%s", zpOverallConfidence,
					lFileName);

				lFd= open(lFileName, O_RDONLY);
				if(lFd == -1)
				{
					*zResult = '\0';
					telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
						"Failed to open the result file (%s). [%d, %s]",
						lFileName, errno, strerror(errno));
					HANDLE_RETURN(TEL_FAILURE);
				}

				rc = read(lFd, zResult, 1024);
				if(rc == 0)
				{
					*zResult = '\0';
					telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
						"Failed to read from the result file (%s). [%d, %s]",
						lFileName, errno, strerror(errno));
					HANDLE_RETURN(TEL_FAILURE);
				}
				zResult[rc] = '\0';
	
				close(lFd);	
				unlink(lFileName);
			}
			HANDLE_RETURN(response.returnCode);
			break;
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;
		case DMOP_DISCONNECT:
   			HANDLE_RETURN(disc(mod, response.appCallNum));
			break;
		default:
			/* Unexpected message. Logged in examineDynMgr... */
			continue;
			break;
		} /* switch rc */
	} /* while */
}

/*-------------------------------------------------------------------------
This function verifies that all parameters are valid.
It returns 1 (yes) if all parmeters are good, 0 (no) if not.
-------------------------------------------------------------------------*/
static int parametersAreGood(char *mod, int *zAlternativeNumber, int zTokenType )
{
    char m[MAX_LENGTH];
    int  errors=0;

    if( ( *zAlternativeNumber < 1 ) || ( *zAlternativeNumber > 3 ))
    {
//		errors++;
		sprintf(m, "%s", "Invalid alternative number (%d).  Should be between 1 and 3.  "
				"Setting to the default of 1." );
		telVarLog(mod, REPORT_DETAIL, TEL_BASE, GV_Warn, m);
		*zAlternativeNumber = 1;
    }

	switch (zTokenType)
	{
        case SR_TAG:
        case SR_INPUT_MODE:
        case SR_SPOKEN_WORD:
        case SR_CONCEPT_AND_ATTRIBUTE_PAIR:
        case SR_CONCEPT_AND_WORD:
            break;
        default:
			sprintf(m, 
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
			errors++;
			telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
			break;
	}

    return(errors == 0);
}


