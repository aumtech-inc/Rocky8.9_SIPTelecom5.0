//#define DEBUG
/*-----------------------------------------------------------------------------
Program:        TEL_GetDTMF.c
Purpose:        Get DTMF characters. 
Author:         Wenzel-Joglekar
Date:		09/14/2000
Update:		10/12/2000 gpw has all ALPHA, DOLLAR, ALPHANUM
Update:		03/28/2001 apj Added code for beep.
Update:		04/09/2001 apj Added validation for length.
Update: 	04/25/2001 apj Checked Beep phrase existance if beep = YES.
Update: 	04/25/2001 apj On retry, speak last phrase if present.
Update: 	04/27/2001 apj In parameter validation, put the missing value
			       passed in the log message.
Update: 	04/27/2001 apj Changed valid ranges and adding defaults for 
				first digit, inter digit, tries. 
Update: 	04/30/2001 apj Make 1-35 as valid range for length parameter.
Update: 	04/30/2001 apj Added DOLLAR option.
Update: 	05/01/2001 apj Added ALPHA option.
Update: 	05/01/2001 apj Added ALPHANUM option.
Update: 	05/04/2001 apj Check for typed ahead keys before beeping.
Update: 	05/07/2001 apj Make AUTO, AUTOSKIP with ALPHA, ALPHANUM valid.
Update: 	06/26/2001 apj undef DEBUG
Update: 	06/26/2001 apj Added mod to get_characters.
Update: 	06/27/2001 apj Extract all typed ahead keys at the beginning.
Update: 	06/28/2001 apj While reprompting pass which party interrupt.
Update: 	06/28/2001 apj Added GV_BeepFile Handling.
Update: 	07/02/2002 apj Changes for 0 timeout.
Update: 	03/05/2004 apj In the beep speak, make synchronous as SYNC.
Update: 	06/21/2004 apj Return buffer on TIMEOUT.
Update: 	09/15/2004 apj Return disconnect from tel_Speak.
Update:     09/10/2013 djb Added Network Transfer logic
Update:     11/10/2014 djb MR 4192 - ALPAHANUM - fix in replaceSpecialCharacters
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#ifdef NETWORK_TRANSFER
#include "/home/devmaul/isp2.2/NetworkTransfer/src/NT.h"
#endif

static int parametersAreGood(char *mod, int party, int *firstDigitTimeout,
			int *interDigitTimeout, int *tries, int beep, 
			char termChar, int length, int terminateOption, 
			int inputOption, char *allowableChars);
static void handle_timeout() ;
static int examineWhatWeHave(char *input, int terminateOption, int length,
		int inputOption, int termCharPressed, char *allowableChars, 
		int *apiReturnCode, int nextTimeStartIndex);

#if 0
/*-----------------------------------------------------------------------------
This routine will get the touch tone key inputs from the user.
int len;    MANDATORY - Max. length of the user input. Max. 35.
        AUTOSKIP - Fixed length of the user input. Max. 35. 
int option; MANDATORY - Variable number of characters ended by #. 
        AUTOSKIP - Fixed number of characters.Can be terminated with                       a terminate key.
        AUTO- Fixed number of characters. 
int type;   NUMERIC (0-9).
        ALPHA (0-9) with each 2 digits representing A-Z,0-9. 
        DOLLAR (0-9 with * for decimal point). 
        NUMSTAR (0-9,* and # if AUTOSKIP.
        ALPHANUM (0-9 and * before each 2-digit ALPHA character).
        back and forth to NUMERIC with a *. 
        back and forth to 2-digit ALPHA with a *.
char    *data;  data entered by the user 
------------------------------------------------------------------------------*/

extern int tel_Speak(char *mod, int interruptOption, void *m, int updateLastPhraseSpoken);

int TEL_InputDTMF(int len, int option, int type, char *data)
{
	retunrn TEL_GetDTMF(FIRST_PARTY, GV_Timeout, GV_InterDigitTimeout, 1, NO, GV_Beep, '#', len, option, type, data, NULL);
}
#endif

