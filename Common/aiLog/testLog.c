#include <stdio.h>
#include <string.h>
#include "gaUtils.h"

int	Init(int arcg, char *argv[]);

#define DEBUG 

int
main(int argc, char *argv[])
{
	int	rc;
	char	mod[]="main";

	rc=testSetGet(argc, argv);

	gaVarLog(mod, 0, "this is a test 1\n");

	return 0;

}

int testSetGet(int argc, char *argv[])
{
	int	rc;
	char	programName[64];
	char	logDir[256];
	char	logBase[64];
	char	resourceName[8];
	char	debugDir[256];

	static char     mod[]="Init";
	static char     gaLogDir[256];
 
	memset((char *)programName,	0, sizeof(programName));
	memset((char *)logDir,		0, sizeof(logDir));
	memset((char *)logBase,		0, sizeof(logBase));
	memset((char *)resourceName,	0, sizeof(resourceName));
	memset((char *)debugDir,	0, sizeof(debugDir));

	sprintf(gaLogDir,"%s/LOG", (char *)getenv("ISPBASE"));

	rc=gaSetGlobalString("$PROGRAM_NAME", argv[0]);
	rc=gaSetGlobalString("$LOG_DIR", gaLogDir);
	rc=gaSetGlobalString("$LOG_BASE", argv[0]);



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
