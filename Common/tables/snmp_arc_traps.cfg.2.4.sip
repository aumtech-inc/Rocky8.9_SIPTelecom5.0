#------------------------------------------------------------------------------
# File:		snmp_arc_traps.cfg
# Purpose:	This file contains the numbers of error messages that are
#		generated from ARC system software, and identifies which of
#		those messages should generate an SNMP trap. For the messages
#		so indicated, an snmp trap will be generated each time the
#		particular message is logged, subject to the filtering criteria
#		used by the ARC snmp agent.
# Format:	<msg_id> <group> <active> <comments> 
#		where, 	msg_id is the number of the message
#			group is the ARC system, e.g., TEL, SNA, NET, etc.
#			active is either "Y" or "N"
#		Only those msg_id's for which the active field is "Y" will
#		generate traps.  Lines with "#" in column one are ignored.
# Author:	Aumtech, Inc.
# Date:		08/13/97
# Update:	<date>	<person> <description of change to this file>
# Update:	11052009 djb	Updated for SIP Telecom
#------------------------------------------------------------------------------

#
# API and Aux Messages
#
1104 TEL Y TEL_ETVSIRGO     Unable to start speech recognition
1150 TEL Y TEL_E_EXEC		Failed to start new app
1156 TEL Y TEL_E_SHMAT		Cannot attach to shmem
1157 TEL Y TEL_E_MSGGET		Cannot attach to dynamic manager message queue
#
# DynMgr & Responsibility Subsystem Messages
#
3017 TEL Y ISP_ERECV_MESG 	Unable to receive message from queue. 
3038 TEL Y ISP_EEXECL		Exec system call failed.
3039 TEL Y ISP_ESTART_NET 	Unable to start network services.
3040 TEL Y ISP_ESTART_ACCESS 	Unable to start vendor software.
3041 TEL Y ISP_EFORK		Fork system call failed.

3100 TEL Y			Unable to get environment variable OBJDB.
3101 TEL Y 			Can't get message queue.
3110 TEL Y			Error in shared memory key.

