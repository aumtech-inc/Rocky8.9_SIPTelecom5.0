/*-----------------------------------------------------------------------------
Program:	TEL_PromptAndCollect.h
Purpose:	Internal #defines used with the TEL_PromptAndCollect APIs.
Author: 	G. Wenzel
Date:		03/01/01
Update:		03/08/01 gpw added overflowTag (not implemented yet)
Update:		03/08/01 gpw changed max tag size from 64 to 32
-----------------------------------------------------------------------------*/
/*--------------------------------------------
	CONSTANTS AND MACRO DEFINITIONS
--------------------------------------------*/
#define TAG_TYPE_PHRASE		1
#define TAG_TYPE_DATA		2
#define TAG_TYPE_STRING		3
#define	MAX_TAG_SIZE		32	/* Maximum length of a tag name */
#define	MAX_TAG_LIST		32	/* Max # of tags in a list */
#define	MAX_PATH_SIZE		512
#define INPUT_BUF_SIZE		2048

/*--------------------------------------------
	TYPEDEFS
--------------------------------------------*/
typedef struct tagStruct
{
	char	tagName[MAX_TAG_SIZE];	/* original name of tag */
	int	type;
	int	inputFormat;
	int	outputFormat;
	char	data[MAX_PATH_SIZE];
} Tag;

typedef struct phraseTagStruct
{
	char	tagName[MAX_TAG_SIZE];	/* name of phrase tag */
	int	nTags;/* # of tags for this name 0 thru MAX_TAG_LIST */
	Tag	tag[MAX_TAG_LIST]; 
				/* (sequence of) the locations of the phrases.*/
	struct phraseTagStruct *nextTag;
} PhraseTagList;

typedef struct pncInputStruct
{
	char	sectionName[128];
	char	promptTag[128];		/* Initial prompt phrase */
	char	repromptTag[128]; 	/* Re-prompt phrase */
	char	invalidTag[128];	/* Phrase spoken on invalid input */
	char	timeoutTag[128];	/* Phrase spoken if timeout on input */
	char	shortInputTag[128];	/* Phrase spoken if input is < minLen */
	char	overflowTag[128];	/* Phrase spoken if input is > maxLen */
	char	validKeys[32];		/* String of valid DTMF keys */
	char	hotkeyList[64];	/* String of comma-delimited list of hotkeys */
	int	party; 			/* party from whom input is expected */
	int	firstDigitTimeout;	/* First digit timeout	*/	
	int	interDigitTimeout;	/* Inter digit timeout	*/	
	int	nTries; 		/* Number of tries to get input	*/
	int	beep;			/* Whether or not to beep caller */
	char	terminateKey;		/* Terminate key */
	int	minLen; 		/* Min. length of input expected */
	int	maxLen; 		/* Max. length of input expected */
	int	terminateOption;	/* (MANDATORY / AUTOSKIP) */
	int	inputOption;	/* Input Informat TEL_GetDTMF (from Telecom.h)*/
	int	interruptOption;/* Input Informat TEL_Speak (from Telecom.h) */

	struct pncInputStruct	*nextSection;
} PncInput;


typedef struct tagFileStruct
{
	char	tagFile[128];
	int	numLines;
} TagFileList;

#define MAX_TAG_FILES		20

/* The following are #defines for the name-value pairs within the sections */
#define NV_PROMPT_TAG		"promptTag"
#define NV_REPROMPT_TAG		"repromptTag"
#define NV_INVALID_TAG		"invalidTag"
#define NV_TIMEOUT_TAG		"timeoutTag"
#define NV_SHORT_INPUT_TAG	"shortInputTag"
#define NV_OVERFLOW_TAG		"overflowTag" 	/* Added 3/8/2001 gpw */
#define NV_VALID_KEYS		"validKeys"
#define NV_HOTKEY_LIST		"hotkeyList"
#define NV_PARTY		"party"
#define NV_FIRST_DIGIT_TIMEOUT	"firstDigitTimeout"
#define NV_INTER_DIGIT_TIMEOUT	"interDigitTimeout"
#define NV_NTRIES		"nTries"
#define NV_BEEP			"beep"
#define NV_TERMINATE_KEY	"terminateKey"
#define NV_MIN_LEN		"minLen"
#define NV_MAX_LEN		"maxLen"
#define NV_TERMINATE_OPTION	"terminateOption"
#define NV_INPUT_OPTION		"inputOption"
#define NV_INTERRUPT_OPTION	"interruptOption"

