/*-----------------------------------------------------------------------------
Program:	TEL_PromptAndCollect.c
Purpose:	Contains the TEL_PromptAndCollect API.
Author: 	Aumtech, Inc.
Update:		11/28/2001 djb	Created the file.
Update:		07/18/01 apj Ported to IP.
Update:		07/30/01 apj In sPncGetDTMFWithHotkeys, hardcoded D as 
		terminate key in a call to TEL_GetDTMF.
Update:		07/31/01 djb Fixed some sprintf() statements.
Update: 01/31/2002 djb Fixed the TEL_TIMEOUT returned on last try bug
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

#define RESULT_DISCONNECTED 	"1st party disconnect"
#define RESULT_SHORT_INPUT 	"Short input"
#define RESULT_LOST_AGENT	"2nd party disconnect"
#define RESULT_TIMEOUT		"Time out"
#define RESULT_FAILURE		"Failure"
#define RESULT_INVALID_INPUT	"Invalid input"
#define RESULT_UNKNOWN		"Unknown"
#define RESULT_OVERFLOW		"Overflow"
#define RESULT_SUCCESS		"Success"
#define RESULT_EXTRA_STAR_IN_DOLLAR "Extra star in dollar"
#define RESULT_HOTKEY		"Hot key pressed"

#define MAX_NUM_HOTKEYS     20
#define MAX_HOTKEY_LENGTH   20
#define	PNC_SUCCESS	0
#define	PNC_KEEP_TRYING	99

typedef struct hotkeyStruct
{
    int     numHotkeys;
    char    hotkey[MAX_NUM_HOTKEYS][MAX_HOTKEY_LENGTH];
} Hotkeys;

static int sOverrideTagData(char *zMod, char *zTagName, char *zTagData,
                        char *zDataOverrides, char *zModifiedData);
static int sPncGetHotkeys(char *zHotkeyList, Hotkeys *zHotkeys);
static int sPncGetInputBySection(char *zSectionName, PncInput *zPncInput);
static int sPncGetDTMFNoHotkeys(char *Mod, int zAttempt, PncInput 
		*zPncInput, char *zResult, char *zData);
static int sPncGetDTMFWithHotkeys(char *Mod, int zAttempt, PncInput *zPncInput, Hotkeys	*zHotkeys, char *zResult, char *zData);
static int sPressedHotkey(Hotkeys *zHotkeys,char *zInput, char *zHotkeyPressed);
static int sProcessOverrides(char *Mod, char *zSectionName, 
		char *zOverrides, char *zDataOverrides, PncInput *zPncInput);
static int sPncValidateInput(PncInput *zPncInput, char *zData);
static int validateInput(char *zData, int zInputOption, 
						char *zAllowedCharacters);
int TEL_PlayTag(char *zMod, int zParty, char *zPhraseTag, int zOption, 
				int zMode, char *zDataOverrides);

/* funtion declarations shared by TEL_LoadTags and TEL_PlayTag */
/* funtion declarations shared by TEL_PlayTag */
extern int sTranslateFormatToInt(char *zFormatStr, int *zFormat);
extern int sGetField(char zDelimiter, char *zInput, int zFieldNum,
                                                char *zFieldValue);
extern int validate_beep(int value);
extern int validate_party(int value);
extern int validate_range(int value, int min, int max);
extern int validate_inputOption(int value);
extern int validate_interruptOption(int value);

