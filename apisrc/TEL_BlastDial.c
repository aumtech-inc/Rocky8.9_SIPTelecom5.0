/*------------------------------------------------------------------------------
File:		TEL_BlastDial.c
Purpose:	Make a request for the BlastDial function.
Author:		
Update: 04/26/2010 djb Created the file
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "TEL_Globals.h"
#include "Telecom.h"

static char	ModuleName[] = "TEL_BlastDial";

static char logBuf[256];

/* Static Function Prototypes */
static int createFileForSDM(struct ArcSipDestinationList dlist[], 
				char *sendSDMFile);
static int parametersAreGood(char *mod, int numRings,
			struct ArcSipDestinationList dlist[]);

static int tel_blastdial(char *mod, int *connected, int *appCallNum, 
				int *retCode, int *channel, void *m,
				int callNotifyCaller,
				struct ArcSipDestinationList dlist[]);

static int spConnectCalls(char *mod, void *m);
static int getBlastDialResults(struct MsgToApp *response,  struct ArcSipDestinationList dlist[]);

extern int parseInitiateCallMessageResponse(char *responseMessage);

/*-----------------------------------------------------------------------------
TEL_BlastDial():
-----------------------------------------------------------------------------*/
int TEL_BlastDial(int numRings, struct ArcSipDestinationList dlist[])
{
	int		rc;
	int		keepAliveCounter;
	char	sendSDMFile[128];
	char 	apiAndParms[MAX_LENGTH] = "";
	struct Msg_BlastDial	msg;
	static char mod[]="TEL_BlastDial";
//	int			connected= 0;
	int			retcode;
	int			channel= 0;
	char	buf[128];
	time_t	t1;
	time_t	t2;
	struct MsgToApp response;
	struct Msg_DropCall lMsgDropCall;

	sprintf(apiAndParms,"%s(%d)", ModuleName,
				(int)arc_val_tostr(numRings));;
	rc = apiInit(ModuleName, TEL_BLASTDIAL, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);

	if (!parametersAreGood(mod, numRings, dlist) )
	{
		HANDLE_RETURN (TEL_FAILURE);
	}
	
	memset((char *)sendSDMFile, '\0', sizeof(sendSDMFile));
	if ((rc = createFileForSDM(dlist, sendSDMFile)) < 0 )
	{
		HANDLE_RETURN (TEL_FAILURE);
	}
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"File created for dyn (%s)", sendSDMFile);

	msg.opcode = DMOP_BLASTDIAL;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;
	msg.numRings = numRings;
	msg.appCallNum 	= GV_AppCallNum1;
	sprintf(msg.dataFile, "%s", sendSDMFile);

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"dataFile=(%s)", msg.dataFile);

	rc = tel_blastdial("TEL_BlastDial", &GV_Connected2,
                &GV_AppCallNum2, &retcode, &channel,
                (void *)&msg, 1, dlist);

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"%d = tel_blastdial(%d, %d)", rc, retcode, channel);
	if(rc == TEL_SUCCESS)
	{
		retcode = 0;
		writeFieldToSharedMemory(mod, GV_AppCallNum2, 
				APPL_PID, GV_AppPid);
		writeFieldToSharedMemory(mod, GV_AppCallNum2, 
				APPL_STATUS, STATUS_BUSY);
		writeFieldToSharedMemory(mod, GV_AppCallNum2, 
				APPL_SIGNAL, 1);
		sprintf(buf, "ASSIGNED:%d", GV_AppCallNum1);
		writeStringToSharedMemory(mod, GV_AppCallNum2,  buf);
	}
	else
	{
		HANDLE_RETURN(rc);
	}
	retcode = 0;

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"TEL_BlastDial entering NotifyCaller C1=%d C2=%d.",
			GV_Connected1, GV_Connected2);

	msg.opcode  = DMOP_BRIDGECONNECT;
	msg.appRef  = GV_AppRef++;
	rc = spConnectCalls(mod, (void *)&msg);
	if (rc != TEL_SUCCESS)
	{
		HANDLE_RETURN(rc);
	}


	time(&t1);	
	while (GV_Connected1 && GV_Connected2)
	{
		usleep(400);
		if(GV_AgentDisct == YES)
		{
			lMsgDropCall.opcode = DMOP_DROPCALL;
			lMsgDropCall.appCallNum = GV_AppCallNum2;
			lMsgDropCall.appPid = GV_AppPid;
			lMsgDropCall.appRef = GV_AppRef++;
			lMsgDropCall.appPassword = GV_AppPassword;
			lMsgDropCall.allParties = 0;
			
		     rc = sendRequestToDynMgr(mod, (struct MsgToDM *)&lMsgDropCall);
                        if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
			GV_AgentDisct = NO;
		}

		keepAliveCounter++;
		if(keepAliveCounter >= 25)
		{
			writeFieldToSharedMemory(mod, GV_AppCallNum1, 
					APPL_SIGNAL, 1);
			writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_SIGNAL, 1);
			keepAliveCounter = 0;
		}

		time(&t2);	
		if((t2 - t1)*1000 >= GV_NotifyCallerInterval) 
		{
			t1 = t2;

			GV_InsideNotifyCaller = 1;

#ifndef CLIENT_GLOBAL_CROSSING
			if(NotifyCaller)
#endif
				NotifyCaller(GV_Connected1 && GV_Connected2);

			GV_InsideNotifyCaller = 0;
		}

		if (!GV_Connected1)
		{
			HANDLE_RETURN(TEL_DISCONNECTED);
		}
		if (!GV_Connected2) 
		{
			writeFieldToSharedMemory(mod, GV_AppCallNum2, APPL_SIGNAL, 1);
			GV_AppCallNum2 = NULL_APP_CALL_NUM;
			HANDLE_RETURN(-10);
		}

		rc = readResponseFromDynMgr(mod,-1,&response,sizeof(response));
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		if(rc == -2) continue;
		/* Note: The msg here is actually irrelevant, but serves
		the appropriate purpose in checking for dtmf of disc. */
		rc = getBlastDialResults(&response,  dlist);

		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	
		switch(rc)
		{
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum,
                                                        response.message);
			break;
		case DMOP_DISCONNECT:
			rc = disc(mod, response.appCallNum);
			if(rc == -10)
			{
				writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_PID, 0);
				writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_STATUS, STATUS_INIT);
				writeFieldToSharedMemory(mod, GV_AppCallNum2, 
					APPL_SIGNAL, 1);
				sprintf(buf, "%s", "N/A");
				writeStringToSharedMemory(mod, GV_AppCallNum2,
  							buf);
				GV_AppCallNum2 = NULL_APP_CALL_NUM;
			}
                        HANDLE_RETURN(rc);
			break;
		default:
			/* Unexpected message. Logged in examineDynMgr... */
			break;
		} /* switch rc */
	}

	if (!GV_Connected1) 
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"TEL_BlastDial exiting NotifyCaller C1=%d C2=%d.",
			GV_Connected1, GV_Connected2);

		HANDLE_RETURN(TEL_DISCONNECTED);
	}

	if (!GV_Connected2) 
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"TEL_BlastDial exiting NotifyCaller C1=%d C2=%d.",
			GV_Connected1, GV_Connected2);

		writeFieldToSharedMemory(mod, GV_AppCallNum2, 
				APPL_SIGNAL, 1);

		GV_AppCallNum2 = NULL_APP_CALL_NUM;
		HANDLE_RETURN(-10);
	}

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
		"TEL_BlastDial exiting NotifyCaller C1=%d C2=%d.",
		GV_Connected1, GV_Connected2);

	HANDLE_RETURN(TEL_SUCCESS);
}

