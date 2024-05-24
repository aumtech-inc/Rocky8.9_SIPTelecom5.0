#ident    "@(#)ISPCommon.h 96/02/03 Release 2.2 Copyright 1996 Aumtech, Inc."
/*
#	ISP Common defines and api prototypes.
#	Copyright (c) 1993, 1994, Aumtech
#	Network Systems Information Services Platform (ISP)
#	All Rights Reserved.
#
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Aumtech ISP
#	which is protected under the copyright laws of the 
#	United States and a trade secret of Aumtech. It may 
#	not be used, copied, disclosed or transferred other 
#	than in accordance with the written permission of Aumtech.
#
#	The copyright notice above does not evidence any actual
#	or intended publication of such source code.
#
#  WARNING: Do NOT change this file. Changes will affect operations.
#----------------------------------------------------------------------------*/
#ifndef _ISPCOMMON_H	/* for preventing multiple defines. */
#define _ISPCOMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/utsname.h>			/* for utsname structure */
#include <termio.h>
#include <ctype.h>
#include <sys/signal.h>
#include <sys/stat.h>  
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/msg.h>

/* Return codes for Utility library.  */
#define 	UTL_SUCCESS		0
#define 	UTL_FAILURE		-1
#define 	UTL_FULL_PATH_LENGTH	-2
#define 	UTL_FILE_COPY_FAIL	-3
#define 	UTL_FILE_EXISTS		-4
#define 	UTL_NO_FILE_EXISTS	-5

/* Return codes for Index Array library.  */
#define 	ARY_SUCCESS		0
#define 	ARY_FAILURE		-1
#define 	ARY_KEY_NOT_FOUND	-2
#define 	ARY_DATA_CORRUPTION	-3
#define 	ARY_NO_DATA_FOUND	-4

/* Return codes for Call Detail Recording library.  */
#define 	CDR_SUCCESS	0
#define 	CDR_FAILURE	-1
#define 	CDR_OVERFLOW	-2

/* Definition for Custom Event Recording API */
#define 	CEV_STRING	1
#define 	CEV_INTEGER	2

/* Definition for Call Detail Recording API */
#define 	CDR_START	1
#define 	CDR_END		2
#define 	CDR_CUSTOM		3
#define 	SERVICE_SUCCESS	1
#define 	SERVICE_FAILURE	2

/* Definitions for Business Hours return codes. */
#define OPEN_HRS		1
#define CLOSED_HRS		2
#define OUT_HRS			3

/* These are multipaly defined. */
#ifndef REPORT_NORMAL
#define REPORT_NORMAL            1
#endif

#ifndef REPORT_VERBOSE
#define REPORT_VERBOSE           2
#endif

#ifndef REPORT_DETAIL
#define REPORT_DETAIL            128
#endif

#ifndef REPORT_EVENT
#define REPORT_EVENT			256           
#endif

#ifndef REPORT_DIAGNOSE
#define REPORT_DIAGNOSE          64
#endif

#ifndef REPORT_CDR
#define REPORT_CDR           	16
#endif

#ifndef REPORT_CEV
#define REPORT_CEV           	32
#endif

/* For UTL_ChangeString() */
#define	UTL_OP_NO_WHITESPACE	0
#define	UTL_OP_CLEAN_WHITESPACE	1
#define UTL_OP_TRIM_WHITESPACE	2
#define	UTL_OP_STRING_LOWER	3
#define	UTL_OP_STRING_UPPER	4

/* For UTL_GetFileType() */
#define UTL_IS_PIPE		3
#define UTL_IS_DIRECTORY	2
#define UTL_IS_OTHER		1
#define UTL_IS_FILE		0
#define UTL_FAILURE		-1

/*
 *	DDN: 02/15/2010 Added Conversion routines
 */
typedef char *Str;
typedef char Character;
typedef Str Date;
typedef float Float;
typedef long Long;
typedef double Double;
typedef int Integer;
typedef Str Phone;
typedef Str SocialSecurity;
typedef Str String;
typedef Str Time;

int IntToString (Integer Source, String Destination);
int LongToString (Long Source, String Destination);
int DoubleToString (Double Source, String Destination);
int FloatToString (Float Source, String Destination);
int DateToString (Date Source, String Destination);
int PhoneToString (Phone Source, String Destination);
int SSNToString (SocialSecurity Source, String Destination);
int TimeToString (Time Source, String Destination);
int StringToInt (String Source, Integer * Destination);
int StringToFloat (String Source, Float * Destination);
int StringToDouble (String Source, Double * Destination);
int StringToLong (String Source, Long * Destination);
int StringToDate(String Source, Date Destination);
int StringToPhone(String Source, Phone Destination);
int StringToSSN(String Source, SocialSecurity Destination);
int StringToTime(String Source, Time Destination);

/*
 * Function Prototypes
 */
int CDR_ServiceTran(int, const char *, const char *, int, const char *, const char *);
int CDR_UserBilling(const char *, const char *, const char *, const char *, const char *);
int CDR_CustomEvent(int, const void *, int);

int ARY_IndxaryInit(const char *, int);
int ARY_IndxaryRetrieve(const char *, int, const char *, int, char *);

int UTL_BusinessHours(char *,char *,char *,int *);
int UTL_GetField(const char *, int, char *);
int UTL_Substring(char *, const char *, int, int);
int UTL_AddToFile(const char *, const char *);
int UTL_CreateFile(const char *);
int UTL_DeleteFile(const char *);
int UTL_SendToMonitor(int, const char *);

/*
 * Function Prototypes for UTL_acccessories
 */
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
int UTL_GetProfileString(char *section, char *key, char *defaultVal,
				char *value, long bufSize, char *file, char *zErrorMsg);
int UTL_FileCopy(char *zSource, char *zDestination, int zPerms, 
				char *zErrorMsg);

#endif /* _ISPCOMMON_H */