/*-----------------------------------------------------------------------------
Program	:	TEL_RecordMedia.c
Purpose	:	To record audio and/or video files.
Author	:	Anand Joglekar, Rahul Gupta
Date	:	12/8/2004
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int parametersAreGood(char *mod, int party, int *interruptOption, 
								char *audiofilename, char *videofilename, 
								int *sync, int *recordTime, 
								int *audiocompression,int *videocompression, 
								int *audiooverwrite, int *videooverwrite,
								int *lead_silence, int *trail_silence, 
								int beep,char terminate_char);


int TEL_RecordMedia(RecordMedia *zRecordMedia)
{
	static char mod[]="TEL_RecordMedia";
	int  			rc;

	char 			apiAndParms[MAX_LENGTH] = "";
	char 			apiAndParmsFormat[]="%s(%s,%s,%s,%s,%s,%d,%s,%s,%s,%s,%d,%d,%s,%c)";
	int  			requestSent;
	char            tempFileName[MAX_APP_MESSAGE_DATA];
	struct MsgToApp response;
	struct MsgToDM  msg, msg1;
	char 			lBeepFileName[256];
	char 			*typeAheadBuffer;
	int             yFdDest;
	char            buf[2048];
	int             bytesWritten;




	GV_RecordTermChar = (int)' ';

	sprintf(apiAndParms,apiAndParmsFormat, mod, arc_val_tostr(zRecordMedia->party),
			arc_val_tostr(zRecordMedia->interruptOption), zRecordMedia->audioFileName,
			zRecordMedia->videoFileName,arc_val_tostr(zRecordMedia->sync),
			zRecordMedia->recordTime, arc_val_tostr(zRecordMedia->audioCompression), 
			arc_val_tostr(zRecordMedia->videoCompression), arc_val_tostr(zRecordMedia->audioOverwrite), 
			arc_val_tostr(zRecordMedia->videoOverwrite), zRecordMedia->lead_silence, 
			zRecordMedia->trail_silence, arc_val_tostr(zRecordMedia->beep),
			zRecordMedia->terminateChar);

	/* Do we need to account for party here ?? */
	rc = apiInit(mod, TEL_RECORDMEDIA, apiAndParms, 1, 1, 0);
	if (rc != 0) HANDLE_RETURN(rc);

    telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
                "In TEL_RecordMedia: party = %d ; interruptOption = %d ;",
				"audioFileName = %s ;videoFileName = %s ;sync = %d ;recordTime = %d ;",
				"audioCompression = %d ;videoCompression = %d ;audioOverwrite = %d ;",
				"videoOverwrite = %d ;lead_silence = %d ;trail_silence = %d ;beep = %d ;",
				"terminateChar = %c ",
				zRecordMedia->party,zRecordMedia->interruptOption, zRecordMedia->audioFileName, 
				zRecordMedia->videoFileName, zRecordMedia->sync, zRecordMedia->recordTime, 
				zRecordMedia->audioCompression, zRecordMedia->videoCompression, 
				zRecordMedia->audioOverwrite, zRecordMedia->videoOverwrite, 
				zRecordMedia->lead_silence, zRecordMedia->trail_silence, 
				zRecordMedia->beep, zRecordMedia->terminateChar);

	if (!parametersAreGood((char *)mod, zRecordMedia->party, &zRecordMedia->interruptOption,
					zRecordMedia->audioFileName, zRecordMedia->videoFileName, 
					&zRecordMedia->sync, &zRecordMedia->recordTime,
					&zRecordMedia->audioCompression, &zRecordMedia->videoCompression,
					&zRecordMedia->audioOverwrite, &zRecordMedia->videoOverwrite, 
					&zRecordMedia->lead_silence, &zRecordMedia->trail_silence, 
					zRecordMedia->beep, zRecordMedia->terminateChar))
	{
		HANDLE_RETURN(TEL_FAILURE);
	}

	memset(&msg, 0, sizeof(struct MsgToDM));
	msg.opcode = DMOP_RECORDMEDIA;
	if (zRecordMedia->party == FIRST_PARTY)
	{
		msg.appCallNum = GV_AppCallNum1;
	}
	else
	{
		msg.appCallNum = GV_AppCallNum2;
	}

	if(zRecordMedia->interruptOption != NONINTERRUPT)		// BT-226
		zRecordMedia->interruptOption = 1;
	else
		zRecordMedia->interruptOption = 0;

	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;


	if(zRecordMedia->beep == YES)
	{
		sprintf(lBeepFileName, "%s/%s", GV_SystemPhraseDir,
				BEEP_FILE_NAME);

		if(strlen(GV_BeepFile) != 0)
		{
			rc = access(GV_BeepFile, F_OK);
			if(rc == 0)
				sprintf(lBeepFileName, "%s", GV_BeepFile);
			else
			{
				telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
						"Failed to access beep file (%s).  [%d, %s] "
						"Using default beep file.",
						GV_BeepFile, errno, strerror(errno));
			}
		}

		rc = access(lBeepFileName, F_OK);
		if(rc == 0)
		{
			//sprintf(msg.beepFile, "%s", lBeepFileName);
		}
		else
		{
			telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
						"Beep phrase (%s) doesn't exist. errno=%d. Setting "
						" beep to NO.", lBeepFileName, errno);
			zRecordMedia->beep = NO;
