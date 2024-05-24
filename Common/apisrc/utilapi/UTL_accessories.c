/*------------------------------------------------------------------------------
File:		UTL_accessories.c

Purpose:	This file contains all UTL_accessory routines.

Author:		Sandeep Agate

Update:	2000/05/09 sja	Copied from Common/accessories to Common/apisrc/utilapi
Update:	2000/05/17 sja	Changed UTL_GetField to UTL_GetFieldByDelimiter
Update:	2000/05/19 sja	Removed unused variables that were global to this file
Update:	2000/05/22 sja	Fixed "1st field empty" bug in UTL_GetFieldByDelimiter
Update:	2000/05/30 sja	Fixed UTL_GetFieldByDelimiter - no stripping of spaces
Update:	2000/06/01 sja	UTL_GetFieldBy.. Terminated extracted field with a '\0'
Update:	2001/01/16 sja	Added #include "ISPCommon.h"
Update:	2001/05/25 sja	Added UTL_GetProfileString()
Update:	2002/01/22 sja	No code change. Touched file in error.
Update:	2002/07/18 sja	Added the zErrorMsg parameter to UTL_GetProfileString
Update:	2004/08/18 djb	Added UTL_GetArcFifoDir().
Update: 2006/06/25 djb  Added if-logic to UTL_GetArcFifoDir().
------------------------------------------------------------------------------*/
/*
 * Include Files
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <regex.h>

#include "ISPCommon.h"
#include "ispinc.h"
#include "loginc.h"
#define UTL_FIFO_DEF	1
#include "arcFifos.h"

#define MYBUF_SIZE	256
#define	SLEEPTIME_LOCK	2

#define	OPEN_MASK			0000
#define	DIRECTORY_CREATION_PERMS	0755

/*
 * Static Function Prototypes
 */
static int UTL_StringLocator(char *buf, char delimiter, int occurance);
static int lCreateDirectory(char *iDirectory, char *oErrorMsg);
static int trim_whitespace(char *string);
static int sGetNameValue(char *section, char *name, char *defaultVal,
			char *value, int bufSize, char *file, char *err_msg);

/*
 * Externs
 */
extern int	errno;

/*----------------------------------------------------------------------------
UTL_VarLog(): Log messages on stderr and/or a log file. (Takes variable length
	 argument lists.
----------------------------------------------------------------------------*/
int UTL_VarLog(char *moduleName, int reportMode, char *groupId, 
		char *resourceName, char *arcService, char *application, 
		int messageId, char *messageFormat, ...)
{

	va_list		ap;
	char		message[2048];
	int		arcReportMode;
	
	memset(message, 0, sizeof(message));
	
	va_start(ap, messageFormat);
	vsprintf(message, messageFormat, ap);
	va_end(ap);

	if(reportMode == 0)
	{
		reportMode = REPORT_NORMAL;
	}
	LogARCMsg(moduleName, reportMode, resourceName, arcService, 
			application, messageId, message);
	return(0);
} /* END: UTL_VaLog() */

