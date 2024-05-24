/*------------------------------------------------------------------------------
File:		TEL_MLSpeakDate.c
Purpose:	Speak the date in multiple languages.
Author:		Aumtech, Inc.
Update:	djb 03/30/2002	Created the file.
Update:	djb 09/09/2002	Added 2nd, 3rd of the day of the month.
Update: 05/31/2005 djb  Added gender parameter.
------------------------------------------------------------------------------*/
#include "headers.h"

/*	Global Variables */
static char		gStrInputFormat[32];
static char		gStrOutputFormat[32];

static char	gMonthArray[14][16] = {
	"",
	"january",
	"february",
	"march",
	"april",
	"may",
	"june",
	"july",
	"august",
	"september",
	"october",
	"november",
	"december",
	""
};

/* Function Prototypes */
static int convert_time_to_seconds(char *mod, char *time_str);
static int parse_date(char *string, int informat, char *ccyymmdd_ptr,
								int *year_ptr, int *month_ptr, int *day_ptr);
static int sMLSpeakDate(int zLanguage, int zInputFormat, int zOutputFormat,
					int zParty, int zInterruptOption, char *zData, int zSync);
static int sSpeakDayOfMonth(int zLanguage, int zGender, int zParty,
			int zInterruptOption, int zDay, int zSync);
static int sSpeakMonth(int zLanguage, int zGender, int zParty,
			int zInterruptOption, int zMonth, int zSync);
static int sSpeakYear(int zLanguage, int zGender, int zParty,
			int zInterruptOption, int zYear, int zSync);
static int sValidateFormats(int zInputFormat, int zOutputFormat);

/*------------------------------------------------------------------------------
TEL_MLSpeakDate():
------------------------------------------------------------------------------*/
int TEL_MLSpeakDate(int zLanguage, int zGender, int zReloadLanguage,
				char *zReloadTagFile, int zParty, int zInterruptOption,
				int zInputFormat, int zOutputFormat, char *zData, int zSync)
{
	static char	yMod[] = "TEL_MLSpeakDate";
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

	yRc = sValidateFormats(zInputFormat, zOutputFormat);
	switch( yRc )
    {
	    case 0:
	        break;
	    case -1:
			yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
					GV_Application_Name, ML_INVALID_PARAM,
					"Invalid input format %d.", zInputFormat);
			return(-1);
	        break;
	    case -2:
			yRc = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name, ML_SVC,
					GV_Application_Name, ML_INVALID_PARAM,
					"Invalid output format %d.", zOutputFormat);
			return(-1);
	        break;
    }

	switch(zLanguage)
	{
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
				"(%d).  Must be ML_SPANISH (%d).", zLanguage, ML_SPANISH);
			return(-1);
			break;
#if FRENCH_TIME_IS_DONE
		case ML_FRENCH:
			if (! gFrenchLoaded)
			{
				if ((yRc = mlLoadLanguage(zLanguage, yGender)) != 0)
				{
					return(yRc);		/* message is logged in routine */
				}
			}
			break;
#endif
	}

	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE, "Received input (%s).", zData);

	if ((yRc = sMLSpeakDate(zLanguage, zInputFormat, zOutputFormat,
			zParty, zInterruptOption, zData, zSync)) != TEL_SUCCESS)
	{
		(void) mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
		return(yRc);
	}

	yRc = mlRestoreLangTag(zReloadLanguage, zReloadTagFile);
	return(yRc);

} /* END: TEL_MLSpeakDate() */