//			memset(msg.beepFile, 0, sizeof(msg.beepFile));
		}
	}

	memset(tempFileName, 0, sizeof(tempFileName));

	sprintf(tempFileName, "/tmp/recordmedia_%d_%d", msg.appPid, msg.appRef);

	if ((yFdDest = open(tempFileName, O_CREAT|O_WRONLY, 0777)) == -1)
	{
		return(-1);
	}



	memset((char *)buf, 0, sizeof(buf));
	if(zRecordMedia->terminateChar == 32)
	{
		zRecordMedia->terminateChar = '!';
	}

	sprintf(buf, "%s%d%s"
			"%s%d%s"
			"%s%s%s"
			"%s%s%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%d%s"
			"%s%c%s"
			"%s%s",
			"party=",zRecordMedia->party, "\n",
			"interruptOption=",zRecordMedia->interruptOption, "\n",
			"audioFileName=",zRecordMedia->audioFileName, "\n",
			"videoFileName=",zRecordMedia->videoFileName, "\n",
			"sync=",zRecordMedia->sync, "\n",
			"recordTime=",zRecordMedia->recordTime, "\n",
			"audioCompression=",zRecordMedia->audioCompression, "\n",
			"videoCompression=",zRecordMedia->videoCompression, "\n",
			"audioOverwrite=",zRecordMedia->audioOverwrite, "\n",
			"videoOverwrite=",zRecordMedia->videoOverwrite, "\n",
			"lead_silence=",zRecordMedia->lead_silence, "\n",
			"trail_silence=",zRecordMedia->trail_silence, "\n",
			"beep=",zRecordMedia->beep, "\n",
			"terminateChar=",zRecordMedia->terminateChar,"\n",
			"beepFile=",lBeepFileName);

