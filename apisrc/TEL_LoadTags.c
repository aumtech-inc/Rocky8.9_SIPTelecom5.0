/*-----------------------------------------------------------------------------
Program:	TEL_LoadTags.c
Purpose:	To load tag file(s) for use with TEL_PromptAndCollect.
Author: 	Aumtech, Inc.
Update:		11/28/00 djb	Created the file.
Update:		07/18/01 apj Ported to IP.
Update:		2001/07/28 apj Set party1Required to 0 in apiInit
Update:		2001/07/31 djb Fixed core dump; added strchr() before sscanf().
Update:		2001/07/31 djb Added sIsThisADataTag() to correct a logic bug.
Update: 08/25/04 djb Changed sys_errlist strerror(errno).
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

/* These values are held during validation to issure that error messages
   are able to show the incorrect value the user specified for a section
   parameter. This is a kludge, but I don't have a better way yet. gpw 3/12/01
*/
static char g_party[64];
static char g_beep[64];
static char g_terminateOption[64];
static char g_inputOption[64];
static char g_interruptOption[64];

static int sAppendFile(char *Mod, char *zSourceFile, char *zDestFile, int zTagFileIndex);
static int sBuildTagFile(char *Mod, char *zTagFileList, char *zTmpTagFile);
static int sDoesEntryAlreadyExist(char *Mod, char *zPhraseTag, int zLineNumber, int zLog);
static int sGetNextSectionInfo(FILE *zFp, char *zSectionName, 
			char *zValue, long zBufSize, char *zErrorMsg);
static int sGetVariablesForTag(char *zTagName, char *zPhraseFile, 
	int *zTagType, int *zInputFormat, int *zOutputFormat);
static int sGetTagInfo(int zLineNumber, char *zTagFile, int *zTagLineNumber);
static int sIsThisADataTag(char *zData);
static int sLoadTagList(char *Mod, FILE *zFpTagFile, char *zTmpTagFile);
static int sLoadPncSectionVariables(char *Mod, FILE *zFpTagFile, char *zTmpTagFile);
static int sNumCharInString(char *zString, char zChar, int *zNumInstances);
static int sPrintTags(char *Mod);
static int sPrintPncInput(char *Mod);
static int sSetPncSectionVariableValue(char *Mod, char *zName, char *zValue, 
		char *zSectionName, char *zFilename, PncInput *zNowPncInput);
static int sSetPncSectionVariableDefaults(char *Mod, PncInput *zNowPncInput);

static int sReadLogicalLine(char *zBuf, int zBufSize, FILE *zFp);
static int sRemoveWhitespace(char *zBuf);
static int sValidatePncSectionVariables(char *Mod, PncInput *zPncInput, 
							char *zFilename);

/* funtion  shared by TEL_PlayTag, should be rewritten & put in TEL_Com */
int sTranslateFormatToInt(char *zFormatStr, int *zFormat);
int sGetField(char zDelimiter, char *zInput, int zFieldNum, 
						char *zFieldValue);

/* Global validation routines */
int validate_beep(int value);
int validate_party(int value);
int validate_range(int value, int min, int max);
int validate_inputOption(int value);
int validate_interruptOption(int value);

/*----------------------------------------------------------------------------
Prepare a list of phrase tags
----------------------------------------------------------------------------*/
int TEL_LoadTags(char *zPhraseTagFiles)
{
	int	yRc;
	FILE	*yFpTagFile;
	char	yTmpTagFile[256];
	char	yDiagMsg[128];
	static char Mod[32] = "TEL_LoadTags";

	/* Now set here; was defined and intialized above. */	
	GV_TagsLoaded = 0;
	sprintf(yDiagMsg,"%s(%s)", Mod, zPhraseTagFiles);

	yRc = apiInit(Mod, TEL_LOADTAGS, yDiagMsg, 1, 0, 0); 
	if (yRc != 0) HANDLE_RETURN(yRc);
 
	sprintf(Mod, "%s", "TEL_LoadTags");
	sprintf(LAST_API, "%s", Mod);

	sprintf(yTmpTagFile, "/tmp/%s_%d", Mod, getpid());
	if ((yRc = sBuildTagFile(Mod, zPhraseTagFiles, yTmpTagFile)) != 0)
	{
		/* message is logged in routine */
		if(access(yTmpTagFile, F_OK) == 0)
		{
			unlink(yTmpTagFile);
		}
		HANDLE_RETURN(-1);		
	}

	if((yFpTagFile = fopen(yTmpTagFile, "r")) == NULL)
	{
		sprintf(Msg, "Failed to open tag file <%s>. errno=%d. [%s]",
			yTmpTagFile, errno, strerror(errno));
                telVarLog(Mod, REPORT_NORMAL, TEL_CANT_OPEN_FILE, GV_Err, Msg);

		if(access(yTmpTagFile, F_OK) == 0)
		{
			unlink(yTmpTagFile);
		}
	    HANDLE_RETURN (-1);
	}

	yRc = sLoadTagList(Mod, yFpTagFile, yTmpTagFile);
	if (yRc == -1)
	{
		fclose(yFpTagFile);	/* Message is logged in routine */
		unlink(yTmpTagFile);
		HANDLE_RETURN (-1);
	}
	
	yRc = sLoadPncSectionVariables(Mod, yFpTagFile, yTmpTagFile);
	if (yRc == -1)
	{
		sPrintPncInput(Mod);
		fclose(yFpTagFile);	/* Message is logged in routine */
		unlink(yTmpTagFile);
	    	HANDLE_RETURN (-1);
	}

	fclose(yFpTagFile);
	unlink(yTmpTagFile);

	yRc = sPrintTags(Mod);
	yRc = sPrintPncInput(Mod);

	GV_TagsLoaded = 1;

	HANDLE_RETURN (TEL_SUCCESS);

} /* END: TEL_LoadTags() */

