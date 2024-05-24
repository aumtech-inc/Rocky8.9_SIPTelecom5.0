/*-----------------------------------------------------------------------------
Program:        TEL_SRDeleteWords.c 
Purpose:        To delete words from the grammar.
Author:         Anand Joglekar
Date:			10/01/2002
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, char *zResourceName, 
	char *zCategoryName, struct SR_WORDS *zpWordList, int zNumWords);
static int createDeleteWordsListFile(char *mod, char *zResourceName, 
	char *zCategoryName, int zNumWords, struct SR_WORDS *zpWordList,char* file);

int TEL_SRDeleteWords(char * zResourceName, char *zCategoryName,
	struct SR_WORDS *zpWordList, int zNumWords, int *zpVendorErrorCode)
{
	static char mod[]="TEL_SRDeleteWords";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	struct MsgToApp response;
	struct Msg_SRDeleteWords lMsg;
	struct MsgToDM msg; 
       
	*zpVendorErrorCode = 0;
	sprintf(apiAndParms,"%s(%s, %s, struct SR_WORDS[], %d, &VendorErrorCode)",
		mod, zResourceName, zCategoryName, zNumWords);
	rc = apiInit(mod, TEL_SRDELETEWORDS, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if(!parametersAreGood(mod, zResourceName, zCategoryName, 
		zpWordList, zNumWords))
	{
		HANDLE_RETURN(-1);
	}

	memset(lMsg.deleteWordsListFile, 0, sizeof(lMsg.deleteWordsListFile));
	rc = createDeleteWordsListFile(mod, zResourceName, zCategoryName,
			zNumWords, zpWordList, lMsg.deleteWordsListFile);
	if (rc != 0) 
	{
		unlink(lMsg.deleteWordsListFile);
		HANDLE_RETURN(rc);
	}
	
	lMsg.opcode = DMOP_SRDELETEWORDS;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRDeleteWords));

#ifdef MRCP_SR
	rc = sendRequestToSRClient(mod, &msg);
#else
	rc = sendRequestToDynMgr(mod, &msg); 
#endif

	if (rc == -1) 
	{
		unlink(lMsg.deleteWordsListFile);
		HANDLE_RETURN(-1);
	}

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,60,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Timeout waiting for response from Dynamic Manager.");
			unlink(lMsg.deleteWordsListFile);
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if (rc == -1) 
		{
			unlink(lMsg.deleteWordsListFile);
			HANDLE_RETURN(TEL_FAILURE);
		}

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_SRDELETEWORDS:
			unlink(lMsg.deleteWordsListFile);
			*zpVendorErrorCode = response.vendorErrorCode;
			HANDLE_RETURN(response.returnCode);
			break;
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;
		case DMOP_DISCONNECT:
			unlink(lMsg.deleteWordsListFile);
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
static int parametersAreGood(char *mod, char *zResourceName, 
	char *zCategoryName, struct SR_WORDS *zpWordList, int zNumWords)
{
	char m[MAX_LENGTH];
	int  errors=0;

	if(zResourceName[0] == '\0')
	{
		errors++;
		sprintf(m, "%s", "TEL_SRDeleteWords failed. Resource name must not be empty.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
	}

	if(zCategoryName[0] == '\0')
	{
		errors++;
		sprintf(m, "%s", "TEL_SRDeleteWords failed. Category name must not be empty.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
	}

	if(zNumWords < 1)
	{
		errors++;
		sprintf(m, "%s", "TEL_SRDeleteWords failed. Number of words must be atleast one.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
	}

	if(zpWordList[0].wordSpelling[0] == '\0')
	{
		errors++;
		sprintf(m, "%s", "TEL_SRDeleteWords failed. Word list must contain atleast 1 word.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
	}

	return(errors == 0);   
}

static int createDeleteWordsListFile(char *mod, char *zResourceName, 
	char *zCategoryName, int zNumWords, struct SR_WORDS *zpWordList, char *file)
{
	FILE *fp;
	char fileName[20];
	char m[MAX_LENGTH];
	int i;

	*file = '\0';
	sprintf(fileName, "DelWordList.%d.%d", GV_AppPid, GV_AppRef);

	fp = fopen(fileName, "w+");
	if(fp == NULL)
	{
		sprintf(m, "Failed to open file (%s) for writing.  [%d, %s]",
			fileName, errno, strerror(errno));
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
		return(-1);
	}

	fprintf(fp, "%s\n%s\n%d\n", zResourceName, zCategoryName, zNumWords);

	for(i=0; i<zNumWords; i++)
	{
		fprintf(fp, "%s\n", zpWordList[i].wordSpelling);
		fprintf(fp, "%s\n", zpWordList[i].variantSpelling);
		fprintf(fp, "%s\n", zpWordList[i].tag);
		fprintf(fp, "%s\n", zpWordList[i].phoneticSpelling);
	}

	fclose(fp);

	sprintf(file, "%s", fileName);
	return(0);
}
