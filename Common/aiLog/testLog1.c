#include <stdio.h>
#include <string.h>
#include "gaUtils.h"

int	testSetGet();
int	testLoggin();

/* #define DEBUG */

main(int argc, char *argv[])
{
	int	rc;

	rc=testSetGet();
	rc=testLoggin();

}

int testLoggin()
{
	static char	mod[]="testLoggin";
	int		rc;

	rc=gaVarLog(mod, 0, "1VarLog - "
			"debug level %d", 0);
/*
	rc=gaVarLog(mod, 1, "1VarLog - debug level %d", 1);
	rc=gaVarLog(mod, 2, "1VarLog - debug level %d", 2);
	rc=gaVarLog(mod, 3, "1VarLog - debug level %d", 3);

	rc=gaVarLog(mod, 0, "2VarLog - debug level %d", 0);
	rc=gaVarLog(mod, 1, "2VarLog - debug level %d", 1);
	rc=gaVarLog(mod, 2, "2VarLog - debug level %d", 2);
	rc=gaVarLog(mod, 3, "2VarLog - debug level %d", 3);

	rc=gaVarLog(mod, 0, "1Log - debug level 0");
	rc=gaVarLog(mod, 1, "1Log - debug level 1");
	rc=gaVarLog(mod, 2, "1Log - debug level 2");
	rc=gaVarLog(mod, 3, "1Log - debug level 3");

	rc=gaVarLog(mod, 0, "2Log - debug level 0");
	rc=gaVarLog(mod, 1, "2Log - debug level 1");
	rc=gaVarLog(mod, 2, "2Log - debug level 2");
	rc=gaVarLog(mod, 3, "2Log - debug level 3");
*/
}

int testSetGet()
{
	int	rc;
	char	programName[64];
	char	logDir[256];
	char	logBase[64];
	char	resourceName[8];
	char	debugDir[256];
 
	memset((char *)programName,	0, sizeof(programName));
	memset((char *)logDir,		0, sizeof(logDir));
	memset((char *)logBase,		0, sizeof(logBase));
	memset((char *)resourceName,	0, sizeof(resourceName));
	memset((char *)debugDir,	0, sizeof(debugDir));

	if ((rc=gaSetGlobalString("$PROGRAM_NAME", "testLog")) == -1)
	{
		fprintf(stderr,"SetGlobalString($PROGRAM_NAME, testLog) failed.");
		return(-1);
	}
	if ((rc=gaSetGlobalString("$LOG_DIR", "/tmp/")) == -1)
	{
		fprintf(stderr,"SetGlobalString($PROGRAM_NAME, testLog) failed.");
		return(-1);
	}
	if ((rc=gaSetGlobalString("$LOG_BASE", "testLog")) == -1)
	{
		fprintf(stderr,"SetGlobalString($PROGRAM_NAME, testLog) failed.");
		return(-1);
	}
	if ((rc=gaSetGlobalString("$RESOURCE_NAME", "0")) == -1)
	{
		fprintf(stderr,"SetGlobalString($PROGRAM_NAME, testLog) failed.");
		return(-1);
	}
	if ((rc=gaSetGlobalString("$DEBUG_DIR", "/tmp/")) == -1)
	{
		fprintf(stderr,"SetGlobalString($PROGRAM_NAME, testLog) failed.");
		return(-1);
	}
	



	if ((rc=gaGetGlobalString("$PROGRAM_NAME", programName)) == -1)
	{
		fprintf(stderr,"GetGlobalString($PROGRAM_NAME) failed.");
		return(-1);
	}
	if ((rc=gaGetGlobalString("$LOG_DIR", logDir)) == -1)
	{
		fprintf(stderr,"GetGlobalString($PROGRAM_NAME) failed.");
		return(-1);
	}
	if ((rc=gaGetGlobalString("$LOG_BASE", logBase)) == -1)
	{
		fprintf(stderr,"GetGlobalString($PROGRAM_NAME) failed.");
		return(-1);
	}
	if ((rc=gaGetGlobalString("$RESOURCE_NAME", resourceName)) == -1)
	{
		fprintf(stderr,"GetGlobalString($PROGRAM_NAME) failed.");
		return(-1);
	}
	if ((rc=gaGetGlobalString("$DEBUG_DIR", debugDir)) == -1)
	{
		fprintf(stderr,"GetGlobalString($PROGRAM_NAME) failed.");
		return(-1);
	}
	
#ifdef DEBUG
	printf("programName:<%s>\n", programName);
	printf("logDir:<%s>\n", logDir);
	printf("logBase:<%s>\n", logBase);
	printf("resourceName:<%s>\n", resourceName);
	printf("debugDir:<%s>\n", debugDir);
#endif

	return 0;

}
