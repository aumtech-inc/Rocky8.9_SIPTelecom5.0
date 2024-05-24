#define IN_MONEY			0
#define IN_TIME				1

#define	WELCOME_MESSAGE			"welcome.32a"
#define	MAKE_A_CALL_MESSAGE		"makeACall.32a"
#define	GOODBYE_MESSAGE			"goodbye.32a"
#define	CALL_HELPLINE_MESSAGE		"helpline.32a"
#define	WARNING_MESSAGE			"warning"
#define	RECHARGE_WARNING_MESSAGE	"rechargeWarning"
#define	OUT_OF_TIME_MESSAGE		"outOfTime"
#define	ENTER_ACCOUNT_NO_MESSAGE	"enterAcctNo"
#define	ENTER_PIN_MESSAGE		"enterPin"
#define	ENTER_PHONE_NO_MESSAGE		"makeACall"
#define	OPERATOR_XFER_MESSAGE		"operatorXfer"
#define	ANNOUNCE_TO_OPERATOR_MESSAGE	"announce"

#define NO_LANGUAGE_SET			"0"
#define ENGLISH				"eng"       /* English    */
#define FRENCH				"fra"       /* French     */
#define GERMAN				"ger"       /* German     */
#define ITALIAN				"ita"       /* Italian    */
#define SPANISH				"spa"       /* Spanish    */
#define PORTUGUESE			"por"       /* Portuguese */
#define TURKISH				"tur"       /* Turkish    */
#define SERBOCROAT			"scr"       /* SerboCroat */
#define GREEK			        "gre"       /* Greek      */
#define IRISH			        "irl"       /* Irish      */
#define LANGUAGE_1			44          /* English    */
#define LANGUAGE_2			33          /* French     */
#define LANGUAGE_3			49          /* German     */
#define LANGUAGE_4			39          /* Italian    */
#define LANGUAGE_5			34          /* Spanish    */
#define LANGUAGE_6			351         /* Portuguese */
#define LANGUAGE_7			90          /* Turkish    */
#define LANGUAGE_8			381         /* SerboCroat */
#define LANGUAGE_9			30          /* Greek      */
#define LANGUAGE_10			353         /* Irish      */
#define UK				"44"
#define FRANCE				"33"
#define GERMANY				"49"
#define ITALY				"39"
#define SPAIN				"34"
#define PORTUGAL			"351"
#define TURKEY  			"90"
#define SERBIA    			"381"
#define GREECE			        "30"
#define IRELAND			        "353"

/* Special numbers */

#define EXTRAORDINARY_NUMBER		-100
#define SPANISH_NUMBER			 90
#define PORTUGUESE_NUMBER		 91
#define ORDINARY_NUMBER			100
#define SPECIAL_NUMBER_1		101
#define SPECIAL_NUMBER_2		102
#define SPECIAL_NUMBER_3		103
#define SPECIAL_NUMBER_4		104
#define SPECIAL_NUMBER_5		105
#define SPECIAL_NUMBER_6		106
#define SPECIAL_NUMBER_7		107
#define SPECIAL_NUMBER_8		108
#define SPECIAL_NUMBER_9		109
#define SPECIAL_NUMBER_10		110
#define SPECIAL_NUMBER_11		111
#define SPECIAL_NUMBER_12		112
#define SPECIAL_NUMBER_13		113
#define SPECIAL_NUMBER_14		114
#define SPECIAL_NUMBER_15		115
#define SPECIAL_NUMBER_16		116
#define SPECIAL_NUMBER_17		117
#define SPECIAL_NUMBER_18		118
#define SPECIAL_NUMBER_19		119
#define SPECIAL_NUMBER_20		120
#define SPECIAL_NUMBER_21		121
#define SPECIAL_NUMBER_22		122
#define SPECIAL_NUMBER_23		123
#define SPECIAL_NUMBER_24		124
#define SPECIAL_NUMBER_25		125
#define SPECIAL_NUMBER_26		126
#define SPECIAL_NUMBER_27		127
#define SPECIAL_NUMBER_28		128
#define SPECIAL_NUMBER_29		129
#define SPECIAL_NUMBER_30		130
#define SPECIAL_NUMBER_31		131

/* Special cash number binary patterns */

/* H   = hundred
   TH  = thousand
   HTH = hundred thousand
   M   = million
   HM  = hundred million */

#define	H				"00001"
#define TH       			"00010"
#define TH_H       			"00011"
#define HTH				"00100"
#define HTH_H				"00101"
#define HTH_TH				"00110"
#define HTH_TH_H			"00111"
#define M				"01000"
#define M_H				"01001"
#define M_TH				"01010"
#define M_TH_H				"01011"
#define M_HTH				"01100"
#define M_HTH_H				"01101"
#define M_HTH_TH			"01110"
#define M_HTH_TH_H			"01111"
#define HM				"10000"
#define HM_H				"10001"
#define HM_TH				"10010"
#define HM_TH_H				"10011"
#define HM_HTH				"10100"
#define HM_HTH_H			"10101"
#define HM_HTH_TH			"10110"
#define HM_HTH_TH_H			"10111"
#define HM_M				"11000"
#define HM_M_H				"11001"
#define HM_M_TH				"11010"
#define HM_M_TH_H			"11011"
#define HM_M_HTH			"11100"
#define HM_M_HTH_H			"11101"
#define HM_M_HTH_TH			"11110"
#define HM_M_HTH_TH_H			"11111"
/*
 * Function Prototypes
 */
int   getGlobalMessages(void);
int   loadVoice(char *lang, char *tagFile);
int   switchLanguage(char *language);
int   playPhrase(char *tag, int sync);
int   playFile(char *filename);
void  playBalance(char *balance, int type, int sync);
int   inpWelcomeGetStar(void);
void  playGoodDay(void);
int   selectLanguage(void);
void  addLanguagePrefix(char *filenameWithPrefix, char *filenameWithoutPrefix);
void  playCallHelpline(void);

/*------------------------------------------------------------------------------
File:		playPhrase.c

Purpose:	Play a phrase or tag 

Author:		Gary Knapper (XAL)

Update:		05/05/2000                            
------------------------------------------------------------------------------*/

#include "application.h"

static void parseBalance(char *balance,
                         char *hundredMillions, 
                         char *tenMillions, 
                         char *millions,
                         char *hundredThousands,
                         char *tenThousands,
                         char *thousands,
                         char *hundreds,
                         char *tens,
                         char *units,
                         char *tenths,
                         char *hundredths);

static int  checkSpecialNumber(char *hundredMillions, 
                               char *tenMillions, 
                               char *millions,
                               char *hundredThousands,
                               char *tenThousands,
                               char *thousands,
                               char *hundreds,
                               char *tens,
                               char *units,
                               char *tenths,
                               char *hundredths,
                               int language);

static void playMainCurrencyUnit(int language, long balance);
static void playMinorCurrencyUnit(int language, long balance);
static int  loadFeminineVoice(char *sysVoxStr);
static void setLanguage(int languageOption);
static void speakGreek(char *balance, int type);

/*------------------------------------------------------------------------------
playFile():
------------------------------------------------------------------------------*/
int playFile(char *filename)
{
        static char  mod[]="playFile";
        char         phraseFile[ARRAY_SIZE];
        int          rc=0;

        memset(phraseFile,'\0',sizeof(phraseFile));
        strcpy(phraseFile,"./GREEK_NOS/");
        strcat(phraseFile,filename);

        gaVarLog(mod,gvAppinfo.Debug,"Playing file <%s>",phraseFile);

        rc=TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,PHRASE_FILE,PHRASE,phraseFile,SYNC);

        if (rc != TEL_SUCCESS)
           gaVarLog(mod,0,"Failed to speak file <%s> TO FIRST_PARTY",phraseFile);

} /* END: playFile() */


