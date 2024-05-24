#ident	"@(#)TEL_SendFax 00/05/22 3.1   Copyright 2000 Aumtech"
static char mod[]="TEL_SendFax";
/*-----------------------------------------------------------------------------
Program:	TEL_SendFax.c
Purpose:	To send out a fax on the same line. 
Author: 	A. Kulkarni.
Date:		02/23/98	
Update:		12/26/02 apj Changes necessary for Single DM structure.
Update: 	02/22/2003 apj Added TEL_CALL_SUSPENDED code.
Update:     08/28/2003 ddn  Added TEL_CALL_RESUMED code
Update: 08/25/04 djb Changed sys_errlist strerror(errno).
Update: 	11/09/2004 apj Save DTMFs in the buffer.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "arcFAX.h"

#include <stdlib.h>
#include <wait.h>

extern unsigned short   gvWidth;
extern unsigned short   gvResolution;
// extern DF_ASCIIDATA gvAsciiData;
extern void getFileTypeString(int parmFileType, char *parmFileTypeString);

static char		*tmp_tiff_files[50];
static char 	tmp_text_data[256];
static int		numTextToTiffs = 0;

static char	logMsg[256];
// static int createSendFaxDataFile(char *mod, char *sendFaxData, char *file);
static int buildTiffFile(char *fax_data, char *outputTiffFile);
static int doTextToTiff(char *listOfFiles, char *tiffFile, char *errorMsg);
static int adjustFiles(char *zTextFile, char *zNewTextFile, char *zTiffFile);

/*------------------------------------------------------------------------------
TEL_SendFax()
	Main routine performs error checking and the internal sendFax function()
	which includes:
	- has TEL_InitTelecom() been called
	- are we still connected
	- speak the prompt phrase
	- calls getAFaxResource to get a fax resource. 
	- calls establishFaxRouting to do necessary routing.
	- ask DM to send a fax.
------------------------------------------------------------------------------*/
int TEL_SendFax(int file_type, char *fax_data, char *prompt_phrase)
{
	int	rc = 0;
	int	rc2;
	int		i;
	char 	apiAndParms[MAX_LENGTH];
	int	fpFaxFile;
	char	faxResourceName[128];
	char 	fileTypeString[20];
	char	faxResourceSlot[10];
	int	fax_handle;
	struct MsgToApp response;
	struct Msg_SendFax lMsg;
	struct MsgToDM msg;
	int breakOutOfWhile = 0;
	int		isThisAListFile;
	struct stat yStat;
	char		tmp_fax_data[256] = "";
	char		tmpTiffFile[256];
	char		errMsg[256];

	memset(fileTypeString, 0, sizeof(fileTypeString));
	memset(tmpTiffFile, '\0', sizeof(tmpTiffFile));

	getFileTypeString(file_type, fileTypeString);

	sprintf(apiAndParms,"%s(%s,%s,%s)", mod,
			fileTypeString, fax_data, prompt_phrase);

	rc = apiInit(mod, TEL_SENDFAX, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);

	if( (rc = stat(fax_data, &yStat)) < 0)
	{
		sprintf(logMsg, 
				"Unable to send fax. Can't open fax data file (%s), rc=%d. [%d, %s]",
				fax_data, rc, errno, strerror(errno));
		fax_log(mod, REPORT_NORMAL,FAX_FILE_IO_ERROR, logMsg);
		HANDLE_RETURN(rc);
	}

	isThisAListFile = 0;
	/*  validate file type */
	switch (file_type)
	{
		case LIST:
			isThisAListFile = 1;
			rc = buildTiffFile(fax_data, lMsg.faxFile);

			if(rc != TEL_SUCCESS)
			{
				HANDLE_RETURN(-1);
			}
			file_type = TIF;
			sprintf(tmp_fax_data, "%s", lMsg.faxFile);
			break;
		case TEXT:
			numTextToTiffs = 1;
			tmp_tiff_files[0] = calloc(256, sizeof(char));

			file_type = TEXT;
			memset((char *)tmp_text_data, '\0', sizeof(tmp_text_data));
			rc = adjustFiles(fax_data, tmp_text_data, tmp_tiff_files[0]);

			memset((char *)errMsg, '\0', sizeof(errMsg));
			rc = doTextToTiff(tmp_text_data, tmp_tiff_files[0], errMsg);
			if ( rc != 0 )
			{
				sprintf(logMsg, "Unable to convert text file to tiff. %s", errMsg);
				fax_log(mod, REPORT_NORMAL,TEL_BASE, logMsg);
				HANDLE_RETURN(rc);
			}
			sprintf(tmp_fax_data, "%s", tmp_tiff_files[0]);
			break;
		case TIF:
			sprintf(tmp_fax_data, "%s", fax_data);
			break;
		default:
			sprintf(logMsg,"Invalid file_type parameter: %d." 
				"Valid values: LIST, TEXT, and TIF."); 
			fax_log(mod,
				REPORT_NORMAL,TEL_BASE,logMsg);
			HANDLE_RETURN(-1);
			break;
	}
			
	/*
	 * If prompt phrase != NULL, speak it out if it is accessable.  If not,
	 * log a message and continue processing the fax.
	 */
	if(strlen(prompt_phrase) > 0)
	{
		if (access(prompt_phrase, R_OK) == 0)
		{
			struct Msg_Speak msg;

			msg.opcode = DMOP_SPEAK;
       		msg.appCallNum = GV_AppCallNum1;
			msg.appPid = GV_AppPid;
			msg.appRef = GV_AppRef++;
			msg.appPassword = GV_AppPassword;
			msg.list = 0;
			msg.allParties = 0;
			msg.synchronous = SYNC;
			msg.interruptOption = FIRST_PARTY_INTERRUPT;
			sprintf(msg.file, "%s", prompt_phrase);

			rc = tel_Speak(mod, FIRST_PARTY_INTERRUPT, &msg, 1);
			if(rc != TEL_SUCCESS)
			{
				HANDLE_RETURN(-1);
			}
		}
		else
		{
			sprintf(logMsg,
			"Failed to access prompt phrase (%s). [%d, %s]",
				 prompt_phrase, errno, strerror(errno));
			fax_log(mod,REPORT_NORMAL, 
					TEL_BASE, logMsg);
		}
	}

	/*DDN: 05252011*/	

	struct Msg_Fax_PlayTone msgCngTone;

	msgCngTone.opcode = DMOP_FAX_PLAYTONE;
  	msgCngTone.appCallNum = GV_AppCallNum1;
	msgCngTone.appPid = GV_AppPid;
	msgCngTone.appRef = GV_AppRef++;
	msgCngTone.appPassword = GV_AppPassword;
	msgCngTone.sendOrRecv = 0;

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &msgCngTone, sizeof(struct Msg_Fax_PlayTone));

	rc = sendRequestToDynMgr(mod, &msg);
	
	breakOutOfWhile = 0;
	while(breakOutOfWhile == 0)
	{
		rc = readResponseFromDynMgr(mod,0,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			sprintf(logMsg, 
				"Timeout waiting for response from Dynamic Manager.");
			fax_log(mod, REPORT_NORMAL,FAX_BASE, logMsg);
			breakOutOfWhile = 1;
			break;
		}
		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;
		case DMOP_DISCONNECT:
			rc = disc(mod, response.appCallNum);	
			return(rc);
			break;
		case DMOP_SUSPENDCALL:
            rc = TEL_CALL_SUSPENDED;
			return(rc);
			break;
		case DMOP_RESUMECALL:
            rc = TEL_CALL_RESUMED;
			return(rc);
			break;
		default:
			/* Unexpected message. Logged in examineDynMgr... */
			break;
		} /* switch rc */
		breakOutOfWhile = 1;
	} /* while */
	
	lMsg.opcode 	= DMOP_SENDFAX;
	lMsg.appPid 	= GV_AppPid;
	lMsg.appPassword= GV_AppPassword;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appRef 	= GV_AppRef++;

	lMsg.fileType = file_type;

	sprintf(lMsg.faxFile, "%s", tmp_fax_data);
