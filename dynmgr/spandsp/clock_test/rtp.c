///This function is designed for sleeping after sending/receiving RTP packets.
int rtpSleep(int millisec, struct timeb *zLastSleepTime)
{
    int rc = -1;
    static char mod[] = "rtpSleep";
    long yIntSleepTime = 0;
    struct timeb yCurrentTime;
    long yIntStartMilliSec;
    long yIntStopMilliSec;

    ftime(&yCurrentTime);

    if(zLastSleepTime->time == 0)
    {
        util_u_sleep(millisec * 1000);
    }
    else
    {
        long current = 0;
        long last = 0;

        current =   (1000 * yCurrentTime.time)      + yCurrentTime.millitm;
        last =      (1000 * zLastSleepTime->time)   + zLastSleepTime->millitm;

        yIntSleepTime = millisec - (current - last);

        util_u_sleep( yIntSleepTime * 1000);
    }

    ftime(zLastSleepTime);
    yIntStopMilliSec = zLastSleepTime->millitm + zLastSleepTime->time*1000;
    yIntStartMilliSec = yCurrentTime.millitm + yCurrentTime.time*1000;

    if((yIntStopMilliSec - yIntStartMilliSec) > 100)
    {
        dynVarLog(__LINE__, -1, mod, REPORT_VERBOSE, 3278, INFO,
            "Sleeping for (%d ms), but slept for(%d ms)",
            millisec, yIntStopMilliSec - yIntStartMilliSec);
    }

    return 0;

}/*END: int rtpSleep() */


