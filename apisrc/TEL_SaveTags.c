/*-----------------------------------------------------------------------------
Program:	TEL_SaveTags.c
Purpose:	To save the currently loaded tag file(s).
Author: 	Aumtech, Inc.
Update:		11/28/00 djb	Created the file.
Update:		07/18/01 apj Ported to IP.
Update: 08/25/04 djb Changed sys_errlist strerror(errno).
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

static int sGetField(char zDelimiter, char *zInput, int zFieldNum, 
						char *zFieldValue);
static int sGetTagInfo(int zLineNumber, char *zTagFile, int *zTagLineNumber);
static int sReadLogicalLine(char *zBuf, int zBufSize, FILE *zFp);
static int sGetTagFromFile(char *zFilename, char *zPhraseTag);


/*------------------------------------------------------------------------------
TEL_SaveTags(): Save the loaded tags to zFilename.
------------------------------------------------------------------------------*/
int TEL_SaveTags(char *zFilename, int overwrite)
{
	FILE		*yFpOutput;
	PhraseTagList	*pTmpTag;
	int			yCounter;
	char		yPhraseTag[256];
	int			yRc;
	time_t		yTm;
	char		yTimeStamp[64];
	char		yDiagMsg[128];
	struct tm	*pTimePtr;
	PncInput	*pPncInput;
	static char Mod[32] = "TEL_SaveTags";
	char 		lMsg[256];

	sprintf(yDiagMsg,"%s(%s)", Mod, zFilename);

	yRc = apiInit(Mod, TEL_SAVETAGS, yDiagMsg, 1, 1, 0); 
	if (yRc != 0) HANDLE_RETURN(yRc);

	sprintf(Mod, "%s", "TEL_SaveTags");
	sprintf(LAST_API, "%s", Mod);

	time(&yTm);
	memset((char *)yTimeStamp, 0, sizeof(yTimeStamp));
    	pTimePtr = localtime(&yTm);

    	strftime(yTimeStamp, strlen(yTimeStamp), "%b %d %Y  %T", pTimePtr);

	if ((yFpOutput = fopen(zFilename, "w")) == NULL)
	{
		sprintf(lMsg, 
			"Failed to open file (%s) for output. [%d, %s]",
			zFilename, errno, strerror(errno));
               	// tel_log(Mod,REPORT_NORMAL, TEL_CANT_OPEN_FILE, lMsg); 
	    HANDLE_RETURN (-1);
	}

	fprintf(yFpOutput, 
		"#-----------------------------------------------------------------\n");
	fprintf(yFpOutput, "%-16s %s\n", "# File:", zFilename);
	fprintf(yFpOutput, "%-16s %s\n", "# Description:", "Phrase Tag File");
	fprintf(yFpOutput, "%-16s %s\n", "# Creation Time:", yTimeStamp);
	fprintf(yFpOutput, 
		"#-----------------------------------------------------------------\n");

	pTmpTag = &GV_AppTags;
	while ( pTmpTag->nextTag != (PhraseTagList *)NULL )
	{
		if ( pTmpTag->nTags == 1 )
		{
			fprintf(yFpOutput, "%-25s\t|%-20s\t|\n",
					pTmpTag->tagName, pTmpTag->tag[0].data);
		}
		pTmpTag = pTmpTag->nextTag;
	}

	fprintf(yFpOutput, "\n#\n# Menus\n#\n");

	pTmpTag = &GV_AppTags;
	while ( pTmpTag->nextTag != (PhraseTagList *)NULL )
	{
		if ( pTmpTag->nTags <= 1 )
		{
			pTmpTag = pTmpTag->nextTag;
			continue;
		}
		fprintf(yFpOutput, "^%s = \\\n", pTmpTag->tagName);

		for (yCounter = 0; yCounter < pTmpTag->nTags; yCounter++)
		{
			if (pTmpTag->tag[yCounter].type == TAG_TYPE_STRING)
			{
				if (yCounter == pTmpTag->nTags - 1)
				{
					fprintf(yFpOutput, "\t\t\t\t%s:%s!%s\t\n\n",
					arc_val_tostr(pTmpTag->tag[yCounter].inputFormat),
					arc_val_tostr(pTmpTag->tag[yCounter].outputFormat),
					pTmpTag->tag[yCounter].data);
				}
				else
				{
					fprintf(yFpOutput, "\t\t\t\t%s:%s|%s,\t\\\n",
					arc_val_tostr(pTmpTag->tag[yCounter].inputFormat),
					arc_val_tostr(pTmpTag->tag[yCounter].outputFormat),
					pTmpTag->tag[yCounter].data);
				}
				continue;
			}

			if ((yRc =
				sGetTagFromFile(pTmpTag->tag[yCounter].data, 
							yPhraseTag)) != 0)
			{
				sprintf(lMsg, TEL_NO_TAG_FROM_FILE_MSG,
					pTmpTag->tag[0].data, zFilename);
                		telVarLog(Mod, REPORT_NORMAL, 
					TEL_NO_TAG_FROM_FILE, GV_Err, Msg);
				fclose(yFpOutput);
				if(access(zFilename, F_OK) == 0)
				{
					unlink(zFilename);
				}
	    			HANDLE_RETURN (-1);
			}

			if (yCounter == pTmpTag->nTags - 1)
			{
				fprintf(yFpOutput, "\t\t\t\t%s\t\n\n", yPhraseTag);
			}
			else
			{
				fprintf(yFpOutput, "\t\t\t\t%s,\t\\\n", yPhraseTag);
			}
		}
		pTmpTag = pTmpTag->nextTag;
	}

	fprintf(yFpOutput, "\n#\n# Sections\n#\n");

	pPncInput = &GV_PncInput;
	while ( pPncInput->nextSection != (PncInput *)NULL )
	{
		fprintf(yFpOutput, "[%s]\n", pPncInput->sectionName);
		fprintf(yFpOutput, "%s=%s\n",
					NV_PROMPT_TAG, pPncInput->promptTag);
		fprintf(yFpOutput, "%s=%s\n",
				NV_REPROMPT_TAG, pPncInput->repromptTag);
		fprintf(yFpOutput, "%s=%s\n",
					NV_INVALID_TAG, pPncInput->invalidTag);
		fprintf(yFpOutput, "%s=%s\n",
					NV_TIMEOUT_TAG, pPncInput->timeoutTag);
		fprintf(yFpOutput, "%s=%s\n",
				NV_SHORT_INPUT_TAG, pPncInput->shortInputTag);
		fprintf(yFpOutput, "%s=%s\n",
					NV_VALID_KEYS, pPncInput->validKeys);
		fprintf(yFpOutput, "%s=%s\n",
					NV_HOTKEY_LIST, pPncInput->hotkeyList);
		fprintf(yFpOutput, "%s=%s\n",
				NV_PARTY, arc_val_tostr(pPncInput->party));
		fprintf(yFpOutput, "%s=%d\n",
			NV_FIRST_DIGIT_TIMEOUT, pPncInput->firstDigitTimeout);
		fprintf(yFpOutput, "%s=%d\n",
			NV_INTER_DIGIT_TIMEOUT, pPncInput->interDigitTimeout);
		fprintf(yFpOutput, "%s=%d\n",
					NV_NTRIES, pPncInput->nTries);
		fprintf(yFpOutput, "%s=%s\n",
					NV_BEEP, arc_val_tostr(pPncInput->beep));
		fprintf(yFpOutput, "%s=%c\n",
				NV_TERMINATE_KEY, pPncInput->terminateKey);
		fprintf(yFpOutput, "%s=%d\n",
					NV_MIN_LEN, pPncInput->minLen);
		fprintf(yFpOutput, "%s=%d\n",
					NV_MAX_LEN, pPncInput->maxLen);
		fprintf(yFpOutput, "%s=%s\n",
				NV_TERMINATE_OPTION,
				arc_val_tostr(pPncInput->terminateOption));
		fprintf(yFpOutput, "%s=%s\n",
					NV_INPUT_OPTION,
					arc_val_tostr(pPncInput->inputOption));
		fprintf(yFpOutput, "%s=%s\n\n",
					NV_INTERRUPT_OPTION,
				arc_val_tostr(pPncInput->interruptOption));

		pPncInput = pPncInput->nextSection;
	}

	fclose(yFpOutput);

	sprintf(lMsg, "Loaded tags successfully saved to (%s)", zFilename);
        // tel_log(Mod,REPORT_VERBOSE,TEL_BASE,lMsg); 
	HANDLE_RETURN (0);

} /* END: TEL_SaveTags() */

/*------------------------------------------------------------------------------
sGetTagInfo(): Based on the line number, zLineNumber,  determine the tag 
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
 * sGetField(): 	This routine extracts the value of the desired
 *				fieldValue from the data record.
 * 
 * Output:
 * 	-1	: Error
 * 	>= 0	: Length of fieldValue extracted
 *----------------------------------------------------------------------------*/
static int sGetField(char zDelimiter, char *zInput, int zFieldNum,
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
	if(yRc == 0)
	{
		return((int)NULL);	
	}
	else
	{
		return(yRc);

	}
} /* END: sReadLogicalLine() */

/*------------------------------------------------------------------------------
sGetTagFromFile():
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
