/*-----------------------------------------------------------------------------
File:		arcavb.c
Purpose:	Contains advance voice biometrics APIs.
-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "arcavb.h"
#include "arcavb_db_hooks.h"
#include "VoiceID_Au_publicAPI.h"
#include "VoiceIDutils.h"
#include "arc_decode.h"

#include "AppMessages.h"
#include "TEL_LogMessages.h"
#include "TEL_Mnemonics.h"

#ifndef AVB_STANDALONE
#include "TEL_Common.h"
#endif // AVB_STANDALONE


#define DEBUG_MSG

#define OPEN_MASK 0000 
#define DIRECTORY_CREATION_PERMS        0755
#define MAX_IMPOSTER_VOICEPRINTS		100

typedef struct
{
	char	Base_Directory[256];
	int		Keep_Mulaw_Files;
	int		Keep_PCM_Files;
	int		Keep_Error_Files;
	int		Continuous_Speech_Size;
	int		Continuous_Speech_Sleep;
	char	SpeechDir[256];
	char	MulawDir[256];
	char	ErrorDir[256];
	char	VpxDir[256];
} ArcAvbConfig;

typedef struct _my_header_wav
{
    char chunk_id[4];
    int chunk_size;
    char format[4];
    char subchunk1_id[4];
    int subchunk1_size;
    short int audio_format;
    short int num_channels;
    int sample_rate;			// sample_rate denotes the sampling rate.
    int byte_rate;
    short int block_align;
    short int bits_per_sample;
    char subchunk2_id[4];
    int subchunk2_size;			// subchunk2_size denotes the number of samples.
} my_header_wav;

#ifndef AVB_STANDALONE
extern int		GV_AppCallNum1;
#endif // AVB_STANDALONE

extern char		*get_time();

static int		gPort         = -1;
static char		gPcmFile[256];
static char		gPcmErrorFile[256];

static int		gVPExistsInDB = 0;
static int		hasContinuousResultBeenCalled = 0;

//static char popen_command[256];
//static int arc_popen(char *command, int zLine);

static void closeVoiceID(void *pv_VoiceID_conObj, void *pv_voiceprint);
static int lValidateDirectory(char *iDirectory, char *oErrorMsg);

static int arcsv_load_audio_buffer(char *pin, char *infile, char *command,
		char **buffer, long int *size, unsigned long *numSamples,
		char *pcmFile, char *err_msg);
static int arcsv_load_audio_buffer_from_file(int port, char *file_to_load,
		char *pcmFile, char **buffer, long int *size,
		unsigned long *numSamples, char *err_msg);
static int getPins_from_file(char *zPinList, int *numRecords, char *err);

//#ifndef AVB_DATABASE
static int writeVpxFile(char *zPin, void *zVoicePrint, unsigned zSize);
//#endif // AVB_DATABASE

static int updateVoiceprint(char *zPin, void *pv_voiceprint,
			unsigned ui_voiceprint_size, int vp_ready, char *dbMessage);

static short int MuLaw_Decode(char number);
static int readWavHeader(FILE *fdWav, my_header_wav *wavHeader, char *errorMsg);
static ArcAvbConfig arcavbConfig = { "", -1, -1, -1, -1, -1, "", "", "" };
static int removeWhiteSpace(char *buf);
 
extern int LogARCMsg(char *mod, int mode, char *portStr,
                char *obj, char *appName, int msgId,  char *msg);

static int readAVBCfgFile() ;
static int avbGetProfileString(char *section,
					char *key, char *defaultVal, char *value,
					long bufSize, FILE *fp, char *file);
static void myTrimWS(char *zStr);

void ARCAVB_Log(char *zFile, int zLine, char *zMod, int zMode, int zMsgId,
												char *messageFormat, ...);
static int temp_rtpSleep(int millisec);
static int avbCopyFile(char *zSourceFile, char	*zDestFile, char *zErrorMsg);

static int getExistingVoiceprint(char *zPin, void **pv_voiceprint,
			unsigned *ui_voiceprint_size, int *vp_ready, int *vp_utterances,
			float *indv_threshold, float *vp_goodness, int *zFileSize);

static int getSpecialExistingVoiceprint(char *zPin, void **pv_voiceprint,
			unsigned *ui_voiceprint_size, int *vp_ready, int *vp_utterances,
			float *indv_threshold, float *vp_goodness);

#ifdef AVB_STANDALONE
/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int ARCAVB_Init(int zPort)
{
	static char		mod[]="ARCAVB_Init";
	int				rc;

	gPort = zPort;
	readAVBCfgFile();

	return(0);
} // END: ARCAVB_Init()
#endif // AVB_STANDALONE

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int ARCAVB_Enroll_Train(char *zPin, char *zFile, int *zVpReady, int *zRetCode,
                        char *zErrString)
{
	static char		mod[]="ARCAVB_Enroll_Train";
	int				rc;
	void			*pv_VoiceID_conObj = NULL;
	void			*pv_voiceprint = NULL;
	short			*psh_buffer_16pcm;
	unsigned long	ul_NumSamplesRead;
	int				vp_ready;
	int				vp_utterances;
	float			indv_threshold;
	float			vp_goodness;
	unsigned		ui_voiceprint_size;
	char			*audio_buffer = NULL;
	long int		size;
	int				voiceprint_filesize;
	char			dbMessage[256];
	char			errMsg[2048];
	int				retCode;

	*zRetCode = 0;
	if ( arcavbConfig.Keep_Mulaw_Files == -1 )
	{
#ifndef AVB_STANDALONE
		gPort = GV_AppCallNum1;
#else
		gPort = 0;
#endif // AVB_STANDALONE
		readAVBCfgFile();
	}
	gVPExistsInDB = 1;
 
	*zRetCode = 0;
	zErrString[0] = '\0';

	*zVpReady = 0;
	pv_voiceprint = VoiceID_allocate_voiceprint(zPin, &ui_voiceprint_size, errMsg);
	if(pv_voiceprint == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString,
		     "Unable to allocate voiceprint; unable to retrieve it.  [%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				zErrString);
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Succesfully allocated voiceprint memory - %d bytes", ui_voiceprint_size);

	rc = getExistingVoiceprint(zPin, &pv_voiceprint,   // message is logged in routine
						&ui_voiceprint_size, &vp_ready,	&vp_utterances,
						&indv_threshold, &vp_goodness, &voiceprint_filesize);  
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"getExistingVoiceprint returned %d for pin (%s) [pv_size=%d, vp_ready=%d]",
		rc, zPin, ui_voiceprint_size, vp_ready);

	/* Create VoiceID container object */
	if( (pv_VoiceID_conObj = VoiceID_initialize(errMsg)) == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString, "VoiceID_initialize failed:[%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				zErrString);

		if((rc=VoiceID_release_voiceprint(pv_voiceprint, errMsg)))
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
				"VoiceID_release_voiceprint() failed. [%d, %s]",
				rc, errMsg);
		}
		errMsg[0] = '\0'; 
		return(-1);
	}
	
	/********************* IMPORTANT *****************************/
	/* The voiceprint has to be copied into the container before training. */
	/* Otherwise VoiceID_EnrollTrain() cannot know what stage of training 
	it is already at. 
	
	NOTE: If the voiceprint doesn't exist you do not have to do this. 
	      An empty voiceprint is allocated in the Container Object and
	      is filled in for the firts time
	   
	Please check the proper use of the condition.
	*/
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Calling  VoiceID_set_voiceprint for pin (%s).", zPin);
	if((rc=VoiceID_set_voiceprint(pv_voiceprint, zPin, pv_VoiceID_conObj, errMsg)))
	{
			*zRetCode = rc;
			sprintf(zErrString, 
					"VoiceID_set_voiceprint() failed.  Unable to set voiceprint container [%d, %s]",
					rc, errMsg);
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
					zErrString);
			errMsg[0] = '\0'; 
			closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
			return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Successfully set voiceprint for pin (%s).", zPin);
	
#if 0
	if( (psh_buffer_16pcm = VIDPub_alloc_audio(errMsg)) == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString, "Unable to allocate audio buffer.  [%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
					zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		return(-1);
	}
