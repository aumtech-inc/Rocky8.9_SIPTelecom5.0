/*------------------------------------------------------------------------------
File:		TEL_MLSpeakTime.c
Purpose:	Speak the time in multiple languages.
Author:		Aumtech, Inc.
Update:	djb 03/30/2002	Created the file.
Update: 05/31/2005 djb  Added gender parameter.
Update: 03/08/2007 djb  Removed DJBDEBUG statements.
------------------------------------------------------------------------------*/
#include "headers.h"

static int sMLSpeakTime(int zLanguage, int zInputFormat, int zOutputFormat,
					int zParty, int zInterruptOption, char *zData, int zSync);
static	int sSL_speak_time(char *mod, int zLanguage, int zParty, 
		int zInterruptOption, int zOutputFormat, int zSync, long tics);
static int convert_time_to_seconds(char *mod, char *time_str);

/*------------------------------------------------------------------------------
TEL_MLSpeakTime():
------------------------------------------------------------------------------*/
int TEL_MLSpeakTime(int zLanguage, int zGender, int zReloadLanguage,
				char *zReloadTagFile, int zParty, int zInterruptOption,
				int zInputFormat, int zOutputFormat, char *zData, int zSync)
{
	static char	yMod[]="TEL_MLSpeakTime";
	int			yRc=0;
	int			yCounter;
	int			yGender;

	if (zData[0] == '\0')
	{
		yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
				"ERROR: Empty data string received (%s).  Must "
				"be a valid string in a time format.", zData);
		return(-1);
	}
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

	switch(zLanguage)
	{
		case ML_SPANISH:
			if (! gSpanishLoaded)
			{
				if ((yRc = mlLoadLanguage(zLanguage, zGender)) != 0)
				{
					return(yRc);		/* message is logged in routine */
				}
			}
			break;
		default:
			yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
				"ERROR: Invalid language parameter received "
				"(%d).  Must be ML_SPANISH (%d).", zLanguage, ML_SPANISH);
			return(-1);
			break;
#if FRENCH_TIME_IS_DONE
		case ML_FRENCH:
			if (! gFrenchLoaded)
			{
				if ((yRc = mlLoadLanguage(zLanguage, zGender)) != 0)
				{
					return(yRc);		/* message is logged in routine */
				}
			}
			break;
#endif
	}

	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, "Received input (%s).", zData);

	if ((yRc = sMLSpeakTime(zLanguage, zInputFormat, zOutputFormat,
			zParty, zInterruptOption, zData, zSync)) != TEL_SUCCESS)
	{
		(void) mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
		return(yRc);
	}

	yRc = mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
	return(yRc);

} /* END: TEL_MLSpeakTime() */

/*------------------------------------------------------------------------------
sMLSpeakTime():
------------------------------------------------------------------------------*/
static int	sMLSpeakTime(int zLanguage, int zInputFormat, int zOutputFormat,
						int zParty, int zInterruptOption, char *zData,
						int zSync)
{
	static char	yMod[]="sMLSpeakTime";
	int			yRc=0;
	long		seconds;

	switch(zInputFormat)
	{
#if 0
	case MILITARY:
#endif
	case STANDARD:
	case STRING  :
    	seconds = convert_time_to_seconds(yMod, zData);
		switch(seconds)
		{
		case -1:
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
					"Invalid am/pm hour combination.");
			return(-1);
			break;	
		case -2:
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
					"Invalid hour.");
			return(-1);
			break;	
		case -3:
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
					"Invalid minute.");
			return(-1);
			break;	
		case -4:
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_INVALID_PARAM,
					"Cannot convert date to seconds.");
			return(-1);
			break;	
		default:
			break;	
		}
		break;
	case TICS  :
    		seconds = atol(zData);
    		break;
	default:
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
						GV_Application_Name, ML_INVALID_PARAM,
						"ERROR: Invalid input format (%d).  "
						"Must be either STANDARD (%d), STRING (%d), or "
						"TICS (%d).", zInputFormat, STANDARD, STRING, TICS);
		return(-1);
	}

	yRc = sSL_speak_time(yMod, zLanguage, zParty, zInterruptOption,
				zOutputFormat, zSync, seconds);
	return(yRc);
} /* END: sMLSpeakTime() */

