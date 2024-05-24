#ident	"@(#)CDR_CustomEvent 95/12/05 2.2.0 Copyright 1996 AT&T"
static char ModuleName[]="CDR_CustomEvent";
/*-----------------------------------------------------------------------------
 * CDR_CustomEvent.c -- Send Custom Event message to log manager
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
 * 12/15/94	P. Bruno	1. Add lm_select to support snmp traps MR756
 * 08/02/95	T. Aridi	1. Added variable SNMP trap capability.
 * 03/18/95	G. Wenzel	Updated for ISP 2.2
 *
 * FUNCTIONS
 *
 *-----------------------------------------------------------------------------
 */

#include "COM_Mnemonics.h"
#include "ISPCommon.h"
#include "ispinc.h"
#include "loginc.h"
#include "CEVmsg.h"
#include "COMmsg.h"
#include "monitor.h"
/* mahesh change following line */
/* #include <ism/sys/lm.h> */
#include <lm.h>

#define	CEV_BUF_SIZE	256
#define	CEV_DATA_SIZE	200

static char	data_buf[CEV_DATA_SIZE + 1];

/*----------------------------------------------------------------------------
Send the Custom Event message to the logger.
-----------------------------------------------------------------------------*/
int 
CDR_CustomEvent(informat, data, end_of_rec)
int	informat;		/* CEV_STRING or CEV_INTEGER. */
const	void	*data;		/* data to be sent to call detail record */
int	end_of_rec;		/* 0 !end data,1 end data,2 end and SNMP alert */
{
	char	my_data[CEV_DATA_SIZE + 1];
	char	custom_event_buf[CEV_BUF_SIZE];
	char	my_int[15];
	char	trap_digits[15];
	char	*trap_ptr;
	long	trap_value;
	int	ret_code = 0;
	int	overflow = 0;
	int	snmp_error = 0;
	char	diag_mesg[1024];

	if(informat == CEV_STRING)
	{
	  sprintf(diag_mesg,"CDR_CustomEvent(%d, %p, %d)", informat, 
		data, end_of_rec);
	
	}
	else if(informat == CEV_INTEGER)
	{
	  sprintf(diag_mesg,"CDR_CustomEvent(%d, %d, %d)", informat, 
		*(int *)data, end_of_rec);
	  sprintf(my_int, "%d", *(int *)data);
	}
	else
	{
	    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_PARMNUM,"informat",informat); 
	    return(-1);
	}

	ret_code = send_to_monitor(MON_API, CDR_CUSTOMEVENT, diag_mesg);	

	/* Check to see if InitTelecom API has been called. */
	if(GV_InitCalled != 1)
	{
	    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_NOTINIT,"Service");
	    return (-1);
	}

	if(informat == CEV_STRING)
	{
	  /* Test length of incoming data string. Account for colon. */
	  if(((int)strlen(data)) < CEV_DATA_SIZE)
	  {
	    /* Substitute control characters. */
	    CDR_strip_str(data, my_data, (int)strlen(data));
	  }
	  else
	  {
	    LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_CEVDATA);
	    overflow = 1;
	  }

	  if(overflow != 1)
	  {
	    /* Test length of concatenated data string. Account for colon. */
	    if(((int)strlen(data_buf)+(int)strlen(my_data)) > (CEV_DATA_SIZE - 1))
	    {
		LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_CEVDATA);
		overflow = 1;
	    }
	    else
	    {
		/* Concatenate incoming data. */
		strcat(data_buf, ":");
		strcat(data_buf, my_data);
	    }
	  }
	}

	if(informat == CEV_INTEGER)
	{
	    /* Test length of concatenated data string. Account for colon. */
	    if(((int)strlen(data_buf) + (int)strlen(my_int)) > (CEV_DATA_SIZE - 1))
	    {
		LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_CEVDATA);
		overflow = 1;
	    }
	    else
	    {
		/* Concatenate incoming data. */
		strcat(data_buf, ":");
		strcat(data_buf, my_int);
	    }
	}

	if(end_of_rec >= 1)	/* No more data to follow. */
	{
	    custom_event_buf[0] = '\0';
	    sprintf(custom_event_buf, "%s", GV_CDR_Key);
	    strcat(custom_event_buf, data_buf);

	/* change options for message for snmp trap if desired */

	    if(end_of_rec==2)
	    {
	       snmp_error=0;
	/* TFA Changes */
	       for(trap_ptr = data_buf+1;
			 (*trap_ptr && (*trap_ptr != ':') );trap_ptr++);
               memset(trap_digits,'\0',15);
               strncpy(trap_digits,data_buf+1,trap_ptr-(data_buf+1));
               if((int)strlen(trap_digits) > 0){
                  trap_ptr=trap_digits;
                  while(*trap_ptr && isdigit(*trap_ptr)) trap_ptr++;
                  if( *trap_ptr){
                      trap_value = 90010;
		      LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_CEVTRAP,trap_digits); 
                  }else{
                      trap_value = atol(trap_digits);
                  }
               }else{ 
                  trap_value = 90010;
		  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_ERR_CEVTRAP,""); 
               }
               printf("The value of the SNMP trap = %ld\n",trap_value);
               
		
	/* TFA End of Changes */
	       if(lm_select(LM_SPECIAL,LM_TRAP,LM_ENABLED,0)==-1)
	       {
		  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_SNMP_ENABLE); 
		  snmp_error=1;
	       }
	       if(lm_select(LM_SPECIAL,LM_TRAP_ID,trap_value,0)==-1)
	       {
		  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_SNMP_TRAP_ID); 
		  snmp_error=1;
	       }
	    }
	 
	    /* End of data record. Send message. */
	    LOGXXPRT(ModuleName,REPORT_CEV,CEV_MESSAGE,custom_event_buf);

	/* Unselect the snmp options */

	    if(end_of_rec==2)
	    {
	       if(lm_unselect(LM_SPECIAL,LM_TRAP)==-1)
	       {
		  LOGXXPRT(ModuleName,REPORT_NORMAL,COM_SNMP_UNSET); 
		  snmp_error=1;
	       }
	    }
	    custom_event_buf[0] = '\0';
	    data_buf[0] = '\0';
	}

	if(overflow == 1)
	    return(-2);	/* CDR_OVERFLOW */
	else
	if(snmp_error == 1)
	    return(-3);
	else
    return(0);
}