/*---------------------------------------------------------------------------
This routine speaks the date in specified format.
--------------------------------------------------------------------------*/
static int  sMLSpeakDate(int zLanguage, int zInputFormat, int zOutputFormat,
                        int zParty, int zInterruptOption, char *zData,
                        int zSync)
{
	static char		yMod[] = "sMLSpeakDate";
	int				yGender;
	int	month;
	int	day;
	int	year;
	int	ret, ret_code;
	char	ccyymmdd[20];
	char	err_str[80];
	time_t	today, tomorrow, yesterday;
	struct	tm *p;
	int m, d, y;
	
	switch(zLanguage)
	{
		case ML_FRENCH:	
			yGender = ML_FEMALE;
			break;
		case ML_SPANISH:	
			yGender = ML_MALE;
			break;
	}
	ret = parse_date(zData, zInputFormat, ccyymmdd, &year, &month, &day);
	switch(ret)
	{
	case -1:
			ret_code = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name,
					ML_SVC, GV_Application_Name, ML_INVALID_PARAM,
					"Unable to speak %s: inf=%s. outf=%s. value=(%s). %s"
					"date", gStrInputFormat, gStrOutputFormat,
					zData, "Invalid date.");
		return(-1);
		break;
	case -2:
			ret_code = UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name,
					ML_SVC, GV_Application_Name, ML_INVALID_PARAM,
					"Invalid input format %d.", zInputFormat);
		return(-1);
		break;
	}
	
	switch (zOutputFormat)
	{
	  	case DATE_YTT:  /* Yesterday, Today, Tommorrow. */
			time(&today);

			p = localtime(&today);
			m = p->tm_mon + 1;
			d = p->tm_mday;
			y = p->tm_year + 1900;
			if ( month == m && day == d && year == y)
			{
				ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "today", zSync);
				break;	
			}
	
			/* See if we should speak "Tomorrow" */	 
			tomorrow = today + (24 * 60 * 60); 
			p = localtime(&tomorrow);
			m = p->tm_mon + 1;
			d = p->tm_mday;
			y = p->tm_year + 1900;
			if ( month == m && day == d && year == y)
			{
				ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "tomorrow", zSync);
				break;
			}
	
			/* See if we should speak yesterday */	
			yesterday = today - (24 * 60 * 60);
			p = localtime(&yesterday);
			m = p->tm_mon + 1;
			d = p->tm_mday;
			y = p->tm_year + 1900;
			if ( month == m && day == d && year == y)
			{
				ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "yesterday", zSync);
				break;
			}
	
			/* Neither today, tomorrow, nor yesterday; so just speak date */
			ret_code = sSpeakDayOfMonth(zLanguage, yGender, zParty,
							zInterruptOption, day, zSync);
			if (ret_code != 0) break;

			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakMonth(zLanguage, yGender, zParty,
							zInterruptOption, month, zSync);
			if (ret_code != 0 ) break;
	
    		break;
	  	case DATE_MMDD:
			ret_code = sSpeakDayOfMonth(zLanguage, yGender, zParty,
							zInterruptOption, day, zSync);
			if (ret_code != 0 ) break;
	    		
			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakMonth(zLanguage, yGender, zParty,
							zInterruptOption, month, zSync);
			if (ret_code != 0 ) break;
	
			break;
	  	case DATE_MMDDYY:
			ret_code = sSpeakDayOfMonth(zLanguage, yGender, zParty,
							zInterruptOption, day, zSync);
			if (ret_code != 0 ) break;
	                
			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakMonth(zLanguage, yGender, zParty,
							zInterruptOption, month, zSync);
			if (ret_code != 0 ) break;
		
			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakYear(zLanguage, yGender, zParty,
							zInterruptOption, year, zSync);
			if (ret_code != 0 ) break;
	               
	    	break;
	  	case DATE_MMDDYYYY:
			ret_code = sSpeakDayOfMonth(zLanguage, yGender, zParty,
							zInterruptOption, day, zSync);
			if (ret_code != 0) break;
	
			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakMonth(zLanguage, yGender, zParty,
							zInterruptOption, month, zSync);
			if (ret_code != 0 ) break;
	                
			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakYear(zLanguage, yGender, zParty,
							zInterruptOption, year, zSync);
			if (ret_code != 0 ) break;
	
	    		break;
	  	case DATE_DDMM:
			ret_code = sSpeakDayOfMonth(zLanguage, yGender, zParty,
							zInterruptOption, day, zSync);
			if (ret_code != 0) break;
	
			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakMonth(zLanguage, yGender, zParty,
							zInterruptOption, month, zSync);
			if (ret_code != 0 ) break;

    		break;
	  	case DATE_DDMMYY:
			ret_code = sSpeakDayOfMonth(zLanguage, yGender, zParty,
							zInterruptOption, day, zSync);
			if (ret_code != 0) break;
	
			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakMonth(zLanguage, yGender, zParty,
							zInterruptOption, month, zSync);
			if (ret_code != 0 ) break;

			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakYear(zLanguage, yGender, zParty,
							zInterruptOption, year, zSync);
	 		if (ret_code != 0 ) break;
	
    		break;
	  	case DATE_DDMMYYYY:
			ret_code = sSpeakDayOfMonth(zLanguage, yGender, zParty,
							zInterruptOption, day, zSync);
			if (ret_code != 0) break;
	               
			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;

			ret_code = sSpeakMonth(zLanguage, yGender, zParty,
							zInterruptOption, month, zSync);
			if (ret_code != 0 ) break;

			ret_code = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "de", zSync);
			if (ret_code != 0) break;
	                
			ret_code = sSpeakYear(zLanguage, yGender, zParty,
							zInterruptOption, year, zSync);
			if (ret_code != 0 ) break;

	    	break;
	} /* switch */

	if (ret_code == -1)
	{
		(void) UTL_VarLog(yMod, REPORT_NORMAL, 0, GV_Resource_Name,
					ML_SVC, GV_Application_Name, ML_CANT_SPEAK,
					"Unable to speak %s: inf=%s. outf=%s. value=*%s).",
					"date", gStrInputFormat, gStrOutputFormat, zData);
	}

	return(ret_code);
} /* END: sMLSpeakDate() */

