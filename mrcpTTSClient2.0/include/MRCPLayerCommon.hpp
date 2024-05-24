/*------------------------------------------------------------------------------
Program Name:   MRCPLayerCommon.hpp
Purpose     :   Common definitions for the common routine of the  mrcp layer
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.
------------------------------------------------------------------------------*/

class MRCPLayerCommon
{
  public:
#if 0
    ArcRTSP_Common::ArcRTSP_Common(int zPort)
    {
        return;
    }
    ~ArcRTSP_Common() { ; }
#endif

    int checkNProcessEvent(int zPort);
    int myProcessEvent(int zPort, Sptr< RtspMsg > &zEvent);

    int processMrcpRequest(int zPort,
        char *zSendMsg, char *zRecvBuf, int zRecvBufSize,
        int *zBytesRead, int *zStatusCode, int *zCauseCode);

//    void printConfigInfo(int zPort);

    int ArcRTSP_Common::readData(int zPort, Connection conn,
                size_t len, int zSec, int zMilliSec,
                Sptr< RtspMsg > &zResponse);

    int ArcRTSP_Common::readData(int zPort, Connection conn,
                size_t len, int zSec, int zMilliSec,
                Sptr< RtspMsg > &zResponse, int zDiffFlag);

    int writeData(int zPort, Connection conn, char* buffer, int len );

};