/*------------------------------------------------------------------------------
Prompt for and collect data
Return Codes:
  0: 	Success
< 0:	Failed to get valid input
> 0: 	Hot key pressed.
------------------------------------------------------------------------------*/
int TEL_PromptAndCollect(char *zSectionName, char *zOverrides, char *zData)
{
	static char Mod[] = "TEL_PromptAndCollect";
	char	yTmpInput[256];
	int	yRc;
	int	yGetRc; 	/* Return code from the GetDTMF function */
	int	yTry;
	char	yDiagMsg[128];
	char	yTagToPlay[128];
	char	yResult[128];
	char	yMonitorMsg[128];	/* For displaying results to monitor */
	char	yDataOverrides[512];
	Hotkeys	yHotkeys;
	PncInput yPncInput;

	sprintf(yDiagMsg, "%s(%s,%s,data)", Mod, zSectionName, zOverrides);
	yRc = apiInit(Mod, TEL_PROMPTANDCOLLECT, yDiagMsg, 1, 1, 0); 
	if (yRc != 0) HANDLE_RETURN(yRc);
 
	sprintf(LAST_API, "%s", Mod);

	if ( ! GV_TagsLoaded )
	{
		sprintf(Msg, "%s", TEL_TAGS_NOT_LOADED_MSG);
		telVarLog(Mod, REPORT_NORMAL, TEL_TAGS_NOT_LOADED, GV_Err, Msg);
		HANDLE_RETURN (-1);
	}

	zData[0] = '\0';

	/* Get the info necessary to speak and do input */
	memset((PncInput *)&yPncInput, 0, sizeof(yPncInput));
	if ((yRc = sPncGetInputBySection(zSectionName, &yPncInput)) != 0)
	{
		sprintf(Msg, TEL_SECTION_NOT_FOUND_MSG, zSectionName);
                telVarLog(Mod, REPORT_NORMAL, TEL_SECTION_NOT_FOUND, 
			GV_Err, Msg);
		HANDLE_RETURN (-1);
	}

	if (strcmp(zOverrides,"") != 0)
	{
		yRc = sProcessOverrides(Mod, zSectionName, zOverrides, 
					yDataOverrides, &yPncInput);
	}

	telVarLog (Mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			"After sProcessOverrides, dataOverrides=(%s).",
			yDataOverrides);

	memset((Hotkeys *)&yHotkeys, 0, sizeof(yHotkeys));
	yRc = sPncGetHotkeys(yPncInput.hotkeyList, &yHotkeys);

	for(yTry = 1; yTry <= yPncInput.nTries; yTry++)
	{
		/* Speak the prompt */
		if((yTry == 1) || (yPncInput.repromptTag[0] == '\0'))
		{
			strcpy(yTagToPlay, yPncInput.promptTag);
		}
		else
		{
			strcpy(yTagToPlay, yPncInput.repromptTag);
		}
		yRc = TEL_PlayTag(Mod, yPncInput.party, yTagToPlay, 
			yPncInput.interruptOption, SYNC, yDataOverrides);

		if (yRc == TEL_DISCONNECTED || yRc == TEL_LOST_AGENT)
		{
			HANDLE_RETURN (yRc);
		}

		/* Get the input */
		if ( 	( yHotkeys.numHotkeys == 0 ) ||
			(yPncInput.inputOption == ALPHA) ||
			(yPncInput.inputOption == ALPHANUM))
			
		{
			memset((char *)yTmpInput, 0, sizeof(yTmpInput));
			yGetRc = sPncGetDTMFNoHotkeys(Mod, yTry,
					&yPncInput, yResult, yTmpInput );
		}
		else
		{
			yGetRc = sPncGetDTMFWithHotkeys(Mod, yTry, 
				&yPncInput, &yHotkeys, yResult, yTmpInput);
		}
		telVarLog (Mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Got user input of (%s) rc=%d", yTmpInput, yGetRc);

		switch(yGetRc)
		{
		case TEL_SUCCESS:
			break;
		case TEL_DISCONNECTED:
		case TEL_LOST_AGENT:
			HANDLE_RETURN(yGetRc);
			break;
		case TEL_TIMEOUT:
			/* Here is where you fall thru if Alternate Term */
			sprintf(yMonitorMsg,
				"Try %d of %d: Timed out after receiving (%s).",
				yTry, yPncInput.nTries, yTmpInput);
			send_to_monitor(MON_DATA,0, yMonitorMsg);
			sprintf(zData, "%s", yTmpInput);
			if ( (yPncInput.terminateOption == AUTOSKIP) ||
			     (yPncInput.terminateOption == AUTO) )
			{
				if((int)strlen(yTmpInput) >= yPncInput.minLen)
				{
					yGetRc = TEL_SUCCESS;
					break;
				}
			}
			if (yTry==yPncInput.nTries) 
			{
				sprintf(Msg, TEL_GAVE_UP_MSG, yTry, yResult);
       			telVarLog(Mod, REPORT_NORMAL, TEL_GAVE_UP, GV_Err, Msg);
				HANDLE_RETURN(TEL_TIMEOUT);

			}
			if (yPncInput.timeoutTag[0] != '\0')
			{
				yRc = TEL_PlayTag(Mod, yPncInput.party,
					yPncInput.timeoutTag, 
					yPncInput.interruptOption,SYNC, 
					yDataOverrides);
			}
			continue;
			break;	
		case TEL_EXTRA_STAR_IN_DOLLAR:
			/* yGetRc=-4 now; break to validate */
			break;
		case TEL_FAILURE:
			/* Should only happen if bad parms passed. */
			sprintf(Msg, TEL_GAVE_UP_MSG, yTry, yResult);
			telVarLog(Mod, REPORT_NORMAL, TEL_GAVE_UP, GV_Err, Msg);
			HANDLE_RETURN(yGetRc);
		case TEL_INVALID_INPUT:
		case TEL_OVERFLOW:
		default:
			if (yGetRc > 0)
			{
				/* Hot key pressed. */
				sprintf(zData, "%s", yTmpInput);
				sprintf(yMonitorMsg,
					"Try %d of %d: Got hotkey (%s).",
					yTry, yPncInput.nTries, yTmpInput);
				send_to_monitor(MON_DATA,0,yMonitorMsg);
				HANDLE_RETURN(yGetRc);
			}
			if (yTry==yPncInput.nTries) 
			{
				sprintf(Msg, TEL_GAVE_UP_MSG, yTry, yResult);
				telVarLog(Mod, REPORT_NORMAL, TEL_GAVE_UP, GV_Err, Msg);
				HANDLE_RETURN(TEL_FAILURE);
			}
			if (yGetRc == TEL_OVERFLOW) 
			{
				sprintf(yMonitorMsg,
					"Try %d of %d: Got data overflow (%s).",
					yTry, yPncInput.nTries,yTmpInput);
				send_to_monitor(MON_DATA,0,yMonitorMsg);
			}
			else 
			{
				sprintf(yMonitorMsg,
					"Try %d of %d: Got invalid data (%s).",
					yTry, yPncInput.nTries,yTmpInput);
				send_to_monitor(MON_DATA,0,yMonitorMsg);
			}
			if (yGetRc == TEL_OVERFLOW && 
					yPncInput.overflowTag[0] != 0)
			{
				yRc = TEL_PlayTag(Mod, yPncInput.party, 
					yPncInput.overflowTag, yPncInput.interruptOption,SYNC, 
					yDataOverrides);
			}
			else
			if (yPncInput.invalidTag[0] != '\0')
			{
				yRc = TEL_PlayTag(Mod, yPncInput.party, 
					yPncInput.invalidTag, 
					yPncInput.interruptOption,SYNC, 
					yDataOverrides);
			}
			continue;
			break;	
		} /* switch */

		/*  Validate the data.  */
		yRc=validateInput(yTmpInput, yPncInput.inputOption,
				yPncInput.validKeys);
		switch(yRc)
		{
		case 0:
			/* Data is invalid */
			sprintf(yMonitorMsg,
				"Try %d of %d: Got invalid input (%s).",
				yTry, yPncInput.nTries,yTmpInput);
			send_to_monitor(MON_DATA,0, yMonitorMsg);
			if (yTry == yPncInput.nTries)
			{
				sprintf(Msg,TEL_GAVE_UP_MSG, yTry, yResult);
				telVarLog(Mod, REPORT_NORMAL, TEL_GAVE_UP, 
					GV_Err, Msg);
				HANDLE_RETURN(TEL_FAILURE);
			}
			if (yPncInput.invalidTag[0] != '\0')
			{
				yRc = TEL_PlayTag(Mod, yPncInput.party,
				yPncInput.invalidTag,
				yPncInput.interruptOption,SYNC, 
					yDataOverrides);
			}
			continue;	
			break;
		case 1:
			/* Data is valid */
			break;
		} /* switch */
		telVarLog (Mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			"yTmpInput=(%s), minLen=%d, maxLen=%d",
			yTmpInput, yPncInput.minLen, yPncInput.maxLen);	

		/*  Validate the length (for shortness) */
		if((int)strlen(yTmpInput) < yPncInput.minLen)
		{
			sprintf(yMonitorMsg,
				"Try %d of %d: Got short input (%s).",
				yTry, yPncInput.nTries, yTmpInput);
			send_to_monitor(MON_DATA,0, yMonitorMsg);
			if (yTry == yPncInput.nTries)
			{
				sprintf(Msg, TEL_GAVE_UP_MSG, yTry,
							RESULT_SHORT_INPUT);
				telVarLog(Mod, REPORT_NORMAL, TEL_GAVE_UP, GV_Err, Msg);
		 		return(TEL_FAILURE); 
			}
			if (yPncInput.shortInputTag[0] != '\0')
			{
				yRc = TEL_PlayTag(Mod, yPncInput.party,
					yPncInput.shortInputTag,
					yPncInput.interruptOption,SYNC,
					yDataOverrides);
			}
			else if (yPncInput.invalidTag[0] != '\0')
			{
				yRc = TEL_PlayTag(Mod, yPncInput.party,
					yPncInput.invalidTag, yPncInput.interruptOption,SYNC,
					yDataOverrides);
			}
			else
			{
				sprintf(Msg, "%s",
					"Both invalidTag & shortInputTag "
					"are empty.");
				telVarLog(Mod, REPORT_VERBOSE, TEL_BASE, GV_Warn, Msg);
			}
			continue;
		}

		/* At this point, everything is OK. */
		sprintf(zData, "%s", yTmpInput);
		sprintf(yMonitorMsg,
			"Try %d of %d: Got valid input (%s). rc=%d.",
			yTry, yPncInput.nTries,yTmpInput,yGetRc);
		send_to_monitor(MON_DATA, 0, yMonitorMsg);
		HANDLE_RETURN (yGetRc);
	}

	sprintf(Msg, TEL_GAVE_UP_MSG, yTry, yResult);
	telVarLog(Mod, REPORT_NORMAL, TEL_GAVE_UP, GV_Err, Msg);
	
	HANDLE_RETURN (TEL_FAILURE);

} /* END: TEL_PromptAndCollect() */

/*------------------------------------------------------------------------------
Purpose: Modify the variable values in the section structure based on the 
	values of the variable(s) specified in the override parameter.
	Those name-value pairs in which the name has a "." are considered
	to be overrides to data tag data fields and are broken out and 
	returned as a semicolon delimited field in zDataOverrides.
	This is subsequently passed to the speak routine where tag 
	data values may be overridden as they are read in to be spoken.
Return values: 
	0: always 
------------------------------------------------------------------------------*/
static int sProcessOverrides(char *zMod, char *zSectionName, 
		char *zOverrides, char *zDataOverrides, PncInput *zPncInput)
{
	int i;
	int yRc;
	char yNameValue[256];
	char yName[256];
	char yValue[256];
	char yTemp[256];
	int  yTempInt;
	int yNumDataOverrides=0; 
	
	if (strcmp(zOverrides,"") == 0) 
	{
		zDataOverrides[0]='\0';
		return(0);
	}

	zDataOverrides[0]='\0';

	i=1;
	while ( yRc = sGetField(';', zOverrides, i, yNameValue) > 0)
	{
 		i++;	
		yRc = sGetField('=', yNameValue, 1, yName);

		/* If there is a period in the name it's a data override */
		if (strchr(yNameValue, '.') != NULL )
		{
			if (yNumDataOverrides) 
				strcat(zDataOverrides,";");
			strcat(zDataOverrides,yNameValue);
			yNumDataOverrides++;
			continue;
		}

		yRc = sGetField('=', yNameValue, 2, yValue);
		if (strcmp(yName,NV_VALID_KEYS) == 0)
		{
			strcpy(yTemp, zPncInput->validKeys);
			sprintf(Msg, TEL_OVERRIDE_PARM_MSG, yName, 
					zSectionName, yTemp, yValue);
			telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
			strcpy(zPncInput->validKeys,yValue);
			continue;
		} 
 		if (strcmp(yName,NV_HOTKEY_LIST) == 0)
		{
			strcpy(yTemp, zPncInput->hotkeyList);
			sprintf(Msg, TEL_OVERRIDE_PARM_MSG, yName, 
					zSectionName, yTemp, yValue);
			telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
			strcpy(zPncInput->hotkeyList,yValue);
			continue;
		} 
 		
 		if (strcmp(yName,NV_PARTY) == 0)
		{
			sTranslateFormatToInt(yValue,&yTempInt);
			if (validate_party(yTempInt) == 0)
			{
				sprintf(Msg, TEL_OVERRIDE_PARM_MSG,
					yName, zSectionName, 
					arc_val_tostr(zPncInput->party), yValue);
			telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
			   	zPncInput->party=yTempInt;
			}
			else 
			{
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG,
					yValue, yName, arc_val_tostr(zPncInput->party));
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
			}
			continue;
		} 
		if (strcmp(yName,NV_FIRST_DIGIT_TIMEOUT) == 0)
		{

			if (validate_range(atoi(yValue),1,60) == 0)
			{
				sprintf(yTemp, "%d", 
					zPncInput->firstDigitTimeout);
				sprintf(Msg, TEL_OVERRIDE_PARM_MSG, yName, 
					zSectionName, yTemp, yValue);
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
				zPncInput->firstDigitTimeout = atoi(yValue);	
			}
			else
			{
			       sprintf(yTemp, "%d",
						zPncInput->firstDigitTimeout);
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG,
					yValue, yName, yTemp );
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
			}
			continue;
		} 
		if (strcmp(yName,NV_INTER_DIGIT_TIMEOUT) == 0)
		{

			if (validate_range(atoi(yValue),1,60) == 0)
			{
				sprintf(yTemp, "%d", 
					zPncInput->interDigitTimeout);
			       	sprintf(Msg,TEL_OVERRIDE_PARM_MSG,yName,
					zSectionName, yTemp, yValue);
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
				zPncInput->interDigitTimeout = atoi(yValue);	
			}
			else
			{
			       sprintf(yTemp, "%d",
					zPncInput->interDigitTimeout);
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG,
					yValue, yName, yTemp );
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
			}
			continue;
		} 

 		if (strcmp(yName,NV_NTRIES) == 0)
		{

			if (validate_range(atoi(yValue), 0, 15) == 0)
			{
				sprintf(yTemp, "%d", zPncInput->nTries);
				sprintf(Msg,TEL_OVERRIDE_PARM_MSG,yName,
					zSectionName, yTemp,yValue);
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
				zPncInput->nTries = atoi(yValue);	
			}
			else
			{
				sprintf(yTemp, "%d", zPncInput->nTries);
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG,
					yValue, yName, yTemp );
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
			}
			continue;
		} 
 		if (strcmp(yName,NV_BEEP) == 0)
		{
			sTranslateFormatToInt(yValue,&yTempInt);
			if (validate_beep(yTempInt) == 0)
			{
				sprintf(Msg, TEL_OVERRIDE_PARM_MSG,
				  	yName, zSectionName, 
					arc_val_tostr(zPncInput->beep), yValue);
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
			   	zPncInput->beep=yTempInt;
			}
			else 
			{
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG,
					yValue, yName, arc_val_tostr(zPncInput->beep));
				telVarLog(zMod, REPORT_DETAIL, TEL_INVALID_OVERRIDE, GV_Warn, Msg);
			}
			continue;
		} 

 		if (strcmp(yName,NV_MIN_LEN) == 0)
		{

			if (validate_range(atoi(yValue), 
					0, zPncInput->maxLen) == 0)
			{
				sprintf(yTemp, "%d", zPncInput->minLen);
				sprintf(Msg,TEL_OVERRIDE_PARM_MSG,yName, zSectionName, yTemp,yValue);
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
				zPncInput->minLen = atoi(yValue);	
			}
			else
			{
				sprintf(yTemp, "%d", zPncInput->minLen);
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG, yValue, yName, yTemp );
				telVarLog(zMod, REPORT_DETAIL, TEL_INVALID_OVERRIDE, GV_Warn, Msg);
			}
			continue;
		} 
 		
		if (strcmp(yName,NV_MAX_LEN) == 0)
		{
			if (validate_range(atoi(yValue), 0, 35) == 0)
			{
				sprintf(yTemp, "%d", zPncInput->maxLen);
			    	sprintf(Msg,TEL_OVERRIDE_PARM_MSG,yName,
					zSectionName,yTemp,yValue);
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
				zPncInput->maxLen = atoi(yValue);	
			}
			else
			{
				sprintf(yTemp, "%d", zPncInput->maxLen);
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG,
					yValue, yName, yTemp );
				telVarLog(zMod, REPORT_DETAIL, TEL_INVALID_OVERRIDE, GV_Warn, Msg);
			}
			continue;
		} 

 		if (strcmp(yName,NV_INPUT_OPTION) == 0)
		{
			sTranslateFormatToInt(yValue,&yTempInt);
			if (validate_inputOption(yTempInt) == 0)
			{
				sprintf(Msg, TEL_OVERRIDE_PARM_MSG,
					yName, zSectionName, 
					arc_val_tostr(zPncInput->inputOption), yValue);
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
				zPncInput->inputOption=yTempInt;
			}
			else 
			{
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG,
					yValue, yName, 
					arc_val_tostr(zPncInput->inputOption));
				telVarLog(zMod, REPORT_NORMAL, TEL_INVALID_OVERRIDE, GV_Warn, Msg);
			}
			continue;
		} 
 		if (strcmp(yName,NV_INTERRUPT_OPTION) == 0)
		{
			sTranslateFormatToInt(yValue,&yTempInt);
			if (validate_interruptOption(yTempInt) == 0)
			{
				sprintf(yTemp, "%d", 
					zPncInput->interruptOption);
				sprintf(Msg, TEL_OVERRIDE_PARM_MSG,
					yName,zSectionName, 
				       arc_val_tostr(zPncInput->interruptOption),
					yValue);
				telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_PARM, GV_Warn, Msg);
				zPncInput->interruptOption=yTempInt;
			}
			else 
			{
				sprintf(Msg, TEL_INVALID_OVERRIDE_MSG,
					yValue, yName, 
				      arc_val_tostr(zPncInput->interruptOption));
				telVarLog(zMod, REPORT_NORMAL, TEL_INVALID_OVERRIDE, GV_Err, Msg);
			}
			continue;
		} 
		sprintf(Msg, TEL_UNKNOWN_OVERRIDE_MSG,yNameValue);
		telVarLog(zMod, REPORT_NORMAL, TEL_UNKNOWN_OVERRIDE_MSG, GV_Err, Msg);
	}
	return(0);
}

