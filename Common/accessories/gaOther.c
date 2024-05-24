/*------------------------------------------------------------------------------
File:		gaOther.c

Purpose:	global utility accessories

Author:		Sandeep Agate

Update:	11/06/97 djb	Created the file
Update:	12/02/97 djb    Added gaFiedCount, gaReadLogicalLine, ...
Update:	01/13/98 gpw	changed sLength to sLength+1 in calloc 
Update:	01/20/98 sja	Changed prototype of gaReadLogicalLine
Update:	01/22/98 djb	Added other ga functions to complete gaUtils
Update:	09/25/98 sja	In gaReadLogicalLine, if bufSize == 0, return
Update:	09/25/98 sja	In gaChangeString, added msg for invalid opcode
Update:	02/04/99 djb	changed cftime() to strftime(); SNA EXPRESS
Update:	01/06/00 djb	Added gaParsePath and gaAppendFile routines.
Update:	01/14/00 djb	Added gaMkdirP.
Update:	02/02/00 gpw	Fixed "Field number (%d)..." in gaGetField; fld missing
Update:	2000/02/17 sja	Added gaStringMatch()
Update:	2000/05/22 sja	Fixed "1st field empty" bug in gaGetField
Update:	2000/05/30 sja	Fixed gaGetField - No more stripping of white spaces.
Update:	2000/06/01 sja	Fixed gaGetField -Terminated extracted field with a '\0'
Update:	2000/11/30 sja	Changed sys_errlist[] to strerror()
------------------------------------------------------------------------------*/
#include "gaIncludes.h"
#include "gaUtils.h"

#define MYBUF_SIZE	256
#define	SLEEPTIME_LOCK	2

#define	OPEN_MASK			0000
#define	DIRECTORY_CREATION_PERMS	0755

	/* Local Function Prototypes */
static int gaStringLocator(char *buf, char delimiter, int occurance);
static int lCreateDirectory(char *iDirectory, char *oErrorMsg);

/*------------------------------------------------------------------------------
gaMkdirP():
	Verify and create the directory path passed in.  This is equivalent
	to the shell command (mkdir -p <pathname>).
------------------------------------------------------------------------------*/
int gaMkdirP(char *iDirectory, char *oErrorMsg)
{
	static char	mod[]="createPath";
	int		rc;
	char		*pNextLevel;
	char		workDir[256];
	char		buildDir[256];
	char		tmpDir[256];
	char    *yStrTok = NULL;

	memset((char *)buildDir, 0, sizeof(buildDir));
	sprintf(workDir, "%s", iDirectory);

	if ((pNextLevel=(char *)strtok_r(workDir, "/", &yStrTok)) == (char *)NULL)
	{
		sprintf(oErrorMsg, "Unable to parse input directory (%s).  "
			"%s=strtok_r(%s, /)", workDir, pNextLevel, workDir);
		return(-1);
	}
	sprintf(buildDir, "/%s", pNextLevel);

	for(;;)
	{
		if ((rc=lCreateDirectory(buildDir, oErrorMsg)) == -1)
		{
			return(-1);	/* oErrorMsg is set in routine */
		}

		if ((pNextLevel=strtok_r(NULL, "/", &yStrTok)) == NULL)
		{
			break;
		}

		sprintf(tmpDir, "%s", buildDir);
		sprintf(buildDir, "%s/%s", tmpDir, pNextLevel);
	}

	return(0);
} /* END: gaMkdirP() */

/*------------------------------------------------------------------------------
 * gaAppendfile(char *file1,
 *		char *file2,
 *		int  deleteAfterAppend,
 *		char *errorMsg)
 *	Purpose:
 * 		Append file1 to the file2.
 * 
 *		If the value of deleteAfterAppend == 1, then file2 is
 *		deleted after appending to file1.  A value of 0 specifies to
 *		retain file2 after the append.
 *
 *		No logging is performed; errorMsg is populated with the 
 *		error message.
 *
 * 	Return Codes:
 * 	>=0	Success
 * 	-1	Failure
 *----------------------------------------------------------------------------*/
 int gaAppendFile(char *file1, char *file2, int deleteAfterAppend,
						char *errorMsg)
{
	int		bytesRead;
	int		bytesWritten;
	char 		lineBuf[2048];
	int		fdFile1;
	int		fdFile2;

	if (file1[0] == '\0')
	{
		sprintf(errorMsg, "Error: Null value passed as first "
			"parameter.  file1:(%s).  Must be a valid file name.",
			file1);
		return(-1);
	}

	if (file2[0] == '\0')
	{
		sprintf(errorMsg, "Error: Null value passed as second "
			"parameter.  file2:(%s).  Must be a valid file name.",
			file2);
		return(-1);
	}

	if ((fdFile2=open(file2, O_RDONLY)) == -1)
	{
		sprintf(errorMsg, "Error:%d=open(%s, O_RDONLY).  [%d, %s].  "
			"Unable to append %s to %s.",
			fdFile2, file2, errno, strerror(errno),
			file2, file1);
		return(-1);
	}
	
	if ((fdFile1=open(file1, O_APPEND|O_WRONLY)) == -1)
	{
		sprintf(errorMsg, "Error:%d=open(%s, O_WRONLY|O_APPEND).  "
			"[%d, %s].  Unable to append %s to %s.",
			fdFile1, file1, errno, strerror(errno),
			file2, file1);
		close(fdFile2);
		return(-1);
	}
	
	while ((bytesRead=read(fdFile2, lineBuf, sizeof(lineBuf))) > 0)
	{
		if ((bytesWritten=write(fdFile1, lineBuf, bytesRead)) 
								!= bytesRead)
		{
			sprintf(errorMsg, "ERR:Error writing to (%s).  "
				"[%d, %s]. ",
				file1, errno, strerror(errno));
			close(fdFile1);
			close(fdFile2);
			return(-1);
		}
	}

	close(fdFile1);
	close(fdFile2);

	if (deleteAfterAppend)
	{
		(void)unlink(file2);
	}
	return(0);
} /* END: gaAppendFile() */

