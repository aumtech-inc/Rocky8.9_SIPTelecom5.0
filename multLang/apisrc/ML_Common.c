/*------------------------------------------------------------------------------ File:		MLCommon.c
Purpose:	Common routines for multiple language processing.
Author:		Aumtech, Inc.
Update:	djb 01/25/2002	Created the file.
Update:	djb 06/10/2005	Modified mlSpeak() to not speak the zero when
                        speaking 2000, and remove the 'and' when speaking 2001.
Update: djb 06/28/2005  In the mlPlayMainCurrencyUnit() routine,
                        play mainCurrencyUnit1 only if balance == 1; otherwise,
                        play mainCurrencyUnit2.  Same for minorCurrencyUnit1
                        and 2 in mlPlayMinorCurrencyUnit().
Update: djb 03/08/2007  Modified mlSpeak(); 1000 was "un mil", now it's "mil".
						Removed the "un" from 1000-1999 numbers.
Update: djb 03/08/2007  Removed DJBDEBUG statements.
------------------------------------------------------------------------------*/
#define	ML_COMMON	1
#include "headers.h"

static int	gIspBaseLoaded		= 0;
static char	gSavedAppPhraseDir[256];

/* Special cash number binary patterns */
/* H   = hundred
   TH  = thousand
   HTH = hundred thousand
   M   = million
   HM  = hundred million */

#define	H				"00001"
#define TH       			"00010"
#define TH_H       			"00011"
#define HTH				"00100"
#define HTH_H				"00101"
#define HTH_TH				"00110"
#define HTH_TH_H			"00111"
#define MM				"01000"
#define M_H				"01001"
#define M_TH				"01010"
#define M_TH_H				"01011"
#define M_HTH				"01100"
#define M_HTH_H				"01101"
#define M_HTH_TH			"01110"
#define M_HTH_TH_H			"01111"
#define HM				"10000"
#define HM_H				"10001"
#define HM_TH				"10010"
#define HM_TH_H				"10011"
#define HM_HTH				"10100"
#define HM_HTH_H			"10101"
#define HM_HTH_TH			"10110"
#define HM_HTH_TH_H			"10111"
#define HM_M				"11000"
#define HM_M_H				"11001"
#define HM_M_TH				"11010"
#define HM_M_TH_H			"11011"
#define HM_M_HTH			"11100"
#define HM_M_HTH_H			"11101"
#define HM_M_HTH_TH			"11110"
#define HM_M_HTH_TH_H			"11111"

#define	THOUSANDS_SW			1
#define	MILLIONS_SW				2
#define	HUNDREDS_SW				3

static int speakSpanishHundreds(int whatToSpeak, int mainBalance,
					char *hundreds, char *tens, char *units,
					int zParty, int zInterruptOption, int zSync);

/*------------------------------------------------------------------------------
mlPlayMainCurrencyUnit(): Play the main currency unit
------------------------------------------------------------------------------*/
int mlPlayMainCurrencyUnit(int zLanguage, long balance, 
			int zParty, int zInterruptOption, int zSync)
{
	static char	yMod[]="mlPlayMainCurrencyUnit";
	char		mainCurrencyUnitPhrase[ARRAY_SIZE];
	int			yRc;

	memset(mainCurrencyUnitPhrase, '\0', sizeof(mainCurrencyUnitPhrase));
	
	if ( balance == 1 )
	{
		sprintf(mainCurrencyUnitPhrase, "%s", "mainCurrencyUnit1");
	}
	else
	{
		sprintf(mainCurrencyUnitPhrase, "%s", "mainCurrencyUnit2");
	}

	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"language:%d   balance:%ld", zLanguage, balance);
	switch (zLanguage)
	{
		case ML_FRENCH:
		case ML_SPANISH:
		default:
			if (balance >= 2)
			{
				sprintf(mainCurrencyUnitPhrase, "%s", "mainCurrencyUnit2");
			}
			break;
	}

	(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Attempting to play tag (%s).", mainCurrencyUnitPhrase);
	yRc = TEL_Speak(zParty, zInterruptOption,
				PHRASE_TAG, PHRASE_FILE, mainCurrencyUnitPhrase, zSync);
	(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"%d = TEL_Speak(%s)", yRc, mainCurrencyUnitPhrase);
	return(yRc);

} /* END: mlPlayMainCurrencyUnit() */