/*------------------------------------------------------------------------------
Purpose: Load all of the elements of the zPncInput structure.
Return values: 
	0: always
------------------------------------------------------------------------------*/
static int sPncGetInputBySection(char *zSectionName, PncInput *zPncInput)
{
	PncInput	*pPncInput;

	pPncInput = &GV_PncInput;
	while ( pPncInput->nextSection != (PncInput *)NULL )
	{
		if ( strcmp(zSectionName, pPncInput->sectionName) != 0)
		{
			pPncInput = pPncInput->nextSection;
			continue;
		}
		memcpy(zPncInput, pPncInput, sizeof(PncInput));
		return(0);
	}

	return(-1);
} /* END: sPncGetInputBySection() */

/*------------------------------------------------------------------------------
Purpose: Get DTMF input and perform validation when no hot keys are defined.
Return values: The return codes from TEL_GetDTMF.
------------------------------------------------------------------------------*/
static int sPncGetDTMFNoHotkeys(char *Mod, int zAttempt, PncInput *zPncInput, 
					char *zResult,	char *zData)
{
	int	yRc;
	char	yTmpInput[128];

	memset((char *)yTmpInput, 0, sizeof(yTmpInput));
	GV_DontSendToTelMonitor = 1; /* Suppress monitor display */
#ifdef DEBUG
	GV_DontSendToTelMonitor = 0; /* Use monitor for debugging */
#endif
	yRc = TEL_GetDTMF(zPncInput->party, zPncInput->firstDigitTimeout,
			zPncInput->interDigitTimeout, 1, 
			zPncInput->beep, zPncInput->terminateKey,
			zPncInput->maxLen, zPncInput->terminateOption,
			zPncInput->inputOption, yTmpInput, "");
	switch(yRc)
	{
	case TEL_SUCCESS:
		strcpy(zResult, RESULT_SUCCESS);
		sprintf(zData, "%s", yTmpInput);
		return(TEL_SUCCESS);
		break;
	case TEL_DISCONNECTED:
		strcpy(zResult, RESULT_DISCONNECTED);
		return(yRc);
	case TEL_LOST_AGENT:
		strcpy(zResult, RESULT_LOST_AGENT);
		return(yRc);
		break;
	case TEL_TIMEOUT:
		strcpy(zResult, RESULT_TIMEOUT);
		sprintf(zData, "%s", yTmpInput);
		return(yRc);
		break;	
	case TEL_EXTRA_STAR_IN_DOLLAR:
		strcpy(zResult, RESULT_EXTRA_STAR_IN_DOLLAR);
		sprintf(zData, "%s", yTmpInput);
		return(yRc);
		break;	
	case TEL_FAILURE:
		/* Should only happen if bad parms passed. */
		strcpy(zResult, RESULT_FAILURE);
		return(yRc);
	case TEL_INVALID_INPUT:
		strcpy(zResult, RESULT_INVALID_INPUT);
		return(yRc);
	case TEL_OVERFLOW:
		strcpy(zResult, RESULT_OVERFLOW);
		return(yRc);
	default:
		strcpy(zResult, RESULT_UNKNOWN);
		return(yRc);
	} /* switch */

} /* END: sPncGetDTMFNoHotkeys() */