/*------------------------------------------------------------------------------
 * gaParsePath(	char *fullPathname,
 *		char *directoryOnly,
 *		char *filenameOnly,
 *		char *errorMsg)
 *	Purpose:
 * 		Separate the directory name from the filename of fullPathname.
 * 
 *		No logging is performed; errorMsg is populated with the 
 *		error message.
 *
 * 	Return Codes:
 * 	>=0	Success
 * 	-1	Failure
 *----------------------------------------------------------------------------*/
 int gaParsePath(char *fullPathname, char *directoryOnly, char *filenameOnly,
								char *errorMsg)
{
	int		rc;
	int		i;
	char		tmpFullPath[256];
	int		len;

	if (fullPathname[0] == '\0')
	{
		sprintf(errorMsg, "Error: Invalid fullPathname (%s).  "
			"Nothing to parse.  No action taken.", fullPathname);
		return(-1);
	}

	len=strlen(fullPathname);
	if (len > 255)
	{
		sprintf(errorMsg, "Error: Invalid string length of (%s) : %d.  "
			"This must be less than 255", fullPathname, len);
		return(-1);
	}

	sprintf(tmpFullPath, "%s", fullPathname);

	for (i=len-1; i>0; i--)
	{
		if (tmpFullPath[i] != '/')
		{
			continue;
		}
		
		if ( i == len-1 )
		{
			tmpFullPath[i]='\0';
			sprintf(directoryOnly, "%s", tmpFullPath);
			filenameOnly[0]='\0';
			return(0);
		}

		sprintf(filenameOnly, "%s", &(tmpFullPath[i+1]));
		tmpFullPath[i]='\0';
		sprintf(directoryOnly, "%s", tmpFullPath);
		return(0);
	}

	/* No '/' was found;  Directory is current; file is everything. */
	sprintf(directoryOnly, "%s", ".");
	sprintf(filenameOnly, "%s", tmpFullPath);
	return(0);

} /* END: gaParsePath() */



/*------------------------------------------------------------------------------
 * gaChangeString():
 * 	Manipulate a string according to the specified opCode.
 *
 * opCode:
 *	OP_NO_WHITESPACE :	removes all whitespace characters from string
 *	OP_CLEAN_WHITESPACE :	replaces all continuous whitespace characters
 * 				with a single space (used to 'clean up' a buffer
 *	OP_TRIM_WHITESPACE  :	removes all whitespace characters from the 
 *				beginning and end of the string
 *	OP_STRING_LOWER     :	converts all alpha characters to lowercase
 *	OP_STRING_UPPER	    :	converts all alpha characters to uppercase
 *
 * Dependencies: 	None.
 * 
 * Input:
 * 	opCode		: must be a valid opCode for this routine
 * 	string		: buffer in which the operation is performed
 * 
 * Output:
 * 	-1	: Invalid opcode
 * 	 0	: all other
 *----------------------------------------------------------------------------*/
int gaChangeString(int opCode, char *string)
{
	int		sLength;
	int		i;
	char		*tmpString;
	char		*ptr2;
	char		*ptr;
	static char	mod[]="gaChangeString";

	if((sLength = strlen(string)) == 0)
	{
		return(0);
	}
		
	switch(opCode)
	{
	case OP_NO_WHITESPACE :
		tmpString = (char *)calloc((size_t)(sLength + 1), sizeof(char));
		ptr2 = tmpString;
		ptr = string;

		while ( *ptr != '\0' )
		{
			if ( isspace( *ptr ) )
			{
				ptr++;
			}
			else
			{
				*ptr2 = *ptr; 
				ptr2++;
				ptr++;
			}
		} /* while */

		sprintf(string, "%s", tmpString);
		free(tmpString);
		break;

	case OP_CLEAN_WHITESPACE :
		tmpString = (char *)calloc((size_t)(sLength + 1), sizeof(char));
		ptr=string;
		ptr2=tmpString;

		i=0;
		while ( *ptr != '\0' )
		{
			if  ( isspace(*ptr) )
			{
				ptr++;
				i++;
				continue;
			}

			if ( i > 0 )
			{
				*ptr2=' ';
				ptr2++;
				i=0;
			}
			*ptr2=*ptr;
			ptr2++;
			ptr++;
		} /* while */

			sprintf(string, "%s", tmpString);
			free(tmpString);
			break;

	case OP_TRIM_WHITESPACE :
		tmpString = (char *)calloc((size_t)(sLength+1), sizeof(char));
		ptr = string;

		if ( isspace(*ptr) )
		{
			while(isspace(*ptr))
			{
				ptr++;
			}
		}

		sprintf(tmpString, "%s", ptr);
		ptr = &tmpString[(int)strlen(tmpString)-1];
		while(isspace(*ptr))
		{
			*ptr = '\0';
			ptr--;
		}

		sprintf(string, "%s", tmpString);
		free(tmpString);
		break;

	case OP_STRING_LOWER :
		for (i=0; i<sLength; i++)
		{
			string[i] = tolower(string[i]);
		}
		break;

	case OP_STRING_UPPER :
		for (i=0; i<sLength; i++)
		{
			string[i] = toupper(string[i]);
		}
		break;

	default	:
		gaVarLog(mod, 0, "Invalid opcode (%d)", opCode);
		return(-1);
	}

	return(0);
} /* END: gaChangeString() */

