/*-----------------------------------------------------------------------------
Program:        TEL_DialExtension.c
Purpose:       	Dial given extension after the call is connected. 
Author:         Anand Joglekar
Date:		10/16/2000
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, int party, int numOfRings, 
	char *extensionNumber, int timeToWaitForDetection);

int TEL_DialExtension(int party, int numOfRings, char *extensionNumber,
                        int timeToWaitForDetection, int *retCode) 
{
	static char mod[]="TEL_DialExtension";

	char apiAndParms[MAX_LENGTH] = "";
        char apiAndParmsFormat[]="%s(%s,%d,%s,%d,%s)";
        int  rc;
	int  requestSent;
	
	struct MsgToApp response;
	struct Msg_DialExtension msg;

	sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(party), numOfRings,
		extensionNumber, timeToWaitForDetection, "retcode");
        rc = apiInit(mod, TEL_DIALEXTENSION, apiAndParms, 1, 1, 0);
        if (rc != 0) HANDLE_RETURN(rc);

       	if (!parametersAreGood(mod, party, numOfRings, extensionNumber, 
		timeToWaitForDetection))
                HANDLE_RETURN (TEL_FAILURE);

        msg.opcode = DMOP_DIALEXTENSION;
	if (party == SECOND_PARTY)
		msg.appCallNum=GV_AppCallNum2;
	else
		msg.appCallNum=GV_AppCallNum1;

	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;

        sprintf(msg.extensionNumber, "%s", extensionNumber);

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
		case DMOP_DIALEXTENSION:
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

static int parametersAreGood(char *mod, int party, int numOfRings, 
	char *extensionNumber, int timeToWaitForDetection)
{
	int length;
	int errors=0;
	char invalidChars[50];
	int isInvalidCharPresent = 0;
	int tempInt = 0;
	int i;
 	char partyValues[]="FIRST_PARTY, SECOND_PARTY";     

	switch(party)
        {
       	case FIRST_PARTY:
		break;
	case SECOND_PARTY:
		if (!GV_Connected2)
		{
			errors++;
       			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"party=SECOND_PARTY, but second party is not connected.");
		}
		break;
	default:
		errors++;
       		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
       			"Invalid value for party: %d. Valid values are %s.",
			party, partyValues);
 		break;
	}

	if(extensionNumber == NULL)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, "%s",
		"Invalid value for extensionNumber: NULL.");
	}

	length = strlen(extensionNumber);
	if(length < 1)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"Invalid length of extensionNumber: %d. Must be greater than 0.",
		length);
	}

	memset(invalidChars, 0, sizeof(invalidChars));
	for(i=0; i<length; i++)
	{
		switch(extensionNumber[i])
		{
			case '0': case '1': case '2': case '3': case '4': 
			case '5': case '6': case '7': case '8': case '9': 
			case 'A': case 'B': case 'C': case 'D': case ',': 
			case '*': case '#': case 'a': case 'b': case 'c': 
			case 'd':
				break;

			default:
				tempInt = strlen(invalidChars);
				invalidChars[tempInt] = extensionNumber[i]; 	
				isInvalidCharPresent = 1;
				break;
		}
	}

	if(isInvalidCharPresent == 1)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
			"Invalid characters (%s) in extensionNumber (%s).",
			invalidChars, extensionNumber);
	}
	
	return( errors == 0);
}