/*------------------------------------------------------------------------------
mlPlayMinorCurrencyUnit(): Play the minor currency unit
------------------------------------------------------------------------------*/
int mlPlayMinorCurrencyUnit(int zLanguage, long balance,
			int zParty, int zInterruptOption, int zSync)
{
	static char	yMod[] = "mlPlayMinorCurrencyUnit";
	char		minorCurrencyUnitPhrase[ARRAY_SIZE];
	int			yRc;

	memset(minorCurrencyUnitPhrase,'\0',sizeof(minorCurrencyUnitPhrase));
	if ( balance == 1 )
	{
		sprintf(minorCurrencyUnitPhrase, "%s", "minorCurrencyUnit1");
	}
	else
	{
		sprintf(minorCurrencyUnitPhrase, "%s", "minorCurrencyUnit2");
	}




	switch (zLanguage)
	{
		case ML_FRENCH:
		case ML_SPANISH:
		default:
		if (balance > 1)
		{
			sprintf(minorCurrencyUnitPhrase, "%s", "minorCurrencyUnit2");
		}
		break;
	}

	(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Attempting to play tag (%s)", minorCurrencyUnitPhrase);

	yRc = TEL_Speak(zParty, zInterruptOption,
			PHRASE_TAG, PHRASE_FILE, minorCurrencyUnitPhrase, zSync);

	(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"%d = TEL_Speak(%s)", yRc, minorCurrencyUnitPhrase);
	return(yRc);

} /* END: mlPlayMinorCurrencyUnit() */

/*------------------------------------------------------------------------------
mlCheckSpecialNumber(): See if we have one of those special numbers
						e.g. 100,1000
------------------------------------------------------------------------------*/
int mlCheckSpecialNumber(char *hundredMillions, 
	char *tenMillions, 
	char *millions,
	char *hundredThousands,
	char *tenThousands,
	char *thousands,
	char *hundreds,
	char *tens,
	char *units,
	char *tenths,
	char *hundredths,
	int language)
{
	static char	yMod[] = "mlCheckSpeckNumber";
	char		binaryPattern[6];
	int			yRc;

	/* 
	      Special numbers are those consisting of :-
	
	      100 million (HM)
	      1 million (M)
	      100 thousand (HTH)
	      1 thousand (TH)
	      1 hundred (H)
	
	      So we build a binary pattern of these components and check against the 
	      binary patterns that determine that the number is a special type 
	
	    */

	switch (language)
	{
		case ML_FRENCH:
			break;
		case ML_SPANISH:
			return(SPANISH_NUMBER);
			break;
		default:
			return(PORTUGUESE_NUMBER);
			break;
	}

	memset(binaryPattern,'\0',sizeof(binaryPattern));
	strcpy(binaryPattern,hundredMillions);
	strcat(binaryPattern,millions);
	strcat(binaryPattern,hundredThousands);
	strcat(binaryPattern,thousands);
	strcat(binaryPattern,hundreds);

	if (!strcmp(binaryPattern,H))
		return(SPECIAL_NUMBER_1);
	else if (!strcmp(binaryPattern,TH))
		return(SPECIAL_NUMBER_2);
	else if (!strcmp(binaryPattern,TH_H))
		return(SPECIAL_NUMBER_3);
	else if (!strcmp(binaryPattern,HTH))
		return(SPECIAL_NUMBER_4);
	else if (!strcmp(binaryPattern,HTH_H))
		return(SPECIAL_NUMBER_5);
	else if (!strcmp(binaryPattern,HTH_TH))
		return(SPECIAL_NUMBER_6);
	else if (!strcmp(binaryPattern,HTH_TH_H))
		return(SPECIAL_NUMBER_7);
	else if (!strcmp(binaryPattern,MM))
		return(SPECIAL_NUMBER_8);
	else if (!strcmp(binaryPattern,M_H))
		return(SPECIAL_NUMBER_9);
	else if (!strcmp(binaryPattern,M_TH))
		return(SPECIAL_NUMBER_10);
	else if (!strcmp(binaryPattern,M_TH_H))
		return(SPECIAL_NUMBER_11);
	else if (!strcmp(binaryPattern,M_HTH))
		return(SPECIAL_NUMBER_12);
	else if (!strcmp(binaryPattern,M_HTH_H))
		return(SPECIAL_NUMBER_13);
	else if (!strcmp(binaryPattern,M_HTH_TH))
		return(SPECIAL_NUMBER_14);
	else if (!strcmp(binaryPattern,M_HTH_TH_H))
		return(SPECIAL_NUMBER_15);
	else if (!strcmp(binaryPattern,HM))
		return(SPECIAL_NUMBER_16);
	else if (!strcmp(binaryPattern,HM_H))
		return(SPECIAL_NUMBER_17);
	else if (!strcmp(binaryPattern,HM_TH))
		return(SPECIAL_NUMBER_18);
	else if (!strcmp(binaryPattern,HM_TH_H))
		return(SPECIAL_NUMBER_19);
	else if (!strcmp(binaryPattern,HM_HTH))
		return(SPECIAL_NUMBER_20);
	else if (!strcmp(binaryPattern,HM_HTH_H))
		return(SPECIAL_NUMBER_21);
	else if (!strcmp(binaryPattern,HM_HTH_TH))
		return(SPECIAL_NUMBER_22);
	else if (!strcmp(binaryPattern,HM_HTH_TH_H))
		return(SPECIAL_NUMBER_23);
	else if (!strcmp(binaryPattern,HM_M))
		return(SPECIAL_NUMBER_24);
	else if (!strcmp(binaryPattern,HM_M_H))
		return(SPECIAL_NUMBER_25);
	else if (!strcmp(binaryPattern,HM_M_TH))
		return(SPECIAL_NUMBER_26);
	else if (!strcmp(binaryPattern,HM_M_TH_H))
		return(SPECIAL_NUMBER_27);
	else if (!strcmp(binaryPattern,HM_M_HTH))
		return(SPECIAL_NUMBER_28);
	else if (!strcmp(binaryPattern,HM_M_HTH_H))
		return(SPECIAL_NUMBER_29);
	else if (!strcmp(binaryPattern,HM_M_HTH_TH))
		return(SPECIAL_NUMBER_30);
	else if (!strcmp(binaryPattern,HM_M_HTH_TH_H))
		return(SPECIAL_NUMBER_31);
	else
		return(ORDINARY_NUMBER);

} /* END: mlCheckSpecialNumber() */


/*------------------------------------------------------------------------------
mlParseBalance(): Get the constituent parts of the balance
------------------------------------------------------------------------------*/
void mlParseBalance(char *balance,
	char *hundredMillions, 
	char *tenMillions, 
	char *millions,
	char *hundredThousands,
	char *tenThousands,
	char *thousands,
	char *hundreds,
	char *tens,
	char *units,
	char *tenths,
	char *hundredths)
{
	static char	yMod[]="mlParseBalance";
	int			balanceLen;
	int			yRc;

	memset(hundredMillions,'\0',sizeof(hundredMillions));
	memset(tenMillions,'\0',sizeof(tenMillions));
	memset(millions,'\0',sizeof(millions));
	memset(hundredThousands,'\0',sizeof(hundredThousands));
	memset(tenThousands,'\0',sizeof(tenThousands));
	memset(thousands,'\0',sizeof(thousands));
	memset(hundreds,'\0',sizeof(hundreds));
	memset(tens,'\0',sizeof(tens));
	memset(units,'\0',sizeof(units));
	memset(tenths,'\0',sizeof(tenths));
	memset(hundredths,'\0',sizeof(hundredths));
	sprintf(hundredMillions,"%c",'0');
	sprintf(tenMillions,"%c",'0');
	sprintf(millions,"%c",'0');
	sprintf(hundredThousands,"%c",'0');
	sprintf(tenThousands,"%c",'0');
	sprintf(thousands,"%c",'0');
	sprintf(hundreds,"%c",'0');
	sprintf(tens,"%c",'0');
	sprintf(units,"%c",'0');
	sprintf(tenths,"%c",'0');
	sprintf(hundredths,"%c",'0');

	balanceLen=strlen(balance);
	if (balance[balanceLen-3] == '.')
	{
		if (strlen(balance) > 11)
			sprintf(hundredMillions,"%c",balance[balanceLen-12]);
		if (strlen(balance) > 10)
			sprintf(tenMillions,"%c",balance[balanceLen-11]);
		if (strlen(balance) > 9)
			sprintf(millions,"%c",balance[balanceLen-10]);
		if (strlen(balance) > 8)
			sprintf(hundredThousands,"%c",balance[balanceLen-9]);
		if (strlen(balance) > 7)
			sprintf(tenThousands,"%c",balance[balanceLen-8]);
		if (strlen(balance) > 6)
			sprintf(thousands,"%c",balance[balanceLen-7]);
		if (strlen(balance) > 5)
			sprintf(hundreds,"%c",balance[balanceLen-6]);
		if (strlen(balance) > 4)
			sprintf(tens,"%c",balance[balanceLen-5]);
		sprintf(units,"%c",balance[balanceLen-4]);
		sprintf(tenths,"%c",balance[balanceLen-2]);
		sprintf(hundredths,"%c",balance[balanceLen-1]);
	}
	else if (balance[balanceLen-2] == '.')
	{
		if (strlen(balance) > 10)
			sprintf(hundredMillions,"%c",balance[balanceLen-11]);
		if (strlen(balance) > 9)
			sprintf(tenMillions,"%c",balance[balanceLen-10]);
		if (strlen(balance) > 8)
			sprintf(millions,"%c",balance[balanceLen-9]);
		if (strlen(balance) > 7)
			sprintf(hundredThousands,"%c",balance[balanceLen-8]);
		if (strlen(balance) > 6)
			sprintf(tenThousands,"%c",balance[balanceLen-7]);
		if (strlen(balance) > 5)
			sprintf(thousands,"%c",balance[balanceLen-6]);
		if (strlen(balance) > 4)
			sprintf(hundreds,"%c",balance[balanceLen-5]);
		if (strlen(balance) > 3)
			sprintf(tens,"%c",balance[balanceLen-4]);
		sprintf(units,"%c",balance[balanceLen-3]);
		sprintf(tenths,"%c",balance[balanceLen-1]);
		sprintf(hundredths,"%c",'0');
	}
	else
	{
		if (strlen(balance) > 8)
			sprintf(hundredMillions,"%c",balance[balanceLen-9]);
		if (strlen(balance) > 7)
			sprintf(tenMillions,"%c",balance[balanceLen-8]);
		if (strlen(balance) > 6)
			sprintf(millions,"%c",balance[balanceLen-7]);
		if (strlen(balance) > 5)
			sprintf(hundredThousands,"%c",balance[balanceLen-6]);
		if (strlen(balance) > 4)
			sprintf(tenThousands,"%c",balance[balanceLen-5]);
		if (strlen(balance) > 3)
			sprintf(thousands,"%c",balance[balanceLen-4]);
		if (strlen(balance) > 2)
			sprintf(hundreds,"%c",balance[balanceLen-3]);
		if (strlen(balance) > 1)
			sprintf(tens,"%c",balance[balanceLen-2]);
		sprintf(units,"%c",balance[balanceLen-1]);
		sprintf(tenths,"%c",'0');
		sprintf(hundredths,"%c",'0');
	}

	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"hundredMillions (%s)",hundredMillions);
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"millions (%s) tenMillions (%s)",millions,tenMillions);
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"tenThousands(%s) Hundredthousands (%s)",
	   			tenThousands,hundredThousands);
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"hundreds (%s) thousands (%s)",hundreds,thousands);
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"tens (%s) units (%s)",tens,units);
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"tenths (%s) hundredths (%s)",tenths,hundredths);
} /* END: mlParseBalance() */

