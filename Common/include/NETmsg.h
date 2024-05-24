/*----------------------------------------------------------------------------
Program:	NETmsg.h
Purpose:	Define Network Services message numbers
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
Update:		05/21/96 G. Wenzel added 4009 - 4021
Update:         05/31/96 G. Wenzel changed NET_EASGBADSTATE 2 NET_BAD_GLOBAL_VAL
Update:         05/31/96 G. Wenzel changed NET_EASGBADGLOB 2 NET_BAD_GLOBAL
Update:		05/31/96 G. Wenzel removed NET_EARCBBADHTO 4110  Not used
Update:		06/04/96 G. Wenzel added NET_SHUTDOWN_REQUEST
Update:		06/04/96 G. Wenzel added NET_CANT_OPEN_FIFO,NET_CANT_CREATE_FIFO
Update:		06/04/96 G. Wenzel added NET_CANT_READ_FIFO,NET_CANT_WRITE_FIFO
Update:		06/04/96 G. Wenzel removed NET_WSRPCPROC (4325) not used
Update:		06/04/96 G. Wenzel changed NET_EAPCBTOOLONG from 4100 to 4040
Update:		06/04/96 G. Wenzel changed NET_EAINOTFIRST from 4090 to 4045
Update:         06/04/96 G. Wenzel changed 4170-4171 to 4035-4036
Update:         06/04/96 G. Wenzel changed 4180-4196 to 4160-4176
Update:         06/04/96 G. Wenzel changed 4200-4202 to 4075-4077
Update:         06/04/96 G. Wenzel changed 4240-4251 to 4125-4136
Update:         06/04/96 G. Wenzel changed 4260-4266 to 4100-4106
Update:         06/04/96 G. Wenzel changed 4270-4276 to 4085-4091
Update:         06/04/96 G. Wenzel changed 4300-4306 to 4065-4070
Update:		06/04/96 G. Wenzel changed 4320-4330 to 4050-4060
Update:		06/10/96 G. Wenzel added NET_UNKNOWN_OBJECT
Update:		06/10/96 G. Wenzel added NET_CANT_ADD_TRAN, NET_COM_HAND_TIMEOUT
Update:		06/10/96 G. Wenzel removed NET_DAGCHOBJTYPE (not used )
Update:		06/10/96 G. Wenzel chngd NET_SERV_DISP_005  NET_GOT_CLIENT_REQ
Update:		06/10/96 G. Wenzel chngd NET_SERV_DISP_004  NET_UNKNOWN_SERVICE
Update:		06/10/96 G. Wenzel removed NET_ESBADOBJ (not used )
Update:		06/10/96 G. Wenzel removed NET_START  4069 (not used )
Update:		06/10/96 G. Wenzel removed NET_DSOBJ 4068 (not used )
Update: 	06/13/96 S. Agate  Added NET_EUACTNOHNDTLT
----------------------------------------------------------------------------*/
#ifndef _NETMSG_H		/* avoid multiple include problems */
#define _NETMSG_H

#define	NET_MSG_VERSION		1
#define NET_MSG_BASE		4000

/* Mnemonic name definition for Network Services  messages */

/*---------------------------------------------------------------------
         Common Messages                        BASE=4000
---------------------------------------------------------------------*/
#define NET_ETRANSNAME  	4000
#define NET_EUSRCIPH    	4001
#define NET_TERMINATE       	4002
#define NET_EAPPTERMINATE   	4003
#define NET_EUSRCOMP    	4004
#define NET_EBADARGS    	4005
#define NET_UNKNOWN_OBJECT    	4006

#define NET_SERV_DISP_001       4009
#define NET_SERV_DISP_002       4010
#define NET_SERV_DISP_003       4011
#define NET_UNKNOWN_SERVICE     4012
#define NET_GOT_CLIENT_REQ      4013
/* Unassigned			4014 */ 
#define NET_SERV_REG_001        4015
#define NET_SERV_REG_002        4016
#define NET_SERV_REG_003        4017
#define NET_SERV_RUN_001        4018
#define NET_SERV_BAD_SELECT     4019
#define NET_SERV_RUN_003        4020
#define NET_SERV_RUN_004        4021

#define NET_SHUTDOWN_REQUEST	4023

#define NET_CANT_OPEN_FIFO	4025
#define NET_CANT_CREATE_FIFO	4026
#define NET_CANT_READ_FIFO	4027
#define NET_CANT_WRITE_FIFO	4028
#define NET_CANT_SEND_DEL	4030

/*---------------------------------------------------------------------
        NET_SetGlobal API                       BASE=4035
---------------------------------------------------------------------*/
#define NET_BAD_GLOBAL_VAL	4035
#define NET_BAD_GLOBAL     	4036

/*---------------------------------------------------------------------
        NET_PutCommBuffer API                   BASE=4040
---------------------------------------------------------------------*/
#define NET_EAPCBTOOLONG    	4040

/*---------------------------------------------------------------------
        NET_Init API                            BASE=4045
---------------------------------------------------------------------*/
#define NET_EAINOTFIRST     	4045
#define NET_EAINPRNOST		4046
#define NET_EAINPRNOAV		4047

/*---------------------------------------------------------------------
        NET_op_RPC_process                      BASE=4050
---------------------------------------------------------------------*/
#define NET_ESTRREJECT      	4050
#define NET_ESFILEOPEN		4051
#define NET_ESFTREJECT      	4052
#define NET_ESFTWRITE      	4053
#define NET_ESFTCLOSE      	4054
/* not assigned			4325 */
#define NET_DSMSGPUT        	4056
#define NET_DSMSGDEL        	4057
#define NET_DSSSIORPTREQ    	4058
#define NET_ESPIDREJECT		4059

