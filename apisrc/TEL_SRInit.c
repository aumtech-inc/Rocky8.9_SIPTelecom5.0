/*-----------------------------------------------------------------------------
Program:        TEL_SRInit.c 
Purpose:        To do Speech Rec initialization.
Author:         Anand Joglekar
Date:			09/27/2002
-----------------------------------------------------------------------------*/

#include <stdlib.h>

#include "TEL_Common.h"

static int parametersAreGood(char *mod, const char *zResourceNames);

static int createResourceNamesFile(char *mod, const char *zResourceNames, 
	char *file);


int TEL_SRInit(const char *zResourceNames, int *zpVendorErrorCode)
{
	static char mod[]="TEL_SRInit";
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	struct MsgToApp response;
	struct Msg_SRInit lMsg;
	struct MsgToDM msg; 
	char	tmpResourceNames[64];
       
	*zpVendorErrorCode = 0;
	sprintf(apiAndParms,"%s(%s, &VendorErrorCode)", mod, zResourceNames);

	rc = apiInit(mod, TEL_SRINIT, apiAndParms, 1, 0, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	sprintf(tmpResourceNames, "%s", zResourceNames);
	if (!parametersAreGood(mod, tmpResourceNames)) 
		HANDLE_RETURN(-1);

	if ( GV_SRType == GOOGLE_SR )
	{
        telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
            "Google SR is enabled. Returning with no action.");
		return(0);
	}

#ifdef MRCP_SR
	rc=openChannelToSRClient(mod);
	if (rc == -1) HANDLE_RETURN(TEL_FAILURE);
#endif

	sprintf(lMsg.resourceNamesFile, "%s", GV_ResponseFifo);
//	memset(lMsg.resourceNamesFile, 0, sizeof(lMsg.resourceNamesFile));
//	rc = createResourceNamesFile(mod, zResourceNames, lMsg.resourceNamesFile);
//	if (rc != 0) 
//	{
//		unlink(lMsg.resourceNamesFile);
//		HANDLE_RETURN(rc);
//	}

	lMsg.opcode = DMOP_SRINIT;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppCallNum1;
	sprintf(lMsg.appName, "%s", GV_AppName);
	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRInit));

#ifdef MRCP_SR
	rc = sendRequestToSRClient(mod, &msg);
#else
	rc = sendRequestToDynMgr(mod, &msg); 
#endif

	if (rc < 0)
	{
//		unlink(lMsg.resourceNamesFile);
		HANDLE_RETURN(-1);
	}

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,60,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
#ifdef MRCP_SR
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
					"Timeout waiting for response from mrcpClient2.");
#else
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
					"Timeout waiting for response from Dynamic Manager.");
#endif
			
//			unlink(lMsg.resourceNamesFile);
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) 
		{
//			unlink(lMsg.resourceNamesFile);
			HANDLE_RETURN(TEL_FAILURE);
		}

		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_SRINIT:
//			unlink(lMsg.resourceNamesFile);
			*zpVendorErrorCode = response.vendorErrorCode;

			/*DDN: Support multiple clients*/
			if(response.returnCode == 0 && response.message[0])
			{

				GV_AttchaedSRClient = atoi(response.message);

				if(GV_AttchaedSRClient >= 0)
				{

					GV_TotalSRRequests = 1;
					
#if 1
					closeChannelToSRClient(mod);

					if( openChannelToSRClient(mod) != 0)
					{
						HANDLE_RETURN(-1);
					}
#endif
#if 0
printf("DDNDEBUG: %s %s %d Attached SR Client(%d) to Port %d\n", __FILE__, __FUNCTION__, __LINE__, GV_AttchaedSRClient, GV_AppCallNum1);fflush(stdout);
#endif
				}
			}

			HANDLE_RETURN(response.returnCode);
			break;
		case DMOP_GETDTMF:
//			unlink(lMsg.resourceNamesFile);
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;
		case DMOP_DISCONNECT:
//			unlink(lMsg.resourceNamesFile);
   			HANDLE_RETURN(disc(mod, response.appCallNum));
			break;
		default:
//			unlink(lMsg.resourceNamesFile);
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
static int parametersAreGood(char *mod, const char *zResourceNames)
{
	char m[MAX_LENGTH];
	int  errors=0;
 
	if(zResourceNames[0] == '\0' || zResourceNames[0] == ' ')
	{
		// zResourceNames is deprecated
		sprintf((char *)zResourceNames, "%s", "defaultResourceName");
//		errors++;
//		sprintf(m,"Invalid parameter zResourceNames. zResourceNames is NULL" );
//		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
	}

	return(errors == 0);   
}

static int createResourceNamesFile(char *mod, const char *zResourceNames, 
	char *file)
{
	FILE *fp;
	char fileName[PATH_MAX];
	char m[MAX_LENGTH];

	*file = '\0';
	sprintf(fileName, "ResourceNames.%d.%d", GV_AppPid, GV_AppRef);

	fp = fopen(fileName, "w+");
	if(fp == NULL)
	{
		sprintf(m, "Failed to open file (%s) for writing. [%d, %s]",
			fileName, errno, strerror(errno));
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, m);
		return(-1);
	}

	fprintf(fp, "%s\n", zResourceNames);

	fclose(fp);

	sprintf(file, "%s", fileName);
	return(0);
}