#endif
	/***********************************************************************/
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"Calling arcsv_load_audio_buffer with pin (%s).", zPin);

    rc = arcsv_load_audio_buffer(zPin, zFile, "enroll", &audio_buffer, &size, 
						&ul_NumSamplesRead, gPcmFile, errMsg);
    if(rc < 0)
    {
		*zRetCode = -1;
		sprintf(zErrString, 
        		"Failed to load audio buffer into memory. [%d, %s]", rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
					zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
        return (-1);
    }

	if( (psh_buffer_16pcm = (short *) calloc(sizeof(short)*ul_NumSamplesRead+10, sizeof(short)))  == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString, "Unable to allocate audio buffer.  [%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
					zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		return(-1);
	}
	memcpy(psh_buffer_16pcm, audio_buffer, size);

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"ul_NumSamplesRead = %d, size=%d sizeof(short)=%d",
			ul_NumSamplesRead, size, sizeof(short));

  /* Process speech data to create or update extended voiceprint. 
		 An error will be raised if the application supplies a voiceprint
		 that is allready fully trained on 3 valid phrases. 
		 When this function is called for the first time for a particular
		 user, it will create an undertrained voiceprint (unless it
		 raises an error for a different reason); the second time it will train the voiceprint
		 further; the third time it will complete training.
		 The 3 training conditions are internally coded into the voiceprint
		 and checked.  This allows training to be done at different dates,
		 and in later versions, will enable the voiceprint to be refreshed.
  */

	errMsg[0] = '\0';
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
			"Calling VoiceID_EnrollTrain() with ul_NumSamplesRead(%d) for pin (%s)",
			ul_NumSamplesRead, zPin);

	if ((rc=VoiceID_EnrollTrain(pv_VoiceID_conObj, psh_buffer_16pcm,
				ul_NumSamplesRead, zPin, &vp_ready, errMsg)))
	{
		*zRetCode = rc;
		sprintf(zErrString, 
			"VoiceID_EnrollTrain() failed for pin (%s).  [%d, %s]", zPin, rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
					zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);

		if ( arcavbConfig.Keep_Error_Files )
		{
			(void) rename(gPcmFile, gPcmErrorFile);
		}
		free(psh_buffer_16pcm);
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_ENROLLTRAIN_ERROR,
			"VoiceID_EnrollTrain() succeeded for pin (%s); vp_ready=%d [%s]",
			zPin, vp_ready, errMsg);

	*zVpReady = vp_ready;
	/* Finished with audio buffer.  Release it. */
	//VIDPub_release_audio(psh_buffer_16pcm);
	free(psh_buffer_16pcm);

	/* Get a copy of the new or updated extended voiceprint from the 
	container object so it can be stored in the database. */
	if((rc=VoiceID_get_voiceprint(pv_voiceprint, &ui_voiceprint_size,
							pv_VoiceID_conObj, errMsg)))
	{
		*zRetCode = rc;
		sprintf(zErrString, 
			"VoiceID_get_voiceprint() failed for pin (%s).  [%d, %s]",
			zPin, rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
					zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		if ( arcavbConfig.Keep_Error_Files )
		{
			(void) rename(gPcmFile, gPcmErrorFile);
		}
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_ENROLLTRAIN_ERROR,
		"VoiceID_get_voiceprint(%s) succeeded; size of voiceprint is %d.",
			zPin, ui_voiceprint_size);

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
			"gVPExistsInDB is %d; vp_ready=%d", gVPExistsInDB, vp_ready	);
	rc = updateVoiceprint(zPin, 				// message is logged in routine
		pv_voiceprint, ui_voiceprint_size, (int)vp_ready, dbMessage);

	/* Report status of voiceprint preparation. */
	if(vp_ready)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
			"Extended voiceprint for pin (%s) is complete. "
			"Attempting to perform individual threshold.", zPin);

		rc = ARCAVB_IndividualThreshold(zPin, &indv_threshold,  &vp_goodness,
					&retCode, errMsg);
		if ( rc == 0 )
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
				"Individual threshold for pin (%s) is successful. Threshold is now %f. "
				"It can now be used for verification.", zPin, indv_threshold);
		}
		else
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
				"Individual threshold for pin (%s) failed.  [%d, %s] "
				"Correct and perform ARCAVB_IndividualThreshold().",
				zPin, retCode, errMsg);
		}
	}
	else
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
			"Extended voiceprint for pin (%s) is updated.  Further training is required"
			" before it can be used for verification.", zPin);
	}

	closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);

	return(0);
} // END: ARCAVB_Enroll_Train()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int ARCAVB_VerifyPhrase(char *zPin, char *zFile, float *zThreshold, 
                        float *zConfidence, float *zScore, int *zRetCode,
                        char *zErrString)
{
	static char		mod[]="ARCAVB_VerifyPhrase";
	int				rc;
	void			*pv_VoiceID_conObj = NULL;
	void			*pv_voiceprint = NULL;
	short			*psh_buffer_16pcm;
	unsigned long	ul_NumSamplesRead;
	int				vp_ready;
	int				vp_utterances;
	float			indv_threshold;
	float			vp_goodness;
	unsigned		ui_voiceprint_size;
	char			*audio_buffer = NULL;
	long int		size;
	char			pcmFile[256];
	int				voiceprint_filesize;
	char			errMsg[2048];

	if ( arcavbConfig.Keep_Mulaw_Files == -1 )
	{
#ifndef AVB_STANDALONE
		gPort = GV_AppCallNum1;
#else
		gPort = 0;
#endif // AVB_STANDALONE
		readAVBCfgFile();
	}

	*zRetCode = 0;
	zErrString[0] = '\0';

	gVPExistsInDB = 1;		// assume record exists
    *zConfidence = 0.0;		// confidence is not used
	pv_voiceprint = VoiceID_allocate_voiceprint(zPin, &ui_voiceprint_size, errMsg);
	if(pv_voiceprint == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString,
		     "Unable to allocate voiceprint; unable to retrieve it.  [%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				zErrString);
		return(-1);
	}

	rc = getExistingVoiceprint(zPin, &pv_voiceprint, &ui_voiceprint_size,
							&vp_ready, &vp_utterances, &indv_threshold, &vp_goodness,&voiceprint_filesize);
	if ( rc != 0 )
	{
		*zRetCode = -1;
		sprintf(zErrString,
		     "Unable to retrieve existing voiceprint for pin (%s).  Cannot verify.", zPin);
		return(-1);			// message is logged in routine
	}
	
	/* Create VoiceID container object */
	if( (pv_VoiceID_conObj = VoiceID_initialize(errMsg)) == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString, "VoiceID_initialize failed:[%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				zErrString);

		if((rc=VoiceID_release_voiceprint(pv_voiceprint, errMsg)))
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
				"VoiceID_release_voiceprint() failed. [%d, %s]",
				rc, errMsg);
		}
		errMsg[0] = '\0'; 
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
		"VoiceID_initialize() succeeded [pin=(%s).", zPin);
	

	/********************* IMPORTANT *****************************/
	/* The voiceprint has to be copied into the container before training. */
	/* Otherwise VoiceID_EnrollTrain() cannot know what stage of training 
	   it is already at. 
	
	NOTE: If the voiceprint doesn't exist you do not have to do this. 
	      An empty voiceprint is allocated in the Container Object and
	      is filled in for the firts time
	   
	Please check the proper use of the updateVP condition.
	*/
    