/*---------------------------------------------------------------------
        NET Svcs RPC Server                     BASE=4065
---------------------------------------------------------------------*/
#define NET_ESDISP           	4065
/* not assigned         	4066 */
#define NET_DSRPCPROG       	4067
/* not assigned          	4068
/* not assigned      		4069 */
#define NET_ESRPCUNREG      	4070

/*---------------------------------------------------------------------
        NET_UTLCIPH utility                     BASE=4075
---------------------------------------------------------------------*/
#define NET_EUCIPHNOSYS      	4075
#define NET_DUCIPHSTRBEF    	4076
#define NET_DUCIPHSTRAFT    	4077

/*---------------------------------------------------------------------
        NET_GetCommHandle API                   BASE=4080
---------------------------------------------------------------------*/
#define NET_EAGCHCLSVRSW    	4080
/* Not assigned 		4081 */
#define NET_EAGCHINIT       	4082
#define NET_CANT_ADD_TRAN      	4083
#define NET_COM_HAND_TIMEOUT	4084

/*---------------------------------------------------------------------
        NET_UTLTLT utility                      BASE=4085
---------------------------------------------------------------------*/
#define NET_EUTLTBADCOMM     	4085
#define NET_DUTLTFOUND       	4086
#define NET_EUTLTNOTFND      	4087
#define NET_EUTLTFLNOTFND    	4088
#define NET_EUTLTFLEMPTY     	4089
#define NET_DUTLTLOADED      	4090
#define NET_DUTLTLOAD       	4091

/*---------------------------------------------------------------------
        NET_UTLSTT utility                      BASE=4100
---------------------------------------------------------------------*/
#define NET_EUSTTBADCOMM     	4100
#define NET_EUSTTTRNAME      	4101
#define NET_EUSTTPID         	4102
#define NET_EUSTTFULL        	4103
#define NET_DUSTTTRNUM       	4104
#define NET_DUSTTTRMATCH       	4105
#define NET_EUSTTTRNOMATCH   	4106

/*---------------------------------------------------------------------
        NET_RecvCommBuffer API                  BASE=4110
---------------------------------------------------------------------*/
#define NET_EARCBTIMEOUT    	4111
#define NET_DARCBNOMSG      	4112
#define NET_EARCBNODATA     	4113
#define NET_EARCBOUTSEQ     	4114
#define NET_DARCBDATAIN     	4115
#define NET_EARCBBADTO  	4116
#define NET_EARCBHOSTTO     	4117
#define NET_EARCBHOSTTOFAIL 	4118
#define NET_DARCBHOSTTOFUNC 	4119
#define NET_EARCBHTOTOOLONG	4120
#define NET_EARCBBADHTO		4121

/*---------------------------------------------------------------------
        NET_UTLMSGQ utility                     BASE=4125
---------------------------------------------------------------------*/
#define NET_EUMSGQBADCOMM    	4125
#define NET_EUMSGQBADOBJ     	4126
#define NET_EUMSGQCREATE     	4127
#define NET_DUMSGQCREATE     	4128
#define NET_EUMSGQNOTATT     	4129
#define NET_EUMSGQDELETE     	4130
#define NET_EUMSGQATTACH     	4131
#define NET_DUMSGQATTACH     	4132
#define NET_DUMSGQPUT        	4133
#define NET_EUMSGQPUT        	4134
#define NET_DUMSGQGET        	4135
#define NET_EUMSGQGET        	4136

/*---------------------------------------------------------------------
        NET_SendCommBuffer API                  BASE=4140
---------------------------------------------------------------------*/
#define NET_EASCBSNDCTL     	4140
#define NET_DASCBALTHFND   	4141
#define NET_DASCBSENDPID    	4142
#define NET_EASCBWTERR      	4143
#define NET_DASCBWAIT       	4144
#define NET_EASCBWTFAIL     	4145
#define NET_EASCBNODATA     	4146

/*---------------------------------------------------------------------
        NET_SendFile API                        BASE=4150
---------------------------------------------------------------------*/
#define NET_EASFSRCNULL     	4150
#define NET_EASFTGTNULL     	4151
#define NET_EASFSRCNOTFND   	4152
#define NET_EASFSRCBADPRM   	4153
#define NET_EASFSRCERROR    	4154
#define NET_EASFSRCBADTYP   	4155
#define NET_EASFSRCTOOBIG   	4156
#define NET_EASFSRCOPEN		4157
#define NET_EASFSRCREAD		4158

/*---------------------------------------------------------------------
        NET_UTLACT utility                      BASE=4160
---------------------------------------------------------------------*/
#define NET_EUACTBADCOMM    	4160
#define NET_DUACTCHCNT      	4161
#define NET_EUACTBADCH      	4162
#define NET_EUACTBADCHRG    	4163
#define NET_EUACTBADCHIN    	4164
#define NET_EUACTNOHNDLS    	4165
#define NET_EUACTTRNAME     	4166
#define NET_EUACTBADSESS    	4167
#define NET_EUACTNODATA     	4168
#define NET_EUACTBUFFULL    	4169
#define NET_EUACTBUFEMPTY   	4170
#define NET_EUACTOBUFEMPTY   	4171
#define NET_EUACTTOOLONG     	4172
#define NET_DUACTSEQADD     	4173
#define NET_DUACTGOODPUT    	4174
#define NET_DUACTGOODGET    	4175
#define NET_EUACTTRNAMEDUP  	4176
#define NET_EUACTNOHNDTLT    	4177

extern char *GV_NETmsg[];
extern int GV_NET_msgnum;
#endif	/* _NETMSG_H */