/*------------------------------------------------------------------------------
 * gaReadLogicalLine():
 * 		Read uncommented lines (lines not beginning with a '#') and
 * 		append all lines ending with a '\' into one line.
 * 
 * Dependencies: 	None.
 * 
 * Input:
 * 	fp		: file pointer to the file being read
 * 	buffer		: buffer in which the line will be read into
 * 	bufSize		: maximun number of characters to be read
 * 
 * Output:
 * 	-1	: Error
 * 	>= 0	: number of characters read.
 *----------------------------------------------------------------------------*/
int gaReadLogicalLine(char *buffer, int bufSize, FILE *fp)
{
	int		rc;
	char		str[BUFSIZ];
	char		tmpStr[BUFSIZ];
	char		*ptr;
	static char	mod[]="gaReadLogicalLine";

	if(bufSize <= 0)
	{
		gaVarLog(mod, 0, "bufSize(%d) must be > 0", bufSize);
	}

	memset(str, 0, sizeof(str));
	memset(buffer, 0, bufSize);

	while(fgets(str, sizeof(str)-1, fp))
	{
		/*
		 * skip empty lines
		 */
		if (str[0] == '\0' )
		{
			continue;
		}

		/*
		 * skip lines with only whitespace
		 */
		ptr = str;
		while(isspace(*ptr))
		{
			ptr++;
		}
		if(ptr[0] == '\0' )
		{
			continue;
		}

		/*
		 * skip lines beginning with a '#'
		 */
		if(ptr[0] == '#')
		{
			continue;
		}

		/*
		 * Check if the last character in the string is a '\'
		 */
		if(str[strlen(str) - 2] == '\\')
		{
			sprintf(tmpStr, "%s", buffer);
			sprintf(buffer, "%s%.*s", tmpStr, strlen(str) - 2, str);
			continue;
		}

		if ( strlen(buffer) == 0 )
		{
			sprintf(buffer, "%s", str);
		}
		else
		{
			sprintf(tmpStr, "%s", buffer);
			sprintf(buffer, "%s%s", tmpStr, str);
		}

		break;
	}

	rc = strlen(buffer);
	if(rc == 0)
	{
		return(0);	
	}
	else
	{
		return(rc);

	}
} /* END: gaReadLogicalLine() */
/*------------------------------------------------------------------------------
 * gaGetField(): 	This routine extracts the value of the desired
 *			fieldValue from the data record.
 * 
 * Dependencies: 	None.
 * 
 * Input:
 * 	delimiter	: that separates fieldValues in the buffer.
 * 	buffer		: containing the line to be parsed.
 * 	fieldNum	: Field # to be extracted from the buffer.
 * 	fieldValue	: Return buffer containing extracted field.
 * 
 * Output:
 * 	-1	: Error
 * 	>= 0	: Length of fieldValue extracted
 *----------------------------------------------------------------------------*/
int gaGetField(char delimiter, char *buffer, int fieldNum, char *fieldValue)
{
	static char	mod[] = "gaGetField";
	register int	i;
	int		fieldLength, state, wc;

	fieldLength = state = wc = 0;

	fieldValue[0] = '\0';

	if(fieldNum <= 0)
	{
		gaVarLog(mod, 0, "Field number (%d) must be > 0", fieldNum);
        	return (-1);
	}

	for(i=0;i<(int)strlen(buffer);i++) 
        {
		/*
		 * If the current character is a delimiter
		 */
        	if(buffer[i] == delimiter || buffer[i] == '\n')
                {
			/*
			 * Set state to 0 indicating that we have hit a 
			 * field boundary.
			 */
                	state = 0;

			/*
			 * If the current character and the previous character
			 * are delimiters, increment the word count.
			 */
                	if(buffer[i] == delimiter)
			{
				if(i == 0 || buffer[i-1] == delimiter)
				{
					++wc;
				}
			}
                }
        	else if (state == 0)
                {
                	state = 1;
                	++wc;
                }
        	if (fieldNum == wc && state == 1)
		{
                	fieldValue[fieldLength++] = buffer[i];
		}
        	if (fieldNum == wc && state == 0)
		{
			fieldValue[fieldLength] = '\0';
                	break;
		}
        } /* for */

	if(fieldLength > 0)
        {
        	fieldValue[fieldLength] = '\0';
		/*
		 * The following code is commented because it manipulates 
		 * the contents of the field that was extracted from the buffer.
		 *  This should be left to the application to do and the API 
		 * should stay out of it.
		 *
		 * SJA - 05/30/2000
		 */
#ifdef COMMENT
        	while(fieldValue[0] == ' ')
                {
                	for (i=0; fieldValue[i] != '\0'; i++)
                        	fieldValue[i] = fieldValue[i+1];
                }
        	fieldLength = strlen(fieldValue);
#endif /* COMMENT */
        	return (fieldLength);
        }

	return(-1);
} /* END: gaGetField() */
/*------------------------------------------------------------------------------
 * gaFieldCount(): Count the number of fields in a delimited buffer
 *----------------------------------------------------------------------------*/