/*------------------------------------------------------------------------------
UTL_MkdirP():
	Verify and create the directory path passed in.  This is equivalent
	to the shell command (mkdir -p <pathname>).
------------------------------------------------------------------------------*/
int UTL_MkdirP(char *iDirectory, char *oErrorMsg)
{
	static char	mod[]="UTL_MkdirP";
	int		rc;
	char		*pNextLevel;
	char		workDir[256];
	char		buildDir[256];
	char		tmpDir[256];
	char		yErrorMsg[256];
	char		*yStrTok = NULL;

	memset((char *)buildDir, 0, sizeof(buildDir));
	memset((char *)yErrorMsg, 0, sizeof(yErrorMsg));
	sprintf(workDir, "%s", iDirectory);

	if ((pNextLevel=(char *)strtok_r(workDir, "/", &yStrTok)) == (char *)NULL)
	{
		sprintf(oErrorMsg, "Unable to create/verify directory (%s).  "
			"Cannot parse it.  %s=strtok(%s, /)", 
			iDirectory, pNextLevel, workDir);
		return(-1);
	}
	sprintf(buildDir, "/%s", pNextLevel);

	for(;;)
	{
		if ((rc=lCreateDirectory(buildDir, yErrorMsg)) == -1)
		{
			sprintf(oErrorMsg, "Unable to create/verify directory (%s).  %s",
				iDirectory, yErrorMsg);
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
} /* END: UTL_MkdirP() */

/*------------------------------------------------------------------------------
 * UTL_Appendfile(char *file1,
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
 int UTL_AppendFile(char *file1, char *file2, int deleteAfterAppend,
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
} /* END: UTL_AppendFile() */

/*------------------------------------------------------------------------------
 * UTL_ParsePath(	char *fullPathname,
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
 int UTL_ParsePath(char *fullPathname, char *directoryOnly, char *filenameOnly,
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

} /* END: UTL_ParsePath() */



/*------------------------------------------------------------------------------
 * UTL_ChangeString():
 * 	Manipulate a string according to the specified opCode.
 *
 * opCode:
 *	UTL_OP_NO_WHITESPACE :	removes all whitespace characters from string
 *	UTL_OP_CLEAN_WHITESPACE :	replaces all continuous whitespace characters
 * 				with a single space (used to 'clean up' a buffer
 *	UTL_OP_TRIM_WHITESPACE  :	removes all whitespace characters from the 
 *				beginning and end of the string
 *	UTL_OP_STRING_LOWER     :	converts all alpha characters to lowercase
 *	UTL_OP_STRING_UPPER	    :	converts all alpha characters to uppercase
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
int UTL_ChangeString(int opCode, char *string)
{
	int		sLength;
	int		i;
	char		*tmpString;
	char		*ptr2;
	char		*ptr;
	static char	mod[]="UTL_ChangeString";

	if((sLength = strlen(string)) == 0)
	{
		return(0);
	}
		
	switch(opCode)
	{
	case UTL_OP_NO_WHITESPACE :
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

	case UTL_OP_CLEAN_WHITESPACE :
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

	case UTL_OP_TRIM_WHITESPACE :
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

	case UTL_OP_STRING_LOWER :
		for (i=0; i<sLength; i++)
		{
			string[i] = tolower(string[i]);
		}
		break;

	case UTL_OP_STRING_UPPER :
		for (i=0; i<sLength; i++)
		{
			string[i] = toupper(string[i]);
		}
		break;

	default	:
		UTL_VarLog(mod, 0, "","","","",0,"Invalid opcode (%d)", opCode);
		return(-1);
	}

	return(0);
} /* END: UTL_ChangeString() */

/*------------------------------------------------------------------------------
 * UTL_ReadLogicalLine():
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
int UTL_ReadLogicalLine(char *buffer, int bufSize, FILE *fp)
{
	int		rc;
	char		str[BUFSIZ];
	char		tmpStr[BUFSIZ];
	char		*ptr;
	static char	mod[]="UTL_ReadLogicalLine";

	if(bufSize <= 0)
	{
		UTL_VarLog(mod, 0, "","","","",0,"bufSize(%d) must be > 0", bufSize);
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
		return((int)NULL);	
	}
	else
	{
		return(rc);

	}
} /* END: UTL_ReadLogicalLine() */
/*------------------------------------------------------------------------------
 * UTL_GetFieldByDelimiter(): 	This routine extracts the value of the desired
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
int UTL_GetFieldByDelimiter(char delimiter, char *buffer, int fieldNum, char *fieldValue)
{
	static char	mod[] = "UTL_GetFieldByDelimiter";
	register int	i;
	int		fieldLength, state, wc;

	fieldLength = state = wc = 0;

	fieldValue[0] = '\0';

	if(fieldNum <= 0)
	{
		UTL_VarLog(mod, 0, "","","","",0,
			"Field number (%d) must be > 0", fieldNum);
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
			{
                        	fieldValue[i] = fieldValue[i+1];
			}
                }
        	fieldLength = strlen(fieldValue);
#endif /* COMMENT */
        	return (fieldLength);
        }

	return(-1);
} /* END: UTL_GetFieldByDelimiter() */
/*------------------------------------------------------------------------------
 * UTL_FieldCount(): Count the number of fields in a delimited buffer
 *----------------------------------------------------------------------------*/
int UTL_FieldCount(char delimiter, char *buffer, int *numFields)
{
	char		*ptr;
	char		*ptr2;
	int		i;
	int		bufferLength;
	static char	mod[]="UTL_FieldCount";

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
} /* END: UTL_FieldCount() */
/*------------------------------------------------------------------------------
 * UTL_SubStringCount():	Return the number of instances of one string within
 *			another.  This function returns UTL_FAILURE when
 *			stringToSearchFor is NULL
 *----------------------------------------------------------------------------*/
int UTL_SubStringCount(char *sourceString, char *stringToSearchFor)
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
} /* END: UTL_SubStringCount() */
/*------------------------------------------------------------------------------
 * UTL_GetLabelList():
 * 	rewinds <fp>, then reads through the file, populating list array
 * 	with the lines belonging to <label>, and incrementing <*nEntries>.
 * 
 * 	Return values:
 * 		 * 0	- always
 *----------------------------------------------------------------------------*/
