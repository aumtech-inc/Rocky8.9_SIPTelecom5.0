/*------------------------------------------------------------------------------
File:		TEL_MLSpeakNumber.c
Purpose:	Speak currency in multiple languages.
Author:		Aumtech, Inc.
Update:	djb 01/25/2002	Created the file.
Update:	djb 03/08/2007	Removed DJBDEBUG statement
------------------------------------------------------------------------------*/
#include "headers.h"

/*------------------------------------------------------------------------------
TEL_MLSpeakNumber():
------------------------------------------------------------------------------*/
int TEL_MLSpeakNumber(int zLanguage, int zGender, int zReloadLanguage, 
		char *zReloadTagFile, int zParty, int zInterruptOption,
		int zOutputFormat, char *zData, int zSync)
{
	static char	yMod[]="TEL_MLSpeakNumber";
	int			yRc=0;
	int			yCounter;
	int			yGender;
	char		*pData;
	char		yDigit[6];

	switch (zGender)
	{
		case ML_MALE:
			yGender = zGender;
			break;
		case ML_FEMALE:
			yGender = zGender;
			break;
		case ML_NEUTRAL:
			yGender = ML_MALE;
			break;
		default:
			yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
				"ERROR: Invalid gender parameter received "
				"(%d).  Must be either ML_MALE (%d), ML_FEMALE (%d), or "
				"ML_NEUTRAL (%d).", zGender, ML_MALE, ML_FEMALE, ML_NEUTRAL);
			return(-1);
			break;
	}

	if (zData[0] == '\0')
	{
		yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0,
			GV_Resource_Name, ML_SVC,
			GV_Application_Name, ML_INVALID_PARAM,
			"ERROR: Empty data string received (%s).  Must "
			"be a valid integer.", zData);
		return(-1);
	}
	for (yCounter = 0; zData[yCounter] != '\0'; yCounter++)
	{
		if ( ! isdigit(zData[yCounter]) )
		{
			yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0,
				GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
				"ERROR: Invalid data string received (%s).  Must "
				"be a valid integer.", zData);
			return(-1);
		}
	}

	switch(zOutputFormat)
	{
		case NUMBER:
		case DIGIT:
			break;
		default:
			(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
						GV_Application_Name, ML_INVALID_PARAM,
						"ERROR: Invalid output format (%d).  "
						"Must be either NUMBER (%d) or DIGIT (%d).",
						zOutputFormat, NUMBER, DIGIT);
			return(-1);
			break;
	}
	switch(zLanguage)
	{
		case ML_FRENCH:
			if (! gFrenchLoaded)
			{
				if ((yRc = mlLoadLanguage(zLanguage, yGender)) != 0)
				{
					return(yRc);		/* message is logged in routine */
				}
			}
			break;
		case ML_SPANISH:
			if (! gSpanishLoaded)
			{
				if ((yRc = mlLoadLanguage(zLanguage, yGender)) != 0)
				{
					return(yRc);		/* message is logged in routine */
				}
			}
			break;
		default:
			yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
				"ERROR: Invalid language parameter received "
				"(%d).  Must be either ML_FRENCH (%d) or ML_SPANISH (%d).",
				zLanguage, ML_FRENCH, ML_SPANISH);
			return(-1);
	}

	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, "Received input (%s).", zData);

	if (zOutputFormat == NUMBER)
	{
		if ((yRc = mlSpeak(zLanguage, ML_NUMBER, zParty, zInterruptOption,
									zData, zSync)) != TEL_SUCCESS)
		{
			(void) mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
			return(yRc);
		}
	}
	else
	{
		pData = zData;
		while ( *pData != '\0' )
		{
			sprintf(yDigit, "%c", *pData);
			if ((yRc = mlSpeak(zLanguage, ML_NUMBER, zParty, zInterruptOption,
										yDigit, zSync)) != TEL_SUCCESS)
			{
				(void) mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
				return(yRc);
			}
			pData++;
		}
	}

	yRc = mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
	return(yRc);

} /* END: TEL_MLSpeakNumber() */
