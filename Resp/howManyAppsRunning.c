/*-----------------------------------------------------------------------------
Program:	howManyAppsRunning.c
Purpose: 	Calculate how many arcVXML2 instances are running by reading
			the shared memory..
Author:		Madhav Bhide.
Date:		06/17/04.
Update:	07/23/04 added date and bridged calsand total calls to result.

Compile/link:
        gcc  -o  howManyAppsRunning howManyAppsRunning.c       \
         -I /home/devmaul/isp2.2/SIPTelecom3.3/include            \
         -I /home/devmaul/isp2.2/SIPTelecom3.3/Common/include          \
         -L /home/devmaul/isp2.2/SIPTelecom3.3/Common/lib          \
         -lUTILS
-----------------------------------------------------------------------------*/
#include "ISPSYS.h"		/* Shared memory/Message Queue id */
#include "BNS.h"		/* shared memory variables */
#include "ArcIPResp.h"		/* global variables definations */
#include "shm_def.h"		/* shared memory field information */

/* resource status free/in use */
static	struct    resource_status	res_status[MAX_PROCS];    
int	res_attach[MAX_PROCS];

int main(int argc, char *argv[])
{
static	char	mod[] = "main";
int	i;
char	programName[50];
char	applicationName[50];
int		instances = 0;
int		bridgedPorts = 0;
int		totalPorts = 0;
int		total_resources = 0;
time_t	the_time;
char	timebuf[64];
struct tm	*PT_time;

if(argc < 2)
{
	fprintf(stdout, "Usage: %s <name of the application>\n", argv[0]);
	exit(0);
}
tran_shmid = shmget(SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);

/* attach shared memory */
if ((tran_tabl_ptr = shmat(tran_shmid, 0, 0)) == (char *) -1)
{			 /* attach the shared memory segment */
	if (errno == 22)
	{
		fprintf(stdout, "Telecom Services may not be running.\n");
		exit(0);
	}
	else
	{
		fprintf(stdout, "Couldn't attch shared memory, errno=%d.\n", errno);
		exit(-1);
	}
}

total_resources = strlen(tran_tabl_ptr)/SHMEM_REC_LENGTH;

memset(applicationName, 0, sizeof(applicationName));
sprintf(applicationName, "%s", argv[1]);

for(i = 0; i < total_resources; i++)
{
	memset(programName, 0, sizeof(programName));
	read_fld(tran_tabl_ptr, i, APPL_NAME, programName);     
	if(strncmp(programName, applicationName, strlen(applicationName)) == 0)
		instances++;
	else if(strncmp(programName, "ASSIGNED:", 9) == 0)
		bridgedPorts++;
}
shmdt(tran_tabl_ptr);		/* detach the shared memory segment */

memset((char *)timebuf, 0, sizeof(timebuf));
time(&the_time);
PT_time=localtime(&the_time);
strftime(timebuf, sizeof(timebuf) ,"%h %d %T %Y", PT_time);

totalPorts = instances + bridgedPorts;

fprintf(stdout, "%s: # of %s instances=%d. Bridged=%d. Total=%d.\n", timebuf, applicationName, instances, bridgedPorts, totalPorts);
exit(0);
} /* main */