#if 0
	if ( file_type == TIF )
	{
		sprintf(lMsg.faxFile, "%s", fax_data);
	}
	else
	{
		rc = createSendFaxDataFile(mod, fax_data, lMsg.faxFile);
		if (rc != 0) 
		{
			if ( file_type != TIF )
			{
				unlink(lMsg.faxFile);
			}
			HANDLE_RETURN(rc);
		}
	}
#endif
	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_SendFax));

	rc = sendRequestToDynMgr(mod, &msg);
	if (rc == -1) 
	{
		breakOutOfWhile = 1;
	}
	else
	{
		breakOutOfWhile = 0;
	}

	while(breakOutOfWhile == 0)
	{
		rc = readResponseFromDynMgr(mod,0,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			sprintf(logMsg, "Timeout waiting for response from Dynamic Manager.");
			fax_log(mod, REPORT_NORMAL,TEL_BASE, logMsg);
			breakOutOfWhile = 1;
			break;
		}
		if(rc == -1) 
		{
			breakOutOfWhile = 1;
			break;
		}
	
		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_SENDFAX:
		case DMOP_FAX_COMPLETE:
			if (response.returnCode == 0)
			{
				/* ?? already have this set from switch */
				GV_AppCallNum1 = response.appCallNum; 
				GV_AppPassword = response.appPassword; 
			}
			breakOutOfWhile = 1;
			rc = response.returnCode;
			break;

		case DMOP_GETDTMF:
			saveTypeAheadData(mod, response.appCallNum, response.message);
			break;

		case DMOP_DISCONNECT:
			breakOutOfWhile = 1;
			rc = disc(mod, response.appCallNum);	
			break;

		case DMOP_SUSPENDCALL:
			breakOutOfWhile = 1;
            rc = TEL_CALL_SUSPENDED;
			break;

		case DMOP_RESUMECALL:
			breakOutOfWhile = 1;
            rc = TEL_CALL_RESUMED;
			break;

		default:
			/* Unexpected message. Logged in examineDynMgr... */
			break;
		} /* switch rc */
	} /* while */

	for ( i = 0; i < numTextToTiffs; i++)
	{
		if ( access (tmp_tiff_files[i], F_OK) == 0) 
		{
			sprintf(logMsg, "Removing generated tiff file (%s)", tmp_tiff_files[i]);
			fax_log(mod, REPORT_VERBOSE,FAX_BASE, logMsg);
	//		(void)unlink(tmp_tiff_files[i]);
		}
		free(tmp_tiff_files[i]);
	}

	if ( (file_type != TIF) || ( isThisAListFile ) )
	{
		unlink(lMsg.faxFile);
	}

	HANDLE_RETURN(rc);
}

