#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <Telecom.h>

LogOff() {}

NotifyCaller() {}
char		Msg[512];
static char gPort[8] = "--";
static char gProg[32] = "tts";

static void myLog(char *messageFormat, ...);

main(int argc, char *argv[])
{
	int		rc;
	char	mod[] = "main";
	char	msg[128]; 

	sprintf(msg, "hello there");

      rc = LogARCMsg(mod, 16, "0", "TEL", "l", 20043, msg);

	sprintf(msg, "again");

      rc = LogARCMsg(mod, 1, "0", "TEL", "l", 20043, msg);

    return(rc);


} // END: main

static void myLog(char *messageFormat, ...)
{
  va_list     ap;
    struct tm   *pTime;
    char        timeBuf[64];
    char        message[2048];
    time_t      myTime;
  char lMilli[10];
  struct timeb lTimeB;

    time(&myTime);

    pTime = localtime(&myTime);
    ftime(&lTimeB);
  sprintf(lMilli, ".%d", lTimeB.millitm);

    strftime(timeBuf, sizeof(timeBuf)-1, "%H:%M:%S", pTime);
    strcat(timeBuf, lMilli);

    va_start(ap, messageFormat);
    vsprintf(message, messageFormat, ap);
    va_end(ap);

    printf("%-2s|%-7s|%-12s|%-5d|%s",
        gPort, gProg, timeBuf, (int)getpid(), message);
  fflush(stdout);
}