/*------------------------------------------------------------------------------
playPhrase(): 
------------------------------------------------------------------------------*/
int playPhrase(char *tag, int sync)
{
	static char  mod[]="playPhrase";
	char         phraseFile[ARRAY_SIZE];
	int	     rc=0;

        if (!strcmp(tag,""))
        {
	   gaLog(mod,gvAppinfo.Debug,"Null phrase tag");
	   return(rc);
        }

	rc=taGetPhraseFromTag(tag,phraseFile);
	if (rc != TEL_SUCCESS)
	   gaVarLog(mod,0,"Failed to get phrasefile from tag <%s>",tag);

	gaVarLog(mod,gvAppinfo.Debug,"Playing tag <%s> = <%s>",tag,phraseFile);

	if (gvFlag.IsOperator)
	{
           rc=TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,PHRASE_FILE,PHRASE,phraseFile,sync);
	   if (rc != TEL_SUCCESS)
	      gaVarLog(mod,0,"Failed to speak tag <%s> to SECOND_PARTY",tag);
	}
	else
	{
           rc=TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,PHRASE_FILE,PHRASE,phraseFile,sync);
	   if (rc != TEL_SUCCESS)
	      gaVarLog(mod,0,"Failed to speak tag <%s> TO FIRST_PARTY",tag);
	   gaVarLog(mod,gvAppinfo.Debug,"TEL_Speak rc <%d>",rc);
	}

        getCpuTime(mod,gvAppinfo.Debug);

	return(rc);

} /* END: playPhrase() */