/*------------------------------------------------------------------------------
Purpose: 	Get DTMF input and perform validation hotkey logic.
		Data is accepted 1 char at a time with AUTO, NUMSTAR.
		Each character is examined.	
Return values: 	return codes from TEL_GetDTMF GPW
------------------------------------------------------------------------------*/
static int sPncGetDTMFWithHotkeys(char *Mod, int zAttempt, PncInput *zPncInput,
			Hotkeys	*zHotkeys, char *zResult, char *zData)
{
	int	i;
	int	yRc;
	char	ySingleChar[2];
	char	yTmpInput[128]; 
	char	yHotkeyPressed[16];
	int	yBeep, yTimeout;
	int	yMaxLength;
	int	yStarCount=0; 	/* Count for * in DOLLAR, so we toss extras */

	/* For mandatory, you only stop when she presses the term key. */
	if (zPncInput->terminateOption == MANDATORY)
		yMaxLength=sizeof(yTmpInput);
	else
		yMaxLength=zPncInput->maxLen;

	memset(yTmpInput, '\0', sizeof(yTmpInput));
	for (i=0; i < yMaxLength; i++)
	{
		memset(ySingleChar, '\0', sizeof(ySingleChar));
		yBeep=NO;
		yTimeout=zPncInput->interDigitTimeout;	
		if (i == 0)
		{
			if (zPncInput->beep == YES) yBeep=YES;
			yTimeout=zPncInput->firstDigitTimeout;	
		}
	
		GV_DontSendToTelMonitor = 1; /* Suppress monitor display */
#ifdef DEBUG
		GV_DontSendToTelMonitor = 0; /* Use monitor for debugging */
#endif
		yRc = TEL_GetDTMF(zPncInput->party, yTimeout, yTimeout, 1, 
			yBeep, 'D', 1, AUTO, 
			NUMSTAR, ySingleChar, "");

		telVarLog(Mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
					"Key received (with hotkeys) (%s).", ySingleChar);
		switch(yRc)
		{
		case TEL_SUCCESS:
			if (ySingleChar[0]=='*')
			{
				if (zPncInput->inputOption==DOLLAR)
				{
					yStarCount++;
					if (yStarCount > 1)
					{
						i--;
						continue;	
					}	
					else
					{
						ySingleChar[0]='.';
					}
				}	
			}
			break;
		case TEL_DISCONNECTED:
			strcpy(zResult, RESULT_DISCONNECTED);
			return(yRc);
			break;
		case TEL_LOST_AGENT:
			strcpy(zResult, RESULT_LOST_AGENT);
			return(yRc);
			break;
		case TEL_TIMEOUT:
			strcpy(zData, yTmpInput);
			strcpy(zResult, RESULT_TIMEOUT);
			return(yRc);
			break;	
		case TEL_FAILURE:
			strcpy(zData, yTmpInput);
			strcpy(zResult, RESULT_FAILURE);
			/* Should only happen if bad parms passed. */
			return(yRc);
		case TEL_INVALID_INPUT:
			strcpy(zData, yTmpInput);
			strcpy(zResult, RESULT_INVALID_INPUT);
			return(yRc);
		default:
			strcpy(zData, yTmpInput);
			strcpy(zResult, RESULT_UNKNOWN);
			return(yRc);
		} /* switch */

		/* We only get here on a success */
		switch(zPncInput->terminateOption)
		{
		case AUTO:
			break;
		case AUTOSKIP:
		case MANDATORY:
			if (ySingleChar[0] == zPncInput->terminateKey)
			{
				sprintf(zData, "%s", yTmpInput);
				if (strlen(yTmpInput) > zPncInput->maxLen)
				{
					strcpy(zResult, RESULT_OVERFLOW);
					return(TEL_OVERFLOW);
				}
				if (yStarCount > 1)
				{
					strcpy(zResult, 
						RESULT_EXTRA_STAR_IN_DOLLAR);
					return(TEL_EXTRA_STAR_IN_DOLLAR);
				}
				else
				{
					strcpy(zResult, RESULT_SUCCESS);
					return(TEL_SUCCESS);
				}
			}
			break;
		}
#ifdef DEBUG
		fprintf(stdout,"About to append (%s) to (%s)\n",
			ySingleChar, yTmpInput);
		fflush(stdout);	
#endif
		/* Append non-terminating key */
		strcat(yTmpInput, ySingleChar);

		memset((char *)yHotkeyPressed,0,sizeof(yHotkeyPressed));
		if (yRc=sPressedHotkey(zHotkeys, yTmpInput, yHotkeyPressed))
		{
			sprintf(zData, "%s", yHotkeyPressed);
			strcpy(zResult, RESULT_HOTKEY);
			return(yRc);
		}
	} /* for */

	sprintf(zData, "%s", yTmpInput);
	if (yStarCount > 1 )
	{
		strcpy(zResult, RESULT_EXTRA_STAR_IN_DOLLAR);
		return(TEL_EXTRA_STAR_IN_DOLLAR);
	}
	else
	{
		strcpy(zResult, RESULT_SUCCESS);
		return(TEL_SUCCESS);
	}

} /* END: sPncGetDTMFWithHotkeys() */