/*------------------------------------------------------------------------------
sTranslateFormatToInt(): 
This function is now shared with TEL_PlayTag and cannot be made static.
------------------------------------------------------------------------------*/
int sTranslateFormatToInt(char *passed_keyword, int *passed_value)
{

	register int i;

	typedef struct integer_keywords
	{
                char * keyword;
                int value;
	}       IKEYWORDS;

	IKEYWORDS ikey[] =      {
	/* parties */
              	"FIRST_PARTY",          FIRST_PARTY,
                "SECOND_PARTY",         SECOND_PARTY,
                "BOTH_PARTIES",         BOTH_PARTIES,
                "FIRST_PARTY_WHISPER",  FIRST_PARTY_WHISPER, 
	/* beep options */ 
                "YES",                  YES,
                "NO",                   NO,
	/* terminateOption */
                "MANDATORY",            MANDATORY,
                "AUTOSKIP",             AUTOSKIP,
                "AUTO",                 AUTO,
	/* interruptOption */
                "NONINTERRUPT",         	NONINTERRUPT,
                "FIRST_PARTY_INTERRUPT",	FIRST_PARTY_INTERRUPT,
                "SECOND_PARTY_INTERRUPT",	SECOND_PARTY_INTERRUPT,
                "INTERRUPT",            	FIRST_PARTY_INTERRUPT,
                "FIRST_PARTY_PLAYBACK_CONTROL" ,FIRST_PARTY_PLAYBACK_CONTROL, 
                "SECOND_PARTY_PLAYBACK_CONTROL",SECOND_PARTY_PLAYBACK_CONTROL,
	/* speak options */
                "NUMERIC",              NUMERIC,
                "ALPHA",                ALPHA,
                "NUMSTAR",              NUMSTAR,
                "ALPHANUM",             ALPHANUM,
                "DOLLAR",               DOLLAR,
	/* input formats */
                "MILITARY",             MILITARY,
                "STANDARD",             STANDARD,
                "STRING",               STRING,
                "TICS",                 TICS,
                "MM_DD_YY",             MM_DD_YY,
                "YY_MM_DD",             YY_MM_DD,
                "DD_MM_YY",             DD_MM_YY,
                "PHRASE_FILE",          PHRASE_FILE,
                "DOUBLE",               DOUBLE,
                "INTEGER",              INTEGER,
                "PHONE",                PHONE,
                "SOCSEC",               SOCSEC,
                "SYSTEM_PHRASE",        SYSTEM_PHRASE,            
        /* output formats */
                "DATE_YTT",             DATE_YTT,
                "DATE_MMDD",            DATE_MMDD,
                "DATE_MMDDYY",          DATE_MMDDYY,
                "DATE_MMDDYYYY",        DATE_MMDDYYYY,
                "DATE_DDMM",            DATE_DDMM,
                "DATE_DDMMYY",          DATE_DDMMYY,
                "DATE_DDMMYYYY",        DATE_DDMMYYYY,
                "TIME_STD",             TIME_STD,
                "TIME_STD_MN",          TIME_STD_MN,
                "TIME_MIL",             TIME_MIL,
                "TIME_MIL_MN",          TIME_MIL_MN,
                "DAY",                  DAY,
                "PHRASE",               PHRASE,
                "DOLLAR",               DOLLAR,
                "DIGIT",                DIGIT,
                "ALPHANUM",             ALPHANUM,
                "NUMBER",               NUMBER,
		"END_OF_KEYWORDS",      -999,              
	};

        i=0;
        while ( strcmp(ikey[i].keyword, "END_OF_KEYWORDS") !=0 )
        {
                if (strcmp(passed_keyword, ikey[i].keyword) == 0) break;
                i++;
        }
        *passed_value = ikey[i].value;
        return(0);


} /* END: sTranslateFormatToInt() */

/*------------------------------------------------------------------------------
sBuildTagFile():
	Accepts a comma-delimited list of tag files, and appends them to
	zTmpTagFile.
------------------------------------------------------------------------------*/
static int	sBuildTagFile(char *Mod, char *zTagFileList, char *zTmpTagFile)
{
	char		*yTagFile;
	int		yRc;
	int		yNumTagFile;
	char		yTagFileList[512];

	memset((TagFileList *)GV_TagFiles, 0, sizeof(GV_TagFiles));
	sprintf(yTagFileList, "%s", zTagFileList);
	yRc = sRemoveWhitespace(yTagFileList);

	yNumTagFile = 0;
	yTagFile=(char *)strtok(yTagFileList, ",");
	while ( yTagFile )
	{
		if ( yNumTagFile >= MAX_TAG_FILES)
		{
			sprintf(Msg, TEL_MAX_TAG_FILES_EXCEEDED_MSG, yNumTagFile);
                	telVarLog(Mod, REPORT_NORMAL, 
				TEL_MAX_TAG_FILES_EXCEEDED, GV_Err, Msg);
			return(-1);
		}

		if(access(yTagFile, F_OK|R_OK) == -1)
		{
			sprintf(Msg, 
			"Failed to access tag file <%s>. errno=%d. [%s]",
					yTagFile, errno, strerror(errno));
                	telVarLog(Mod, REPORT_NORMAL, TEL_CANT_ACCESS_FILE, 
				GV_Err, Msg);
			return(-1);
		}

		if ((yRc=sAppendFile(Mod,yTagFile,zTmpTagFile,yNumTagFile))==-1)
		{
			return(-1);		/* message logged in routine */
		}
	
		yNumTagFile++;
		yTagFile=(char *)strtok(NULL, ",");
	}

	return(0);
} /* END: sBuildTagFile() */

/*------------------------------------------------------------------------------
Append sourceFile to destFile. Updates GV_TagFiles entries.
------------------------------------------------------------------------------*/
int sAppendFile(char *Mod, char *zSourceFile, char *zDestFile,int zTagFileIndex)
{
	int	yLineCounter;
	FILE	*yFdSource;
	FILE	*yFdDest;
	char	buf[512];

	if ((yFdSource = fopen(zSourceFile, "r")) == NULL)
	{
		sprintf(Msg, "Failed to open <%s> for reading. errno=%d. [%s]",
				zSourceFile, errno, strerror(errno));
                telVarLog(Mod, REPORT_NORMAL, TEL_CANT_OPEN_FILE, GV_Err, Msg);
		return(-1);
	}
	
	if ((yFdDest = fopen(zDestFile, "a")) == NULL)
	{
		sprintf(Msg, "Failed to open <%s> for writing. errno=%d. [%s]",
					zDestFile, errno, strerror(errno));
                telVarLog(Mod, REPORT_NORMAL, TEL_CANT_OPEN_FILE, GV_Err, Msg);
		fclose(yFdSource);
		return(-1);
	}

	sprintf(GV_TagFiles[zTagFileIndex].tagFile, "%s", zSourceFile);
	yLineCounter = 0;
	
	memset((char *)buf, 0, sizeof(buf));
	while( sReadLogicalLine(buf, sizeof(buf)-1, yFdSource) )
	{
		if ((fprintf(yFdDest, "%s", buf)) < 0)
		{
			sprintf(Msg, 
			"Failed to write <%s> to <%s>. errno=%d. [%s]",
				buf, zDestFile, errno, strerror(errno));
                	telVarLog(Mod, REPORT_NORMAL, TEL_CANT_WRITE_TO_FILE, 
				GV_Err, Msg);
			fclose(yFdSource);
			fclose(yFdDest);
			return(-1);
		}
		yLineCounter++;
	}

	fclose(yFdSource);
	fclose(yFdDest);

	GV_TagFiles[zTagFileIndex].numLines = yLineCounter;

	return(0);

} /* END: sAppendFile() */

