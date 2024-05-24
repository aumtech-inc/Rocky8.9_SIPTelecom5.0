/*----------------------------------------------------------------------------
Program:	fax_config.c
Purpose:	This program contains a routine to get the configuration
		parameters from the Fax configuration file.

Note:		This function assumes that the fax file format is that for
		a Dialogic system. 

Author:		George Wenzel
Date:		Unknown
Update:		10/30/98 gpw added this header
Update:		02/09/99 gpw changed prototype according to design doc 1/13/98
Update:		02/09/99 gpw reads the lines according to the design document
----------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>

/*-----------------------------------------------------------------------------
Get the configuration information for outbound faxes. 
-----------------------------------------------------------------------------*/
get_fax_config(base_fax_dir, timeout, default_tries, fail_on_busy, retry_interval)
char *base_fax_dir;
int *timeout;
int *default_tries;
int *fail_on_busy; 
int *retry_interval;
{
char 	*base;
char 	table_dir[]="Telecom/Tables";
FILE 	*fp;
int 	line_no;	
int 	def_timeout=20;	
int 	def_default_tries=3;
int 	def_fail_on_busy=1;
int 	def_retry_interval=10; /* minutes */
char 	config_file[128]; 
char 	line[256]; 
char 	string[128]; 

/* Set all the defaults */
base 		= getenv("ISPBASE");
*timeout 	= def_timeout;
*default_tries 	= def_default_tries;
*fail_on_busy 	= def_fail_on_busy;
*retry_interval = def_retry_interval;	

sprintf(base_fax_dir,"%s/Fax",base);

sprintf(config_file,"%s/%s/FaxServer.cfg", base, table_dir);

#ifdef DEBUG
fprintf(stdout,"DEBUG get_fax_config: config file is %s\n", config_file); fflush(stdout);
#endif

if ((fp = fopen(config_file, "r")) == NULL)
	{
	/* all defaults are set */
	return;
	}

line_no = 1;
while (fgets(line, 256, fp) != NULL)
        {
	if ( line[0] == '#') continue;
	switch(line_no)
		{
		case 1: 
			sscanf(line,"%s", base_fax_dir);
			line_no++;
			break; 
		case 2: 	
			sscanf(line,"%s", string);
			*timeout = atoi(string);
			line_no++;
			break;
		case 3: 	
			sscanf(line,"%s", string);
			*default_tries = atoi(string);
			line_no++;
			break;
		case 4: 	
			sscanf(line,"%s", string);
			*fail_on_busy = atoi(string);
			line_no++;
			break;
		case 5: 	
			sscanf(line,"%s", string);
			*retry_interval = atoi(string);
			line_no++;
			break;
		}
	}

fclose(fp);	
return;
}
