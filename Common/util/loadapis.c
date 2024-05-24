/*----------------------------------------------------------------------------
Program: 	loadapis.c
Purpose:	This file contains routines to read the names of APIs from
		the API monitor data file, so that API names can be displayed
		on the ARC monitor based solely on the numbers passed to it
		via send_to_monitor API.
	
Functions:	load_arc_custom_api(err_msg)
		read_apiName(api_id, apiName)

Author:		Madhav Bhide
Date:		06/20/99
Update:		06/22/99 gpw added this header; put ModuleName in all err msgs	
Update:		07/29/99 gpw added msg parm to load_arc..., no more printfs
Update:		08/25/99 gpw put "Unknown" as default in read_apiName
Update:	2000/10/07 sja 4 Linux: Added function prototypes
----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MONITOR_DATA_FILE "sendToMonitor.dat"
#define MAX_API 200

struct arc_custom_api
{
	int api_id;
	char api_name[50];
} api_list[MAX_API+1]; 

static char *base_dir;

/*
 * Static Function Prototypes
 */
static int load_apis( char *file_path, char *err_msg);
static int load_apiname(int api_id, char *apiname, char *err_msg);
static int convert_to_upper(char *s);

/*-----------------------------------------------------------------------------
This routine dumps the contents of the loaded array to a file. It is only
for the purpose of debugging.
-----------------------------------------------------------------------------*/
int dump_api_list()
{
	int	i;
	FILE	*fp;

//	if ( access("/tmp/dump_api_list", F_OK) != 0 ) return(0);

	fp=fopen("/tmp/dump_api_list", "w");
	
	i=0;
	while(api_list[i].api_id != 0 )
	{
		fprintf(fp, "%3d %5d <%s>\n", 
				i, api_list[i].api_id, api_list[i].api_name);
		i++;
	}
	fclose(fp);
	return(0);

}

int load_arc_custom_api(char *err_msg)
{
	int rc;
	char file_path[128];
	char environ_var[]="ISPBASE";

	memset(api_list, 0, sizeof(api_list));

	base_dir = getenv(environ_var);
	if(base_dir == NULL)
	{
		strcpy(err_msg, "Failed to read environment variable ISPBASE.");
		return(-1);	
	}

	sprintf(file_path,"%s/Global/Tables/%s", base_dir, MONITOR_DATA_FILE);
	rc = load_apis(file_path, err_msg);
	dump_api_list();
	return(rc);
}


static int load_apis( char *file_path, char *err_msg)
{
FILE *fp;
char buffer[512];
char file_to_read[512];
int i, rc, api_id;
char  api_name[50];
char  api_id_str[50];
char  *ptr;

	/* If the file in not a full path, assume it is in Gt */
	ptr= strstr(file_path,"/");
	if (ptr == NULL)
	{
		sprintf(file_to_read,"%s/Global/Tables/%s",base_dir,file_path);
	}
	else
	{
		strcpy(file_to_read, file_path);
	}

	if((fp = fopen(file_to_read, "r")) == NULL)
	{
		sprintf(err_msg, 
		"Failed to open <%s>. errno=%d.", file_to_read, errno);
		return(-1);
	}

	while ( 1 )
	{
		fgets(buffer, 511, fp);
		if (feof(fp))
		{
			fclose(fp);
			break;
		}
		if (ferror(fp))
		{
			fclose(fp);
			sprintf(err_msg, "Error reading <%s>. errno=%d.", 
							file_to_read,errno);
			return(-1);
		}
		else 
		if ( (buffer[0] == '#') || (buffer[0] == '\n') ) continue; 

		sscanf(buffer, "%s %s", api_id_str, api_name); 
		convert_to_upper(api_id_str);
		if (strcmp(api_id_str,"INCLUDE") == 0)
		{
			/* Careful: Recursion here to handle include file */
			rc=load_apis(api_name, err_msg);
	/*		if (rc != 0) return(-1); */
		}
		else
		{
			rc = load_apiname(atoi(api_id_str), api_name, err_msg);
			if (rc != 0) return(-1);
		}
	} /* while */
return(0);
}

/*-----------------------------------------------------------------------------
This routine searches the array and returns the name of an API based on it's 
numeric api id.
-----------------------------------------------------------------------------*/
int read_apiName(int api_id, char *apiName)
{
	int	i;

	i=0;
	memset(apiName, 0, sizeof(apiName));
	sprintf(apiName, "Unknown:%d", api_id);
	while(api_list[i].api_id != 0 )
	{
		if(api_id == api_list[i].api_id)
		{
			sprintf(apiName, "%s", api_list[i].api_name);
			break;
		}
		i++;
	}
	return(0);
}

/*-----------------------------------------------------------------------------
Find the next available slot to load the api name into
-----------------------------------------------------------------------------*/
static int load_apiname(int api_id, char *apiname, char *err_msg) 
{
	int i, position;

	position=0;

	while (api_list[position].api_id != 0) position++;

	/* Now we have the position to load into */

	if (position >= MAX_API)
	{
		sprintf(err_msg, 
			"Failed to load <%s>. Maximum (%d) already loaded",
			apiname, MAX_API);
		return(-1);
	}

	api_list[position].api_id = api_id;	 

	sprintf(api_list[position].api_name, "%s", apiname); 
	return(0);
}

static int convert_to_upper(char *s)
{
	int i;
	
	for (i=0; i< (int)strlen(s); i++)
		s[i]=toupper(s[i]);
	return(0);
}

