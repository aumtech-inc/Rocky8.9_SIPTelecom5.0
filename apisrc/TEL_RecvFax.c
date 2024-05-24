#ident	"@(#)TEL_RecvFax 00/05/22 3.1 Copyright 2000 Aumtech"
static char mod[]="TEL_RecvFax";
/*-----------------------------------------------------------------------------
Program:	TEL_RecvFax.c
Purpose:	Receive a fax into file on current line.
Author: 	Dan Barto
Date:		05/21/99
Update:		12/26/02 apj Changes necessary for Single DM structure.
Update: 	02/22/2003 apj Added TEL_CALL_SUSPENDED code.
Update: 	08/05/2003 apj Put a prompt phrase name in the log message.
Update:     08/28/2003 ddn  Added TEL_CALL_RESUMED code
Update:     10/13/2003 apj If tel_Speak returns 1, continue.
Update:     11/10/2004 apj Save DTMFs in the buffer.
Update:     01/04/2006 mpb Replaced sys_errlist with strerror.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"

extern int tel_Speak(char *mod, int interruptOption, void *m, int updateLastPhraseSpoken);

/*------------------------------------------------------------------------------
TEL_RecvFax()
	Main routine performs error checking and the internal receiveFax
	function() which includes:
	- has TEL_InitTelecom() been called
	- are we still connected
	- speak the prompt phrase
	- verify the fax_file path
	- ask DM to receive a fax.
------------------------------------------------------------------------------*/
int TEL_RecvFax(fax_file, fax_pages, prompt_phrase)
char	*fax_file;		/* phrase number */
int	*fax_pages;		/* number of pages in the fax */
char	*prompt_phrase;		/* phrase to speak, i.e. Press start button */
{
	static	char	logMsg[256];
	int		rc;
	int		rc2;
	char		faxFileName[256];
	char		faxPath[256];
	char		*ptr;
	struct stat	statBuf;
	char		faxResourceName[128];
	char		faxResourceSlot[10];
	int		fax_handle;
	char 	apiAndParms[MAX_LENGTH];
	struct MsgToApp response;
	struct Msg_RecvFax lMsg;
	struct MsgToDM msg;
	int breakOutOfWhile = 0;


#if 0
//RGDEBUG need to remove the hardcoded path	
	int isAculabDetected = access( "/usr/local/aculab/v6/lib/libTiNG.a", F_OK);		
	if(isAculabDetected != 0)
	{
		sprintf(logMsg, "Third party software (libTiNG.a) required for fax not found.");
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err, logMsg);
		return(TEL_FAILURE);
	}
#endif
	
	sprintf(apiAndParms,"%s(%s,%s,%s)", mod,
			fax_file, "&fax_pages", prompt_phrase);
	rc = apiInit(mod, TEL_RECVFAX, apiAndParms, 1, 1, 0); 
	if (rc != 0) HANDLE_RETURN(rc);

	memset((char *)faxPath, 0, sizeof(faxPath));
	ptr = (char*)strrchr(fax_file, (int)'/');
	if(ptr != NULL)
	{
		strncpy(faxPath, fax_file, ptr - fax_file);
		faxPath[ptr - fax_file] = '\0';
		
		if (access(faxPath, W_OK) == -1)
		{
			sprintf(logMsg, "Error in path specified to receive "
				"fax (%s) [%d, %s]",
				faxPath, errno, strerror(errno));
			fax_log(mod, REPORT_NORMAL, TEL_INVALID_PARM, logMsg);
			HANDLE_RETURN(TEL_FAILURE);
		}
		
		memset((struct statBuf *)&statBuf, 0, sizeof(statBuf));
		if((rc = stat(faxPath, &statBuf)) != 0)
		{ 
			sprintf(logMsg,
				"Unable to stat fax directory (%s). [%d, %s]",
				faxPath, errno, strerror(errno));
			fax_log(mod,REPORT_NORMAL,TEL_INVALID_PARM,logMsg);
			HANDLE_RETURN(TEL_FAILURE);
		}
		if( (rc = S_ISDIR(statBuf.st_mode)) == 0)
		{
			sprintf(logMsg,
				"Path (%s) specified to receive fax is not a "
				"directory.", faxPath);
			fax_log(mod,REPORT_NORMAL,TEL_INVALID_PARM,
				logMsg);
			HANDLE_RETURN(TEL_FAILURE);
		}
	}

	/*  If prompt phrase != NULL, speak it out if it is accessable.  
	  If not, log a message and continue processing the fax. */
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
			if((rc != TEL_SUCCESS) && (rc != 1))
			{
				HANDLE_RETURN(-1);
			}
		}
		else
		{
			sprintf(logMsg,
			"Failed to access prompt phrase (%s). [%d, %s]",
			 	prompt_phrase, errno, strerror(errno));
			fax_log(mod,REPORT_NORMAL, TEL_INVALID_PARM, logMsg);
		}
	}

