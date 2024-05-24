#ident	"@(#)BuildFaxList 00/05/22 3.1   Copyright 2000 Aumtech, Inc."
static char mod[]="TEL_BuildFaxList";
/*-----------------------------------------------------------------------------
Program:	BuildFaxList.c
Purpose:	Prepares the list of file names along with the type
			of files to fax them out.
Author:		Unknown
Update:		12/26/02 apj Changes necessary for Single DM structure.
Update:     01/04/2006 mpb Replaced sys_errlist with strerror.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "arcFAX.h"

int TEL_BuildFaxList(int operation,  char *listFile, char *faxFile, 
		int fileType,  int startPage, int numberOfPages,
		char *comment, int ejectPage, char *forFutureUse)
{
int	ret_code;
char 	apiAndParms[MAX_LENGTH];
FILE	*fp;
static	char	logMsg[512];
char 	fileTypeString[20];

memset(fileTypeString, 0, sizeof(fileTypeString));
getFileTypeString(fileType, fileTypeString);

sprintf(apiAndParms,"%s(%d,%s,%s,%s,%d,%d,%s,%d,%s)", mod, 
	operation, listFile, faxFile, fileTypeString, startPage,
	numberOfPages, comment, ejectPage, "future_use");
ret_code = apiInit(mod, TEL_BUILDFAXLIST, apiAndParms, 1, 0, 0); 
if (ret_code != 0) HANDLE_RETURN(ret_code);
 
if((int)strlen(listFile) == 0)
{
	sprintf(logMsg,"No list file specified.");
	fax_log(mod, REPORT_NORMAL, TEL_BASE,logMsg);
	HANDLE_RETURN(-1);
}

if(access(faxFile, F_OK) != 0)
{
	sprintf(logMsg,"File <%s> to be added to <%s> does not exist.",
					 faxFile, listFile);
	fax_log(mod, REPORT_NORMAL, TEL_BASE,logMsg);
	HANDLE_RETURN(-1);
}

if( (ejectPage != 0) && (ejectPage != 1) )
{
	sprintf(logMsg,
		"Invalid page eject parameter (%d) when adding file (%s) "
		"to list file (%s).  Valid values: 0 and 1.", 
		ejectPage, faxFile, listFile);
	fax_log(mod, REPORT_NORMAL, TEL_BASE,logMsg);
	HANDLE_RETURN(-1);
}

if(operation == APPEND_TO_LIST_FILE)
	fp = fopen(listFile, "a+");
else if(operation == CREATE_NEW_LIST_FILE)
	fp = fopen(listFile, "w");
else
{
	sprintf(logMsg,
		"Invalid operation (%d) when adding file <%s> to list "
		" file <%s>. Valid values: 0 and 1.",
		operation, faxFile, listFile);
	fax_log(mod, REPORT_NORMAL, TEL_BASE,logMsg);
	HANDLE_RETURN(-1);
}

if(fp == NULL)
{
	sprintf(logMsg, "Failed to open list file <%s>. errno=%d. [%s]",
			listFile, errno, strerror(errno));
	fax_log(mod, REPORT_NORMAL, TEL_BASE,logMsg);
	HANDLE_RETURN(-1);
}
	
if (fileType == TEXT)
{ 

	fprintf(fp,"%s|TEXT|0|0|%d| # %s\n", faxFile,ejectPage,comment);
	fclose(fp);
	HANDLE_RETURN(TEL_SUCCESS);
}

if (fileType != TIF)
{
	sprintf(logMsg, "Invalid file type parameter: %d. Must be 'TIF'(%d) or 'TEXT'(%d).",
		fileType, TIF, TEXT);
	fax_log(mod, REPORT_NORMAL, TEL_BASE,logMsg);
	HANDLE_RETURN(TEL_FAILURE);
}

	// DJB validateFaxPageParams(mod, faxFile, &startPage, &numberOfPages);
fprintf(fp,"%s|TIF |%d|%d|%d| # %s\n", faxFile, startPage,
				numberOfPages, ejectPage, comment);
fclose(fp);
HANDLE_RETURN(TEL_SUCCESS);

} /* END BuildFaxList */