/*------------------------------------------------------------------------------
Purpose: Reads from zFpTagFile, and loads all tag phrases and lists in 2 passes.
	Pass 1:  Anything that does not begin with ^ or [
	Pass 2:  Anything that does begin with ^
------------------------------------------------------------------------------*/
static int sLoadTagList(char *Mod, FILE *zFpTagFile, char *zTmpTagFile)
{
	int		yRc, i;
	char		yLine[INPUT_BUF_SIZE];
	char		*pTmpBuf;
	PhraseTagList	*pPrevTag;
	PhraseTagList	*pNowTag;
	char		yStrTag[MAX_TAG_SIZE];
	char		yPayload[MAX_PATH_SIZE]; /* 2nd data/prompt tag field */
	char		yTmpString[MAX_PATH_SIZE];
	int		yNumInstances;
	int		yLineNumber;
	int		yLogicalLine;
	char		yActualTagFile[256];
	char		*yEnvPtr;
	int		yTagType;
	int		yInputFormat;
	int		yOutputFormat;
	char		yInputFormatStr[MAX_TAG_SIZE];
	char		yOutputFormatStr[MAX_TAG_SIZE];
	char		yTagData[128];
	int		yTempInt;
	int		yNumTagsInList;
	char		yTempData[128];

	pPrevTag = &GV_AppTags;
	pNowTag = &GV_AppTags;
	pNowTag->nextTag = (struct phraseTagStruct *) NULL;
	/*
	 * First pass through the file: Read all the phrase labels, get the
	 * corresponding pathname and update the list.
	 */
#ifdef DEBUG
	fprintf(stdout,"TEL_LoadTags: Pass 1 begins\n"); fflush(stdout);
#endif
	memset((char *)yLine, 0, sizeof(yLine));
	yLineNumber = 0;
	while( sReadLogicalLine(yLine, sizeof(yLine)-1, zFpTagFile) )
	{
		yRc = sRemoveWhitespace(yLine);
		yLineNumber++;

		if ( (yLine[0] == '^' ) ||
		     (yLine[0] == '[' ) )
		{
			memset((char *)yLine, 0, sizeof(yLine));
			continue;
		}

		if ( ! strchr(yLine, '|') )
		{
			memset((char *)yLine, 0, sizeof(yLine));
			continue;
		}
		
		memset((char *)yStrTag, 0, sizeof(yStrTag));
		memset((char *)yPayload, 0, sizeof(yPayload));
		if(sscanf(yLine, "%[^|]|%[^|]|",yStrTag, yPayload) < 2)
		{
			memset((char *)yLine, 0, sizeof(yLine));
			continue;
		}
		
		if ( sDoesEntryAlreadyExist(Mod, yStrTag, yLineNumber, 1) )
		{
			return(-1);	/* Message is logged in routine */
		}
		
		/* For first pass, we process phrase or data tags */	
		sprintf(pNowTag->tagName, "%s", yStrTag);
		pNowTag->nTags = 1;

		if ((yRc = sIsThisADataTag(yPayload)) == -1)
		{
			sprintf(Msg, TEL_INVALID_TAG_FORMAT_TMP_MSG,
				zTmpTagFile, yLineNumber, yLine);
	          	telVarLog(Mod, REPORT_NORMAL,
				TEL_INVALID_TAG_FORMAT, GV_Err, Msg);
			return(-1);
		}

		if (yRc == 1)
		{
			/* Data Tags */
			sprintf(pNowTag->tag[0].tagName,"%s",pNowTag->tagName);
			pNowTag->tag[0].type = TAG_TYPE_DATA;
			yRc = sGetField(':', yPayload, 1, yTempData);
			sTranslateFormatToInt(yTempData, &yTempInt);
			pNowTag->tag[0].inputFormat=STRING; /* ?? */
			pNowTag->tag[0].inputFormat=yTempInt; /* fix ?? */

			yRc = sGetField(':', yPayload, 2, yTempData);
			sTranslateFormatToInt(yTempData, &yTempInt);
			pNowTag->tag[0].outputFormat = DIGIT; /* ?? */
			pNowTag->tag[0].outputFormat = yTempInt; /* fix ?? */

			pNowTag->tag[0].outputFormat = yTempInt;
			yRc = sGetField(':', yPayload, 3, yTempData);
			strcpy(pNowTag->tag[0].data,  yTempData);
		} 
		else
		{
			/* Phrase Tags */
			sprintf(pNowTag->tag[0].tagName,"%s",pNowTag->tagName);
			pNowTag->tag[0].type = TAG_TYPE_PHRASE;
			pNowTag->tag[0].inputFormat = PHRASE_FILE;
			pNowTag->tag[0].outputFormat = PHRASE;
			sprintf(pNowTag->tag[0].data, "%s", yPayload);
		}

#ifdef DEBUG
		fprintf(stdout,
			"Loaded tag: name=<%s>, nTags=%d. payload=<%s>\n",
				pNowTag->tagName, pNowTag->nTags, yPayload);

		for (i=0; i<pNowTag->nTags; i++)
		{
			fprintf(stdout,"\ttype: %d, in=%d, out=%d, data=<%s>\n",
			pNowTag->tag[i].type, pNowTag->tag[i].inputFormat, 
			pNowTag->tag[i].outputFormat,	pNowTag->tag[i].data);
		}
#endif

		/* Create another node, and assign it appropriately */
		pNowTag = (PhraseTagList *)calloc(1, sizeof(PhraseTagList));
		pPrevTag->nextTag = pNowTag;
		pPrevTag = pNowTag;

		memset((char *)yLine, 0, sizeof(yLine));
	}

/*
 * Second pass through the file: Locate all the menu list definitions,
 * obtain the list of phrase labels and assign all the phrase files into
 * the list.
 */
	rewind(zFpTagFile);
#ifdef DEBUG
	fprintf(stdout,"Pass 2 begins\n"); fflush(stdout);
#endif
	memset((char *)yLine, 0, sizeof(yLine));
	yLineNumber = 0;
	while( sReadLogicalLine(yLine, sizeof(yLine)-1, zFpTagFile) )
	{
		yLineNumber++;

		yRc = sRemoveWhitespace(yLine);

		if ( yLine[0] != '^' )
		{
			memset((char *)yLine, 0, sizeof(yLine));
			continue;
		}

		yRc = sNumCharInString(yLine, '^', &yNumInstances);
		if (yNumInstances > 1)
		{
			if ((yRc = sGetTagInfo(yLineNumber, yActualTagFile, 
							&yLogicalLine)) == -1)
			{
				sprintf(Msg, TEL_INVALID_TAG_FORMAT_TMP_MSG,
						zTmpTagFile,yLineNumber,yLine);

          		telVarLog(Mod, REPORT_NORMAL, 
					TEL_INVALID_TAG_FORMAT, GV_Err, Msg);
			}
			else
			{
				sprintf(Msg, TEL_INVALID_TAG_FORMAT_MSG,
					yActualTagFile, yLogicalLine, yLine);
                		telVarLog(Mod, REPORT_NORMAL, 
					TEL_INVALID_TAG_FORMAT, GV_Err, Msg);
			}
			return(-1);
		}

		pTmpBuf = (char *)strtok(yLine, "=");
		if(pTmpBuf[1] == '\0')
		{
			if ((yRc = sGetTagInfo(yLineNumber, yActualTagFile, 
							&yLogicalLine)) == -1)
			{
				sprintf(Msg, TEL_EMPTY_LIST_TAG_TMP_MSG,
					pTmpBuf, pNowTag->tagName, zTmpTagFile,
							 yLineNumber);
                		telVarLog(Mod, REPORT_NORMAL, 
					TEL_EMPTY_LIST_TAG, GV_Err, Msg);
			}
			else
			{
				sprintf(Msg, TEL_EMPTY_LIST_TAG_MSG,
					pTmpBuf, yActualTagFile, yLogicalLine);
                		telVarLog(Mod, REPORT_NORMAL, 
					TEL_EMPTY_LIST_TAG, GV_Err, Msg);
			}
			return(-1);
		}

		if ( sDoesEntryAlreadyExist(Mod, &(pTmpBuf[1]),yLineNumber, 1))
		{
			return(-1);	/* Message is logged in routine */
		}

#ifdef DEBUG	
		fprintf(stdout,"Pass 2 processing  <%s>\n", yLine);
		fflush(stdout);
#endif
		sprintf(pNowTag->tagName, "%s", &(pTmpBuf[1]) );


		pTmpBuf = (char *)strtok(NULL, ",");

		pNowTag->nTags=0;
		yNumTagsInList=0;
		/* Go thru the comma, delimited list of tokens */
		while ( pTmpBuf )
		{
			yNumTagsInList++;
			if (yNumTagsInList > MAX_TAG_LIST)
			{
				sprintf(Msg, TEL_TOO_MANY_ITEMS_IN_LIST_TAG_MSG,
				yNumTagsInList, pNowTag->tagName, MAX_TAG_LIST);
                		telVarLog(Mod, REPORT_NORMAL, 
				  TEL_TOO_MANY_ITEMS_IN_LIST_TAG, GV_Err, Msg); 
				return(-1);
			}
			pNowTag->nextTag = (PhraseTagList *) NULL;
			sprintf(yTagData, pTmpBuf); 
			yRc = sGetVariablesForTag(yTagData, yPayload, 
				&yTagType, &yInputFormat, &yOutputFormat);
			if (yRc == -1)
			{
				if ((yRc = sGetTagInfo(yLineNumber, 
					yActualTagFile, &yLogicalLine)) == -1)
				{
					sprintf(Msg, 
						TEL_BAD_PHRASE_MENU_TMP_MSG,
						yTagData, pNowTag->tagName,
						zTmpTagFile, yLineNumber);
                			telVarLog(Mod, REPORT_NORMAL, 
					 TEL_BAD_PHRASE_MENU_TMP, GV_Err, Msg); 
				}
				else
				{
					sprintf(Msg, 
						TEL_BAD_PHRASE_MENU_MSG,
						yTagData, pNowTag->tagName,
						yActualTagFile, yLogicalLine);
                			telVarLog(Mod, REPORT_NORMAL, 
					TEL_BAD_PHRASE_MENU, GV_Err, Msg); 
				}
				return(-1);
			}
		   sprintf(pNowTag->tag[pNowTag->nTags].tagName,"%s",yTagData);
			pNowTag->tag[pNowTag->nTags].type = yTagType;
			pNowTag->tag[pNowTag->nTags].inputFormat=yInputFormat;
			pNowTag->tag[pNowTag->nTags].outputFormat=yOutputFormat;
		       sprintf(pNowTag->tag[pNowTag->nTags].data,"%s",yPayload);
#ifdef DEBUG
			fprintf(stdout,"Got Type=%d, Payload=<%s> \n",
				yTagType, yPayload); fflush(stdout);
#endif		
			pNowTag->nTags += 1;
			pTmpBuf = (char *)strtok(NULL, ",");
		}
		pNowTag = (PhraseTagList *)calloc(1, sizeof(PhraseTagList));
		pPrevTag->nextTag = pNowTag;
		pPrevTag = pNowTag;
		memset((char *)yLine, 0, sizeof(yLine));
	}

	pNowTag->nextTag = (PhraseTagList *)NULL;
	return(0);
} /* END: sLoadTagList() */