int gaFieldCount(char delimiter, char *buffer, int *numFields)
{
	char		*ptr;
	char		*ptr2;
	int		i;
	int		bufferLength;
	static char	mod[]="gaFieldCount";

	if ((ptr=strchr(buffer, delimiter)) == (char *)NULL)
	{
		*numFields=1;
		return(0);
	}
	
	*numFields=1;
	i=0;

	if (buffer[i] == delimiter)
	{
		++i;
	}

	bufferLength=strlen(buffer);

	while ( i < bufferLength )
	{
		if ( buffer[i] == delimiter )
		{
			if ( i < (bufferLength - 1) )
			{
				*numFields+=1;
			}
		}
		++i;
	} /* while */
	
	return(0);
} /* END: gaFieldCount() */
/*------------------------------------------------------------------------------
 * gaSubStringCount():	Return the number of instances of one string within
 *			another.  This function returns GA_FAILURE when
 *			stringToSearchFor is NULL
 *----------------------------------------------------------------------------*/
int gaSubStringCount(char *sourceString, char *stringToSearchFor)
{
	char		*ptr;
	char		*s1Tmp;
	int		numHits=0;

	s1Tmp = sourceString;
	
	while((ptr=strstr(s1Tmp, stringToSearchFor)) != NULL)
	{
		s1Tmp=ptr+strlen(stringToSearchFor);
		numHits++;
	}
	
	return(numHits);
} /* END: gaSubStringCount() */
/*------------------------------------------------------------------------------
 * gaGetLabelList():
 * 	rewinds <fp>, then reads through the file, populating list array
 * 	with the lines belonging to <label>, and incrementing <*nEntries>.
 * 
 * 	Return values:
 * 		 * 0	- always
 *----------------------------------------------------------------------------*/
int gaGetLabelList(FILE *fp, int maxEntries, char *label, char **list,
			 					int *nEntries)
{
	static char	mod[]="gaGetLabelList";
	int		rc;
	int		index;
	char		buf[MYBUF_SIZE];
	char		tmpLabel[32];
	int		done=0;

	rewind(fp);
	*nEntries=0;

	while (! done)
	{
		memset((char *) buf, 0, MYBUF_SIZE);
		if ( gaReadLogicalLine(buf, MYBUF_SIZE, fp) == 0 )
		{
			done=1;
			continue;
		}

		rc = gaChangeString(OP_TRIM_WHITESPACE, buf);

		if ( buf[0] != '[' )
		{
			continue;
		}

		sprintf(tmpLabel,"%.*s",strcspn(&buf[1],"]"),&buf[1]);

		if ( strcmp(label, tmpLabel) != 0 )
		{
			continue;
		}		

		index=0;
		memset((char *) buf, 0, MYBUF_SIZE);

		while ( gaReadLogicalLine(buf, MYBUF_SIZE, fp) )
		{
			rc=gaChangeString(OP_TRIM_WHITESPACE, buf);
			
			if ( buf[0] == '[' )
			{
				done=1;
				break;
			}
			else
			{
				index=(*nEntries)++;
				sprintf(list[index],"%s", buf);
			}

			if (*nEntries == maxEntries)
			{
				done=1;
				break;
			}

			memset((char *) buf, 0, sizeof(buf));
		}
	} /* while !done */

	return(0);
} /* END: gaGetLabelList() */
/*------------------------------------------------------------------------------
 * gaUnlockNClose(int *fp)
 * 
 * 	Purpose:
 * 		Unlock and close open file pointer fp.
 * 
 * 	Return Codes:
 * 	0	Success
 *----------------------------------------------------------------------------*/
int gaUnlockNClose(int *fp)
{
	int		rc;
	static char	mod[]="gaUnlockNClose";

	rc=lockf(*fp, F_ULOCK, 0);
	close(*fp);

	return(0);
} /* END: gaUnlockNClose() */

/*------------------------------------------------------------------------------
 * gaOpenNLock(char *fileName,		input:  file to be opened & locked
 * 		int mode,		input:  oflag (i.e. O_RDONLY)
 * 		int permissions,	input:  permissions for open (i.e. 0755)
 * 		int timeLimit,		input:  max retry time obtaining a lock
 * 		int *fp)		output: file pointer to <fileName>
 * 
 * 	Purpose:
 * 		Opens <fileName>, and attempts to obtain a lock on it.  If
 * 		obtaining a lock on <fileName> fails, it retries every
 * 		SLEEPTIME_LOCK seconds for <timeLimit> seconds.
 * 
 * 	Return Codes:
 * 	0	Success
 * 	-1	Failure: <fileName> could not be open, or could not obtain lock
 *----------------------------------------------------------------------------*/