/*------------------------------------------------------------------------------
mlLoadFeminineVoice(): Load specified feminine system prompts
------------------------------------------------------------------------------*/
int mlLoadFeminineVoice(char *sysVoxStr)
{
	static char	yMod[]="mlLoadFeminineVoice";
	char 		languageIntStr[7];
	char 		tmpSysVoxSelection[7];
	int			newLang;
	int 		yRc = 0;
	int 		i=0;

	memset(languageIntStr,		'\0',	sizeof(languageIntStr));
	memset(tmpSysVoxSelection,	'\0',	sizeof(tmpSysVoxSelection));

	for (i=4; i<=(int)strlen(sysVoxStr)-1; i++)
	{
		languageIntStr[i-4]=sysVoxStr[i];
	}

	sprintf(tmpSysVoxSelection, "%d%s", ML_FEMALE, languageIntStr);
	newLang = atoi(languageIntStr);

	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Based on SysVox (%s)",sysVoxStr);
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"New Tmp SysVox <LANG%d>", newLang);

	if ((yRc = TEL_SetGlobal("$LANGUAGE", newLang)) != TEL_SUCCESS)
	{
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
			GV_Application_Name, ML_LOAD_ERROR, 
			"Failed to load (%d:LANG%d)", newLang, newLang);
		return(yRc);
	}
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
			GV_Application_Name, ML_VERBOSE, 
			"%d = TEL_SetGlobal($LANGUAGE, %d)", yRc, newLang);

	return(0);
} /* END: mlLoadFeminineVoice() */

