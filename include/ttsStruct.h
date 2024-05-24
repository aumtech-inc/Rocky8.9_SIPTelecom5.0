/*------------------------------------------------------------------------------
File:		ttsStruct.h

Purpose:	Contains all common header information used by the ttsClient 
		and ttsServer program.

Author:		Sandeep Agate

Update:		08/05/97 sja	Created the file
Update:		11/23/99 mpb	Changed string in TTS_REQUEST structure from
				256 to 2048.
Update:		03/15/2002 sja	Changed TTS_SUCCESS to ARC_TTS_SUCCESS
Update:		03/15/2002 sja	Changed TTS_FAILURE to ARC_TTS_FAILURE
Update:		03/15/2002 sja	Changed TTS_REQUEST to ARC_TTS_REQUEST
Update:		03/15/2002 sja	Changed TTS_RESULT to ARC_TTS_RESULT
Update:		07/15/2002 mpb	Added timeOut to Request Structure 
Update:		07/15/2002 mpb	Changed resultFifo lenght form 80 to 64. 
Update:		07/20/2002 mpb	Changed ttsRequest.string from 512 to 256
Update:		10/31/2002 apj  added async & asyncCompletionFifo ttsRequest struct.
Update:		04/03/2007 djb  increased resultCode in ttsResult structure.
Update:     14/12/2009 djb  Removed voiceName from tts structures
------------------------------------------------------------------------------*/
#ifndef ttsStruct_H__		/* Just preventing multiple includes */
#define ttsStruct_H__

/* TTS FIFO Information */
#define TTS_REQUEST_FIFO	"/tmp/tts.fifo"
#define PERMS			0666

/* Result codes indicating success or failure */

#define ARC_TTS_SUCCESS	'1'
#define ARC_TTS_FAILURE	'2'

/* Structures used to send info between the ttsClient & the ttsServer */

typedef struct ttsRequest
{
    char    fileName[96];  /* Absolute Path of file and fileName     */
    char    type[8];  		/* file, string or e-mail     */
	char	port[5];		/* Port of the application requesting TTS */
	char	pid[7];		/* PID of the application requesting TTS  */
	char	app[30];	/* Name of the application requesting TTS */
	char	string[256];	/* String to be converted to speech       */
	char	language[20];	/* Language */
	char	gender[8];	/* MALE or FEMALE */
	char	compression[12];	/* Compression */
	char	timeOut[5];	/* Compression */
	char	resultFifo[64];	/* Name of result FIFO			  */
} ARC_TTS_REQUEST;

typedef struct ttsRequestSingleDM
{
    char    fileName[96];  /* Absolute Path of file and fileName     */
    char    type[8];  		/* file, string or e-mail     */
	char	port[5];		/* Port of the application requesting TTS */
	char	pid[7];		/* PID of the application requesting TTS  */
	char	app[30];	/* Name of the application requesting TTS */
	char	string[256];	/* String to be converted to speech       */
	char	language[20];	/* Language */
	char	gender[8];	/* MALE or FEMALE */
    char    voiceName[20];  /* Optional name of the voice to be played */
	char	compression[12];	/* Compression */
	char	timeOut[5];	/* Compression */
	char	resultFifo[64];	/* Name of result FIFO			  */

	struct 	Msg_Speak	speakMsgToDM;
	int 	async;
	char 	asyncCompletionFifo[64];
} ARC_TTS_REQUEST_SINGLE_DM;

typedef struct ttsResult
{
    char    fileName[128];  /* Absolute Path of file and fileName     */
	char	port[5];	/* Port of the application requesting TTS */
	char	pid[7];		/* PID of the application requesting TTS     */
	char	resultFifo[80];	/* Name of result FIFO			  */
	char	resultCode[3];	/* Result code indicating success or failure */
	char	error[256];	/* String containing reason for failure      */
	char	speechFile[128];/* Name of the file containing speech        */
} ARC_TTS_RESULT;

#endif /* ttsStruct_H__ */