//	printf(" The buf value = %s", buf);fflush(stdout);
	if ((bytesWritten = write(yFdDest, buf, strlen(buf))) != strlen(buf))
	{
		close(yFdDest);
		return(-1);
	}
	close(yFdDest);
	sprintf(msg.data, "%s", tempFileName);


	if(zRecordMedia->interruptOption != NONINTERRUPT)
	{
		msg1.opcode=DMOP_GETDTMF;
		msg1.appPid     = GV_AppPid;
		msg1.appPassword = 0;
		if (zRecordMedia->party == FIRST_PARTY)
		{
			typeAheadBuffer = GV_TypeAheadBuffer1;
			msg1.appCallNum=GV_AppCallNum1;
		}
		else
		{
			typeAheadBuffer = GV_TypeAheadBuffer2;
			msg1.appCallNum=GV_AppCallNum2;
		}


		while(1)
		{
			rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
			if(rc == -1) 
			{
				if(access(tempFileName, F_OK) == 0)
				{
					unlink(tempFileName);
				}
				HANDLE_RETURN(TEL_FAILURE);
			}
			if(rc == -2) 
			{
				break;
			}
			rc = examineDynMgrResponse(mod, &msg1, &response);
			switch(rc)
			{
				case DMOP_GETDTMF:
					saveTypeAheadData(mod, msg1.appCallNum, "CLEAR");
					break;
				case DMOP_DISCONNECT:

					unlink(tempFileName);
					//HANDLE_RETURN(disc(mod, response.appCallNum, GV_AppCallNum1, GV_AppCallNum2));
					HANDLE_RETURN(disc(mod, response.appCallNum));
					break;
				default:
				/* Unexpected msg. Logged in examineDynMgr.. */
					break;
			} /* switch rc */
		}

    	switch(zRecordMedia->interruptOption)
    	{
        	case FIRST_PARTY_INTERRUPT:
	        case SECOND_PARTY_INTERRUPT:
    	        if(*typeAheadBuffer != '\0')
				{
					unlink(tempFileName);
        	        HANDLE_RETURN(TEL_SUCCESS);
				}
            	break;

	        case NONINTERRUPT:
    	    default:
        	    break;
    	}

	}

	requestSent=0;
	while ( 1 )
	{
		if (!requestSent)
		{
			rc = readResponseFromDynMgr(mod,-1,&response,
									sizeof(response));
			if (rc == -1)
			{
				unlink(tempFileName);
				HANDLE_RETURN(TEL_FAILURE);
			}

			if (rc == -2)
			{
				rc = sendRequestToDynMgr(mod, &msg);
				if (rc == -1)
				{
					unlink(tempFileName);
					HANDLE_RETURN(TEL_FAILURE);
				}

				requestSent=1;
				rc = readResponseFromDynMgr(mod,0,
											&response,sizeof(response));

				if(rc == -1)
				{
					unlink(tempFileName);
					HANDLE_RETURN(TEL_FAILURE);
				}
			}
		}
		else
		{
			rc = readResponseFromDynMgr(mod,0,
										&response,sizeof(response));
			if(rc == -1)
			{
				unlink(tempFileName);
				HANDLE_RETURN(TEL_FAILURE);
			}
		}

		rc = examineDynMgrResponse(mod, &msg, &response);

		switch(rc)
		{
			case DMOP_RECORDMEDIA:

				if ( zRecordMedia->interruptOption != NONINTERRUPT ) 
				{
					if((zRecordMedia->terminateChar == ' ') || 
				   		(zRecordMedia->terminateChar == '\0') ||
				   		(zRecordMedia->terminateChar == '!') ||
				   		(zRecordMedia->terminateChar == response.message[0]) ||
				   		(strchr(GV_RecordTerminateKeys, response.message[0])!=NULL))
					{
						GV_RecordTermChar = response.message[0];

						telVarLog(mod,REPORT_VERBOSE, TEL_BASE, GV_Info,
							"GV_RecordTermChar=%c", GV_RecordTermChar);
					}
				}


				saveTypeAheadData(mod, msg.appCallNum, "CLEAR");

				if(access(tempFileName, F_OK) == 0)
				{
					unlink(tempFileName);
				}

				HANDLE_RETURN(response.returnCode);

			break;
			case DMOP_GETDTMF:
				if (response.appCallNum != msg.appCallNum)
				{
					/* Save data for the other leg of the call */
					saveTypeAheadData(mod, response.appCallNum,
										response.message);
					break;
				}
				if ( zRecordMedia->interruptOption != NONINTERRUPT )
				{
					if((zRecordMedia->terminateChar == ' ') ||
						(zRecordMedia->terminateChar == '\0') ||
						(zRecordMedia->terminateChar == '!') ||
						(zRecordMedia->terminateChar == response.message[0]) ||
						(strchr(GV_RecordTerminateKeys, response.message[0])!=NULL))
					{
						GV_RecordTermChar = response.message[0];
						telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
							"GV_RecordTermChar=%c", GV_RecordTermChar);

						msg.opcode=DMOP_STOPACTIVITY;
						sendRequestToDynMgr(mod, &msg);

						usleep(50);

						saveTypeAheadData(mod, msg.appCallNum,
											"CLEAR");
						HANDLE_RETURN(TEL_SUCCESS);
					}
				}
				break;
			case DMOP_DISCONNECT:

				if(access(tempFileName, F_OK) == 0)
				{
					unlink(tempFileName);
				}

				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
			default:
				/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		} /* switch rc */
	} /* while */

	if(access(tempFileName, F_OK) == 0)
	{
		unlink(tempFileName);
	}
}


