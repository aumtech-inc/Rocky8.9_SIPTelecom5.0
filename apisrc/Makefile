#----------------------------------------------------------------------------
# Program:	Makefile
# Purpose:	Make file for building Telecom Server APIs and archiving
#		them into libISPTel.a.
# Author:	Wenzel-Joglekar
# Date:		08/28/2000
# Update:	09/13/2000 gpw added TEL_SetGlobalString.c TEL_GetGlobalString.c
# Update:	09/21/2000 gpw added TEL_DropCall
# Update:	10/03/2000 gpw added TEL_SetGlobal, TEL_GetGlobal 
# Update:	10/16/2000 gpw added TEL_BridgeCall
# Update:	10/29/2000 apj added COMINC in the include directory paths.
# Update:	10/29/2000 apj added UTLLIBPATH and UTLLIB.
# Update:	10/28/2000 gpw added TEL_PortOperation
# Update:	03/07/2001 mb  added TEL_(Un)ScheduleCall, ocmUtil, StartNewApp
# Update:	07/16/2001 apj Added TEL_LoadTags, TEL_SaveTags, 
# 					TEL_PromptAndCollect
# Update:	08/02/2001 apj Added TEL_DialExtension.
# Update:	03/30/2004 apj Added TEL_SRLogEvent.
#----------------------------------------------------------------------------
all: Always
	make -f Makefile.mrcp all
#	rm TEL_*.o
#	make -f Makefile.mrcp.globalCrossing all
#	rm TEL_*.o

clean: Always
	make -f Makefile.mrcp clean
#	make -f Makefile.mrcp.globalCrossing clean

Always:
