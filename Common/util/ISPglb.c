#ident "@(#)ISPglb 96/07/01 2.2.0 Copyright 1996 Aumtech, Inc."
/*-----------------------------------------------------------------------------
Program:	ISPglb.c
Purpose:	To define some global variables.
		Anybody needing access to the global variables defined in 
		this file should include ispinc.h.
Author:		Satish Joshi
Date:		03/30/94
Update:		07/01/96 G. Wenzel updated this header 
Update:		09/01/98 mpb Changed GV_CDR_Key to 50 
-------------------------------------------------------------------------------
 * Copyright (c) 1996, Aumtech, Inc.
 * All Rights Reserved.
 *
 * This is an unpublished work of Aumtech which is protected under 
 * the copyright laws of the United States and a trade secret
 * of Aumtech. It may not be used, copied, disclosed or transferred
 * other than in accordance with the written permission of Aumtech.
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ispinc.h"

GV_Directory_type GV_Directory;		/* Base directory structure */
GV_GlobalAttr_type GV_GlobalAttr; /* Global attributes visible to all objects */
GV_GlobalConfig_type GV_GlobalConfig;
GV_SRV_GlobalConfig_type GV_SRV_GlobalConfig;
extern char	GV_CDR_Key[50];
char	GV_CDR_ServTranName[27];
char	GV_CDR_CustData1[9];
char	GV_CDR_CustData2[101];
extern char	GV_ServType[4];
int	GV_InitCalled = 0;
int	GV_EndOfCall = 0;
int	GV_HideData = 0;
char	GV_IndexDelimit = ',';
