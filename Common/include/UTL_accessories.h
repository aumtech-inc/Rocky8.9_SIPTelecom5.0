/*------------------------------------------------------------------------------
File:		UTL_accessories.h

Purpose:	Contains function prototypes of all UTL accessory 
		routines and related header information.

Author:		Aumtech Inc.

Date:		2000/09/97 sja	Created the file
Update:	2000/05/17 sja	Changed UTL_GetField to UTL_GetFieldByDelimiter
------------------------------------------------------------------------------*/
#ifndef UTL_accessories_H	/* Just preventing multiple includes... */
#define UTL_accessories_H

#include <sys/types.h>

#define	UTL_OP_NO_WHITESPACE	0
#define	UTL_OP_CLEAN_WHITESPACE	1
#define UTL_OP_TRIM_WHITESPACE	2
#define	UTL_OP_STRING_LOWER	3
#define	UTL_OP_STRING_UPPER	4

#define UTL_IS_PIPE		3
#define UTL_IS_DIRECTORY	2
#define UTL_IS_OTHER		1
#define UTL_IS_FILE		0
#define UTL_FAILURE		-1

/*------------------------------
Function Prototypes
------------------------------*/

int UTL_SetGlobalString(char *globalVar, char *newValue);
int UTL_GetDebugLevel();
int UTL_GetGlobalString(char *globalVar, char *value);
int UTL_GetLabelList(FILE *fp, int maxEntries, char *label, char **list,
					int *nEntries);
int UTL_VarLog(char *moduleName, int reportMode, char *groupId, 
		char *resourceName, char *arcService, char *application, 
		int messageId, char *messageFormat, ...);
int UTL_Log(char *moduleName, int debugLevel, char *message);
int UTL_GetFieldByDelimiter(char delimiter, char *buffer, int fieldNum, 
		char *fieldValue);
int UTL_FieldCount(char delimiter, char *buffer, int *numFields);
int UTL_ReadLogicalLine(char *buffer, int bufSize, FILE *fp);
int UTL_ChangeString(int opCode, char *string);
int UTL_SubStringCount(char *sourceString, char *stringToSearchFor);

int UTL_OpenNLock(char *fileName, int mode, int permissions, int timeLimit,
						                int *fp);
int UTL_UnlockNClose(int *fp);
int UTL_FileAccessTime(char *fileName, char *dateFormat, char *timeFormat,
						char *date, char *time);
int UTL_GetFileType(char *fileName);
int UTL_ReplaceChar(char *buffer, char oldChar, char newChar, int instances);
int UTL_ChangeRecord(char delimiter, char *fileName, int keyField,
				char *keyValue, int fieldNum, char *newValue);
int UTL_GetMaxKey(char delimiter, char *fileName, int keyField,
							char *maxKeyValue);
int UTL_ChangeRecord(char delimiter, char *fileName, int keyField,
				char *keyValue, int fieldNum, char *newValue);
int UTL_RemoveRecord(char delimiter, char *fileName, int keyField,
							char *newValue);
int UTL_MkdirP(char *iDirectory, char *oErrorMsg);


/* Funtion prototypes for service creation */

int UTL_StrCpy(char *dest, char *source);
int UTL_StrNCpy(char *dest, char *source, int len);
int UTL_StrCmp(char *string1, char *string2);
int UTL_StrNCmp(char *string1, char *string2, int len);
int UTL_StrCat(char *string1, char *string2);
int UTL_GetDateTime(char *date, char *dateFormat);
int UTL_MakeDirectory(char *dirName, mode_t permissions);
int UTL_ParsePath(char *fullPathname, char *directoryOnly,
				char *filenameOnly, char *errorMsg);
int UTL_AppendFile(char *file1, char *file2, int deleteAfterAppend,
						char *errorMsg);
int UTL_GetArcFifoDir(int iId, char *oFifoDir,
            int iFifoDirSize, char *oErrorMsg, int iErrorMsgSize);

#endif /* UTL_accessories_H */
