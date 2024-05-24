/*----------------------------------------------------------------------------
WSS_VAR.h : This is header file for work station server.(using socket)
	   This file contain all defination and data structure required by
	   work station server library.

Update:		04/16/97 sja	Declared _*clnt_mesg buffers to be MAXDATA+1
-----------------------------------------------------------------------------*/

char	__S_clnt_mesg[MAXDATA+1];	/* message buffer for client */
char	__R_clnt_mesg[MAXDATA+1];	/* message buffer for client */
char	*__S_clnt_mesg_ptr;		/* pointer to message buffer */
char	*__R_clnt_mesg_ptr;		/* pointer to message buffer */
/*char	__Log_Date[8];			/* time string YYMMDD */	

extern	int errno;			/* system error no */

int	flags;				/* used to receive info */
struct 	hostent *hp;			/* holds IP address of server */
/*============================= eof() =======================================*/