/*--------------------------------------------------------------------------
Purpose:	Get DTMF input and perform validation hotkey logic.

Return values: 
	>0:	True  - this is the numeric designation of the hotkey pressed
	0: 	False - a hotkey was not pressed
--------------------------------------------------------------------------*/
static int sPressedHotkey(Hotkeys *zHotkeys, char *zInput, char *zHotkeyPressed)
{
	int	yCounter;

	for (yCounter = 0; yCounter < zHotkeys->numHotkeys; yCounter++)
	{
		if ( strstr(zInput, zHotkeys->hotkey[yCounter] ) != NULL)
		{
			sprintf(zHotkeyPressed,"%s",zHotkeys->hotkey[yCounter]);
			return(yCounter+1);
		}
	}
	return(0);

} /* END: sPressedHotkey() */

/*------------------------------------------------------------------------------
Purpose: Parse zHotkeyList into the Hotkeys structure:
Return values: 
	0: always
------------------------------------------------------------------------------*/
static int sPncGetHotkeys(char *zHotkeyList, Hotkeys *zHotkeys)
{
	int	yCounter;
	int	yLength;
	char	*pTmpBuf;
	char	yTmpStorage[64];


	if ( zHotkeyList[0] == '\0' )
	{
		zHotkeys->numHotkeys = 0;
		return(0);
	}

	sprintf(yTmpStorage, "%s", zHotkeyList);

    if ((pTmpBuf = (char *)strtok(yTmpStorage, ",")) == (char *)NULL)
    {
        sprintf(zHotkeys->hotkey[0], "%s", zHotkeyList);
	zHotkeys->numHotkeys = 1;
	return(0);
    }

	sprintf(zHotkeys->hotkey[0], "%s", pTmpBuf);
	zHotkeys->numHotkeys = 1;
	while ((pTmpBuf = (char *)strtok(NULL, ",")) != (char *)NULL)
	{
        	sprintf(zHotkeys->hotkey[zHotkeys->numHotkeys], "%s", pTmpBuf);
		zHotkeys->numHotkeys += 1;
	}
	
	return(0);
	
} /* END: sPncGetHotkeys() */