/*------------------------------------------------------------------------------
sIsThisADataTag();
	Determine, from zData, where it is a data tag, or not.  If it is not, then
	it is a phrase tag.  A data tag requires that two colons (:) appear in the 
	string.

	Return values:
	1:		String is a data tag
	0:		String is not a data tag
	-1:		String is invalid
------------------------------------------------------------------------------*/
static int sIsThisADataTag(char *zData)
{
	int		yCounter;
	int		yIndex;
	int		yLen;

	if (zData[0] == '\0')
	{
		return(-1);
	}

	yLen = strlen(zData);
	yCounter = 0;
	for (yIndex = 0; yIndex < yLen; yIndex++)
	{
		if (zData[yIndex] == ':')
		{
			yCounter++;
		}
	}

	if (yCounter == 2)
	{
		return(1);
	}

	return(0);
} /* END: sIsThisADataTag() */

/*------------------------------------------------------------------------------
Reads from zFpTagFile, and loads all the section names and values.
------------------------------------------------------------------------------*/
static int sLoadPncSectionVariables(char *Mod, FILE *zFpTagFile, char *zTmpTagFile)
{
	PncInput	*pPrevPncInput;
	PncInput	*pNowPncInput;
	int		yRc;
	char		ySectionName[128];
	char		ySectionInfo[2048];
	long		yBufSize;
	char		yErrorMsg[256];
	int		yBytesRead;
	char		*pNameValue;
	char		*pEqualSign;
	char		yName[128];
	char		yValue[256];
	int		yNameValueLen;
	char		yFunctionRc=0;

	memset((char *)ySectionName, 0, sizeof(ySectionName));
	memset((char *)ySectionInfo, 0, sizeof(ySectionInfo));
	memset((char *)yErrorMsg, 0, sizeof(yErrorMsg));

	yBufSize = (long)sizeof(ySectionInfo) - 1;
	pPrevPncInput = pNowPncInput = &GV_PncInput;
	pNowPncInput->nextSection = (PncInput *) NULL;

	rewind(zFpTagFile);

	while ((yBytesRead = sGetNextSectionInfo(zFpTagFile, ySectionName,
				ySectionInfo, yBufSize, yErrorMsg)) > 0)
	{	
		
		pNameValue = ySectionInfo;
		sprintf(pNowPncInput->sectionName, "%s", ySectionName);
		sSetPncSectionVariableDefaults(Mod, pNowPncInput);
		memset(g_party,			'\0',sizeof(g_party));
		memset(g_beep,			'\0',sizeof(g_beep));
		memset(g_terminateOption, 	'\0',sizeof(g_terminateOption));
		memset(g_inputOption, 		'\0',sizeof(g_inputOption));
		memset(g_interruptOption,      	'\0',sizeof(g_interruptOption));
		for (;;)
		{
			if ((pEqualSign = strchr(pNameValue, '=')) == NULL)
			{
				sprintf(Msg, TEL_BAD_NAMEVALUE_FORMAT_MSG,
					pNameValue, ySectionName, zTmpTagFile);
                		telVarLog(Mod, REPORT_NORMAL, 
					TEL_BAD_NAMEVALUE_FORMAT, GV_Err, Msg);
				yFunctionRc=-1;
				continue;
			}

			memset((char *)yName, 0, sizeof(yName));
			memset((char *)yValue, 0, sizeof(yValue));
	
			yRc = sGetField('=', pNameValue, 1, yName);
			yRc = sGetField('=', pNameValue, 2, yValue);

			if ((yRc = sSetPncSectionVariableValue(Mod, yName, 
				yValue, ySectionName, zTmpTagFile, 
				pNowPncInput)) == -1)
			{
				/* message is logged in routine. */
				yFunctionRc=-1;
				continue;
			}

			/* Process the next item line in the section */

			yNameValueLen = strlen(pNameValue);
			pNameValue += yNameValueLen;
			if ( (*pNameValue == '\0') &&
			     (*(pNameValue+1) == '\0') )
			{
				break;
			}
			pNameValue++;
		}

		if ((yRc = sValidatePncSectionVariables(Mod,
					pNowPncInput,zTmpTagFile))!= 0)
		{
			/* messages for all errors logged in routine */
			yFunctionRc=-1;
			/* Let it fall thru to evaluate subsquent sections.*/	
		}
		/* create another node, and assign it appropriately */
		pNowPncInput = (PncInput *)calloc(1, sizeof(PncInput));
		pPrevPncInput->nextSection = pNowPncInput;
		pPrevPncInput = pNowPncInput;
	}

	return(yFunctionRc);

} /* END: sLoadPncSectionVariables() */

/*------------------------------------------------------------------------------
Set those fields of the section that we will allow to take default values.
These fields will not have to be present in a section. 
Return values:
	0:	Success
------------------------------------------------------------------------------*/
static int sSetPncSectionVariableDefaults(char *Mod, PncInput *zNowPncInput)
{
	#define default_firstDigitTimeout 5
	#define default_interDigitTimeout 5
	#define default_nTries 1
	#define default_terminateKey '#'
	#define default_beep NO
	
	/* Additional defaults added 3/9/2001 */
	#define default_party FIRST_PARTY
	#define default_minLen  1
	#define default_maxLen  1
	#define default_inputOption  NUMERIC
	#define default_interruptOption  FIRST_PARTY_INTERRUPT

	sprintf(zNowPncInput->validKeys, "%s", "0123456789");
	sprintf(zNowPncInput->hotkeyList, "%s", "");
	zNowPncInput->firstDigitTimeout = default_firstDigitTimeout;
	zNowPncInput->interDigitTimeout = default_interDigitTimeout;
	zNowPncInput->nTries = 		default_nTries; 
	zNowPncInput->beep = 		default_beep; 
	zNowPncInput->terminateKey = 	default_terminateKey;

	/* Additional defaults added 3/9/2001 */
	zNowPncInput->party = default_party;
	zNowPncInput->minLen = default_minLen;
	zNowPncInput->maxLen = default_maxLen;
	zNowPncInput->inputOption = default_inputOption;
	zNowPncInput->interruptOption = default_interruptOption;

	
	return(0);
}

