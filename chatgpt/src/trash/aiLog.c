#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

void c_aiLog (int zLine, char *zFile, int zCall, char *zMod, int zMode, int zMsgId, int zMsgType, char *zMsg)
{
    char        cMsg[512];
    char        cType[16];
    char        cPort[6];

    switch (zMsgType)
    {
        case 0:         // ERR
            sprintf(cType, "ERR: ");
            break;
        case 1:         // WARN
            sprintf(cType, "WARN: ");
            break;
        case 2:         // INFO
            sprintf(cType, "INF: ");
            break;
        default:        // INFO
            sprintf(cType, "INF: ");
            break;
    }
    sprintf(cMsg, "%s[%s, %d]%s", cType, zFile, zLine, zMsg);
    sprintf(cPort, "%d", zCall);

	LogARCMsg(zMod, zMode, cPort, "AI", "ArcAIClient.py", 823732, cMsg);
}


