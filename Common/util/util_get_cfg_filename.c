/*------------------------------------------------------------------------------
Program:	util_get_cfg_filename.c
Purpose:	This routine is used to get the name of a particular 
		configuration file, or indeed any file, given the unique
		string identifier for the file. This routine should be used
		by all Aumtech code so that we can eventually switch to the
		use of a defined file which gives the name of all config
		files in the system. This file will be used by arcFlash.

		Right now this file does not use the master file 
		with the names of configuration files, but it can be easily
		modified and re-linked into programs so that it can.
Author:		George Wenzel
Date:		06/17/99
------------------------------------------------------------------------------*/
/* #define DEBUG    */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int util_get_cfg_filename(token, file_name, err_msg)
char	*token;
char	*file_name;
char	*err_msg;
{
	register int i;
	int found=0;
	char *base;

	typedef struct list_of_files
	{
		char * id;
		char * name;
	}	file_struct;

	file_struct config_file[] = 	{ 
		"TEL_CONFIG",	 	"Telecom/Tables/.TEL.cfg",
		"ISDN_CONFIG",	 	"Telecom/Tables/isdncfg",
		"ANALOG_CONFIG", 	"Telecom/Tables/analogcfg",
		"CALL_CONFIG", 		"Telecom/Tables/call.cfg",
		"GLOBAL_CONFIG", 	"Global/.Global.cfg",
		"RESOURCE_CONFIG", 	"Global/Tables/ResourceMgr.cfg",
		"END_OF_LIST",	 	"",
			};

	file_name[0]='\0';
	err_msg[0]='\0';

	base = (char *)getenv("ISPBASE");
	if (base == NULL)
	{
		strcpy(err_msg, "Environment variable ISPBASE is not set.");
		return(-1);	
	}

	i=0;	
	while ( strcmp(config_file[i].id, "END_OF_LIST") != 0 )
	{
		if (strcmp(token, config_file[i].id) == 0)
		{
			found=1;
			break;
		}
		i++;
	}

	if (found == 0)
	{
		sprintf(err_msg, 
		"Could not determine file name using id <%s>.", token);
		return(-1);	

	}

	sprintf(file_name, "%s/%s", base, config_file[i].name);
	return(0);
} 

/*----------------------------------------------------------------------------
This main routine is provided for testing only 
----------------------------------------------------------------------------*/ 
#ifdef DEBUG
main(int argc, char **argv)
{
        char err_msg[256];
        char name[80];
        char token[80];
        int rc;

        while ( 1 )
        {
                printf("File id : ");
                gets(token);

		rc =util_get_cfg_filename(token, name, err_msg);
                printf("Got back: rc=%d, name=<%s>\nerr_msg=<%s>\n",
                                rc, name, err_msg);
        }
        return(0);
}
#endif