int gaOpenNLock(char *fileName,
		int mode,
		int permissions,
		int timeLimit,
		int *fp)
{
	int		rc;
	int		counter=0;
	static char	mod[]="gaOpenNLock";

	if ((*fp=open(fileName, mode, permissions)) == -1)
	{
		gaVarLog(mod, 0,
				"open(%s,%o,%o) failed. errno=%d",
				fileName,mode, permissions, errno);
		return(-1);
	}

	while (counter <= timeLimit)
	{
		if ((rc=lockf(*fp, F_TLOCK, 0)) == -1) 
		{
			gaVarLog(mod, 0,
				"lockf(%d, %d, %d) failed. errno=%d",
					*fp, F_TLOCK, 0, errno);
			sleep(SLEEPTIME_LOCK);
			counter+=SLEEPTIME_LOCK;
			continue;
		}
		break;
	}

	if (counter > timeLimit)
	{
		gaVarLog(mod, 0,
				"Unable to obtain lock on %s",fileName);
		rc=lockf(*fp, F_ULOCK, 0);
		close(*fp);
		return(-1);
	}

	return(0);

} /* END: gaOpenNLock() */

/*------------------------------------------------------------------------------
 * gaFileAccessTime(char *fileName,	input: file or directory
 * 		char *dateFormat,	input: format string (i.e. %m%d%y)
 * 		char *timeFormat,	input: format string (i.e. %H%M)
 * 		char *date,		output: date (i.e. 091997)
 * 		char *time)		output: time (i.e. 1345)
 * 
 * 	Purpose:
 * 		Returns the date and time strings of *fileName.
 * 
 * 	Return Codes:
 * 	0	success
 * 	-1	Failure : can't stat <fileName>; most likely does not exist
 *----------------------------------------------------------------------------*/
int gaFileAccessTime(char *fileName,
			char *dateFormat,
			char *timeFormat,
			char *date,
			char *time)
{
	int		rc;
	static char	mod[]="gaFileAccessTime";
	struct stat	statBuf;
	struct tm	*PT_time;

	if ( (rc=stat(fileName, &statBuf)) != 0)
	{
		gaVarLog(mod, 0,
			"Could not stat %s. errno=%d",
			fileName,errno);
		return(-1);
	}
		
	PT_time=localtime(&(statBuf.st_mtime));
/*
	rc=cftime(date, dateFormat, &(statBuf.st_mtime));
	rc=cftime(time, timeFormat, &(statBuf.st_mtime));
*/
	strftime(date,sizeof(date), dateFormat, PT_time);
	strftime(time,sizeof(time), timeFormat, PT_time);
	return(0);

} /* END: gaFileAccessTime() */

/*------------------------------------------------------------------------------
 * gaGetFileType(char *fileName)
 * 
 * 	Purpose:
 * 		Determine whether a system file is a file, directory, pipe,
 *		or link.
 * 
 * 	Return Codes:
 * 	3	GA_IS_PIPE
 * 	2	GA_IS_LINK
 * 	1	GA_IS_DIRECTORY
 * 	0	GA_IS_FILE
 *     -1	GA_FAILURE
 *----------------------------------------------------------------------------*/
int gaGetFileType(char *fileName)
{
	int		rc;
	static char	mod[]="gaGetFileType";
	struct stat	statBuf;
	mode_t		mode;

	if ( (rc=stat(fileName, &statBuf)) != 0)
	{
		gaVarLog(mod, 0,
			"Could not stat %s. errno=%d", fileName, errno);
		return(GA_FAILURE);
	}
		
	mode = statBuf.st_mode & S_IFMT;

	if ( mode == S_IFIFO )
	{
		return(GA_IS_PIPE);
	}
		
	if ( mode == S_IFDIR )
	{
		return(GA_IS_DIRECTORY);
	}
		
	if ( mode == S_IFREG )
	{
		return(GA_IS_FILE);
	}
		
	return(GA_IS_OTHER);

} /* END: gaGetFileType() */

/*------------------------------------------------------------------------------
 * gaReplaceChar(char *buffer,		The string to be modified 
 *		 char oldChar,		Character which will be replaced 
 *		 char newChar,		Character to substitue for oldChar
 *		 int instances)		Maximum number of replacements
 *						to make, -1 = ALL, 0 = NONE
 * 
 * 	Purpose:
 * 		Replace all or for x instances of a charcter in a string
 *		with a new character.
 * 
 * 	Return Codes:
 * 		 0	GA_SUCCESS
 *		-1 	GA_FAILURE
 *----------------------------------------------------------------------------*/
