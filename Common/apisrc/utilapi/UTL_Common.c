#ident	"@(#)UTL_Common 95/12/05 2.2.0 Copyright 1994 AT&T"
static char ModuleName[]="UTL_Common";
/*-----------------------------------------------------------------------------
 * UTL_Common.c -- Utility Common functions.
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
 * Author	    : R. Schmidt
 * Created On	    : 9/23/94
 * Last Modified By : R. Schmidt
 * Last Modified On : 9/23/94
 * 
 * HISTORY
 *
 * 9/23/94	R. Schmidt	1. Create new Utility Common file with
 *				   functions UTL_full_path and UTL_filecopy.
 *
 * FUNCTIONS:
 *	UTL_full_path(file, fullpath)  		 - Get full path of given file.
 *	UTL_filecopy(from FILE ptr, to FILE ptr) - Copy file. Returns bytes.
 *
 *-----------------------------------------------------------------------------
 */

#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"

/*---------------------------------------------------------------------------
This routine will get the full path of the given file.
---------------------------------------------------------------------------*/
int
UTL_full_path(file, fullpath)
char	*file;
char	*fullpath;
{
char	*cwd;			/* current working directory */
int	ret_code;

fullpath[0] = '\0';

/* Check if already full path. */
if((ret_code = strncmp("/",file,1)) == 0)
{
  strcpy(fullpath, file);
  return(0);
}

/* Get Current Working Directory. */
if((cwd = getcwd(NULL, 150)) == NULL)
{
  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_GENSTR,
    "ERROR: Unable to get current working directory.");
  return(-1);
}

/* Check if length is too long. */
if(((int)strlen(cwd) + (int)strlen(file) + 2) > 199)
{
  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_GENSTR,
    "ERROR: Full path string length problem.");
  return(-2);	/* UTL_FULL_PATH_LENGTH */
}

/* Concatenate the full path. */
strcpy(fullpath,cwd);
strcat(fullpath,"/");
strcat(fullpath,file);

free(cwd);

return(0);
}
/*---------------------------------------------------------------------------
This routine will copy one file to another given two open file pointers.
---------------------------------------------------------------------------*/
int
UTL_filecopy(in_ptr, out_ptr)
FILE	*in_ptr;
FILE	*out_ptr;
{
int	c;
int	bytes = 0;

/* Copy bytes until End Of File. */
while((c =getc(in_ptr)) != EOF)
{
  putc(c, out_ptr);
  bytes++;
}

return(bytes);
}

