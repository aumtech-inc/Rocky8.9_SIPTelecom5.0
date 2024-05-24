/*---------------------------------------------------------------------------
Purpose of this program is to remove all the messages of given type
from the message Que.
It removes all the message and show them on standard out.

01/17/00  mpb  Added TCP,TEL,SNA,DYNMGR options to argv.
----------------------------------------------------------------------------*/
#include "../include/ISPSYS.h"
#include "../include/BNS.h"

typedef struct {                        /* message structure */
        long    mesg_type2;             /* message type */
        char    mesg_data2[512];        /* message text */
} Mesg2;

Mesg2	mesg2;

main(argc, argv)
int	argc;
char	*argv[];
{
long	dyn_mgr_que=0L;
int	ret_code;
int	dyn_mqid;
int	msgQid = 0;
char	tempBuf[30];
char	tempBuf1[20];
int	i;

if(argc < 3)
{
	fprintf(stdout,"Usage:%s TCP,TEL,SNA,DYNMGR or messgeQueID type_of_message\n", argv[0]);
	exit(0);
}

sprintf(tempBuf, "%s", argv[1]);
for (i=0; i<strlen(tempBuf); i++) 
	tempBuf[i]=toupper(tempBuf[i]);

if(strcmp(tempBuf, "TEL") == 0)
	dyn_mgr_que = 0313L;
else if(strcmp(tempBuf, "TCP") == 0)
	dyn_mgr_que = 0323L;
else if(strcmp(tempBuf, "SNA") == 0)
	dyn_mgr_que = 1234L;
else if(strcmp(tempBuf, "DYNMGR") == 0)
	dyn_mgr_que = 5020L;
else
	{
	dyn_mgr_que = atoi(argv[1]);
	if(dyn_mgr_que == 0)
		{
		fprintf(stderr, "Invalid argument passed, <%s>\n", argv[1]);
		exit(0);
		}
	}
sprintf(tempBuf1, "%s", argv[2]);

dyn_mqid = msgget(dyn_mgr_que, PERMS);
if(dyn_mqid < 0)
	{
	fprintf(stderr, "Couldn't get message que id <%s>, errno %d\n", argv[1], errno);
	exit(0);
	}
fprintf(stderr, "About to remove messages for <%s>, que #<%x, %o>, que id %d, type=%d\n", argv[1],dyn_mgr_que, dyn_mgr_que, dyn_mqid, atoi(tempBuf1));
for(;;)
	{
	mesg2.mesg_type2 = atoi(tempBuf1);/* message of dynamic manager type */
	memset(mesg2.mesg_data2, 0, sizeof(mesg2.mesg_data2));
	ret_code = mesg_recv1(dyn_mqid, &mesg2);
	if(ret_code < 0)
		break;
	}
exit(0);
}
/*------------------------------------------------------------------------------
 This routine reads the message from the message queue 
------------------------------------------------------------------------------*/
int mesg_recv1(id, mesqptr2)
int	id;
Mesg2   *mesqptr2;
{
int	ret_code;
int	i;

memset(mesg2.mesg_data2, 0, sizeof(mesg2.mesg_data2));
ret_code = msgrcv(id, mesqptr2, 512, mesqptr2->mesg_type2, 0|IPC_NOWAIT);
if(ret_code < 0)
	{
	fprintf(stderr, "Couldn't receive message, errno %d\n", errno);
	return(ret_code);
	}
fprintf(stderr, "message is type is <%ld>\n", mesg2.mesg_type2);
fprintf(stderr, "%s\n", "message is ");
for(i = 0; i < 80; i++)
	fprintf(stderr, "%c", mesg2.mesg_data2[i]);
fprintf(stderr, "\n");
return(ret_code);
} /* mesg_recv1 */
