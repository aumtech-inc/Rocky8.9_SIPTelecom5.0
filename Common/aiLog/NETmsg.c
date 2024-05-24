/*------------------------------------------------------------------------------
Program:	NETmsg.c
Purpose:	ISP Network message definitions 
Author: 	D. Stephens
Date:		05/16/94
 ------------------------------------------------------------------------------
 - Copyright (c) 1996, Aumtech, Inc.
 - All Rights Reserved.
 -
 - This is an unpublished work of Aumtech which is protected under 
 - the copyright laws of the United States and a trade secret
 - of Aumtech. It may not be used, copied, disclosed or transferred
 - other than in accordance with the written permission of Aumtech.
 ------------------------------------------------------------------------------
Update:		07/12/94 D. Stephens TR# 4188162100 changed order of 
Update:				msg NET_UTLTLTLOAD
Update:		05/22/96 G. Wenzel added 4009-4021
Update:		05/31/96 G. Wenzel changed NET_EASGBADSTATE 2 NET_BAD_GLOBAL_VAL
Update:		05/31/96 G. Wenzel changed NET_EASGBADGLOB 2 NET_BAD_GLOBAL
Update:		05/31/96 G. Wenzel added %s NET_BAD_GLOBAL_VAL  	
Update:		05/31/96 G. Wenzel added %s to  NET_EUACTTRNAME
Update:		05/31/96 G. Wenzel added errno to  NET_EASFSRCOPEN
Update:		05/31/96 G. Wenzel added errno to  NET_EASFSRCREAD
Update:		05/31/96 G. Wenzel added %ld (max size) to NET_EASFSRCTOOBIG 
Update:		05/31/96 G. Wenzel removed NET_EARCBBADHTO 4110 - not used
Update:		05/31/96 G. Wenzel add %d (variable max) to * NET_EARCBBADTO
Update:         06/04/96 G. Wenzel added NET_SHUTDOWN_REQUEST 
Update:		06/04/96 G. Wenzel added NET_CANT_OPEN_FIFO,NET_CANT_CREATE_FIFO
Update:         06/04/96 G. Wenzel added NET_CANT_READ_FIFO,NET_CANT_WRITE_FIFO
Update:         06/04/96 G. Wenzel removed 4230-4239, 4220-4229
Update:		06/04/96 G. Wenzel removed NET_SERV_DISP_006 (4014) not used
Update:		06/04/96 G. Wenzel changed NET_SERV_RUN_002 NET_SERV_BAD_SELECT
Update:		06/04/96 G. Wenzel changed 4170-4171 to 4035-4036
Update:		06/04/96 G. Wenzel changed 4180-4196 to 4160-4176
Update:		06/04/96 G. Wenzel changed 4200-4202 to 4075-4077
Update:		06/04/96 G. Wenzel changed 4240-4251 to 4125-4136
Update:		06/04/96 G. Wenzel changed 4260-4266 to 4100-4106
Update:		06/04/96 G. Wenzel changed 4270-4276 to 4085-4091
Update:		06/04/96 G. Wenzel changed 4300-4306 to 4065-4070
Update:		06/04/96 G. Wenzel changed 4320-4330 to 4050-4060
Update:		06/04/96 G. Wenzel removed  NET_WSRPCPROC (4325) not used 
Update:		06/10/96 G. Wenzel added NET_UNKNOWN_OBJECT
Update:		06/10/96 G. Wenzel added NET_CANT_ADD_TRAN,NET_COM_HAND_TIMEOUT
Update:		06/10/96 G. Wenzel removed  NET_DAGCHOBJTYPE (4081) not used
Update:		06/10/96 G. Wenzel added NET_CANT_SEND_DEL  4030
Update:         06/10/96 G. Wenzel added transname, max len to NET_ETRANSNAME
Update:		06/10/96 G. Wenzel changed NET_SERV_DISP_005 NET_GOT_CLIENT_REQ
Update:		06/10/96 G. Wenzel changed NET_SERV_DISP_004 NET_UNKNOWN_SERVICE
Update:		06/10/96 G. Wenzel removed  NET_ESBADOBJ not used
Update:		06/10/96 G. Wenzel removed  NET_START (4069) not used 
Update:		06/10/96 G. Wenzel removed  NET_DSOBJ (4068) not used 
Update:         06/10/96 G. Wenzel added ClientOrServer to NET_EAGCHCLSVRSW msg 
Update:		06/13/96 M. Bhide added NET_EAINPRNOST(4046)NET_EAINPRNOAV(4047)
Update:		06/13/96 S. Agate changed NET_EUACTNODATA to specify max.
Update:		06/13/96 S. Agate Added NET_EUACTNOHNDTLT
----------------------------------------------------------------------------*/
#include	"NETmsg.h"