/*--------------------------------------------------------------------------
This function sets the number of seconds to wait for a keystroke depending
on what is in the buffer. If the buffer is empty we are waiting for the 
first keystroke, otherwise we are waiting for a subsequent keystroke.
--------------------------------------------------------------------------*/
int setTimeoutValue(char *buffer, int firstDigitTimeout, int interDigitTimeout)
{
	if (buffer[0] == '\0')
		return(firstDigitTimeout);
	else
		return(interDigitTimeout);
}

int replaceSpecialCharacters(char *mod, int inputOption, char *input, 
		int *apiReturnCode, int *nextTimeStartIndex)
{
	char tempBuf[64];
	int i,j;
	int alreadyGotDecimal = 0;
	int length;
	int tempInt, tempInt1;
	char *ptrToFirstStar;


//#ifdef DEBUG
//    fprintf(stderr, " %s:%s:%d input= <%s>\n", __FILE__,__func__, __LINE__, input);
//#endif

	sprintf(tempBuf, "%s", input); 
	switch(inputOption)
	{
		case DOLLAR:
			if(strchr(tempBuf, '.') != NULL)
				alreadyGotDecimal = 1;
			
			memset(input, 0, MAX_LENGTH);	
			strncpy(input, tempBuf, *nextTimeStartIndex);
			for(i=*nextTimeStartIndex; i<strlen(tempBuf); i++)
			{
				if(tempBuf[i] == '*')
				{
					if(alreadyGotDecimal == 1)
					{
					*apiReturnCode=TEL_EXTRA_STAR_IN_DOLLAR;
					}
					else
					{
						input[i] = '.';
						alreadyGotDecimal = 1;
					}
				}
				else
					input[i] = tempBuf[i];	
			}		
			*nextTimeStartIndex = i;
			break;

		case ALPHA:
			length = strlen(tempBuf);

			memset(input, 0, MAX_LENGTH);	
			strncpy(input, tempBuf, *nextTimeStartIndex);

			tempInt = length-*nextTimeStartIndex;
			if(tempInt%2 == 1)
			{
				input[length-1] = tempBuf[length-1];
			}

			for(i=*nextTimeStartIndex,j=*nextTimeStartIndex; 
i<*nextTimeStartIndex+tempInt/2; i=i+2,j++)
			{
			switch(tempBuf[i])
			{	
				case '0':
					input[j] = tempBuf[i+1];
					break;

				case '1':
					switch(tempBuf[i+1])
					{
						case '1':	
							input[j] = 'Q';
							break;

						case '2':	
							input[j] = 'Z';
							break;

						default:
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Invalid keystroke (%c%c).", tempBuf[i], tempBuf[i+1]);
  			memset(input, 0, MAX_LENGTH);	
			*apiReturnCode=TEL_INVALID_INPUT;
			return(-1);
							break;
					}
					break;

				default:
					if((tempBuf[i+1] < '1') ||
					   (tempBuf[i+1] > '3'))
					{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Invalid keystroke (%c%c).", tempBuf[i], tempBuf[i+1]);
  			memset(input, 0, MAX_LENGTH);	
			*apiReturnCode=TEL_INVALID_INPUT;
			return(-1);
						break;
					}
					switch(tempBuf[i])
					{
					case '2':
						input[j]='A'+tempBuf[i+1]-'1';
						break;

					case '3':
						input[j]='D'+tempBuf[i+1]-'1';
						break;

					case '4':
						input[j]='G'+tempBuf[i+1]-'1';
						break;

					case '5':
						input[j]='J'+tempBuf[i+1]-'1';
						break;

					case '6':
						input[j]='M'+tempBuf[i+1]-'1';
						break;

					case '7':
						if(tempBuf[i+1]<'2')
						input[j]='P'+tempBuf[i+1]-'1';
						else
						input[j]='P'+tempBuf[i+1]-'1'+1;
						break;

					case '8':
						input[j]='T'+tempBuf[i+1]-'1';
						break;

					case '9':
						input[j]='W'+tempBuf[i+1]-'1';
						break;

					default:
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Invalid keystroke (%c).", tempBuf[i]);  
			memset(input, 0, MAX_LENGTH);	
			*apiReturnCode=TEL_INVALID_INPUT;
			return(-1);
						break;
					}
					break;
			}
			*nextTimeStartIndex = j+1;
			}
			
			break;

		case ALPHANUM:
			length = strlen(tempBuf);

			if((ptrToFirstStar = strchr(tempBuf, '*')) != NULL)
			{
				*nextTimeStartIndex = ptrToFirstStar - tempBuf;
				memset(input, 0, MAX_LENGTH);	
				strncpy(input, tempBuf, *nextTimeStartIndex);
			}
			else
			{
				*nextTimeStartIndex = length;
				return(0);
			}

			tempInt = length-*nextTimeStartIndex;
			switch(tempInt%3)
			{
				case 2:
					input[length-2]=tempBuf[length-2];
				case 1:
					input[length-1]=tempBuf[length-1];
					break;
				default:
					break;
			}

			tempInt1 = *nextTimeStartIndex+tempInt/3;
			for(i=*nextTimeStartIndex,j=*nextTimeStartIndex; 
i<tempInt1; i=i+3,j++)
			{
			switch(tempBuf[i])
			{
				case '*':
					break;

				default:
					input[j] = tempBuf[i];
					*nextTimeStartIndex = j+1;
					i = i-2;
					continue;
					break;
			}

			switch(tempBuf[i+1])
			{	
				case '0':
					input[j] = tempBuf[i+2];
					break;

				case '1':
					switch(tempBuf[i+2])
					{
						case '1':	
							input[j] = 'Q';
							break;

						case '2':	
							input[j] = 'Z';
							break;

						default:
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Invalid keystroke (%c%c%c).", tempBuf[i], 
			tempBuf[i+1], tempBuf[i+2]);
  			memset(input, 0, MAX_LENGTH);	
			*apiReturnCode=TEL_INVALID_INPUT;
			return(-1);
							break;
					}
					break;

				default:
					if((tempBuf[i+2] < '1') ||
					   (tempBuf[i+2] > '3'))
					{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Invalid keystroke (%c%c%c).", tempBuf[i], 
			tempBuf[i+1], tempBuf[i+2]);
  			memset(input, 0, MAX_LENGTH);	
			*apiReturnCode=TEL_INVALID_INPUT;
			return(-1);
						break;
					}
					switch(tempBuf[i+1])
					{
					case '2':
						input[j]='A'+tempBuf[i+2]-'1';
						break;

					case '3':
						input[j]='D'+tempBuf[i+2]-'1';
						break;

					case '4':
						input[j]='G'+tempBuf[i+2]-'1';
						break;

					case '5':
						input[j]='J'+tempBuf[i+2]-'1';
						break;

					case '6':
						input[j]='M'+tempBuf[i+2]-'1';
						break;

					case '7':
						if(tempBuf[i+2]<'2')
						input[j]='P'+tempBuf[i+2]-'1';
						else
						input[j]='P'+tempBuf[i+2]-'1'+1;
						break;

					case '8':
						input[j]='T'+tempBuf[i+2]-'1';
						break;

					case '9':
						input[j]='W'+tempBuf[i+2]-'1';
						break;

					default:
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Invalid keystroke (%c).", tempBuf[i+1]);  
			memset(input, 0, MAX_LENGTH);	
			*apiReturnCode=TEL_INVALID_INPUT;
			return(-1);
						break;
					}
					break;
			}
			*nextTimeStartIndex = j+1;
			}
			
			break;

		default:
			*nextTimeStartIndex = strlen(input);
			break;
	}

    //fprintf(stderr, " %s:%s:%d input= <%s>\n", __FILE__,__func__, __LINE__, input);

	return(0);
}

