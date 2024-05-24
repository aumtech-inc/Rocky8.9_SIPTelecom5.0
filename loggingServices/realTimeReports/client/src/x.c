#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void *mainThread();

static pthread_t	processCDRThreadId;
static int	gShutdown = 0;
static int	gMainThreadLoop = 0;


/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
static void exitClient(int arg)
{

	fprintf(stderr, "Got signal - System cleanup shutting down.\n");

	gShutdown = 1;
	gMainThreadLoop = 1;
	sleep(2);

	exit(0);

} // END: exitClient


int main(int argc, char *argv[])
{
    struct sigaction    act;
	int					rc;

    signal(SIGINT, exitClient);
    signal(SIGQUIT, exitClient);
	signal(SIGHUP, exitClient);
	signal(SIGTERM, exitClient);

	if ((rc = pthread_create(&processCDRThreadId, NULL,  mainThread, NULL)) != 0)
	{
		fprintf(stderr, "[%d] pthread_create(%d) failed. rc=%d. [%d, %s] "
					"Unable to create outbound thread.", __LINE__, 
					processCDRThreadId, rc, errno, strerror(errno));
		gShutdown = 1;
		gMainThreadLoop  = 1;
	}
		
	
	while (! gShutdown )
	{
		fprintf(stderr, "%s: Herro...\n", "main");
		sleep(10);
	}

	fprintf(stderr, "%s: Exiting.\n", "main");

}

/*-----------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
void *mainThread()
{
	static char		mod[]="mainThread";
	int				rc;

	while ( ! gMainThreadLoop )
	{
		fprintf(stderr, "%s: There...\n", "thread");
		sleep(10);
	}

	fprintf(stderr, "%s: Exiting...\n", "thread");
	pthread_detach(pthread_self());
	pthread_exit(NULL);

} // END: mainThread()