int UTL_GetLabelList(FILE *fp, int maxEntries, char *label, char **list,
			 					int *nEntries)
{
	static char	mod[]="UTL_GetLabelList";
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
		if ( UTL_ReadLogicalLine(buf, MYBUF_SIZE, fp) == 0 )
		{
			done=1;
			continue;
		}

		rc = UTL_ChangeString(UTL_OP_TRIM_WHITESPACE, buf);

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

		while ( UTL_ReadLogicalLine(buf, MYBUF_SIZE, fp) )
		{
			rc=UTL_ChangeString(UTL_OP_TRIM_WHITESPACE, buf);
			
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
} /* END: UTL_GetLabelList() */
/*------------------------------------------------------------------------------
 * UTL_UnlockNClose(int *fp)
 * 
 * 	Purpose:
 * 		Unlock and close open file pointer fp.
 * 
 * 	Return Codes:
 * 	0	Success
 *----------------------------------------------------------------------------*/
int UTL_UnlockNClose(int *fp)
{
	int		rc;
	static char	mod[]="UTL_UnlockNClose";

	rc=lockf(*fp, F_ULOCK, 0);
	close(*fp);

	return(0);
} /* END: UTL_UnlockNClose() */

/*------------------------------------------------------------------------------
 * UTL_OpenNLock(char *fileName,		input:  file to be opened & locked
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
int UTL_OpenNLock(char *fileName,
		int mode,
		int permissions,
		int timeLimit,
		int *fp)
{
	int		rc;
	int		counter=0;
	static char	mod[]="UTL_OpenNLock";

	if ((*fp=open(fileName, mode, permissions)) == -1)
	{
		UTL_VarLog(mod, 0,
				"","","","",0,"open(%s,%o,%o) failed. errno=%d",
				fileName,mode, permissions, errno);
		return(-1);
	}

	while (counter <= timeLimit)
	{
		if ((rc=lockf(*fp, F_TLOCK, 0)) == -1) 
		{
			UTL_VarLog(mod, 0,
			"","","","",0,"lockf(%d, %d, %d) failed. errno=%d",
					*fp, F_TLOCK, 0, errno);
			sleep(SLEEPTIME_LOCK);
			counter+=SLEEPTIME_LOCK;
			continue;
		}
		break;
	}

	if (counter > timeLimit)
	{
		UTL_VarLog(mod, 0,
			"","","","",0,"Unable to obtain lock on %s",fileName);
		rc=lockf(*fp, F_ULOCK, 0);
		close(*fp);
		return(-1);
	}

	return(0);

} /* END: UTL_OpenNLock() */

/*------------------------------------------------------------------------------
 * UTL_FileAccessTime(char *fileName,	input: file or directory
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
int UTL_FileAccessTime(char *fileName,
			char *dateFormat,
			char *timeFormat,
			char *date,
			char *time)
{
	int		rc;
	static char	mod[]="UTL_FileAccessTime";
	struct stat	statBuf;
	struct tm	*PT_time;

	if ( (rc=stat(fileName, &statBuf)) != 0)
	{
		UTL_VarLog(mod, 0,
			"","","","",0,"Could not stat %s. errno=%d",
			fileName,errno);
		return(-1);
	}
		
	PT_time=localtime(&(statBuf.st_mtime));
	strftime(date,sizeof(date), dateFormat, PT_time);
	strftime(time,sizeof(time), timeFormat, PT_time);
	return(0);

} /* END: UTL_FileAccessTime() */

/*------------------------------------------------------------------------------
 * UTL_GetFileType(char *fileName)
 * 
 * 	Purpose:
 * 		Determine whether a system file is a file, directory, pipe,
 *		or link.
 * 
 * 	Return Codes:
 * 	3	UTL_IS_PIPE
 * 	2	UTL_IS_LINK
 * 	1	UTL_IS_DIRECTORY
 * 	0	UTL_IS_FILE
 *     -1	UTL_FAILURE
 *----------------------------------------------------------------------------*/
int UTL_GetFileType(char *fileName)
{
	int		rc;
	static char	mod[]="UTL_GetFileType";
	struct stat	statBuf;
	mode_t		mode;

	if ( (rc=stat(fileName, &statBuf)) != 0)
	{
		UTL_VarLog(mod, 0,
			"","","","",0,"Could not stat %s. errno=%d", 
			fileName, errno);
		return(UTL_FAILURE);
	}
		
	mode = statBuf.st_mode & S_IFMT;

	if ( mode == S_IFIFO )
	{
		return(UTL_IS_PIPE);
	}
		
	if ( mode == S_IFDIR )
	{
		return(UTL_IS_DIRECTORY);
	}
		
	if ( mode == S_IFREG )
	{
		return(UTL_IS_FILE);
	}
		
	return(UTL_IS_OTHER);

} /* END: UTL_GetFileType() */

/*------------------------------------------------------------------------------
 * UTL_ReplaceChar(char *buffer,		The string to be modified 
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
 * 		 0	UTL_SUCCESS
 *		-1 	UTL_FAILURE
 *----------------------------------------------------------------------------*/
int UTL_ReplaceChar(char *buffer, char oldChar, char newChar, int instances)
{
	int		rc;
	static char	mod[]="UTL_ReplaceChar";
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

} /* END: UTL_ReplaceChar() */

/*------------------------------------------------------------------------------
 * UTL_ChangeRecord(char delimiter,	field separator
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
int UTL_ChangeRecord(char delimiter, char *fileName, int keyField,
			char *keyValue, int fieldNum, char *newValue)
{
	int		rc;
	static char	mod[]="UTL_ChangeRecord";
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
		UTL_VarLog(mod, 0, "","","","",0,"Cannot access %s. errno=%d", fileName, errno);
		return(-1);
	}

	memset((char *)timeBuf, 0, sizeof(timeBuf));
	time(&tm);
	PT_time=localtime(&tm);
	strftime(timeBuf, sizeof(timeBuf),"%H%M%S", PT_time);

	sprintf(tmpFileName, "/tmp/%s.%d.%s", mod, getpid(), timeBuf);
	
	if ( (fpOut=fopen(tmpFileName, "w")) == NULL)
	{
		UTL_VarLog(mod, 0, "","","","",0,"Cannot open %s for writing. errno=%d",
							 tmpFileName, errno);
		return(-1);
	}

	if ( (fpIn=fopen(fileName, "r")) == NULL)
	{
		UTL_VarLog(mod, 0, "","","","",0,"Cannot open %s for reading. errno=%d",
							 fileName, errno);
		fclose(fpOut);
		remove(tmpFileName);
		return(-1);
	}

	memset((char *)buf, 0, BUFSIZ);
	while ( UTL_ReadLogicalLine(buf, BUFSIZ, fpIn) )
	{
		memset((char *)field, 0, sizeof(field));
		if (UTL_GetFieldByDelimiter(delimiter, buf, keyField, field) == -1)
		{
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				UTL_VarLog(mod, 0,
					"","","","",0,"Error writing <%s> to %s. errno=%d",
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
				UTL_VarLog(mod, 0,
					"","","","",0,"Error writing <%s> to %s. errno=%d",
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

		if ((delimPos1=UTL_StringLocator(buf, delimiter,
							fieldNum-1)) == -1)
		{
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				UTL_VarLog(mod, 0,
					"","","","",0,"Error writing <%s> to %s. errno=%d",
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
		if ((delimPos2=UTL_StringLocator(buf, delimiter,
							fieldNum)) == -1)
		{
			delimPos2=delimPos1+1;
		}

		if ((rc=fprintf(fpOut, "%.*s", delimPos1+1, buf)) < 0)
		{
			UTL_VarLog(mod, 0,
				"","","","",0,
				"Error writing <%s> to %s. errno=%d.",
				buf, tmpFileName, errno);
			fclose(fpIn);
			fclose(fpOut);
			remove(tmpFileName);
			return(-1);
		}

		if ((rc=fprintf(fpOut, "%s", newValue)) < 0)
		{
			UTL_VarLog(mod, 0,
				"","","","",0,
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
				UTL_VarLog(mod, 0,
					"","","","",0,
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
				UTL_VarLog(mod, 0,
					"","","","",0,
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

} /* END: UTL_ChangeRecord() */

/*------------------------------------------------------------------------------
 * UTL_RemoveRecord(char delimiter,	character used to separate fields
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
int UTL_RemoveRecord(char delimiter, char *fileName, int keyField, char *keyValue)
{
	int		rc=0;
	static char	mod[]="UTL_RemoveRecord";
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
		UTL_VarLog(mod, 0, "","","","",0,
			"Cannot access %s. errno=%d", fileName, errno);
		return(-1);
	}

	memset((char *)timeBuf, 0, sizeof(timeBuf));
	time(&tm);
	PT_time=localtime(&tm);
	strftime(timeBuf, sizeof(timeBuf),"%H%M%S", PT_time);

	sprintf(tmpFileName, "/tmp/%s.%d.%s", mod, getpid(), timeBuf);
	
	if ( (fpOut=fopen(tmpFileName, "w")) == NULL)
	{
		UTL_VarLog(mod, 0, "","","","",0,
			"Cannot open %s for writing. errno=%d",
							 tmpFileName, errno);
		return(-1);
	}

	if ( (fpIn=fopen(fileName, "r")) == NULL)
	{
		UTL_VarLog(mod, 0, "","","","",0,
			"Cannot open %s for reading. errno=%d",
							 fileName, errno);
		fclose(fpOut);
		remove(tmpFileName);
		return(-1);
	}

	memset((char *)buf, 0, sizeof(buf));
	while (UTL_ReadLogicalLine(buf, BUFSIZ, fpIn) )
	{
		memset((char *)field, 0, sizeof(field));
		if ((rc=UTL_GetFieldByDelimiter(delimiter, buf, keyField, field)) == -1)
		{
			if ((rc=fprintf(fpOut, "%s", buf)) < 0)
			{
				UTL_VarLog(mod, 0,
					"","","","",0,
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
				UTL_VarLog(mod, 0,
					"","","","",0,
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

} /* END: UTL_RemoveRecord() */

/*------------------------------------------------------------------------------
 * UTL_GetMaxKey( char delimiter,		character used to separate fields
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
int UTL_GetMaxKey(char delimiter, char *fileName, int keyField,
							char *maxKeyValue)
{
	int		rc=0;
	static char	mod[]="UTL_GetMaxKey";
	FILE		*fpIn;
	char		buf[BUFSIZ];	/* record size (BUFSIZ) in stdio.h */
	char		holdKey[MYBUF_SIZE];
	char		tmpKey[MYBUF_SIZE];
	
	if (access(fileName, R_OK) != 0)
	{
		UTL_VarLog(mod, 0, "","","","",0,
			"Cannot access %s. errno=%d", fileName, errno);
		return(-1);
	}

	if ( (fpIn=fopen(fileName, "r")) == NULL)
	{
		UTL_VarLog(mod, 0, "","","","",0,
			"Cannot open %s for reading. errno=%d",
							 fileName, errno);
		return(-1);
	}

	memset((char *)buf, 0, sizeof(buf));
	memset((char *)holdKey, 0, sizeof(holdKey));
	while (UTL_ReadLogicalLine(buf, BUFSIZ, fpIn) )
	{
		memset((char *)tmpKey, 0, sizeof(tmpKey));
		if ((rc=UTL_GetFieldByDelimiter(delimiter, buf, keyField,
						tmpKey)) == -1)
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

} /* END: UTL_GetMaxKey() */

/*------------------------------------------------------------------------------
 * UTL_StringLocator(char *buf,		
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
int UTL_StringLocator(char *buf, char delimiter, int occurance)
{
	int		rc=0;
	static char	mod[]="UTL_StringLocator";
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
} /* END: UTL_StringLocator */

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

/*------------------------------------------------------------------------------
UTL_StringMatch(): Matches the given string based on the pattern.

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
int UTL_StringMatch(char *iPattern, char *iString)
{
	static char	mod[] = "UTL_StringMatch";
	int		rc;
	regex_t 	preg;

	if(iPattern[0] == '\0' || iString[0] == '\0')
	{
		return(0);
	}

	if(regcomp(&preg, iPattern, 0) != 0)
	{
		return(0);
	}

	if(regexec(&preg, iString, 0, 0, 0) != 0)
	{
		rc = 0;
	}
	else
	{
		rc = 1;
	}

	regfree(&preg);

	return(rc);
} /* END UTL_StringMatch() */
/*------------------------------------------------------------------------------
UTL_GetProfileString():
------------------------------------------------------------------------------*/
int UTL_GetProfileString(char *section, char *key, char *defaultVal,
				char *value, long bufSize, char *file, char *zErrorMsg)
{
	static char	mod[] = "UTL_GetProfileString";
	FILE		*fp;
	char		*ptr;
	char		line[1024];
	char		currentSection[80],lhs[512], rhs[512];
	short		found, inSection, ignoreSection;

	sprintf(value, "%s", defaultVal);

	if(key[0] == '\0')
	{
		sprintf(zErrorMsg, "Empty key.");
		return(-1);
	}
	if(file[0] == '\0')
	{
		sprintf(zErrorMsg, "Empty file name.");
		return(-1);
	}

	ignoreSection = 0;
	if(section[0] == '\0')
	{
		ignoreSection = 1;
	}

	if(bufSize <= 0)
	{
		sprintf(zErrorMsg,
				"Return bufSize (%ld) must be > 0.", bufSize);
		return(-1);
	}

	if((fp = fopen(file, "r")) == NULL)
	{
		sprintf(zErrorMsg,
				"Failed to open file (%s) for reading, [%d: %s]",
				file, errno, strerror(errno));

		return(-1);
	}

	memset(line, 0, sizeof(line));

	inSection = 0;
	found = 0;
	while(UTL_ReadLogicalLine(line, sizeof(line)-1, fp))
	{
		/*
		 * Strip \n from the line if it exists
		 */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		if(ignoreSection == 0)
		{
			if(!inSection)
			{
				if(line[0] != '[')
				{
					memset(line, 0, sizeof(line));
					continue;
				}
	
				memset(currentSection, 0, 
							sizeof(currentSection));
				sscanf(&line[1], "%[^]]", currentSection);
	
				if(strcmp(section, currentSection) != 0)
				{
					memset(line, 0, sizeof(line));
					continue;
				}
	
				inSection = 1;
				memset(line, 0, sizeof(line));
				continue;
			} /* if ! inSection */
			else
			{
				/*
			 	 * If we are already in a section and have 
				 * encountered another section, get out of 
				 * the loop.
			 	 */
				if(line[0] == '[')
				{
					memset(line, 0, sizeof(line));
					break;
				}
			} /* if inSection */
		} /* if we are NOT ignoring sections */

		memset(lhs, 0, sizeof(lhs));
		memset(rhs, 0, sizeof(rhs));

		if ((ptr=strchr(line, '=')) == NULL)
		{
			memset(line, 0, sizeof(line));
			continue;
		}

		sscanf(line, "%[^=]=%[^=]", lhs, rhs);

		UTL_ChangeString(UTL_OP_TRIM_WHITESPACE, lhs);

		if(strcmp(lhs, key) != 0)
		{
			memset(line, 0, sizeof(line));
			continue;
		}

		found = 1;

		sprintf(value, "%.*s", bufSize, rhs);

		break;
	} /* while UTL_ReadLine != NULL */

	fclose(fp);

	if(!found)
	{
		if(ignoreSection == 0)
		{
			sprintf(zErrorMsg, 
				"Failed to find value of key (%s) under "
				"section (%s) in file (%s)", 
				key, section, file);
		}
		else
		{
			sprintf(zErrorMsg, 
				"Failed to find value of key (%s) in file (%s)",
				key, file);
		}
		return(-1);
	}

#ifdef COMMENT
	if(ignoreSection == 0)
	{
		gaVarLog(mod, 1, 
			"Value of key(%s) under section(%s) in file(%s) is "
			"(%s)", 
			key, section, file, value);
	}
	else
	{
		gaVarLog(mod, 1, 
			"Value of key(%s) in file(%s) is (%s)", 
			key, file, value);
	}
#endif /* COMMENT */

	return(0);

} /* END UTL_GetProfileString() */

/*------------------------------------------------------------------------------
UTL_FileCopy(): Copy a file 
------------------------------------------------------------------------------*/
int UTL_FileCopy(char *zSource, char *zDestination, int zPerms, char *zErrorMsg)
{
	struct stat	yStat;
	int			yFdSource;
	int			yFdDestination;
	int			yBytesRead;
	void		*yPtrBytes;

	/*
	** Clear all buffers
	*/
	*zErrorMsg = 0;

	/*
	** Validate parameter: source
	*/
	if(! zSource || ! *zSource)
	{
		sprintf(zErrorMsg, "No source file name specified.");

		return(-1);
	}

	/*
	** Validate parameter: destination
	*/
	if(! zDestination || ! *zDestination)
	{
		sprintf(zErrorMsg, "No destination file name specified.");

		return(-1);
	}

	/*
	** Open the source file for reading.
	*/
	if((yFdSource = open(zSource, O_RDONLY)) < 0)
	{
		sprintf(zErrorMsg,
				"Failed to open source file for reading, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				errno, strerror(errno), zSource, zDestination);

		return(-1);
	}

	/*
	** Open the destination file for writing.
	*/
	yFdDestination = open(zDestination, O_WRONLY|O_CREAT|O_TRUNC, 
							zPerms);
	if(yFdDestination < 0)
	{

		sprintf(zErrorMsg,
				"Failed to open destination file for writing, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				errno, strerror(errno), zSource, zDestination);

		close(yFdSource);

		return(-1);
	}

	/*
	** Get the file information
	*/
	if(fstat(yFdSource, &yStat) < 0)
	{
		sprintf(zErrorMsg,
				"Failed to stat source file, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				errno, strerror(errno), zSource, zDestination);

		close(yFdSource);
		close(yFdDestination);

		return(-1);
	}

	/*
	** Allocate the memory
	*/
	if((yPtrBytes = (void *)malloc(yStat.st_size)) == NULL)
	{
		sprintf(zErrorMsg,
				"Failed to malloc (%d) bytes for source file size, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				yStat.st_size, errno, strerror(errno), zSource, zDestination);

		close(yFdSource);
		close(yFdDestination);

		return(-1);
	}
			
	/*
	** Read the file
	*/
	yBytesRead = read(yFdSource, yPtrBytes, yStat.st_size);

	if(yBytesRead != yStat.st_size)
	{
		sprintf(zErrorMsg,
				"Failed to read (%d) bytes from source file, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				yStat.st_size, errno, strerror(errno), zSource, zDestination);

		free(yPtrBytes);

		close(yFdSource);
		close(yFdDestination);

		return(-1);
	}

	/*
	** Write the contents of the file to the destination
	*/
	if(write(yFdDestination, yPtrBytes, yStat.st_size) != yStat.st_size)
	{
		sprintf(zErrorMsg,
				"Failed to write (%d) of source to destination, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				yStat.st_size, errno, strerror(errno), zSource, zDestination);

		free(yPtrBytes);

		close(yFdSource);
		close(yFdDestination);

		return(-1);
	}

 	/*
	** Close the file and free the buffer
  	*/
	free(yPtrBytes);

	close(yFdSource);
	close(yFdDestination);
		
	return(0);

} /* END: UTL_FileCopy() */

/*------------------------------------------------------------------------------
UTL_GetArcFifoDir()
------------------------------------------------------------------------------*/
int UTL_GetArcFifoDir(int iId, char *oFifoDir,
			int iFifoDirSize, char *oErrorMsg, int iErrorMsgSize)
{
	static char 	mod[] = "UTL_GetArcFifoDir";
	char			*ispbase;
	char			globalConfigFile[128];
	char			tmpFifoDir[128];
	char			tmpFifoDir2[256];
	char			yErrorMsg[256];
	int				rc;

	memset((char *)oFifoDir, '\0', iFifoDirSize);
	memset((char *)oErrorMsg, '\0', iErrorMsgSize);
	memset((char *)yErrorMsg, '\0', sizeof(yErrorMsg));
	memset((char *)tmpFifoDir2, '\0', sizeof(tmpFifoDir2));

    if ((ispbase = (char *)getenv("ISPBASE")) == NULL)
	{
		sprintf(oErrorMsg, "%s", "Unable to get FIFO directory.  "
				"Environment variable is $ISPBASE not set.  "
				"Set and retry.");
		return(-1);
	}

    if ( iId >= MAX_FIFO_SUBDIRS )
	{
		sprintf(oErrorMsg, "Error: Invalid FIFO id received (%d).  "
				"It must be < MAX_FIFO_SUBDIRS(%d).", iId, MAX_FIFO_SUBDIRS);
		return(-1);
	}
	sprintf(globalConfigFile, "%s/Global/.Global.cfg", ispbase);
	sprintf(tmpFifoDir, "%s/temp", ispbase);

	if ((rc = sGetNameValue("", "FIFO_DIR",
			tmpFifoDir, tmpFifoDir2, sizeof(tmpFifoDir2) - 1, globalConfigFile,
			yErrorMsg)) != 0)
	{
		sprintf(oErrorMsg, "Unable to get FIFO directory from (%s).  %s",
			globalConfigFile, yErrorMsg);
		return(-1);		/* Error message is set in oErrorMsg. */
	}
	if ( tmpFifoDir2[strlen(tmpFifoDir2) - 1] == '/' )
	{
		tmpFifoDir2[strlen(tmpFifoDir2) - 1] = '\0';
	}

    if ( strcmp (arcFifoDirs[iId].subDir, "-1" ) == 0 )
    {
        sprintf(oFifoDir, "%s", tmpFifoDir2);
    }
    else
    {
        sprintf(oFifoDir, "%s/%s", tmpFifoDir2, arcFifoDirs[iId].subDir);
    }

	if ((rc = UTL_MkdirP(oFifoDir, yErrorMsg)) == -1)
	{
		sprintf(oErrorMsg, "Unable to create FIFO directory.  %s",
			yErrorMsg);
		return(-1);
	}
	sprintf(oErrorMsg, "Successfully retrieved/verified directory (%s).",
			oFifoDir);
	
	return(0);
} /* END: UTL_GetArcFifoDir() */

/*------------------------------------------------------------------------------
Function:	sGetNameValue
Purpose:	This routine gets a name value pair from a file. If no header
		is specified, the first instance of the name found in the file
		is processed.
		This routine was adapted from gaGetProfileString, but has no
		dependencies on gaUtils.
		Parameters: 
		section: name of section in file, e.g. [Header]. If this is
			NULL (""), the whole file is searched regardless of
			headers.
		name: The "name" of a "name=value" pair.
		defaultVal: The value to return if name is not found in file.
		value: The "value" of a "name=value" pair. This what you are
			looking for in the file.
		bufSize: Maximum size of the value that can be returned. 
			This is so the routine will not cause you to core dump.
		file: the name of the file to search in
		err_msg: The errror message if any.
		Values returned are: 0 (success), -1 (failure)
Author:		George Wenzel
Date:		06/17/99
------------------------------------------------------------------------------*/
static int sGetNameValue(char *section, char *name, char *defaultVal,
			char *value, int bufSize, char *file, char *err_msg)
{
	char	mod[] = "sGetNameValue";
	FILE		*fp;
	char		line[512];
	char		currentSection[80],lhs[256], rhs[256];
	int		found, inDesiredSection=0, sectionSpecified=1;

	sprintf(value, "%s", defaultVal);
	err_msg[0]='\0';

	if(section[0] == '\0')
	{
		/* If no section specified, we're always in desired section */
		sectionSpecified=0;
		inDesiredSection=1;	
	}
	if(name[0] == '\0')
	{
		sprintf(err_msg,"No name passed to %s.", mod);
		return(-1);
	}
	if(file[0] == '\0')
	{
		sprintf(err_msg,"No file name passed to %s.", mod);
		return(-1);
	}
	if(bufSize <= 0)
	{
		sprintf(err_msg,
		"Return bufSize (%d) passed to %s must be > 0.", bufSize, mod);
		return(-1);
	}

	if((fp = fopen(file, "r")) == NULL)
	{
		sprintf(err_msg,"Failed to open file <%s>. errno=%d. [%s]",
				file, errno, strerror(errno));
		return(-1);
	}

	memset(line, 0, sizeof(line));
	found = 0;
	while(fgets(line, sizeof(line)-1, fp))
	{
		/*  Strip \n from the line if it exists */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		trim_whitespace(line);

		if(!inDesiredSection)
		{
			if(line[0] != '[')
			{
				memset(line, 0, sizeof(line));
				continue;
			}

			memset(currentSection, 0, sizeof(currentSection));
			sscanf(&line[1], "%[^]]", currentSection);

			if(strcmp(section, currentSection) != 0)
			{
				memset(line, 0, sizeof(line));
				continue;
			}
			inDesiredSection = 1;
			memset(line, 0, sizeof(line));
			continue;
		} 
		else
		{
			/* If we are already in a section and have encountered
			  another section AND a section was specified then
			   get out of the loop.  */
			if( line[0] == '[' && sectionSpecified )
			{
				memset(line, 0, sizeof(line));
				break;
			}
		} 

		memset(lhs, 0, sizeof(lhs));
		memset(rhs, 0, sizeof(rhs));

		if(sscanf(line, "%[^=]=%[^=]", lhs, rhs) < 2)
		{
			memset(line, 0, sizeof(line));
			continue;
		}
		trim_whitespace(lhs);
		if(strcmp(lhs, name) != 0)
		{
			memset(line, 0, sizeof(line));
			continue;
		}
		found = 1;
		sprintf(value, "%.*s", bufSize, rhs);
		break;
	} /* while fgets */
	fclose(fp);
	if (!found)
	{
		if (sectionSpecified)
			sprintf(err_msg,
			"Failed to find <%s> under section <%s> in file <%s>.", 
				name, section, file);
		else
			sprintf(err_msg,
			"Failed to find <%s> entry in file <%s>.", name, file);
		return(-1);
	}
	return(0);
} /* END: sGetNameValue() */

/*----------------------------------------------------------------------------
Trim white space from the front and back of a string.
----------------------------------------------------------------------------*/
static int trim_whitespace(char *string)
{
	int sLength;
	char *tmpString;
	char *ptr;

	if((sLength = strlen(string)) == 0) return(0);

	tmpString = (char *)calloc((size_t)(sLength+1), sizeof(char));
	ptr = string;

	if ( isspace(*ptr) )
	{
		while(isspace(*ptr)) ptr++;
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
	return(0);

} /* END: trim_whitespace() */