/*-----------------------------------------------------------------------------
tel_blastdial():
-----------------------------------------------------------------------------*/
static int tel_blastdial(char *mod, int *connected, int *appCallNum, 
				int *retCode, int *channel, void *m,
				int callNotifyCaller,
				struct ArcSipDestinationList dlist[])
{	
	int rc;
	int lCallNum;
	int requestSent;
	struct Msg_BlastDial msg;
	struct MsgToApp response;
//	struct Msg_DropCall lMsgDropCall;
	time_t t1;

	msg=*(struct Msg_BlastDial *)m;

	*retCode = 21;
	requestSent=0;
	time(&t1);	
	memset(&GV_BridgeResponse, 0, sizeof(struct MsgToApp));
	while ( 1 )
	{
#if 0
			if(GV_AgentDisct == YES)
			{
				lMsgDropCall.opcode = DMOP_DROPCALL;
				lMsgDropCall.appCallNum = GV_AppCallNum1;
				lMsgDropCall.appPid = GV_AppPid;
				lMsgDropCall.appRef = GV_AppRef++;
				lMsgDropCall.appPassword = GV_AppPassword;
				lMsgDropCall.allParties = 2;
			
		     		rc = sendRequestToDynMgr(mod, &lMsgDropCall);
                        	if(rc == -1) return(TEL_FAILURE);
				GV_AgentDisct = NO;
				return(-10);
			}
#endif
	
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
							sizeof(response));
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
				"%d = readResponseFromDynMgr()", rc);
			if(rc == -1) return(TEL_FAILURE);
                        if(rc == -2)
			{
				rc = sendRequestToDynMgr(mod, (struct MsgToDM *) &msg);
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"%d = sendRequestToDynMgr()", rc);
				if (rc == -1) return(-1);
				requestSent=1;

