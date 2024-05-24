/*----------------------------------------------------------------------------
Program:    arcML.h
Purpose:    To define constants, prototypes, etc. used by
			Multiple Language applications.
Author:     Aumtech
Date:       2005/05/31
-----------------------------------------------------------------------------*/

#define	ML_FRENCH		33
#define	ML_SPANISH		34

#define ML_NEUTRAL		0
#define ML_FEMALE		1
#define ML_MALE			2

int TEL_MLSpeakCurrency(int zLanguage, int zGender, int zReloadLanguage, 
		char *zReloadTagFile, int zParty, int zInterruptOption,
		char *zData, int zSync);

int TEL_MLSpeakNumber(int zLanguage, int zGender, int zReloadLanguage, 
		char *zReloadTagFile, int zParty, int zInterruptOption, 
		int zOutputFormat, char *zData, int zSync);

int TEL_MLSpeakTime(int zLanguage, int zGender, int zReloadLanguage, 
		char *zReloadTagFile, int zParty, int zInterruptOption, 
		int zInputFormat, int zOutputFormat, char *zData, int zSync);

int TEL_MLSpeakDate(int zLanguage, int zGender, int zReloadLanguage,
		char *zReloadTagFile, int zParty, int zInterruptOption,
		int zInputFormat, int zOutputFormat, char *zData, int zSync);

