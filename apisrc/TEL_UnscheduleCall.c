#ident	"@(#)TEL_UnscheduleCall 97/06/26 2.2.0 Copyright 1996 Aumtech, Inc."
/*------------------------------------------------------------------------------
File:		TEL_UnscheduleCall.c
Purpose:	Unschedule an outbound call.
Author:		Sandeep Agate
Date:		Unknown
Update:		03/08/01 gpw fixed to use GV_ISPBASE, proper TEL_ log msgs
Update:		03/08/01 gpw removed use of LOGXXPRT
-----------------------------------------------------------------------------
Copyright (c) 1996, Aumtech, Inc.
All Rights Reserved.

This is an unpublished work of Aumtech which is protected under 
the copyright laws of the United States and a trade secret
of Aumtech. It may not be used, copied, disclosed or transferred
other than in accordance with the written permission of Aumtech.
-----------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "ocmHeaders.h"

/*-----------------------------------------------------------------------------
TEL_UnscheduleCall(): Unschedule an outbound call.
-----------------------------------------------------------------------------*/
int TEL_UnscheduleCall(char *cdf)
{
	static char mod[] = "TEL_UnscheduleCall";
	char	cdfPath[256];
	char	apiAndParms[256] = "";
	int 	rc;

	sprintf(apiAndParms,"%s(%s)", mod, cdf);
	rc = apiInit(mod, TEL_UNSCHEDULECALL, apiAndParms, 1, 0, 0);
	if (rc != 0) HANDLE_RETURN(rc);
	
	if(cdf[0] == '\0')
	{
                telVarLog(mod, REPORT_NORMAL, TEL_INVALID_PARM, GV_Err,
                        "No call definition file specified.");
    		HANDLE_RETURN (TEL_FAILURE);
	}

	sprintf(cdfPath, "%s/%s/%s", GV_ISPBASE, OCS_WORK_DIR, cdf);
	if(access(cdfPath, F_OK) == 0)
	{
		/* Found the cdf. Delete it. */
		if(unlink(cdfPath) == -1)
		{
          	telVarLog(mod,REPORT_NORMAL, TEL_CANT_DELETE_FILE,
				GV_Err, TEL_CANT_DELETE_FILE_MSG, 
				cdfPath, errno, strerror(errno));
    			HANDLE_RETURN (-1);
		}
		/* Call is unscheduled */
		HANDLE_RETURN(TEL_SUCCESS);
	}

	/* We haven't found the cdf in the OCS_WORK_DIR.
	 * Is it in the OCS_CALLED_DIR?
	 */
	sprintf(cdfPath, "%s/%s/%s", GV_ISPBASE, OCS_CALLED_DIR, cdf);
	if(access(cdfPath, F_OK) == 0)
	{
                telVarLog(mod,REPORT_NORMAL,TEL_EPLACEDCALL,GV_Err,
			TEL_EPLACEDCALL_MSG, cdf);
		HANDLE_RETURN(TEL_PLACED_CALL);
	}
	/* We haven't found the cdf in the OCS_WORK_DIR & in the OCS_CALLED_DIR
	 * Is it in the OCS_ERR_DIR?
	 */
	sprintf(cdfPath, "%s/%s/%s", GV_ISPBASE, OCS_ERR_DIR, cdf);
	if(access(cdfPath, F_OK) == 0)
	{
		sprintf(Msg, "Failed to access file (%s). [%d,%s]",
			cdfPath, errno, strerror(errno));
    	       telVarLog(mod,REPORT_NORMAL,TEL_CANT_ACCESS_FILE,GV_Err, Msg);
		HANDLE_RETURN(TEL_OCS_ERROR);
	}

	/* We haven't found the cdf at all. Return a not found error */
	sprintf(Msg,"Cannot find call definition file (%s).", cdf);
	telVarLog(mod,REPORT_NORMAL,TEL_ECDFNOTFOUND,GV_Err, Msg);

	HANDLE_RETURN(TEL_CDF_NOT_FOUND);
} /* END: TEL_UnscheduleCall() */