/*------------------------------------------------------------------------------
Populate the correct field of zNowPncInput with zValue, based on zName.

Return values:
	0:	Success
	-1:	Failure
------------------------------------------------------------------------------*/
static int sSetPncSectionVariableValue(char *Mod, char *zName, char *zValue, 
		char *zSectionName, char *zFilename, PncInput *zNowPncInput)
{

	if (strcmp(zName, NV_PROMPT_TAG) == 0)
	{
		sprintf(zNowPncInput->promptTag, "%s", zValue);
	}
	else if (strcmp(zName, NV_REPROMPT_TAG) == 0)
	{
		sprintf(zNowPncInput->repromptTag, "%s", zValue);
	}
	else if (strcmp(zName, NV_INVALID_TAG) == 0)
	{
		sprintf(zNowPncInput->invalidTag, "%s", zValue);
	}
	else if (strcmp(zName, NV_TIMEOUT_TAG) == 0)
	{
		sprintf(zNowPncInput->timeoutTag, "%s", zValue);
	}
	else if (strcmp(zName, NV_SHORT_INPUT_TAG) == 0)
	{
		sprintf(zNowPncInput->shortInputTag, "%s", zValue);
	}
	else if (strcmp(zName, NV_OVERFLOW_TAG) == 0)
	{
		sprintf(zNowPncInput->overflowTag, "%s", zValue);
	}
	else if (strcmp(zName, NV_VALID_KEYS) == 0)
	{
		sprintf(zNowPncInput->validKeys, "%s", zValue);
	}
	else if (strcmp(zName, NV_HOTKEY_LIST) == 0)
	{
		sprintf(zNowPncInput->hotkeyList, "%s", zValue);
	}
	else if (strcmp(zName, NV_PARTY) == 0)
	{
		sTranslateFormatToInt(zValue, &(zNowPncInput->party));
		strcpy(g_party,zValue);
	}
	else if (strcmp(zName, NV_FIRST_DIGIT_TIMEOUT) == 0)
	{
		zNowPncInput->firstDigitTimeout = atoi(zValue);
	}
	else if (strcmp(zName, NV_INTER_DIGIT_TIMEOUT) == 0)
	{
		zNowPncInput->interDigitTimeout = atoi(zValue);
	}
	else if (strcmp(zName, NV_NTRIES) == 0)
	{
		zNowPncInput->nTries = atoi(zValue);
	}
	else if (strcmp(zName, NV_BEEP) == 0)
	{
		sTranslateFormatToInt(zValue, &(zNowPncInput->beep));
		strcpy(g_beep, zValue);
	}
	else if (strcmp(zName, NV_TERMINATE_KEY) == 0)
	{
		zNowPncInput->terminateKey = zValue[0];
	}
	else if (strcmp(zName, NV_MIN_LEN) == 0)
	{
		zNowPncInput->minLen = atoi(zValue);
	}
	else if (strcmp(zName, NV_MAX_LEN) == 0)
	{
		zNowPncInput->maxLen = atoi(zValue);
	}
	else if (strcmp(zName, NV_TERMINATE_OPTION) == 0)
	{
		sTranslateFormatToInt(zValue, &(zNowPncInput->terminateOption));
		strcpy(g_terminateOption, zValue);
	}
	else if (strcmp(zName, NV_INPUT_OPTION) == 0)
	{
		sTranslateFormatToInt(zValue, &(zNowPncInput->inputOption));
		strcpy(g_inputOption, zValue);
	}
	else if (strcmp(zName, NV_INTERRUPT_OPTION) == 0)
	{
		sTranslateFormatToInt(zValue, &(zNowPncInput->interruptOption));
		strcpy(g_interruptOption,zValue);
	}
	else
	{
		sprintf(Msg, TEL_UNKNOWN_SECTION_PARM_MSG,
			zName, zValue, zSectionName, zFilename);
                telVarLog(Mod, REPORT_NORMAL, 
			TEL_UNKNOWN_SECTION_PARM, GV_Err, Msg);
		return(-1);
	}
	return(0);
} /* END: sSetPncSectionVariableValue() */

