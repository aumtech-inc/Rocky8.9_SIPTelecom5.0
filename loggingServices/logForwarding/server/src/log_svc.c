/*-----------------------------------------------------------------------
Program:	log_svc.c
Author:		Dan Barto
Date:		3/14/97
Purpose:	Receive either a file or data from a client process, then
		write it to the ARC log file defined in
		$ISPBASE/Global/.Global.cfg.  This is started by the
		log_listen deamon.

Update:		05/01/97 sja	No code change.
Update:		05/13/97 djb	Receive file into temporary, then append
				to log file.
Update:		08/22/97 sja	memset buffer before ss_RecvData
Update:		10/23/97 djb	added debug code and uncommented #define DEBUG
Update:		10/24/97 djb	commented #define DEBUG
Update:		10/28/97 djb	changed strcmp(null) to strlen(DestFile)
Update:		10/30/97 djb	changed all logging to gaVarLog()
Update:		11/06/97 djb	added memsets
Update:		05/09/01 djb	added lValidateDirectory().
Update:		03/07/03 djb	changed mktemp to mkstemp
Update:	11/28/06 djb	Changed ss_RecvFile() to ss_NewRecvFile().
Update:	12/09/14 djb	Updated logging.
-----------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
// #include <rpc/des_crypt.h>

#include "ss.h"
#include "gaUtils.h"

#define LOG_SERVER_CLIENT
#include "log_defs.h"

static char	LogFile1[PATHSZ];
static char	LogFile2[PATHSZ];
static char	Dir1[PATHSZ];
static char	BaseDir[PATHSZ];
char		msgbuf[256];

#define MAXBUFF	512
#define	DIRECTORY_CREATION_PERMS	0755
#define	OPEN_MASK			0000

/* #define DEBUG */

void		Exit();
static int	lValidateDirectory(char *iDirectory, char *oErrorMsg);

main(argc, argv)
int	argc;
char	*argv[];
{
static char mod[]="main";
char	databuf[32];
char	databuf2[32];
void	RcvData();
void RcvFile(char *encChar);
void RcvFileSplit();
int	ret_code, i;
long	recvdBytes;
static char	gaLogDir[256];
int		rc;

if(argc == 2 && (strcmp(argv[1], "-v") == 0))
{
	fprintf(stdout, 
		"Aumtech's Logging Services (%s).\n"
		"Version 2.3.  Compiled on %s %s.\n", argv[0], __DATE__, __TIME__);
	exit(0);
}
sprintf(BaseDir,"%s",(char *)getenv("ISPBASE"));
sprintf(gaLogDir,"%s/LOG", BaseDir);

rc=gaSetGlobalString("$PROGRAM_NAME", argv[0]);
rc=gaSetGlobalString("$LOG_DIR", gaLogDir);
rc=gaSetGlobalString("$LOG_BASE", argv[0]);

gaVarLog(mod, 1, "Starting.");

memset((char *)LogFile1, 0, sizeof(LogFile1));
memset((char *)LogFile2, 0, sizeof(LogFile2));
GetLogPaths(LogFile1, LogFile2);

for (i=strlen(LogFile1)-1; i>0; i--)
	if (LogFile1[i] == '/')
		break;

sprintf(Dir1,"%.*s",i,LogFile1);
if ((ret_code=chdir(Dir1)) != 0)
	{
	gaVarLog(mod, 0,"Unable to chdir into %s. errno=%d", Dir1, errno);
	exit(1);
	}

if ((ret_code = ss_Init(argc, argv)) != ss_SUCCESS)
{
	gaVarLog(mod, 0,"ss_Init() failed.  rc=%d.", ret_code);
	Exit();
}
gaVarLog(mod, 1,"ss_Init() succeeded.  rc=%d.", ret_code);

memset(databuf, 0, sizeof(databuf));
if ((ret_code=ss_RecvData(60, -1, databuf, &recvdBytes)) != ss_SUCCESS)
{
	gaVarLog(mod, 0,"ss_RecvData() of transfer type failed.  rc=%d.", ret_code);
	Exit();
}
gaVarLog(mod, 1,"%s|%d|Successfully received transfer type (%s) from server.  "
            "XFER_FILE=(%s) XFER_DATA=(%s) )",
            __FILE__, __LINE__, databuf, XFER_FILE, XFER_DATA);

if (strcmp(databuf, XFER_FILE) == 0)
{
	memset(databuf2, 0, sizeof(databuf2));
	if ((ret_code=ss_RecvData(60, -1, databuf2, &recvdBytes)) != ss_SUCCESS)
	{
		gaVarLog(mod, 0,"ss_RecvData() of file type failed.  rc=%d.", ret_code);
		Exit();
	}
	gaVarLog(mod, 1,"%s|%d|Successfully received file type (%s) from server.  "
			"XFER_ENCR=%s XFER_NOENCR=%s",
            __FILE__, __LINE__, databuf2, XFER_ENCR, XFER_NOENCR);
	RcvFile(databuf2);
}
else if (strcmp(databuf, XFER_DATA) == 0)
{
	RcvData();
}

Exit();
}