/*------------------------------------------------------------------------------
playBalance(): Speak the balance on a valid account as money
------------------------------------------------------------------------------*/
void playBalance(char *balance, int type, int sync)
{
   static char	mod[]="playBalance";
   int		rc=0;
   int		language=0;
   long		mainBalance;
   char		minorBalance[3];
   char		hundredMillions[2],tenMillions[2],millions[2];
   char		hundredThousands[2],tenThousands[2],thousands[2];
   char		hundreds[2],tens[2],units[2];
   char		tenths[2],hundredths[2];
   static int   minutes;
   static int   seconds;
   char         minuteStr[8];
   char         secondStr[8];
   char         tmpSysVoxSelection[ARRAY_SIZE];


   memset(minorBalance,'\0',sizeof(minorBalance));
   memset(hundredMillions,'\0',sizeof(hundredMillions));
   memset(tenMillions,'\0',sizeof(tenMillions));
   memset(millions,'\0',sizeof(millions));
   memset(hundredThousands,'\0',sizeof(hundredThousands));
   memset(tenThousands,'\0',sizeof(tenThousands));
   memset(thousands,'\0',sizeof(thousands));
   memset(hundreds,'\0',sizeof(hundreds));
   memset(tens,'\0',sizeof(tens));
   memset(units,'\0',sizeof(units));
   memset(tenths,'\0',sizeof(tenths));
   memset(hundredths,'\0',sizeof(hundredths));
   memset(minuteStr,'\0',sizeof(minuteStr));
   memset(secondStr,'\0',sizeof(secondStr));
   memset(tmpSysVoxSelection,'\0',sizeof(tmpSysVoxSelection));

   rc=TEL_GetGlobal("$LANGUAGE",&language);

   /*
          LANGUAGE_1        English
          LANGUAGE_2        French
          LANGUAGE_3        German
          LANGUAGE_4        Italian
          LANGUAGE_5        Spanish
          LANGUAGE_6        Portuguese
          LANGUAGE_7        Turkish
          LANGUAGE_8        Serbo-Croat
          LANGUAGE_9        Greek
   */

   /* FOR EXCEL IMPACT */
   /* if (language == LANGUAGE_9) language=LANGUAGE_1; */

   switch (language)
   {
      case LANGUAGE_1:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 1");
             break;

      case LANGUAGE_2:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 2");
             break;

      case LANGUAGE_3:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 3");
             break;

      case LANGUAGE_4:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 4");
             break;

      case LANGUAGE_5:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 5");
             break;

      case LANGUAGE_6:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 6");
             break;

      case LANGUAGE_7:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 7");
             break;

      case LANGUAGE_8:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 8");
             break;

      case LANGUAGE_9:
	     gaLog(mod,gvAppinfo.Debug,"Speaking language 9");
             if (type == IN_MONEY)
                speakGreek(balance,FALSE);
             else
                speakGreek(balance,TRUE);
             return;
             break;

      default:
	     gaLog(mod,gvAppinfo.Debug,"Speaking default language");
             break;
   }

   if (type == IN_MONEY)
      gaVarLog(mod,gvAppinfo.Debug,"Cash balance : <%s>",balance);
   else if (type == IN_TIME)
   {
      gvMaxDuration=atol(gvMaxDurationStr);
      minutes=gvMaxDuration/gvAppinfo.SecondsPerMin;
      seconds=gvMaxDuration%gvAppinfo.SecondsPerMin;
      sprintf(minuteStr,"%d",minutes);
      sprintf(secondStr,"%d",seconds);
      gaVarLog(mod,0,"Time remaining: <%s> mins <%s> secs",minuteStr,secondStr);
      strcpy(balance,minuteStr);
      if (language != LANGUAGE_1)
      {
         /* Not English, so switch to feminine system voice directory */
         if (rc=loadFeminineVoice(gvAppinfo.SysVoxSelection) != 0)
            return;
      }
   }
   else
   {
      gaLog(mod,0,"Invalid type - defaulting to IN_MONEY");
      gaVarLog(mod,gvAppinfo.Debug,"Cash balance : <%s>",balance);
      return;
   }

   parseBalance(balance,hundredMillions,tenMillions,millions,hundredThousands,
                tenThousands,thousands,hundreds,tens,units,tenths,hundredths);

   mainBalance=atol(balance);
   strcpy(minorBalance,tenths);
   strcat(minorBalance,hundredths);
   gaVarLog(mod,gvAppinfo.Debug,"mainBal <%ld> minorBal <%s>",mainBalance,minorBalance);

   rc=checkSpecialNumber(hundredMillions,tenMillions,millions,hundredThousands,
                         tenThousands,thousands,hundreds,tens,units,tenths,
                         hundredths,language);

   gaVarLog(mod,gvAppinfo.Debug,"Special number <%d>",rc);

   switch (rc)
   {
      case SPECIAL_NUMBER_1:
                mainBalance=mainBalance-100;
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_2:
                mainBalance=mainBalance-1000;
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_3:
                mainBalance=mainBalance-1100;
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_4:
                mainBalance=mainBalance-100000;
	        playPhrase("hundred",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_5:
                mainBalance=mainBalance-100100;
	        playPhrase("hundred",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_6:
                mainBalance=mainBalance-101000;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_7:
                mainBalance=mainBalance-101100;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_8:
                mainBalance=mainBalance-1000000;
	        playPhrase("million",SYNC);
                break;

      case SPECIAL_NUMBER_9:
                mainBalance=mainBalance-1000100;
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_10:
                mainBalance=mainBalance-1001000;
	        playPhrase("million",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_11:
                mainBalance=mainBalance-1001100;
	        playPhrase("million",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_12:
                mainBalance=mainBalance-1100000;
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_13:
                mainBalance=mainBalance-1100100;
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_14:
                mainBalance=mainBalance-1101000;
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_15:
                mainBalance=mainBalance-1101100;
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_16:
                mainBalance=mainBalance-100000000;
	        playPhrase("hundred",SYNC);
	        playPhrase("million",SYNC);
                break;

      case SPECIAL_NUMBER_17:
                mainBalance=mainBalance-100000100;
	        playPhrase("hundred",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_18:
                mainBalance=mainBalance-100001000;
	        playPhrase("hundred",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_19:
                mainBalance=mainBalance-100001100;
	        playPhrase("hundred",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_20:
                mainBalance=mainBalance-100100000;
	        playPhrase("hundred",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_21:
                mainBalance=mainBalance-100100100;
	        playPhrase("hundred",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_22:
                mainBalance=mainBalance-100101000;
	        playPhrase("hundred",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_23:
                mainBalance=mainBalance-100101100;
	        playPhrase("hundred",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_24:
                mainBalance=mainBalance-101000000;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("million",SYNC);
                break;

      case SPECIAL_NUMBER_25:
                mainBalance=mainBalance-101000100;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_26:
                mainBalance=mainBalance-101001000;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_27:
                mainBalance=mainBalance-101001100;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_28:
                mainBalance=mainBalance-101100000;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_29:
                mainBalance=mainBalance-101100100;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPECIAL_NUMBER_30:
                mainBalance=mainBalance-101101000;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("thousand",SYNC);
                break;

      case SPECIAL_NUMBER_31:
                mainBalance=mainBalance-101101100;
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("million",SYNC);
	        playPhrase("hundred",SYNC);
	        playPhrase("and",SYNC);
	        playPhrase("one",SYNC);
	        playPhrase("thousand",SYNC);
	        playPhrase("hundred",SYNC);
                break;

      case SPANISH_NUMBER:
      case PORTUGUESE_NUMBER:
                gaLog(mod,gvAppinfo.Debug,"Speaking Spanish/Portuguese");
                if (mainBalance >= 1000)
                {
                   switch (atol(thousands))
                   {
                      case 1:
                              playPhrase("thousand",SYNC);
                              mainBalance=mainBalance-1000;
                              break;
                      case 2:
                              if (!gvFlag.IsOperator)
                                 TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,"2000",SYNC);
                              else
                                 TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,"2000",SYNC);
                              mainBalance=mainBalance-2000;
                              break;
                      case 3:
                              if (!gvFlag.IsOperator)
                                 TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,"3000",SYNC);
                              else
                                 TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,"3000",SYNC);
                              mainBalance=mainBalance-3000;
                              break;
                      case 4:
                              if (!gvFlag.IsOperator)
                                 TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,"4000",SYNC);
                              else
                                 TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,"4000",SYNC);
                              mainBalance=mainBalance-4000;
                              break;
                      case 5:
                              if (!gvFlag.IsOperator)
                                 TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,"5000",SYNC);
                              else
                                 TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,"5000",SYNC);
                              mainBalance=mainBalance-5000;
                              break;
                      case 6:
                              if (!gvFlag.IsOperator)
                                 TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,"6000",SYNC);
                              else
                                 TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,"6000",SYNC);
                              mainBalance=mainBalance-6000;
                              break;
                      case 7:
                              if (!gvFlag.IsOperator)
                                 TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,"7000",SYNC);
                              else
                                 TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,"7000",SYNC);
                              mainBalance=mainBalance-7000;
                              break;
                      case 8:
                              if (!gvFlag.IsOperator)
                                 TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,"8000",SYNC);
                              else
                                 TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,"8000",SYNC);
                              mainBalance=mainBalance-8000;
                              break;
                      case 9:
                              if (!gvFlag.IsOperator)
                                 TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,"9000",SYNC);
                              else
                                 TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,"9000",SYNC);
                              mainBalance=mainBalance-9000;
                              break;
                   }
                }

                if (atol(thousands) > 0 && mainBalance > 0 && mainBalance < 100)
                   playPhrase("and",SYNC);

                if (mainBalance >= 100 && mainBalance < 1000)
                {
                   switch (atol(hundreds))
                   {
                      case 1:
                              mainBalance=mainBalance-100;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("hundred",SYNC);
                              }
                              else
                                 playPhrase("hundredPlus",SYNC);
                              break;
                      case 2:
                              mainBalance=mainBalance-200;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("twoHundred",SYNC);
                              }
                              else
                                 playPhrase("twoHundred",SYNC);
                              break;
                      case 3:
                              mainBalance=mainBalance-300;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("threeHundred",SYNC);
                              }
                              else
                                 playPhrase("threeHundred",SYNC);
                              break;
                      case 4:
                              mainBalance=mainBalance-400;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("fourHundred",SYNC);
                              }
                              else
                                 playPhrase("fourHundred",SYNC);
                              break;
                      case 5:
                              mainBalance=mainBalance-500;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("fiveHundred",SYNC);
                              }
                              else
                                 playPhrase("fiveHundred",SYNC);
                              break;
                      case 6:
                              mainBalance=mainBalance-600;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("sixHundred",SYNC);
                              }
                              else
                                 playPhrase("sixHundred",SYNC);
                              break;
                      case 7:
                              mainBalance=mainBalance-700;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("sevenHundred",SYNC);
                              }
                              else
                                 playPhrase("sevenHundred",SYNC);
                              break;
                      case 8:
                              mainBalance=mainBalance-800;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("eightHundred",SYNC);
                              }
                              else
                                 playPhrase("eightHundred",SYNC);
                              break;
                      case 9:
                              mainBalance=mainBalance-900;
                              if (mainBalance == 0)
                              {
                                 if (atol(thousands) > 0)
                                    playPhrase("and",SYNC);
                                 playPhrase("nineHundred",SYNC);
                              }
                              else
                                 playPhrase("nineHundred",SYNC);
                              break;
                   }
                   if (mainBalance > 0 && rc == PORTUGUESE_NUMBER)
                      playPhrase("and",SYNC);
                }
                break;

      case ORDINARY_NUMBER:
      case EXTRAORDINARY_NUMBER:
      default:
                break;
        
   } /* End switch special number type */

   if (type == IN_MONEY)
   {
      if (mainBalance > 0)
      {
         /* Speak remainder */
         sprintf(balance,"%ld",mainBalance);
         if (!gvFlag.IsOperator)
            TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,balance,sync);
         else
            TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,balance,sync);
      }
      if (atol(balance) > 0)
         playMainCurrencyUnit(language,mainBalance);

      /* Italy/Spain/Portugal have no minor currency units */

      if (strcmp(gvAppinfo.ApplicationCountry,ITALY) &&
          strcmp(gvAppinfo.ApplicationCountry,SPAIN) &&
          strcmp(gvAppinfo.ApplicationCountry,PORTUGAL))
      {
         gaVarLog(mod,0,"Speak minor currency units");
         if (strcmp(minorBalance,"00"))
         {
            if (atol(balance) > 0)
               playPhrase("and",SYNC);
            if (!gvFlag.IsOperator)
               TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,minorBalance,sync);
            else
               TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,minorBalance,sync);
            playMinorCurrencyUnit(language,atol(minorBalance));
         }
      }
   }
   else if (type == IN_TIME)
   {
      if (minutes > 0)
      {
         if (mainBalance > 0)
         {
            /* Speak remainder */
            sprintf(minuteStr,"%ld",mainBalance);
            if (!gvFlag.IsOperator)
               TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,minuteStr,sync);
            else
               TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,minuteStr,sync);
         }
         if (minutes == 1)
            playPhrase("minute",SYNC);
         else
            playPhrase("minutes",SYNC);
      }
      else if ((minutes <= 0) && (seconds <= 0))
         playPhrase("cardExpired",SYNC);

      if (language != LANGUAGE_1)
      {
         /* Not English, so switch vox files back again */
         if (rc=loadVoice(gvAppinfo.SysVoxSelection,gvAppinfo.AppVoxSelection) != 0)
         {
            gaLog(mod,0,"Failed to put sysVox back again");
            return;
         }
      }

      if (seconds == 0)
        return;

      playPhrase("and",SYNC);

      if (seconds > 0)
      {
         if (!gvFlag.IsOperator)
            TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,secondStr,sync);
         else
            TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,secondStr,sync);
         playPhrase("seconds",sync);
      }
   }
   else
   {
      gaLog(mod,0,"Invalid type - defaulting to IN_MONEY");
      if (mainBalance > 0)
      {
         /* Speak remainder */
         sprintf(balance,"%ld",mainBalance);
         if (!gvFlag.IsOperator)
            TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,balance,sync);
         else
            TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,balance,sync);
      }
      if (atol(balance) > 0)
         playMainCurrencyUnit(language,mainBalance);
      
      /* Italy/Spain/Portugal have no minor currency units */

      if (strcmp(gvAppinfo.ApplicationCountry,ITALY) &&
          strcmp(gvAppinfo.ApplicationCountry,SPAIN) &&
          strcmp(gvAppinfo.ApplicationCountry,PORTUGAL))
      {
         if (strcmp(minorBalance,"00"))
         {
            if (atol(balance) > 0)
               playPhrase("and",SYNC);
            if (!gvFlag.IsOperator)
               TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,STRING,NUMBER,minorBalance,sync);
            else
               TEL_Speak(SECOND_PARTY,SECOND_PARTY_INTRPT,STRING,NUMBER,minorBalance,sync);
            playMinorCurrencyUnit(language,atol(minorBalance));
         }
      }
   }

} /* END: playBalance() */