//	if ( gVPExistsInDB == 1 )
//	{
		if((rc=VoiceID_set_voiceprint(pv_voiceprint, zPin, pv_VoiceID_conObj, errMsg)))
		{
			*zRetCode = rc;
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				"VoiceID_set_voiceprint() failed.  Unable to set voiceprint container [%d, %s]",
			rc, errMsg);
			errMsg[0] = '\0'; 
			closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
			return(-1);
		}
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				"VoiceID_set_voiceprint(%s) [update] succeeded.", zPin);
//	}
	
	if( (psh_buffer_16pcm = VIDPub_alloc_audio(errMsg)) == NULL)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				"Unable to allocate audio buffer.  [%s]", errMsg);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		return(-1);
	}
	/***********************************************************************/

    rc = arcsv_load_audio_buffer(zPin, zFile, "verify", &audio_buffer, &size, 
						&ul_NumSamplesRead, pcmFile, errMsg);
    if(rc < 0)
    {
		*zRetCode = -1;
		sprintf(zErrString, 
        		"Failed to load audio buffer into memory. [%d, %s]", rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
					zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
        return (-1);
    }
	memcpy(psh_buffer_16pcm, audio_buffer, size);

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"ul_NumSamplesRead = %d, size=%d sizeof(short)=%d",
			ul_NumSamplesRead, size, sizeof(short));

    if ( (rc=VoiceID_VerifyPhrase(zScore, &indv_threshold, pv_VoiceID_conObj,
				psh_buffer_16pcm, ul_NumSamplesRead, zPin, errMsg)) )
	{
		*zRetCode = rc;
		sprintf(zErrString, 
			"VoiceID_VerifyPhrase() failed for pin (%s).  [%d, %s]", zPin, rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_VERIFY_ERROR,
					zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		if ( arcavbConfig.Keep_Error_Files )
		{
			(void) rename(gPcmFile, gPcmErrorFile);
		}
		return(-1);
	}
	*zThreshold = indv_threshold;

    if(*zScore < *zThreshold)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"VoiceID_VerifyPhrase() succeeded with score=%f; threshold=%f - accepted.", *zScore, *zThreshold);
	}
	else
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"VoiceID_VerifyPhrase() succeeded with score=%f; threshold=%f - rejected.", *zScore, *zThreshold);
	}

	/* Finished with audio buffer.  Release it. */
	VIDPub_release_audio(psh_buffer_16pcm);

	closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);

	return(0);
} // END: ARCAVB_VerifyPhrase()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int ARCAVB_IndividualThreshold(char *zPin, float *indvThreshold, 
					float *vp_goodness, int *zRetCode, char *zErrString)
{
	static char		mod[]="ARCAV_IndividualThreshold";
	char			errMsg[512];
	int				rc;
	int				i;

	void			*pv_VoiceID_conObj = NULL;
	void			*pv_voiceprint = NULL;
	void			*pv_voiceprint_imposter = NULL;
	unsigned		ui_voiceprint_size;
	int				vp_ready;
	int				vp_utterances;
	float			indv_threshold;
	float			local_vp_goodness;

	long int		size;
	int				voiceprint_filesize;
	char			dbMessage[256];

    char			*p;
    char			*pStrTok = NULL;
    char			thePin[32];

	char			pinList[1024] = "";
	int				numRecs;

	float			threshold;
	double			significance = VoiceID_DEFAULT_SIGNIFICANCE;

	if ( arcavbConfig.Keep_Mulaw_Files == -1 )
	{
#ifndef AVB_STANDALONE
		gPort = GV_AppCallNum1;
#else
		gPort = 0;
#endif // AVB_STANDALONE
		readAVBCfgFile();
	}

	*zRetCode = 0;
	zErrString[0] = '\0';

	pv_voiceprint = VoiceID_allocate_voiceprint(zPin, &ui_voiceprint_size, errMsg);
	if(pv_voiceprint == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString,
		     "Unable to allocate voiceprint; unable to retrieve it.  [%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				zErrString);
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Succesfully allocated voiceprint memory - %d bytes", ui_voiceprint_size);

	if ((rc = getExistingVoiceprint(zPin, &pv_voiceprint, &ui_voiceprint_size,
					&vp_ready, &vp_utterances, &indv_threshold, &local_vp_goodness, &voiceprint_filesize)) == -1)
	{
		*zRetCode = -1;
		sprintf(zErrString,
            "Unable to retrieve voice print for pin (%s).  Cannot perform continuous verification.",
            zPin);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR, zErrString);
        (void)VoiceID_release_voiceprint(pv_voiceprint, errMsg);
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"getExistingVoiceprint returned %d for pin (%s) [pv_size=%d, vp_ready=%d]",
				rc, zPin, ui_voiceprint_size, vp_ready);

	/* Create VoiceID container object */
	if( (pv_VoiceID_conObj = VoiceID_initialize(errMsg)) == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString, "VoiceID_initialize failed:[%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				zErrString);

		if((rc=VoiceID_release_voiceprint(pv_voiceprint, errMsg)))
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
				"VoiceID_release_voiceprint() failed. [%d, %s]",
				rc, errMsg);
		}
		errMsg[0] = '\0'; 
		return(-1);
	}
	
	if((rc=VoiceID_set_voiceprint(pv_voiceprint, zPin, pv_VoiceID_conObj, errMsg)))
	{
		*zRetCode = rc;
		sprintf(zErrString, 
					"VoiceID_set_voiceprint() failed.  Unable to set voiceprint container [%d, %s]",
					rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR, zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Successfully set voiceprint for pin (%s).", zPin);

	if ((rc = getPins_from_file(pinList, &numRecs, errMsg)) != 0)
	{
		*zRetCode = rc;
		sprintf(zErrString, 
			"Unable to retrieve list of voiceprints.  Cannot perform "
			"individual thresholds for pin (%s). [%d, %s]", 
			zPin, rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
			zErrString);
		errMsg[0] = '\0'; 
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"pinList=(%s)", pinList);
	
    if ((p=(char *)strtok_r(pinList, ",", &pStrTok)) == (char *)NULL)
    {
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
        		"Unable to parse list (%s) to get ids; cannot perform "
                "individual thresholds.", pinList);
		return(-1);
    }
    sprintf(thePin, "%s", p);

	for (;;)
	{
		rc = getSpecialExistingVoiceprint(thePin, &pv_voiceprint,  
						&ui_voiceprint_size, &vp_ready,	&vp_utterances,
						&indv_threshold, &local_vp_goodness);  
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"getSpecialExistingVoiceprint returned %d for pin (%s) [pv_size=%d, vp_ready=%d]",
			rc, thePin, ui_voiceprint_size, vp_ready);

		if((rc=VoiceID_addPopulationMember(pv_VoiceID_conObj, pv_voiceprint, errMsg)))
		{
			*zRetCode = rc;
			sprintf(zErrString, 
   		         	"VoiceID_addPopulationMember() failed for pin (%s) [%d, %s].", zPin, rc, errMsg);
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR, zErrString);

			closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
			return(-1);
		}
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
   		        	"VoiceID_addPopulationMember() succeeded for pin (%s).", thePin);

		if ((p=strtok_r(NULL, ",", &pStrTok)) == NULL) 
		{
			break;
		}
		sprintf(thePin, "%s", p);
	}

	if((rc = VoiceID_computeIndThreshold(&threshold, pv_VoiceID_conObj,
									significance, errMsg)))
	{
		*zRetCode = rc;
		sprintf(zErrString, 
			"VoiceID_computeIndThreshold() failed for pin (%s).  [%d, %s]",
			zPin, rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR, zErrString);
		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		return(-1);
	}      
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"VoiceID_computeIndThreshold() succeeded for pin (%s); threshold=%f.",
			zPin, threshold);

	/* Get a copy of the new or updated extended voiceprint from the 
	container object so it can be stored in the database. */
	if((rc=VoiceID_get_voiceprint(pv_voiceprint, &ui_voiceprint_size,
												pv_VoiceID_conObj, errMsg)))
	{
		*zRetCode = rc;
		sprintf(zErrString, 
				"VoiceID_get_voiceprint() failed for pin (%s).  [%d, %s]", zPin, rc, errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR, zErrString);

		closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);
		errMsg[0] = '\0'; 
		return(-1);
	}

    vp_utterances = VoiceID_get_voiceprint_nUtterances(pv_voiceprint);
    *vp_goodness   = VoiceID_get_voiceprint_goodness(pv_voiceprint);

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"VoiceID_get_voiceprint(%s) succeeded; size of voiceprint is %d. [%d, %d, %f]",
		zPin, ui_voiceprint_size, vp_ready, vp_utterances, *vp_goodness);

	rc = arcavb_db_update_refXT0(
                zPin,                    // id
                "",                     // app_id
                "",                     // name
                (char *)pv_voiceprint,                 // refXT0
                ui_voiceprint_size,
                "no error",             // error message
                vp_ready,               // vp_ready
                vp_utterances,
                threshold,
                *vp_goodness,
                errMsg);
	*indvThreshold = threshold;

	if ( rc == 0 )
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Successfully updated record for pin (%s); threshold=(%f).", zPin, threshold);
	}
	else
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
			"Failed to update record for pin (%s); threshold=(%f). [%d, %s] ",
			zPin, threshold, rc, errMsg);
	}

	closeVoiceID(pv_VoiceID_conObj, pv_voiceprint);

	if ( pv_voiceprint_imposter )
	{
		errMsg[0] = '\0'; 
		if((rc=VoiceID_release_voiceprint(pv_voiceprint_imposter, errMsg)) != 0 )
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
					"VoiceID_release_voiceprint() failed.  [%d, %s]", rc, errMsg);
		}
		else
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
					"VoiceID_release_voiceprint() succeeded.  [%d, %s]", rc, errMsg);
		}
		errMsg[0] = '\0'; 
	}
	return(0);
} // END: ARCAVB_IndividualThresholds()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int ARCAVB_VerifyContinuousSetup(int zPort, char *zPin, float *zThreshold, float *zConfidence,
                        float *zScore, int *zRetCode, char *zErrString)
{
	static char		mod[]="ARCAVB_VerifyContinuousSetup";
	int				rc;
	void			*pv_voiceprint = NULL;
	unsigned		ui_voiceprint_size;
    char			absolutePathAndFileName[1024];
	int				resultFd;
	char			fileName[256];
	char			errMsg[2048];
	char			*tStamp;
	FILE			*fp;
	int				voiceprint_filesize;
	int				vp_ready;
	int				vp_utterances;
	float			indv_threshold;
	float			vp_goodness;
	int				requestSent;

	struct Msg_ContinuousSpeech	msg;
	struct Msg_AVBMsgToApp		msgToApp;
	struct MsgToApp				response;

	if ( arcavbConfig.Keep_Mulaw_Files == -1 )
	{
#ifndef AVB_STANDALONE
		gPort = GV_AppCallNum1;

#else
		gPort = 0;
#endif // AVB_STANDALONE
		readAVBCfgFile();
	}
	gVPExistsInDB = 1;		// assume record exists

	*zRetCode = 0;
	zErrString[0] = '\0';
	hasContinuousResultBeenCalled = 0;

#ifndef AVB_STANDALONE
//	sprintf(apiAndParms,apiAndParmsFormat, mod, zPin);
//	rc = apiInit(mod, ARCAVB_VERIFYCONTINUOUSSETUP, apiAndParms, 1, 0, 0);
//	if (rc != 0) HANDLE_RETURN(rc);
//
	memset(fileName, 0, sizeof(fileName));

	msg.opcode = DMOP_VERIFY_CONTINUOUS_SETUP;
//	msg.appCallNum = GV_AppCallNum1;
	msg.appCallNum = zPort;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;
	memset(msg.file, 0, sizeof(msg.file));
	sprintf(msg.pin, "%s", zPin);
	msg.Keep_PCM_Files = arcavbConfig.Keep_Mulaw_Files;

	pv_voiceprint = VoiceID_allocate_voiceprint(zPin, &ui_voiceprint_size, errMsg);
	if(pv_voiceprint == NULL)
	{
		*zRetCode = -1;
		sprintf(zErrString,
		     "Unable to allocate voiceprint; unable to retrieve it.  [%s]", errMsg);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				zErrString);
		return(-1);
	}

	if ((rc = getExistingVoiceprint(zPin, &pv_voiceprint, &ui_voiceprint_size,
								&vp_ready, &vp_utterances, &indv_threshold, &vp_goodness,
								&voiceprint_filesize)) == -1)
	{
		msg.ui_voiceprint_size = 0;
		*zRetCode = -1;
		sprintf(zErrString,
            "Unable to retrieve voice print for pin (%s)  Cannot perform continuous verification.",
            zPin);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR, zErrString);
			
        (void)VoiceID_release_voiceprint(pv_voiceprint, errMsg);
		return(-1);

	}
	else
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Existing voiceprint retrieved for pin (%s). "
			"ui_voiceprint_size=%d",
			zPin, ui_voiceprint_size);
		msg.ui_voiceprint_size = ui_voiceprint_size;

		/* Create a file and send it */
		tStamp = get_time();
		sprintf(fileName, "/tmp/%s.%d", tStamp, getpid());
		if((fp = fopen(fileName, "w")) == NULL)
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
					"Failed to open file (%s).  Unable to perform continuous speech.  [%d, %s]", 
								fileName, errno, strerror(errno));
        	(void)VoiceID_release_voiceprint(pv_voiceprint, errMsg);
			return(-1);
//			HANDLE_RETURN(-1);
		}

		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
				"Attempting to write.... ui_voiceprint_size=%d",
					ui_voiceprint_size);
		if((rc=fwrite(pv_voiceprint, sizeof(void), ui_voiceprint_size, fp)) <= 0)
		{
			*zRetCode = -1;
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
			    "Unable to write voiceprint to temporary file (%s). rc=%d. [%d, %s]",
				fileName, rc, errno, strerror(errno));
        	(void)VoiceID_release_voiceprint(pv_voiceprint, errMsg);
			fclose(fp);
			return(-1);
		}
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
			    "Succesfully wrote %d to (%s).", rc, fileName);
		fclose(fp);
        (void)VoiceID_release_voiceprint(pv_voiceprint, errMsg);
		sprintf(msg.file, "%s", fileName);
	}

	requestSent=0;
	while(1)
	{
		if(!requestSent)
		{
			rc = sendRequestToDynMgr(mod, &msg);
			if (rc == -1) HANDLE_RETURN(TEL_FAILURE);
			requestSent=1;
			rc = readResponseFromDynMgr(mod, 0, &response, 
								sizeof(response));
		}
		else
		{
			rc = readResponseFromDynMgr(mod, 0, &response, 
							sizeof(response));
       		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
		}
				
	
		memcpy(&msgToApp, &response, sizeof(struct MsgToApp));
		rc = examineDynMgrResponse(mod, &msg, &response);	
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"%d = examineDynMgrResponse(retcode=%d);", rc, response.returnCode);

		*zRetCode = msgToApp.vendorErrorCode;
		sprintf(zErrString, "%s", msgToApp.errMsg);

		switch(rc)
		{
			case DMOP_VERIFY_CONTINUOUS_SETUP:
				HANDLE_RETURN(response.returnCode);
				break;
	
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum, response.message);
				break;
	
			case DMOP_DISCONNECT:
				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
			default:
				/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		} /* switch rc */
	} /* while */
	HANDLE_RETURN(0);		