/*--------------------------------------------------------------*/
/* Purpose: Call ss_RecvFile to receive a log file in append	*/
/*	    mode.						*/
/*--------------------------------------------------------------*/
void RcvFile(char *encChar)
{
	char	DestFile[PATHSZ];
	int	ret_code, done=0;
	char	TmpFile[256];
	char	TmpFileEnc[256];
	char	Template[128];
	void	AppendTmpFile(char *, char *);
	long	recvdBytes;
	static char	mod[]="RcvFile";
	char	TempDir[128];
	char	tmpBuf[128];
	char	ErrorMsg[256];
	int		fdTmp;
	int		rc;
	
	memset(TmpFile,     0, sizeof(TmpFile));
	memset(TmpFileEnc,  0, sizeof(TmpFileEnc));
	while (1)
	{
		memset(DestFile, 0, sizeof(DestFile));
		if ((ret_code=ss_RecvData(120,-1,DestFile,&recvdBytes)) != ss_SUCCESS)
		{
			gaVarLog(mod, 0, "%s|%d|ss_RecvData of filename failed. rc=%d",
								__FILE__, __LINE__,	ret_code);
			break;
		}
	
		if (strlen(DestFile) == 0)
			break;
	
		gaVarLog(mod, 1, "%s|%d|ss_RecvData of filename succeeded. destinationFile=(%s); "
					"recvdBytes=%d", __FILE__, __LINE__, DestFile, recvdBytes);
	
		ErrorMsg[0] = '\0';
		sprintf(TempDir, "%s/temp", BaseDir);
		if ((ret_code = lValidateDirectory(TempDir, ErrorMsg)) != 0)
		{
			gaVarLog(mod, 0, "%s|%d|ss_RecvFile of (%s) failed.  Unable "
				"to create directory (%s).  %s", __FILE__, __LINE__, DestFile,
				TempDir, ErrorMsg);
			break;
		}
	
		sprintf(Template,"%s/%s.XXXXXX",TempDir,DestFile);
	
		fdTmp = mkstemp(Template);
		close(fdTmp);
		sprintf(TmpFile, "%s", Template);
	
		if ((ret_code=ss_RecvFile(TmpFile, RF_OVERWRITE)) != ss_SUCCESS)
		{
			gaVarLog(mod, 0,"%s|%d|ss_RecvFile of (%s) failed. rc=%d",
								__FILE__, __LINE__, DestFile, ret_code);
			break;
		}
		gaVarLog(mod, 1, "%s|%d|ss_RecvFile of (%s) succeeded. rc=%d",
					__FILE__, __LINE__, TmpFile, ret_code);
	
		if ( strcmp(encChar, XFER_NOENCR) != 0 )
		{
			sprintf(TmpFileEnc, "%s.enc", TmpFile);
			rc = decryptFile(TmpFile, TmpFileEnc);
			if ( rc != 0 )
			{
				gaVarLog(mod, 1,"Unable to decrypt file (%s).  rc=%d.", TmpFile, rc);
				break;
			}
			gaVarLog(mod, 1,"%s|%d|Appending (%s) to (%s)",
				__FILE__, __LINE__, TmpFileEnc, DestFile);
			AppendTmpFile(DestFile, TmpFileEnc);
		}
		else
		{
			gaVarLog(mod, 1,"%s|%d|Appending (%s) to (%s)",
				__FILE__, __LINE__, TmpFileEnc, DestFile);
			AppendTmpFile(DestFile, TmpFile);
		}

		if ( access(TmpFile, F_OK) == 0 )
		{
			unlink(TmpFile);
		}
		if ( access(TmpFileEnc, F_OK) == 0 )
		{
			unlink(TmpFileEnc);
		}
	
		sprintf(tmpBuf, "%s", "1");
		if ((rc = ss_SendData(strlen(tmpBuf), tmpBuf)) != ss_SUCCESS)
		{
			gaVarLog(mod, 0,"%s|%d|Unable to send go-ahead data (%s) to client. rc=%d",
				__FILE__, __LINE__, tmpBuf, rc);
			if ( access(TmpFile, F_OK) == 0 )
			{
				unlink(TmpFile);
			}
			if ( access(TmpFileEnc, F_OK) == 0 )
			{
				unlink(TmpFileEnc);
			}
			break;
		}
		gaVarLog(mod, 1,"%s|%d|Successfully sent go-ahead data (%s) to client.",
			__FILE__, __LINE__, tmpBuf);
	}

	sprintf(tmpBuf, "%s", "0");
	if ((rc = ss_SendData(strlen(tmpBuf), tmpBuf)) != ss_SUCCESS)
	{
		gaVarLog(mod, 1, "%s|%d|Unable to send go-ahead data (%s) to client. rc=%d",
			__FILE__, __LINE__, tmpBuf, rc);
	}
	else
	{
		gaVarLog(mod, 1,"%s|%d|Successfully sent go-ahead data (%s) to client.",
						__FILE__, __LINE__, tmpBuf);
	}

	if ( access(TmpFile, F_OK) == 0 )
	{
		unlink(TmpFile);
	}
	if ( access(TmpFileEnc, F_OK) == 0 )
	{
		unlink(TmpFileEnc);
	}

} // END: RcvFile()

