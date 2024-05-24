/*----------------------------------------------------------------------------
Program:    arcMLInternal.h
Purpose:    To define constants, variables, prototypes,  used internally by
			Multiple Language applications.
Author:     Aumtech
Date:       2002/01/05
-----------------------------------------------------------------------------*/
#include <stdio.h>

#define	ARRAY_SIZE		128
#define	DEBUG			1
#define	ML_SVC			"TEL"
#define ML_CURRENCY		1
#define ML_NUMBER		2

/* Special numbers */
#define EXTRAORDINARY_NUMBER	-100
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


#ifdef ML_COMMON
	int  gFrenchLoaded       = 0;
	int  gSpanishLoaded      = 0;
	int  *gpLoadedLanguage;
#else
	extern int	gFrenchLoaded;
	extern int	gSpanishLoaded;
	extern int  *gpLoadedLanguage;
#endif

extern char GV_Application_Name[];
extern char GV_Resource_Name[];

/*
 * Function Prototypes
 */
int	mlGetIspBase(char *zIspBase);
int	mlLoadLanguage(int zLanguageId, int zGender);
int	mlPlayMainCurrencyUnit(int language, long balance, 
					int zParty, int zInterruptOption, int zSync);
int	mlPlayMinorCurrencyUnit(int language, long balance,
			int zParty, int zInterruptOption, int zSync);

int	mlRestoreLangTag(int zReloadLanguage, char *zReloadTagFile);

void mlParseBalance(char *balance,
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

int  mlCheckSpecialNumber(char *hundredMillions, 
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

int  mlLoadFeminineVoice(char *sysVoxStr);

int	mlSpeak(int zLanguage, int zType, int zParty, int zInterruptOption,
		char *zData, int zSync);
