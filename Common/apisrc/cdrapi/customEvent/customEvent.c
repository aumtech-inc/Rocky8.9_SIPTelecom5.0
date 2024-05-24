#ident	"@(#)CDR_CustomEvent 95/12/05 2.2.0 Copyright 1996 AT&T"
static char ModuleName[]="CDR_CustomEvent";
/*-----------------------------------------------------------------------------
 * CustomEvent.c - executable version of the CDR_CustomEvent API
 *---------------------------------------------------------------------------*/

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

char GV_CDR_Key[50];

static char	data_buf[CEV_DATA_SIZE + 1];

int main(int argc, char *argv[])
{
	int		rc;

	if ( argc != 2 )
	{
		fprintf(stderr, "Usage: %s <string in quotes>\n", argv[0]);
		exit(0);
	}

	rc = customEvent(CEV_STRING, argv[1], 1);
}

/*-----------------------------------------------------------------------------
customEvent():
	int           informat;		// CEV_STRING or CEV_INTEGER.
	const void    *data;		// data to be sent to call detail record
	int           end_of_rec;	// 0 !end data,1 end data,2 end and SNMP alert
  ---------------------------------------------------------------------------*/
int customEvent(int informat, char *data, int end_of_rec)
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
	static	firstTime = 1;
	int		rc;

	if( firstTime ) 
	{
	    rc = LOGINIT("TEL", "TEL", 0, 0, "");
	    if(rc < 0)
	    {
           	fprintf(stderr, "Can't register with log manager (LOGINIT failed). rc=%d.", rc);
           	return(-1);
		}
		firstTime = 0;
    }

	if(informat == CEV_STRING)
	{
	  sprintf(diag_mesg,"CDR_CustomEvent(%d, %s, %d)", informat, 
		data, end_of_rec);
	
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
	    logxxprt(ModuleName,REPORT_NORMAL,COM_ERR_CEVDATA);
	    overflow = 1;
	  }

	  if(overflow != 1)
	  {
	    /* Test length of concatenated data string. Account for colon. */
	    if(((int)strlen(data_buf)+(int)strlen(my_data)) > (CEV_DATA_SIZE - 1))
	    {
		logxxprt(ModuleName,REPORT_NORMAL,COM_ERR_CEVDATA);
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
		      logxxprt(ModuleName,REPORT_NORMAL,COM_ERR_CEVTRAP,trap_digits); 
                  }else{
                      trap_value = atol(trap_digits);
                  }
               }else{ 
                  trap_value = 90010;
		  logxxprt(ModuleName,REPORT_NORMAL,COM_ERR_CEVTRAP,""); 
               }
//               printf("The value of the SNMP trap = %ld\n",trap_value);
               
		
	/* TFA End of Changes */
	       if(lm_select(LM_SPECIAL,LM_TRAP,LM_ENABLED,0)==-1)
	       {
		  logxxprt(ModuleName,REPORT_NORMAL,COM_SNMP_ENABLE); 
		  snmp_error=1;
	       }
	       if(lm_select(LM_SPECIAL,LM_TRAP_ID,trap_value,0)==-1)
	       {
		  logxxprt(ModuleName,REPORT_NORMAL,COM_SNMP_TRAP_ID); 
		  snmp_error=1;
	       }
	    }
	 
	    /* End of data record. Send message. */
	    logxxprt(ModuleName,REPORT_CEV,CEV_MESSAGE,custom_event_buf);

	/* Unselect the snmp options */

	    if(end_of_rec==2)
	    {
	       if(lm_unselect(LM_SPECIAL,LM_TRAP)==-1)
	       {
		  logxxprt(ModuleName,REPORT_NORMAL,COM_SNMP_UNSET); 
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
