/*------------------------------------------------------------------------------
File:		arcLog.c

Purpose:	Routine to log messages to the ARC system log file.

Author:		Sandeep Agate, Aumtech Inc.

Update:		2002/05/23 sja 	Created the file.
------------------------------------------------------------------------------*/
#include <arcLicenseClient.h>

/*------------------------------------------------------------------------------
arcLog(): Perform all logging for the application.
------------------------------------------------------------------------------*/
int
arcLog (char *zFile, int zLine, char *zMod, int zReportMode, int zMsgId,
		char *zMsgFormat, ...)
{
	static char yMod[] = "arcLog";
	static int	yFirstTime = 1;
	va_list 	yArgPtr;
	char 		*yUserMsg;
	int			ySize;
	int			yLength;
	int			yDone;

	ySize = 2048;

	if((yUserMsg = calloc(ySize, sizeof(char))) == NULL)
	{
		fprintf(stdout, 
				"PID %d: Failed to allocate (%d) bytes for arc log message, "
				"[%d: %s]\n",
				getpid(), ySize, errno, strerror(errno));
		return(-1);
	}

	yDone = 0;
	while(! yDone)
	{
   		/* Try to print in the allocated space. */
   		va_start(yArgPtr, zMsgFormat);
   		yLength = vsnprintf (yUserMsg, ySize, zMsgFormat, yArgPtr);
   		va_end(yArgPtr);

   		/* If that worked, return the string. */
   		if (yLength > -1 && yLength < ySize)
		{
			yDone = 1;
			break;
		}

   		/* Else try again with more space. */
   		if (yLength > -1)    /* glibc 2.1 */
		{
      		ySize = yLength + 1; /* precisely what is needed */
		}
   		else           /* glibc 2.0 */
		{
      		ySize *= 2;  /* twice the old size */
		}
   		if ((yUserMsg = realloc (yUserMsg, ySize)) == NULL)
		{
			fprintf(stdout, 
					"PID %d: Failed to re-allocate (%d) bytes for arc log "
					"message, [%d: %s]\n",
					getpid(), ySize, errno, strerror(errno));
			return(-1);
		}
	} // while ! yDone

	/*
	** Strip the \n from the message if it exists.
	*/
	if (yUserMsg[strlen (yUserMsg) - 1] == '\n')
	{
		yUserMsg[strlen (yUserMsg) - 1] = '\0';
	}

	fprintf(stdout, "%d: %s: %d: %s\n",
			getpid(), zFile, zLine, yUserMsg);

	free(yUserMsg);

	return (0);

} /* END: arcLog() */