#else
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Currently, ARCAVB_VerifyContinuousSetup() is not available in a "
			"stand-a-lone version.");
	return(0);
#endif // AVB_STANDALONE

	return(0);

} // END: ARCAVB_VerifyContinuousSetup()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int ARCAVB_VerifyContinuousGetResults(int zPort, float *zThreshold,
		float *zConfidence, float *zScore)
{
	static char		mod[]="ARCAVB_VerifyContinuousGetResults";
	int				rc;

	int requestSent;
	int ret_code;

	struct Msg_GetVerifyContinuousResults msg;
	struct MsgToApp response;
	struct Msg_AVBMsgToApp	avbMsg;

	if ( arcavbConfig.Keep_Mulaw_Files == -1 )
	{
#ifndef AVB_STANDALONE
		gPort = GV_AppCallNum1;
#else
		gPort = 0;
#endif // AVB_STANDALONE
		readAVBCfgFile();
	}

	if ( hasContinuousResultBeenCalled )
	{
		temp_rtpSleep(30);	// We don't to want to have the app slam the 
			                // mediamgr with continuous requests; we need a very
			                // small sleep time between requests.
	}
	hasContinuousResultBeenCalled = 1;

#ifndef AVB_STANDALONE
	if(GV_IsCallDisconnected1 == 1) 
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
			"Party 1 is disconnected.");
		HANDLE_RETURN(TEL_DISCONNECTED);
	}

	msg.opcode = DMOP_VERIFY_CONTINUOUS_GET_RESULTS;
//	msg.appCallNum = GV_AppCallNum1;
	msg.appCallNum = zPort;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;

	requestSent=0;
	while(1)
	{
		if(!requestSent)
		{
			ret_code = readResponseFromDynMgr(mod, -1, &response, 
							sizeof(response));
			if (ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
			if (ret_code == -2)
			{
				ret_code = sendRequestToDynMgr(mod, &msg);
				if (ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
				requestSent=1;
				ret_code = readResponseFromDynMgr(mod, 0, &response, 
								sizeof(response));
				if(ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
			}
		}
		else
		{
			ret_code = readResponseFromDynMgr(mod, 0, &response, 
							sizeof(response));
       		if(ret_code == -1) HANDLE_RETURN(TEL_FAILURE);
		}
			
		ret_code = examineDynMgrResponse(mod, &msg, &response);	
		switch(ret_code)
		{
			case DMOP_VERIFY_CONTINUOUS_GET_RESULTS:
				if(response.returnCode == 0)
				{
					memcpy(&avbMsg, &response, sizeof(struct MsgToApp));
					*zThreshold = avbMsg.indThreshold;
					*zConfidence = avbMsg.confidence;
					*zScore = avbMsg.score;

					ARCAVB_Log(__FILE__, __LINE__, mod,
											REPORT_VERBOSE, AVB_BASE,
							"avbMsg.returnCode = %d" 
			                "msg_avbMsgToApp.score=%f; "
           			    	"msg_avbMsgToApp.indThreshold=%f; "
               				"msg_avbMsgToApp.confidence=%f; --  "
							"zT=%f, zC=%f, zS=%f",
							avbMsg.returnCode,
			                avbMsg.score,
           			    	avbMsg.indThreshold,
               				avbMsg.confidence,
							*zThreshold,
							*zConfidence,
							*zScore);

					HANDLE_RETURN(avbMsg.returnCode);
				}
				else
				if(response.returnCode == -50)
				{
					memcpy(&avbMsg, &response, sizeof(struct MsgToApp));
					*zThreshold = avbMsg.indThreshold;
					*zConfidence = avbMsg.confidence;
					*zScore = avbMsg.score;

					ARCAVB_Log(__FILE__, __LINE__, mod,
											REPORT_VERBOSE, AVB_BASE,
							"avbMsg.returnCode = %d" 
			                "msg_avbMsgToApp.score=%f; "
           			    	"msg_avbMsgToApp.indThreshold=%f; "
               				"msg_avbMsgToApp.confidence=%f; --  "
							"zT=%f, zC=%f, zS=%f",
							avbMsg.returnCode,
			                avbMsg.score,
           			    	avbMsg.indThreshold,
               				avbMsg.confidence,
							*zThreshold,
							*zConfidence,
							*zScore);
				}
				HANDLE_RETURN(response.returnCode);
				break;
	
			case DMOP_GETDTMF:
				saveTypeAheadData(mod, response.appCallNum, response.message);
				break;
	
			case DMOP_DISCONNECT:
				HANDLE_RETURN(disc(mod, response.appCallNum));
				break;
	
			default:
				/* Unexpected message. Logged in examineDynMgr... */
				continue;
				break;
		} /* switch ret_code */
	} /* while */
	HANDLE_RETURN(0);		
#else
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Currently, ARCAVB_VerifyContinuousGetResults() is not available in a "
			"stand-a-lone version.");
	return(0);
#endif // AVB_STANDALONE

} // END: ARCAVB_VerifyContinuousGetResults()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int ARCAVB_GetVPInfo(char *zPin, int *vp_ready, int *vp_utterances,
                float *indv_threshold, float *vp_goodness,
                int *zRetCode, char *zErrString)
{
	static char		mod[]="ARCAVB_GetVPInfo";
	int				rc;
    void            *pv_voiceprint = NULL;
	unsigned		ui_voiceprint_size;
	int				fileSize;
	char			tmpPin[32] = "";

	*zRetCode = 0;
	zErrString[0] = '\0';

	if ( arcavbConfig.Keep_Mulaw_Files == -1 )
	{
#ifndef AVB_STANDALONE
		gPort = GV_AppCallNum1;
#else
		gPort = 0;
#endif // AVB_STANDALONE
		readAVBCfgFile();
	}

	pv_voiceprint = (void*)malloc(2097152);
	if (strncmp(zPin, "SPECIAL:", 8) == 0 )
	{
		sprintf(tmpPin, "%s", (zPin+8));
		rc = getSpecialExistingVoiceprint(tmpPin, &pv_voiceprint,  
						&ui_voiceprint_size, vp_ready,	vp_utterances,
						indv_threshold, vp_goodness);  
	}
	else
	{
		sprintf(tmpPin, "%s", zPin);
		rc = getExistingVoiceprint(tmpPin, &pv_voiceprint,
						&ui_voiceprint_size, vp_ready,	vp_utterances,
						indv_threshold, vp_goodness, &fileSize);  
	}

	free(pv_voiceprint);
	if ( rc != 0 )
	{
		*zRetCode = -1;
		sprintf(zErrString,
            "Unable to retrieve voice print for pin (%s)  Cannot retrieve information.",
            tmpPin);
		return(rc);		// message is logged in routine
	}

	return(0);
} // END: ARCAVB_GetVPInfo()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static void closeVoiceID(void *pv_VoiceID_conObj, void *pv_voiceprint)
{
	static char	mod[]="closeVoiceID";
	int        	rc;
	char		errMsg[2048];

	if ( pv_VoiceID_conObj ) 
	{
		if((rc=VoiceID_close(pv_VoiceID_conObj, errMsg)) != 0 )
		{	
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
					"VoiceIDClose() failed.  [%d, %s]", rc, errMsg);
		}
		else
		{	
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
					"VoiceIDClose() succeeded.  [%d, ]", rc);
		}
		errMsg[0] = '\0'; 
	}
	
	if ( pv_voiceprint )
	{
		errMsg[0] = '\0'; 
		if((rc=VoiceID_release_voiceprint(pv_voiceprint, errMsg)) != 0 )
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
					"VoiceID_release_voiceprint() failed.  [%d, %s]", rc, errMsg);
		}
		else
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
					"VoiceID_release_voiceprint() succeeded.  [%d, ]", rc);
		}
		errMsg[0] = '\0'; 
	}
	
} // END: closeVoiceID()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
void ARCAVB_Log(char *zFile, int zLine, char *zMod, int zMode, int zMsgId,
												char *messageFormat, ...)
{
    va_list     ap;
    char        message[1024];
    char        message2[1024];
	char		chan[8];

    va_start(ap, messageFormat);
    vsprintf(message, messageFormat, ap);
    va_end(ap);

	sprintf(chan, "%d", gPort);
	sprintf(message2, "[%s, %d] %s", zFile, zLine, message);
#ifdef AVB_STANDALONE
//	fprintf(stderr, "%s\n", message2);
#else
	LogARCMsg(zMod, zMode, chan, "SRC", GV_AppName, zMsgId, message2);
#endif // AVB_STANDALONE


} // END: ARCAVB_Log() 


/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int readAVBCfgFile() 
{
	char		mod[]="readAVBCfgFile";
	char		yConfigFile[256];
	char		*ispbase;
	FILE		*pFp;
	int			i;
	int			rc;
	char		yTmpBuf[128];
	char		errMsg[2048];
	char		yServiceStr[8];

	// set defaults
	memset((ArcAvbConfig *)&arcavbConfig, '\0', sizeof(arcavbConfig));
	arcavbConfig.Keep_Mulaw_Files = 0;
	arcavbConfig.Keep_PCM_Files = 0;
	arcavbConfig.Keep_Error_Files = 0;
	arcavbConfig.Continuous_Speech_Size = 16384; 
	arcavbConfig.Continuous_Speech_Sleep = 2000; // 2 seconds

	/* Get config file name */
	if((ispbase = (char *)getenv("ISPBASE")) == NULL)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				"Env. var. ISPBASE is not set.  Unable to continue.");
		sprintf(yConfigFile, "./arcavb.cfg", ispbase);
		
	}
	sprintf(yConfigFile, "%s/Telecom/Tables/arcavb.cfg", ispbase);

	if ((pFp = fopen(yConfigFile,"r")) == NULL )
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				"Failed to open config file (%s) for reading, [%d, %s].",
				yConfigFile, errno, strerror(errno));
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
				"Successfully opened config file (%s) for reading.",
				yConfigFile);
	
#ifdef TESTONLY
	sprintf(arcavbConfig.Base_Directory, "%s", ".");