/*------------------------------------------------------------------------------
playMainCurrencyUnit(): Play the main currency unit
------------------------------------------------------------------------------*/
static void playMainCurrencyUnit(int language, long balance) 
{
   static char	mod[]="playMainCurrencyUnit";
   char		mainCurrencyUnitPhrase[ARRAY_SIZE];

   memset(mainCurrencyUnitPhrase,'\0',sizeof(mainCurrencyUnitPhrase));
   strcpy(mainCurrencyUnitPhrase,"mainCurrencyUnit1");

   switch (language)
   {
      case LANGUAGE_4:
             if (balance >= 2)
                strcpy(mainCurrencyUnitPhrase,"mainCurrencyUnit2");
             else if (balance >= 1000000)
                strcpy(mainCurrencyUnitPhrase,"mainCurrencyUnit3");
             break;

      case LANGUAGE_1:
      case LANGUAGE_2:
      case LANGUAGE_3:
      case LANGUAGE_5:
      case LANGUAGE_6:
      case LANGUAGE_7:
      case LANGUAGE_8:
      case LANGUAGE_9:
      default:
             if (balance >= 2)
                strcpy(mainCurrencyUnitPhrase,"mainCurrencyUnit2");
             break;
   }
   playPhrase(mainCurrencyUnitPhrase,SYNC);
   getCpuTime(mod,gvAppinfo.Debug);

} /* END: playMainCurrencyUnit() */


/*------------------------------------------------------------------------------
playMinorCurrencyUnit(): Play the minor currency unit
------------------------------------------------------------------------------*/
static void playMinorCurrencyUnit(int language, long balance) 
{
   static char	mod[]="playMinorCurrencyUnit";
   char		minorCurrencyUnitPhrase[ARRAY_SIZE];

   memset(minorCurrencyUnitPhrase,'\0',sizeof(minorCurrencyUnitPhrase));
   strcpy(minorCurrencyUnitPhrase,"minorCurrencyUnit1");
   switch (language)
   {
      case LANGUAGE_4:
             strcpy(minorCurrencyUnitPhrase,"");
             break;

      case LANGUAGE_1:
      case LANGUAGE_2:
      case LANGUAGE_3:
      case LANGUAGE_5:
      case LANGUAGE_6:
      case LANGUAGE_7:
      case LANGUAGE_8:
      case LANGUAGE_9:
      default:
             if (balance > 1)
                strcpy(minorCurrencyUnitPhrase,"minorCurrencyUnit2");
             break;
   }
   playPhrase(minorCurrencyUnitPhrase,SYNC);
   getCpuTime(mod,gvAppinfo.Debug);
} /* END: playMinorCurrencyUnit() */


/*------------------------------------------------------------------------------
checkSpecialNumber(): See if we have one of those special numbers e.g. 100,1000
------------------------------------------------------------------------------*/
static int checkSpecialNumber(char *hundredMillions, 
                              char *tenMillions, 
                              char *millions,
                              char *hundredThousands,
                              char *tenThousands,
                              char *thousands,
                              char *hundreds,
                              char *tens,
                              char *units,
                              char *tenths,
                              char *hundredths,
                              int language)
{
   static char	mod[]="checkSpecialNumber";
   char		binaryPattern[6];

   /* 
      Special numbers are those consisting of :-

      100 million (HM)
      1 million (M)
      100 thousand (HTH)
      1 thousand (TH)
      1 hundred (H)

      So we build a binary pattern of these components and check against the 
      binary patterns that determine that the number is a special type 

    */

   switch (language)
   {
      case LANGUAGE_2:
      case LANGUAGE_3:
      case LANGUAGE_4:
                          break;
      case LANGUAGE_5:
                          return(SPANISH_NUMBER);
                          break;
      case LANGUAGE_6:
                          return(PORTUGUESE_NUMBER);
                          break;
      case LANGUAGE_1:
      default:
                          return(ORDINARY_NUMBER);
                          break;
   }

   memset(binaryPattern,'\0',sizeof(binaryPattern));
   strcpy(binaryPattern,hundredMillions);
   strcat(binaryPattern,millions);
   strcat(binaryPattern,hundredThousands);
   strcat(binaryPattern,thousands);
   strcat(binaryPattern,hundreds);
   gaVarLog(mod,gvAppinfo.Debug,"binaryPattern <%s>",binaryPattern);

   if (!strcmp(binaryPattern,H))
      return(SPECIAL_NUMBER_1);
   else if (!strcmp(binaryPattern,TH))
      return(SPECIAL_NUMBER_2);
   else if (!strcmp(binaryPattern,TH_H))
      return(SPECIAL_NUMBER_3);
   else if (!strcmp(binaryPattern,HTH))
      return(SPECIAL_NUMBER_4);
   else if (!strcmp(binaryPattern,HTH_H))
      return(SPECIAL_NUMBER_5);
   else if (!strcmp(binaryPattern,HTH_TH))
      return(SPECIAL_NUMBER_6);
   else if (!strcmp(binaryPattern,HTH_TH_H))
      return(SPECIAL_NUMBER_7);
   else if (!strcmp(binaryPattern,M))
      return(SPECIAL_NUMBER_8);
   else if (!strcmp(binaryPattern,M_H))
      return(SPECIAL_NUMBER_9);
   else if (!strcmp(binaryPattern,M_TH))
      return(SPECIAL_NUMBER_10);
   else if (!strcmp(binaryPattern,M_TH_H))
      return(SPECIAL_NUMBER_11);
   else if (!strcmp(binaryPattern,M_HTH))
      return(SPECIAL_NUMBER_12);
   else if (!strcmp(binaryPattern,M_HTH_H))
      return(SPECIAL_NUMBER_13);
   else if (!strcmp(binaryPattern,M_HTH_TH))
      return(SPECIAL_NUMBER_14);
   else if (!strcmp(binaryPattern,M_HTH_TH_H))
      return(SPECIAL_NUMBER_15);
   else if (!strcmp(binaryPattern,HM))
      return(SPECIAL_NUMBER_16);
   else if (!strcmp(binaryPattern,HM_H))
      return(SPECIAL_NUMBER_17);
   else if (!strcmp(binaryPattern,HM_TH))
      return(SPECIAL_NUMBER_18);
   else if (!strcmp(binaryPattern,HM_TH_H))
      return(SPECIAL_NUMBER_19);
   else if (!strcmp(binaryPattern,HM_HTH))
      return(SPECIAL_NUMBER_20);
   else if (!strcmp(binaryPattern,HM_HTH_H))
      return(SPECIAL_NUMBER_21);
   else if (!strcmp(binaryPattern,HM_HTH_TH))
      return(SPECIAL_NUMBER_22);
   else if (!strcmp(binaryPattern,HM_HTH_TH_H))
      return(SPECIAL_NUMBER_23);
   else if (!strcmp(binaryPattern,HM_M))
      return(SPECIAL_NUMBER_24);
   else if (!strcmp(binaryPattern,HM_M_H))
      return(SPECIAL_NUMBER_25);
   else if (!strcmp(binaryPattern,HM_M_TH))
      return(SPECIAL_NUMBER_26);
   else if (!strcmp(binaryPattern,HM_M_TH_H))
      return(SPECIAL_NUMBER_27);
   else if (!strcmp(binaryPattern,HM_M_HTH))
      return(SPECIAL_NUMBER_28);
   else if (!strcmp(binaryPattern,HM_M_HTH_H))
      return(SPECIAL_NUMBER_29);
   else if (!strcmp(binaryPattern,HM_M_HTH_TH))
      return(SPECIAL_NUMBER_30);
   else if (!strcmp(binaryPattern,HM_M_HTH_TH_H))
      return(SPECIAL_NUMBER_31);
   else
      return(ORDINARY_NUMBER);

} /* END: speakBalance() */


