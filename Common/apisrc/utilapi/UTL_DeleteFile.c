#ident	"@(#)UTL_DeleteFile 95/12/05 2.2.0 Copyright 1996 AT&T"
static char ModuleName[]="UTL_DeleteFile";
/*-----------------------------------------------------------------------------
 * UTL_DeleteFile.c -- Delete a text file.
 *
 * Copyright (c) 1996, AT&T Network Systems Information Services Platform (ISP)
 * All Rights Reserved.
 *
 * This is an unpublished work of AT&T ISP which is protected under 
 * the copyright laws of the United States and a trade secret
 * of AT&T. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of AT&T.
 *
 * ISPID            : 
 * Author	    : M. Bhide
 * Created On	    : 2/23/94
 * Last Modified By : G. Wenzel
 * Last Modified On : 12/05/95
 * 
 * HISTORY
 *
 * 12/05/95	G. Wenzel	Updated to ISP 2.2
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <unistd.h>
#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "COM_Mnemonics.h"
#include "monitor.h"

#define	MAX_SIZE	256

/*----------------------------------------------------------------------------
This routine deletes the fax text file.
----------------------------------------------------------------------------*/
int UTL_DeleteFile(filename)
const	char 	*filename;
{
int	ret_code = 0;
char	filepath[200];
char	diag_mesg[128];

sprintf(diag_mesg,"UTL_Delete(%s)", filename);
ret_code = send_to_monitor(MON_API, UTL_DELETEFILE, diag_mesg);
if ( ret_code < 0 )
{
        sprintf(diag_mesg, "send_to_monitor failed. rc = %d", ret_code);
        LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ISPDEBUG, diag_mesg);
}

/* Check to see if Init'Service' API has been called. */
if(GV_InitCalled != 1)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOTINIT,"Service");
    return (-1);
}

/* Get full path of file. */
if((ret_code = UTL_full_path(filename, filepath)) != 0)
{
    /* Error message printed in UTL_full_path function. */
    return (ret_code);
}

/* Delete the file. */
if((ret_code = unlink(filepath)) != 0)
{
  /* If file exists. */
  if(errno != ENOENT)
  {
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_FILEREMOVE,filepath,
			strerror(errno));
    return (-1);
  }
  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOFILE,filepath);
  return (-5);	/* UTL_NO_FILE_EXISTS */
}

return (0);
}