#else
	rc = avbGetProfileString("", "Base_Directory", "",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	sprintf(arcavbConfig.Base_Directory, "%s", yTmpBuf);
#endif
	if ( arcavbConfig.Base_Directory[strlen(arcavbConfig.Base_Directory) - 1] == '/' )
	{
		arcavbConfig.Base_Directory[strlen(arcavbConfig.Base_Directory) - 1] == '\0';
	}

	rc = avbGetProfileString("", "Keep_PCM_Files", "",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	i = atoi(yTmpBuf);
	if ( i == 1 ) 
	{
		arcavbConfig.Keep_PCM_Files = 1;
	}
	else if ( i != 0 ) 
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_INITIALIZATION_ERROR,
			"Warning: Invalid value (%s) for Keep_PCM_Files; must be 0 or 1.  "
			"Setting to default of %d.", arcavbConfig.Keep_PCM_Files);
	}

	rc = avbGetProfileString("", "Keep_Mulaw_Files", "",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	i = atoi(yTmpBuf);
	if ( i == 1 ) 
	{
		arcavbConfig.Keep_Mulaw_Files = 1;
	}
	else if ( i != 0 ) 
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_INITIALIZATION_ERROR,
			"Warning: Invalid value (%s) for Keep_Mulaw_Files; must be 0 or 1.  "
			"Setting to default of %d.", arcavbConfig.Keep_Mulaw_Files);
	}

	rc = avbGetProfileString("", "Keep_Error_Files", "",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	i = atoi(yTmpBuf);
	if ( i == 1 ) 
	{
		arcavbConfig.Keep_Error_Files = 1;
	}
	else if ( i != 0 ) 
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_INITIALIZATION_ERROR,
			"Warning: Invalid value (%s) for Keep_Error_Files; must be 0 or 1.  "
			"Setting to default of %d.", arcavbConfig.Keep_Error_Files);
	}

	rc = avbGetProfileString("", "Continuous_Speech_Size", "",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	i = atoi(yTmpBuf);

	if ( i <= 0 )
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_INITIALIZATION_ERROR,
			"Warning: Invalid value (%d) for Continuous_Speech_Size.  Setting to default of %d.",
			i, arcavbConfig.Continuous_Speech_Size);
	}
	if ( i % 2 != 0 )
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_INITIALIZATION_ERROR,
			"Warning: Invalid value (%d) for Continuous_Speech_Size.  Must be divisible by 256.  "
			"Setting to default of %d.",
			i, arcavbConfig.Continuous_Speech_Size);
	}

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"Config=[%s, %d, %d, %d, %d, %d, %s, %s, %s, %s]",
		arcavbConfig.Base_Directory,
		arcavbConfig.Keep_Mulaw_Files,
		arcavbConfig.Keep_PCM_Files,
    	arcavbConfig.Keep_Error_Files,
		arcavbConfig.Continuous_Speech_Size,
		arcavbConfig.Continuous_Speech_Sleep,
		arcavbConfig.SpeechDir,
		arcavbConfig.MulawDir,
		arcavbConfig.ErrorDir,
		arcavbConfig.VpxDir);

	fclose(pFp);
	
	sprintf(arcavbConfig.SpeechDir, "%s/speech", arcavbConfig.Base_Directory);
	if ((rc = lValidateDirectory(arcavbConfig.SpeechDir, errMsg)) != 0)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
			"Unable to validate directory (%s).  Cannot continue.  [%s]",
				arcavbConfig.SpeechDir, errMsg);
		errMsg[0] = '\0'; 
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"Successfully validated speech directory (%s)", arcavbConfig.SpeechDir);
	
	sprintf(arcavbConfig.MulawDir, "%s/mulaw", arcavbConfig.Base_Directory);
	if ((rc = lValidateDirectory(arcavbConfig.MulawDir, errMsg)) != 0)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
			"Unable to validate directory (%s).  Cannot continue.  [%s]",
				arcavbConfig.MulawDir, errMsg);
		errMsg[0] = '\0'; 
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"Successfully validated mulaw directory (%s)", arcavbConfig.MulawDir);
	
	sprintf(arcavbConfig.VpxDir, "%s/vpX", arcavbConfig.Base_Directory);
	if ((rc = lValidateDirectory(arcavbConfig.VpxDir, errMsg)) != 0)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
			"Unable to validate directory (%s).  Cannot continue.  [%s]",
				arcavbConfig.VpxDir, errMsg);
		errMsg[0] = '\0'; 
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"Successfully validated vpX directory (%s)", arcavbConfig.VpxDir);
	
	sprintf(arcavbConfig.ErrorDir, "%s/errors", arcavbConfig.Base_Directory);
	if ((rc = lValidateDirectory(arcavbConfig.ErrorDir, errMsg)) != 0)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
			"Unable to validate directory (%s).  Cannot continue.  [%s]",
				arcavbConfig.ErrorDir, errMsg);
		errMsg[0] = '\0'; 
		return(-1);
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"Successfully validated error directory (%s)", arcavbConfig.ErrorDir);
	
	return(0);
} // END: readAVBCfgFile

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int avbGetProfileString(char *section,
					char *key, char *defaultVal, char *value,
					long bufSize, FILE *fp, char *file)
{
	static char	mod[] = "sGetProfileString";
	char		line[1024];
	char		currentSection[80],lhs[512], rhs[512];
	short		found, inSection;
	char 		*ptr;
	int			rc;

	sprintf(value, "%s", defaultVal);

	if(key[0] == '\0')
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				"Error: No key received.  retruning failure.");
		return(-1);
	}
	if(bufSize <= 0)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INITIALIZATION_ERROR,
				"Error: Return bufSize (%ld) must be > 0", bufSize);
		return(-1);
	}

	rewind(fp);
	memset(line, 0, sizeof(line));
	inSection = 0;
	found = 0;
	while(fgets(line, sizeof(line)-1, fp) != NULL)
	{
		/*
		 * Strip \n from the line if it exists
		 */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		myTrimWS(line);

		memset(lhs, 0, sizeof(lhs));
		memset(rhs, 0, sizeof(rhs));

		rc = sscanf(line, "%[^=]=%s", lhs, rhs);

		if (rc < 2)
		{
			if ((ptr = (char *)strchr(line, '=')) == (char *)NULL)
			{
				memset(line, 0, sizeof(line));
				continue;
			}
		}

		myTrimWS(lhs);
		myTrimWS(rhs);

		if(strcmp(lhs, key) != 0)
		{
			memset(line, 0, sizeof(line));
			continue;
		}
		found = 1;

		sprintf(value, "%.*s", bufSize, rhs);

		break;
	} /* while != NULL */

	if(!found)
	{
		return(-1);
	}

	return(0);
} /* END avbGetProfileString() */

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int getPins_from_file(char *zPinList, int *numRecords, char *err)
{
	static char	mod[] = "getPins_from_file";
	int 		rc;
	FILE		*fpPinsFile = NULL;
	char		idList[2048];
	char		line[512];
	char		rhs[256] = "";
	char		lhs[128] = "";
	char		*ispbase;
	char		yConfigFile[256] = "";

	if((ispbase = (char *)getenv("ISPBASE")) == NULL)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
				"Env. var. ISPBASE is not set.  Unable to continue.");
		return(-1);
	}
	sprintf(yConfigFile, "%s/Telecom/Tables/arcavb_thresholds.cfg", ispbase);

    if(access(yConfigFile, R_OK) != 0)
    {   
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
        			"Unable to access config file (%s)", yConfigFile);
		return(-1);
    }
    
    fpPinsFile = fopen(yConfigFile, "rb");
    if(fpPinsFile == NULL)
    {   
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
        	"Failed to open pin list file (%s) [%d, %s]",
			yConfigFile, errno, strerror(errno));
		return(-1);
    }

	memset((char *)idList, '\0', sizeof(idList));
	memset((char *)line, '\0', sizeof(line));
    while(fgets(line, sizeof(line) -1, fpPinsFile))
    {
        int line_len = strlen(line);

        if(line[0] == '#') continue;
        if(line[0] == ';') continue;

        if (line[line_len - 1] == '\n')
        {
            line[line_len - 1] = '\0';
        }
		rc = removeWhiteSpace(line);

        memset (rhs, 0, sizeof (rhs));
        memset (lhs, 0, sizeof (lhs));

        sscanf (line, "%[^=]=%s", lhs, rhs);

		if(lhs[0] && rhs[0])
		{
			if(strcasecmp(lhs, "specialIDs") != 0)
			{
				ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
        				"Read unrecognized parameter name (%s) from file (%s).  "
						"Unable to process.  Correct and retry.", lhs, yConfigFile);
				fclose(fpPinsFile);
				return(-1);
			}

			while ( ( rhs[strlen(rhs) - 1] == ',' ) ||
			        ( rhs[strlen(rhs) - 1] == ';' ) )
			{
				 rhs[strlen(rhs) - 1] = '\0';
			}
			
        	// fprintf(stderr, "[%s, %d] rhs=(%s)\n", __FILE__, __LINE__, rhs);
			strcat(rhs, ",");
			strcat(idList, rhs);
		}

	}//while

	while ( ( idList[strlen(idList) - 1] == ',' ) ||
			        ( idList[strlen(idList) - 1] == ';' ) )
	{
		 idList[strlen(idList) - 1] = '\0';
	}

	fclose(fpPinsFile);
	sprintf(zPinList, "%s", idList);
	return(0);

} // END: getPins_from_file()

/*------------------------------------------------------------------------------
myTrimWS():
------------------------------------------------------------------------------*/
static void myTrimWS(char *zStr)
{
    char        *p1;
    char        *p2;

    p2 = (char *)calloc(strlen(zStr) + 1, sizeof(char));

    p1 = zStr;

    if ( isspace(*p1) )
    {
        while(isspace(*p1))
        {
            p1++;
        }
    }

    sprintf(p2, "%s", p1);
    p1 = &(p2[strlen(p2)-1]);
    while(isspace(*p1))
    {
        *p1 = '\0';
        p1--;
    }

    sprintf(zStr, "%s", p2);
    free(p2);

} // END: myTrimSW()