/*-----------------------------------------------------------------------------
------------------------------------------------------------------------------*/


int TEL_GetDTMF(int party, int firstDigitTimeout,
                        int interDigitTimeout, int tries,
                        int beep, char termChar, int length,
                        int terminateOption, int inputOption,
                        char *data, char *futureUse)
{
//printf("RGDEBUG::%d::%s::\n", __LINE__, __FUNCTION__);fflush(stdout);

	static char mod[]="TEL_GetDTMF";
	char apiAndParms[MAX_LENGTH]  = "";
	char apiAndParmsFormat[]="%s(%s,%d,%d,%d,%s,%c,%d,%s,%s,data,future)";
 
 	int  rc;
 	int  needMoreCharacters=1;
 	int  termCharPressed=0;
 	int  numChars;
 	int  apiReturnCode=0;
	int  maxCharsToGet;
	int  try;
	int timeout;
	char input[MAX_LENGTH];
	char allowableChars[MAX_LENGTH];
	char *typeAheadBuffer;
	int  firstTimeForThisTry = 1;
	char lBeepFileName[256];
	int  nextTimeStartIndex = 0;
	
	struct MsgToDM msg;
	struct MsgToApp response;
	struct Msg_Speak beepMsg;

	char partyVal[32] = "";
	char beepVal[32] = "";
	char terminateOptionVal[32] = "";
	char inputOptionVal[32] = "";
#ifdef NETWORK_TRANSFER
    struct Msg_SendMsgToApp msgToApp;
#endif

	sprintf(partyVal, "%s", arc_val_tostr(party));
	sprintf(beepVal, "%s", arc_val_tostr(beep));
	sprintf(terminateOptionVal, "%s", arc_val_tostr(terminateOption));
	sprintf(inputOptionVal, "%s", arc_val_tostr(inputOption));

	memset(apiAndParms,0, sizeof(apiAndParms));

	snprintf(apiAndParms, sizeof(apiAndParms), apiAndParmsFormat, mod, arc_val_tostr(party),
		firstDigitTimeout, interDigitTimeout, tries,
		arc_val_tostr(beep), termChar, length, arc_val_tostr(terminateOption),
		arc_val_tostr(inputOption));


/* ?? Do we need to adjust parameter for party ?? */
        rc = apiInit(mod, TEL_GETDTMF, apiAndParms, 1, 1, 0);
        if (rc != 0) HANDLE_RETURN(rc);
	
	if (!parametersAreGood(mod, party, &firstDigitTimeout,
                        &interDigitTimeout, &tries, beep, termChar, length,
                        terminateOption, inputOption, allowableChars)) 
        	HANDLE_RETURN (TEL_FAILURE);

	/* Note: msg is only set up to allow logging of unexpected msgs */
	msg.opcode=DMOP_GETDTMF;
	if (party == FIRST_PARTY)
	{
		typeAheadBuffer = GV_TypeAheadBuffer1;
		msg.appCallNum=GV_AppCallNum1;
	}
	else
	{
		typeAheadBuffer = GV_TypeAheadBuffer2;
		msg.appCallNum=GV_AppCallNum2;
	}

	msg.appPid=GV_AppPid;
	msg.appRef=GV_AppRef++;

	/*DDN: 07082011 For AUTO, termChar should something other than allowed inputs.*/
	if(terminateOption == AUTO)
	{
		termChar = 'Z';
	}

	if(beep == YES)
	{	
		/* Populate beep message tobe sent to DM. */	
        	beepMsg.opcode = DMOP_SPEAK;
		if (party == FIRST_PARTY || party == BOTH_PARTIES)
        		beepMsg.appCallNum = GV_AppCallNum1;
		else
        		beepMsg.appCallNum = GV_AppCallNum2;
        	beepMsg.appPid 	= GV_AppPid;
        	beepMsg.appRef 	= GV_AppRef;
        	beepMsg.appPassword = GV_AppPassword;
        	beepMsg.list 	= 0;
        	beepMsg.allParties 	= (party == BOTH_PARTIES);
        	beepMsg.synchronous = SYNC;

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
				"Failed to access beep file <%s>. errno=%d. "
				"Using default beep file.", GV_BeepFile, errno);
			}
		}

		rc = access(lBeepFileName, F_OK);
                if(rc == 0)
                {
			sprintf(beepMsg.file, "%s/%s", GV_SystemPhraseDir, 
				BEEP_FILE_NAME);
                }
                else
                {
			telVarLog(mod,REPORT_DETAIL,TEL_BASE,GV_Warn,
                        "Beep phrase <%s> doesn't exist. errno=%d. Setting "
			" beep to NO.", lBeepFileName, errno);
			beep = NO;
                }
	}
	
	memset(input,'\0', sizeof(input));

	while(1)
	{
#ifdef NETWORK_TRANSFER
        rc = readNTResponse(mod, -1, &response, sizeof(response));
        if ( (rc != -1) && (rc != -2) )
        {
            rc = examineDynMgrResponse(mod, &msg, &response);
             if ( rc == DMOP_SENDMSGTOAPP )
            {
				telVarLog(mod,REPORT_VERBOSE,TEL_BASE,GV_Info,
                            "[%s, %d] Got DMOP_SENDMSGTOAPP", __FILE__, __LINE__);
                 memcpy (&msgToApp, &response, sizeof(msgToApp));
                 rc = addSendMsgToApp(&msgToApp);
                    HANDLE_RETURN(TEL_MSG_FROM_APP);
            }
        }
#endif

	rc = readResponseFromDynMgr(mod, -1, &response, sizeof(response));
	if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
	if(rc == -2) break;

	rc = examineDynMgrResponse(mod, &msg, &response);	
	switch(rc)
	{
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, 
							response.message);
			break;
        case DMOP_CONFERENCE_SEND_DATA:
            sprintf(GV_ConfData, "%s", response.message);
            continue;
            break;
        case DMOP_CONFERENCE_REMOVE_USER:
            if(isConferenceOn == YES)
            {
                isConferenceOn = NO;
                HANDLE_RETURN(-13);
                break;
            }
            continue;
        case DMOP_CONFERENCE_STOP:
            if(isConferenceOn == YES)
            {
                isConferenceOn = NO;
                HANDLE_RETURN(-12);
            }
            break;
        case DMOP_CONFERENCE_ADD_USER:
            isConferenceOn = YES;
            HANDLE_RETURN(-11);
            break;
		case DMOP_DISCONNECT:
			//HANDLE_RETURN(disc(mod, response.appCallNum, GV_AppCallNum1, GV_AppCallNum2));
			HANDLE_RETURN(disc(mod, response.appCallNum));
			break;
		default:
			/* Unexpected msg. Logged in examineDynMgr.. */
			break;
	} /* switch rc */
	}

	for (try=1; try<tries+1; try++)	
	{
		nextTimeStartIndex = 0;
		apiReturnCode = 0;
#ifdef DEBUG
		printf("TEL_GetDTMF::%d:: TypeAhead:<%s>, input:<%s>, length=%d\n", __LINE__, typeAheadBuffer, input, length);
#endif
		firstTimeForThisTry = 1;

     	while (needMoreCharacters)
		{ 
			if (terminateOption == MANDATORY)
				maxCharsToGet = 100000;
			else
				maxCharsToGet = length-nextTimeStartIndex;

			numChars = get_characters(mod, input, typeAheadBuffer, 
				termChar, maxCharsToGet, 
				allowableChars, &termCharPressed);
			if(numChars == TEL_INVALID_INPUT)
			{	
				typeAheadBuffer[0] = 0;	//DDN 07252011
				HANDLE_RETURN(TEL_INVALID_INPUT);
				break;
			}

			rc = replaceSpecialCharacters(mod, inputOption, input, 
				&apiReturnCode, &nextTimeStartIndex);
			if(rc != 0)
			{
				if(try == tries)
					HANDLE_RETURN(apiReturnCode);
				break;
			}
 
			rc = examineWhatWeHave(input, terminateOption, length,
				inputOption, termCharPressed, allowableChars, 
				&apiReturnCode, nextTimeStartIndex);
			if (rc == 0)
			{
				strcpy(data, input);
				HANDLE_RETURN(apiReturnCode);
			}

#ifdef DEBUG
			printf("TEL_GetDTMF::%d:: Have %d in input. Need more.\n", __LINE__, strlen(input)); 
#endif
			if((beep == YES) &&
			   (firstTimeForThisTry == 1) &&
			   (strlen(input) == 0))
			{
				rc=tel_Speak(mod, INTERRUPT, (void *)&beepMsg, 0);
				if(rc == TEL_DISCONNECTED)
				{
					HANDLE_RETURN(rc);
				}
			}
			firstTimeForThisTry = 0;

			timeout=setTimeoutValue(input, 
				firstDigitTimeout,interDigitTimeout);

			if(timeout == 0)
				timeout = -1;

			rc = readResponseFromDynMgr(mod, timeout, &response, sizeof(response));
#ifdef DEBUG
			printf("TEL_GetDTMF::%d:: readResponseFromDynMgr. response=%s rc=%d\n", __LINE__, response.message, rc); 
#endif
			if (rc == TEL_TIMEOUT)
			{
				strcpy(data, input);
#ifndef NETWORK_TRANSFER
				telVarLog(mod,REPORT_VERBOSE,TEL_BASE,GV_Info,
					"Timeout on try # %d.",try);
#endif
				break;
			}
			if(rc == -1) HANDLE_RETURN(TEL_FAILURE);

			rc = examineDynMgrResponse(mod, &msg, &response);	
			switch(rc)
			{
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum, response.message);
				break;
			case DMOP_DISCONNECT:
				//HANDLE_RETURN(disc(mod, response.appCallNum, GV_AppCallNum1, GV_AppCallNum2));
				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
#ifdef NETWORK_TRANSFER
            case DMOP_SENDMSGTOAPP:
                telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
                        "[%s, %d] Got DMOP_SENDMSGTOAPP", __FILE__, __LINE__);
                memcpy (&msgToApp, &response, sizeof(msgToApp));
                rc = addSendMsgToApp(&msgToApp);
                HANDLE_RETURN(TEL_MSG_FROM_APP);
                break;
#endif
			default:
				/* Unexpected msg. Logged in examineDynMgr.. */
				continue;
				break;
			} /* switch rc */
			
		} /* while needMoreCharacters */

		/* At this point, we timed out */
		memset(input,'\0', sizeof(input));
	
		/* Respeak unless he just timed out on his last attempt. */
		if (try < tries) 
		{
			/* ?? Note that INTERRUPT is hard coded */
			if (party == FIRST_PARTY)
			{
				if(GV_LastSpeakMsg1.opcode == DMOP_SPEAK)
				{
					rc=tel_Speak(mod, 
						FIRST_PARTY_INTERRUPT, 
						(void *)&GV_LastSpeakMsg1, 0);
					if(rc == TEL_DISCONNECTED)
					{
						HANDLE_RETURN(rc);
					}
				}
			}
			else
			{
				if(GV_LastSpeakMsg2.opcode == DMOP_SPEAK)
				{
					rc=tel_Speak(mod, 
						SECOND_PARTY_INTERRUPT, 
						(void *)&GV_LastSpeakMsg2, 0);
					if(rc == TEL_DISCONNECTED)
					{
						HANDLE_RETURN(rc);
					}
				}
			}
		}
	} /* for loop for tries */

	/* We must have timed out */	
	HANDLE_RETURN(TEL_TIMEOUT); 
}