/*------------------------------------------------------------------------------
Play the loaded tags to zFilename.
------------------------------------------------------------------------------*/
int TEL_PlayTag(char *zMod, int zParty, char *zPhraseTag, int zOption, 
				int zMode, char *zDataOverrides)
{
	int	yRc;
	PhraseTagList	*pTmpTag;
	int	yCounter;
	char	yDiagMsg[128];
	int	yTagType;
	char	yInputFormatStr[64];
	char	yOutputFormatStr[64];
	int	yInputFormat;
	int	yOutputFormat;
	char	yTagData[128];

	/* Don't think we need this gpw ?? */
	if ( zOption == INTERRUPT )
	{
		zOption = FIRST_PARTY_INTERRUPT;
	}
	
	if ( ! GV_TagsLoaded )
	{
		sprintf(Msg, TEL_TAGS_NOT_LOADED_MSG);
                telVarLog(zMod, REPORT_NORMAL, TEL_TAGS_NOT_LOADED, GV_Err, Msg);
		return (-1);
	}

	pTmpTag = &GV_AppTags;
	while (pTmpTag->nextTag != (struct phraseTagStruct *)NULL)
	{
		if ( strcmp(pTmpTag->tagName, zPhraseTag) == 0) 
		{
			/* Speak all phrases associated w/ the tag */
			for(yCounter = 0; yCounter < pTmpTag->nTags; yCounter++)
			{
#ifdef DEBUG
		fprintf(stdout,"TEL_PlayTag count=%d, <tag,subtag>=<%s:%s>.\n",
		yCounter, pTmpTag->tagName,pTmpTag->tag[yCounter].tagName); 
		fflush(stdout);
#endif
			/* Determine the data, taking account of overrides */
			sOverrideTagData(zMod, pTmpTag->tag[yCounter].tagName,
				pTmpTag->tag[yCounter].data,  
					zDataOverrides, yTagData);
				GV_DontSendToTelMonitor=1; /* No monitor */
#ifdef DEBUG
				GV_DontSendToTelMonitor = 0; /*  4 Debugging */
#endif
				yRc = TEL_Speak(zParty, zOption,
					pTmpTag->tag[yCounter].inputFormat,
					pTmpTag->tag[yCounter].outputFormat,
				(void *)yTagData, zMode);
				
				if ( (yRc == TEL_DISCONNECTED) || 
					(yRc == TEL_LOST_AGENT) )
				{
					return (yRc);
				}

				if (yRc != TEL_SUCCESS)
				{
					sprintf(Msg, 
					TEL_FAILED_TO_SPEAK_TAG_MSG, zPhraseTag,
				             pTmpTag->tag[yCounter].data,yRc);
                			telVarLog(zMod, REPORT_NORMAL, 
						TEL_FAILED_TO_SPEAK_TAG, 
						GV_Warn, Msg);
				}
				else
				{
					sprintf(Msg, TEL_SPOKE_TAG_MSG, 
						zPhraseTag, 
						pTmpTag->tag[yCounter].data);
                			telVarLog(zMod, REPORT_VERBOSE, 
						TEL_SPOKE_TAG, GV_Warn, Msg);
				}
			}
			return (TEL_SUCCESS);
		}
		pTmpTag = pTmpTag->nextTag;
	}

	sprintf(Msg, TEL_UNABLE_TO_FIND_TAG_MSG);
        telVarLog(zMod, REPORT_NORMAL, TEL_UNABLE_TO_FIND_TAG, GV_Err, Msg);
	return (-1);

} /* END: TEL_PlayTag() */