/*------------------------------------------------------------------------------
parseBalance(): Get the constituent parts of the balance
------------------------------------------------------------------------------*/
static void parseBalance(char *balance,
                         char *hundredMillions, 
                         char *tenMillions, 
                         char *millions,
                         char *hundredThousands,
                         char *tenThousands,
                         char *thousands,
                         char *hundreds,
                         char *tens,
                         char *units,
                         char *tenths,
                         char *hundredths)
{
   static char	mod[]="parseBalance";
   int		balanceLen;

   memset(hundredMillions,'\0',sizeof(hundredMillions));
   memset(tenMillions,'\0',sizeof(tenMillions));
   memset(millions,'\0',sizeof(millions));
   memset(hundredThousands,'\0',sizeof(hundredThousands));
   memset(tenThousands,'\0',sizeof(tenThousands));
   memset(thousands,'\0',sizeof(thousands));
   memset(hundreds,'\0',sizeof(hundreds));
   memset(tens,'\0',sizeof(tens));
   memset(units,'\0',sizeof(units));
   memset(tenths,'\0',sizeof(tenths));
   memset(hundredths,'\0',sizeof(hundredths));
   sprintf(hundredMillions,"%c",'0');
   sprintf(tenMillions,"%c",'0');
   sprintf(millions,"%c",'0');
   sprintf(hundredThousands,"%c",'0');
   sprintf(tenThousands,"%c",'0');
   sprintf(thousands,"%c",'0');
   sprintf(hundreds,"%c",'0');
   sprintf(tens,"%c",'0');
   sprintf(units,"%c",'0');
   sprintf(tenths,"%c",'0');
   sprintf(hundredths,"%c",'0');

   balanceLen=strlen(balance);
   if (balance[balanceLen-3] == '.')
   {
      if (strlen(balance) > 11)
         sprintf(hundredMillions,"%c",balance[balanceLen-12]);
      if (strlen(balance) > 10)
         sprintf(tenMillions,"%c",balance[balanceLen-11]);
      if (strlen(balance) > 9)
         sprintf(millions,"%c",balance[balanceLen-10]);
      if (strlen(balance) > 8)
         sprintf(hundredThousands,"%c",balance[balanceLen-9]);
      if (strlen(balance) > 7)
         sprintf(tenThousands,"%c",balance[balanceLen-8]);
      if (strlen(balance) > 6)
         sprintf(thousands,"%c",balance[balanceLen-7]);
      if (strlen(balance) > 5)
         sprintf(hundreds,"%c",balance[balanceLen-6]);
      if (strlen(balance) > 4)
         sprintf(tens,"%c",balance[balanceLen-5]);
      sprintf(units,"%c",balance[balanceLen-4]);
      sprintf(tenths,"%c",balance[balanceLen-2]);
      sprintf(hundredths,"%c",balance[balanceLen-1]);
   }
   else if (balance[balanceLen-2] == '.')
   {
      if (strlen(balance) > 10)
         sprintf(hundredMillions,"%c",balance[balanceLen-11]);
      if (strlen(balance) > 9)
         sprintf(tenMillions,"%c",balance[balanceLen-10]);
      if (strlen(balance) > 8)
         sprintf(millions,"%c",balance[balanceLen-9]);
      if (strlen(balance) > 7)
         sprintf(hundredThousands,"%c",balance[balanceLen-8]);
      if (strlen(balance) > 6)
         sprintf(tenThousands,"%c",balance[balanceLen-7]);
      if (strlen(balance) > 5)
         sprintf(thousands,"%c",balance[balanceLen-6]);
      if (strlen(balance) > 4)
         sprintf(hundreds,"%c",balance[balanceLen-5]);
      if (strlen(balance) > 3)
         sprintf(tens,"%c",balance[balanceLen-4]);
      sprintf(units,"%c",balance[balanceLen-3]);
      sprintf(tenths,"%c",balance[balanceLen-1]);
      sprintf(hundredths,"%c",'0');
   }
   else
   {
      if (strlen(balance) > 8)
         sprintf(hundredMillions,"%c",balance[balanceLen-9]);
      if (strlen(balance) > 7)
         sprintf(tenMillions,"%c",balance[balanceLen-8]);
      if (strlen(balance) > 6)
         sprintf(millions,"%c",balance[balanceLen-7]);
      if (strlen(balance) > 5)
         sprintf(hundredThousands,"%c",balance[balanceLen-6]);
      if (strlen(balance) > 4)
         sprintf(tenThousands,"%c",balance[balanceLen-5]);
      if (strlen(balance) > 3)
         sprintf(thousands,"%c",balance[balanceLen-4]);
      if (strlen(balance) > 2)
         sprintf(hundreds,"%c",balance[balanceLen-3]);
      if (strlen(balance) > 1)
         sprintf(tens,"%c",balance[balanceLen-2]);
      sprintf(units,"%c",balance[balanceLen-1]);
      sprintf(tenths,"%c",'0');
      sprintf(hundredths,"%c",'0');
   }

   gaVarLog(mod,gvAppinfo.Debug,"hundredMillions <%s>",hundredMillions);
   gaVarLog(mod,gvAppinfo.Debug,"millions <%s> tenMillions <%s>",millions,tenMillions);
   gaVarLog(mod,gvAppinfo.Debug,"tenThousands<%s> Hundredthousands <%s>",
            tenThousands,hundredThousands);
   gaVarLog(mod,gvAppinfo.Debug,"hundreds <%s> thousands <%s>",hundreds,thousands);
   gaVarLog(mod,gvAppinfo.Debug,"tens <%s> units <%s>",tens,units);
   gaVarLog(mod,gvAppinfo.Debug,"tenths <%s> hundredths <%s>",tenths,hundredths);

} /* END: parseBalance() */


/*------------------------------------------------------------------------------
getGlobalMessages(): Get the filenames of the messages required in notifyCaller
                     and for operator assisted calls.
------------------------------------------------------------------------------*/
int getGlobalMessages(void)
{
   static char	mod[]="getGlobalMessages";
   int		rc=0;

   rc=taGetPhraseFromTag(ENTER_ACCOUNT_NO_MESSAGE,gvEnterAccountNoMessage);
   if (rc != SYSTEM_OK)
   {
      gaVarLog(mod,0,"Failed to get phrase from tag <%s> rc <%d>",ENTER_ACCOUNT_NO_MESSAGE,rc);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,gvAppinfo.Debug,"Loaded phrase <%s> from tag <%s>",gvEnterAccountNoMessage,ENTER_ACCOUNT_NO_MESSAGE);

   rc=taGetPhraseFromTag(ENTER_PIN_MESSAGE,gvEnterPINMessage);
   if (rc != SYSTEM_OK)
   {
      gaVarLog(mod,0,"Failed to get phrase from tag <%s> rc <%d>",ENTER_PIN_MESSAGE,rc);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,gvAppinfo.Debug,"Loaded phrase <%s> from tag <%s>",gvEnterPINMessage,ENTER_PIN_MESSAGE);

   rc=taGetPhraseFromTag(ENTER_PHONE_NO_MESSAGE,gvEnterPhoneNoMessage);
   if (rc != SYSTEM_OK)
   {
      gaVarLog(mod,0,"Failed to get phrase from tag <%s> rc <%d>",
               ENTER_PHONE_NO_MESSAGE,rc);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,gvAppinfo.Debug,"Loaded phrase <%s> from tag <%s>",gvEnterPhoneNoMessage,ENTER_PHONE_NO_MESSAGE);

   rc=taGetPhraseFromTag(WARNING_MESSAGE,gvWarningMessage);
   if (rc != SYSTEM_OK)
   {
      gaVarLog(mod,0,"Failed to get phrase from tag <%s> rc <%d>",WARNING_MESSAGE,rc);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,gvAppinfo.Debug,"Loaded phrase <%s> from tag <%s>",gvWarningMessage,WARNING_MESSAGE);

   rc=taGetPhraseFromTag(OUT_OF_TIME_MESSAGE,gvOutOfTimeMessage);
   if (rc != SYSTEM_OK)
   {
      gaVarLog(mod,0,"Failed to get phrase from tag <%s> rc <%d>",OUT_OF_TIME_MESSAGE,rc);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,gvAppinfo.Debug,"Loaded phrase <%s> from tag <%s>",gvOutOfTimeMessage,OUT_OF_TIME_MESSAGE);

   rc=taGetPhraseFromTag(RECHARGE_WARNING_MESSAGE,gvRechargeWarningMessage);
   if (rc != SYSTEM_OK)
   {
      gaVarLog(mod,0,"Failed to get phrase from tag <%s> rc <%d>",RECHARGE_WARNING_MESSAGE,rc);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,gvAppinfo.Debug,"Loaded phrase <%s> from tag <%s>",gvRechargeWarningMessage,RECHARGE_WARNING_MESSAGE);

   rc=taGetPhraseFromTag(ANNOUNCE_TO_OPERATOR_MESSAGE,gvAnnounceMessage);
   if (rc != SYSTEM_OK)
   {
      gaVarLog(mod,0,"Failed to get phrase from tag <%s> rc <%d>",ANNOUNCE_TO_OPERATOR_MESSAGE,rc);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,gvAppinfo.Debug,"Loaded phrase <%s> from tag <%s>",gvAnnounceMessage,ANNOUNCE_TO_OPERATOR_MESSAGE);

   return(SYSTEM_OK);

} /* END: getGlobalMessages() */


