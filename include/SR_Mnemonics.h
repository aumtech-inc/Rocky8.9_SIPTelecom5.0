/*----------------------------------------------------------------------------
Program:        SR_Mnemonics.h
Purpose:        To define a set of id numbers used by send_to_monitor.
Author:         Jeff
Date:           01/23/01
Update: djb 08112005    Added TEL_SRUNLOADGRAMMAR.
----------------------------------------------------------------------------*/
#ifndef SR_MNEMONICS_H
#define SR_MNEMONICS_H

#define SR_API_BASE 1900

#define TEL_SRINIT   				SR_API_BASE+1
#define TEL_SRRESERVERESOURCE   	SR_API_BASE+2
#define TEL_SRRELEASERESOURCE   	SR_API_BASE+3
#define TEL_SRRECOGNIZE          	SR_API_BASE+4
#define TEL_SRGETRESULT          	SR_API_BASE+5
#define TEL_SRSETPARAMETERS			SR_API_BASE+6
#define TEL_SRGETPARAMETER			SR_API_BASE+7
#define TEL_SRADDWORDS				SR_API_BASE+8
#define TEL_SRDELETEWORDS			SR_API_BASE+9
#define TEL_SREXIT   				SR_API_BASE+10
#define TEL_SRLEARNWORD   			SR_API_BASE+11
#define TEL_SRLOADGRAMMAR  			SR_API_BASE+12
#define TEL_SRGETPHONETICSPELLING	SR_API_BASE+13
#define TEL_SRUNLOADGRAMMARS		SR_API_BASE+14
#define TEL_SRGETXMLRESULT  		SR_API_BASE+15
#define TEL_SRLOGEVENT  			SR_API_BASE+16
#define TEL_SRUNLOADGRAMMAR			SR_API_BASE+17
#define TEL_SRUTTERANCE				SR_API_BASE+18

#endif