#if 0
				if(callNotifyCaller == 1)
				{
					rc = readResponseFromDynMgr(mod,-1,
						&response,sizeof(response));
					if(rc == -1) return(TEL_FAILURE);
					if(rc == -2)
					{
						usleep(400);
						continue;
					}
				}
				else
				{
#endif
					rc = readResponseFromDynMgr(mod,0,
						(void *)&response,sizeof(response));
					telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
						"%d = readResponseFromDynMgr()", rc);
					if(rc == -1) return(TEL_FAILURE);
//				}
			}
		}
		else
		{
#if 0
			if(callNotifyCaller == 1)
			{
				rc = readResponseFromDynMgr(mod,-1,
					&response,sizeof(response));
				if(rc == -1) return(TEL_FAILURE);
				if(rc == -2)
				{
					usleep(400);
					continue;
				}
			}
			else
			{
#endif
				rc = readResponseFromDynMgr(mod,0,
					&response,sizeof(response));
				if(rc == -1) return(TEL_FAILURE);
//			}
		}
		rc = examineDynMgrResponse(mod, (struct MsgToDM *)&msg, &response);	
		switch(rc)
		{
		case DMOP_BLASTDIAL:
		case DMOP_INITIATECALL:
			if (response.returnCode >= 0)
			{
				lCallNum = parseInitiateCallMessageResponse(response.message);
				*connected=1;    
				//lCallNum=atoi(response.message); 
				*appCallNum=lCallNum;
				*channel=lCallNum;
				*retCode = response.returnCode;
        		telVarLog(mod,REPORT_VERBOSE,TEL_BASE, GV_Info,
        			"pid=%d assigned call # %d.",
                	response.appPid, response.appCallNum);

				return(0);	
			}
			else
			{
				rc = getBlastDialResults(&response,  dlist);
//				if(response.returnCode != -1)
//					*retCode = -1 * response.returnCode;
//				else
//					*retCode = response.returnCode;

				return(-1);	
			}
			break;
		case DMOP_GETDTMF:
			/* Save typeahead if it's for the other call leg. */
			rc = getBlastDialResults(&response,  dlist);
			if (msg.appCallNum != response.appCallNum)
				saveTypeAheadData(mod, response.appCallNum,
                                                        response.message);
			break;
		case DMOP_DISCONNECT:
				rc = getBlastDialResults(&response,  dlist);
				return(disc(mod, response.appCallNum));
			break;
		default:
			/* Unexpected message. Logged in examineDynMgr... */
			continue;
			break;
		} /* switch rc */
	} /* while */
}

