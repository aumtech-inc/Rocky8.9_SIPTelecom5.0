/*------------------------------------------------------------------------------
File:		testML.c
Purpose:	Application to test multiple languages.
Author:		Aumtech, Inc.
Update:		01/30/2002 djb	Created the file.
------------------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "Telecom.h"
#include "ISPCommon.h"
#include "gaUtils.h"
#include "arcML.h"

#define		DO_NUMBERS	1
#define		DO_CURRENCY	2
#define		DO_TIME		3
#define		DO_DATE		4

static char		gAppName[32] = "testML";
static char		gTagFile[128] = "testML.tag";

int				exitApp();
static int		sMainProcess(int zLanguage, int zType);
/*------------------------------------------------------------------------------
main:
------------------------------------------------------------------------------*/
main (int argc, char *argv[])
{
	int				yRc;
	static char		yMod[]="main";
	char			yPort[8];
	int				yPhrase;
	char			ySection[32];
	char			yBuf[32];
	int				yLanguage;
	int				yType;
	int				yAnswer;

	yRc = gaSetGlobalString("$LOG_DIR", ".");
	yRc = gaSetGlobalString("$LOG_BASE", argv[0]); 
	yRc = gaSetGlobalString("$PROGRAM_NAME", argv[0]);

	SERV_M3 = exitApp;
	if ((yRc = TEL_InitTelecom(argc, argv)) != 0)
	{
		gaVarLog(yMod, 0, "%d:TEL_InitTelecom", yRc);
		exitApp();
	}
	gaVarLog(yMod, 0, "%d:TEL_InitTelecom", yRc);

	yPort[0] = '\0';
	if ((yRc = TEL_GetGlobalString("$PORT_NAME", yPort)) != TEL_SUCCESS)
	{
		gaVarLog(yMod, 0, "%d:TEL_GetGlobalString($PORT_NAME)", yRc);
		exitApp();
	}
	gaVarLog(yMod, 0, "%d:TEL_GetGlobalString($PORT_NAME, %s)", yRc, yPort);
	yRc = gaSetGlobalString("$RESOURCE_NAME", yPort);

	if ((yRc = TEL_AnswerCall(1)) != 0)
	{
		gaVarLog(yMod, 0, "%d:TEL_AnswerCall", yRc);
		exitApp();
	}
	gaVarLog(yMod, 0, "%d:TEL_AnswerCall", yRc);

	yRc = TEL_LoadTags(gTagFile);
	if (yRc != TEL_SUCCESS)
	{
		gaVarLog(yMod, 0, "%d = TEL_LoadTags(%s)", yRc, gTagFile);
		yRc = exitApp();
	}
	gaVarLog(yMod, 0, "%d = TEL_LoadTags(%s)", yRc, gTagFile);

	sprintf(ySection, "%s", "langMenu");
	yBuf[0] = '\0';
	if ((yRc = TEL_PromptAndCollect(ySection, "", yBuf)) != TEL_SUCCESS)
	{
		gaVarLog(yMod, 0, "%d:TEL_PromptAndCollect(%s,%s)",
				yRc, ySection, yBuf);
		exitApp();
	}
	gaVarLog(yMod, 0, "%d:TEL_PromptAndCollect(%s,%s)",
				yRc, ySection, yBuf);
	if (atoi(yBuf) == 2)
	{
		yLanguage = ML_SPANISH;
		gaVarLog(yMod, 0, "Language is set to Spanish.");
	}
	else
	{
		yLanguage = ML_FRENCH;
		gaVarLog(yMod, 0, "Language is set to French.");
	}

	sprintf(ySection, "%s", "typeMenu");
	yBuf[0] = '\0';
	if ((yRc = TEL_PromptAndCollect(ySection, "", yBuf)) != TEL_SUCCESS)
	{
		gaVarLog(yMod, 0, "%d:TEL_PromptAndCollect(%s,%s)",
				yRc, ySection, yBuf);
		exitApp();
	}
	gaVarLog(yMod, 0, "%d:TEL_PromptAndCollect(%s,%s)",
				yRc, ySection, yBuf);
	yAnswer = atoi(yBuf);
	switch (yAnswer) 
	{
		case 1:
			yType = DO_CURRENCY;
			break;
		case 2:
			yType = DO_NUMBERS;
			break;
		case 3:
			if ( yLanguage != ML_SPANISH )
			{
				gaVarLog(yMod, 0,
					"Error: time is only available in Spanish right now.");
				exitApp();
			}
			yType = DO_TIME;
			break;
		case 4:
			if ( yLanguage != ML_SPANISH )
			{
				gaVarLog(yMod, 0,
					"Error: dates is only available in Spanish right now.");
				exitApp();
			}
			yType = DO_DATE;
			break;
	}

	yRc = sMainProcess(yLanguage, yType);
	exitApp();

} /* END: main() */