/*------------------------------------------------------------------------------
inpWelcomeGetStar(): Play welcome message and press the star key
------------------------------------------------------------------------------*/
int inpWelcomeGetStar()
{
   static char	mod[]="inpWelcomeGetStar";
   char		filename[ARRAY_SIZE];
   char		buf[2];
   int		rc=0;

   memset(filename,'\0',sizeof(filename));
   memset(buf,'\0',sizeof(buf));
   strcpy(filename,APP_DIR);
   strcat(filename,gvCaller.dnis);
   strcat(filename,"/");
   strcat(filename,WELCOME_MESSAGE);

   if (!strcmp(gvAppinfo.PlayWelcome,"YES"))
   {
      gaVarLog(mod,gvAppinfo.Debug,"Playing <%s>",filename);

      TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,PHRASE_FILE,PHRASE,filename,SYNC);

      if (!strcmp(gvAppinfo.PressStar,"YES"))
      {
         playPhrase("pressStarNow",SYNC);
	 TEL_GetDTMF(FIRST_PARTY,gvAppinfo.DtmfTimeout,gvAppinfo.InterDigitTimeout,1,NO,
                     TERMINATION_KEY,1,AUTOSKIP,NUMSTAR,buf,"");
      }
   }

   getCpuTime(mod,gvAppinfo.Debug);

   return(SYSTEM_OK);

} /* END: inpWelcomeGetStar() */


/*------------------------------------------------------------------------------
playGoodDay():
------------------------------------------------------------------------------*/
void playGoodDay(void)
{
   	static char	mod[]="playGoodDay";
	time_t		lTime;
	char		charHour[8];
	int		intHour=0;
	int		rc=0;

	/*
	 * Get the currentTime and the hour
	 */
	memset(charHour,'\0',sizeof(charHour));
	time(&lTime);
	cftime((char *)charHour, "%H", &lTime);
	intHour=(int)atoi(charHour);

        gaVarLog(mod,gvAppinfo.Debug,"ICP clock set at <%d>",intHour);

        intHour=intHour+gvAppinfo.TimeAdjustment;
        if (intHour == 24) intHour=0;

        gaVarLog(mod,gvAppinfo.Debug,"Adjusted ICP clock <%d>",intHour);

	if ((intHour >= 0) && (intHour < 12))
           playPhrase("goodMorning",SYNC);
	else if ((intHour >= 12) && (intHour < 17))
           playPhrase("goodAfternoon",SYNC);
	else
           playPhrase("goodEvening",SYNC);

        getCpuTime(mod,gvAppinfo.Debug);

} /* END: playGoodDay() */


/*------------------------------------------------------------------------------
loadVoice(): Load specified system and application prompts
------------------------------------------------------------------------------*/
int loadVoice(char *lang, char *tagFile)
{
   static char	mod[]="loadVoice";
   char 	languageIntStr[16];
   int 		rc=0;
   int 		i=0;

   memset(languageIntStr,'\0',sizeof(languageIntStr));

   for (i=4;i<=(int)strlen(lang)-1;i++)
      languageIntStr[i-4]=lang[i];

   if (rc=TEL_SetGlobal("$LANGUAGE",atoi(languageIntStr)) != TEL_SUCCESS)
   {
      gaVarLog(mod,0,"Failed to set language <%s> rc <%d>",lang,rc);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,0,"System Voice Directory <LANG%d>",atoi(languageIntStr));
   if (taLoadTags(tagFile) < 0)
   {
      gaVarLog(mod,0,"Failed to load tags from <%s>",tagFile);
      return(SYSTEM_ERROR);
   }
   gaVarLog(mod,gvAppinfo.Debug,"Application tag file <%s>",tagFile);
   /*
    * For debugging purposes, uncomment the following line to print
    * all phrase tags and lists loaded.
    */
   /*
   if (gvAppinfo.Debug)
      taPrintTags();
    */

   return(SYSTEM_OK);

} /* END: loadVoice() */
/*------------------------------------------------------------------------------
loadFeminineVoice(): Load specified feminine system prompts
------------------------------------------------------------------------------*/
int loadFeminineVoice(char *sysVoxStr)
{
   static char	mod[]="loadFeminineSysVox";
   char 	languageIntStr[7];
   char 	tmpSysVoxSelection[7];
   int 		rc=0;
   int 		i=0;


   memset(languageIntStr,'\0',sizeof(languageIntStr));
   memset(tmpSysVoxSelection,'\0',sizeof(tmpSysVoxSelection));

   for (i=4;i<=(int)strlen(sysVoxStr)-1;i++)
      languageIntStr[i-4]=sysVoxStr[i];

   strcpy(tmpSysVoxSelection,"1");
   strcat(tmpSysVoxSelection,languageIntStr);

   gaVarLog(mod,gvAppinfo.Debug,"Based on SysVox <%s>",sysVoxStr);
   gaVarLog(mod,gvAppinfo.Debug,"New Tmp SysVox <LANG%d>",atoi(tmpSysVoxSelection));

   if (rc=TEL_SetGlobal("$LANGUAGE",atoi(tmpSysVoxSelection)) != TEL_SUCCESS)
   {
      gaVarLog(mod,0,"Failed to load feminine sysVox <%d>",rc);
      return(SYSTEM_ERROR);
   }

} /* END: loadFeminineVoice() */

/*------------------------------------------------------------------------------
selectLanguage()
------------------------------------------------------------------------------*/
int selectLanguage()
{
   static char	mod[]="selectLanguage";
   int		rc=0;
   int		lang=0;
   int		languageOption=0;


   if (strcmp(gvAppinfo.MainMenuOptions,NOTHING))
   {

      /* Select the language required */
      rc=menu(FIRST_PARTY,FIRST_PARTY_INTRPT,SYNC,gvAppinfo.MainMenuOptions,
              "menu","","",gvAppinfo.MainMenuMaxTries);

      switch(rc)
      {
            case ONE_PRESSED:
            case STAR_ONE_PRESSED:
            case HASH_ONE_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 1 selected");
                           languageOption=0;
                           break;

            case TWO_PRESSED:
            case STAR_TWO_PRESSED:
            case HASH_TWO_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 2 selected");
                           languageOption=1;
                           break;

            case THREE_PRESSED:
            case STAR_THREE_PRESSED:
            case HASH_THREE_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 3 selected");
                           languageOption=2;
                           break;

            case FOUR_PRESSED:
            case STAR_FOUR_PRESSED:
            case HASH_FOUR_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 4 selected");
                           languageOption=3;
                           break;

            case FIVE_PRESSED:
            case STAR_FIVE_PRESSED:
            case HASH_FIVE_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 5 selected");
                           languageOption=4;
                           break;

            case SIX_PRESSED:
            case STAR_SIX_PRESSED:
            case HASH_SIX_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 6 selected");
                           languageOption=5;
                           break;

            case SEVEN_PRESSED:
            case STAR_SEVEN_PRESSED:
            case HASH_SEVEN_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 7 selected");
                           languageOption=6;
                           break;

            case EIGHT_PRESSED:
            case STAR_EIGHT_PRESSED:
            case HASH_EIGHT_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 8 selected");
                           languageOption=7;
                           break;

            case NINE_PRESSED:
            case STAR_NINE_PRESSED:
            case HASH_NINE_PRESSED:
	                   gaLog(mod,gvAppinfo.Debug,"Language option 9 selected");
                           languageOption=8;
                           break;

            case TOO_MANY_TRIES:
            case UNDEFINED:
            default:
	                   gaLog(mod,gvAppinfo.Debug,"Invalid or no language selected");
                           strcpy(gvCaller.language,NO_LANGUAGE_SET);
                           return(SYSTEM_OK);
      }
   }
   setLanguage(languageOption);
   gaVarLog(mod,gvAppinfo.Debug,"Switching to option <%d> lang <%s>",languageOption,gvCaller.language);

   /* IRISH KLUDGE
   if (!strcmp(gvCaller.language,UK) && !strcmp(gvAppinfo.ApplicationCountry,IRELAND))
   {
      gaLog(mod,gvAppinfo.Debug,"Speaking Irish-English");
      return(SYSTEM_OK);
   }
   */

   /*
   if (strcmp(gvCaller.language,gvAppinfo.ApplicationCountry))
      return(switchLanguage(gvCaller.language));
   */
   return(switchLanguage(gvCaller.language));

} /* END: selectLanguage() */


