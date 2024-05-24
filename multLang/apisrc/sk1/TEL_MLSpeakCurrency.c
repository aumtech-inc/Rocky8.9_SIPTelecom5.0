/*------------------------------------------------------------------------------
File:		TEL_MLSpeakCurrency.c
Purpose:	Speak currency in multiple languages.
Author:		Aumtech, Inc.
Update:	djb 01/25/2002	Created the file.
------------------------------------------------------------------------------*/
#include "headers.h"

static void sGetDollars(char *zInData, char *zDollars);

/*------------------------------------------------------------------------------
TEL_MLSpeakCurrency():
------------------------------------------------------------------------------*/
int TEL_MLSpeakCurrency(int zLanguage, int zGender, int zReloadLanguage, 
		char *zReloadTagFile, int zParty, int zInterruptOption,
		char *zData, int zSync)
{
	static char	yMod[] = "TEL_MLSpeakCurrency";
	int			yRc = 0;
	int			yTmpGender;
	char		yTmpData[32];
	char		yTmpDollars[32];

	if (zData[0] == '\0')
	{
		yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
			GV_Application_Name, ML_INVALID_PARAM,
			"ERROR: Empty data string received (%s).  "
			"Must be a valid integer.", zData);
		return(-1);
	}

	switch(zLanguage)
	{
		case ML_FRENCH:
			if (! gFrenchLoaded)
			{
				if ((yRc = mlLoadLanguage(zLanguage, ML_MALE)) != 0)
				{
					return(yRc);		/* message is logged in routine */
				}
			}
			break;
		case ML_SPANISH:
			sprintf(yTmpData, "%s", zData);
			sGetDollars(yTmpData, yTmpDollars);
			if ( ( zGender == ML_FEMALE ) &&
			     ( strcmp(yTmpDollars, "1") == 0 ) )
			{
				yTmpGender = ML_FEMALE;
			}
			else
			{
				yTmpGender = ML_MALE;
			}
			if (! gSpanishLoaded)
			{
				if ((yRc = mlLoadLanguage(zLanguage, yTmpGender)) != 0)
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

	if ((yRc = mlSpeak(zLanguage, ML_CURRENCY, zParty, zInterruptOption,
									zData, zSync)) != TEL_SUCCESS)
	{
		(void) mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
		return(yRc);
	}

	yRc = mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
	return(yRc);

} /* END: TEL_MLSpeakCurrency() */

/*------------------------------------------------------------------------------sGetDollars():
------------------------------------------------------------------------------*/
static void sGetDollars(char *zInData, char *zDollars)
{
    char        *p;

    zDollars[0] = '\0';

    if ( zInData[0] == '.' )
    {
        sprintf(zDollars, "%s", "0");
        return;

    }
    if ( (p=(char *)strstr(zInData, ".")) == NULL )
    {
        sprintf(zDollars, "%s", zInData);
        return;
    }
    *p = '\0';
    sprintf(zDollars, "%s", zInData);
} /* END: sGetDollars() */

