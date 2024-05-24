/*-----------------------------------------------------------------------------
Program:        TEL_Rest.c
Purpose:        This file contains a number of dummy functions that 
		are not now but will eventually be implemented in the 
		H323 version. They are built here so that apitest will
		find them.  
Author:         Wenzel-Joglekar
Date:		08/24/2000
Update:		08/28/2000 gpw created dummy version
Update:		09/12/2000 gpw took out TEL_InitiateCall.
Update:		09/13/2000 gpw took out TEL_GetGlobalString.
Update:		09/13/2000 gpw took out TEL_ExitTelecom
Update:		09/21/2000 gpw took out TEL_DropCall
Update:		03/21/2005 ddn took out TEL_TransferCall
------------------------------------------------------------------------------*/
#include "TEL_Common.h"

int TEL_SendInfoElements(int parmToWhom, char *info_element, int msgType)
{
	return(0);
}

#if 0
int TEL_RecvFax(char *fax_file, int *fax_pages, char *prompt_phrase)
{
	return(0);
}
#endif

#if 0
int TEL_SendFax(int file_type, char *fax_data, char *prompt_phrase)
{
	return(0);
}
#endif

int TEL_EndOfCall()
{
	return(0);
}

int TEL_SpeakLHTTS(int party, int partyInterruptOption,int cleanupOption,
	int syncOption, int timeout, char *speechFile, 
	char *userString, int ttsMode)
{
	return(0);
}

#if 0
int TEL_GetCDFInfo(char *a,int *b,int *c,int *d,char *e,char  *f)
{
	return(0);
}

int TEL_ScheduleCall(int a,int b,int c,char *d,char *e,char *f,char *g,char *h)
{
	return(0);
}
#endif

int TEL_SpeakSystemPhrase(int phraseNumber)
{
	return(0);
}

#if 0
int TEL_TransferCall(int a,int b,int c,const char *d,const char *e,int *f,int *g)
{
	return(0);
}

#endif

#if 0
int TEL_UnscheduleCall(char * cdfFile)
{
	return(0);
}
#endif

#if 0
extern int TEL_BridgeCall(int, int,int,const char *,const char *, int *, int *);
extern int TEL_ClearDTMF();
extern int TEL_RecvFax(char *, int *, char *);
extern int TEL_SendFax(int, char *, char *);
extern int TEL_SpeakTTS(int interruptOption,int cleanupOption, int syncOption,
                        int timeout, char *speechFile, char *userString);
extern int TEL_StartNewApp(int appType, char *newApp, char *dnis, char *ani);
#endif

