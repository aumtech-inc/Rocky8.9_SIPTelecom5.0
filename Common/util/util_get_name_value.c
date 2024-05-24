/*------------------------------------------------------------------------------
Program:	util_get_name_value.c
Purpose:	This routine gets a name value pair from a file. If no header
		is specified, the first instance of the name found in the file
		is processed.
		This routine was adapted from gaGetProfileString, but has no
		dependencies on gaUtils.
		Parameters: 
		section: name of section in file, e.g. [Header]. If this is
			NULL (""), the whole file is searched regardless of
			headers.
		name: The "name" of a "name=value" pair.
		defaultVal: The value to return if name is not found in file.
		value: The "value" of a "name=value" pair. This what you are
			looking for in the file.
		bufSize: Maximum size of the value that can be returned. 
			This is so the routine will not cause you to core dump.
		file: the name of the file to search in
		err_msg: The errror message if any.
		Values returned are: 0 (success), -1 (failure)
Author:		George Wenzel
Date:		06/17/99
Update:	2000/10/07 sja 4 Linux: Added function prototypes
------------------------------------------------------------------------------*/
/* #define DEBUG */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

extern int	errno;

/*
 * Static Function Prototypes
 */
static int trim_whitespace(char *string);


/*-----------------------------------------------------------------------------
Get the value of a name=value pair.
-----------------------------------------------------------------------------*/
int util_get_name_value(char *section, char *name, char *defaultVal,
			char *value, int bufSize, char *file, char *err_msg)
{
	static char	mod[] = "util_get_name_value";
	FILE		*fp;
	char		line[512];
	char		currentSection[80],lhs[256], rhs[256];
	int		found, inDesiredSection=0, sectionSpecified=1;

	sprintf(value, "%s", defaultVal);
	err_msg[0]='\0';

	if(section[0] == '\0')
	{
		/* If no section specified, we're always in desired section */
		sectionSpecified=0;
		inDesiredSection=1;	
	}
	if(name[0] == '\0')
	{
		sprintf(err_msg,"No name passed to %s.", mod);
		return(-1);
	}
	if(file[0] == '\0')
	{
		sprintf(err_msg,"No file name passed to %s.", mod);
		return(-1);
	}
	if(bufSize <= 0)
	{
		sprintf(err_msg,
		"Return bufSize (%d) passed to %s must be > 0.", bufSize, mod);
		return(-1);
	}

	if((fp = fopen(file, "r")) == NULL)
	{
		sprintf(err_msg,"Failed to open file <%s>. errno=%d. [%s]",
				file, errno, strerror(errno));
		return(-1);
	}

	memset(line, 0, sizeof(line));

	found = 0;
	while(fgets(line, sizeof(line)-1, fp))
	{
		/*  Strip \n from the line if it exists */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		trim_whitespace(line);

		if(!inDesiredSection)
		{
			if(line[0] != '[')
			{
				memset(line, 0, sizeof(line));
				continue;
			}

			memset(currentSection, 0, sizeof(currentSection));
			sscanf(&line[1], "%[^]]", currentSection);

			if(strcmp(section, currentSection) != 0)
			{
				memset(line, 0, sizeof(line));
				continue;
			}
			inDesiredSection = 1;
			memset(line, 0, sizeof(line));
			continue;
		} 
		else
		{
			/* If we are already in a section and have encountered
			  another section AND a section was specified then
			   get out of the loop.  */
			if( line[0] == '[' && sectionSpecified )
			{
				memset(line, 0, sizeof(line));
				break;
			}
		} 

		memset(lhs, 0, sizeof(lhs));
		memset(rhs, 0, sizeof(rhs));

		if(sscanf(line, "%[^=]=%[^=]", lhs, rhs) < 2)
		{
			memset(line, 0, sizeof(line));
			continue;
		}
		trim_whitespace(lhs);
		if(strcmp(lhs, name) != 0)
		{
			memset(line, 0, sizeof(line));
			continue;
		}
		found = 1;
		sprintf(value, "%.*s", bufSize, rhs);
		break;
	} /* while fgets */
	fclose(fp);
	if (!found)
	{
		if (sectionSpecified)
			sprintf(err_msg,
			"Failed to find <%s> under section <%s> in file <%s>.", 
				name, section, file);
		else
			sprintf(err_msg,
			"Failed to find <%s> entry in file <%s>.", name, file);
		return(-1);
	}
	return(0);
} 

/*----------------------------------------------------------------------------
Trim white space from the front and back of a string.
----------------------------------------------------------------------------*/
static int trim_whitespace(char *string)
{
	int sLength;
	char *tmpString;
	char *ptr;

	if((sLength = strlen(string)) == 0) return(0);

	tmpString = (char *)calloc((size_t)(sLength+1), sizeof(char));
	ptr = string;

	if ( isspace(*ptr) )
	{
		while(isspace(*ptr)) ptr++;
	}

	sprintf(tmpString, "%s", ptr);
	ptr = &tmpString[(int)strlen(tmpString)-1];
	while(isspace(*ptr))
	{
		*ptr = '\0';
		ptr--;
	}

	sprintf(string, "%s", tmpString);
	free(tmpString);
	return(0);
}

#ifdef DEBUG
/*----------------------------------------------------------------------------
This main routine
----------------------------------------------------------------------------*/
main(int argc, char **argv)
{
	long bufSize=100;
	char err_msg[256];
	char value[256];
	char defaultVal[256];
	char name[80];
	char section[80];
	char file[80];
	int rc;

	strcpy(defaultVal,"default");
	while ( 1 )
	{
		printf("Value to look for : ");
		gets(name);

		printf("Filename : ");
		gets(file); 
		strcpy(file, "junk");
		
		printf("Section : ");
		gets(section);
		rc = util_get_name_value(section, name, defaultVal,
			value, bufSize, file, err_msg);
		printf("Got back: rc=%d, value=<%s>\nerr_msg=<%s>\n", 
				rc, value, err_msg);
	}
	return(0);
}
#endif