/*-------------------------------------------------------------------------
This function verifies that all parameters are valid and sets any global
variables necessary for execution. It returns 1 (yes) if all parmeters are
good, 0 (no) if not.
-------------------------------------------------------------------------*/
static int parametersAreGood(char *mod, int party, int *firstDigitTimeout,
                        int *interDigitTimeout, int *tries, int beep, 
			char termChar, int length, int terminateOption, 
			int inputOption, char *allowableChars)
{
	int  errors=0;
	char termCharHolder[2];	
	char partyValues[]="FIRST_PARTY, SECOND_PARTY";
	int  triesMin=1;
	int  triesMax=15;
	int  firstDigitTimeoutMin=0;
	int  interDigitTimeoutMin=0;

#ifdef MRCP_SR
	int  firstDigitTimeoutMax=60;
	int  interDigitTimeoutMax=60;
#else
	int  firstDigitTimeoutMax=60000;
	int  interDigitTimeoutMax=60000;
#endif

	char beepValues[]="YES, NO";
	char termCharValues[]="0-9, A-D, a-d, *, #";
	char terminateOptionValues[]="MANDATORY, AUTOSKIP, AUTO";
	char inputOptionValues[]="NUMERIC, NUMSTAR, DOLLAR, ALPHA, ALPHANUM";

	switch(party)
	{
	case FIRST_PARTY:
	case SECOND_PARTY:
		break;
	default:
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for party: %d. Valid values are %s.", 
			party, partyValues);
	}

	if ((*firstDigitTimeout < firstDigitTimeoutMin) || 
	    (*firstDigitTimeout > firstDigitTimeoutMax))
	{
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
	"Invalid value for firstDigitTimeout: %d. Valid values are %d to %d. Using 5 seconds.", *firstDigitTimeout, firstDigitTimeoutMin, firstDigitTimeoutMax);
	*firstDigitTimeout = 5;
	}
	
	if ((*interDigitTimeout < interDigitTimeoutMin) || 
	    (*interDigitTimeout > interDigitTimeoutMax))
	{
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
	"Invalid value for interDigitTimeout: %d. Valid values are %d to %d. Using 5 seconds.", *interDigitTimeout, interDigitTimeoutMin, interDigitTimeoutMax);
		*interDigitTimeout = 5;
	}

	if ((*tries < triesMin) || (*tries > triesMax))
	{
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,  
		"Invalid value for tries: %d. Valid values are %d to %d. Using 1.", *tries, triesMin, triesMax);
		*tries = 1;
	}
	
	switch(beep)
	{
	case YES:
	case NO:
		break;
	default:
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
			"Invalid value for beep: %d. Valid values are %s.", 
			beep, beepValues);
	}

	switch(termChar)
	{
	case '0': case '1': case '2': case '3': case '4': case '5': case '6':
	case '7': case '8': case '9': case '#': case '*': case 'A': case 'B':
	case 'C': case 'D': case 'a': case 'b': case 'c': case 'd':
		break;
	default:
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
	      		"Invalid value for termChar: %d. Valid values are %s.",
			termChar, termCharValues);
	}
	
	memset(termCharHolder, 0, sizeof(termCharHolder));
	termCharHolder[0]=termChar;

	if((length < 1) || (length > 35))
	{
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
	      		"Invalid value for length: %d. Valid range is 1 to 35.",
			length);
	}

	switch(terminateOption)
	{
	case MANDATORY:
		strcat(allowableChars,termCharHolder); 
		break;
	case AUTOSKIP:
		strcat(allowableChars,termCharHolder); 
		break;
	case AUTO:
		break;
	default:
		errors++;
		telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"Invalid value for terminateOption: %d. Valid values are %s.", 
			terminateOption, terminateOptionValues);
	}
	
	switch(inputOption)
	{
	case NUMERIC:
		strcat(allowableChars,"0123456789"); 
		break;
	case NUMSTAR:
		strcat(allowableChars,"0123456789*#"); 
		break;
	case DOLLAR:
		strcat(allowableChars,"0123456789*"); 
		break;
	case ALPHA:
		strcat(allowableChars,"0123456789"); 
		break;
	case ALPHANUM:
		strcat(allowableChars,"0123456789*"); 
		break;
	default:
		errors++;
		telVarLog(mod,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
		"Invalid value for inputOption: %d. Valid values are %s.", 
			inputOption, inputOptionValues);
	}

	return( errors == 0);
}