/*------------------------------------------------------------------------------
switchLanguage()
------------------------------------------------------------------------------*/
int switchLanguage(char *language)
{
   static char	mod[]="switchLanguage";
   int		rc=0;


   gaVarLog(mod,gvAppinfo.Debug,"language <%s>",language);

   if (!strcmp(language,UK))
      strcpy(language,ENGLISH);
   else if (!strcmp(language,FRANCE))
      strcpy(language,FRENCH);
   else if (!strcmp(language,GERMANY))
      strcpy(language,GERMAN);
   else if (!strcmp(language,ITALY))
      strcpy(language,ITALIAN);
   else if (!strcmp(language,SPAIN))
      strcpy(language,SPANISH);
   else if (!strcmp(language,PORTUGAL))
      strcpy(language,PORTUGUESE);
   else if (!strcmp(language,TURKEY))
      strcpy(language,TURKISH);
   else if (!strcmp(language,SERBIA))
      strcpy(language,SERBOCROAT);
   else if (!strcmp(language,GREECE))
      strcpy(language,GREEK);

   if (!strcmp(language,ENGLISH))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang1SysVox,NOTHING) || !strcmp(gvAppinfo.Lang1AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang1SysVox,gvAppinfo.Lang1AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang1SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang1AppVox);
      strcpy(gvCaller.language,ENGLISH);
   }
   else if (!strcmp(language,FRENCH))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang2SysVox,NOTHING) || !strcmp(gvAppinfo.Lang2AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang2SysVox,gvAppinfo.Lang2AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang2SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang2AppVox);
      strcpy(gvCaller.language,FRENCH);
   }
   else if (!strcmp(language,GERMAN))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang3SysVox,NOTHING) || !strcmp(gvAppinfo.Lang3AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang3SysVox,gvAppinfo.Lang3AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang3SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang3AppVox);
      strcpy(gvCaller.language,GERMAN);
   }
   else if (!strcmp(language,ITALIAN))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang4SysVox,NOTHING) || !strcmp(gvAppinfo.Lang4AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang4SysVox,gvAppinfo.Lang4AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang4SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang4AppVox);
      strcpy(gvCaller.language,ITALIAN);
   }
   else if (!strcmp(language,SPANISH))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang5SysVox,NOTHING) || !strcmp(gvAppinfo.Lang5AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang5SysVox,gvAppinfo.Lang5AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang5SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang5AppVox);
      strcpy(gvCaller.language,SPANISH);
   }
   else if (!strcmp(language,PORTUGUESE))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang6SysVox,NOTHING) || !strcmp(gvAppinfo.Lang6AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang6SysVox,gvAppinfo.Lang6AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang6SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang6AppVox);
      strcpy(gvCaller.language,PORTUGUESE);
   }
   else if (!strcmp(language,TURKISH))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang7SysVox,NOTHING) || !strcmp(gvAppinfo.Lang7AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang7SysVox,gvAppinfo.Lang7AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang7SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang7AppVox);
      strcpy(gvCaller.language,TURKISH);
   }
   else if (!strcmp(language,SERBOCROAT))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang8SysVox,NOTHING) || !strcmp(gvAppinfo.Lang8AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang8SysVox,gvAppinfo.Lang8AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang8SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang8AppVox);
      strcpy(gvCaller.language,SERBOCROAT);
   }
   else if (!strcmp(language,GREEK))
   {
      gaVarLog(mod,0,"Switching to language <%s>",language);
      if (!strcmp(gvAppinfo.Lang9SysVox,NOTHING) || !strcmp(gvAppinfo.Lang9AppVox,NOTHING))
         return(SYSTEM_OK);
      if (rc=loadVoice(gvAppinfo.Lang9SysVox,gvAppinfo.Lang9AppVox) != 0)
         return(SYSTEM_ERROR);
      strcpy(gvAppinfo.SysVoxSelection,gvAppinfo.Lang9SysVox);
      strcpy(gvAppinfo.AppVoxSelection,gvAppinfo.Lang9AppVox);
      strcpy(gvCaller.language,GREEK);
   }
   else
   {
      gaVarLog(mod,0,"Cannot switch to language <%s>",language);
      return(SYSTEM_OK);
   }
   if ((rc=getGlobalMessages()) == SYSTEM_ERROR)
      gaLog(mod,0,"Could not get global messages");

   return(SYSTEM_OK);
} /* END: switchLanguage() */


/*------------------------------------------------------------------------------
addLanguagePrefix(): Determine the name of the voice file to make call
------------------------------------------------------------------------------*/
void addLanguagePrefix(char *filenameWithPrefix, char *filenameWithoutPrefix)
{
    static char	mod[]="addLanguagePrefix";

    gaVarLog(mod,gvAppinfo.Debug,"language <%s>",gvCaller.language);
    memset(filenameWithPrefix,0,sizeof(filenameWithPrefix));
    strcpy(filenameWithPrefix,APP_DIR);
    strcat(filenameWithPrefix,gvCaller.dnis);
    strcat(filenameWithPrefix,"/");
    if (strcmp(gvCaller.language,""))
    {
       strcat(filenameWithPrefix,gvCaller.language);
       strcat(filenameWithPrefix,".");
    }
    strcat(filenameWithPrefix,filenameWithoutPrefix);
    gaVarLog(mod,gvAppinfo.Debug,"filenameWithPrefix <%s>",filenameWithPrefix);

} /* END: switchLanguage() */
/*------------------------------------------------------------------------------
setLanguage(): Get the language corresponding to the menu option
------------------------------------------------------------------------------*/
void setLanguage(int languageOption)
{
    static char	mod[]="setLanguage";
    int         language;
    char 	languageIntStr[16];
    int         i=0;
    int         j=0;
    int         k=0;
    char        option[32][8];

    memset(languageIntStr,'\0',sizeof(languageIntStr));
    memset(option,'\0',sizeof(option));

    gaVarLog(mod,gvAppinfo.Debug,"Using language option <%d>",languageOption);

    for (i=0;i<=(int)strlen(gvAppinfo.LanguageOptions)-1;i++)
    {
       if (gvAppinfo.LanguageOptions[i] == ',')
       {
          j++;
          k=0;
          continue;
       }
       option[j][k]=gvAppinfo.LanguageOptions[i];
       k++;
    }

    gaVarLog(mod,gvAppinfo.Debug,"Setting to <%s>",option[languageOption]);
    strcpy(gvCaller.language,option[languageOption]); 

    if (!strcmp(gvCaller.language,NOTHING))
    {
       /* gaVarLog(mod,gvAppinfo.Debug,"Resetting language to <%s>",gvAppinfo.ApplicationCountry); */
       /* strcpy(gvCaller.language,gvAppinfo.ApplicationCountry); */

       TEL_GetGlobal("$LANGUAGE",&language);
       sprintf(languageIntStr,"%d",language);
       gaVarLog(mod,gvAppinfo.Debug,"Resetting to default language <%s>",languageIntStr);
       strcpy(gvCaller.language,languageIntStr);
    }

} /* END: setLanguage() */


