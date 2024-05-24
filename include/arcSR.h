/* ----------------------------------------------------------------------
Purpose:	Common includes, defines, globals, and prototypes
typedef struct
{
    int         party;
    int         interruptOption;
    int         leadSilence;
    int         trailSilence;
    int         totalTime;
	int			beep;
    char        resourceNames[256];
    int         numResults;         // formerly numAlternatives
	char		termination_char;
} SRRecognizeParams;
			for speech api's
Author :	Aumtech Inc.
--------------------------------------------------------------------------*/  
#ifndef ARC_SR_H
#define ARC_SR_H

#define	SR_TRANSCRIBE_ONLY			0
#define	SR_ADD_TO_RESOURCE			1

#define	SR_STRING			0
#define	SR_FILE				1
#define	SR_URI				2
#define	SR_GRAM_ID			3

#define GOOGLE_SR			140
#define MS_SR				141
/*
** Defines the different types of data that can be requested.
** This is for backwards compatability and Philips SP 2000.
*/
#define SR_PATH_WORD				1
#define SR_PATH_WORD_CONF			2
#define SR_CONCEPT_WORD				3
#define SR_CONCEPT_WORD_CONF		4
#define SR_TAG						5
#define SR_TAG_CONFIDENCE			6
#define SR_CONCEPT					7
#define SR_CONCEPT_CONFIDENCE		8
#define SR_CATEGORY					9                  
#define SR_ATTRIBUTE_VALUE			10            
#define SR_ATTRIBUTE_NAME			11                  
#define SR_ATTRIBUTE_CONCEPTS		12                  
#define SR_ATTRIBUTE_TYPE			13                  
#define SR_ATTRIBUTE_STATUS			14                  
#define SR_ATTRIBUTE_CONFIDENCE		15                  
#define SR_CONCEPT_AND_TAG			16                  
#define SR_CONCEPT_AND_ATTRIBUTE_PAIR		17
#define	SR_REC						18
#define SR_LEARN					19                  
#define SR_CONCEPT_AND_WORD			20                  
#define SR_INPUT_MODE               21
#define SR_GOOGLE               	99

/*
** Defines the different types of data that can be requested
** (SpeechWorks and Scansoft XML1.0).
*/
#define SR_SPOKEN_WORD						3
/* #define SR_TAG							5  - defined above. */
#define SR_CONCEPT_AND_SPOKEN_WORD_PAIR		17                  
#define SR_CONCEPT_AND_TAG_PAIR				21                  
#define SR_GRAMMAR_AND_SPOKEN_WORD			20
#define SR_GRAMMAR_AND_TAG					16

/* Keep these for backward compatibility */

#define SR_WORD_SPELLING			SR_PATH_WORD
#define SR_WORD_CONFIDENCE			SR_PATH_WORD_CONF
#define SR_ATTRIBUTE				SR_ATTRIBUTE_VALUE
#define SR_ALL						SR_REC

/* This holds the word information when adding or deleting words */

struct SR_WORDS
{
	char	wordSpelling[129];
	char	tag[129];
	char	phoneticSpelling[129];
	char	variantSpelling[129];
	int		result;
};

typedef struct			// For mrcpV2 recognize API
{
    int         party;
    int         bargein;
    int         interruptOption;
    int         leadSilence;
    int         trailSilence;
    int         totalTime;
	int			beep;
    char        resourceNames[256];
    int         numResults;         // formerly numAlternatives
	char		termination_char;
} SRRecognizeParams;
/*-----------------------------------------------------------
Following are the Speech Rec api's prototype
-------------------------------------------------------------*/

int TEL_SRInit(const char *resourceNames, int *vendorErrorCode);
int TEL_SRReleaseResource( int *vendorErrorCode );
int TEL_SRReserveResource( int *vendorErrorCode);
int TEL_SRSetParameter( char *parameterName, char *parameterValue );
int TEL_SRGetParameter( char *parameterName, char *parameterValue );
int TEL_SRLoadGrammar( int grammarType, char * grammarData, 
		char * parameters, int *vendorErrorCode);
int TEL_SRUnloadGrammars( int *vendorErrorCode);
int TEL_SRExit( void );
int TEL_SRGetResult(int alternativeNumber, int tokenType,
		char *delimeter, char *result, int *overallConfidence);
int TEL_SRGetPhoneticSpelling( char *resourceName, char * audioPath,
	    char * phoneticSpelling, int *vendorCode);
int TEL_SRLogEvent(char *LogEventText);
int TEL_SRRecognize(int party, int bargeIn, int touchToneInterrupt,
		int beep, int leadSilence, int trailSilence, int totalTime,
		int *numberOfAlternatives, char *resourceName, 
		int tokenTypeRequested, int *vendorCode);
int TEL_SRRecognizeV2(SRRecognizeParams *zParams);
int TEL_SRUtterance(char *zUtteranceFile, char *zTranslation);



int TEL_SRAddWords( char * resourceName, 
		char * 	categoryName, struct SR_WORDS *wordList, 
		int numWords, int *vendorErrorCode);
int TEL_SRLearnWord( char * resourceName, 
		char * 	categoryName, struct SR_WORDS *word, 
		int addTrans, int *vendorErrorCode);
int TEL_SRDeleteWords( char * resourceName, 
		char * 	categoryName, struct SR_WORDS *wordList, 
		int numWords, int *vendorErrorCode);

#endif