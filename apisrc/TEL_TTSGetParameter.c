
#ident	"@(#)TEL_TTSGetParameter 02/06/07 2.2.0 Copyright 2002 Aumtech, Inc."
/* ----------------------------------------------------------------------
Program:	TEL_TTSGetParameter.c
Purpose:
Author :	Madhav Bhide
Date   : 	06/10/2002
--------------------------------------------------------------------------*/  

#include "TEL_Common.h"

#include "ttsStruct.h"

static char logBuf[256];
static char	ModuleName[] = "TEL_TTSGetParameter";

int TEL_TTSGetParameter( char *zParameterName, char *zParameterValue )
{
	char apiAndParms[MAX_LENGTH] = "";
	int rc;

	sprintf(apiAndParms,"%s(%s,<val>)",ModuleName,zParameterName);

	rc = apiInit(ModuleName, TEL_TTSGETPARAMETER, apiAndParms, 1, 0, 0); 
	if (rc != 0) HANDLE_RETURN(rc);

	if( zParameterName == 0 || zParameterName[0] == '\0')
	{
		sprintf(logBuf,
			"Invalid parameter name.Parameter name cannot be empty."); 
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_READ_FIFO, GV_Err,logBuf);
		HANDLE_RETURN(TEL_FAILURE);
	}

	if( zParameterValue == 0)
	{
		sprintf(logBuf, "%s", "Invalid output buffer.Output buffer cannot be null."); 
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, logBuf);
		HANDLE_RETURN(TEL_FAILURE);
	}

	if(strcmp(zParameterName, "$TTS_LANGUAGE") == 0)
	{
		sprintf(zParameterValue, "%s", GV_TTS_Language);
	}
	else if(strcmp(zParameterName, "$TTS_GENDER") == 0)
	{
		sprintf(zParameterValue, "%s", GV_TTS_Gender);
	}
	else if(strcmp(zParameterName, "$TTS_VOICENAME") == 0)
	{
		sprintf(zParameterValue, "%s", GV_TTS_VoiceName);
	}
	else if(strcmp(zParameterName, "$TTS_DATA_TYPE") == 0)
	{
		sprintf(zParameterValue, "%s", GV_TTS_DataType);
	}
	else if(strcmp(zParameterName, "$TTS_COMPRESSION") == 0)
	{
		sprintf(zParameterValue, "%s", GV_TTS_Compression);
	}
	else
	{
		sprintf(logBuf, "Invalid parameter name: %s. "
			"No such adjustable parameter.", zParameterName);
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, logBuf);
		HANDLE_RETURN(TEL_FAILURE);
	}

	if(*zParameterValue == 0)
	{
		sprintf(logBuf, "Value of %s has not been set.", zParameterName);
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_INVALID_PARM, GV_Err, logBuf);
		HANDLE_RETURN(TEL_FAILURE);
		
	}

	HANDLE_RETURN( TEL_SUCCESS );
}