/*------------------------------------------------------------------------------
Validate each parameter of the function. When errors are encountered, 
continue processing variables until the end so we get all the error mesages
writen.
Return values:
	0:	Success
	-1:	Failure
------------------------------------------------------------------------------*/
static int sValidatePncSectionVariables(char *Mod, PncInput *zPncInput, 
							char *zFilename)
{
	int		yRc;
	int		yFunctionRc=0;
	int		yTagType;
	char		yInputFormat[64];
	char		yOutputFormat[64];
	char		yData[128];
	char		yBadValue[256];

	/* promptTag */
	if (zPncInput->promptTag[0] == '\0')
	{
		sprintf(Msg, TEL_NULL_TAG_MSG, zPncInput->promptTag, 
				zPncInput->sectionName, NV_PROMPT_TAG);
                telVarLog(Mod, REPORT_NORMAL, TEL_NULL_TAG, GV_Err, Msg);
		yFunctionRc=-1;
	}
	else
	{
		if ((yRc = sDoesEntryAlreadyExist(Mod, 
				zPncInput->promptTag, 0, 0)) == 0)
		{
			sprintf(Msg, TEL_NO_DEFINED_TAG_MSG,
				NV_PROMPT_TAG, zPncInput->promptTag, 
				zPncInput->sectionName, zPncInput->promptTag);
                	telVarLog(Mod, REPORT_NORMAL, TEL_NO_DEFINED_TAG, 
				GV_Err, Msg);
			yFunctionRc=-1;
		}
	}

	/* repromptTag */
	if (zPncInput->repromptTag[0] != '\0')
	{
		if ((yRc = sDoesEntryAlreadyExist(Mod, 
					zPncInput->repromptTag, 0, 0)) == 0)
		{
			sprintf(Msg, TEL_NO_DEFINED_TAG_MSG,
				NV_REPROMPT_TAG, zPncInput->repromptTag,
				zPncInput->sectionName, zPncInput->repromptTag);
                	telVarLog(Mod, REPORT_NORMAL, 
				TEL_NO_DEFINED_TAG, GV_Err, Msg);
			yFunctionRc=-1;
		}
	}

	/* invalidTag */
	if (zPncInput->invalidTag[0] != '\0')
	{
		if ((yRc = sDoesEntryAlreadyExist(Mod, 
					zPncInput->invalidTag, 0, 0)) == 0)
		{
			sprintf(Msg, TEL_NO_DEFINED_TAG_MSG,
				NV_INVALID_TAG, zPncInput->invalidTag,
			zPncInput->sectionName, zPncInput->invalidTag);
                	telVarLog(Mod, REPORT_NORMAL, TEL_NO_DEFINED_TAG, 
				GV_Err, Msg);
			yFunctionRc=-1;
		}
	}

	/* timeoutTag */
	if (zPncInput->timeoutTag[0] != '\0')
	{
		if ((yRc = sDoesEntryAlreadyExist(Mod, zPncInput->timeoutTag, 
			0, 0)) == 0)
		{
			sprintf(Msg, TEL_NO_DEFINED_TAG_MSG,
				NV_TIMEOUT_TAG, zPncInput->timeoutTag,
				zPncInput->sectionName, zPncInput->timeoutTag);
                	telVarLog(Mod, REPORT_NORMAL, TEL_NO_DEFINED_TAG, 
				GV_Err, Msg);
			yFunctionRc=-1;
		}
	}

	/* shortInputTag */
	if (zPncInput->shortInputTag[0] != '\0')
	{
		if ((yRc = sDoesEntryAlreadyExist(Mod, 
				zPncInput->shortInputTag, 0, 0)) == 0)
		{
			sprintf(Msg, TEL_NO_DEFINED_TAG_MSG,
				NV_SHORT_INPUT_TAG, zPncInput->shortInputTag,
			       zPncInput->sectionName,zPncInput->shortInputTag);
                	telVarLog(Mod, REPORT_NORMAL, TEL_NO_DEFINED_TAG, 
				GV_Err, Msg);
			yFunctionRc=-1;
		}
	}
	
	/* overflowTag */
	if (zPncInput->overflowTag[0] != '\0')
	{
		if ((yRc = sDoesEntryAlreadyExist(Mod, 
				zPncInput->overflowTag, 0, 0)) == 0)
		{
			sprintf(Msg, TEL_NO_DEFINED_TAG_MSG,
				NV_OVERFLOW_TAG, zPncInput->overflowTag,
			       zPncInput->sectionName,zPncInput->overflowTag);
                	telVarLog(Mod, REPORT_NORMAL, TEL_NO_DEFINED_TAG, 
				GV_Err, Msg);
			yFunctionRc=-1;
		}
	}

	if (zPncInput->validKeys[0] == '\0')
	{
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG, 
			NV_VALID_KEYS, zPncInput->validKeys, 
			zPncInput->sectionName, 
			"non-empty string made up of 0-9, *, #");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	if (validate_party(zPncInput->party) != 0)
	{
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG,
			 NV_PARTY, g_party, 
			zPncInput->sectionName,"FIRST_PARTY, SECOND_PARTY");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	if (validate_range(zPncInput->firstDigitTimeout,1,60) !=0 )
	{
		sprintf(yBadValue, "%d", zPncInput->firstDigitTimeout);
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG, 
			NV_FIRST_DIGIT_TIMEOUT, yBadValue,
			zPncInput->sectionName, "1-60");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}
	
	if (validate_range(zPncInput->interDigitTimeout,1,60) !=0 )
	{
		sprintf(yBadValue, "%d", zPncInput->interDigitTimeout);
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG, 
			NV_INTER_DIGIT_TIMEOUT, yBadValue,
			zPncInput->sectionName, "1-60");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	if (validate_range(zPncInput->nTries, 0, 15) != 0)
	{
		sprintf(yBadValue, "%d", zPncInput->nTries);
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG, 
			NV_NTRIES, yBadValue, 
			zPncInput->sectionName, "0-15");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	if (validate_beep(zPncInput->beep) != 0)
	{
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG, 
			NV_BEEP, g_beep, 
			zPncInput->sectionName, "YES, NO");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	if (validate_range(zPncInput->minLen, 0, 35) != 0)
	{
		sprintf(yBadValue,"%d", zPncInput->minLen);
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG, 
				NV_MIN_LEN, yBadValue,
				zPncInput->sectionName, "0-35");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	if (validate_range(zPncInput->maxLen, 0, 35) != 0)
	{
		sprintf(yBadValue,"%d", zPncInput->maxLen);
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG, 
			NV_MAX_LEN, yBadValue, 
			zPncInput->sectionName,"0-35");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	if(zPncInput->minLen > zPncInput->maxLen)
	{
		sprintf(yBadValue,"%d", zPncInput->minLen);
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG, 
			NV_MIN_LEN, yBadValue,
			zPncInput->sectionName, 
			"any value not greater than maxLen");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	switch(zPncInput->terminateOption)
	{
		case MANDATORY:
		case AUTOSKIP:
			if( (zPncInput->terminateKey != '#') &&
					(zPncInput->terminateKey != '*'))
			{
				if( (zPncInput->terminateKey < '0') || 
			 	    (zPncInput->terminateKey > '9'))
				{
					sprintf(yBadValue,"%c",
						zPncInput->terminateKey);
					sprintf(Msg, 
						TEL_INVALID_SECTION_PARM_MSG,
						NV_TERMINATE_KEY, yBadValue,
						zPncInput->sectionName,
						"0-9, *, #");
                			telVarLog(Mod, REPORT_NORMAL, 
						TEL_INVALID_SECTION_PARM, 
						GV_Err, Msg);
					yFunctionRc=-1;
				}
			}
			break;
		case AUTO:
			break;
		default:
			sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG,
				NV_TERMINATE_OPTION, g_terminateOption,
				zPncInput->sectionName, 
				"MANDATORY, AUTO, AUTOSKIP");
                	telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM,
				GV_Err, Msg);
			yFunctionRc=-1;
			break;
	}

	if (validate_inputOption(zPncInput->inputOption) != 0)
	{
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG,
			NV_INPUT_OPTION, g_inputOption,
			zPncInput->sectionName,
			"NUMERIC, NUMSTAR, DOLLAR, ALPHANUM, ALPHA");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	if (validate_interruptOption(zPncInput->interruptOption) != 0)
	{
		sprintf(Msg, TEL_INVALID_SECTION_PARM_MSG,
			NV_INTERRUPT_OPTION, g_interruptOption, 
			zPncInput->sectionName,
	"NONINTERRUPT, FIRST_PARTY_INTERRUPT, SECOND_PARTY_INTERRUPT");
                telVarLog(Mod, REPORT_NORMAL, TEL_INVALID_SECTION_PARM, 
			GV_Err, Msg);
		yFunctionRc=-1;
	}

	return(yFunctionRc);
} /* END: sValidatePncSectionVariables() */

int validate_range(int value, int min, int max)
{
		
	if (value < min || value > max) return(-1);
	return(0);
}


int validate_beep(int value)
{	
	switch(value)
	{
	case YES:
	case NO:
		return(0);
	default:
		return(-1);
	}
}

int validate_inputOption(int value)
{	
	fflush(stdout);	
	switch(value)
	{
		case NUMERIC:
		case NUMSTAR:
		case DOLLAR:
		case ALPHANUM:
		case ALPHA:
			return(0);
			break;
		default:
			return(-1);
	}
}

int validate_party(int value)
{
	switch(value)
	{
		case FIRST_PARTY:
		case SECOND_PARTY:
			return(0);
		default:
			return(-1);
			break;
	}
}

int validate_interruptOption(int value)
{
	switch(value)
	{
		case FIRST_PARTY_INTERRUPT:
		case SECOND_PARTY_INTERRUPT:
		case NONINTERRUPT:
		case FIRST_PARTY_PLAYBACK_CONTROL:
		case SECOND_PARTY_PLAYBACK_CONTROL:
			return(0);
			break;
		default:
			return(-1);
			break;
	}
}

/*------------------------------------------------------------------------------
Based on the line number, zLineNumber,  determine the tag 
		file and the line number associated with it.
------------------------------------------------------------------------------*/
static int sGetTagInfo(int zLineNumber, char *zTagFile, int *zTagLineNumber)
{
	int	yCounter;
	int	yTallyLines;
	int	yBeginningOfFile;

	yCounter = 0;
	yTallyLines = 0;
	yBeginningOfFile = 0;

	while ( GV_TagFiles[yCounter].tagFile[0] != '\0' )
	{
		yTallyLines += GV_TagFiles[yCounter].numLines;

		if (zLineNumber > yTallyLines)
		{
			yBeginningOfFile += GV_TagFiles[yCounter].numLines;
			yCounter++;
			continue;
		}

		sprintf(zTagFile, "%s", GV_TagFiles[yCounter].tagFile);
		*zTagLineNumber = zLineNumber - yBeginningOfFile;
		return(0);
	}
	return(-1);
} /* END: sGetTagInfo() */