#if 0
static int createSendFaxDataFile(char *mod, char *sendFaxData, char *file)
{
	FILE *fp;
	char fileName[20];
	char m[MAX_LENGTH];

	*file = '\0';
	sprintf(fileName, "SendFaxData.%d.%d", GV_AppPid, GV_AppRef);

	fp = fopen(fileName, "w+");
	if(fp == NULL)
	{
		sprintf(m, "Failed to open file (%s) for writing. [%d, %s].",
			fileName, errno, strerror(errno));
		fax_log(mod, REPORT_NORMAL,TEL_BASE, m);
		return(-1);
	}

	fprintf(fp, "%s", sendFaxData);

	fclose(fp);

	sprintf(file, "%s", fileName);
	return(0);
}
#endif

static int buildTiffFile(char *fax_data, char *outputTiffFile)
{
	char		listFile[128] = "myList.txt";
	char 		mod[] = "buildTiffFile";
	FILE		*fpListFile;
	char		buf[512];
	int			i;
	int			rc;
	int			invalidParameterFound;
	char		cmdbuff[2048];
	char		files[2048];
	int			pos;
	char		outfile[256];
	char		tmpTiffFile[256];
	char		msg[256];
	int			sizecount;
	int			sizetotal;
	FILE		*fin;
    struct stat	file_stat;

	struct  faxListStruct
	{   
		char faxFile[256];
		int  fileType;
		int  startPage;         /* Beginning page number of tiff file to send */
		int  numberOfPages;     /* Number of pages to send from a tiff file */
		char comment[256];
		int  ejectPage;         /* Eject a page after this file? */
	};

    struct intFieldsStruct
    {
        char            fileType[32];
        char            startPage[32];
        char            numberOfPages[32];
        char            ejectPage[32];
    } intListField;

	//struct faxListStruct    faxListItems[MAX_FAX_ITEMS+1];
	struct faxListStruct    faxListItems[10];

	memset((struct faxListStruct *)faxListItems, '\0', sizeof(faxListItems));

	outputTiffFile[0] = '\0';

	sprintf(listFile, "%s", fax_data);
	if ((fpListFile = fopen(listFile, "r")) == NULL)
	{
		sprintf(logMsg, 
			"Failed to open fax list file (%s).  "
			"[%d, %s] No fax sent.", listFile, errno, strerror(errno));
		fax_log(mod, REPORT_NORMAL,FAX_FILE_IO_ERROR, logMsg);
		return(-1);
	}

	memset((char *)files, '\0', sizeof(files));

	invalidParameterFound=0;
	pos = 0;
	i = 0;
	sizetotal = 0;

	while(fgets(buf, sizeof(buf), fpListFile) != NULL)
	{
		buf[strlen(buf)-1] = '\0';

		if (buf[0] == '#')
		{
			continue;
		}

		if (strcmp(buf,"") == 0)
		{
			continue;
		}

		memset((struct inFieldsStruct *)&intListField, 0,
				sizeof(intListField));

		sscanf(buf,"%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]",
				faxListItems[i].faxFile,
				intListField.fileType,
				intListField.startPage,
				intListField.numberOfPages,
				intListField.ejectPage,
				faxListItems[i].comment);

		if (access(faxListItems[i].faxFile, F_OK) != 0)
		{
			sprintf(logMsg, 
				"On line %d of list file (%s), "
				"fax content file (%s) is not available. "
				"[%d, %s]",
				i+1, listFile,
				faxListItems[i].faxFile,
				errno, strerror(errno));
			fax_log(mod, REPORT_NORMAL,TEL_BASE, logMsg);
			invalidParameterFound=1;
			continue;
		}

		/*  validate ejectPage */
		if ( (strcmp(intListField.ejectPage, "0") != 0  ) &&
		     (strcmp(intListField.ejectPage, "1") != 0  ) )
		{
			sprintf(logMsg, 
				"Error on line %d of list file (%s): Invalid ejectPage parameter: %d.  "
				"Valid values: 0 or 1.", 
				i+1, listFile, faxListItems[i].ejectPage);
			fax_log(mod, REPORT_NORMAL,TEL_BASE, logMsg);
			invalidParameterFound=1;
			continue;
		}

		if (stat (faxListItems[i].faxFile, &file_stat) != 0)
		{
			sprintf(logMsg, 
					"Unable to stat fax content file (%s).  "
					"Unable to send fax.", faxListItems[i].faxFile);
			fax_log(mod, REPORT_NORMAL,TEL_BASE, logMsg);
			invalidParameterFound=1;
			continue;
		}
		else
		{
			if ( file_stat.st_size <= 0 )
			{
				sprintf(logMsg, 
					"Fax content file (%s) is empty; size=%d. "
					"Unable to send fax.", faxListItems[i].faxFile, file_stat.st_size);
				fax_log(mod, REPORT_NORMAL,TEL_BASE, logMsg);
				invalidParameterFound=1;
				continue;
			}
			else
			{
				sizetotal += file_stat.st_size;
//				sprintf(logMsg, 
//					"faxListItems[%d].faxFile=(%s)", i, faxListItems[i].faxFile);
//				fax_log(mod, REPORT_NORMAL,TEL_BASE, logMsg);
				pos += snprintf(&files[pos], sizeof(files) - pos, "%s ",
									faxListItems[i].faxFile);

				if ( strcmp(intListField.ejectPage, "1") == 0  )
				{
					pos += snprintf(&files[pos], sizeof("page.brk") - pos, "%s ",
									"page.brk");
				}
			}
		}
	

		i++;
//		if (i == MAX_FAX_ITEMS)
//		{
//			sprintf(logMsg, 
//				"(%d) has been reached.  "
//				"Fax will not be sent.", i);
//			fax_log(mod, REPORT_NORMAL,TEL_BASE, logMsg);
//			fclose(fpListFile);
//			return(-1);
//		}
	}
	fclose(fpListFile);
	sizecount = sizetotal - file_stat.st_size;
	
	if ( invalidParameterFound )
	{
		sprintf(logMsg, 
			"Due to invalid parameter, fax will not be sent.");
		fax_log(mod, REPORT_NORMAL,FAX_INVALID_PARM, logMsg);
		return(-1);
	}
	
	if ( files[0] == '\0' )
	{	
		sprintf(logMsg, 
		     "There is nothing to send; returning failure.");
		fax_log(mod, REPORT_NORMAL,FAX_INVALID_PARM, logMsg);
		return(-1);
	}

	memset((char *) tmpTiffFile, '\0', sizeof(tmpTiffFile));
	memset((char *) msg, '\0', sizeof(msg));
	if ((rc = doTextToTiff(files, tmpTiffFile, msg)) != 0 )
	{
		sprintf(logMsg, "Unable to process list file (%s).  %s", 
					listFile, msg);
		fax_log(mod, REPORT_NORMAL,FAX_INVALID_PARM, logMsg);
		return(-1);
	}

	sprintf(outputTiffFile, "%s", tmpTiffFile);
	sprintf(logMsg, "Successfully created TIFF file (%s) from list file (%s).",
			tmpTiffFile, listFile);
	fax_log(mod, REPORT_VERBOSE,FAX_BASE, logMsg);

	return(0);
}/*END: buildTiffFile() */