int gaReplaceChar(char *buffer, char oldChar, char newChar, int instances)
{
	int		rc;
	static char	mod[]="gaReplaceChar";
	int		bufferLength;
	int		i=0;
	int		changedInstances=0;

	if ( (instances == 0) || ((bufferLength=strlen(buffer)) == 0) )
	{
		return(0);
	}

	if ( instances < 0 )
	{
		while ( i < bufferLength )
		{
			if ( buffer[i] == oldChar )
			{
				buffer[i] = newChar;
			}
			++i;
		}
	}
	else
	{
		while ( i < bufferLength )
		{
			if ( buffer[i] == oldChar )
			{
				if (changedInstances < instances)
				{
					buffer[i] = newChar;
					++changedInstances;
					if (changedInstances == instances)
					{
						break;
					}
				}
			}
			++i;
	
		}
	}

	return(0);

} /* END: gaReplaceChar() */

/*------------------------------------------------------------------------------
 * gaChangeRecord(char delimiter,	field separator
 *		  char *fileName,	name of file containing record
 *		  int keyField,		number of the field to search on
 *		  char *keyValue,	value of the key field to search for
 *		  int fieldNum,		number of the field to be changed
 *		  char *newValue)	value that the field in fieldNum
 * 
 * 	Purpose:
 * 		Modify the field in a flat file database.
 * 
 * 	Return Codes:
 * 	0	Success
 * 	-1	Failure:
 *----------------------------------------------------------------------------*/
int gaChangeRecord(char delimiter, char *fileName, int keyField,
			char *keyValue, int fieldNum, char *newValue)
{
	int		rc;
	static char	mod[]="gaChangeRecord";
	char		tmpFileName[256];
	FILE		*fpOut;
	FILE		*fpIn;
	char		buf[BUFSIZ];	/* record size (BUFSIZ) in stdio.h */
	char		field[MYBUF_SIZE];
	int		delimPos1;
	int		delimPos2;
	time_t		tm;
	char		timeBuf[64];
	struct tm	*PT_time;
	
	if (access(fileName, R_OK | W_OK) != 0)
	{
		gaVarLog(mod, 0, "Cannot access %s. errno=%d", fileName, errno);
		return(-1);
	}

	memset((char *)timeBuf, 0, sizeof(timeBuf));
	time(&tm);
	PT_time=localtime(&tm);
	strftime(timeBuf, sizeof(timeBuf),"%H%M%S", PT_time);

	sprintf(tmpFileName, "/tmp/%s.%d.%s", mod, getpid(), timeBuf);
	
	if ( (fpOut=fopen(tmpFileName, "w")) == NULL)
	{
		gaVarLog(mod, 0, "Cannot open %s for writing. errno=%d",
							 tmpFileName, errno);
		return(-1);
	}

	if ( (fpIn=fopen(fileName, "r")) == NULL)
	{
		gaVarLog(mod, 0, "Cannot open %s for reading. errno=%d",
							 fileName, errno);
		fclose(fpOut);
		remove(tmpFileName);
		return(-1);
	}

	memset((char *)buf, 0, BUFSIZ);
	while ( gaReadLogicalLine(buf, BUFSIZ, fpIn) )
	{
		memset((char *)field, 0, sizeof(field));
		if (gaGetField(delimiter, buf, keyField, field) == -1)
		{
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				gaVarLog(mod, 0,
					"Error writing <%s> to %s. errno=%d",
					buf, tmpFileName, errno);
				fclose(fpIn);
				fclose(fpOut);
				remove(tmpFileName);
				return(-1);
			}
			fflush(fpOut);
			memset((char *)buf, 0, BUFSIZ);
			continue;
		}

		if ( strcmp(field, keyValue) != 0)
		{
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				gaVarLog(mod, 0,
					"Error writing <%s> to %s. errno=%d",
					buf, tmpFileName, errno);
				fclose(fpIn);
				fclose(fpOut);
				remove(tmpFileName);
				return(-1);
			}
			fflush(fpOut);
			memset((char *)buf, 0, BUFSIZ);
			continue;
		}

		if ((delimPos1=gaStringLocator(buf, delimiter,
							fieldNum-1)) == -1)
		{
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				gaVarLog(mod, 0,
					"Error writing <%s> to %s. errno=%d",
					buf, tmpFileName, errno);
				fclose(fpIn);
				fclose(fpOut);
				remove(tmpFileName);
				return(-1);
			}
			fflush(fpOut);
			memset((char *)buf, 0, BUFSIZ);
			continue;
		}

		if ((delimPos2=gaStringLocator(buf, delimiter,
							fieldNum)) == -1)
		{
			delimPos2=delimPos1+1;
/*
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				gaVarLog(mod, 0,
					"Error writing <%s> to %s. errno=%d",
					buf, tmpFileName, errno);
				fclose(fpIn);
				fclose(fpOut);
				remove(tmpFileName);
				return(-1);
			}
			fflush(fpOut);
			memset((char *)buf, 0, BUFSIZ);
			continue;
*/
		}

		if ((rc=fprintf(fpOut, "%.*s", delimPos1+1, buf)) < 0)
		{
			gaVarLog(mod, 0,
				"Error writing <%s> to %s. errno=%d",
				buf, tmpFileName, errno);
			fclose(fpIn);
			fclose(fpOut);
			remove(tmpFileName);
			return(-1);
		}

		if ((rc=fprintf(fpOut, "%s", newValue)) < 0)
		{
			gaVarLog(mod, 0,
				"Error writing <%s> to %s. errno=%d",
				keyValue, tmpFileName, errno);
			fclose(fpIn);
			fclose(fpOut);
			remove(tmpFileName);
			return(-1);
		}

		if ( delimPos2 > (delimPos1 + 1) )
		{
			if ((rc=fprintf(fpOut, "%s", &buf[delimPos2])) < 0)
			{
				gaVarLog(mod, 0,
					"Error writing <%s> to %s. errno=%d",
					keyValue, tmpFileName, errno);
				fclose(fpIn);
				fclose(fpOut);
				remove(tmpFileName);
				return(-1);
			}
		}
		else
		{
			if ((rc=fprintf(fpOut, "%s", &buf[strlen(buf)-1])) < 0)
			{
				gaVarLog(mod, 0,
					"Error writing <%s> to %s. errno=%d",
					keyValue, tmpFileName, errno);
				fclose(fpIn);
				fclose(fpOut);
				remove(tmpFileName);
				return(-1);
			}
		}

		fflush(fpOut);
		memset((char *)buf, 0, BUFSIZ);
	}

	fclose(fpIn);
	fclose(fpOut);
	sprintf(buf, "cp %s %s", tmpFileName, fileName);
	
	rc=system(buf);

	remove(tmpFileName);

	if ( rc == 0 )
	{
		return(0);
	}
	else
	{
		return(-1);
	}

} /* END: gaChangeRecord() */