/*------------------------------------------------------------------------------
mlGetIspBase():
------------------------------------------------------------------------------*/
int mlGetIspBase(char *zIspBase)
{
	char	defaultConfigFileName[128];
	char	str[80];
	FILE	*fp;
	char	*ptr;
	char	*pIspBase;

	zIspBase[0] = '\0';
	if((pIspBase = (char *)getenv("ISPBASE")) != NULL)
	{
		sprintf(zIspBase, "%s", pIspBase);
		return(0);
	}

	sprintf(defaultConfigFileName, "%s", "/arc/arcEnv");

	if((fp=fopen(defaultConfigFileName, "r")) != NULL)
	{
		while(fgets(str, 80, fp) != NULL)
		{
			switch(str[0])
			{
				case '[':
				case ' ':
				case '\n':
				case '#':
					continue;
					break;
				default:
					break;
			}

			if(strncmp(str, "ISPBASE", strlen("ISPBASE")) == 0)
			{
	 			if((ptr = strchr(str, '=')) != NULL)
 				{
					ptr++;
					sprintf(zIspBase, "%s", ptr);
				}
				fclose(fp);
				return(0);
			}
		}
		fclose(fp);
	}

	return(-1);
} /* END: mlGetIspBase() */

/*------------------------------------------------------------------------------
mlLoadLanguage():
------------------------------------------------------------------------------*/
int mlLoadLanguage(int zLanguageId, int zGender)
{
	static char		yMod[]="mlLoadLanguage";
	int				yRc;
	char			yBuf[16];
	int				yLanguage;
	char			yPhraseDir[128];
	char			yPhraseFile[256];
	char			*pIspBase;
	static char		yIspBase[128];

	if (! gIspBaseLoaded )
	{
		if ((yRc = mlGetIspBase(yIspBase)) != 0)
		{
			(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_SYSTEM_ERROR, 
				"ERROR: Environment variable ISPBASE is not set.");
			return(-1);
		}
		gIspBaseLoaded = 1;
	}

	if (zGender == ML_FEMALE)
	{
		sprintf(yBuf, "1%d", zLanguageId);
	}
	else
	{
		sprintf(yBuf, "%d", zLanguageId);
	}
	yLanguage = atoi(yBuf);
	
	if ((yRc = TEL_SetGlobal("$LANGUAGE", yLanguage)) != TEL_SUCCESS)
	{
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
					GV_Application_Name, ML_LOAD_ERROR, 
					"Unable to set language to (%d).", yLanguage);
		return(yRc);
	}
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Successfully set language to (%d).", yLanguage);

	sprintf(yPhraseDir, "%s/Telecom/phrases/appPhrases%d",
				yIspBase, zLanguageId);

	gSavedAppPhraseDir[0] = '\0';
	if ((yRc = TEL_GetGlobalString("$APP_PHRASE_DIR", gSavedAppPhraseDir))
												!= TEL_SUCCESS)
	{
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
					GV_Application_Name, ML_LOAD_ERROR, 
					"Unable to retrieve APP_PHRASE_DIR.  yRc=%d  ", yRc);
		return(yRc);
	}

	if ( yPhraseDir[0] == '\0' )
	{
		yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Attempted to retrieve APP_PHRASE_DIR to save; however, "
				"APP_PHRASE_DIR is not set.");
	}
	else
	{
		yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Retrieved APP_PHRASE_DIR (%s) to save.",
				gSavedAppPhraseDir);
	}

	if ((yRc = TEL_SetGlobalString("$APP_PHRASE_DIR", yPhraseDir))
												!= TEL_SUCCESS)
	{
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
					GV_Application_Name, ML_LOAD_ERROR, 
					"Unable to set APP_PHRASE_DIR to (%s).  ", yPhraseDir);
		return(yRc);
	}

	sprintf(yPhraseFile, "%s/appPhrases%d.phr", yPhraseDir, zLanguageId);

    if ((yRc = TEL_LoadTags(yPhraseFile)) != TEL_SUCCESS)
	{
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
					GV_Application_Name, ML_LOAD_ERROR, 
        			"TEL_LoadTags(%s) failed.  yRc = %d", yPhraseFile, yRc);
		return(yRc);
    }
	(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
    			"%d = TEL_LoadTags(%s)", yRc, yPhraseFile);

	switch( zLanguageId )
	{
		case ML_FRENCH:
				gFrenchLoaded = 1;
				gpLoadedLanguage = &gFrenchLoaded;
				gSpanishLoaded = 0;
				break;
		case ML_SPANISH:
				gFrenchLoaded = 0;
				gSpanishLoaded = 1;
				gpLoadedLanguage = &gSpanishLoaded;
				break;
	}

	return(0);
} /* END: mlLoadLanguage() */

