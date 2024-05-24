#ident	"@(#)TEL_TTSReleaseResource 97/08/13 2.2.0 Copyright 1997 Aumtech, Inc."
/*------------------------------------------------------------------------------
File:		TEL_TTSReleaseResource.c
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "Telecom.h"

#include "ttsStruct.h"


static	char	logBuf[256];
static	char	fifo_dir[128];
static	int	get_fifo_dir(char *);
static  char	fileName[30];
static 	int	counter = 0;

static char	ModuleName[] = "TEL_TTSReleaseResource";

extern int sendRequestToTTSClient (const ARC_TTS_REQUEST_SINGLE_DM * ttsRequest);

/* Static Function Prototypes */

static int 	makeResultFifo(char *resultFifo, int *fd);
extern int	errno;

int TEL_TTSReleaseResource()
{
	static char mod[]="TEL_TTSReleaseResource";
	char 	apiAndParms[MAX_LENGTH] = "";
	char	resultFifo[80];
    char	absolutePathAndFileName[1024];
	int		rc, resultFd;
	ARC_TTS_REQUEST_SINGLE_DM	ttsRequest;
	ARC_TTS_RESULT	ttsResult;
	struct Msg_Speak msg;

	sprintf(apiAndParms,"%s()", ModuleName);
	rc = apiInit(ModuleName, TEL_TTSRELEASERESOURCE, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);

	msg.opcode = DMOP_TTSRELEASERESOURCE;
	msg.appPid = GV_AppPid;
	msg.appRef = GV_AppRef++;
	msg.appPassword = GV_AppPassword;
	msg.list = 0;
	memset(msg.file, 0, sizeof(msg.file));

	memset(&ttsRequest, 0, sizeof(ARC_TTS_REQUEST_SINGLE_DM));

	sprintf(ttsRequest.port, 	"%d", GV_AppCallNum1);
	sprintf(ttsRequest.pid, 	"%d", getpid());
	sprintf(ttsRequest.app, 	"%s", "avTest");

	memcpy(&ttsRequest.speakMsgToDM, &msg, sizeof(struct Msg_Speak)); 

	makeResultFifo(resultFifo, &resultFd);
	sprintf(ttsRequest.resultFifo, 	"%s", resultFifo);

	sprintf(ttsRequest.fileName,    "%s", absolutePathAndFileName);
	sprintf(ttsRequest.speakMsgToDM.file, "%s", ttsRequest.fileName);
//	sprintf(msg.file, "%s", ttsRequest.fileName);

	if(sendRequestToTTSClient(&ttsRequest) < 0)
	{
		close(resultFd);
		unlink(resultFifo);
		if(strlen(fileName) > 0)
			unlink(fileName);
		HANDLE_RETURN(-1);		/* Message written in routine */
	}

	rc = getTTSResult(resultFd, resultFifo, 2, SPEAK_AND_DELETE, &ttsResult);

	if ( rc == DMOP_DISCONNECT )
	{
		HANDLE_RETURN(disc(mod, GV_AppCallNum1));
	}

	if( (rc < 0) || ( atoi(ttsResult.resultCode) < 0 ) )
	{
		close(resultFd);
		unlink(resultFifo);
		HANDLE_RETURN(rc);		/* Message written in routine */
	}

	HANDLE_RETURN(TEL_SUCCESS);

} /* END: TEL_SpeakTTS() */

/*------------------------------------------------------------------------------
makeResultFifo(): Make the FIFO on which the result is going to be written.
------------------------------------------------------------------------------*/
static int makeResultFifo(char *resultFifo, int *fd)
{
	sprintf(resultFifo, "/tmp/ttsResult.%d.fifo", GV_AppCallNum1);
	if(mkfifo(resultFifo, S_IFIFO|PERMS) < 0 && errno != EEXIST)
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_CREATE_FIFO, 
			GV_Err, TEL_CANT_CREATE_FIFO_MSG, resultFifo, 
			errno, strerror(errno));
		return(-1);
	}

	if((*fd = open(resultFifo, O_CREAT|O_RDWR)) < 0)
	{
		telVarLog(ModuleName ,REPORT_NORMAL, TEL_CANT_OPEN_FIFO, 
			GV_Err, TEL_CANT_OPEN_FIFO_MSG, resultFifo, 
			errno, strerror(errno));
		return(-1);
	}

	return(0);
} /* END: makeResultFifo() */