void  AppendTmpFile(char *DestFile, char *SourceFile)
/*--------------------------------------------------------------*/
/* Purpose: Write data, which is received via ss_RecvData, to	*/
/*	    the local log file.					*/
/*--------------------------------------------------------------*/
{
FILE	*input_fd, *output_fd;
char	inbuf[MAXBUFF];
int	ret_code;
static char	mod[]="AppendTmpFile";

if ((input_fd=fopen(SourceFile, "r")) == NULL)
{
	gaVarLog(mod, 0, "Unable to open %s.  %s not appended to %s. [%d, %s]",
			SourceFile, DestFile, errno, strerror(errno));
	return;
}
gaVarLog(mod, 1, "Successfully opened source file (%s).", SourceFile);

if ((output_fd=fopen(DestFile, "a+")) == NULL)
	{
	gaVarLog(mod, 0, "Unable to open %s.  %s not appended to %s. [%d, %s]",
			SourceFile, DestFile, errno, strerror(errno));
	return;
	}
gaVarLog(mod, 1, "Successfully opened destination file (%s).", DestFile);

while (fgets(inbuf, MAXBUFF, input_fd))
	{
	if ((ret_code=fputs(inbuf, output_fd)) != EOF)
		continue;
	else
		{
		ret_code=DelReadRecords(SourceFile, input_fd, inbuf);
		return;
		}
	}

// remove(SourceFile);
}