/*------------------------------------------------------------------------------
sDoesEntryAlreadyExist():
	Determine if the zPhraseTag exists is the loaded tags.
	If a duplicate is found, then log an error message.
	
	Return Values:
		1:		Duplicate exists
		0: 		No duplicates found
------------------------------------------------------------------------------*/
static int sDoesEntryAlreadyExist(char *Mod, char *zPhraseTag, int zLineNumber, int zLog)
{
	PhraseTagList	*yTmpTag;
	char		yActualTagFile[256];
	int			yLogicalLine;
	int			yRc;

	yTmpTag = &GV_AppTags;
	while ( yTmpTag->nextTag != (PhraseTagList *)NULL )
	{
		if ( strcmp(yTmpTag->tagName, zPhraseTag) == 0 )
		{
			yActualTagFile[0] = '\0';
			yRc = sGetTagInfo(zLineNumber, yActualTagFile, &yLogicalLine);
			if (zLog)
			{
				sprintf(Msg, TEL_DUPLICATE_TAG_MSG, zPhraseTag, yActualTagFile);
           		telVarLog(Mod, REPORT_NORMAL, TEL_DUPLICATE_TAG, GV_Warn, Msg); 
			}
			return(1);
		}
		yTmpTag = yTmpTag->nextTag;
	}

	return(0);
} /* END: sDoesEntryAlreadyExist() */

/*------------------------------------------------------------------------------
 * sGetField(): 	This routine extracts the value of the desired
 *				fieldValue from the data record.
 * 
 * Output:
 * 	-1	: Error
 * 	>= 0	: Length of fieldValue extracted
 *----------------------------------------------------------------------------*/
int sGetField(char zDelimiter, char *zInput, int zFieldNum,
						 char *zFieldValue)
{
	int	yCounter;
	int	yFieldLen, yState, yWordCount;
	int	yInputLen;

	yFieldLen = yState = yWordCount = 0;

	zFieldValue[0] = '\0';

	yInputLen = (int)strlen(zInput);
	for (yCounter=0; yCounter < yInputLen; yCounter++) 
	{
       	if(zInput[yCounter] == zDelimiter || zInput[yCounter] == '\n')
		{
			yState = 0;
			if ( (zInput[yCounter] == zDelimiter) &&
			     (zInput[yCounter-1] == zDelimiter) )
				{
					++yWordCount;
				}
		}
		else if (yState == 0)
		{
			yState = 1;
			++yWordCount;
		}
		if (zFieldNum == yWordCount && yState == 1)
		{
			zFieldValue[yFieldLen++] = zInput[yCounter];
		}
		if (zFieldNum == yWordCount && yState == 0)
		{
			break;
		}
	} /* for */

	if(yFieldLen > 0)
	{
		zFieldValue[yFieldLen] = '\0';
		while(zFieldValue[0] == ' ')
		{
			for (yCounter = 0; zFieldValue[yCounter] != '\0'; yCounter++)
			{
				zFieldValue[yCounter] = zFieldValue[yCounter+1];
			}
		}
		yFieldLen = strlen(zFieldValue);
		return (yFieldLen);
	}

	return(-1);
} /* END: sGetField() */

/*------------------------------------------------------------------------------
Look for the specified section in a file and put the contents of this 
	section in a buffer.

The returned string will look like:

"line 1\0line 2\0line 3\0....line n\0\0"
------------------------------------------------------------------------------*/
static int sGetNextSectionInfo(FILE *zFp, char *zSectionName, char *zValue, 
				long zBufSize, char *zErrorMsg)
{
	char		*pTmp, yLine[1024];
	short		yInSection;
	long		yBytesCopied, yBytesToCopy;
	long		yCurrentOffset;
	long		yEndOffset;

	zSectionName[0] = '\0';

	if((pTmp = (char *)malloc(zBufSize)) == NULL)
	{
		sprintf(zErrorMsg, "malloc(%ld) failed, (%d:%s)", zBufSize, 
				errno, strerror(errno));
		return(-1);
	}

	memset(pTmp, 0, zBufSize);

	yBytesCopied = 0;
	yInSection = 0;
	memset((char *)yLine, 0, sizeof(yLine));
	yEndOffset = ftell(zFp);
	while(sReadLogicalLine(yLine, sizeof(yLine)-1, zFp))
	{
		yCurrentOffset = ftell(zFp);
		/*
		 * Strip \n from the yLine if it exists
		 */
		if(yLine[(int)strlen(yLine)-1] == '\n')
		{
			yLine[(int)strlen(yLine)-1] = '\0';
		}

		sRemoveWhitespace(yLine);

		/*
		 * If we are not in a zSectionName, check to see if we find the 
		 * zSectionName that we are looking for.
		 */
		if(!yInSection)
		{
			if(yLine[0] != '[')
			{
				memset((char *)yLine, 0, sizeof(yLine));
				continue;
			}

			sscanf(&yLine[1], "%[^]]", zSectionName);

			yInSection = 1;
		} /* if ! yInSection */
		else
		{
			/*
			 * If we are already in a zSectionName and have 
			encountered  another zSectionName, get out of the loop.
			 */
			if(yLine[0] == '[')
			{
				fseek(zFp, yEndOffset, 0L);
				yInSection = 0;
				break;
			}
		
			/*
			 * Calculate how many bytes we can copy
			 */
			if(((int)strlen(yLine) + yBytesCopied) > (zBufSize - 2))
			{
				yBytesToCopy = (zBufSize - 2) - yBytesCopied;
			}
			else
			{
				yBytesToCopy = strlen(yLine);
			}
	
			/*
			 * Append the current yLine to the pTmp 
			  The variable yBytesCopied is our index into the buffer
			 */
			sprintf(&pTmp[yBytesCopied],"%.*s",yBytesToCopy,yLine);
	
			/* Increment the number of bytes copied so far into 
				the pTmp */
			yBytesCopied += yBytesToCopy;
	
			/*
			 * Skip a place - this is our \0 after each string
			 */
			yBytesCopied++;
	
			/* If we have exhausted our buffer, append two nulls 
				at the end and break out of the loop.
			 */
			if((yBytesCopied+1) >= zBufSize)
			{
				pTmp[zBufSize-1] = '\0';
				pTmp[zBufSize-2] = '\0';
				break;
			}
		} /* if yInSection */

		memset((char *)yLine, 0, sizeof(yLine));
		yEndOffset = ftell(zFp);
	} /* while myReadLine != NULL */

	if (yBytesCopied > 0)
	{
		sprintf(&pTmp[yBytesCopied], "%s", "\0");
		yBytesCopied += 1;
	}

	memcpy(zValue, pTmp, zBufSize);
	free(pTmp);

	return(yBytesCopied);
} /* END: sGetProfileSection() */

