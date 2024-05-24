#include <iostream>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "AppMessages.h"
#include "Telecom.h"

using namespace std;

int		yDynMgrCount;
int 	GV_PortNum = -1;
int 	GV_RequestFifoFd[12];
int 	GV_ResponseFifoFd = -1;
char 	GV_ResponseFifo[128] = "";
int		fifo_is_open = 0;
int		portNum = -1;
int		inRtpPort = -1;
int		outRtpPort = -1;
char	audioCodec[32] = "";
char	remoteIP[32] = "";
int		portBNum = -1;
int		portBInRtpPort = -1;
int		portBOutRtpPort = -1;
char	audioBCodec[32] = "";
char	remoteBIP[32] = "";
char	portBDnis[64] = "";

int 	openChannelToDynMgr(int dynMgrId);
int		closeChannelToDynMgr(int dynMgrId);
int 	getPortDetailsForAni(char *ani, int yDynMgrId);
int 	parseResponseMessage(char *Msg);
int		getDynMgrCount();
int 	readResponseFromDynMgr(char *mod, int timeout, struct MsgToApp *pMsg, int zMsgLength);
int 	startCallRecordSilently(char *zFileName);
int 	stopCallRecordSilently(char *zFileName);
int 	sendRequestToDynMgr(char *mod, int, struct MsgToDM *request);
int		getAni();
int		getOperation();
int     printCallDetails();
int     listenCall();
int     cleanUp();
int 	createCaleaFifo();

int main(int argc, char **argv)
{
	int	yRc = 0;
	int canContinue = 1;
	int yDynMgrId = -1;
	char	ani[32] = "anonymous";
	struct MsgToDM yMsgToDM;
	struct MsgToApp response;
	int opcode = -1;

	if(argc < 2)
	{
		printf("USAGE\n");fflush(stdout);
		printf("%s -a <ani> -o <operation>\n", argv[0]);
		return 0;
	}

	if(argc == 2 && strcmp(argv[1], "-v") == 0)
	{
		printf("ArcCalea: Aumtech's Call Record Utility\n");fflush(stdout);
		sleep(1);
		exit(0);
	}
	else
	if(argc == 2 && strcmp(argv[1], "-h") == 0)
	{
		printf("%s: Aumtech's Call Record Utility\n", argv[0]);fflush(stdout);
		printf("Usage:\n%s -a <ani> -o <operation>\nOperation: 1: Start recording.  2: Stop Recording.", argv[0]);
		sleep(1);
		exit(0);
	}
	else
	if(argc >= 3)
	{
		if(strcmp(argv[1], "-a") == 0)
		{
			sprintf(ani, "%s", argv[2]);
		}
		else
		{
			printf("%s: Unknown argument %s. Must be -a <ani>\n", argv[0], argv[1]);
			sleep(1);
			exit(-1);
		}

		if(argc >= 5)
		{
			if(strcmp(argv[3], "-o") == 0)
			{
				opcode =  atoi(argv[4]);
			}
			else
			{
				printf("%s: Unknown argument %s. Must be -o <operation>\n", argv[0], argv[3]);
				sleep(1);
				exit(-1);
			}
		}
	}
	else
	{
		printf("%s: Unknown argument %s\n", argv[0], argv[1]);
		sleep(1);
		exit(-1);
	}

	sprintf(GV_ResponseFifo, "/tmp/caleaAppRespFifo");

	yRc = createCaleaFifo();
	if(yRc != 0)
	{
		printf("ArcCalea: Failed to create %s EXITING \n", GV_ResponseFifo);
		exit(-1);
	}

	yDynMgrCount = getDynMgrCount();
	
	for(int i=0; i< yDynMgrCount; i++)
	{
		openChannelToDynMgr(i);
		getPortDetailsForAni(ani, i);
	}

	readResponseFromDynMgr( "", 10, &response, sizeof(response));
		
	switch(response.opcode)
	{
		case DMOP_GETPORTDETAILFORANI:
			GV_PortNum = parseResponseMessage(response.message);	
			break;
		default:
		/* Unexpected message. Logged in examineDynMgr... */
			break;
    }/* switch rc */

	//printf("ArcCalea: Got port Num = %d\n", GV_PortNum);fflush(stdout);

	switch(opcode)
	{
		case 1:
			startCallRecordSilently("");
			break;
		case 2:
			stopCallRecordSilently("");
			break;
		case 3:
        	printCallDetails();
			break;
	}

#if 0	
	if(opcode == 1)
	{
		startCallRecordSilently("");
	}
	else if (opcode == 2)
	{
		stopCallRecordSilently("");
	}
    else if (opcode == 3)
    {
        printCallDetails();
    }
#endif

	cleanUp();

	exit(0);
}