/*---------------------------------------------------------------------------
sSpeakDayOfMonth(): speak day of month
---------------------------------------------------------------------------*/
static int sSpeakDayOfMonth(int zLanguage, int zGender, int zParty,
			int zInterruptOption, int zDay, int zSync)
{
	int		yRc;
	char	yDayStr[8];

	if ( (zLanguage == ML_SPANISH) && (zDay < 4) )
	{
		switch ( zDay )
		{
			case 1:
				yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "first", zSync);
				break;
			case 2:
				yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "second", zSync);
				break;
			case 3:
				yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, "third", zSync);
				break;
		}
		return(yRc);
	}

	sprintf(yDayStr, "%d", zDay);
	yRc = mlSpeak(zLanguage, ML_NUMBER, zParty, zInterruptOption,
					yDayStr, zSync);
	return(yRc);
} /* END: sSpeakDayOfMonth() */

/*---------------------------------------------------------------------------
sSpeakMonth(): speak the month
---------------------------------------------------------------------------*/
static int sSpeakMonth(int zLanguage, int zGender, int zParty,
			int zInterruptOption, int zMonth, int zSync)
{
	int		yRc;
	char	yMod[] = "sSpeakMonth";

	yRc = TEL_Speak(zParty, zInterruptOption, PHRASE_TAG,
								PHRASE_FILE, gMonthArray[zMonth], zSync);
	yRc = UTL_VarLog(yMod, REPORT_VERBOSE, 0, GV_Resource_Name, ML_SVC,
				GV_Application_Name, ML_VERBOSE,
				"%d = TEL_Speak(%s)", yRc, gMonthArray[zMonth]);
	return(yRc);

} /* END: sSpeakMonth() */

/*---------------------------------------------------------------------------
sSpeakYear(): speak the year
---------------------------------------------------------------------------*/
static int sSpeakYear(int zLanguage, int zGender, int zParty,
			int zInterruptOption, int zYear, int zSync)
{
	int		yRc;
	char	yYearStr[8];

	sprintf(yYearStr, "%d", zYear);
	yRc = mlSpeak(zLanguage, ML_NUMBER, zParty, zInterruptOption,
					yYearStr, zSync);

	return(yRc);

} /* END: sSpeakYear() */