/*-----------------------------------------------------------------------------
createFileForSDM():
-----------------------------------------------------------------------------*/
static int createFileForSDM(struct ArcSipDestinationList dlist[], 
				char *sendSDMFile)
{
	FILE	*fp;
	char	fileName[128];
	int	i;

	/* Create a file and send it */
	sprintf(fileName, ".blastDial.%d", GV_AppCallNum1);
	if((fp = fopen(fileName, "w+")) == NULL)
	{
		sprintf(logBuf,"Failed to open file (%s). [%d, %s].",
				fileName, errno, strerror(errno));

		telVarLog(ModuleName,REPORT_NORMAL,TEL_CANT_OPEN_FILE,
					GV_Err, logBuf);
		HANDLE_RETURN(-1);
	}

	for (i=0; i<5; i++)
	{
		if ( dlist[i].destination[0] == '\0' )
		{
			break;
		}
       	fprintf(fp, "%s,%d\n",
			dlist[i].destination, dlist[i].inputType);
	}
        fclose(fp);

	sprintf(sendSDMFile, "%s", fileName);

	return(0);
} // END: createFileForSDM()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int parametersAreGood(char *mod, int numRings,
			struct ArcSipDestinationList dlist[])
{
	int	i;
	int	numDests;
	int	errors = 0;
	int	numRingsMin = 1;
	int	numRingsMax = 100;
	char	editedDestination[128];
	
	if ((numRings < numRingsMin) || (numRings > numRingsMax))
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for numRings: %d. Valid values are %d to %d",
			numRings, numRingsMin, numRingsMax);
	}

	for (numDests=0; numDests<5; numDests++)
	{
		if ( dlist[numDests].destination[0] == '\0' )
		{
			break;
		}
	}

	if ( numDests < 1 )
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
	        	"%d:Invalid number of destinations received (%d).  Must be "
			"between 1 and 5.", __LINE__, numDests);
		errors++;
	}
	
	for (i=0; i<numDests; i++)
	{
		switch(dlist[i].inputType)
		{
			case NONE:
			case IP:
				memset((char *) editedDestination, '\0',
						sizeof(editedDestination));
				if (!goodDestination(mod, dlist[i].inputType,
					dlist[i].destination, editedDestination))
				{
                        		errors++; /* Message written in subroutine */
				}
				if ( strcmp(dlist[i].destination, editedDestination) )
				{
					sprintf(dlist[i].destination, "%s", editedDestination);
				}
				break;
			default:
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			            "For destination (%s), invalid value for "
				    "informat: %d. Valid values are %s.",
				    dlist[i].destination, dlist[i].inputType, "NONE, IP");
				errors++;
		}
	}
	return(errors==0);
} // END: parametersAreGood()

static int spConnectCalls(char *mod, void *m)
{
    int rc;
    struct Msg_BridgeConnect msg;
    struct MsgToApp response;

    msg=*(struct Msg_BridgeConnect *)m;

	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"sending msg.opcode(%d) to fifo", msg.opcode);
        rc = sendRequestToDynMgr(mod, &msg);
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
			"%d=sendRequestToDynMgr()", rc);
        if (rc == -1) return(-1);

    while ( 1 )
    {
        rc = readResponseFromDynMgr(mod,0,&response,sizeof(response));
        if(rc == -1) return(TEL_FAILURE);

        rc = examineDynMgrResponse(mod, &msg, &response);
        switch(rc)
        {
        case DMOP_BRIDGECONNECT:
            if (response.returnCode == 0)
            {
                    telVarLog(mod,REPORT_VERBOSE,TEL_BASE,GV_Info,
                        "pid=%d connected calls %d & %d.",
                            response.appPid,
                    GV_AppCallNum1, GV_AppCallNum2);
            }
            return(response.returnCode);
            break;
        case DMOP_GETDTMF:
            saveTypeAheadData(mod, response.appCallNum,
                                                        response.message);
            break;
        case DMOP_DISCONNECT:
                        return(disc(mod, response.appCallNum));
            break;
        default:
            /* Unexpected message. Logged in examineDynMgr... */
            continue;
            break;
        } /* switch rc */
    } /* while */
}