/*------------------------------------------------------------------------------
 * gaRemoveRecord(char delimiter,	character used to separate fields
 *		  char *fileName,	name of file containing records
 *		  int  keyField,	number of the field to search on
 *		  char *keyValue)	value of the record to be searched for
 * 
 * 	Purpose:
 * 		Remove a line from a flat file database
 * 
 * 	Return Codes:
 * 	>=0	Success
 * 	-1	Failure
 *----------------------------------------------------------------------------*/
int gaRemoveRecord(char delimiter, char *fileName, int keyField, char *keyValue)
{
	int		rc=0;
	static char	mod[]="gaRemoveRecord";
	FILE		*fpIn;
	FILE		*fpOut;
	char		buf[BUFSIZ];	/* record size (BUFSIZ) in stdio.h */
	char		field[MYBUF_SIZE];
	char		tmpFileName[MYBUF_SIZE];
	time_t		tm;
	char		timeBuf[64];
	struct tm	*PT_time;
	
	if (access(fileName, R_OK | W_OK) != 0)
	{
		gaVarLog(mod, 0, "Cannot access %s. errno=%d", fileName, errno);
		return(-1);
	}

	memset((char *)timeBuf, 0, sizeof(timeBuf));
	time(&tm);
	PT_time=localtime(&tm);
	strftime(timeBuf, sizeof(timeBuf),"%H%M%S", PT_time);

	sprintf(tmpFileName, "/tmp/%s.%d.%s", mod, getpid(), timeBuf);
	
	if ( (fpOut=fopen(tmpFileName, "w")) == NULL)
	{
		gaVarLog(mod, 0, "Cannot open %s for writing. errno=%d",
							 tmpFileName, errno);
		return(-1);
	}

	if ( (fpIn=fopen(fileName, "r")) == NULL)
	{
		gaVarLog(mod, 0, "Cannot open %s for reading. errno=%d",
							 fileName, errno);
		fclose(fpOut);
		remove(tmpFileName);
		return(-1);
	}

	memset((char *)buf, 0, sizeof(buf));
	while (gaReadLogicalLine(buf, BUFSIZ, fpIn) )
	{
		memset((char *)field, 0, sizeof(field));
		if ((rc=gaGetField(delimiter, buf, keyField, field)) == -1)
		{
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				gaVarLog(mod, 0,
					"Error writing <%s> to %s. errno=%d",
					buf, tmpFileName, errno);
				fclose(fpIn);
				fclose(fpOut);
				remove(tmpFileName);
				return(-1);
			}

			fflush(fpOut);
			memset((char *)buf, 0, BUFSIZ);
			continue;
		}

		if ( strcmp(field, keyValue) != 0 )
		{
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				gaVarLog(mod, 0,
					"Error writing <%s> to %s. errno=%d",
					buf, tmpFileName, errno);
				fclose(fpIn);
				fclose(fpOut);
				remove(tmpFileName);
				return(-1);
			}
			fflush(fpOut);
			memset((char *)buf, 0, BUFSIZ);
			continue;
		}
		continue;
	}

	sprintf(buf, "cp %s %s", tmpFileName, fileName);
	rc=system(buf);
	remove(tmpFileName);

	if ( rc == 0 )
	{
		return(0);
	}
	else
	{
		return(-1);
	}

} /* END: gaRemoveRecord() */

/*------------------------------------------------------------------------------
 * gaGetMaxKey( char delimiter,		character used to separate fields
 *		char *fileName,		name of file containing records
 *		int  keyField,		number of the field to search on
 *		char *maxKeyValue)	largest existing key value
 * 
 * 	Purpose:
 * 		identify the maximum key type in a flat file database
 * 
 * 	Return Codes:
 * 	>=0	Success
 * 	-1	Failure
 *----------------------------------------------------------------------------*/