/*------------------------------------------------------------------------------
mlRestoreLangTag():
------------------------------------------------------------------------------*/
int mlRestoreLangTag(int zReloadLanguage, char *zReloadTagFile )
{
	static char		yMod[] = "mlRestoreLangTag";
	int				yRc;

	if ( gSavedAppPhraseDir[0] != '\0' )
	{
		if ((yRc = TEL_SetGlobalString("$APP_PHRASE_DIR", gSavedAppPhraseDir))
													!= TEL_SUCCESS)
		{
			(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
					GV_Application_Name, ML_LOAD_ERROR, 
					"Unable to set APP_PHRASE_DIR to (%s). yRc=%d.",
					gSavedAppPhraseDir, yRc);
			return(yRc);
		}
		yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Successfully reset APP_PHRASE_DIR to (%s).",
				gSavedAppPhraseDir);
	}

	if (zReloadLanguage != 0)
	{
		if ((yRc = TEL_SetGlobal("$LANGUAGE", zReloadLanguage)) != TEL_SUCCESS)
		{
			(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_LOAD_ERROR, 
				"Failed to reload (%d)", zReloadLanguage);
			return(yRc);
		}
		yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"%d = TEL_SetGlobal($LANGUAGE, %d)", yRc, zReloadLanguage);
		*gpLoadedLanguage = 0;
	}

	if (zReloadTagFile[0] != '\0')
	{
    	if ((yRc = TEL_LoadTags(zReloadTagFile)) != TEL_SUCCESS)
		{
			(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
					GV_Application_Name, ML_LOAD_ERROR, 
        			"TEL_LoadTags(%s) failed.  yRc = %d", zReloadTagFile, yRc);
			return(yRc);
	    }
		(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
    			"%d = TEL_LoadTags(%s)", yRc, zReloadTagFile);
	}
	return(TEL_SUCCESS);
} /* END: mlRestoreLangTag() */