static int getBlastDialResults(struct MsgToApp *response,  struct ArcSipDestinationList dlist[])
{

    static char mod[] = "getBlastDialResults";
    FILE        *fp;
    char        tmpBuf[128];
    char        *startStr;
	char		*ptr;
    char        line[256];
    int         i;
	char		destination[50];
	int			outboundChannel;
	int			resultCode;
	char		resultData[128];

    if (access (response->message, F_OK) < 0)
    {
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
            "Unable to access speedDial reultFile (%s).  "
            "Cannot obtain speedDial results.  [%d, %s]",
            response->message, errno, strerror(errno));
        return(-1);
    }

    if((fp = fopen(response->message, "r")) == NULL)
    {
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
            "Unable to open speedDial reultFile (%s).  "
            "Cannot obtain speedDial results.  [%d, %s]",
            response->message, errno, strerror(errno));
		unlink(response->message);
        return(-1);
    }
	telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
	    "Successfully opened (%s) for reading", response->message);

    memset (line, 0, sizeof (line));
    while (fgets (line, sizeof (line) - 1, fp))
    {
        if(line[(int)strlen(line)-1] == '\n')
        {
            line[(int)strlen(line)-1] = '\0';
        }

        memset (destination, 0, sizeof (destination));
        memset (resultData, 0, sizeof (resultData));


		sprintf(tmpBuf, "%s", line);

        memset (destination, 0, sizeof (destination));
        memset (resultData, 0, sizeof (resultData));

		startStr = tmpBuf;
		if ( (ptr = (char *)index(tmpBuf, (int)','))  == NULL )
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
               "Invalid data returned (%s) in speeddial result "
                " file (%s).  Ignoring", line, response->message);
            continue;
        }
		*ptr='\0';
		sprintf(destination, "%s", startStr);

		sprintf(tmpBuf, "%s", ptr+1);
		startStr = tmpBuf;
		if ( (ptr=index(tmpBuf, ','))  == NULL )
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
               "Invalid data returned (%s) in speeddial result "
                " file (%s).  Ignoring\n", line, response->message);
            continue;
        }
		*ptr='\0';
		outboundChannel = atoi(startStr);

		sprintf(tmpBuf, "%s", ptr+1);
		startStr = tmpBuf;
		if ( (ptr=index(tmpBuf, ','))  == NULL )
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
               "Invalid data returned (%s) in speeddial result "
                " file (%s).  Ignoring\n", line, response->message);
            continue;
        }
		*ptr='\0';
		resultCode = atoi(startStr);

		if(resultCode < 0 )
			resultCode = -1 * resultCode;

		ptr++;
		sprintf(resultData, "%s", ptr);

		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
            "Read destination(%s, %d, %s)\n",
            destination, resultCode, resultData);

    	for (i=0; i<5; i++)
		{
			if ( strcmp(destination, dlist[i].destination) == 0 )
			{
				dlist[i].outboundChannel = outboundChannel;
				dlist[i].resultCode = resultCode;
				sprintf(dlist[i].resultData, "%s", resultData);
				telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Set dlist[%d]=[%s, %d, %d, %s]", i,
					dlist[i].destination, dlist[i].outboundChannel, dlist[i].resultCode,
					dlist[i].resultData);
				break;
			}
		}




#if 0

	    rc = sscanf (line, "%[^,],%d,%d,%s",
			destination, &outboundPort, &resultCode, resultData);
		if ( (rc < 2 ) || (rc > 2) )
		{
			telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Err,
	           "Invalid number of fields (%d) received from line (%s) of speeddial result "
				" file (%s).  Ignoring", rc, line, response->message);
	    	memset (line, 0, sizeof (line));
			continue;
		}
	    memset (line, 0, sizeof (line));

		telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
            "Read destination(%s, %d, %d, %s)",
			destination, outboundPort, resultCode, resultData);
#endif
    }

    fclose(fp);
    unlink(response->message);
    return(0);

} // END: getBlastDialResults