void RcvData()
/*--------------------------------------------------------------*/
/* Purpose: Write data, which is received via ss_RecvData, to	*/
/*	    the local log file.					*/
/*--------------------------------------------------------------*/
{
char	data[1024];
int	ret_code;
long	recvdBytes;
static char	mod[]="RcvData";

while(1)
	{
	memset(data,0,sizeof(data));
	ret_code = ss_RecvData(3600, -1, data, &recvdBytes);
	gaVarLog(mod, 1,"[%d] %d=ss_RecvData(numbytes=%d)", __LINE__, ret_code, recvdBytes);

	if(ret_code == -2)
		continue;

	else if(ret_code != 0)
		{
		gaVarLog(mod, 0,"ss_RecvData->%d",ret_code);
		break;
		}

	if ((ret_code = WriteLocal(LogFile1, data, LOCAL_LOG)) != 0)
		{
		gaVarLog(mod, 0,"Unable to write message data <%s>", data);
		break;
		}
		gaVarLog(mod, 0,"[%d] %d=WriteLocal(%s, ...)", __LINE__, ret_code, LogFile1);
	}

}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int decryptFile(char *decFile, char *outFile)
{
	static char	mod[]="decryptFile";
    char        sysBuf[512];
    int         rc;
	FILE		*fp;

    sprintf(sysBuf, "base64 -di %s >%s", decFile, outFile);
#if 0
    rc = system(sysBuf);
    if ((rc = system(sysBuf)) != 0)
    {
        gaVarLog(mod, 0, "%s|%d|system command (%s) failed:rc=%d.  Unable to decode.",
					__FILE__, __LINE__, sysBuf, rc);
        return(-1);
    }
    gaVarLog(mod, 1, "%s|%d|system command (%s) succeeded:rc=%d.",
				__FILE__, __LINE__, sysBuf, rc);
#endif

    if((fp = popen(sysBuf, "r")) != NULL)
    {
    	gaVarLog(mod, 1, "%s|%d|system command (%s) succeeded.  Decoding succeeded.",
			__FILE__, __LINE__, sysBuf);
    	(void) pclose(fp);
	}
    else
    {
        gaVarLog(mod, 0, "%s|%d|system command (%s) failed.  Unable to decode.",
					__FILE__, __LINE__, sysBuf);
    	(void) pclose(fp);
		return(-1);
    }

    return(0);

#if 0
	FILE	*decFp;
	FILE	*outFp;
	int		rc;
	int		bytesRead;
	int		bytesWritten;
	struct stat   sbuf;
	char		data[NUM_ENC_BYTES_TO_PROCESS + 1];

	static char	key[9]="37494723";
	unsigned	mode;

	int			remainderMod8;
	char		*dataP;
	int			i;
	int			j;
	size_t		tmpBytesRead;

	if ( (rc = stat(decFile, &sbuf)) == -1)
	{
		gaVarLog(mod, 0, "stat(%s) failed.  rc=%d.  [%d, %s]",
				decFile, rc, errno, strerror(errno));
		return(-1);
	}
		
	//fprintf(stderr, "2480397 =%d", sbuf.st_size);
	//if ( sbuf.st_size <= 0 )

	if ( sbuf.st_size <= 0 )
	{
		gaVarLog(mod, 0, "file (%s) has a size of %d bytes. Unable to transfer.",
				decFile, sbuf.st_size);
		return(-1);
	}

//		fprintf(stderr, "DJB - HERE");
//		return(0);

	if ((decFp = fopen(decFile, "r")) == NULL )
    {
		gaVarLog(mod, 0, "Unable to open file (%s) for reading. [%d, %s]",
				decFile, errno, strerror(errno));
		return(-1);
	}

	if ((outFp = fopen(outFile, "w")) == NULL )
    {
		gaVarLog(mod, 0, "Unable to open file (%s) for writing. [%d, %s]",
				outFile, errno, strerror(errno));
		return(-1);
	}

	des_setparity(key);
	mode = DES_DECRYPT | DES_SW;
	memset((char *)data, '\0', sizeof(data));
	remainderMod8 = -1;
	while ((bytesRead=fread(data, sizeof(char), NUM_ENC_BYTES_TO_PROCESS, decFp)) > 0)
	{
		rc =  ecb_crypt(key, data, bytesRead, mode);
		gaVarLog(mod, 1, "%s|%d|%d = ecb_crypt(%s, %d, DES_DECRYPT)",
				__FILE__, __LINE__, rc, key, bytesRead);

		i = bytesRead;

		if ( bytesRead < NUM_ENC_BYTES_TO_PROCESS )
		{
			while ( data[i] == '\0' )
			{
//				printf("DJB - %s|%d|data[%d]=0x%x,(%c)",
//						__FILE__, __LINE__, i, data[i], data[i]);
				bytesRead--;
				i--;

//				printf("DJB - %s|%d|bytesRead = %d",
//						__FILE__, __LINE__, bytesRead);
			}
//			printf("DJB - %s|%d|data[%d]=0x%x,(%c)",
//						__FILE__, __LINE__, i, data[i], data[i]);
			bytesRead++;
		}

  		if ((bytesWritten=fwrite(data, sizeof(char), bytesRead, outFp)) != bytesRead)
		{
			gaVarLog(mod, 0, "%s|%d|write() failed to write %d encryted bytes to (%s). "
				"Closing and returning failure.  [%d, %s]",
				__FILE__, __LINE__, bytesWritten, outFile, errno, strerror(errno));
			
			fclose(decFp);
			fclose(outFp);
			return(-1);
		}
		gaVarLog(mod, 1, "%s|%d|write() succeeded to write %d encryted bytes to (%s).",
				__FILE__, __LINE__, bytesWritten, decFile);

		if ( remainderMod8 != -1 )
		{
			break;
			dataP = &data[strlen(data) - remainderMod8];
  			if ((bytesWritten=fwrite(dataP, sizeof(char), remainderMod8, outFp)) != remainderMod8)
			{
				gaVarLog(mod, 0,
					"%s|%d|write() failed to write %d encryted bytes (%s) to (%s). rc=%d  [%d, %s] "
					"Closing and returning failure.",
					__FILE__, __LINE__, remainderMod8, dataP, outFile, bytesWritten, errno, strerror(errno));
				
				fclose(decFp);
				fclose(outFp);
				return(-1);
			}
			gaVarLog(mod, 1, "%s|%d|write() succeeded to write %d encryted bytes (%s) to (%s).",
					__FILE__, __LINE__, bytesWritten, dataP, outFile);
			
		}
		memset((char *)data, '\0', sizeof(data));
	}

	fclose(decFp);
	fclose(outFp);
	return(0);

#endif
} // END: decryptFile()

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

	(void)umask(OPEN_MASK);
	/*
	 * Does the directory exist
	 */
	if ((rc=access(iDirectory, F_OK)) == 0)
	{
		/*
		 * Is it really directory
		 */
		if ( (rc=stat(iDirectory, &statBuf)) != 0)
		{
			sprintf(oErrorMsg, "Could not stat (%s).  [%d, %s].  "
					"(%s) exists but unable to access it.",
					iDirectory, errno, strerror(errno),
					iDirectory);
			return(-1);
		}

	        mode = statBuf.st_mode & S_IFMT;

		if ( mode != S_IFDIR)
		{
			sprintf(oErrorMsg, "Error: (%s) exists, but is not a "
				"directory.", iDirectory);
			return(-1);
		}
		return(0);
	}

	if ((rc=mkdir(iDirectory, DIRECTORY_CREATION_PERMS)) != 0)
	{
		sprintf(oErrorMsg,
			"Error: Unable to create directory (%s).  [%d, %s]",
			iDirectory, errno, strerror(errno));
		return(-1);
	}

	return(0);
} /* END: lValidateDirectory() */

void Exit()
{
static char	mod[]="Exit";

gaVarLog(mod, 1, "%s|%d|Calling ss_Exit()...", __FILE__, __LINE__);
ss_Exit();
gaVarLog(mod, 1, "%s|%d|Exiting...", __FILE__, __LINE__);
exit(0);

}