/*----------------------------------------------------------------------------
This function takes the original data to be spoken and checks it against
the data override string. If tagname.data=override_value is found for the
tag in question, it returns override_value in the last parameter.
If no overrides are specified, it simply copies the data for the tag to 
the last parameter and returns.

zDataOverrides looks like "data_tag1.data=111; data_tag1.data=222";
Parsing of the first item looks like: yName="data_tag1.data" yValue="111"
Then yName is parsed to get: yTag="data_tag1" yElement="data".	
----------------------------------------------------------------------------*/
static int sOverrideTagData(char *zMod, char *zTagName, char *zTagData,
                        char *zDataOverrides, char *zModifiedData)
{
	int i, yRc=0;
	char yNameValue[256];
	char yName[256];
	char yValue[256];
	char yTagName[256];
	char yElement[256];

	if (zDataOverrides[0]=='\0')
	{
		/* No data overrides, just copy over the data from the tag */
		strcpy(zModifiedData, zTagData);
		return(0);
	}

	i=1;
	while ( yRc=sGetField(';', zDataOverrides, i, yNameValue) > 0)
	{ 
		i++;
		
		yRc = sGetField('=', yNameValue, 1, yName);
	
		yRc = sGetField('.', yName, 1, yTagName);
		
		/* We know the tag name to be overriden now. 
		   Is it the one we are about to speak? */	
		if (strcmp(yTagName,zTagName)!=0) continue;
		
		yRc = sGetField('=', yNameValue, 2, yValue);

		yRc = sGetField('.', yName, 2, yElement);

		if (strcmp(yTagName,zTagName)==0 && 
				strcmp(yElement,"data") == 0)
		{
			strcpy(zModifiedData, yValue);
			sprintf(Msg,TEL_OVERRIDE_TAG_MSG, 
				yTagName, yElement, zTagData, zModifiedData);
                	telVarLog(zMod, REPORT_DETAIL, TEL_OVERRIDE_TAG, 
				GV_Warn, Msg);
			return(0);
		}
		else
		{
			sprintf(Msg, TEL_UNKNOWN_OVERRIDE_MSG, yNameValue);
                	telVarLog(zMod, REPORT_NORMAL, TEL_UNKNOWN_OVERRIDE, 
				GV_Warn, Msg);
		}
	}

	/* There were overrides specified, but not for the tag in question. */
        strcpy(zModifiedData, zTagData);
        return(0);
}