static int parametersAreGood(char *mod, int party, int *interruptOption,
								char *audiofilename, char *videofilename,
								int *sync, int *recordTime,
								int *audioCompression,int *videoCompression,
								int *audiooverwrite, int *videooverwrite,
								int *lead_silence, int *trail_silence,
								int beep, char terminate_char)
{
	int errors=0;
	char partyValues[]="FIRST_PARTY, SECOND_PARTY";
	int  recordTimeMin=1;
	int  recordTimeMax=1000;
	int  recordTimeForever=-1;
	int  defaultRecordTime = 60;
	int  defaultLeadSilence = 3;
	int  defaultTrailSilence = 3;
	char interruptOptionValues[]="INTERRUPT,NONINTERRUPT, FIRST_PARTY_INTERRUPT, SECOND_PARTY_INTERRUPT";
	char overwriteValues[]="YES, NO, APPEND";
	char synchronousValues[]="SYNC, ASYNC";
	char beepValues[]="YES, NO";
	char termCharValues[]="0-9, *, #";
	char tempBuf[80];

	switch(party)
	{
		case FIRST_PARTY:
			break;
		case SECOND_PARTY:
			if (!GV_Connected2)
			{
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Failed to access party=SECOND_PARTY, but second party is not connected.");
			}
			break;
		case SECOND_PARTY_NO_CALLER:
		/* Do we want to support this option?? */
			break;
		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Failed to interpret value for party: %d. Valid values are %s.",
						party, partyValues);
			break;
	}

	if ((*recordTime > recordTimeMax) ||
		(*recordTime < recordTimeMin && *recordTime != recordTimeForever))
	{
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Failed to interpret value for recordTime: %d. "
			"Valid values are %d to %d or %d (record until disconnect). Using default %d.",
			*recordTime, recordTimeMin, recordTimeMax,
			recordTimeForever, defaultRecordTime);
		*recordTime = defaultRecordTime;
	}

	if(*lead_silence <= 0)
	{
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
					"Invalid lead_silence parameter: %d. Must be greater "
					"than 0. Using default %d.",
					*lead_silence, defaultLeadSilence);
					*lead_silence = defaultLeadSilence;
	}

	if(*trail_silence <= 0)
	{
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
					"Invalid trail_silence parameter: %d. Must be greater "
					"than 0. Using default %d.",
					*trail_silence, defaultTrailSilence);
		*trail_silence = defaultTrailSilence;
	}


//	if(strcmp(audiofilename, "NULL"))
	if(audiofilename != NULL && audiofilename[0] != '\0')
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
					"Audio File name (%s)", audiofilename);
		switch(*audioCompression)
		{
			case COMP_64PCM:
			case COMP_WAV:
			case COMP_MVP:
			case COMP_711:
			case COMP_711R:
			case COMP_729:
			case COMP_729R:
			case COMP_729A:
			case COMP_729AR:
			case COMP_729BR:
														
				break;
			case COMP_729B:
				telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
					"audioCompression=%d: Recording of 729B without RTP is not supported. Setting it to COMP_729BR.", 
					*audioCompression);
				*audioCompression = COMP_729BR;
				break;

			default:
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Failed to interpret value for audio compression: %d.", *audioCompression);
				break;
		}
	}

//	if(strcmp(videofilename,"NULL"))
	if(videofilename != NULL && videofilename[0] != '\0')
	{
		telVarLog(mod,REPORT_VERBOSE, TEL_INVALID_PARM, GV_Info,
					"Video File name (%s)", videofilename);


		switch(*videoCompression)
		{
			case COMP_H263:
			case COMP_H263R_QCIF:
			case COMP_H263R_CIF:
				break;

			default:
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Failed to interpret value for compression: %d. "
							"Not supported.", *videoCompression);
				break;
		}
	}

	switch(beep)
	{
		case YES:
		case NO:
			break;
		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Failed to interpret value for beep: %d. Valid values are %s.",
						beep, beepValues);
	}

	switch(terminate_char)
	{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case '#': case '*': case '\0': case ' ':
			break;
		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Failed to interpret value for termChar: %c. Valid values are %s.",
						terminate_char, termCharValues);
	}

//	if(!strcmp(audiofilename, "NULL") && !strcmp(videofilename,"NULL"))

	if( (audiofilename == NULL || audiofilename[0] == '\0') &&
		(videofilename == NULL || videofilename[0] == '\0'))
	{
		memset(tempBuf, 0, sizeof(tempBuf));
		get_uniq_filename(tempBuf);
		sprintf(audiofilename, "%s.wav", tempBuf);
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
					"Parameter video filename is NULL. Using autocreated "
					"audiofilename (%s).", audiofilename);
        get_uniq_filename(tempBuf);
        sprintf(videofilename, "%s.263", tempBuf);
		telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
					"Parameter video filename is NULL. Using autocreated "
					"videofilename (%s).", videofilename);	
	
	}
	else //if(!strcmp(audiofilename, "NULL"))
	if(audiofilename == NULL || audiofilename[0] == '\0')
	{
		memset(tempBuf, 0, sizeof(tempBuf));
		sprintf(audiofilename, "%s", "NULL");
	}
	else //if(!strcmp(videofilename, "NULL"))
	if(videofilename == NULL || videofilename[0] == '\0')
	{
		memset(tempBuf, 0, sizeof(tempBuf));
		telVarLog(mod, REPORT_VERBOSE, TEL_INVALID_PARM, GV_Warn,
					"Parameter video filename is NULL. Using autocreated "
					"videofilename (%s).", videofilename);
	}