/*------------------------------------------------------------------------------
mlSpeak():
------------------------------------------------------------------------------*/
int mlSpeak(int zLanguage, int zType, int zParty, int zInterruptOption,
				char *zData, int zSync)
{
	static char	yMod[] = "mlSpeak";
	int			*pLoadedLanguage;
	int			yRc=0;
	int			rc=0;
	long		mainBalance;
	char		minorBalance[3];
	char		hundredMillions[2],tenMillions[2],millions[2];
	char		hundredThousands[2],tenThousands[2],thousands[2];
	char		hundreds[2],tens[2],units[2];
	char		tenths[2],hundredths[2];
	static int	minutes;
	static int	seconds;
	char		minuteStr[8];
	char		secondStr[8];
	char		tmpSysVoxSelection[ARRAY_SIZE];
	char		balance[64];
	char		yTagName[64];

	char		hundredsOnly[8];
	char		thousandsOnly[8];
	char		millionsOnly[8];
	int			yMinorBalance;

	memset(hundredsOnly,        '\0',   sizeof(hundredsOnly));
	memset(thousandsOnly,        '\0',   sizeof(thousandsOnly));

	memset(minorBalance,		'\0',	sizeof(minorBalance));
	memset(hundredMillions,		'\0',	sizeof(hundredMillions));
	memset(tenMillions,			'\0',	sizeof(tenMillions));
	memset(millions,			'\0',	sizeof(millions));
	memset(hundredThousands,	'\0',	sizeof(hundredThousands));
	memset(tenThousands,		'\0',	sizeof(tenThousands));
	memset(thousands,			'\0',	sizeof(thousands));
	memset(hundreds,			'\0',	sizeof(hundreds));
	memset(tens,				'\0',	sizeof(tens));
	memset(units,				'\0',	sizeof(units));
	memset(tenths,				'\0',	sizeof(tenths));
	memset(hundredths,			'\0',	sizeof(hundredths));
	memset(minuteStr,			'\0',	sizeof(minuteStr));
	memset(secondStr,			'\0',	sizeof(secondStr));
	memset(tmpSysVoxSelection,	'\0',	sizeof(tmpSysVoxSelection));

	mlParseBalance(zData, hundredMillions, tenMillions, millions,
			hundredThousands, tenThousands, thousands, hundreds, tens,
			units,tenths,hundredths);
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
		GV_Application_Name, ML_VERBOSE, 
		"mlParseBalance(%s, %s, %s, %s, %s, %s, "
		"%s, %s, %s, %s, %s, %s)",
		zData, hundredMillions, tenMillions,millions,hundredThousands,
	    tenThousands,thousands,hundreds,tens,units,tenths,hundredths);

	mainBalance = atol(zData);
	sprintf(minorBalance, "%s",tenths);
	strcat(minorBalance, hundredths);
	yMinorBalance = atoi(minorBalance);

	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
		GV_Application_Name, ML_VERBOSE, 
		"Dollars (%ld); Cents (%s)", mainBalance, minorBalance);

	rc = mlCheckSpecialNumber(hundredMillions, tenMillions, millions,
		hundredThousands, tenThousands, thousands, hundreds, tens, units,
		tenths, hundredths, zLanguage);

	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
		GV_Application_Name, ML_VERBOSE, "%d = mlCheckSpecialNumber(%s, %s, "
		"%s, %s, %s, %s, %s, %s, %s, %s, %s, %d)",
		rc, hundredMillions, tenMillions, millions, hundredThousands,
	    tenThousands, thousands, hundreds, tens, units, tenths, hundredths,
		zLanguage);


	switch (rc)
	{
	case SPECIAL_NUMBER_1:
		mainBalance = mainBalance - 100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Successfully spoke phrase tag (%s).", yTagName);
		break;

	case SPECIAL_NUMBER_2:
		mainBalance=mainBalance-1000;
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_3:
		mainBalance=mainBalance-1100;
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_4:
		mainBalance=mainBalance-100000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_5:
		mainBalance=mainBalance-100100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_6:
		mainBalance=mainBalance-101000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_7:
		mainBalance=mainBalance-101100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_8:
		mainBalance=mainBalance-1000000;
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_9:
		mainBalance=mainBalance-1000100;
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_10:
		mainBalance=mainBalance-1001000;
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_11:
		mainBalance=mainBalance-1001100;
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_12:
		mainBalance=mainBalance-1100000;
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_13:
		mainBalance=mainBalance-1100100;
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_14:
		mainBalance=mainBalance-1101000;
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_15:
		mainBalance=mainBalance-1101100;
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_16:
		mainBalance=mainBalance-100000000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_17:
		mainBalance=mainBalance-100000100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_18:
		mainBalance=mainBalance-100001000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_19:
		mainBalance=mainBalance-100001100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_20:
		mainBalance=mainBalance-100100000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_21:
		mainBalance=mainBalance-100100100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_22:
		mainBalance=mainBalance-100101000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_23:
		mainBalance=mainBalance-100101100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_24:
		mainBalance=mainBalance-101000000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_25:
		mainBalance=mainBalance-101000100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_26:
		mainBalance=mainBalance-101001000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_27:
		mainBalance=mainBalance-101001100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_28:
		mainBalance=mainBalance-101100000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_29:
		mainBalance=mainBalance-101100100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_30:
		mainBalance=mainBalance-101101000;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPECIAL_NUMBER_31:
		mainBalance=mainBalance-101101100;
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "million");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "and");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "one");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "thousand");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		sprintf(yTagName, "%s", "hundred");
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
		break;

	case SPANISH_NUMBER:
		if  ( mainBalance == 0 )
		{
			if ((yRc = TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTRPT, STRING, 
					DIGIT, "0", zSync)) != TEL_SUCCESS)
			{
				return(yRc);
			}
		}
		if (mainBalance >= 1000000)
		{
			yRc = speakSpanishHundreds(MILLIONS_SW, mainBalance,
					hundredMillions, tenMillions, millions,
					zParty, zInterruptOption, zSync);
			sprintf(yTagName, "%s", "million");
			if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
						PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
			{
				return(yRc);
			}
			sprintf(thousandsOnly, "%s%s%s%s%s%s",
				hundredThousands, tenThousands,thousands,hundreds,tens,units);
			mainBalance = atoi(thousandsOnly);
	
			if ( ( ( mainBalance > 0 ) || (yMinorBalance > 0 ) ) &&
			     ( zType != ML_CURRENCY ) )
			{
				sprintf(yTagName, "%s", "and");
				if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
				{
					return(yRc);
				}
			}
		}

        if (mainBalance >= 1000)
        {
			if ( mainBalance >= 2000 )
			{
            	yRc = speakSpanishHundreds(THOUSANDS_SW, mainBalance,
                    hundredThousands, tenThousands, thousands,
					zParty, zInterruptOption, zSync);
			}
            sprintf(yTagName, "%s", "thousand");
            if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
                        PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
            {
                return(yRc);
            }
            sprintf(hundredsOnly, "%s%s%s", hundreds,tens,units);
            mainBalance = atoi(hundredsOnly);

#if 0 		// djb - Mario says it should not speak 'and' here
            if ( ( mainBalance >= 0 ) || (minorBalance[0] != '\0' ) )
            {
                sprintf(yTagName, "%s", "and");
                if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
                                PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
                {
                    return(yRc);
                }
            }
#endif
        }

        if (mainBalance > 0)
        {
			yRc = speakSpanishHundreds(HUNDREDS_SW, mainBalance,
                    hundreds, tens, units, zParty, zInterruptOption, zSync);
			mainBalance = 0;
		}

		break;
	case ORDINARY_NUMBER:
	case EXTRAORDINARY_NUMBER:
	default:
		break;

	} /* End switch special number type */

	if ( rc != SPANISH_NUMBER )
	{
		if (mainBalance >= 1)
		{
			/* Speak remainder */
			sprintf(balance, "%ld", mainBalance);
			if ((yRc = TEL_Speak(zParty, zInterruptOption, STRING, NUMBER,	
						balance, zSync)) != TEL_SUCCESS)
			{
				yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
						GV_Application_Name, ML_VERBOSE, 
						"Failed to speak balance (%s).", balance);
				return(yRc);
			}
			yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
						GV_Application_Name, ML_VERBOSE, 
						"Successfully spoke balance (%s).", balance);
		}
	}

	if (zType == ML_CURRENCY)
	{
//		if (atol(zData) > 0)
//		{
			if ((yRc = mlPlayMainCurrencyUnit(zLanguage, mainBalance, zParty,
						zInterruptOption, zSync)) != TEL_SUCCESS)
			{
				return(yRc);
			}
//		}
		yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, 
				"Speak minor currency units (%s).", minorBalance);
	}

	if ( ( strcmp(minorBalance, "00") ) ||
	     ( zType == ML_CURRENCY ) )
	{
//		if (atol(zData) > 0)
//		{
			sprintf(yTagName, "%s", "and");
			if ((yRc = TEL_Speak(zParty, zInterruptOption,
								PHRASE_TAG, PHRASE_FILE, yTagName, zSync))
								!= TEL_SUCCESS)
			{
				return(yRc);
			}
//		}
		if ( ( zLanguage == ML_SPANISH ) && ( mainBalance == 1 ) )
		{
			yRc = mlLoadLanguage(ML_SPANISH, ML_MALE); // reset it back to male
		}

	// if (mainBalance >= 0)
		if ( strcmp(minorBalance, "00") == 0 )
		{
			sprintf(minorBalance, "%s", "0");
		}

		if ((yRc = TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTRPT, STRING, NUMBER,
					minorBalance, zSync)) != TEL_SUCCESS)
		{
				yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
						GV_Application_Name, ML_VERBOSE, 
						"Failed to speak minor balance (%s).", minorBalance);
			return(yRc);
		}
			yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
						GV_Application_Name, ML_VERBOSE, 
						"Successfully spoke minor balance (%s).", minorBalance);

		if (zType == ML_CURRENCY)
		{
			yRc = mlPlayMinorCurrencyUnit(zLanguage, atol(minorBalance), zParty,
					zInterruptOption, zSync);
			return(yRc);
		}
	}

	return(TEL_SUCCESS);
} /* END: mlSpeak() */