/*---------------------------------------------------------------------------
speak time	: Speak all time format
---------------------------------------------------------------------------*/
static	int sSL_speak_time(char *mod, int zLanguage, int zParty, 
			int zInterruptOption, int zOutputFormat, int zSync, long tics)
{
char	hour_str[5];
char	min_str[5];
long	t = tics;
char	am_pm[5];			/* AM - PM */
int		hour, min;
int		ret;
struct tm *pTime;
char	yTmpPhraseTag[64];
static char	yMod[] = "sSL_speak_time";

pTime = localtime(&t);
strftime(min_str, sizeof(min_str), "%M", pTime);

switch(zOutputFormat)
	{
	case TIME_STD:
		strftime(hour_str, sizeof(hour_str), "%I", pTime);	/* hour 1 to 12 */
		strftime(am_pm, sizeof(am_pm), "%p", pTime);
		break;
	case TIME_STD_MN:
		strftime(hour_str, sizeof(hour_str), "%I", pTime);	/* hour 1 to 12 */
		strftime(am_pm, sizeof(am_pm), "%p", pTime);
		if((atoi(hour_str) == 12) && (atoi(min_str) == 0))
		{
			if(strcmp(am_pm, "AM") == 0)
			{
				sprintf(yTmpPhraseTag, "%s", "midnight");
			}
			else if (strcmp(am_pm, "PM") == 0)
			{
				sprintf(yTmpPhraseTag, "%s", "noon");
			}

			ret = TEL_Speak(zParty, zInterruptOption,
					PHRASE_TAG, PHRASE_FILE, yTmpPhraseTag, zSync);
			(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0,
					GV_Resource_Name, ML_SVC, GV_Application_Name, ML_VERBOSE, 
					"%d = TEL_Speak(%s)", ret, yTmpPhraseTag);
			return(ret);
		}
		break;
#if 0
	case TIME_MIL:
		strftime(hour_str, sizeof(hour_str), "%H", pTime);	/* hour 0 to 23 */
		break;
	case TIME_MIL_MN:
		strftime(hour_str, sizeof(hour_str), "%H", pTime);	/* hour 0 to 23 */
		if((atoi(hour_str) == 0) && (atoi(min_str) == 0))
		{
			ret = play_system_phrase(mod, MIDNIGHT_PHRASE);
			return(ret);
		}
		if((atoi(hour_str) == 12) && (atoi(min_str) == 0))
		{
			ret = play_system_phrase(mod, NOON_PHRASE);
			return(ret);
		}
		break;
#endif
	default:
		ret = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
						GV_Application_Name, ML_INVALID_PARAM,
						"ERROR: Invalid output format (%d).  ",
						zOutputFormat);
			return(-1);
			break;
		break;
	} /* switch */

	hour = atoi(hour_str);
	min  = atoi(min_str);

	if ((ret = mlSpeak(zLanguage, ML_NUMBER, zParty, zInterruptOption,
						hour_str, zSync)) != 0)
	{
		return(ret);
	}

	if ( min != 0 )
	{
		ret = TEL_Speak(zParty, zInterruptOption,
						PHRASE_TAG, PHRASE_FILE, "and", zSync);
		if ( ret != 0 )
		{
			return(ret);
		}
	
		if ( ( zLanguage == ML_SPANISH ) && ( min == 15 ) )
		{
			ret = TEL_Speak(zParty, zInterruptOption,
						PHRASE_TAG, PHRASE_FILE, "15Minutes", zSync);
			(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0,
					GV_Resource_Name, ML_SVC, GV_Application_Name, ML_VERBOSE, 
					"line %d: Spoke 15 minutes returned %d.", __LINE__, ret);
			if ( ret != 0 )
			{
				return(ret);
			}
		}
		else if ( ( zLanguage == ML_SPANISH ) && ( min == 30 ) )
		{
			ret = TEL_Speak(zParty, zInterruptOption,
						PHRASE_TAG, PHRASE_FILE, "30Minutes", zSync);

			(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0,
					GV_Resource_Name, ML_SVC, GV_Application_Name, ML_VERBOSE, 
					"line %d: Spoke 30 minutes returned %d.", __LINE__, ret);
			if ( ret != 0 )
			{
				return(ret);
			}
		}
		else 
		{
			(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0,
					GV_Resource_Name, ML_SVC, GV_Application_Name, ML_VERBOSE, 
					"line %d: Speaking %s..", __LINE__, min_str);
			if ((ret = mlSpeak(zLanguage, ML_NUMBER, zParty, zInterruptOption,
							min_str, zSync)) != 0)
			{
				return(ret);
			}
		}
	}

	if ( zLanguage == ML_SPANISH )
	{
		(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0,
					GV_Resource_Name, ML_SVC, GV_Application_Name, ML_VERBOSE, 
					"am_pm:(%s)  hour:%d", am_pm, hour);
		if ( strcmp(am_pm, "AM") == 0 )
		{
			if ( ( hour < 6 ) || ( hour == 12 ) )
			{
				sprintf(yTmpPhraseTag, "%s", "delaEarlyMorning");
			}
			else
			{
				sprintf(yTmpPhraseTag, "%s", "delaMorning");
			}
		}
		else
		{
			if ( ( hour < 6 ) || ( hour == 12 ) )
			{
				sprintf(yTmpPhraseTag, "%s", "delaAfternoon");
			}
			else
			{
				sprintf(yTmpPhraseTag, "%s", "delaEvening");
			}
		}
		ret = TEL_Speak(zParty, zInterruptOption,
					PHRASE_TAG, PHRASE_FILE, yTmpPhraseTag, zSync);
		(void) UTL_VarLog(yMod, REPORT_VERBOSE, 0,
					GV_Resource_Name, ML_SVC, GV_Application_Name, ML_VERBOSE, 
					"line %d: %d = TEL_Speak(%s)",
					__LINE__, ret, yTmpPhraseTag);
	}
	return (0);
