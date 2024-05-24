/*-----------------------------------------------------------------------------
Program:    TEL_SRLoadGrammar.c
Purpose:    To load the philips grammar.
Author:     Aumtech
Date:       10/01/2002
Update: djb 08102005 Added SR_GRAM_ID validation.
Update: djb 10082009 Added weight for SRLoadGrammar.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, int zGrammarType, 
	char * zGrammarData, char * zParameters);
static int createLoadGrammarFile(char *mod, char *zpGrammarData, char *file);

int TEL_SRLoadGrammar(int zGrammarType, char * zGrammarData,
    char * zParameters, int *zpVendorErrorCode)
{
	static char mod[]="TEL_SRLoadGrammar";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	struct MsgToApp response;
	struct Msg_SRLoadGrammar lMsg;
	struct MsgToDM msg; 
       
	if ( GV_SRType == GOOGLE_SR )
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR is enabled. Returning with no action.");
		return(0);
	}

	sprintf(apiAndParms,"%s(%d, zGrammarData, %s, &VendorErrorCode)", mod,
		zGrammarType, zParameters);
	rc = apiInit(mod, TEL_SRLOADGRAMMAR, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if (!parametersAreGood(mod, zGrammarType, zGrammarData, zParameters))
		HANDLE_RETURN(-1);

	memset((struct Msg_SRLoadGrammar *)&lMsg, '\0', sizeof(lMsg));
	if((zGrammarType == SR_STRING) || (zGrammarType == SR_URI))
	{
		rc = createLoadGrammarFile(mod, zGrammarData, lMsg.grammarFile);
		if(rc != 0)
		{
			unlink(lMsg.grammarFile);
			HANDLE_RETURN(-1);
		}
	}
	else
	{
		sprintf(lMsg.grammarFile, "%s", zGrammarData);
	}

	lMsg.opcode = DMOP_SRLOADGRAMMAR;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;
	lMsg.weight		= 1.0;
	lMsg.grammarType = zGrammarType;
	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRLoadGrammar));

#ifdef MRCP_SR
	rc = sendRequestToSRClient(mod, &msg);
#else
	rc = sendRequestToDynMgr(mod, &msg); 
#endif

	if (rc == -1) 
	{
		if( (zGrammarType == SR_STRING) ||
			(zGrammarType == SR_URI) )
		{
			unlink(lMsg.grammarFile);
		}
		HANDLE_RETURN(-1);
	}

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,60,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Timeout waiting for response from Dynamic Manager.");

			if((zGrammarType == SR_STRING) || (zGrammarType == SR_URI))
			{
				unlink(lMsg.grammarFile);
			}

			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) 
		{
			if((zGrammarType == SR_STRING) || (zGrammarType == SR_URI))
			{
				unlink(lMsg.grammarFile);
			}

			HANDLE_RETURN(TEL_FAILURE);
		}

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_SRLOADGRAMMAR:
			*zpVendorErrorCode = response.vendorErrorCode;
			if((zGrammarType == SR_STRING) || (zGrammarType == SR_URI))
			{
				unlink(lMsg.grammarFile);
			}
			HANDLE_RETURN(response.returnCode);
			break;
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;
		case DMOP_DISCONNECT:
			if((zGrammarType == SR_STRING) || (zGrammarType == SR_URI))
			{
				unlink(lMsg.grammarFile);
			}
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
static int parametersAreGood(char *mod, int zGrammarType, 
	char * zGrammarData, char * zParameters)
{
	char m[MAX_LENGTH];
	int  errors=0;

	if(zGrammarData == (char*)0)
	{
		errors++;
		sprintf(m, "%s", "TEL_SRLoadGrammar failed.Grammar string must not be empty.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
	}

#ifdef MRCP_SR
	if(zGrammarType != SR_STRING && zGrammarType != SR_FILE &&
			zGrammarType != SR_URI && SR_GRAM_ID != SR_GRAM_ID)
	{
		errors++;
		sprintf(m,  "%s", "TEL_SRLoadGrammar failed. Grammar Type must be SR_FILE"
			", SR_STRING or SR_URI.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
	}
#else
	if(zGrammarType != SR_STRING && zGrammarType != SR_FILE &&
			zGrammarType != SR_URI && zGrammarType != SR_GRAM_ID)
	{
		errors++;
		sprintf(m,  "%s", "TEL_SRLoadGrammar failed. Grammar Type must be SR_FILE"
			", SR_STRING, SR_URI, or SR_GRAM_ID.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
	}
#endif

	return(errors == 0);   
}

static int createLoadGrammarFile(char *mod, char *zpGrammarData, char *file)
{
	FILE *fp;
	char fileName[128];
	char m[MAX_LENGTH];
	int i;

//	*file = '\0';
	sprintf(fileName, "LoadGrammar.%d.%d", GV_AppPid, GV_AppRef);

	fp = fopen(fileName, "w+");
	if(fp == NULL)
	{
        sprintf(m, "Failed to open file (%s) for writing. [%d, %s]",
            fileName, errno, strerror(errno));
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
		return(-1);
	}

	fprintf(fp, "%s", zpGrammarData);

	fclose(fp);

	sprintf(file, "%s", fileName);
	return(0);
}