/*------------------------------------------------------------------------------
lValidateDirectory():
	Verifies if the directory passed in exists and is a valid directory.  
	If it does not exist, create it with the system call 'mkdir'.
------------------------------------------------------------------------------*/
static int lValidateDirectory(char *iDirectory, char *oErrorMsg)
{
	int		rc;
	mode_t		mode;
	struct stat	statBuf;
	static char	mod[]="lValidateDirectory";
	char		pathName2[512];
	int			i;
	int			l;

	(void)umask(OPEN_MASK);

	l = strlen(iDirectory);
	for (i=1; i <= l; i++)
	{
		if ( iDirectory[i] == '/' || iDirectory[i] == '\0' )
		{
			memset((char *)pathName2, '\0', sizeof(pathName2));
			strncpy(pathName2, iDirectory, i);
		}
		else
		{
			continue;
		}
				
		/*
		 * Does the directory exist
		 */
		if ((rc=access(pathName2, F_OK)) == 0)
		{
			/*
			 * Is it really directory
			 */
			if ( (rc=stat(pathName2, &statBuf)) != 0)
			{
				sprintf(oErrorMsg, "Could not stat (%s).  [%d, %s].  "
						"(%s) exists but unable to access it.",
						pathName2, errno, strerror(errno),
						pathName2);
				return(-1);
			}
	
			mode = statBuf.st_mode & S_IFMT;
	
			if ( mode != S_IFDIR)
			{
				sprintf(oErrorMsg, "Error: (%s) exists, but is not a "
					"directory.", pathName2);
				return(-1);
			}
			continue;
		}
		
		if ((rc=mkdir(pathName2, DIRECTORY_CREATION_PERMS)) != 0)
		{
			sprintf(oErrorMsg,
				"Error: Unable to create directory (%s).  [%d, %s]",
				pathName2, errno, strerror(errno));
			return(-1);
		}
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Successfully validated directory (%s)", iDirectory);

	return(0);
} /* END: lValidateDirectory() */

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static int readWavHeader(FILE *fdWav, my_header_wav *wavHeader, char *errorMsg)
{
	char		tmpBuf[32];

	/* first read in the ChunkID and ChunkSize fields */
	//bytesRead=fread((my_header_wav *)wavHeader, 1, 8, fdWav);
	fread((my_header_wav *)wavHeader, 1, 8, fdWav);

	/* Now read in the format, subchunk1_id, and subchunk1_size fields */
	//bytesRead=fread((my_header_wav *) &(wavHeader->format), 1, 12, fdWav);
	fread((my_header_wav *) &(wavHeader->format), 1, 12, fdWav);

	if (strncmp(wavHeader->format, "WAVE", 4) != 0)
	{
		sprintf(errorMsg, "Wave file is not a valid wav file.");
		return(-1);
	}

	/* Now read in the rest of the format chunk (16 bytes). */
	//bytesRead=fread((my_header_wav *) &(wavHeader->audio_format), 1, 16, fdWav);
	fread((my_header_wav *) &(wavHeader->audio_format), 1, 16, fdWav);
	if (wavHeader->subchunk1_size > 16) {
		/* Throw-away bytes */
		//bytesRead=fread((char *)tmpBuf, 1, wavHeader->subchunk1_size - 16, fdWav);
		fread((char *)tmpBuf, 1, wavHeader->subchunk1_size - 16, fdWav);
	}

	/* Now read the subchunk2_id and SubChunk2Size fields */
	//bytesRead=fread((my_header_wav *) &(wavHeader->subchunk2_id), 1, 8, fdWav);
	fread((my_header_wav *) &(wavHeader->subchunk2_id), 1, 8, fdWav);
	return(0);

} /* END: readWavHeader() */

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static short int MuLaw_Decode(char number)
{
   const uint16_t MULAW_BIAS = 33;
   uint8_t sign = 0, position = 0;
   int16_t decoded = 0;
   number = ~number;
   if (number & 0x80)
   {
      number &= ~(1 << 7);
      sign = -1;
   }
   position = ((number & 0xF0) >> 4) + 5;
   decoded = ((1 << position) | ((number & 0x0F) << (position - 4))
             | (1 << (position - 5))) - MULAW_BIAS;
   return (sign == 0) ? (decoded) : (-(decoded));
}

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int arcsv_load_audio_buffer(char *pin, char *infile, char *command,
		char **buffer, long int *size, unsigned long *numSamples,
		char *pcmFile, char *err_msg)
{
	static char	mod[]="arcsv_load_audio_buffer";
	int rc = -1;
	char file_to_load[256];
	FILE	*fd16Bit = NULL;
	int		call_num;
	int		i;

	call_num=0;
	if(!pin || !pin[0])
	{
		sprintf(err_msg, "%s", "Empty PIN.");
		return (-1);
	}

	if(!command || !command[0])
	{
		sprintf(err_msg, "%s", "Empty command.");
		return (-1);
	}
	
#ifdef AVB_STANDALONE
	sprintf(file_to_load, "%s/mulaw_%s_%s_%d.wav", arcavbConfig.MulawDir, pin, command, call_num);
#else
	i=call_num;
	for(;;)
	{
		sprintf(file_to_load, "%s/mulaw_%s_%s_%d.wav", arcavbConfig.MulawDir, pin, command, i);
		if (access(file_to_load, F_OK) != 0 )
		{
			break;
		}
		i++;
	}
#endif // AVB_STANDALONE

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
					"file_to_load=(%s)", file_to_load);

#ifdef AVB_STANDALONE
	sprintf(pcmFile, "%s/%s_%s_%d.wav", arcavbConfig.SpeechDir, pin, command, call_num);
#else
	i=call_num;
	for(;;)
	{
		sprintf(pcmFile, "%s/%s_%s_%d.wav", arcavbConfig.SpeechDir, pin, command, i);
		if (access(pcmFile, F_OK) != 0 )
		{
			break;
		}
		i++;
	}
#endif // AVB_STANDALONE
	if ( arcavbConfig.Keep_Error_Files )
	{
#ifdef AVB_STANDALONE
	sprintf(gPcmErrorFile, "%s/%s_%s_%d.wav", arcavbConfig.ErrorDir, pin, command, call_num);
#else
		for(;;)
		{
			sprintf(gPcmErrorFile, "%s/%s_%s_%d.wav", arcavbConfig.ErrorDir, pin, command, i);
			if (access(gPcmErrorFile, F_OK) != 0 )
			{
				break;
			}
			i++;
		}
#endif // AVB_STANDALONE
	}
	else
	{
		gPcmErrorFile[0] = '\0';
	}
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
					"errorFile=(%s)", gPcmErrorFile);

	if(access(infile, R_OK) != 0)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
				"File (%s) is not readable. Unable to enroll/train/verify. [%d, %s]",
				file_to_load, errno, strerror(errno));
		return (-1);
	}

	if ( arcavbConfig.Keep_Mulaw_Files )
	{
		if ( (rc = avbCopyFile(infile, file_to_load, err_msg)) != 0 )
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
				"Unable to save (%s) to (%s).  [%s]  Continuing.", infile, file_to_load, err_msg);
			err_msg[0] = '\0';
		}
	}

	rc = arcsv_load_audio_buffer_from_file(call_num, infile,
											pcmFile, buffer, size, numSamples, err_msg);
	return rc;
} // END: arcsv_load_audio_buffer()

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
static int arcsv_load_audio_buffer_from_file(int port, char *file_to_load,
		char *pcmFile, char **buffer, long int *size,
		unsigned long *numSamples, char *err_msg)
{
	static char mod[]="arcsv_load_audio_buffer_from_file";
	int rc = -1;
	struct stat yStat;
	FILE	*fp;
    short *buffer_16bit = NULL;
    char *buffer8bit_tmp  = NULL;
    int		i;
	FILE	*fd16Bit = NULL;

	if(access(file_to_load, R_OK) != 0)
	{
		sprintf(err_msg, "File %s not readable. [%d, %s]", file_to_load, errno, strerror(errno));
		return (-1);
	}

	rc = stat (file_to_load, &yStat);
	if(rc == -1)
	{
		sprintf(err_msg, "stat() failed for (%s). [%d, %s]", file_to_load, errno, strerror(errno));
		return (-1);
	}

	if ((fp = fopen(file_to_load, "r")) == (FILE *)NULL)
	{
		*buffer = NULL;
		sprintf(err_msg, "File %s not readable. [%d, %s]", file_to_load,
					 errno, strerror(errno));
		return(-1);
	}

	// header_p points to a header struct that contains the wave file metadata fields
	my_header_wav *wav_meta = (my_header_wav*)malloc(sizeof(my_header_wav));	

    if ((rc = readWavHeader(fp, wav_meta, err_msg)) != 0)
    {
        *buffer = NULL;
        return(-1);			// err_msg is populated in readWavHeader
    }

    buffer8bit_tmp = (char *) calloc(yStat.st_size + 1, sizeof(char));
    if(buffer8bit_tmp == NULL)
    {
        sprintf(err_msg, "Failed to allocate memory. [%d, %s]", errno, strerror(errno));
        return (-1);
    }
//	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
//					"8bit - calloc(%d, %d) sizeof()=%d, %d",
//					yStat.st_size + 1, sizeof(char), sizeof(buffer8bit_tmp), sizeof(*buffer8bit_tmp));
    *buffer = (char *)buffer8bit_tmp;

	rc = fread(buffer8bit_tmp, 1, yStat.st_size - sizeof(my_header_wav), fp);
	fclose(fp);

//	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
//					"%d - fread(, 1, %d, )", rc, yStat.st_size - sizeof(my_header_wav));
	*numSamples = yStat.st_size - sizeof(my_header_wav);

	if( wav_meta->byte_rate == 8000)
	{
		int					sizeToWrite;
    	struct arc_decoder_t adc;
	    struct arc_g711_parms_t parms;

		buffer_16bit = (short *) calloc(yStat.st_size + 1, sizeof(short));
		if(buffer_16bit == NULL)
		{
			sprintf(err_msg, "[%s, %d] Failed to allocate memory. [%d, %s]",
						__FILE__, __LINE__, errno, strerror(errno));
			return (-1);
		}
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
					"16bit - calloc(%d, %d) sizeof()=%d, %d",
					yStat.st_size + 1, sizeof(short), sizeof(buffer_16bit), sizeof(*buffer_16bit));

		ARC_G711_PARMS_INIT (parms, 1);
	    arc_decoder_init (&adc, ARC_DECODE_G711, &parms, sizeof (parms), 1);

		sizeToWrite = yStat.st_size - sizeof(my_header_wav);
		rc = arc_decode_buff (&adc, buffer8bit_tmp, sizeToWrite, (char *)buffer_16bit, sizeToWrite*2);
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_DETAIL, AVB_BASE,
					"%d = arc_decode_buff(,,%d,,%d)", rc, sizeToWrite, sizeToWrite*2);

