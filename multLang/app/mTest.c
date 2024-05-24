/*---------------------------------------------------------------------------
File:		french.c
Purpose:	This speaking french.
Author:		Aumtech, Inc.
Date:		06/08/99
Update:		06/09/99 gpw added additional features.
Update:		11/17/00 gpw upgraded to use TEL_Speak, TEL_GetDTMF, and
			 TEL_Record. This makes it compatible with Telecom 
			 Services 2.9.2 & higher, including Linux versions.
---------------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "Telecom.h"
#include "gaUtils.h"

int		sExitApp();

/*-----------------------------------------------------------------------------
Main application routine.
-----------------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
	int			yRc;
	int			yChoice;
	static char	yMod[]="main";
	int         yParty;
    int         yInterrupt;
    int         yInputFormat;
    int         yOutputFormat;
    char        yData[64];
    char        yResponse[64];
	char		yBalance[10];
    int         ySync;
	char		yPhraseFile[128];
	char		yPhraseDir[128];


	yRc = gaSetGlobalString("$LOG_DIR", ".");
	yRc = gaSetGlobalString("$LOG_BASE", argv[0]);
	yRc = gaSetGlobalString("$PROGRAM_NAME", argv[0]);

	if ((yRc = TEL_InitTelecom (argc, argv)) != TEL_SUCCESS)
	{
		gaVarLog(yMod, 0, "TEL_InitTelecom failed.  yRc = %d", yRc);
		yRc = sExitApp();
	}
	yData[0] = '\0';
	yRc = TEL_GetGlobalString("$PORT_NAME", yData);
	yRc = gaSetGlobalString("$RESOURCE_NAME", yData);
	
	if ((yRc = TEL_AnswerCall (1)) != TEL_SUCCESS)
	{
		gaVarLog(yMod, 0, "TEL_AnswerCall failed.  yRc = %d", yRc);
		yRc = sExitApp();
	}
	gaVarLog(yMod, 0, "TEL_AnswerCall ok.  yRc = %d", yRc);

	yRc = TEL_SetGlobal("$LANGUAGE", 133);
	gaVarLog(yMod, 0, "%d = TEL_SetGlobal($LANGUAGE, S_FRENCH)", yRc);

#if 0
	yRc = chdir("/home/arc/.ISP/Telecom/Exec/Languages/French");
#endif

	sprintf(yPhraseDir, "%s", "/home/arc/.ISP/Telecom/Exec/Languages/French");

	yRc = TEL_SetGlobalString("$APP_PHRASE_DIR", yPhraseDir);
	gaVarLog(yMod, 0, "%d = TEL_SetGlobalString($APP_PHRASE_DIR, %s)",
				yRc, yPhraseDir);

	yPhraseDir[0] = '\0';
	yRc = TEL_GetGlobalString("$APP_PHRASE_DIR", yPhraseDir);
	gaVarLog(yMod, 0, "%d = TEL_GetGlobalString($APP_PHRASE_DIR, %s)",
				yRc, yPhraseDir);

	sprintf(yPhraseFile, "%s", "phr.GenericApp33/mainCU2.32a");

    yParty = FIRST_PARTY;
    yInterrupt = NONINTERRUPT;
    yInputFormat = PHRASE_FILE;
    yOutputFormat = PHRASE;
    ySync = SYNC;

	sprintf(yData, "%s", "mainCurrencyUnit2");
	sprintf(yData, "%s", yPhraseFile);
	yRc = TEL_Speak(yParty, yInterrupt, yInputFormat, yOutputFormat, yData, ySync);
	gaVarLog(yMod, 0, "%d = TEL_Speak(%d, %d, %d, %d, %s, %d)", 
			yRc, yParty, yInterrupt, yInputFormat, yOutputFormat, yData, ySync);

	return(0);
} /* END: main() */

/*----------------------------------------------------------------------------
This routine speaks instructions and writes them to wherever LogMsg writes.
----------------------------------------------------------------------------*/
int sExitApp()
{
	int			yRc;
	static char yMod[]="sExitApp";

	yRc = TEL_ExitTelecom();
	exit(0);
} /* END: sExitApp() */

			
/*-----------------------------------------------------------------------------
This routine is required. Your application will not link if this routine is
not present. This routine is called automatically if you application is killed,
therefore it is a good place to put code that must be executed before your
application dies, e.g., code that logs data about the call. 
-----------------------------------------------------------------------------*/
int LogOff () 
{ 
	return (0); 
}