#if 0
	memset((char *)faxResourceName, 0, sizeof(faxResourceName));
	if  ((rc=getAFaxResource(mod, faxResourceName,
					faxResourceSlot)) == -1)
	{
		HANDLE_RETURN(TEL_FAILURE); /* message is logged in the routine */
	}
	sprintf(logMsg,"Got fax resource (%s) with slot number(%s). "
			"%d=GetAFaxResource(%s, %s).", faxResourceName, 
			faxResourceSlot, rc, faxResourceName, faxResourceSlot);
	fax_log(mod,REPORT_VERBOSE,TEL_BASE, logMsg);
#endif

	/*DDN: 05252011*/	
	struct Msg_Fax_PlayTone msgCngTone;

	msgCngTone.opcode = DMOP_FAX_PLAYTONE;
   	msgCngTone.appCallNum = GV_AppCallNum1;
	msgCngTone.appPid = GV_AppPid;
	msgCngTone.appRef = GV_AppRef++;
	msgCngTone.appPassword = GV_AppPassword;
	msgCngTone.sendOrRecv = 1;

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &msgCngTone, sizeof(struct Msg_Fax_PlayTone));

	rc = sendRequestToDynMgr(mod, &msg);
	
	breakOutOfWhile = 0;
	while(breakOutOfWhile == 0)
	{
		rc = readResponseFromDynMgr(mod,0,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod, REPORT_NORMAL, FAX_API_TIMEOUT_WAIT_FOR_RESPONSE, ERR,
				"Timeout waiting for response from Dynamic Manager.");
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

	lMsg.opcode 	= DMOP_RECVFAX;
	lMsg.appPid 	= GV_AppPid;
	lMsg.appPassword= GV_AppPassword;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appRef 	= GV_AppRef++;

	// sprintf(lMsg.faxResourceName, "%s", faxResourceName);
	sprintf(lMsg.faxFile, "%s", fax_file);

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_RecvFax));

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
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
				"Timeout waiting for response from Dynamic Manager.");
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
		case DMOP_RECVFAX:
			if (response.returnCode == 0)
			{
				/* ?? already have this set from switch */
				GV_AppCallNum1 = response.appCallNum; 
				GV_AppPassword = response.appPassword; 
				*fax_pages = atoi(response.message);
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
            HANDLE_RETURN(TEL_CALL_RESUMED);
			break;

		case TEL_FM_CONNECTED:
			HANDLE_RETURN(TEL_FM_CONNECTED);
			break;
		case TEL_FM_SENDDATA:
			HANDLE_RETURN(TEL_FM_SENDDATA);
			break;

		default:
			/* Unexpected message. Logged in examineDynMgr... */
			break;
		} /* switch rc */
	} /* while */

#if 0
	sprintf(logMsg,"Sending request to Resource Manager to free resource "
			"(%s) of type (%s), subtype (%s), port number=%d.",
			faxResourceName, RESOURCE_TYPE, RESOURCE_SUB_TYPE, 
			GV_AppCallNum1);
	fax_log(mod,REPORT_VERBOSE,TEL_BASE, logMsg);

	if((rc2=Free_Resource(RESOURCE_TYPE,RESOURCE_SUB_TYPE,GV_AppCallNum1,
	 	GV_AppName, faxResourceName, logMsg)) == RESM_UNABLE_TO_SEND)
	{
		/* First log the message returned from the subroutine. */
                fax_log(mod,REPORT_NORMAL,TEL_BASE,logMsg);
                sprintf(logMsg, "Warning: Request to free fax resource (%s) "
				"failed.  rc=%d.", faxResourceName, rc2);
                fax_log(mod,REPORT_NORMAL,
				TEL_BASE,logMsg);
	}
#endif
	if ( ( rc != TEL_DISCONNECTED) && ( rc != TEL_SUCCESS ) )
	{
		if ( access(fax_file, F_OK) == 0 )
		{
			unlink(fax_file);
		}
	}

	HANDLE_RETURN(rc);
}