//	if(strcmp(audiofilename, "NULL"))
	if(audiofilename != NULL && audiofilename[0] != '\0')
	{
		switch(*audiooverwrite)
		{
			case YES:
				break;
			case APPEND:
				if(access(audiofilename, F_OK) != 0)
				{
					*audiooverwrite = YES;
				}
			break;
			case NO:
				if(access(audiofilename, F_OK) == 0)
				{
					errors++;
					telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
								"Failed to overwrite, File (%s) exists and overwrite=NO. Cannot overwrite file.",
								audiofilename);
				}
				break;
			default:
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Failed to interpret value for overwrite: %d. Valid values are %s.",
							*audiooverwrite, overwriteValues);
				break;
		}
	}

//	if(strcmp(videofilename, "NULL"))
	if(videofilename != NULL && videofilename[0] != '\0')
	{
		switch(*videooverwrite)
		{
			case YES:
				break;
			case APPEND:
				telVarLog(mod,REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
							"videooverwrite=APPEND is not available. Setting to default value YES.");
				*videooverwrite = YES;
			break;
			case NO:
				if(access(videofilename, F_OK) == 0)
				{
					errors++;
					telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Failed to overwrite, File (%s) exists and overwrite=NO. Cannot overwrite file.",
							videofilename);
				}
				break;
			default:
				errors++;
				telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
							"Failed to interpret value for overwrite: %d. Valid values are %s.",
							*videooverwrite, overwriteValues);
				break;
		}
	}



	switch(*interruptOption)
	{
		case INTERRUPT:
			switch(party)
			{
				case FIRST_PARTY:
					telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
								"Invalid interrupt_option: INTERRUPT. "
								"Using FIRST_PARTY_INTERRUPT.");
					*interruptOption = FIRST_PARTY_INTERRUPT;
					break;

				case SECOND_PARTY:
				case SECOND_PARTY_NO_CALLER:
					telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
								"Invalid interrupt_option: INTERRUPT. "
								"Using SECOND_PARTY_INTERRUPT.");
					*interruptOption = SECOND_PARTY_INTERRUPT;
					break;

				case BOTH_PARTIES:
					telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
								"Invalid interrupt_option: INTERRUPT. "
								"Using NONINTERRUPT.");
					*interruptOption = NONINTERRUPT;
					break;

				default:
					break;
			}
			break;

		case NONINTERRUPT:
			break;

		case FIRST_PARTY_INTERRUPT:
			if((party == SECOND_PARTY) ||
				(party == SECOND_PARTY_NO_CALLER))
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
							"Invalid interrupt_option: FIRST_PARTY_INTERRUPT "
							"when party is SECOND_PARTY or SECOND_PARTY_NO_CALLER. "
							"Using SECOND_PARTY_INTERRUPT.");
				*interruptOption = SECOND_PARTY_INTERRUPT;
			}
			break;

		case SECOND_PARTY_INTERRUPT:
			if(party == FIRST_PARTY)
			{
				telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Warn,
							"Invalid interrupt_option: SECOND_PARTY_INTERRUPT "
							"when party is FIRST_PARTY. "
							"Using FIRST_PARTY_INTERRUPT.");
				*interruptOption = FIRST_PARTY_INTERRUPT;
			}
			break;

		default:
			errors++;
			telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Failed to interpret value for interruptOption: %d. "
						"Valid values are %s",
			*interruptOption, interruptOptionValues);
			break;
	}

	switch(*sync)
	{
		case SYNC:
		case ASYNC:
			break;
		default:
			errors++;
			telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
						"Failed to interpret value for synchronous: %d. "
						"Valid values are %s.", sync, synchronousValues);
		break;
	}

	return(errors == 0);
}

#if 0
int get_uniq_filename(char  *filename)
{
	char    template[20];
	int     proc_id, rc;
	long    seconds;

	filename[0] = '\0';
	proc_id = getpid();
	sprintf(filename, "%d", proc_id);
	time(&seconds);
	sprintf(template, "%ld", seconds);
	strcat(filename, &template[4]);/* append all but 1st 4 digits of tics */

	/* Check to see if file exists, if so, try again. */
	rc = access(filename, F_OK);
	if(rc == 0)
	{
		get_uniq_filename(filename);
	}
	return(0);
}

#endif


