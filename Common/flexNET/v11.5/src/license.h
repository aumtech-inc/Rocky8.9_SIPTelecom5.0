/*---------------------------------------------------------------------------
File:		license.h
Purpose:	Definitions needed for Aumtech licensing using flexlm
		licensing software.
Author:		George Wenzel
Date:		03/22/99	
---------------------------------------------------------------------------*/
/* License function prototypes */

#ifndef LICENCE_DOT_H
#define LICENCE_DOT_H


#include "lmpolicy.h"

int flexLMCheckIn(LM_HANDLE **lm_handle, char *feature, char *err_msg);
int flexLMCheckOut(char *feature, char *version, int numLicenses,
            LM_HANDLE **lm_handle, char *err_msg);
int flexLMGetNumLicenses(char *feature, char *version, 
			int *tot_lic_in_use,  int *num_lic, char *err_msg);


#endif 


