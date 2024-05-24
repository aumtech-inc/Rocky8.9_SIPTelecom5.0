/*-----------------------------------------------------------------------------
Program:        TEL_SRSetParameter.c 
Purpose:        To set the given global parameter.
Author:         Anand Joglekar
Date:			10/01/2002
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

#define	LIST_TERMINATOR	 "NULL"

/* valid set of global variables to be used in api */
static char *vars[] = {
			"$CONFIDENCE_LEVEL", 			
			"$INCOMPLETE_TIMEOUT",
			"$INTERDIGIT_TIMEOUT",			
			"$MAX_N_BEST",
			"MRCP:", 			
			"$RECOGNITION_MODE",
			"$SENSITIVITY_LEVEL",	
			"$SPEED_ACCURACY",
			"$SR_GRAMMAR_NAME", 		
			"$SR_LANGUAGE",
			"$TERMDIGIT_TIMEOUT", 		
			"$HIDE_DTMF", 		
			LIST_TERMINATOR /* Must be last item in the list. */ 	
			};

static int  sValidateFloat(char *zFloat, char *zMsg);

int TEL_SRSetParameter(char *zParameterName, char *zParameterValue)
{
	static char	mod[]="TEL_SRSetParameter";
	char		apiAndParms[MAX_LENGTH] = "";
	int			rc;
	int			index;
	char		errMsg[256];
	char		yStrFileName[128];
	char		yParameterValue[1024];
	FILE		*fp;

	struct MsgToApp response;
	struct Msg_SRSetParameter lMsg;
	struct MsgToDM msg; 
       
	if ( GV_SRType == GOOGLE_SR )
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Google SR is enabled. Returning with no action.");
		return(0);
	}

	sprintf(apiAndParms,"%s(%s, %s)", mod, zParameterName, zParameterValue);
	rc = apiInit(mod, TEL_SRSETPARAMETERS, apiAndParms, 1, 0, 0); 
	if (rc != 0) HANDLE_RETURN(rc);
 
	index=0;
	while((strcmp(vars[index], LIST_TERMINATOR)) != 0)
	{
	/* If we find the item we are looking for, exit the loop. */
		if ( index == 4 )		// MRCP:
		{
   	 		if (strncmp(zParameterName, vars[index], 5) == 0)
			{
				break;	
			}
			index++;
		}
		else
		{
   	 		if (strcmp(zParameterName, vars[index]) == 0)
			{
				break;	
			}
			index++;
		}
	}

	sprintf(yParameterValue, "%s", zParameterValue);
	switch(index)
	{
    	case 0: 	/* $CONFIDENCE_LEVEL */
			if ((rc=sValidateFloat(zParameterValue, errMsg)) == -1)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid confidence level received (%s).  %s",
					zParameterValue, errMsg);
				HANDLE_RETURN(rc);
			}
			break;

    	case 1: 	/* $INCOMPLETE_TIMEOUT */
			if(!zParameterValue || !*zParameterValue)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Empty incomplete timeout received (%s).  Must be a value >= 0.", zParameterValue);
				HANDLE_RETURN(-1);
			}
			rc=atoi(zParameterValue);
			if (rc < 0)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid incomplete timeout received (%s).  Must be >= 0",
					zParameterValue);
				HANDLE_RETURN(-1);
			}
			break;

    	case 2: 	/*  $INTERDIGIT_TIMEOUT */
			if(!zParameterValue || !*zParameterValue)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Empty interdigit timeout received (%s).  Must be a value >= 0.", zParameterValue);
				HANDLE_RETURN(-1);
			}
			rc=atoi(zParameterValue);
			if (rc < 0)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid interdigit timeout received (%s).  Must be >= 0",
					zParameterValue);
				HANDLE_RETURN(-1);
			}
			break;

    	case 3: 	/* $MAX_NBEST */
			if(!zParameterValue || !*zParameterValue)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Empty incomplete timeout received (%s).  Must be a value >= 0.", zParameterValue);
				HANDLE_RETURN(-1);
			}
			rc=atoi(zParameterValue);
			if (rc < 2)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid max nbest received (%s).  Must be >= 2",
					zParameterValue);
				HANDLE_RETURN(-1);
			}
			break;

    	case 4:		/* MRCP: */
			if(!zParameterValue || !*zParameterValue)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid data. Empty MRCP: value received. Must be a valid "
					"MRCPv2 recognition name-value pair.");
				HANDLE_RETURN(-1);
			}

			if(strlen(zParameterValue) >= 200)
			{
				/* Write this data to a file, so that dynmgr can read it. */
				sprintf(yStrFileName, "/tmp/%s.%d", mod, getpid());
				if((fp = fopen(yStrFileName, "w+")) == NULL)
				{
					telVarLog(mod,REPORT_NORMAL,TEL_BASE,GV_Err,
						"Failed to open file (%s) for output. [%d, %s]",
						yStrFileName, errno, strerror(errno));
					HANDLE_RETURN(-1);
				}
		
				fprintf(fp, "%s", zParameterValue);
		
				fclose(fp);
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Wrote parameter value MRCP: to file (%s) for IPC.", yStrFileName);
				sprintf(yParameterValue, "FILE:%s", yStrFileName);
			}
			break;
    	case 5: 	/* $RECOGNITION_MODE */
			if(!zParameterValue || !*zParameterValue)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid data. Empty recognition mode received. Must be set to either "
					"\"normal\" or \"hotword\".");
				HANDLE_RETURN(-1);
			}
			if ( (strcmp(zParameterValue, "normal") != 0) &&
			     (strcmp(zParameterValue, "hotword") != 0) )
			{
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid recognition mode received (%s) Must be set to either "
					"\"normal\" or \"hotword\".", zParameterValue);
				HANDLE_RETURN(-1);
			}
			break;

    	case 6: 	/* $SENSITIVITY_LEVEL */
			if ((rc=sValidateFloat(zParameterValue, errMsg)) == -1)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid confidence level received (%s).  %s",
					zParameterValue, errMsg);
				HANDLE_RETURN(-1);
			}
			break;

    	case 7:		/* $SPEED_ACCURACY */
			if ((rc=sValidateFloat(zParameterValue, errMsg)) == -1)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid confidence level received (%s).  %s",
					zParameterValue, errMsg);
				HANDLE_RETURN(-1);
			}
			break;

    	case 8:		/* $SR_GRAMMAR_NAME */
			if(!zParameterValue || !*zParameterValue)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Empty $SR_GRAMMAR_NAME passed. It must be a non-empty string.");
				HANDLE_RETURN(-1);
			}
			break;
    	case 9:	/* $SR_LANGUAGE */
			if(!zParameterValue || !*zParameterValue)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Empty SR language passed. It must be a non-empty RFC 5646-compliant string.");
				HANDLE_RETURN(-1);
			}
			break;

    	case 10: 	/* $TERMDIGIT_TIMEOUT */
			if(!zParameterValue || !*zParameterValue)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Empty termination dtmf timeout received (%s).  Must be a value >= 0.", zParameterValue);
				HANDLE_RETURN(-1);
			}
			rc=atoi(zParameterValue);
			if (rc < 0)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
					"Invalid dtmf termination timeout received (%s).  Must be >= 0",
					zParameterValue);
				HANDLE_RETURN(-1);
			}
			if ( rc == 0 )
			{
				char		*p;
				p = zParameterValue;
				while (*p != '\0' || p )
				{
					if ( isdigit(*p) == 0 )
					{
						telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
							"Invalid termination dtmf timeout received (%s).  Must be a integer value >= 0.",
							zParameterValue);
						HANDLE_RETURN(-1);
					}
					p++;
				}
			}
			break;
        case 11:    /* $HIDE_DTMF */
            if(!zParameterValue || !*zParameterValue)
            {
                telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
                            "Invalid data received (%s) for $HIDE_DTMF.  Must be either 0 or 1",
                            zParameterValue);
                HANDLE_RETURN(-1);
            }
            if ( (strcmp(zParameterValue, "0") != 0) &&
                 (strcmp(zParameterValue, "1") != 0) )
            {
                telVarLog(mod, REPORT_NORMAL, TEL_INVALID_DATA, GV_Err,
                    "Invalid $HIDE_DTMF (%s) Must be set to either 0 or 1.",
                    zParameterValue);
                HANDLE_RETURN(-1);
            }
            break;
    	default:
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
				"Invalid value for variable name: (%s).", 
				zParameterName);
			HANDLE_RETURN(-1);
			break;
	} /* switch */

	lMsg.opcode = DMOP_SRSETPARAMETER;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appPid = GV_AppPid;
	lMsg.appRef = GV_AppRef++;
	lMsg.appPassword = GV_AppPassword;
	sprintf(lMsg.nameValue, "%s=%s", zParameterName, yParameterValue);
	
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SRSetParameter));

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"Sending TEL_SRSetParameter(%s, %s) to mrcp client",
		zParameterName, zParameterValue);
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
		case DMOP_SRSETPARAMETER:
			if ( response.returnCode == 0 )
			{
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Successfully set TEL_SRSetParameter(%s, %s).",
					zParameterName, zParameterValue);
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

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int  sValidateFloat(char *zFloat, char *zMsg)
{
	char	*p;
	char	pWhole[32];
	char	pDecimal[32];
	int		tmpInt;

	if ( !*zFloat || !zFloat )
	{
		sprintf(zMsg, "%s", "Empty string received; it must contain a valid decimal value "
				"between 0.0 and 1.0");
		return(-1);
	}

	zMsg[0] = '\0';
	sprintf(pWhole, "%s", zFloat);

	if ((p = (char *)strchr(pWhole, '.')) == (char *) NULL)
	{
		sprintf(zMsg, "[%s, %d] Not a valid decimal value; valid range is 0.0 to 1.0",
					__FILE__, __LINE__);
		return(-1);
	}

	*p = '\0';

	p++;
	sprintf(pDecimal, "%s", p);

	if ( strcmp(pWhole, "0") == 0 )
	{
		;
	}
	else if ( strcmp(pWhole, "1") == 0 )
	{
		if ( strcmp(pDecimal, "0") != 0 )
		{
			sprintf(zMsg, "[%s, %d] Not a valid decimal value; valid range is 0.0 to 1.0",
					__FILE__, __LINE__);
			return(-1);
		}
	}
	else
	{
		sprintf(zMsg, "[%s, %d] Not a valid decimal value; valid range is 0.0 to 1.0",
					__FILE__, __LINE__);
		return(-1);
	}

	return(0);
} // END: sValidateFloat()
