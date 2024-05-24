/*
	gcc -o googleRecordCmdLine  googleRecordCmdLine.c
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#define GSR_STREAMING_REQUEST   1       // mm -> java client
#define GSR_RESPONSE            2       // java client -> mm
#define GSR_VXI_RECORD_REQUEST	6		// mm -> java client

static int googleRecordOnNomatch(int zCall, char *zUtterance, char *zTranslation);
static int checkV2GoogleResult(char *zResultFile, char *zTranslation);
int chopUtterance (int zCall, char *zFile, long startFrom);
int makeDuplicateWavFile (int zCall, char *zFile);

int main(int argc, char *argv[])
{
	int			rc;
	char		translation[128]="";
	int			utterance_location = 0;
	char		utterance_file[256];
	int			zCall = 10;
	
	if ( argc < 2 )
	{
		printf("Usage: %s <utterance file> [ <lead trim location> ]\n", argv[0]);
		exit(0);
	}

	sprintf(utterance_file, "%s", argv[1]);
	if ( argc == 3 )
	{
		utterance_location = atoi(argv[2]);
	}

	if ( utterance_location > ( 8192 + 4096 ) )			// BT-272 
	{
		if ( access ("/tmp/utteranceDebug.txt", F_OK) == 0 )
		{
			makeDuplicateWavFile (zCall, utterance_file);
		}
		chopUtterance (zCall, utterance_file, utterance_location - 8192); // 1 second prior
	}
	else
	{
		printf("[%d] Not trimming utterance file (%s) since utterance started < 1 second from start (%d).\n", __LINE__,
			utterance_file, utterance_location);
	}

	rc = googleRecordOnNomatch(zCall, argv[1], translation);

	printf("%d=googleRecordOnNomatch(%s, %s)\n",
			rc, argv[1], translation);

}

static int googleRecordOnNomatch(int zCall, char *zUtterance, char *zTranslation)
{
	typedef struct
	{ 
		int		opcode;
		int		mmid;
		int		telport;
		int		rectime;
		int		trailtime;
		char	data[128];
		char	resultFileName[64];
		char	options[64];
	} GSR_request;

	typedef struct
	{ 
		int		opcode;
		int		mmid;
		int		telport;
		int		udpport;
		int		other;
		char	data[128];
		char	resultFileName[64];
		char	options[64];
	} GSR_response;

	GSR_request		gsrRequest;
	GSR_response	gsrResponse;
	char			resultStr[256];
	int			rc;
	char			resultFile[128];
	char			triggerFile[128];

	static char		googleResponseFifo[128] = "";
	static int		googleResponseFifoFd = -1;
	static char		googleRequestFifo[128] = "/tmp/ArcGSRRequestFifo";
	static int		googleRequestFifoFd = -1;

    time_t      tm;
    struct tm   tmStruct;
    char        tmChar[16];

	*zTranslation = '\0';
	//
	// Send the the request struct
	//
	if(googleRequestFifoFd <= -1)
	{
		if ( (googleRequestFifoFd = open (googleRequestFifo, O_RDWR)) < 0 )
		{
			printf("[%d] ARCGS: Failed to open request fifo (%s). [%d, %s] "
					"Unable to communicate with Google SR Client.\n", __LINE__,
					googleRequestFifo, errno, strerror (errno));
			return(-1);
		}
	}
	printf("[%d] Successfully opened request fifo (%s) \n", __LINE__, googleRequestFifo);

	time(&tm);
	localtime_r (&tm, &tmStruct);
	strftime(tmChar, 10, "%H%M%S", &tmStruct);
#if 0
	sprintf(resultFile, "/tmp/googleResult.%d", zCall);
	if ( access( resultFile, F_OK ) )
	{
		unlink(resultFile);
	}
	sprintf(triggerFile, "/tmp/googleResult.trigger.%d", zCall);
	if ( access( triggerFile, F_OK ) )
	{
		unlink(triggerFile);
	}

	if ( access( zUtterance, F_OK ) )
	{
		printf("[%d] Utterance file (%s) does not exist [%d, %s].  Returning failure.\n", __LINE__, zUtterance, errno, strerror(errno));
		return(-1);
	}
#endif


	memset((GSR_request *)&gsrRequest, '\0', sizeof(gsrRequest));
	gsrRequest.opcode 	= 6;
	gsrRequest.mmid 	= 99;
	gsrRequest.telport = zCall;
	gsrRequest.rectime = 97;
	gsrRequest.trailtime = 96;
	sprintf(gsrRequest.data, "%s", zUtterance);
	sprintf(gsrRequest.resultFileName, "/tmp/googleResult.%d.%s", zCall, tmChar);
	sprintf(gsrRequest.options, "%s", "");

	rc = write (googleRequestFifoFd, &gsrRequest, sizeof (gsrRequest));
	
	printf("[%d] Sent %d bytes to (%s) for opcode=%d, file=(%s) resultFile(%s)\n", __LINE__,
		rc, googleRequestFifo, gsrRequest.opcode, gsrRequest.data, gsrRequest.resultFileName);
	
#if 0
	sprintf (googleResponseFifo, "/tmp/vxiGoogleResponse.%d", zCall);
	if ( (rc = access(googleResponseFifo, R_OK)) != 0 ) 
	{
		if (mknod (googleResponseFifo, S_IFIFO | 0666, 0) < 0 && errno != EEXIST)
		{
			printf("Failed to create response fifo (%s). [%d, %s] "
				"Unable to communicate with Google SR Client.\n",
					googleResponseFifo, errno, strerror (errno));
	
			close(googleResponseFifoFd);
			googleResponseFifoFd = -1;
	
			return (-1);
		}
	}
#endif

	rc = checkV2GoogleResult(gsrRequest.resultFileName, zTranslation);
	close(googleResponseFifoFd);
	return(rc);

} // END: googleRecordOnNomatch()

int checkV2GoogleResult(char *zResultFile, char *zTranslation)
{
	#define		TIME_TO_WAIT	60 // seconds
	int rc = 0;
	int removeFile = 1;
	int	status;
	time_t currentTime;
	time_t endTime;
	int gotResult;
	int			counter;
	char		tmpBuf[512];
	struct stat	stat_buf;

	char mod[] = "checkV2GoogleResult";

	*zTranslation= '\0';
	if((rc = access("/tmp/gsKeepAudio.txt", R_OK)) == 0)
	{
		removeFile = 0;
	}
	
	memset((char *)tmpBuf, '\0', sizeof(tmpBuf));
	char yStrGoogleResultFile[64];
	char yStrGoogleTriggerFile[64];

	char *triggerLine = NULL;
	size_t len = 0;
	ssize_t read;
	int triggerContent = 0;


	sprintf(yStrGoogleResultFile, "%s", zResultFile);
	sprintf(yStrGoogleTriggerFile, "%s.trigger", zResultFile);

    time(&currentTime);
	endTime = currentTime + TIME_TO_WAIT;

	while ( currentTime < endTime )
	{
		if(access(yStrGoogleTriggerFile, W_OK) > -1)
		{
			FILE *yGoogleTriggerFp = NULL;
			if((yGoogleTriggerFp = fopen(yStrGoogleTriggerFile, "r") )!= NULL)
			 {
				read = getline(&triggerLine, &len, yGoogleTriggerFp);	
	
				fclose(yGoogleTriggerFp);
	
				if(triggerLine)
				{
					triggerContent = atoi(triggerLine);
					free(triggerLine);
				}
			}
	
			printf( "Google trigger file contents (%d).\n", triggerContent);
	
			//  This means the final google result is expected. so wait for at least 10 sec
	
			counter = 1;
	
			while( (triggerContent == 0) &&
			       (currentTime < endTime) &&
				   (access(yStrGoogleResultFile, R_OK|W_OK)) < 0)
			{
				if ( counter++ % 15 == 0 )  // check for disconnect every 1.5  seconds
				{
#if 0
					if ((rc = TEL_PortOperation(GET_FUNCTIONAL_STATUS, GV_AppCallNum1, &status)) == TEL_DISCONNECTED )
					{
						printf("Got DISCONNECT waiting for google result file.\n");
						if ( ( removeFile == 1 ) &&
						     ( access (yStrGoogleTriggerFile, F_OK) == 0 ) )
				        {
							if ( (rc = unlink(yStrGoogleTriggerFile)) == 0 )
							{
								printf("Removed (%s)\n", yStrGoogleTriggerFile);
							}
							else
							{
								printf("Failed to unlink (%s). [%d, %s]\n", yStrGoogleTriggerFile, errno, strerror(errno));
							}
						}
						return( TEL_DISCONNECTED);
					}
#endif
					return(-3);
				}
	
				usleep(100000); // 0.1 seconds
				time(&currentTime);
			}
			break;
		}
		else
		{
			usleep(100000); // 0.1 seconds
			time(&currentTime);
		}
	}
	
	if ( ( removeFile == 1 ) &&
	     ( access (yStrGoogleTriggerFile, F_OK) == 0 ) )
	{
			if ( (rc = unlink(yStrGoogleTriggerFile)) == 0 )
			{
				printf("Removed (%s)\n", yStrGoogleTriggerFile);
			}
			else
			{
				printf(
					"Failed to unlink (%s). [%d, %s]\n", yStrGoogleTriggerFile, errno, strerror(errno));
			}
	}
	if ( (triggerContent == -2) ||
         (currentTime >= endTime) )
	{
		//return(TEL_TIMEOUT);
		return(-2);
	}

	if(access(yStrGoogleResultFile, R_OK|W_OK) > -1)
	{
		FILE *yGoogleFp = NULL;

		if((yGoogleFp = fopen(yStrGoogleResultFile, "r") )!= NULL)
		{
			int nBytes = 0;

			if(stat(yStrGoogleResultFile,  &stat_buf) < 0)
			{
				printf("Failed to stat result file (%s). [%d, %s] Returning failure.\n",
								yStrGoogleResultFile, errno, strerror(errno));
				fclose(yGoogleFp);
				if ( ( removeFile == 1 ) &&
	     		     ( access (yStrGoogleResultFile, F_OK) == 0 ) )
				{
					if ( (rc = unlink(yStrGoogleResultFile)) == 0 )
					{
						printf("Removed (%s)\n", yStrGoogleResultFile);
					}
					else
					{
						printf("Failed to unlink (%s). [%d, %s]\n", yStrGoogleResultFile, errno, strerror(errno));
					}
				}
			}
			if ( stat_buf.st_size <= 0 )
			{
				printf("No results returned.  Returning failure.\n");
			}
		
			nBytes = fread(tmpBuf, 1, stat_buf.st_size, yGoogleFp);
			sprintf(zTranslation, "%s\n", tmpBuf);
			fclose(yGoogleFp);
			
			printf("Google Result (%s)\n", zTranslation);
		}
		else
		{
			printf("Failed to open result file (%s). [%d, %s] Returning failure.\n",
							yStrGoogleResultFile, errno, strerror(errno));
			if ( ( removeFile == 1 ) &&
	     	     ( access (yStrGoogleResultFile, F_OK) == 0 ) )
			{
				if ( (rc = unlink(yStrGoogleResultFile)) == 0 )
				{
					printf("Removed (%s)\n", yStrGoogleResultFile);
				}
				else
				{
					printf("Failed to unlink (%s). [%d, %s]\n", yStrGoogleResultFile, errno, strerror(errno));
				}
			}
			return (-1);
		}

		if ( ( removeFile == 1 ) &&
     	     ( access (yStrGoogleResultFile, F_OK) == 0 ) )
		{
			if ( (rc = unlink(yStrGoogleResultFile)) == 0 )
			{
				printf("Removed (%s)\n", yStrGoogleResultFile);
			}
			else
			{
				printf("Failed to unlink (%s). [%d, %s]\n", yStrGoogleResultFile, errno, strerror(errno));
			}
		}
		return(0);
	}//end access yStrGoogleResult
	else
	{
		printf("Failed to acces result file (%s). [%d, %s] Returning failure.\n",
							yStrGoogleResultFile, errno, strerror(errno));
		if ( ( removeFile == 1 ) &&
     	     ( access (yStrGoogleResultFile, F_OK) == 0 ) )
		{
			if ( (rc = unlink(yStrGoogleResultFile)) == 0 )
			{
				printf("Removed (%s)", yStrGoogleResultFile);
			}
			else
			{
				printf( "Failed to unlink (%s). [%d, %s]\n", yStrGoogleResultFile, errno, strerror(errno));
			}
		}
		return (-1);
	}

} // static int checkV2GoogleResult(char *zTranslation)

int chopUtterance (int zCall, char *zFile, long startFrom)
{
	int             yRc;
	int             yIntFread = 0;
	int             yIntFwrite = 0;
	char            mod[] = { "chopUtterance" };

	char            yStrBackupFile[256];

	FILE           *fp_write = NULL;
	FILE           *fp_read = NULL;

	struct stat     yStat;

	char           *buf = NULL;
	size_t			yStartingPt;
	size_t			yContentToWrite;

	sprintf (yStrBackupFile, "%s.dupe", zFile);

	if ((yRc = stat (zFile, &yStat)) < 0)
	{
		printf("stat(%s) failed. [%d, %s]\n", zFile, errno, strerror(errno));
		return (-1);
	}
	printf("Chopping %s; size is currently %ld. Will trim off front %d bytes.\n", zFile, yStat.st_size, startFrom);

#if 0 
	fp_write = fopen (yStrBackupFile, "w");
	if (fp_write == NULL)
	{
		printf("fopen(%s) failed. [%d, %s]\n", yStrBackupFile, errno, strerror(errno));
		return (-1);
	}
#endif

	fp_read = fopen (zFile, "r");
	if (fp_read == NULL)
	{
		fclose (fp_write);
		printf("fopen(%s) failed. [%d, %s]\n", zFile, errno, strerror(errno));
		return (-1);
	}

	buf = (char *) malloc (yStat.st_size + 1);

	if (buf == NULL)
	{
		printf("failed to malloc. [%d, %s]\n", errno, strerror(errno));
		fclose (fp_write);
		fclose (fp_read);
		return (-1);
	}

	fseek( fp_read, startFrom, SEEK_SET );

	memset (buf, 0, yStat.st_size + 1);

	printf("[%d] DJB: yStat.st_size:%d  startFrom:%d\n", __LINE__, yStat.st_size, startFrom);
	if ( yStat.st_size - startFrom < 0 )
	{
		yStartingPt = 0;
	}
	else
	{
		yContentToWrite = yStat.st_size - startFrom;
	}
	printf("[%d] DJB: yContentToWrite:%d\n", __LINE__, yContentToWrite);
	if ((yIntFread = fread (buf, 1, yContentToWrite, fp_read)) < 0)
	{
		printf("fread(%s) failed. [%d, %s]\n", zFile, errno, strerror(errno));
		fclose (fp_write);
		fclose (fp_read);
		free (buf);
		return (-1);
	}
	fclose (fp_read);
	printf("read %d bytes. strlen(buf)=%d\n", yIntFread, strlen(buf));

	fp_write = fopen (zFile, "w");
	if (fp_write == NULL)
	{
		printf("fopen(%s) failed. [%d, %s]\n", zFile, errno, strerror(errno));
		return (-1);
	}

	if ((yIntFwrite =
		 fwrite (buf, 1, (int)yContentToWrite, fp_write)) < 0)
	{
		printf("fwrite(%s) failed. [%d, %s]\n", yStrBackupFile, errno, strerror(errno));
		fclose (fp_write);

		free (buf);

		return (-1);
	}
	printf("Wrote %d bytes to (%s).\n", yIntFwrite, zFile);

	fclose (fp_write);
	free (buf);

	if ((yRc = stat (zFile, &yStat)) < 0)
	{
		printf("stat(%s) failed. [%d, %s]\n", zFile, errno, strerror(errno));
		return (-1);
	}
	printf("Chopped %s; size is now %ld.\n", zFile, yStat.st_size);
	return (0);

}	/*END: chopUtterance() */