#if 0
		for (i=0; i<yStat.st_size; i++)
		{
			buffer_16bit[i] = MuLaw_Decode(buffer8bit_tmp[i]);
		}
		free(*buffer);
#endif
		*buffer = (char *)buffer_16bit;
		rc = yStat.st_size - sizeof(my_header_wav)*sizeof(short);

			//	memcpy(wav_meta->chunk_id, "RIFF", 4);		// - no need to change
			//  memcpy(wav_meta->format, "WAVE", 4);		// - no need to change
			//  memcpy(wav_meta->subchunk1_id, "fmt", 3);	// - no need to change
		wav_meta->subchunk1_size = 16;
		wav_meta->audio_format = 1;
		wav_meta->num_channels = 1;
			// wav_meta->sample_rate = ws8.SampleRate;		// - no need to change
		wav_meta->byte_rate = 16000;
		wav_meta->block_align = 2;
		wav_meta->bits_per_sample = 16;
		memcpy(&wav_meta->subchunk2_id, "data", 4);
		wav_meta->subchunk2_size = wav_meta->subchunk2_size * 2;
		wav_meta->chunk_size = wav_meta->subchunk2_size + 44 - 8;

		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_ENROLLTRAIN_ERROR,
				"arcavbConfig.Keep_PCM_Files=%d", arcavbConfig.Keep_PCM_Files);
		if ( arcavbConfig.Keep_PCM_Files )
		{
			if ((fd16Bit=fopen(pcmFile, "w")) == (FILE *)NULL)
			{
				ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_ENROLLTRAIN_ERROR,
					"Error opening wav file (%s) for input.  No conversion performed. "
					"[%d, %s]", pcmFile, errno, strerror(errno));
			}
			else
			{
				if ( (rc = fwrite(wav_meta, 1, sizeof(my_header_wav), fd16Bit) ) != sizeof(my_header_wav))
				{
#ifdef DEBUG_MSG
					ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_ENROLLTRAIN_ERROR,
						"Attempted to write (%ld) bytes to (%s).  Failed.  rc=%d.  [%d, %s]",
						sizeof(my_header_wav), pcmFile, i, errno, strerror(errno));
#endif
				}
				else
				{
					if ( (rc = fwrite(buffer_16bit, sizeof(short),
							sizeToWrite, fd16Bit) ) != sizeToWrite)
					{
#ifdef DEBUG_MSG
						ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_ENROLLTRAIN_ERROR,
							"Failed.  rc=%d. [%d, %s]",
							rc, errno, strerror(errno));
#endif
						fclose(fd16Bit);
						return(-1);
					}
					rc = rc *  sizeof(short);
//					ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
//						"%d=fwrite(%d, %d)", rc, sizeof(short), sizeToWrite);
				}
				fclose(fd16Bit);
			}
		}
		arc_decoder_free (&adc);
	}

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
					"arcsv_load_audio_buffer: Loaded %d bytes.", rc);

	*size = rc;
	*numSamples = *size / sizeof(short);

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
					"pcmFile=(%s)  *size=%ld, *numSamples=%ld", pcmFile, *size, *numSamples);

	return (rc);
	
}//END: arcsv_load_audio_buffer_from_file()

/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/
static int getExistingVoiceprint(char *zPin, void **pv_voiceprint,
			unsigned *ui_voiceprint_size, int *vp_ready, int *vp_utterances,
			float *indv_threshold, float *vp_goodness, int *zFileSize)
{
	static char	mod[]="getExistingVoiceprint";
	FILE			*pF;
	char			vpxFullPath[256];
	char			errorMsg[256] = "";
	struct stat		yStat;
	int				rc;
	int				voiceprint_size;

	char			app_id[128] = "";
	char			name[128] = "";

	char			errMsg[2048];

	int				tmp_ready;
	int				tmp_utterances;
	float			tmp_indv_threshold;
	float			tmp_goodness;


#ifndef AVB_DATABASE
	sprintf(vpxFullPath, "%s/%s.vpX", arcavbConfig.VpxDir, zPin);

	if ( access(vpxFullPath, F_OK))
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
				"Voice print file (%s) does not exist. Unable to retrieve voice print.  "
				"[%d, %s]", vpxFullPath, errno, strerror(errno)); 
		return(-1);
	}

	rc = stat (vpxFullPath, &yStat);
	if(rc == -1)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
				"stat() failed for (%s). Unable to retrieve voice print.  [%d, %s]",
				vpxFullPath, errno, strerror(errno));
		return (-1);
	}
	*zFileSize = yStat.st_size;

	/* If updating a partly-trained extended voiceprint, read it in from file
	   in the stand-alone version.  (The VoiceID library function,
	   VoiceID_EnrollTrain(), called below, will determine the precise status of
	   training from information embedded in the voiceprint.)
	
	   IMPORTANT:  In production the voiceprint is fetched from the database. 
	   The code below must be replaced with code that reads from the database.
	*/
	/* Read voiceprint */
	if((pF = fopen(vpxFullPath, "rb")) == NULL)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
					"Error opening (%s) to read.  Unable to retrieve voice print. "
					"[%d, %s]", vpxFullPath, errno, strerror(errno));
		return(-1); 
	}

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Successfully opened (%s).  Attempting to read %d bytes from it.", vpxFullPath, *zFileSize);
	errno=0;
	if((rc=fread(*pv_voiceprint, 1, yStat.st_size,  pF)) != yStat.st_size)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
					"Failed to read voiceprint file (%s).  Unable to retrieve voice print. "
					"rc=%d. [%d, %s]", vpxFullPath, rc, errno, strerror(errno));
		errMsg[0] = '\0'; 
		fclose(pF);
		return(-1);
	}
	fclose(pF);
 	*ui_voiceprint_size = rc; 
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
				"Successfully read %d bytes from voice print file (%s).",
				*ui_voiceprint_size, vpxFullPath);
#else
	if ( (rc = arcavb_db_retrieve_ref(
                zPin,                    // id
                app_id,                     // app_id
                name,					// name
                (char **)pv_voiceprint,          // refXT0
				&voiceprint_size,
                vp_ready,
                vp_utterances,
                indv_threshold,
                vp_goodness,
                errorMsg)) == -1)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
				"Failed to retrieve voiceprint for pin (%s). [%s]",
				zPin, errorMsg);
		gVPExistsInDB = 0;
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
			"Set gVPExistsInDB to %d", gVPExistsInDB);
		return(-1);
	}
	*ui_voiceprint_size = voiceprint_size;
	gVPExistsInDB = 1;
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"arcavb_db_retrieve_ref(%s) returned "
		"[vp_size=%d, ready=%d, utterances=%d, threshold=%f, goodness=%f].  rc=%d", zPin,
		voiceprint_size, *vp_ready, *vp_utterances, *indv_threshold, *vp_goodness, rc);

#endif // AVB_DATABASE

    if ( rc != 0 )
    {       
        return(rc);     // message is logged in routine
    }
    tmp_ready = VoiceID_get_voiceprint_voiceprintReady(*pv_voiceprint);
    tmp_utterances = VoiceID_get_voiceprint_nUtterances(*pv_voiceprint);
    tmp_indv_threshold = VoiceID_get_voiceprint_indThreshold(*pv_voiceprint);
    tmp_goodness = VoiceID_get_voiceprint_goodness(*pv_voiceprint);

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"values from voiceprint(%s) returned "
		"[vp_size=%d, ready=%d, utterances=%d, threshold=%f, goodness=%f].  rc=%d", zPin,
		voiceprint_size, tmp_ready, tmp_utterances, tmp_indv_threshold, tmp_goodness, rc);

#if 0
	if (tmp_ready != *vp_ready) 
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_BASE,
			"WARNING - No match on pin (%s) for vp_ready [%d, %d]",
			zPin, tmp_ready, *vp_ready);
	}
		
	if (tmp_utterances != *vp_utterances) 
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_BASE,
			"WARNING - No match on pin (%s) for vp_utterances [%d, %d]",
			zPin, tmp_utterances, *vp_utterances);
	}
		
	if (tmp_indv_threshold != *indv_threshold) 
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_BASE,
			"WARNING - No match on pin (%s) for indv_threshold [%f, %f]",
			zPin, tmp_indv_threshold, *indv_threshold);
	}
		
	if (tmp_goodness != *vp_goodness) 
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_BASE,
			"WARNING - No match on pin (%s) for vp_goodness [%f, %f]",
			zPin, tmp_goodness, *vp_goodness);
	}
		
	if ( (tmp_ready != *vp_ready) ||
	     (tmp_utterances != *vp_utterances) ||
	     (tmp_indv_threshold != *indv_threshold) ||
	     (tmp_goodness != *vp_goodness) )
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_BASE,
			"WARNING: Database values for voiceprint (%s) does not match voiceprint values.",
			zPin);
	}
#endif
    *vp_ready = tmp_ready;
    *vp_utterances = tmp_utterances;
    *indv_threshold = tmp_indv_threshold;
    *vp_goodness = tmp_goodness;

	return(rc);
} // END: getExistingVoiceprint()

/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/
static int getSpecialExistingVoiceprint(char *zPin, void **pv_voiceprint,
			unsigned *ui_voiceprint_size, int *vp_ready, int *vp_utterances,
			float *indv_threshold, float *vp_goodness)
{
	static char	mod[]="getSpecialExistingVoiceprint";
	FILE			*pF;
	char			vpxFullPath[256];
	char			errorMsg[256] = "";
	int				rc;
	int				voiceprint_size;

	char			name[128] = "";

	int				tmp_ready;
	int				tmp_utterances;
	float			tmp_indv_threshold;
	float			tmp_goodness;

	if ( (rc = arcavb_db_special_retrieve_ref(
                zPin,                    // id
                name,					// name
                (char **)pv_voiceprint,          // refXT0
				&voiceprint_size,
                vp_ready,
                vp_utterances,
                indv_threshold,
                vp_goodness,
                errorMsg)) == -1)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_INDV_THRESHOLD_ERROR,
					"Failed to retrieve voiceprint for pin (%s). [%s]",
					zPin, errorMsg);
		return(-1);
	}
	*ui_voiceprint_size = voiceprint_size;
	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"arcavb_db_special_retrieve_ref(%s) returned "
		"[vp_size=%d, ready=%d, utterances=%d, threshold=%f, goodness=%f].  rc=%d", zPin,
		voiceprint_size, *vp_ready, *vp_utterances, *indv_threshold, *vp_goodness, rc);

    if ( rc != 0 )
    {       
        return(rc);
    }

    tmp_ready = VoiceID_get_voiceprint_voiceprintReady(*pv_voiceprint);
    tmp_utterances = VoiceID_get_voiceprint_nUtterances(*pv_voiceprint);
    tmp_indv_threshold = VoiceID_get_voiceprint_indThreshold(*pv_voiceprint);
    tmp_goodness = VoiceID_get_voiceprint_goodness(*pv_voiceprint);

	ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
		"values from voiceprint(%s) returned "
		"[vp_size=%d, ready=%d, utterances=%d, threshold=%f, goodness=%f].  rc=%d", zPin,
		voiceprint_size, tmp_ready, tmp_utterances, tmp_indv_threshold, tmp_goodness, rc);

    *vp_ready = tmp_ready;
    *vp_utterances = tmp_utterances;
    *indv_threshold = tmp_indv_threshold;
    *vp_goodness = tmp_goodness;

	return(rc);
} // END: getSpecialExistingVoiceprint()

