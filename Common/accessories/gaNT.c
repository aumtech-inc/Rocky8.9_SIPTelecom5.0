/*------------------------------------------------------------------------------
File:		gaNT.c

Purpose:	Contains NT routines that were ported to UNIX for compatibility
		purposes.

Author:		Sandeep Agate

Update:		98/09/21 sja	Created the file
Update:	1999/07/29 djb	fixed gaGetProfileString - failed if no data after '='
Update:	2000/01/18 sja	Added code to ignore sections in gaGetProfileString
Update:	2001/10/02 djb	Got rid of annoying debug messages
------------------------------------------------------------------------------*/
#include <gaIncludes.h>
#include <gaUtils.h>

extern int	errno;

/*------------------------------------------------------------------------------
gaGetProfileString():
------------------------------------------------------------------------------*/
int gaGetProfileString(char *section, char *key, char *defaultVal,
				char *value, long bufSize, char *file)
{
	static char	mod[] = "gaGetProfileString";
	FILE		*fp;
	char		*ptr;
	char		line[1024];
	char		currentSection[80],lhs[512], rhs[512];
	short		found, inSection, ignoreSection;

	sprintf(value, "%s", defaultVal);

	if(key[0] == '\0')
	{
		gaVarLog(mod, 0, "NULL key");
		return(-1);
	}
	if(file[0] == '\0')
	{
		gaVarLog(mod, 0, "NULL file name");
		return(-1);
	}

	ignoreSection = 0;
	if(section[0] == '\0')
	{
		gaVarLog(mod, 1, "Ignoring sections in (%s)", file);
		ignoreSection = 1;
	}

	if(bufSize <= 0)
	{
		gaVarLog(mod, 0, "Return bufSize (%ld) must be > 0", bufSize);
		return(-1);
	}

	if((fp = fopen(file, "r")) == NULL)
	{
		gaVarLog(mod, 0,"(%s:%d) Failed to open file (%s) for reading, (%d:%s)",
				__FILE__, __LINE__, file, errno, strerror(errno));
		return(-1);
	}

	memset(line, 0, sizeof(line));

	inSection = 0;
	found = 0;
	while(gaReadLogicalLine(line, sizeof(line)-1, fp))
	{
		/*
		 * Strip \n from the line if it exists
		 */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		if(ignoreSection == 0)
		{
			if(!inSection)
			{
				if(line[0] != '[')
				{
					memset(line, 0, sizeof(line));
					continue;
				}
	
				memset(currentSection, 0, 
							sizeof(currentSection));
				sscanf(&line[1], "%[^]]", currentSection);
	
				if(strcmp(section, currentSection) != 0)
				{
					memset(line, 0, sizeof(line));
					continue;
				}
	
				inSection = 1;
				memset(line, 0, sizeof(line));
				continue;
			} /* if ! inSection */
			else
			{
				/*
			 	 * If we are already in a section and have 
				 * encountered another section, get out of 
				 * the loop.
			 	 */
				if(line[0] == '[')
				{
					memset(line, 0, sizeof(line));
					break;
				}
			} /* if inSection */
		} /* if we are NOT ignoring sections */

		memset(lhs, 0, sizeof(lhs));
		memset(rhs, 0, sizeof(rhs));

		if ((ptr=strchr(line, '=')) == NULL)
		{
			memset(line, 0, sizeof(line));
			continue;
		}

		sscanf(line, "%[^=]=%[^=]", lhs, rhs);

		gaChangeString(OP_TRIM_WHITESPACE, lhs);

		if(strcmp(lhs, key) != 0)
		{
			memset(line, 0, sizeof(line));
			continue;
		}

		found = 1;

		sprintf(value, "%.*s", bufSize, rhs);

		break;
	} /* while gaReadLine != NULL */

	fclose(fp);

	if(!found)
	{
		if(ignoreSection == 0)
		{
			gaVarLog(mod, 0,
				"Failed to find value of key(%s) under "
				"section(%s) in file(%s)", 
				key, section, file);
		}
		else
		{
			gaVarLog(mod, 0,
				"Failed to find value of key(%s) in file(%s)", 
				key, file);
		}
		return(-1);
	}

#ifdef DO_ANNOYING_DEBUG_MESSAGES
	if(ignoreSection == 0)
	{
		gaVarLog(mod, 1, 
			"Value of key(%s) under section(%s) in file(%s) is "
			"(%s)", 
			key, section, file, value);
	}
	else
	{
		gaVarLog(mod, 1, 
			"Value of key(%s) in file(%s) is (%s)", 
			key, file, value);
	}
#endif

	return(0);
} /* END gaGetProfileString() */
/*------------------------------------------------------------------------------
gaGetProfileSection(): Look for the specified section in a file and
				put the contents of this sectio in a buffer.

The returned string will look like:

"line 1\0line 2\0line 3\0....line n\0\0"
------------------------------------------------------------------------------*/
int gaGetProfileSection(char *section, char *value, long bufSize, char *file)
{
	static char	mod[] = "gaGetProfileSection";
	FILE		*fp;
	char		*ptr, line[1024];
	char		currentSection[80];
	short		found, inSection;
	long		bytesCopied, bytesToCopy;

	if(section[0] == '\0')
	{
		gaVarLog(mod, 0, "NULL section name");
		return(-1);
	}
	if(file[0] == '\0')
	{
		gaVarLog(mod, 0, "NULL file name");
		return(-1);
	}
	if(bufSize <= 0)
	{
		gaVarLog(mod, 0, "Return bufSize (%ld) must be > 0", bufSize);
		return(-1);
	}

	if((fp = fopen(file, "r")) == NULL)
	{
		gaVarLog(mod, 0, "Failed to open file(%s) for reading, (%d:%s)"
				,file, errno, strerror(errno));
		return(-1);
	}

	if((ptr = (char *)malloc(bufSize)) == NULL)
	{
		gaVarLog(mod, 0, "malloc(%ld) failed, (%d:%s)", bufSize, 
				errno, strerror(errno));
		return(-1);
	}

	memset(ptr, 0, bufSize);

	bytesCopied = 0;
	inSection = 0;
	found = 0;
	while(gaReadLogicalLine(line, sizeof(line)-1, fp))
	{
		/*
		 * Strip \n from the line if it exists
		 */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		gaChangeString(OP_TRIM_WHITESPACE, line);

		/*
		 * If we are not in a section, check to see if we find the 
		 * section that we are looking for.
		 */
		if(!inSection)
		{
			if(line[0] != '[')
			{
				continue;
			}

			memset(currentSection, 0, sizeof(currentSection));
			sscanf(&line[1], "%[^]]", currentSection);

			if(strcmp(section, currentSection) != 0)
			{
				continue;
			}

			inSection = 1;
			found = 1;
			continue;
		} /* if ! inSection */
		else
		{
			/*
			 * If we are already in a section and have encountered
			 * another section, get out of the loop.
			 */
			if(line[0] == '[')
			{
				break;
			}
		} /* if inSection */

		
		/*
		 * Calculate how many bytes we can copy
		 */
		if(((int)strlen(line) + bytesCopied) > (bufSize - 2))
		{
			bytesToCopy = (bufSize - 2) - bytesCopied;
		}
		else
		{
			bytesToCopy = strlen(line);
		}

		/*
		 * Append the current line to the ptr 
		 *
		 * The variable bytesCopied is our index into the buffer
		 */
		sprintf(&ptr[bytesCopied], "%.*s", bytesToCopy, line);

		/*
		 * Increment the number of bytes copied so far into the ptr
		 */
		bytesCopied += bytesToCopy;

		/*
		 * Skip a place - this is our \0 after each string
		 */
		bytesCopied++;

		/*
		 * If we have exhausted our buffer, append two nulls at the
		 * end and break out of the loop.
		 */
		if((bytesCopied+1) >= bufSize)
		{
			ptr[bufSize-1] = '\0';
			ptr[bufSize-2] = '\0';
			break;
		}
	} /* while gaReadLine != NULL */

	fclose(fp);

	if(!found)
	{
		free(ptr);

		gaVarLog(mod, 0, "Failed to find section (%s) in file(%s)", 
				section, file);
		return(-1);
	}

	memcpy(value, ptr, bufSize);

	free(ptr);

	return(0);
} /* END: gaGetProfileSection() */
/*------------------------------------------------------------------------------
gaGetProfileSectionNames(): Look for all section names in the given file and
				put them in a buffer.

The returned string will look like:

"section name 1\0section name 2\0section name 3\0....section name n\0\0"
------------------------------------------------------------------------------*/
int gaGetProfileSectionNames(char *value, long bufSize, char *file)
{
	static char	mod[] = "gaGetProfileSectionNames";
	FILE		*fp;
	char		*ptr, line[1024], currentSection[80];
	long		bytesCopied, bytesToCopy;

	if(bufSize <= 0)
	{
		gaVarLog(mod, 0, "Return bufSize (%ld) must be > 0", bufSize);
		return(-1);
	}
	if(file[0] == '\0')
	{
		gaVarLog(mod, 0, "NULL file name");
		return(-1);
	}

	if((fp = fopen(file, "r")) == NULL)
	{
		gaVarLog(mod, 0, "Failed to open file(%s) for reading, (%d:%s)"
				,file, errno, strerror(errno));
		return(-1);
	}

	if((ptr = (char *)malloc(bufSize)) == NULL)
	{
		gaVarLog(mod, 0, "malloc(%ld) failed, (%d:%s)", bufSize, 
				errno, strerror(errno));
		return(-1);
	}

	memset(ptr, 0, bufSize);

	bytesCopied = 0;
	while(gaReadLogicalLine(line, sizeof(line)-1, fp))
	{
		/*
		 * Strip \n from the line if it exists
		 */
		if(line[(int)strlen(line)-1] == '\n')
		{
			line[(int)strlen(line)-1] = '\0';
		}

		gaChangeString(OP_TRIM_WHITESPACE, line);

		if(line[0] != '[')
		{
			continue;
		}

		memset(currentSection, 0, sizeof(currentSection));
		sscanf(&line[1], "%[^]]", currentSection);

		/*
		 * Calculate how many bytes we can copy
		 */
		if(((int)strlen(currentSection) + bytesCopied) > (bufSize - 2))
		{
			bytesToCopy = (bufSize - 2) - bytesCopied;
		}
		else
		{
			bytesToCopy = strlen(currentSection);
		}

		/*
		 * Append the current line to the ptr 
		 *
		 * The variable bytesCopied is our index into the buffer
		 */
		sprintf(&ptr[bytesCopied], "%.*s", bytesToCopy, currentSection);

		/*
		 * Increment the number of bytes copied so far into the ptr
		 */
		bytesCopied += bytesToCopy;

		/*
		 * Skip a place - this is our \0 after each string
		 */
		bytesCopied++;

		/*
		 * If we have exhausted our buffer, append two nulls at the
		 * end and break out of the loop.
		 */
		if((bytesCopied+1) >= bufSize)
		{
			ptr[bufSize-1] = '\0';
			ptr[bufSize-2] = '\0';
			break;
		}
	} /* while gaReadLine != NULL */

	fclose(fp);

	memcpy(value, ptr, bufSize);

	free(ptr);

	return(0);
} /* END: gaGetProfileSectionNames() */
