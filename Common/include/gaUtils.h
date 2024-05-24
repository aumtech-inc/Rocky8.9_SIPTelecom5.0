/*------------------------------------------------------------------------------
File:		gaUtil.h

Purpose:	Contains function prototypes of all global accessory 
		routines and related header information.

Author:		Sandeep Agate

Update:	09/09/97 sja	Created the file
Update:	11/06/97 djb	Added gaSubStringCount
Update:	12/02/97 djb	Added gaFieldCount, gaReadLogicalLine, ...
Update:	01/19/98 sja	Added gaGetLabelList
Update:	01/20/98 sja	Changed prototype of gaReadLogicalLine
Update:	01/22/98 djb	Added all other prototypes for completion
Update:	01/06/00 djb	Added gaAppendFile and gaParsePath prototypes
Update:	2001/01/16 sja	Added function prototypes for gaNT functions.
------------------------------------------------------------------------------*/
#ifndef gaUtil_H	/* Just preventing multiple includes... */
#define gaUtil_H

#include <sys/types.h> 


#define	OP_NO_WHITESPACE	0
#define	OP_CLEAN_WHITESPACE	1
#define OP_TRIM_WHITESPACE	2
#define	OP_STRING_LOWER		3
#define	OP_STRING_UPPER		4

#define GA_IS_PIPE		3
#define GA_IS_DIRECTORY		2
#define GA_IS_OTHER		1
#define GA_IS_FILE		0
#define GA_FAILURE		-1

/*------------------------------
Function Prototypes
------------------------------*/

int gaSetGlobalString(char *globalVar, char *newValue);
int gaGetDebugLevel();
int gaGetGlobalString(char *globalVar, char *value);
int gaGetLabelList(FILE *fp, int maxEntries, char *label, char **list,
					int *nEntries);
int gaVarLog(char *moduleName, int debugLevel, char *messageFormat, ...);
int gaLog(char *moduleName, int debugLevel, char *message);
int gaGetField(char delimiter, char *buffer, int fieldNum, char *fieldValue);
int gaFieldCount(char delimiter, char *buffer, int *numFields);
int gaReadLogicalLine(char *buffer, int bufSize, FILE *fp);
int gaChangeString(int opCode, char *string);
int gaSubStringCount(char *sourceString, char *stringToSearchFor);

int gaOpenNLock(char *fileName, int mode, int permissions, int timeLimit,
						                int *fp);
int gaUnlockNClose(int *fp);
int gaFileAccessTime(char *fileName, char *dateFormat, char *timeFormat,
						char *date, char *time);
int gaGetFileType(char *fileName);
int gaReplaceChar(char *buffer, char oldChar, char newChar, int instances);
int gaChangeRecord(char delimiter, char *fileName, int keyField,
				char *keyValue, int fieldNum, char *newValue);
int gaGetMaxKey(char delimiter, char *fileName, int keyField,
							char *maxKeyValue);
int gaChangeRecord(char delimiter, char *fileName, int keyField,
				char *keyValue, int fieldNum, char *newValue);
int gaRemoveRecord(char delimiter, char *fileName, int keyField,
							char *newValue);
int gaMkdirP(char *iDirectory, char *oErrorMsg);


/* Funtion prototypes for service creation */

int gaStrCpy(char *dest, char *source);
int gaStrNCpy(char *dest, char *source, int len);
int gaStrCmp(char *string1, char *string2);
int gaStrNCmp(char *string1, char *string2, int len);
int gaStrCat(char *string1, char *string2);
int gaGetDateTime(char *date, char *dateFormat);
int gaMakeDirectory(char *dirName, mode_t permissions);
int gaParsePath(char *fullPathname, char *directoryOnly,
				char *filenameOnly, char *errorMsg);
int gaAppendFile(char *file1, char *file2, int deleteAfterAppend,
						char *errorMsg);

int gaGetProfileString(char *section, char *key, char *defaultVal,
				char *value, long bufSize, char *file);
int gaGetProfileSection(char *section, char *value, long bufSize, char *file);
int gaGetProfileSectionNames(char *value, long bufSize, char *file);

#endif /* gaUtil_H */