int cleanUp()
{
	for(int i=0; i<= yDynMgrCount; i++)
	{
		closeChannelToDynMgr(i);
	}

	return(0);
}


int printCallDetails()
{

    printf("PORT_NUM=%d\n"
            "IN_RTP_PORT=%d\n"
            "OUT_RTP_PORT=%d\n"
            "AUDIO_CODEC=%s\n"
            "REMOTE_IP=%s\n"
            "PORT_B_NUM=%d\n"
            "IN_B_RTP_PORT=%d\n"
            "OUT_B_RTP_PORT=%d\n"
            "AUDIO_B_CODEC=%s\n"
            "REMOTE_B_IP=%s\n"
            "B_DNIS=%s\n",
            portNum,
            inRtpPort,
            outRtpPort,
            audioCodec,
            remoteIP,
            portBNum,
            portBInRtpPort,
            portBOutRtpPort,
            audioBCodec,
            remoteBIP,
            portBDnis);

}


int getAni()
{
	char ani[32] = "";
	cout<<"Please Enter ANI :";
	cin>>ani;	
}

int getOperation()
{
	int operation;
	
	cout<<"Please Select from one of the options:"<<endl;
	cout<<"1. Start Recording the Call Silently"<<endl;
	cout<<"2. Stop Recording the Call Silently"<<endl;
	
	cin>>operation;

	return operation;
}

int startCallRecordSilently(char *zFileName)
{
	char mod[] = "startCallRecordSilently";
	struct Msg_StartRecordSilently  yMsgToDM;

	yMsgToDM.opcode = DMOP_STARTRECORDSILENTLY;
	yMsgToDM.appCallNum = GV_PortNum;
	sprintf(yMsgToDM.inFilename, "/tmp/caleaInFile_%d", GV_PortNum);
	sprintf(yMsgToDM.outFilename, "/tmp/caleaOutFile_%d", GV_PortNum);

	sendRequestToDynMgr(mod, GV_PortNum/48, (MsgToDM *)&yMsgToDM);

	if(portBNum > -1)
	{
		yMsgToDM.opcode = DMOP_STARTRECORDSILENTLY;
		yMsgToDM.appCallNum = portBNum;
		sprintf(yMsgToDM.inFilename, "/tmp/caleaInFile_%d", portBNum);
		sprintf(yMsgToDM.outFilename, "/tmp/caleaOutFile_%d", portBNum);

		sendRequestToDynMgr(mod, portBNum/48, (MsgToDM *)&yMsgToDM);
	}

}


int stopCallRecordSilently(char *zFileName)
{
	char mod[] = "startCallRecordSilently";
	struct Msg_StartRecordSilently  yMsgToDM;

	yMsgToDM.opcode = DMOP_STOPRECORDSILENTLY;
	yMsgToDM.appCallNum = GV_PortNum;

	sendRequestToDynMgr(mod, GV_PortNum/48, (MsgToDM *)&yMsgToDM);
	
	if(portBNum > -1)
	{
		yMsgToDM.opcode = DMOP_STOPRECORDSILENTLY;
		yMsgToDM.appCallNum = portBNum;

		sendRequestToDynMgr(mod, portBNum/48, (MsgToDM *)&yMsgToDM);
	}
}

int createCaleaFifo()
{
	if (!fifo_is_open)
	{
		if ((mknod(GV_ResponseFifo, S_IFIFO | 0666, 0) < 0)
			&& (errno != EEXIST))
		{
			return(-1);
		}

		if ((GV_ResponseFifoFd = open(GV_ResponseFifo, O_RDWR)) < 0)
		{
			return(-1);
		}

		fifo_is_open = 1;
	}
	return 0;
}

