#ident	"@(#)TEL_TTSSetParameter 02/06/07 2.2.0 Copyright 2002 Aumtech, Inc."
/* ----------------------------------------------------------------------
Program:	TEL_TTSSetParameter.c
Purpose:
Author :	Madhav Bhide
Date   : 	06/10/2002
--------------------------------------------------------------------------*/  

#include "TEL_Common.h"

#include "ttsStruct.h"

static char logBuf[256];
static char	ModuleName[] = "TEL_TTSSetParameter";

int TEL_TTSSetParameter( char *zParameterName, char *zParameterValue )
{
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	sprintf(apiAndParms,"%s(%s,%s)",ModuleName,zParameterName,zParameterValue);

	rc = apiInit(ModuleName, TEL_TTSSETPARAMETER, apiAndParms, 1, 0, 0); 
	if (rc != 0) HANDLE_RETURN(rc);

	if(zParameterName[0] == '\0')
	{
		sprintf(logBuf, "%s", 
			"Invalid parameter name.Parameter name cannot be empty.");
		telVarLog(ModuleName,REPORT_NORMAL,TEL_INVALID_PARM, 
				GV_Err, logBuf); 
		HANDLE_RETURN(TEL_FAILURE);
	}

	if(strcmp(zParameterName, "$TTS_LANGUAGE") == 0)
	{
		if((strcmp(zParameterValue, "ENGLISH") == 0) ||
			(strcmp(zParameterValue, "FRENCH") == 0) ||
			(strcmp(zParameterValue, "MEXICAN_SPANISH") == 0) ||
			(strcasecmp(zParameterValue, "es-US") == 0) ||
			(strcasecmp(zParameterValue, "es-MX") == 0) ||
			(strcasecmp(zParameterValue, "en-US") == 0) ||
			(strcasecmp(zParameterValue, "en-IN") == 0) ||
			(strcasecmp(zParameterValue, "en-GB") == 0) ||
			(strcasecmp(zParameterValue, "en-AU") == 0) ||
			(strcasecmp(zParameterValue, "fr-CA") == 0) ||
			(strcasecmp(zParameterValue, "fr-FR") == 0) ||
			(strcasecmp(zParameterValue, "de-DE") == 0) ||
			(strcasecmp(zParameterValue, "it-IT") == 0) ||
			(strcasecmp(zParameterValue, "ja-JP") == 0) ||
			(strcasecmp(zParameterValue, "ko-KR") == 0) ||
			(strcasecmp(zParameterValue, "pt-BR") == 0) ||
			(strcasecmp(zParameterValue, "zh-CN") == 0) ||
			(strcasecmp(zParameterValue, "zh-TW") == 0) ||
		   (strcmp(zParameterValue, "SPANISH") == 0))
		{
			sprintf(GV_TTS_Language, "%s", zParameterValue);
		}
		else
		{
			sprintf(logBuf, "Warning:Invalid value (%s) for parameter (%s) "
				"Valid values are es/en/fr/de/it/ja/ko/pt/jh. Using default as en-US.", 
				zParameterValue, zParameterName);
			telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
				logBuf);
			sprintf(GV_TTS_Language, "%s", "en-US");
		}
	}
	else if(strcmp(zParameterName, "$TTS_GENDER") == 0)
	{
		if((strcmp(zParameterValue, "MALE") == 0) ||
		   (strcmp(zParameterValue, "FEMALE") == 0))
		{
			sprintf(GV_TTS_Gender, "%s", zParameterValue);
		}
		else
		{
			sprintf(logBuf, "Warning:Invalid value (%s) for parameter (%s). "
				"Valid values are MALE or FEMALE. Using default as FEMALE.", 
				zParameterValue, zParameterName);
			telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
				logBuf);
			sprintf(GV_TTS_Gender, "%s", "FEMALE");
		}
	}
	else if(strcmp(zParameterName, "$TTS_VOICENAME") == 0)
	{
		sprintf(GV_TTS_VoiceName, "%s", zParameterValue);
	}
	else if(strcmp(zParameterName, "$TTS_DATA_TYPE") == 0)
	{
		if((strcmp(zParameterValue, "FILE") == 0) ||
		   (strcmp(zParameterValue, "EMAIL") == 0) ||
		   (strcmp(zParameterValue, "STRING") == 0))
		{
			sprintf(GV_TTS_DataType, "%s", zParameterValue);
		}
		else
		{
			sprintf(logBuf, "Warning:Invalid value (%s) for parameter (%s). "
				"Valid values are STRING, FILE or EMAIL. "
				"Using default as STRING.", zParameterValue, zParameterName);
			telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
				logBuf);
			sprintf(GV_TTS_DataType, "%s", "STRING");
		}
	}
	else if(strcmp(zParameterName, "$TTS_COMPRESSION") == 0)
	{
		if((strcmp(zParameterValue, "COMP_24ADPCM") == 0) ||
		   (strcmp(zParameterValue, "COMP_32ADPCM") == 0) ||
		   (strcmp(zParameterValue, "COMP_48PCM") == 0) ||
		   (strcmp(zParameterValue, "COMP_64PCM") == 0) ||
		   (strcmp(zParameterValue, "COMP_WAV") == 0))
		{
			sprintf(GV_TTS_Compression, "%s", zParameterValue);
		}
		else
		{
			sprintf(logBuf, "Warning:Invalid value (%s) for parameter (%s). "
				"Valid values are COMP_24ADPCM, COMP_32ADPCM, COMP_48PCM, "
				"COMP_64PCM, COMP_WAV. Using default as COMP_64PCM.", 
				zParameterValue, zParameterName);
			telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, 
				logBuf);
			sprintf(GV_TTS_Compression, "%s", "COMP_64PCM");
		}
	}
	else
	{
		sprintf(logBuf, "Invalid parameter name: %s. "
			"No such adjustable parameter.", zParameterName);
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, logBuf);
		HANDLE_RETURN(TEL_FAILURE);
	}

	HANDLE_RETURN( TEL_SUCCESS );
}