/*-------------------------------------------------------------------------
  -----------------------------------------------------------------------*/
static int doTextToTiff(char *listOfFiles, char *tiffFile, char *errorMsg)
{
	char	utilityCommand[512];
	FILE 	*f;
	char 	returnValue[512];
	int		rc;

	char *ispbase = NULL;
	char utilitypath[256];

	if((ispbase = (char *)getenv("ISPBASE")) == (char *)NULL)
	{
		sprintf(errorMsg,  "getenv(ISPBASE) failed.");
		return (-1);
	}

	sprintf(utilitypath, "%s/Telecom/Exec/TextToTiffConvertor/TextToTIFFConvertor.class", ispbase);
	if(access(utilitypath, F_OK) == -1)
	{
		sprintf(errorMsg,  "TextToTiffConvertor not found.");
		return (-1);
	}
	memset((char *)utilityCommand, '\0', sizeof(utilityCommand));
	sprintf(utilityCommand, "java TextToTIFFConvertor %s", listOfFiles);

	sprintf(logMsg, "Executing command (%s)", utilityCommand);
	fax_log(mod, REPORT_VERBOSE,FAX_BASE, logMsg);

	f = popen(utilityCommand, "r");
	if (f == 0)
	{
		sprintf(errorMsg,  "popen(%s) failed. [%d, %s]",
				utilityCommand, errno, strerror(errno));
		return(-1);
	}
	/*
		ReturnValue: 
			1. if successful, generated tiff file name with full path.
			2. if unsuccessful, negative return code (-1 / -2).
				-1: Some exception etc. occurred.
				-2: utility is not invoked with all the required parameters	
	*/

	memset((char *)returnValue, '\0', sizeof(returnValue));
	fgets(returnValue, sizeof(returnValue), f);

	pclose(f);

	if ( returnValue[0] == '-' )
	{
		sprintf(errorMsg,  "TextToTIFFConvertor failed (rc=%s) for list "
				"of files (%s).",
				returnValue, listOfFiles);
		return (-1);
	}

	sprintf(tiffFile, "%s", returnValue);

	sprintf(logMsg,  "TextToTIFFConvertor succeeded for list "
				"of files (%s).", listOfFiles);
	fax_log(mod, REPORT_VERBOSE,FAX_BASE, logMsg);

	return(0);

} // END: doTextToTiff()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static int adjustFiles(char *zTextFile, char *zNewTextFile, char *zTiffFile)
{
	char 		*p;
	char		teDir[128];
	char		telecomDir[128];
	char		*ispbase;
	char		tmpBuf[256];

	sprintf(tmpBuf, "%s", zTextFile);

	// is it a full path?
	if ( zTextFile[0] == '/' )
	{
		sprintf(zNewTextFile, "%s", zTextFile);
		p = strrchr(tmpBuf, '/');
		p++;
		sprintf(zTiffFile, "%s.tif", p);
		return(0);
	}

	if((ispbase = (char *)getenv("ISPBASE")) == (char *)NULL)
	{
		// print error message, default to /home/arc
	}

	sprintf(teDir, "%s/Telecom/Exec", ispbase);
	sprintf(telecomDir, "%s/Telecom", ispbase);

	// is there no path (current directory)?
	if ( (p = strrchr(zTextFile, '/')) == (char *) NULL )
	{
		sprintf(zNewTextFile, "%s/%s", teDir, zTextFile);
		sprintf(zTiffFile, "%s.tif", zTextFile);
		return(0);
	}

	// must be a relative path

	// relative path; current directory
	if ( ( zTextFile[0] == '.' ) && ( zTextFile[0] != '.' ) )
	{
		p = &(zTextFile[1]);
		sprintf(zNewTextFile, "%s/%s", teDir, p);
		p = strrchr(tmpBuf, '/');
		p++;
		sprintf(zTiffFile, "%s.tif", p);
		return(0);
	}

	// relative path; current directory
	if ( zTextFile[0] != '.' )
	{
		sprintf(zNewTextFile, "%s/%s", teDir, zTextFile);
		p = strrchr(tmpBuf, '/');
		p++;
		sprintf(zTiffFile, "%s.tif", p);
		return(0);
	}
	
	// relative path; parent directory
	if ( ( zTextFile[0] == '.' ) && ( zTextFile[0] == '.' ) )
	{
		p = &(zTextFile[3]);
		sprintf(zNewTextFile, "%s/%s", telecomDir, p);
		p = strrchr(tmpBuf, '/');
		p++;
		sprintf(zTiffFile, "%s.tif", p);
		return(0);
	}

	// what the stink
	return(-1);
	
} // END:  adjustFiles()