/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/
static int updateVoiceprint(
	char *zPin,				// key
	void *pv_voiceprint,
	unsigned ui_voiceprint_size,
	int vp_ready,
	char *dbMessage)
{
	static char mod[] = "updateVoiceprint";
	int			rc;
	int			vp_utterances;
	float		indv_threshold;
	float		vp_goodness;
	char		*pStr;
	char		errorMsg[512]="";

#ifndef AVB_DATABASE
	rc = writeVpxFile(zPin, pv_voiceprint, ui_voiceprint_size);
	return(rc);
#endif
	vp_utterances = VoiceID_get_voiceprint_nUtterances(pv_voiceprint);
	indv_threshold = VoiceID_get_voiceprint_indThreshold(pv_voiceprint);
	vp_goodness = VoiceID_get_voiceprint_goodness(pv_voiceprint);

	if ( gVPExistsInDB == 0 )
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
			"Calling arcavb_db_does_record_exist(%s)", zPin);
#if 0
		if ( (rc = arcavb_db_does_record_exist(zPin, errorMsg)) == -1)
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
				"Failed to query database for pin (%s); unable to update voiceprint. [%s]",
				zPin, errorMsg);
			return(-1);
		}
#endif
		rc = 0;
		if ( rc == 0 )
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
				"Pin/id (%s) does not exist.", zPin);

			rc = arcavb_db_upload_ref(
				zPin,                    // id
				"",                     // app_id
				"",          // name
				(char *)pv_voiceprint,                 // refXT0
				ui_voiceprint_size,
				dbMessage,
				vp_ready,               // vp_ready
				vp_utterances,
				indv_threshold,
				vp_goodness,
				errorMsg);

				if ( rc == -1 )
				{
					ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
						"Failed to insert record/voiceprint in to database for pin (%s). [%s]",
						zPin, errorMsg);
					return(-1);
				}
				ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
						"Successfully inserted voiceprint for pin (%s) in to database. "
						"size=%d", zPin, ui_voiceprint_size );
				return(rc);
		}
		else
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
				"Pin/id (%s) does exist.", zPin);
			rc = arcavb_db_update_refXT0(
						zPin,                    // id
						"",                     // app_id
						"",          // name
						(char *)pv_voiceprint,                 // refXT0
						ui_voiceprint_size,
						dbMessage,
						vp_ready,               // vp_ready
						vp_utterances,
						indv_threshold,
						vp_goodness,
						errorMsg);
			if ( rc == -1 )
			{
				ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
						"Failed to update voiceprint in to database for pin (%s). [%s]",
						zPin, errorMsg);
				return(-1);
			}
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
						"Successfully updated voiceprint pin (%s) in the database.", zPin );
			return(rc);
		}
	}
	else
	{
		rc = arcavb_db_update_refXT0(
					zPin,                    // id
					"",                     // app_id
					"",          // name
					(char *)pv_voiceprint,                 // refXT0
					ui_voiceprint_size,
					dbMessage,
					vp_ready,               // vp_ready
					vp_utterances,
					indv_threshold,
					vp_goodness,
					errorMsg);
		if ( rc == -1 )
		{
			ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
					"Failed to update voiceprint in to database for pin (%s). [%s]",
					zPin, errorMsg);
			return(-1);
		}
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_VERBOSE, AVB_BASE,
			"Successfully updated voiceprint pin (%s) in the database with vp_ready=%d, size=%d.",
			zPin, vp_ready, ui_voiceprint_size);
		return(rc);
	}

} // END: updateVoiceprint()

//#ifndef AVB_DATABASE
/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/
static int writeVpxFile(char *zPin, void *zVoicePrint, unsigned zSize)
{
	static char	mod[]="writeVpxFile";
	FILE		*pF;
	char		vpxFullPath[256];

	sprintf(vpxFullPath, "%s/%s.vpX", arcavbConfig.VpxDir, zPin);
	if(((pF = fopen(vpxFullPath, "wb")) == NULL))
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
		    "Unable to open (%s) for output. Unable to save voiceprint to disk.  [%d, %s]",
			vpxFullPath, errno, strerror(errno));
		return(-1);
	}

	/* Save voiceprint. */
	if(fwrite(zVoicePrint, 1, zSize,  pF) != zSize)
	{
		ARCAVB_Log(__FILE__, __LINE__, mod, REPORT_NORMAL, AVB_IO_ERROR,
		    "Unable to write voiceprint to file (%s). [%d, %s]",
			vpxFullPath, errno, strerror(errno));
		fclose(pF);
		return(-1);
	}

	fclose(pF);

	return(0);
} // END: writeVpxFile()
//#endif // AVB_DATABASE

/*------------------------------------------------------------------------------
avbCopyFile(): Copy sourceFile to destFile.
------------------------------------------------------------------------------*/
static int avbCopyFile(char *zSource, char	*zDestination, char *zErrorMsg)
{
	struct stat	yStat;
	int			yFdSource;
	int			yFdDestination;
	int			yBytesRead;
	void		*yPtrBytes;

	/*
	** Clear all buffers
	*/
	*zErrorMsg = 0;

	/*
	** Validate parameter: source
	*/
	if(! zSource || ! *zSource)
	{
		sprintf(zErrorMsg, "No source file name specified.");

		return(-1);
	}

	/*
	** Validate parameter: destination
	*/
	if(! zDestination || ! *zDestination)
	{
		sprintf(zErrorMsg, "No destination file name specified.");

		return(-1);
	}

	/*
	** Open the source file for reading.
	*/
	if((yFdSource = open(zSource, O_RDONLY)) < 0)
	{
		sprintf(zErrorMsg,
				"Failed to open source file for reading, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				errno, strerror(errno), zSource, zDestination);

		return(-1);
	}

	/*
	** Open the destination file for writing.
	*/
	yFdDestination = open(zDestination, O_WRONLY|O_CREAT|O_TRUNC, 0777);
	if(yFdDestination < 0)
	{

		sprintf(zErrorMsg,
				"Failed to open destination file for writing, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				errno, strerror(errno), zSource, zDestination);

		close(yFdSource);

		return(-1);
	}

	/*
	** Get the file information
	*/
	if(fstat(yFdSource, &yStat) < 0)
	{
		sprintf(zErrorMsg,
				"Failed to stat source file, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				errno, strerror(errno), zSource, zDestination);

		close(yFdSource);
		close(yFdDestination);

		return(-1);
	}

	/*
	** Allocate the memory
	*/
	if((yPtrBytes = (void *)malloc(yStat.st_size)) == NULL)
	{
		sprintf(zErrorMsg,
				"Failed to malloc (%d) bytes for source file size, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				yStat.st_size, errno, strerror(errno), zSource, zDestination);

		close(yFdSource);
		close(yFdDestination);

		return(-1);
	}
			
	/*
	** Read the file
	*/
	yBytesRead = read(yFdSource, yPtrBytes, yStat.st_size);

	if(yBytesRead != yStat.st_size)
	{
		sprintf(zErrorMsg,
				"Failed to read (%d) bytes from source file, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				yStat.st_size, errno, strerror(errno), zSource, zDestination);

		free(yPtrBytes);

		close(yFdSource);
		close(yFdDestination);

		return(-1);
	}

	/*
	** Write the contents of the file to the destination
	*/
	if(write(yFdDestination, yPtrBytes, yStat.st_size) != yStat.st_size)
	{
		sprintf(zErrorMsg,
				"Failed to write (%d) of source to destination, [%d: %s] - "
				"source (%s) not copied to destination (%s).",
				yStat.st_size, errno, strerror(errno), zSource, zDestination);

		free(yPtrBytes);

		close(yFdSource);
		close(yFdDestination);

		return(-1);
	}

	/*
	** Close the file and free the buffer
	*/
	free(yPtrBytes);

	close(yFdSource);
	close(yFdDestination);
		
	return(0);

} /* END: avbCopyFile() */

/*------------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/
static int temp_rtpSleep(int millisec)
{

	struct timeval SleepTime;

	SleepTime.tv_sec = 0;
	SleepTime.tv_usec = (long)millisec * 1000;

	select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &SleepTime);

	return(0);
} // END: temp_rtpSleep()

/*--------------------------------------------------------------------
int removeWhiteSpace(): Remove all whitespace characters from sting.
--------------------------------------------------------------------*/
static int removeWhiteSpace(char *buf)
{
        char    *ptr;
        char    workBuf[1024];
        int     rc;
        int     i=0;

        memset(workBuf, 0, sizeof(workBuf));

        ptr=buf;

        while ( *ptr != '\0' )
        {
                if (isspace(*ptr))
                        ptr++;
                else
                        workBuf[i++]=*ptr++;
        }
        workBuf[i]=*ptr;

        sprintf(buf,"%s", workBuf);

        return(strlen(buf));
} // END: removeWhiteSpace()

#if 0
static int arc_popen(char *command, int zLine)
{
	FILE *fpin = popen(command, "r");
	char line[1024];
	int rc = -1;

	if(fpin)
	{
		rc = 0;

		while(fgets(line, 255, fpin) != NULL)
		{

					__FILE__, zLine, line);
		}//while

		fclose(fpin);

	}//if fpin

	return (rc);

}//arc_popen
#endif

#ifdef __cplusplus
}
#endif