/*------------------------------------------------------------------------------
playCallHelpline(): Play a message informing caller of helpline number
------------------------------------------------------------------------------*/
void playCallHelpline(void)
{
    static char	   mod[]="callHelpline";
    char	   callHelplineVox[ARRAY_SIZE];


    memset(callHelplineVox,0,sizeof(callHelplineVox));
    addLanguagePrefix(callHelplineVox,CALL_HELPLINE_MESSAGE);
    TEL_Speak(FIRST_PARTY,FIRST_PARTY_INTRPT,PHRASE_FILE,PHRASE,callHelplineVox,SYNC);
    getCpuTime(mod,gvAppinfo.Debug);

} /* END: playCallHelpline() */
/*------------------------------------------------------------------------------
speakGreek(): Play the units in Greek (f=FALSE, m=TRUE)
------------------------------------------------------------------------------*/
static void speakGreek(char *balance, int type)
{
   static char  mod[]="speakGreek";
   static int   minutes;
   static int   seconds;
   char         minuteStr[8];
   char         secondStr[8];
   char         lBalanceStr[16];
   char         iBalanceStr[16];
   float        fBalance;
   long         lBalance;
   int          iBalance;
   int          dot;
   int          rc=0;

   fBalance=atof(balance);
   lBalance=atol(balance);
   iBalance=((fBalance*100)-(lBalance*100));

   memset(lBalanceStr,'\0',sizeof(lBalanceStr));
   sprintf(lBalanceStr,"%ld",lBalance);
   memset(iBalanceStr,'\0',sizeof(iBalanceStr));
   sprintf(iBalanceStr,"%02d",iBalance);

   if (lBalance <= 0 && iBalance <= 0)
   {
      playPhrase("cardExpired",SYNC);
      return;
   }

   if (type == FALSE)
   {
      /* pronounce(lBalanceStr,type); */
      /* playMainCurrencyUnit(LANGUAGE_9,lBalance); */
      /* playPhrase("and",SYNC); */
      /* pronounce(iBalanceStr,type); */
      /* playMinorCurrencyUnit(LANGUAGE_9,iBalance); */
      
      dot=strlen(lBalanceStr);
      iBalanceStr[0]=balance[dot+1];
      iBalanceStr[1]=balance[dot+2];
      if (iBalanceStr[1] == 0)
         iBalanceStr[1]='0';

      gaVarLog(mod,gvAppinfo.Debug,"balance <%s>.<%s> type <%d>",lBalanceStr,iBalanceStr,type);

      if (fBalance < 2000)
         type=TRUE;
      else
         type=FALSE;

      pronounce(lBalanceStr,type);

      if (strcmp(iBalanceStr,""))
         playPhrase("point",SYNC);

      if (iBalanceStr[0] == '0')
         playPhrase("zero",SYNC);
      
      pronounce(iBalanceStr,type);

      playMainCurrencyUnit(LANGUAGE_9,iBalance);
   }
   else
   {
      gaVarLog(mod,gvAppinfo.Debug,"balance <%s>.<%s> type <%d>",lBalanceStr,iBalanceStr,type);

      minutes=lBalance/60;
      seconds=lBalance%60;
      sprintf(minuteStr,"%d",minutes);
      sprintf(secondStr,"%d",seconds);

      if (minutes > 0)
      {
         pronounce(minuteStr,type);

         if (minutes == 1)
            playPhrase("minute",SYNC);
         else
            playPhrase("minutes",SYNC);

         if (seconds > 0)
            playPhrase("and",SYNC);
      }
      if (seconds > 0)
      {
         pronounce(secondStr,type);

         if (seconds == 1)
            playPhrase("second",SYNC);
         else
            playPhrase("seconds",SYNC);
      }
   }

} /* END: speakGreek() */



greek.c

include "application.h"

#ifndef TRUE
	#define TRUE   1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

/*
	This program is the real algorithm for Greek number pronuncing. If you use
        it in your application you will have what we want.

	This program is checked and behaves correctly on number situation.
*/

struct prompt
{
	int	        code;
	char		*str;
}; 

struct prompt malePrompts[] =
	{
		{ 1, "ena.32a"},
		{ 2, "dio.32a"},
		{ 3, "tria.32a"},
		{ 4, "tessera.32a"},
		{ 5, "pente.32a"},
		{ 6, "eksi.32a"},
		{ 7, "epta.32a"},
		{ 8, "okto.32a"},
		{ 9, "ennea.32a"},
		{ 10, "deka.32a"},
		{ 11, "enteka.32a"},
		{ 12, "dodeka.32a"},
		{ 20, "eikosi.32a"},
		{ 30, "trianta.32a"},
		{ 40, "saranta.32a"},
		{ 50, "peninta.32a"},
		{ 60, "eksinta.32a"},
		{ 70, "evdominta.32a"},
		{ 80, "ogdonta.32a"},
		{ 90, "eneninta.32a"},
		{ 100, "ekato.32a"},
		{ 200, "diakosia.32a"},
		{ 300, "triakosia.32a"},
		{ 400, "tetrakosia.32a"},
		{ 500, "pentakosia.32a"},
		{ 600, "eksakosia.32a"},
		{ 700, "eptakosia.32a"},
		{ 800, "oktakosia.32a"},
		{ 900, "enniakosia.32a"},
		{ 1000, "hilia.32a"},
		{ -1, ""}
	};

struct prompt femalePrompts[] =
	{
		{ 1, "mia.32a"},
		{ 2, "dio.32a"},
		{ 3, "tris.32a"},
		{ 4, "tesseris.32a"},
		{ 5, "pente.32a"},
		{ 6, "eksi.32a"},
		{ 7, "epta.32a"},
		{ 8, "okto.32a"},
		{ 9, "ennea.32a"},
		{ 10, "deka.32a"},
		{ 11, "enteka.32a"},
		{ 12, "dodeka.32a"},
		{ 20, "eikosi.32a"},
		{ 30, "trianta.32a"},
		{ 40, "saranta.32a"},
		{ 50, "peninta.32a"},
		{ 60, "eksinta.32a"},
		{ 70, "evdominta.32a"},
		{ 80, "ogdonta.32a"},
		{ 90, "eneninta.32a"},
		{ 100, "ekato.32a"},
		{ 200, "diakosies.32a"},
		{ 300, "triakosies.32a"},
		{ 400, "tetrakosies.32a"},
		{ 500, "pentakosies.32a"},
		{ 600, "eksakosies.32a"},
		{ 700, "eptakosies.32a"},
		{ 800, "oktakosies.32a"},
		{ 900, "enniakosies.32a"},
		{ 1000, "hilies.32a"},
		{ -1, ""}
	};

void word( char *word)
{
   playFile(word);
}

void spell(int number, int isMale)
{
	int    i;

	struct prompt	*prompts = ((isMale) ?malePrompts :femalePrompts);

	for(i=0;prompts[i].code >= 0;i++)
		if(prompts[i].code == number)
		{
			word( prompts[i].str);
			return;
		}
}

void trimZero(char *number)
{
	int	i, j;

	for (i=0;number[i] && number[i] == '0';i++)
		;

	for (j=0;number[i];i++,j++)
		number[j]=number[i];

	number[j]=0;
}

void pronounce(char *number, int isMale)
{
	char		tmp[20];

	if (atoi(number) > 999999) /* We don't have such situation in PrePaid application */
	   return;

	trimZero(number);

	if (strlen(number) > 20) /* There is no such possibility. Just for check */
	   return;
	
	if (strlen(number) > 3) 
	{
		strcpy(tmp,number);
		tmp[strlen(number)-3]=0; /* now the tmp holds everything bigger than hunderds */
		if (atoi(number) >= 2000)
		{
		   pronounce(tmp,isMale);
		   word("hiliades.32a");
		} 
                else
		   spell(1000,isMale);

		strcpy(number,&(number[strlen(number)-3]));
		trimZero(number);
	}
	if (strlen(number) > 2) /* Remember that this routine could be used for thousands */
	{
		spell((number[0]-'0')*100,isMale); /* e.g. number[0]="3" spell("3"-"0"*100)=300 */
		strcpy(number,&(number[strlen(number)-2]));
		trimZero(number);
	}
	if (strlen(number) > 1)
	{
		if (atoi(number) == 11 || atoi(number) == 12 || number[1] == '0')
		{
			spell(atoi(number),isMale);
			return;
		}
		spell((number[0]-'0')*10,isMale); 
		strcpy(number,&(number[strlen(number)-1]));
		trimZero(number);
	}
	spell(atoi(number),isMale);
}

/*
To test it try the following
	prompts 1653 m  Would give hilia eksakosia peninta tria
	prompts 1653 f  Would give hilies eksakosies peninta tris
*/
/*
int main(int argc, char **argv)
{
	if (--argc < 2)
		return 0;
	if (strcmp( argv[2],"m") == 0)
		pronounce( argv[1], TRUE);
	else
		pronounce(argv[1], FALSE);
	printf("\n");
}
*/

speakLang.doc		Page 13 of 13

