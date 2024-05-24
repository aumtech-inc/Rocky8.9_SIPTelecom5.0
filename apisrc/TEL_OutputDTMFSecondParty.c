/*-----------------------------------------------------------------------------
Program:        TEL_OutputDTMFSecondParty.c
Purpose:        Send a string of DTMF characters.
		Note:	This API does not have a party associated with it, so
			first party is assumed.
Author:         Wenzel-Joglekar
Date:		03/19/2023
djb			Just copied TEL_OutputDTMF and make a couple of tweeks.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, const char *dtmf_string);

int TEL_OutputDTMFSecondParty(const char * dtmf_string) 
{
	static char mod[]="TEL_OutputDTMFSecondParty";

	char apiAndParms[MAX_LENGTH] = "";
        char apiAndParmsFormat[]="%s(%s)";
        int  rc;
	int  requestSent;
	
	struct MsgToApp response;
	struct Msg_OutputDTMF msg;

	sprintf(apiAndParms,apiAndParmsFormat, mod, dtmf_string);
        rc = apiInit(mod, TEL_OUTPUTDTMF, apiAndParms, 1, 1, 0);
        if (rc != 0) HANDLE_RETURN(rc);

       	if (!parametersAreGood(mod, dtmf_string))
                HANDLE_RETURN (TEL_FAILURE);

        msg.opcode = DMOP_OUTPUTDTMF;
	msg.appCallNum = GV_AppCallNum2;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;

        sprintf(msg.dtmf_string, "%s", dtmf_string);

	requestSent=0;
	while ( 1 )
	{
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
						sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
                        if(rc == -2)
                        {
				rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&msg);
				if (rc == -1) HANDLE_RETURN(-1);
				requestSent=1;
				rc = readResponseFromDynMgr(mod,0,
                                                &response,sizeof(response));
                                if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			}          
		}
		else
		{
			rc = readResponseFromDynMgr(mod,0,
					&response,sizeof(response));
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		}
	
		rc = examineDynMgrResponse (mod, (struct MsgToDM *)&msg, &response);
		switch(rc)
		{
		case DMOP_OUTPUTDTMF:
			HANDLE_RETURN(response.returnCode);
			break;
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum,
							response.message);
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

static int parametersAreGood(char *mod,  const char *dtmf_string)
{
	int length;
	int errors=0;
	char invalidChars[50];
	int isInvalidCharPresent = 0;
	int tempInt = 0;
	int i;

	if(dtmf_string == NULL)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
				"Invalid value for dtmf_string: NULL.");
	}

	length = strlen(dtmf_string);
	if(length < 1)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid length of dtmf_string: %d. Must be greater than 0.",
			length);
	}

	memset(invalidChars, 0, sizeof(invalidChars));
	for(i=0; i<length; i++)
	{
		switch(dtmf_string[i])
		{
			case '0': case '1': case '2': case '3': case '4': 
			case '5': case '6': case '7': case '8': case '9': 
			case 'A': case 'B': case 'C': case 'D': case ',': 
			case '*': case '#': case 'a': case 'b': case 'c': 
			case 'd':
				break;

			default:
				tempInt = strlen(invalidChars);
				invalidChars[tempInt] = dtmf_string[i]; 	
				isInvalidCharPresent = 1;
				break;
		}
	}

	if(isInvalidCharPresent == 1)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid characters <%s> in dtmf_string <%s>.",
			invalidChars, dtmf_string);
	}
	
	return( errors == 0);
}