/*-----------------------------------------------------------------------------
This routine takes a string in any of the valid date input formats and 
gives back a string in yyyymmdd format and integers for year, month, day.
Return codes: 
	 0 - success
	-1 - invalid input - string too short, month, day no good, etc.
	-2 - invalid input format
-----------------------------------------------------------------------------*/
static int parse_date(char *string, int informat, char *ccyymmdd_ptr, int *year_ptr, int *month_ptr, int *day_ptr)
{
	char	cent_str[]="cc";
	char	year_str[]="yy";
        char    month_str[]="mm";
        char    day_str[]="dd";
	char 	ccyymmdd[]="ccyymmdd";
	long 	tics;
	int	month, day, year,century;
	int	i, j;
	char	string1[100];
	struct tm *pTime;

	if ( informat != TICS)
	{
                /* Determine century for when input format is YY, not YYYY */
                time(&tics);
		pTime = localtime(&tics);
                strftime(ccyymmdd, sizeof(ccyymmdd), "%Y%m%d", pTime);
                cent_str[0]     = ccyymmdd[0];
                cent_str[1]     = ccyymmdd[1];
                century         = atoi(cent_str)*100; /* either 1900 or 2000 */

		/* Remove all non-numeric characters, including spaces */
		j = 0;
		for(i = 0; i < (int)strlen(string); i++)
		{
			if(isdigit(string[i]))
			{
				string1[j] = string[i];
				j++;
			}
		}
		string1[j] = '\0';
	}

	switch (informat)
	{
	case TICS:
		tics=atol(string);
		pTime = localtime(&tics);
                strftime(ccyymmdd, sizeof(ccyymmdd), "%Y%m%d", pTime);
		cent_str[0] 	= ccyymmdd[0];
		cent_str[1] 	= ccyymmdd[1];
		year_str[0] 	= ccyymmdd[2];
		year_str[1] 	= ccyymmdd[3];
		month_str[0] 	= ccyymmdd[4];
		month_str[1] 	= ccyymmdd[5];
		day_str[0] 	= ccyymmdd[6];
		day_str[1] 	= ccyymmdd[7];
		year 		= atoi(year_str) + atoi(cent_str) * 100; 
		month		= atoi(month_str);
		day		= atoi(day_str);
		break;
	case STRING:
	case MM_DD_YY:
		switch(strlen(string1))
		{
		case 6:		/* mmddyy */
			month_str[0] 	= string1[0];
			month_str[1] 	= string1[1];
			day_str[0] 	= string1[2];
			day_str[1] 	= string1[3];
			year_str[0] 	= string1[4];
			year_str[1] 	= string1[5];
			year 		= atoi(year_str) + century; 
			month		= atoi(month_str);
			day		= atoi(day_str);
			break;
		case 8:	/* mmddyyyy */
			month_str[0] 	= string1[0];
			month_str[1] 	= string1[1];
			day_str[0] 	= string1[2];
			day_str[1] 	= string1[3];
			cent_str[0] 	= string1[4];
			cent_str[1] 	= string1[5];
			year_str[0] 	= string1[6];
			year_str[1] 	= string1[7];
			year 		= atoi(year_str) + atoi(cent_str)*100; 
			month		= atoi(month_str);
			day		= atoi(day_str);
			break;	
		default:
			return(-1);
			break;	
		}
		break;	
	case DD_MM_YY:
		switch(strlen(string1))
		{
		case 6: 	/* ddmmyy */
			day_str[0] 	= string1[0];
			day_str[1] 	= string1[1];
			month_str[0] 	= string1[2];
			month_str[1] 	= string1[3];
			year_str[0] 	= string1[4];
			year_str[1] 	= string1[5];
			year 		= atoi(year_str) + century; 
			month		= atoi(month_str);
			day		= atoi(day_str);
			break;
		case 8:		/* ddmmyyyy */
			day_str[0] 	= string1[0];
			day_str[1] 	= string1[1];
			month_str[0] 	= string1[2];
			month_str[1] 	= string1[3];
			cent_str[0] 	= string1[4];
			cent_str[1] 	= string1[5];
			year_str[0] 	= string1[6];
			year_str[1] 	= string1[7];
			year 		= atoi(year_str)+atoi(cent_str) * 100; 
			month		= atoi(month_str);
			day		= atoi(day_str);
			break;
		default:
			return(-1);
			break;	
		}
		break;
	case YY_MM_DD:
		switch(strlen(string1))
		{
		case 6:		/* yymmdd */
			year_str[0] 	= string1[0];
			year_str[1] 	= string1[1];
			month_str[0] 	= string1[2];
			month_str[1] 	= string1[3];
			day_str[0] 	= string1[4];
			day_str[1] 	= string1[5];
			year 		= atoi(year_str) + century; 
			month		= atoi(month_str);
			day		= atoi(day_str);
			break;
		case 8:	/* yyyymmdd */
			cent_str[0] 	= string1[0];
			cent_str[1] 	= string1[1];
			year_str[0] 	= string1[2];
			year_str[1] 	= string1[3];
			month_str[0] 	= string1[4];
			month_str[1] 	= string1[5];
			day_str[0] 	= string1[6];
			day_str[1] 	= string1[7];
			year 		= atoi(year_str)+ atoi(cent_str) * 100; 
			month		= atoi(month_str);
			day		= atoi(day_str);
			break;	
		default:
			return(-1);
			break;	
		}
		break;
	default:
		/* Invalid input format */
		return(-2); 	
		break;

	} /* end of switch */
	
	/* Set return values */
	*month_ptr = month;
	strcpy(ccyymmdd_ptr, ccyymmdd);
	*day_ptr = day;
	*year_ptr = year;

        if ((month < 1) || (month > 12) || (day < 1) || (day > 31)) return(-1); 

	/* Check for number of days in each month. */
	switch(month)
	{
    	case 1:     /* 31 days. */
    	case 3:
    	case 5:
    	case 7:
    	case 8:
    	case 10:
    	case 12:
       		if (day > 31) return(-1); 
        	break;
    	case 4:     /* 30 days. */
    	case 6:
    	case 9:
    	case 11:
        	if (day > 30) return(-1);
        	break;
    	case 2:     /* February. */
       		if( (year % 4) == 0)
        	{
            		if(day > 29) return(-1);
        	}
        	else
        	{
            		if(day > 28) return(-1);
        	}
        	break;
    	default:
       		return(-1);
        	break;
	} /* switch on month */
}
/*-----------------------------------------------------------------------------
Validate the input and output format for speaking a date phrase.
	Returns:
		0	zInputFormat and zOutputFormat are valid.
		-1  zInputFormat is invalid
		-2  zOutputFormat is invalid
-----------------------------------------------------------------------------*/
static int sValidateFormats(int zInputFormat, int zOutputFormat)
{ 
	int		ret=0;

	gStrInputFormat[0] = '\0';
	gStrOutputFormat[0] = '\0';

	switch(zOutputFormat)
	{
    	case DAY:
				sprintf(gStrOutputFormat, "%s", "DAY");
				break;
		case DATE_YTT:		
				sprintf(gStrOutputFormat, "%s", "DATE_YTT");
				break;
    	case DATE_MMDD:		
				sprintf(gStrOutputFormat, "%s", "DATE_MMDD");
				break;
    	case DATE_MMDDYY:	
				sprintf(gStrOutputFormat, "%s", "DATE_MMDDYY");
				break;
    	case DATE_MMDDYYYY:	
				sprintf(gStrOutputFormat, "%s","DATE_MMDDYYYY");
				break;
    	case DATE_DDMM:		
				sprintf(gStrOutputFormat, "%s", "DATE_DDMM");
				break;
    	case DATE_DDMMYY:	
				sprintf(gStrOutputFormat, "%s", "DATE_DDMMYY");
				break;
    	case DATE_DDMMYYYY:	
				sprintf(gStrOutputFormat, "%s", "DATE_DDMMYYYY");
				break;
    	default:
				return(-2);
				break;
	} /* switch */

	switch (zInputFormat)
	{
		case MM_DD_YY:
				sprintf(gStrInputFormat, "%s", "MM_DD_YY");
				break;
		case DD_MM_YY:
				sprintf(gStrInputFormat, "%s", "DD_MM_YY");
				break;
		case YY_MM_DD:
				sprintf(gStrInputFormat, "%s", "YY_MM_DD");
				break;
		case STRING:
				sprintf(gStrInputFormat, "%s", "STRING");
				break;
		case TICS:
				sprintf(gStrInputFormat, "%s", "TICS");
				break;
		default:
				return(-1);
				break;
	}

	return(0);
} /* END: sValidateFormats() */