int makeDuplicateWavFile (int zCall, char *zFile)
{
	int             yRc;
	int             yIntFread = 0;
	int             yIntFwrite = 0;
	char            mod[] = { "makeDuplicateWavFile" };

	char            yStrBackupFile[256];

	FILE           *fp_write = NULL;
	FILE           *fp_read = NULL;
	char			*p;

	struct stat     yStat;

	char           *buf = NULL;

	sprintf (yStrBackupFile, "%s", zFile);

	if ((p=strstr (yStrBackupFile, ".wav")) == (char *)NULL)
	{
		sprintf (yStrBackupFile, "%s.before", zFile);
	}
	else
	{
		*p = '\0';
		strcat(yStrBackupFile, "_before.wav");
	}

	if ((yRc = stat (zFile, &yStat)) < 0)
	{
		printf("stat(%s) failed. [%d, %s] Unable to make backup of utterance file.\n", zFile, errno, strerror(errno));
		return (-1);
	}

	fp_write = fopen (yStrBackupFile, "w");
	if (fp_write == NULL)
	{
		printf("fopen(%s) failed. [%d, %s] Unable to make backup of utterance file.\n", yStrBackupFile, errno, strerror(errno));
		return (-1);
	}

	fp_read = fopen (zFile, "r");
	if (fp_read == NULL)
	{
		fclose (fp_write);
		printf("fopen(%s) failed. [%d, %s] Unable to make backup of utterance file.\n", zFile, errno, strerror(errno));
		return (-1);
	}

	buf = malloc (yStat.st_size + 1);

	if (buf == NULL)
	{
		printf("failed to malloc. [%d, %s] Unable to make backup of utterance file (%s).\n", errno, strerror(errno), zFile);
		fclose (fp_write);
		fclose (fp_read);
		return (-1);
	}

	memset (buf, 0, yStat.st_size + 1);

	if ((yIntFread = fread (buf, 1, yStat.st_size, fp_read)) < 0)
	{
		printf("fread(%s) failed. [%d, %s] Unable to make backup of utterance file.\n", zFile, errno, strerror(errno));
		fclose (fp_write);
		fclose (fp_read);
		free (buf);
		return (-1);
	}

	if ((yIntFwrite =
		 fwrite (buf, 1, (int) yStat.st_size, fp_write)) < 0)
	{
		printf("fwrite(%s) failed. [%d, %s] Unable to make backup of utterance file.\n", yStrBackupFile, errno, strerror(errno));
		fclose (fp_write);
		fclose (fp_read);

		free (buf);

		return (-1);
	}

	fclose (fp_write);
	fclose (fp_read);

	free (buf);

	printf("For debug, copied file utterance file (%s) to (%s)\n", zFile, yStrBackupFile);
	return (0);

}	/*END: makeDuplicateWavFile() */