static int speakSpanishHundreds(int whatToSpeak, int mainBalance,
					char *hundreds, char *tens, char *units,
					int zParty, int zInterruptOption, int zSync)
{
	char		balance[16];
	char		yTagName[64] = "";
	int			yRc;

	yRc = UTL_VarLog("speakSpanish", REPORT_VERBOSE, 0, GV_Resource_Name,
			ML_SVC, GV_Application_Name, ML_VERBOSE, "Speaking (%d:%s:%s:%s) "
			"in Spanish.", mainBalance, hundreds, tens, units);
	memset((char *)yTagName, '\0', sizeof(yTagName));
	switch (atol(hundreds))
	{
	case 1:
//		if ( whatToSpeak == HUNDREDS_SW )
//		{
			if ( ( strcmp(tens, "0") == 0 ) &&
			     ( strcmp(units, "0") == 0 ) )
			{
            	sprintf(yTagName, "%s", "hundred");
			}
			else
			{
            	sprintf(yTagName, "%s", "hundredPlus");
			}
//		}
		break;
	case 2:
		sprintf(yTagName, "%s", "twoHundred");
		break;
	case 3:
		sprintf(yTagName, "%s", "threeHundred");
		break;
	case 4:
		sprintf(yTagName, "%s", "fourHundred");
		break;
	case 5:
		sprintf(yTagName, "%s", "fiveHundred");
		break;
	case 6:
		sprintf(yTagName, "%s", "sixHundred");
		break;
	case 7:
		sprintf(yTagName, "%s", "sevenHundred");
		break;
	case 8:
		sprintf(yTagName, "%s", "eightHundred");
		break;
	case 9:
		sprintf(yTagName, "%s", "nineHundred");
		break;

	}
	if ( yTagName[0] != '\0' )
	{
		if ((yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
				PHRASE_FILE, yTagName, zSync)) != TEL_SUCCESS)
		{
			return(yRc);
		}
	}

	sprintf(balance, "%s%s", tens, units);
	if ( strcmp(balance, "00") == 0 )
	{
		sprintf(balance, "%s", "0");
		return(0);
	}

	yRc = TEL_Speak(zParty, zInterruptOption, STRING, NUMBER,
				balance, zSync);

	yRc = UTL_VarLog("speakSpanishHundreds", REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
						GV_Application_Name, ML_VERBOSE, 
						"Successfully spoke balance (%s).", balance);
	return(yRc);
	
} // END: speakSpanishHundreds()