char *GV_NETmsg[]= {

/*---------------------------------------------------------------------
	 Common Messages 			BASE=4000
---------------------------------------------------------------------*/
"Invalid length for TransName %s. Must be 1 to %d.", /* NET_ETRANSNAME */
"Bad return from user cipher %d",		/* NET_EUSRCIPH		*/
"Received shutdown message in module: %s",     	/* NET_TERMINATE     	*/
"Application received shutdown msg",		/* NET_EAPPTERMINATE 	*/
"Bad return from user compression function: %d",/* NET_EUSRCOMP      	*/
"Invalid argument combination passed to module",/* NET_EBADARGS      	*/
"Error: unsupported object %d.",		/* NET_UNKNOWN_OBJECT	*/
"",						/* 4007			*/
"",						/* 4008			*/
"Error retrieving arguments",                   /* NET_SERV_DISP_001 */
"Error sending reply to client",                /* NET_SERV_DISP_002 */
"Error freeing argument space.",                /* NET_SERV_DISP_003 */
"Server contacted for unknown service %ld.",    /* NET_UNKNOWN_SERVICE */
"Received a request from a client",             /* NET_GOT_CLIENT_REQ */
"",                     			/* 4014*/
"Create Handle Failed",                        /* NET_SERV_REG_001 */
"Failed to register service with rpc bind/portmapper.", /* NET_SERV_REG_002 */
"Started RPC Initialization",                  /* NET_SERV_REG_003 */
"Unable to determin max number of open file descriptors",/* NET_SERV_RUN_001 */
"Select() failed. errno=%d",                   /* NET_SERV_BAD_SELECT */
"Dispatch Routine is in trouble",              /* NET_SERV_RUN_003 */
"<<----- Max Network Pick ---->>",             /* NET_SERV_RUN_004 */
"",						/* 4022			*/
"Shutdown of Network Services requested.",	/* NET_SHUTDOWN_REQUEST */
"",						/* 4024			*/
"Error opening fifo <%s>. errno=%d.",		/* NET_CANT_OPEN_FIFO   */
"Error creating fifo <%s>. errno=%d.",		/* NET_CANT_CREATE_FIFO */
"Error reading fifo <%s>. errno=%d.",		/* NET_CANT_READ_FIFO 	*/
"Error writing fifo <%s>. errno=%d.",		/* NET_CANT_WRITE_FIFO 	*/
"",						/* 4029			*/
"Can't send DEL signal for transaction %s to %s. errno=%d.",
						/* NET_CANT_SEND_DEL*/
"",						/* 4031			*/
"",						/* 4032			*/
"",						/* 4033			*/
"",						/* 4034			*/
/*---------------------------------------------------------------------
	NET_SetGlobal API			BASE=4035
---------------------------------------------------------------------*/
"Invalid value %d for global %s",         	/* NET_BAD_GLOBAL_VAL  	*/
"Invalid Global passed: %s",			/* NET_BAD_GLOBAL   	*/
"",						/* 4037			*/
"",						/* 4038			*/
"",						/* 4039			*/
/*---------------------------------------------------------------------
	NET_PutCommBuffer API  			BASE=4040 
---------------------------------------------------------------------*/
"User data too long: %d",			/* NET_EAPCBTOOLONG  	*/
"",						/* 4041			*/
"",						/* 4042			*/
"",						/* 4043			*/
"",						/* 4044			*/
/*---------------------------------------------------------------------
	NET_Init API  				BASE=4045 
---------------------------------------------------------------------*/
"NET_Init function has already been called.",	/* NET_EAINOTFIRST   	*/
"Network process >%s< is not running.",		/* NET_EAINPRNOST	*/
"Failed to check process %s. errno=%d",		/* NET_EAINPRNOAV	*/
"",						/* 4048			*/
"",						/* 4049			*/
/*---------------------------------------------------------------------
	NET_op_RPC_process 			BASE=4050
---------------------------------------------------------------------*/
"Request for TransName %s from remote PID %ld, svr %s rejected by RPC server!",			/* NET_ESTRREJECT    */
"Target file %s open error",				/* NET_ESFILEOPEN    */
"File transfer request failed: no file desc. found",	/* NET_ESFTREJECT    */
"File transfer request failed during write",            /* NET_ESFTWRITE     */
"File transfer request failed during close",            /* NET_ESFTCLOSE     */
"",							/* 4055    */
"Msg placed on Q with type: %ld",			/* NET_DSMSGPUT      */
"Leftover msq deleted for PID: %ld", 			/* NET_DSMSGDEL      */
"RPC server received reg. report request",		/* NET_DSSSIORPTREQ  */
"Ping on local PID %ld by rem PID %ld, svr %s failed - RPC request rejected!",	
							/* NET_ESPIDREJECT   */
"",						/* 4060			*/
"",						/* 4061			*/
"",						/* 4062			*/
"",						/* 4063			*/
"",						/* 4064			*/
/*---------------------------------------------------------------------
	NET Svcs RPC Server			BASE=4065
---------------------------------------------------------------------*/
"Operational dispatcher returned a failure.",		/* NET_ESDISP        */
"",							/* 4066 */
"RPC Network server program #: %d, version #: %d",	/* NET_DSRPCPROG     */
 "",							/* 4068         */
"",							/* 4069         */
"RPC Unregister failed during shutdown", 		/* NET_ESRPCUNREG    */
"",						/* 4071			*/
"",						/* 4072			*/
"",						/* 4073			*/
"",						/* 4074			*/
/*---------------------------------------------------------------------
	NET_UTLCIPH utility			BASE=4075
---------------------------------------------------------------------*/
"No encryption available",			/* NET_EUCIPHNOSYS   	*/
"String before encryption/decryption: \"%s\"",  /* NET_DUCIPHSTRBEF  	*/
"String after encryption/decryption: \"%s\"",   /* NET_DUCIPHSTRAFT  	*/
"",						/* 4078			*/
"",						/* 4079			*/

/*---------------------------------------------------------------------
	NET_GetCommHandle API  			BASE=4080
---------------------------------------------------------------------*/
"Invalid Client/Server value: %d",		/* NET_EAGCHCLSVRSW 	*/
"",			 			/* 4081		  	*/
"Application must call NET_Init first",		/* NET_EAGCHINIT     	*/
"Failed to add transaction %s. errno=%d",	/* NET_CANT_ADD_TRAN	*/
"Timeout (%d seconds) expired waiting for transaction %s comm handle. errno=%d",
						/* NET_COM_HAND_TIMEOUT */
/*---------------------------------------------------------------------
	NET_UTLTLT utility			BASE=4085
---------------------------------------------------------------------*/
"Invalid Command: %d",				/* NET_EUTLTBADCOMM  	*/
"TLT entry found: Name: %s, Obj: %d",		/* NET_DUTLTFOUND    	*/
"No TLT entry found: Name: %s, Obj: %d",	/* NET_EUTLTNOTFND   	*/
"No TLT file found w/ path: %s",		/* NET_EUTLTFLNOTFND 	*/
"TLT file empty",				/* NET_EUTLTFLEMPTY  	*/
"TLT entries loaded: %d",			/* NET_DUTLTLOADED   	*/
"Another TLT entry: #%d, TN:%s, OT:%d, HN1:%s, HN2:%s, HN3:%s",/*NET_DUTLTLOAD*/
"",						/* 4092			*/
"",						/* 4093			*/
"",						/* 4094			*/
"",						/* 4095			*/
"",						/* 4096			*/
"",						/* 4097			*/
"",						/* 4098			*/
"",						/* 4099			*/
/*---------------------------------------------------------------------
	NET_UTLSTT utility                      BASE=4100

---------------------------------------------------------------------*/
"Invalid Command: %d",				/* NET_EUSTTBADCOMM  	*/
"Passed TransName is NULL",			/* NET_EUSTTTRNAME   	*/
"Passed PID is zero",				/* NET_EUSTTPID      	*/
"STT full - no more transactions",		/* NET_EUSTTFULL     	*/
"Trans succ. reg. - TransNum: %d",		/* NET_DUSTTTRNUM    	*/
"TRNAME match on TRNUM: %d",			/* NET_DUSTTTRMATCH  	*/
"no match on TRNAME: %s, TRNUM: %d",		/* NET_EUSTTTRNOMATCH	*/
"",						/* 4107			*/
"",						/* 4108			*/
"",						/* 4109			*/
/*---------------------------------------------------------------------
	NET_RecvCommBuffer API			BASE=4110 
---------------------------------------------------------------------*/
"",					 	/* 4110   	*/
"Timed out on Receive",				/* NET_EARCBTIMEOUT  	*/
"No data in msg queue",				/* NET_DARCBNOMSG    	*/
"No data in msg buffer",			/* NET_EARCBNODATA   	*/
"Packets out of sequence: CH %d, S# %d, AId %ld", /* NET_EARCBOUTSEQ   	*/
"Data received via RPC: %s",			/* NET_DARCBDATAIN   	*/
"Invalid timeout %d; must be between -1 and %d seconds.",  /* NET_EARCBBADTO */
"Host timed out on receive",                    /* NET_EARCBHOSTTO   	*/
"Host Timeout function failed",			/* NET_EARCBHOSTTOFAIL	*/
"Host Timeout function called",			/* NET_DARCBHOSTTOFUNC	*/
"HostTimeout %d should be less than Timeout %d",/* NET_EARCBHTOTOOLONG 	*/
"HostTimeout %d has -ve value",			/* NET_EARCBBADHTO	*/
"",						/* 4122			*/
"",						/* 4123			*/
"",						/* 4124			*/
/*---------------------------------------------------------------------
	NET_UTLMSGQ utility			BASE=4125
---------------------------------------------------------------------*/
"Invalid Command: %d",				/* NET_EUMSGQBADCOMM 	*/
"Invalid ObjectType: %d",			/* NET_EUMSGQBADOBJ  	*/
"Create Q failed with Qid: %d", 		/* NET_EUMSGQCREATE  	*/
"Create Q succeeded with Qid: %d", 		/* NET_DUMSGQCREATE  	*/
"Q not attached with Qid: %d",		 	/* NET_EUMSGQNOTATT  	*/
"Q not deleted with Qid: %d", 			/* NET_EUMSGQDELETE  	*/
"Attach Q failed with Qid: %d, Key: %ld", 	/* NET_EUMSGQATTACH  	*/
"Attach Q succeeded with Qid: %d", 		/* NET_DUMSGQATTACH  	*/
"Put to Q succeeded with MsgType: %ld", 	/* NET_DUMSGQPUT     	*/
"Put to Q failed with MsgType: %ld, errno: %d",	/* NET_EUMSGQPUT     	*/
"Get from Q succeeded MsgType: %ld, Ret: %d", 	/* NET_DUMSGQGET     	*/
"Get from Q failed MsgType: %ld, Ret: %d, error: %s", /* NET_EUMSGQGET 	*/
"",						/* 4137			*/
"",						/* 4138			*/
"",						/* 4139			*/
/*---------------------------------------------------------------------
	NET_SendCommBuffer API			BASE=4140
---------------------------------------------------------------------*/
"Invalid Send Control parameter value: %d.",  	/* NET_EASCBSNDCTL   	*/
"Alt host found: %s",                 		/* NET_DASCBALTHFND  	*/
"PID of send child: %d",			/* NET_DASCBSENDPID  	*/
"Send wait failed - retd PID %d, errno %d",	/* NET_EASCBWTERR    	*/
"Send wait successful: PID %d",			/* NET_DASCBWAIT     	*/
"Send wait failed - passed PID %d",		/* NET_EASCBWTFAIL   	*/
"No data in DataOut buffer",             	/* NET_EASCBNODATA   	*/
"",						/* 4147			*/
"",						/* 4148			*/
"",						/* 4149			*/

/*---------------------------------------------------------------------
	NET_SendFile API			BASE=4150
---------------------------------------------------------------------*/
"Source file name is NULL.",			/* NET_EASFSRCNULL   	*/
"Target file name is NULL.",			/* NET_EASFTGTNULL   	*/
"Source file %s not found.",			/* NET_EASFSRCNOTFND 	*/
"Read permission denied on source file: %s",	/* NET_EASFSRCBADPRM 	*/
"Problem with accessing source file %s. errno: %d",/* NET_EASFSRCERROR 	*/
"Source file %s has an invalid file type ",	/* NET_EASFSRCBADTYP 	*/
"Source file %s is too large: %ld bytes. Max is %ld.",/* NET_EASFSRCTOOBIG */
"Source file %s open error. errno=%d.",		/* NET_EASFSRCOPEN   	*/
"Source file %s read error. errno=%d.",		/* NET_EASFSRCREAD   	*/
"",						/* 4159			*/
/*---------------------------------------------------------------------
	NET_UTLACT utility			BASE=4160 
---------------------------------------------------------------------*/
"Invalid Command: %d",				/* NET_EUACTBADCOMM  	*/
"Updated comm handle count: %d",		 /* NET_DUACTCHCNT    	*/
"Invalid CommHandle: %d",			/* NET_EUACTBADCH    	*/
"CommHandle: %d out of range",			/* NET_EUACTBADCHRG  	*/
"CommHandle: %d not initialized",		/* NET_EUACTBADCHIN  	*/
"No CommHandles available",			/* NET_EUACTNOHNDLS  	*/
"Invalid TransName <%s>",			/* NET_EUACTTRNAME   	*/
"CommHandle not released - invalid state",	 /* NET_EUACTBADSESS  	*/
"UserData buffer has no data",			/* NET_EUACTNODATA   	*/
"DataOut buffer full",      			/* NET_EUACTBUFFULL  	*/
"DataIn buffer empty",      			/* NET_EUACTBUFEMPTY 	*/
"DataOut buffer empty",      			/* NET_EUACTOBUFEMPTY	*/
"UserData buffer too long (> 256)",		/* NET_EUACTTOOLONG  	*/
"Sequence number %d added for commhandle %d",   /* NET_DUACTSEQADD   	*/
"A string token \"%s\" w/ %d char was added giving %d chars", /* NET_DUACTGOODPUT  	*/
"A string token \"%s\" w/ %d chars was parsed giving %d chars", /* NET_DUACTGOODGET  	*/
"Transaction %s already in use by CommHandle %d",/* NET_EACTTRNAMEDUP 	*/
"Unable to find tran >%s< in TLT or no CommHandles available",/* NET_EUACTNOHNDTLT */
"",						/* 4178			*/
"",						/* 4179			*/

};

int GV_NET_msgnum = { sizeof(GV_NETmsg) / sizeof(GV_NETmsg[0]) };