/*------------------------------------------------------------------------------
sRemoveWhitespace():
 	removes all whitespace characters from string
------------------------------------------------------------------------------*/
static int sRemoveWhitespace(char *zBuf)
{
	int	yLength;
	char	*yTmpString;
	char	*yPtr2;
	char	*yPtr;

	if((yLength = strlen(zBuf)) == 0)
	{
		return(0);
	}
		
	yTmpString = (char *)calloc((size_t)(yLength + 1), sizeof(char));
	yPtr2 = yTmpString;
	yPtr = zBuf;

	while ( *yPtr != '\0' )
	{
		if ( isspace( *yPtr ) )
		{
			yPtr++;
		}
		else
		{
			*yPtr2 = *yPtr; 
			yPtr2++;
			yPtr++;
		}
	} /* while */

	sprintf(zBuf, "%s", yTmpString);
	free(yTmpString);

	return(0);
} /* END: sRemoveWhitespace() */

/*------------------------------------------------------------------------------
sNumCharInString():
	Sets zNumInstances for the number of times the zChar character appears in 
	zString.
------------------------------------------------------------------------------*/
static int sNumCharInString(char *zString, char zChar, int *zNumInstances)
{
	int	yCounter;
	int	yStringLen;

	yStringLen = strlen(zString);
	*zNumInstances = 0;

	for (yCounter = 0; yCounter < yStringLen; yCounter++)
	{
		if ( zChar == zString[yCounter] )
		{
			*zNumInstances += 1;
		}
	}

	return(0);
} /* END: sNumCharInString() */

/*------------------------------------------------------------------------------
Search through the loaded tag list to find the phrase file, and return
the tag associated with it.
	
	Return Values:
		0:		Success
		-1:		No tagname found.
------------------------------------------------------------------------------*/
static int sGetTagFromFile(char *zFilename, char *zPhraseTag)
{
	PhraseTagList	*yTmpTag;

	yTmpTag = &GV_AppTags;
	while ( yTmpTag->nextTag != (PhraseTagList *)NULL )
	{
		if ( yTmpTag->nTags > 1 )
		{
			yTmpTag = yTmpTag->nextTag;
			continue;
		}
			
		if (strcmp(zFilename, yTmpTag->tag[0].data) == 0)
		{
			sprintf(zPhraseTag, "%s", yTmpTag->tagName);
			return(0);
		}
		yTmpTag = yTmpTag->nextTag;
	}
	return(-1);

} /* END: sGetTagFromFile() */

/*------------------------------------------------------------------------------
Purpose: Determine if the data string has only valid characters.
	For the DOLLAR option, a "." is allowed and will not necessarily be
	found in the list of good characters specified in the section,
	so it must be added.
Return values: 
	0: Data is not valid.
	1: Data is valid.
------------------------------------------------------------------------------*/
static int validateInput(char *zData, int zInputOption, char *zAllowedChars)
{
	int i;
	char yGoodChars[64];

	strcpy(yGoodChars,zAllowedChars);
	if (zInputOption == DOLLAR)
	{
		strcat(yGoodChars,".");	
	}
	for (i = 0; i < strlen(zData); i++)
	{
		if(strchr(yGoodChars,zData[i])==NULL) 
		{
			/* This should be a verbose message */
	/*		fprintf(stdout,"Can't find <%c> in (%s)\n",
			zData[i], zAllowedChars); fflush(stdout); */
			return(0);
		}
	}
	return(1);
}