int readResponseFromDynMgr(char *mod, int timeout, struct MsgToApp *pMsg, int zMsgLength)
{
	int rc;
	struct MsgToApp *response;	//???
	struct pollfd pollset[1];

	memset(pMsg, 0, sizeof(struct MsgToApp));

	if (!fifo_is_open)
	{
		if ((mknod(GV_ResponseFifo, S_IFIFO | 0666, 0) < 0)
			&& (errno != EEXIST))
		{
			return(-1);
		}

		if ((GV_ResponseFifoFd = open(GV_ResponseFifo, O_RDWR)) < 0)
		{
			return(-1);
		}

		fifo_is_open = 1;
	}
		
	pollset[0].fd = GV_ResponseFifoFd;
	pollset[0].events = POLLIN;

	if (timeout > 0 || timeout == -1)
	{

		if(timeout == -1)
		{
			rc = poll(pollset, 1, 10);
		}
		else
		{
			//printf("ArcCalea: Polling on fifo(%d).\n", GV_ResponseFifoFd);fflush(stdout);
			rc = poll(pollset, 1, timeout * 1000);
		}

		if(rc < 0)
		{
			printf("ArcCalea: Polling on fifo(%d) returned(%d).\n", GV_ResponseFifoFd, rc);fflush(stdout);
			return(-1);
		}

		if(pollset[0].revents == 0)
		{
			//printf("ArcCalea: Polling on fifo(%d) returned(%d).\n", GV_ResponseFifoFd, -2);fflush(stdout);
/* Timed out waiting for data to read */
			return(-2);
		}
		else
		if(pollset[0].revents == 1)
		{
			//printf("ArcCalea: Polling on fifo(%d) found data..\n", GV_ResponseFifoFd);fflush(stdout);
/* There is data to read. */
		}
		else
		{
			printf("ArcCalea: Polling on fifo(%d) returned(%d).\n", GV_ResponseFifoFd, -1);fflush(stdout);
/* Unexpected return code from poll */
			return(-1);
		}
	}

	//printf("ArcCalea: Reading from fifo(%d).\n", GV_ResponseFifoFd);fflush(stdout);

	rc = read(GV_ResponseFifoFd, (char *)pMsg, zMsgLength);
		
	//printf("ArcCalea: Read %d bytes from fifo(%d). data=%s\n", rc, GV_ResponseFifoFd, (char *)pMsg);fflush(stdout);
		
	response=(struct MsgToApp *)pMsg;	//???

	if(rc == -1)
	{
		return(-1);
	}

	return(0);
}

int  getDynMgrCount()
{
    static  char    mod[]="getDynMgrCount";
    char    record[256];
    FILE    *fp;
    int     i;
    char    *p;
    char    *isp_base;
    char 	resource_file[128];
	int 	numResources;

    if((isp_base=(char *)getenv("ISPBASE")) == NULL)
    {
		printf("ArcCalea: Failed to get ispbase\n");fflush(stdout);
        numResources = 1;
        return(0);
    }

    sprintf(resource_file, "%s/Telecom/Tables/ResourceDefTab", isp_base);
    numResources = 0;
    i = 0;
    if((fp=fopen(resource_file, "r")) == NULL)
    {
        printf("ArcCalea: Can't open file %s. [%d, %s]  Defaulting to 48 ports.\n",
                resource_file, errno, strerror(errno));
        return(numResources);
    }

    while( fgets(record, sizeof(record), fp) != NULL)
    {
        if( (p = strstr(record, "DCHANNEL")) != (char *)NULL)
        {
            continue;
        }
        i++;
    }

    (void) fclose(fp);

    numResources = i/48;
		
	//printf("ArcCalea: Number of ports scheduled=%d, numResources=%d\n", i, numResources);fflush(stdout);

	if(i%48 == 0)
	{
    	return(numResources);
	}
	else
	{
    	return(numResources + 1);
	}

} // END: getDynMgrCount()

int parseMessage(char *portDetails)
{

}


