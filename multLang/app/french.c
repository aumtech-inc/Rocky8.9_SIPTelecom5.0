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
#include "arcML.h"

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


	yRc = gaSetGlobalString("$LOG_DIR", "/home/arc/.ISP/Telecom/Exec");
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
    yParty = FIRST_PARTY;
    yInterrupt = NONINTERRUPT;
    yInputFormat = STRING;
    yOutputFormat = NUMBER;
    ySync = SYNC;
	sprintf(yBalance, "%s", "100");

	yRc = ML_SpeakNumber(ML_FRENCH, ML_MALE, FIRST_PARTY, FIRST_PARTY_INTRPT,
				yBalance, SYNC);
	gaVarLog(yMod, 0, "%d = ML_SpeakNumber(%d, %d, %d, %d, %s, %d)",
				yRc, ML_FRENCH, ML_MALE, FIRST_PARTY, FIRST_PARTY_INTRPT,
                yBalance, SYNC);

	yRc = ML_SpeakNumber(ML_FRENCH, ML_FEMALE, FIRST_PARTY, FIRST_PARTY_INTRPT,
				yBalance, SYNC);
	gaVarLog(yMod, 0, "%d = ML_SpeakNumber(%d, %d, %d, %d, %s, %d)",
				yRc, ML_FRENCH, ML_MALE, FIRST_PARTY, FIRST_PARTY_INTRPT,
                yBalance, SYNC);

#if 0
	yRc = ML_SpeakCurrency(ML_FRENCH, FIRST_PARTY, FIRST_PARTY_INTRPT,
				yBalance, SYNC);
	gaVarLog(yMod, 0, "%d = ML_SpeakCurrency(%d, %d, %d, %s, %d)",
				yRc, ML_FRENCH, FIRST_PARTY, FIRST_PARTY_INTRPT,
                yBalance, SYNC);

	if (yRc != TEL_SUCCESS)
	{
		sExitApp();
	}

	sprintf(yBalance, "%s", "20");
	yRc = ML_SpeakCurrency(ML_FRENCH, FIRST_PARTY, FIRST_PARTY_INTRPT,
				yBalance, SYNC);
	gaVarLog(yMod, 0, "%d = ML_SpeakCurrency(%d, %d, %d, %s, %d)",
				yRc, ML_FRENCH, FIRST_PARTY, FIRST_PARTY_INTRPT,
                yBalance, SYNC);

	if (yRc != TEL_SUCCESS)
	{
		sExitApp();
	}

#endif
	gaVarLog(yMod, 0, "*****  ");
	sleep(3);

	yRc = TEL_SetGlobal("$LANGUAGE", 33);
	gaVarLog(yMod, 0, "%d = TEL_SetGlobal($LANGUAGE, S_FRENCH)", yRc);

	sprintf(yPhraseDir, "%s", "/home/arc/.ISP/Telecom/phrases");

	yRc = TEL_SetGlobalString("$APP_PHRASE_DIR", yPhraseDir);
	gaVarLog(yMod, 0, "%d = TEL_SetGlobalString(%s)", yRc, yPhraseDir);
	yRc = chdir(yPhraseDir); 
		
	sprintf(yPhraseFile, "%s",
		"/home/arc/.ISP/Telecom/phrases/appPhrases33/appPhrases33.phr");

	yRc = TEL_LoadTags(yPhraseFile);
	if (yRc != TEL_SUCCESS)
	{
		gaVarLog(yMod, 0, "%d = TEL_LoadTags(%s)", yRc, yPhraseFile);
		yRc = sExitApp();
	}
	gaVarLog(yMod, 0, "%d = TEL_LoadTags(%s)", yRc, yPhraseFile);

	playBalance(yBalance, 0, SYNC);

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
	gaVarLog(yMod, 0, "Exiting.");

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