/*------------------------------------------------------------------------------
Given a tag, this function searchs through the tag list and returns
	the data associated with that tag. 

Return Codes:
 0:	Success
-1:	tagName not found

NOTE: This function has a "fatal" bug in it. If you pass it a tag that does
	not exist, it will search through memory pretty much forever looking
	for it. At present, there I am not aware of and don't have the time
	to look for a variable that tells me that there can be no more
	tags to look for, so I have hardcoded a "very high" limit. If I 
	do not find the tag of interest before I hit this limit, I consider
	it not there. 
	The condition that causes this "forever search" is not difficult to
	construct. Simply put a tag in a list tag that does not exist or
	fail to put a comma between two items in a list tag.  gpw 4/17/2001
------------------------------------------------------------------------------*/
static int sGetVariablesForTag(char *zTagName, char *zTagData, int *zTagType,
				int *zInputFormat, int *zOutputFormat)
{
	PhraseTagList	*yTmpTag;
	int count=0; 	
	int limit=1500;

	yTmpTag = &GV_AppTags;

	while ( yTmpTag->nextTag != (PhraseTagList *)NULL )
	{
		count++;
		if (count > limit) return(-1);

		if ( yTmpTag->nTags != 1 )
		{
			continue;
		}

		if ( strcmp(yTmpTag->tagName, zTagName) == 0 )
		{
			sprintf(zTagData, "%s", yTmpTag->tag[0].data);
			*zTagType=yTmpTag->tag[0].type;
			*zInputFormat=yTmpTag->tag[0].inputFormat;
			*zOutputFormat=yTmpTag->tag[0].outputFormat;
			return(0);
		}

		yTmpTag = yTmpTag->nextTag;
	}
	zTagData[0] = '\0';
	return(-1);

} /* END: sGetVariablesForTag() */

/*------------------------------------------------------------------------------
 * sReadLogicalLine():
 * 		Read uncommented lines (lines not beginning with a '#') and
 * 		append all lines ending with a '\' into one line.
 * 
 * Dependencies: 	None.
 * 
 * Input:
 * 	zFp		: file pointer to the file being read
 * 	zBuf		: buffer in which the line will be read into
 * 	zBufSize		: maximun number of characters to be read
 * 
 * Output:
 * 	-1	: Error
 * 	>= 0	: number of characters read.
 *----------------------------------------------------------------------------*/
static int sReadLogicalLine(char *zBuf, int zBufSize, FILE *zFp)
{
	int	yRc;
	char	yStr[BUFSIZ];
	char	yTmpStr[BUFSIZ];
	char	*yPtr;

	memset(yStr, 0, sizeof(yStr));
	memset(zBuf, 0, zBufSize);

	while(fgets(yStr, sizeof(yStr)-1, zFp))
	{
		/*
		 * skip empty lines
		 */
		if (yStr[0] == '\0' )
		{
			continue;
		}

		/*
		 * skip lines with only whitespace
		 */
		yPtr = yStr;
		while(isspace(*yPtr))
		{
			yPtr++;
		}
		if(yPtr[0] == '\0' )
		{
			continue;
		}

		/*
		 * skip lines beginning with a '#'
		 */
		if(yPtr[0] == '#')
		{
			continue;
		}

		/*
		 * Check if the last character in the yStr is a '\'
		 */
		if(yStr[strlen(yStr) - 2] == '\\')
		{
			sprintf(yTmpStr, "%s", zBuf);
			sprintf(zBuf,"%s%.*s",yTmpStr, strlen(yStr) - 2, yStr);
			continue;
		}

		if ( strlen(zBuf) == 0 )
		{
			sprintf(zBuf, "%s", yStr);
		}
		else
		{
			sprintf(yTmpStr, "%s", zBuf);
			sprintf(zBuf, "%s%s", yTmpStr, yStr);
		}

		break;
	}

	yRc = strlen(zBuf);

	return(yRc);
} /* END: sReadLogicalLine() */

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

/*----------------------------------------------------------------------------
sPrintPncInput(): Print the list of elements to PncInput
----------------------------------------------------------------------------*/
static int sPrintPncInput(char *Mod)
{
	PncInput	*pHoldPncInput;
	int	yCounter;

	pHoldPncInput = &GV_PncInput;
	while ( pHoldPncInput->nextSection != (PncInput *)NULL )
	{
		sprintf(Msg, 
		"Section %s: %s=%s,%s=%s,%s=%s,%s=%s,%s=%s,%s=%s,%s=%s.",
			pHoldPncInput->sectionName, 
			NV_PROMPT_TAG, pHoldPncInput->promptTag,
			NV_REPROMPT_TAG, pHoldPncInput->repromptTag,
			NV_INVALID_TAG, pHoldPncInput->invalidTag,
			NV_TIMEOUT_TAG, pHoldPncInput->timeoutTag,
			NV_SHORT_INPUT_TAG, pHoldPncInput->shortInputTag,
			NV_OVERFLOW_TAG, pHoldPncInput->overflowTag,
			NV_VALID_KEYS, pHoldPncInput->validKeys);
                telVarLog(Mod, REPORT_VERBOSE, TEL_BASE, GV_Info, Msg);

		sprintf(Msg,
			"Section %s: %s=%s,%s=%s,%s=%d,%s=%d,%s=%d,%s=%s.",
			pHoldPncInput->sectionName, 
			NV_HOTKEY_LIST, pHoldPncInput->hotkeyList,
			NV_PARTY, arc_val_tostr(pHoldPncInput->party),
			NV_FIRST_DIGIT_TIMEOUT,pHoldPncInput->firstDigitTimeout,
			NV_INTER_DIGIT_TIMEOUT,pHoldPncInput->interDigitTimeout,
			NV_NTRIES, pHoldPncInput->nTries,
			NV_BEEP, arc_val_tostr(pHoldPncInput->beep));
                telVarLog(Mod, REPORT_VERBOSE, TEL_BASE, GV_Info, Msg);

		sprintf(Msg, 
			"Section %s: %s=%c,%s=%d,%s=%d,%s=%s,%s=%s,%s=%s.",
			pHoldPncInput->sectionName, 
			NV_TERMINATE_KEY, pHoldPncInput->terminateKey,
			NV_MIN_LEN, pHoldPncInput->minLen,
			NV_MAX_LEN, pHoldPncInput->maxLen,
			NV_TERMINATE_OPTION, 
				arc_val_tostr(pHoldPncInput->terminateOption),
			NV_INPUT_OPTION, 
				arc_val_tostr(pHoldPncInput->inputOption),
			NV_INTERRUPT_OPTION, 
				arc_val_tostr(pHoldPncInput->interruptOption));
                telVarLog(Mod, REPORT_VERBOSE, TEL_BASE, GV_Info, Msg);

		pHoldPncInput = pHoldPncInput->nextSection;
	}

	return(0);
} /* END: sPrintPncInput() */

/*----------------------------------------------------------------------------
sPrintTags(): Print the list of phrase tags 
----------------------------------------------------------------------------*/
static int sPrintTags(char *Mod)
{
	PhraseTagList	*pHoldTags;
	int			yCounter;

	pHoldTags = &GV_AppTags;
	while ( pHoldTags->nextTag != (PhraseTagList *)NULL )
	{
		sprintf(Msg, "Tag name (%s) has %d tag(s).", 
						pHoldTags->tagName, pHoldTags->nTags);
		telVarLog(Mod, REPORT_VERBOSE, TEL_BASE, GV_Info, Msg);

		for (yCounter = 0; yCounter < pHoldTags->nTags; yCounter++)
		{
			sprintf(Msg, "   Tag list[%d]: type=(%d); inputFormat=(%d); "
						"outputFormat=(%d); data=(%s).", yCounter,
						pHoldTags->tag[yCounter].type,
						pHoldTags->tag[yCounter].inputFormat,
						pHoldTags->tag[yCounter].outputFormat,
						pHoldTags->tag[yCounter].data);

			telVarLog(Mod, REPORT_VERBOSE, TEL_BASE, GV_Info, Msg);
		}
		pHoldTags = pHoldTags->nextTag;
	}

	return(0);
} /* END: sPrintTags() */