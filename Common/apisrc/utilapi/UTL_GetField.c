#ident	"@(#)UTL_GetField 96/06/26 2.2.0 Copyright 1996 Aumtech, Inc."
static char ModuleName[]="UTL_GetField";
/*-----------------------------------------------------------------------------
Program:	 UTL_GetField.c
Purpose:	Get the field from the string.
Author:		M. Bhide
Date:		02/23/94
Update:		12/05/95 G. Wenzel updated for ISP 2.2
Update:		06/26/96 G. Wenzel removed error check for send_to_monitor
Update:		06/26/96 G. Wenzel put field number in messages
Update:		07/22/96 M. Joshi fix the bug in the message 
--------------------------------------------------------------------------------
 * Copyright (c) 1996, Aumtech, Inc.
 * All Rights Reserved.
 *
 * This is an unpublished work of Aumtech which is protected under 
 * the copyright laws of the United States and a trade secret
 * of Aumtech. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of Aumtech.
------------------------------------------------------------------------------*/

#include "ISPCommon.h"
#include "ispinc.h"
#include "COMmsg.h"
#include "COM_Mnemonics.h"
#include "monitor.h"

#define	IN	0
#define	OUT	1

extern char	GV_IndexDelimit;

extern int LOGXXPRT(char *module, int requested_mode, int MessageID, char *bogus, ... );
/*------------------------------------------------------------------------------
This routine gets the value of desired field from the buffer.
------------------------------------------------------------------------------*/
int UTL_GetField(buffer, fld_num, field)
const	char	*buffer;		/* data record */
int	fld_num;			/* field # in the buffer */
char	*field;				/* field value returned */
{
register	int	i;
int	fld_len = 0;			/* field length */
int	state = OUT;			/* current state IN or OUT */
int	wc = 0;				/* word count */
int	ret_code = 0;
char	diag_mesg[1024];

if(GV_HideData)
  sprintf(diag_mesg,"UTL_GetField(%s,%d,%s)", "buffer", fld_num, "data");
else
  sprintf(diag_mesg,"UTL_GetField(%s,%d,%s)", buffer, fld_num, "data");

ret_code = send_to_monitor(MON_API, UTL_GETFIELD, diag_mesg);

/* Check to see if Init 'Service' API has been called. */
if(GV_InitCalled != 1)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOTINIT,"Service");
    return (-1);
}

field[0] = '\0';
if( fld_num < 0)
{
    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_PARMNUM, "fld_num", fld_num);
    return (-1);
}

for(i=0; i < (int)strlen(buffer); i++)
{
    if(buffer[i] == GV_IndexDelimit || buffer[i] == '\t' || buffer[i] == '\n')
    {
	state = OUT;
	if(buffer[i] == GV_IndexDelimit && buffer[i-1] == GV_IndexDelimit)
	++wc;
    }
    else if (state == OUT)
    {
	state = IN;
	++wc;
    }
	
    if (fld_num == wc && state == IN) 	field[fld_len++] = buffer[i];
    if (fld_num == wc && state == OUT) 	break;
} /* end for */

if (fld_num != wc)
{
	sprintf(diag_mesg,"Field number %d is out of range.", fld_num);
    	LOGXXPRT(ModuleName, REPORT_NORMAL, COM_ISPDEBUG, diag_mesg);
    	return(-1);
}

if (fld_len > 0)
{
    /* Remove leading spaces. */
    field[fld_len] = '\0';
    while(field[0] == ' ')
    {
	for (i=0; field[i] != '\0'; i++)
		field[i] = field[i+1];
    }
}

return (0);
}