int gaGetMaxKey(char delimiter, char *fileName, int keyField,
							char *maxKeyValue)
{
	int		rc=0;
	static char	mod[]="gaGetMaxKey";
	FILE		*fpIn;
	char		buf[BUFSIZ];	/* record size (BUFSIZ) in stdio.h */
	char		holdKey[MYBUF_SIZE];
	char		tmpKey[MYBUF_SIZE];
	
	if (access(fileName, R_OK) != 0)
	{
		gaVarLog(mod, 0, "Cannot access %s. errno=%d", fileName, errno);
		return(-1);
	}

	if ( (fpIn=fopen(fileName, "r")) == NULL)
	{
		gaVarLog(mod, 0, "Cannot open %s for reading. errno=%d",
							 fileName, errno);
		return(-1);
	}

	memset((char *)buf, 0, sizeof(buf));
	memset((char *)holdKey, 0, sizeof(holdKey));
	while (gaReadLogicalLine(buf, BUFSIZ, fpIn) )
	{
		memset((char *)tmpKey, 0, sizeof(tmpKey));
		if ((rc=gaGetField(delimiter, buf, keyField, tmpKey)) == -1)
		{
			memset((char *)buf, 0, sizeof(buf));
			continue;
		}

		if ( strcmp(tmpKey, holdKey) > 0 )
		{
			sprintf(holdKey, "%s", tmpKey);
		}
	}

	sprintf(maxKeyValue, "%s", holdKey);
	return(0);

} /* END: gaGetMaxKey() */

/*------------------------------------------------------------------------------
 * gaStringLocator(char *buf,		
 *		   char delimiter,
 * 		   int  occurance)
 * 
 * 	Purpose:
 * 		return location within string of the Nth occurance of 
 *		delimiter
 * 
 * 	Return Codes:
 * 	>=0	Success
 * 	-1	Failure: delimter was not found
 *----------------------------------------------------------------------------*/
int gaStringLocator(char *buf, char delimiter, int occurance)
{
	int		rc=0;
	static char	mod[]="gaStringLocator";
	int		i=0;
	int		bufLength;
	int		foundIt=0;

	if ( occurance < 1 )
	{
		return(-1);
	}

	bufLength=(int)strlen(buf);

	while (i < bufLength)
	{
		if ( buf[i] == delimiter )
		{
			++rc;
			if ( rc == occurance )
			{
				foundIt=1;
				break;
			}
		}
		++i;
	}

	if ( foundIt )
	{
		return(i);
	}
	else
	{
		return(-1);
	}
} /* END: gaStringLocator */

/*------------------------------------------------------------------------------
lCreateDirectory():
	Verifies if the directory passed in exists and is a valid directory.  
	If it does not exist, create it with the system call 'mkdir'.
------------------------------------------------------------------------------*/
static int lCreateDirectory(char *iDirectory, char *oErrorMsg)
{
	int		rc;
	mode_t		mode;
	struct stat	statBuf;
	static char	mod[]="lCreateDirectory";

	(void)umask(OPEN_MASK);
	/*
	 * Does the directory exist
	 */
	if ((rc=access(iDirectory, F_OK)) == 0)
	{
		/*
		 * Is it really directory
		 */
		if ( (rc=stat(iDirectory, &statBuf)) != 0)
		{
			sprintf(oErrorMsg, "Could not stat (%s).  [%d, %s].  "
					"(%s) exists but unable to access it.",
					iDirectory, errno, strerror(errno),
					iDirectory);
			return(-1);
		}

	        mode = statBuf.st_mode & S_IFMT;

		if ( mode != S_IFDIR)
		{
			sprintf(oErrorMsg, "Error: (%s) exists, but is not a "
				"directory.", iDirectory);
			return(-1);
		}
		return(0);
	}

	if ((rc=mkdir(iDirectory, DIRECTORY_CREATION_PERMS)) != 0)
	{
		sprintf(oErrorMsg,
			"Error: Unable to create directory (%s).  [%d, %s]",
			iDirectory, errno, strerror(errno));
		return(-1);
	}

	return(0);
} /* END: lCreateDirectory() */

#ifdef COMMENTED_FOR_LINUX

/*------------------------------------------------------------------------------
gaStringMatch(): Matches the given string based on the pattern.

Input:

	iPattern			The pattern that needs to be matched	
	iString				The string containing the pattern

Output:

	None

Return Codes:

	0					String does not match pattern	
	1					String matches pattern.

Notes:
	1. If either the pattern or the string are empty, a mismatch is returned.
------------------------------------------------------------------------------*/
int gaStringMatch(char *iPattern, char *iString)
{
	static char	mod[] = "gaStringMatch";
	char		*compiledForm, *newPtr, ret0[9];
	int			rc;

	if(iPattern[0] == '\0' || iString[0] == '\0')
	{
		return(0);
	}

	if((compiledForm = (char *)regcmp(iPattern, (char *)0)) == (char *)NULL)
	{
		return(0);
	}

	memset(ret0, 0, sizeof(ret0));

	if((newPtr = (char *)regex(compiledForm, iString, ret0)) == (char *)NULL)
	{
		rc = 0;
	}
	else
	{
		rc = 1;
	}

	free(compiledForm);

	return(rc);
} /* END gaStringMatch() */

#endif /* COMMENTED_FOR_LINUX */