/*----------------------------------------------------------------------------
This function examines the input that we have so far and determines whether
we (1) need more input, or (0) have the required length (and/or terminator).
Note: ?? We don't yet verify the allowable characters. 
----------------------------------------------------------------------------*/
static int examineWhatWeHave(char *buffer, int terminateOption, int length,
		int inputOption, int termCharPressed, char *allowableChars, 
		int *apiReturnCode, int nextTimeStartIndex)
{
#ifdef DEBUG
	printf("TELGetDTMF::%d::Input so far=<%s> termCharPressed=%d length=%d nextTimeStartIndex=%d\n", __LINE__, buffer, termCharPressed, length, nextTimeStartIndex);fflush(stdout);
#endif
	switch(terminateOption)
	{
	case MANDATORY:
		if (termCharPressed)
		{
			if (strlen(buffer) <= length)
			{
				return(0);
			}
			else
			{
				*apiReturnCode = TEL_OVERFLOW;
				return(0);
			}
		}
		break;
	case AUTOSKIP:
		if (termCharPressed)
		{
			if (strlen(buffer) <= length)
			{
				return(0);
			}
			else
			{
				*apiReturnCode = TEL_OVERFLOW;
				return(0);
			}
		}
		else
		{
			if(nextTimeStartIndex == length)
			{
				return(0);
			}
		}
		break;
	case AUTO:
		if(nextTimeStartIndex == length)
		{
			return(0);
		}
	}

	/* Indicate that we should continue getting input */
	return(1);
}