/*------------------------------------------------------------------------------
sMainProcess:
------------------------------------------------------------------------------*/
static int		sMainProcess(int zLanguage, int zType)
{
	static char	yMod[] = "sMainProcess";
	int			yRc;
	FILE		*pFp;
	char		yInputFile[256];
	char		yBuf[64];
	char		yTag[32];

	switch( zType )
	{
		case DO_NUMBERS:
			sprintf(yInputFile, "%s", "testMLNumbers.dat");
			break;
		case DO_CURRENCY:
			sprintf(yInputFile, "%s", "testMLCurrency.dat");
			break;
		case DO_TIME:
			sprintf(yInputFile, "%s", "testMLTime.dat");
			break;
		case DO_DATE:
			sprintf(yInputFile, "%s", "testMLDate.dat");
			break;
	}

	if ((pFp = fopen(yInputFile,"r")) == NULL )
	{
		gaVarLog(yMod, 0, "Unable to open (%s) for input. [%d,%s]",
			yInputFile, errno, strerror(errno));
		return(-1);
	}
	gaVarLog(yMod, 0, "Reading data from (%s).", yInputFile);

	memset((char *)yBuf, 0, sizeof(yBuf));
	while ( fgets(yBuf, sizeof(yBuf)-1, pFp) != NULL )
	{
		yBuf[strlen(yBuf) - 1] = '\0';

		gaVarLog(yMod, 0, "Speaking (%s).", yBuf);
		if (zType == DO_NUMBERS)
		{
			yRc = TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, STRING, NUMBER,
                                yBuf, SYNC);

			sprintf(yTag, "%s", "male");
			yRc = TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, PHRASE_TAG,
					PHRASE, yTag, SYNC);

			yRc = TEL_MLSpeakNumber(zLanguage, ML_MALE, S_AMENG, gTagFile, 
				FIRST_PARTY, FIRST_PARTY_INTERRUPT, yBuf, SYNC);

			sleep(1);

			sprintf(yTag, "%s", "female");
			yRc = TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, PHRASE_TAG,
					PHRASE_FILE, yTag, SYNC);
			yRc = TEL_MLSpeakNumber(zLanguage, ML_FEMALE, S_AMENG, gTagFile,
				FIRST_PARTY, FIRST_PARTY_INTERRUPT, yBuf, SYNC);
			memset((char *)yBuf, 0, sizeof(yBuf));
			continue;
		}

		if (zType == DO_CURRENCY)
		{
			yRc = TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, STRING, DOLLAR,
                                yBuf, SYNC);
			sleep(1);

			yRc = TEL_MLSpeakCurrency(zLanguage, S_AMENG, 
				gTagFile, FIRST_PARTY, FIRST_PARTY_INTERRUPT, yBuf, SYNC);
			memset((char *)yBuf, 0, sizeof(yBuf));
			continue;
		}

		if (zType == DO_TIME)
		{
			yRc = TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, STRING,
						TIME_STD, yBuf, SYNC);
			gaVarLog(yMod, 0, "%d:TEL_Speak(%s)", yRc, yBuf);

			sleep(1);

			yRc = TEL_MLSpeakTime(zLanguage, S_AMENG, 
				gTagFile, FIRST_PARTY, FIRST_PARTY_INTERRUPT, 
				STANDARD, TIME_STD, yBuf, SYNC);
			gaVarLog(yMod, 0, "%d:TEL_MLSpeakTime(%d, %d, %s, %s)",
				yRc, zLanguage, S_AMENG, gTagFile, yBuf);
			memset((char *)yBuf, 0, sizeof(yBuf));
			continue;
		}

		if (zType == DO_DATE)
		{
			yRc = TEL_Speak(FIRST_PARTY, FIRST_PARTY_INTERRUPT, STRING,
						DATE_YTT, yBuf, SYNC);
			gaVarLog(yMod, 0, "%d:TEL_Speak(%s)", yRc, yBuf);

			sleep(1);

			yRc = TEL_MLSpeakDate(zLanguage, S_AMENG, 
				gTagFile, FIRST_PARTY, FIRST_PARTY_INTERRUPT, 
				STRING, DATE_YTT, yBuf, SYNC);
			gaVarLog(yMod, 0, "%d:TEL_MLSpeakDate(%d, %d, %s, %s)",
				yRc, zLanguage, S_AMENG, gTagFile, yBuf);
			memset((char *)yBuf, 0, sizeof(yBuf));
			continue;
		}
	}
	
	fclose(pFp);
	return(0);
} /* END: sMainProcess() */

/*------------------------------------------------------------------------------
exitApp:
------------------------------------------------------------------------------*/
int exitApp()
{
	int			yRc;
	static char	yMod[]="exitApp";

	yRc = TEL_ExitTelecom();
	exit(0);

} /* END: exitApp() */

int LogOff() { return(0); }