int parseResponseMessage(char *responseMessage)
{
	FILE *fp;
	int yRc = -1;
	struct stat yStat;
	char portDetails[4096];
	char yName[256];
	char yValue[256];
	char str[512 + 1];
	char    *yPtr;
	char    yStrTokBuf[256];

	if( (yRc = stat(responseMessage, &yStat)) < 0)
    {
        return(-1);
    }

	fp = fopen(responseMessage, "r");
	if(fp == NULL)
	{
		return -1;
	}

	while(fgets(str, sizeof(str)-1, fp))
	{
		yPtr = strtok_r(str, "=", (char**)&yStrTokBuf);
        if(! yPtr)
        {
            continue;
        }

		sprintf(yName, "%s", yPtr);

        yPtr = (char *)strtok_r(NULL, "\n", (char**)&yStrTokBuf);

        if(! yPtr)
        {
            *yValue = 0;
        }
        else
        {
            sprintf(yValue, "%s", yPtr);
        }

        if(strcmp(yName, "PORT_NUM") == 0)
        {
            if(! *yValue)
            {
                portNum = -1;
            }
            else
            {
                portNum = atoi(yValue);
            }
        }
        else if(strcmp(yName, "IN_RTP_PORT") == 0)
		{
            if(! *yValue)
            {
                inRtpPort = -1;
            }
            else
            {
                inRtpPort = atoi(yValue);
            }
		}
        else if(strcmp(yName, "OUT_RTP_PORT") == 0)
		{
            if(! *yValue)
            {
                outRtpPort = -1;
            }
            else
            {
                outRtpPort = atoi(yValue);
            }
		}
        else if(strcmp(yName, "AUDIO_CODEC") == 0)
		{
            if(! *yValue)
            {
				sprintf(audioCodec, "UNKNOWN");
            }
            else
            {
                sprintf(audioCodec, "%s", yValue);
            }
		}
        else if(strcmp(yName, "REMOTE_IP") == 0)
		{
            if(! *yValue)
            {
				sprintf(remoteIP, "UNKNOWN");
            }
            else
            {
                sprintf(remoteIP, "%s", yValue);
            }
		}
        else if(strcmp(yName, "PORT_B_NUM") == 0)
		{
            if(! *yValue)
            {
				portBNum = -1;
            }
            else
            {
				portBNum = atoi(yValue);
            }
		}
        else if(strcmp(yName, "IN_B_RTP_PORT") == 0)
		{
            if(! *yValue)
            {
				portBInRtpPort = -1;
            }
            else
            {
				portBInRtpPort = atoi(yValue);
            }
		}
        else if(strcmp(yName, "OUT_B_RTP_PORT") == 0)
		{
            if(! *yValue)
            {
				portBOutRtpPort = -1;
            }
            else
            {
				portBOutRtpPort = atoi(yValue);
            }
		}
        else if(strcmp(yName, "AUDIO_B_CODEC") == 0)
		{
            if(! *yValue)
            {
				sprintf(audioBCodec, "UNKNOWN");
            }
            else
            {
                sprintf(audioBCodec, "%s", yValue);
            }
		}
        else if(strcmp(yName, "REMOTE_B_IP") == 0)
		{
            if(! *yValue)
            {
				sprintf(remoteBIP, "UNKNOWN");
            }
            else
            {
                sprintf(remoteBIP, "%s", yValue);
            }
		}
        else if(strcmp(yName, "B_DINS") == 0)
		{
            if(! *yValue)
            {
				sprintf(portBDnis, "UNKNOWN");
            }
            else
            {
                sprintf(portBDnis, "%s", yValue);
            }
		}
		
	}


//	fread(portDetails, yStat.st_size, 1, fp);

	fclose(fp);

	unlink(responseMessage);
	
	//sscanf (portDetails, "PORT_NUM=%d", &yRc);
	
	return portNum;
}

int openChannelToDynMgr(int dynMgrId)
{

	char GV_RequestFifo[256];

	sprintf(GV_RequestFifo, "/tmp/RequestFifo.%d", dynMgrId);
	
    if ( (GV_RequestFifoFd[dynMgrId] = open(GV_RequestFifo, O_WRONLY)) < 0)
    {
        return(-1);
    }

    return(0);
}

int closeChannelToDynMgr(int dynMgrId)
{
    if (GV_RequestFifoFd[dynMgrId] > 0)
    {
        close(GV_RequestFifoFd[dynMgrId]);
		GV_RequestFifoFd[dynMgrId] = -1;
    }
    return(0);
}

int sendRequestToDynMgr(char *mod, int zDynMgrId, struct MsgToDM *request)
{
	int rc;

	int	appCallNum = request->appCallNum;

    rc=write(GV_RequestFifoFd[zDynMgrId], (char *)request, sizeof(struct MsgToDM));
    if (rc == -1)
    {
        return(-1);
    }

	return 0;
}

int getPortDetailsForAni(char *ani, int yDynMgrCount)
{
	char mod[] = "getPortDetailsForAni";
    int rc;
    struct MsgToApp response;
	int canContinue = 1;
	struct MsgToDM yMsgToDM;

	yMsgToDM.opcode = DMOP_GETPORTDETAILFORANI;
	yMsgToDM.appCallNum = -1;
	sprintf(yMsgToDM.data, "%s", ani);

	
	//printf("RGDEBUG::Sending ani=%s\n", yMsgToDM.data);fflush(stdout);

	sendRequestToDynMgr(mod, yDynMgrCount, &yMsgToDM);

	return(0);

}
