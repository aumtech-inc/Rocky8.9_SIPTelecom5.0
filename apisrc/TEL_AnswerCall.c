/*-----------------------------------------------------------------------------
Program:        TEL_AnswerCall.c 
Purpose:        Direct the dynamic manager to answer the call.
Author:         Wenzel-Joglekar
Date:		11/10/2000
	NOTE:	GV_ConnectTime should be passed from the DM to be more accurate.
Update:		03/01/2001 gpw added code to send "busy" to monitor
Update:		04/25/2001 apj Number of rings, make valid range 0 or positive.
Update: 	06/22/01 APJ Set 60 sec timeout for response from DM.
Update: 	07/01/02 APJ Extract bandwidth info.
Update:     11/10/2004 apj Save DTMFs in the buffer.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

/*-------------------------------------------------------------------------
This function verifies that all parameters are valid.
It returns 1 (yes) if all parmeters are good, 0 (no) if not.
-------------------------------------------------------------------------*/
static int parametersAreGood(char *mod, int numRings)
{
	char m[MAX_LENGTH];
	int  errors=0;
  
	if (numRings < 0)
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for numRings: %d. Valid values are 0 or greater.", numRings);
	}
	return( errors == 0);   
}


static int parseAnswerCallMessageResponse(char *responseMessage)
{
	static char mod[] = "parseAnswerCallMessageResponse";
	char stringToFind[] = {"BANDWIDTH="};
	char    *zQuery = NULL;
	char    lhs[32] = "";
	char    rhs[32] = "";
	char    buf[MAX_APP_MESSAGE_DATA] = "";	  
	int 	  index;
    char tmpVal[100] = "";
	
	zQuery = (char*)strtok_r((char *)responseMessage, ",", (char**)&buf);

	
	if(zQuery == NULL )
	{
		return 0;
	}
	strcat(zQuery, "^");

	for(index = 0; index < strlen(zQuery); index++)
	{
		sprintf(tmpVal, "%s", "0");
		if((strstr(zQuery + index, "AUDIO_CODEC") == zQuery + index )) 
		{


			getCodecValue(
							zQuery + index,
							tmpVal,
							"AUDIO_CODEC",
							'=',
							'^');
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info, "AUDIO_CODEC=%s.", tmpVal);

			if(strcmp(tmpVal, "711") == 0)
			{
				sprintf(GV_BeepFile, "beep.711");	
				sprintf(GV_Audio_Codec, "%d", 3);	
			}
			else if(strcmp(tmpVal, "711r") == 0)
			{
				sprintf(GV_BeepFile, "beep.711");	
				sprintf(GV_Audio_Codec, "%d", 1);
			}
			else if(strcmp(tmpVal, "729a") == 0)
			{
				sprintf(GV_BeepFile, "beep.g729a");	
				sprintf(GV_Audio_Codec, "%d", 4);
			}
			else if(strcmp(tmpVal, "AMR") == 0)
			{
				sprintf(GV_BeepFile, "beep.amr");	
				sprintf(GV_Audio_Codec, "%d", 5);
			}
			else if(strcmp(tmpVal, "729b") == 0)
			{
				sprintf(GV_BeepFile, "beep.g729b");	
				sprintf(GV_Audio_Codec, "%d", 7);
			}
			else
			{
				sprintf(GV_BeepFile, "beep.wav");	
				sprintf(GV_Audio_Codec, "%d", 0);
			}
				
			
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"GV_Audio_Codec=%s.", GV_Audio_Codec);
			index = index + strlen("AUDIO_CODEC") - 1;
		}
		else
		if(strstr(zQuery + index, "VIDEO_CODEC") == zQuery + index)
		{
			getCodecValue(
								zQuery + index,
								tmpVal,
								"VIDEO_CODEC",
								'=',
								'^');

			if(strstr(tmpVal, "263"))
				sprintf(GV_Video_Codec, "%d", 2);	

		}

	}

	
	return(0);
}


int TEL_AnswerCall(int numRings) 
{
	static char mod[]="TEL_AnswerCall";
	char apiAndParms[MAX_LENGTH]  = "";
	int rc = 0;

	struct MsgToApp response;
	struct Msg_AnswerCall lMsg;
	struct MsgToDM msg; 
       
	sprintf(apiAndParms,"%s(%d)", mod, numRings);
	rc = apiInit(mod, TEL_ANSWERCALL, apiAndParms, 1, 0, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	if (!parametersAreGood(mod, numRings)) 
	{
		//HANDLE_RETURN(-1);
		HANDLE_RETURN(0);
	}

	lMsg.opcode = DMOP_ANSWERCALL;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;
	lMsg.numRings = numRings;


    /*
    ** If GV_Overlay == 1, attach the shared memory and exit
    */
    if(GV_Overlay == 1)
    {
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Inside GV_Overlay, returning GV_Connected1=1");
        GV_Connected1 = 1;
        HANDLE_RETURN (0);
    }

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Overlay is not set TEL_AnswerCall");

	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_AnswerCall));
	rc = sendRequestToDynMgr(mod, &msg); 
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
		case DMOP_ANSWERCALL:
			if (response.returnCode == 0)
			{
				parseAnswerCallMessageResponse(response.message);
				setTime(GV_ConnectTime, &GV_ConnectTimeSec);
				writeStartCDRRecord(mod);
  				sprintf (GV_CDR_Key, "%s", GV_CDRKey);
				if (GV_DiAgOn_ == 0)/* app not under monitor */
					send_to_monitor(MON_STATUS, 
							STATUS_BUSY, "");
				GV_Connected1 = 1;
			}
			if(!strcmp(GV_Audio_Codec, "3"))
			{
				TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT,
            		PHRASE_FILE, PHRASE, "silence_1.64p", SYNC);
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




#if 0
static int parseAnswerCallMessageResponse(char *responseMessage)
{
	char stringToFind[] = {"BANDWIDTH="};

	if(strstr(responseMessage, stringToFind) != NULL)
	{
		sscanf(responseMessage+10, "%d", &GV_BandWidth);
	}
	
	return(0);
}
#endif