#if 0
	if (zOutputFormat == TIME_MIL && min == 0)		
	{
		ret = play_system_phrase(mod, HUNDRED_BASE);
		if (ret != 0) return(ret);
	}

	/* speak minutes */	
	if (min != 0)
	{
		if(min <= 9)
		{
			ret = play_system_phrase(mod, MINUTE_BASE+min);
			if (ret != 0) return(ret);
		}
		else
		{
			ret = play_system_phrase(mod, NUMBER_BASE+min);
			if (ret != 0) return(ret);
		}
	}

	if (strcmp(am_pm, "AM") == 0)
	{
		ret = play_system_phrase(mod, AM_PHRASE);
		if (ret != 0) return(ret);
	}
	else if (strcmp(am_pm, "PM") == 0)
	{
		ret = play_system_phrase(mod, PM_PHRASE);
		if (ret != 0) return(ret);
	}
#endif

} 

/*---------------------------------------------------------------------------
This routine will a string containing a time to seconds since 1/1/1970.
Sting should be in MILITARY (e.g. "1200"), STANDARD ( e.g., "12:01), or
STRING (e.g., "12:01 am")
Returns:  0 - success
	 -1 - problem with am/pm indicator
	 -2 - invalid hour
	 -3 - invalid minute
	 -4 - Couldn't convert to seconds.
---------------------------------------------------------------------------*/
static int convert_time_to_seconds(char *mod, char *time_str)
{

	char hour_str[]="hh";
	char minute_str[]="mm";
	char temp[50];
	int am_found=0, pm_found=0;
	int hour, minute;
	int i, j;
	struct tm t;	
	time_t seconds;
	int	ctr =0;

	  /* Remove blanks and tabs from time string; convert to upper */
	j = 0;
	for (i=0; i < (int)(strlen(time_str)); i++)
	{
		if ( (time_str[i] != ' ') && (time_str[i] != '\t') )
		{
			temp[j++] = toupper(time_str[i]);	
		}
	}
	temp[j]='\0';
       
	if ( strstr(temp, "AM") ) am_found = 1;
	if ( strstr(temp, "PM") ) pm_found = 1;

	if (strchr(temp, ':') != 0)   /* format is hh:mm */
	{
		if (temp[2] == ':')        /* hh:mm */
		{
			hour_str[0] = temp[0];
			hour_str[1] = temp[1];
			minute_str[0] = temp[3];
			minute_str[1] = temp[4];
		}
		if(temp[1] == ':')        /* h:mm */
		{
			hour_str[0] = '0';
			hour_str[1] = temp[0];
			minute_str[0] = temp[2];
			minute_str[1] = temp[3];
		}
	}
	else                            /* format is hhmm */
	{
		ctr=0;
		for(i=0; i <= 4; i++)
			{
			if(isdigit(temp[i]))
				ctr++;
			}
		if(ctr == 3)
			{
			hour_str[0] = '0';
			hour_str[1] = temp[0];
			minute_str[0] = temp[1];
			minute_str[1] = temp[2];
			}
		else
			{
			hour_str[0] = temp[0];
			hour_str[1] = temp[1];
			minute_str[0] = temp[2];
			minute_str[1] = temp[3];
			}
	}
	hour=atoi(hour_str);
	minute=atoi(minute_str);

	if ( (am_found || pm_found) && (hour > 12) ) 	return(-1);
	if ( hour < 0 || hour > 23 )               	return(-2);
	if ( minute < 0 || minute > 59 )        	return(-3);

	/* Adjust hour for am/pm indicator */
	if ( am_found && (hour == 12) ) hour = 0;
	if ( pm_found && (hour <  12) ) hour = hour + 12;

        t.tm_year    = 96;		/* 1996 */
        t.tm_mon     = 3;		/* April (4-1) */
        t.tm_mday    = 12;		/* 12 th */
        t.tm_hour    = hour;
        t.tm_min     = minute;
        t.tm_sec          = 0;
        t.tm_isdst   = -1;
        if ( (seconds = mktime(&t)) == -1) return (-4);
	return(seconds);
} /* END: convert_time_to_seconds() */
