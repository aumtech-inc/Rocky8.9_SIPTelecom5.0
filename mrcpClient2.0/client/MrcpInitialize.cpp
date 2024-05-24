/*------------------------------------------------------------------------------
Program Name:   MrcpInitialize.cpp
Author      :   Aumtech, Inc.
Purpose     :   Handle the processing of the arcMRCP2.cfg file.
Update      :   07/19/06 djb Created the file.
Update      :   06/06/06 djb Added transportLayer logic.
Update      :   10/23/09 djb  MR 2120 - Modified readCfg()
Update      :   08/05/10 djb  MR 3052 - Added pid initialization logic
Update: 08/20/14 djb MR 4366 - changed 5 minute disable time to 10 seconds.
Update: 08/22/2014 djb MR 4241. changed disable timeout from 10 to 5 seconds
------------------------------------------------------------------------------*/

#include <iostream>

#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"
#include <string>
#include <pthread.h>

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>
	#include <errno.h>
	#include "arcSR.h"
}

using namespace std;
using namespace mrcp2client;


//=============================
// class MrcpInitialize
//=============================
// --- constructor --- 
MrcpInitialize::MrcpInitialize()
{
	mrcpCfgInfo.sipVersion		= "";
	mrcpCfgInfo.mrcpVersion		= "";

	mrcpCfgInfo.serverInfo[SR].sipPort			= 5060;
	mrcpCfgInfo.serverInfo[SR].transportLayer	= "";
    mrcpCfgInfo.serverInfo[SR].numUris         = 0;
    mrcpCfgInfo.serverInfo[SR].uriIndex        = 0;
	mrcpCfgInfo.serverInfo[SR].via				= "";
	mrcpCfgInfo.serverInfo[SR].payloadType		= "";

    mrcpCfgInfo.serverInfo[SR].uri[0].serverIP     = "";
    mrcpCfgInfo.serverInfo[SR].uri[0].requestURI   = "";
    mrcpCfgInfo.serverInfo[SR].uri[0].inService    = 0;


	mrcpCfgInfo.serverInfo[TTS].sipPort		= 5070;
	mrcpCfgInfo.serverInfo[TTS].via			= "";

	mrcpCfgInfo.serverInfo[TTS].payloadType	= "";

	tooManyFailures_shutErDown 		= false;

	pthread_mutex_init(&uriInfoLock, NULL);
	pthread_mutex_init(&uriIndexLock, NULL);
}

// --- destructor --- 
MrcpInitialize::~MrcpInitialize()
{
	pthread_mutex_destroy(&uriInfoLock);
	pthread_mutex_destroy(&uriIndexLock);
}

void MrcpInitialize::setMrcpFifoDir (string zMrcpFifoDir)
{
	mrcpFifoDir = zMrcpFifoDir;
}

void MrcpInitialize::setDynmgrFifoDir (string zDynmgrFifoDir)
{
	dynmgrFifoDir = zDynmgrFifoDir;
}

void MrcpInitialize::setApplFifoDir (string zApplFifoDir)
{
	applFifoDir = zApplFifoDir;
}

#ifdef EXOSIP_4
void MrcpInitialize::setEx4Context(eXosip_t *zexcontext)
{
    excontext = zexcontext;
}
#endif // EXOSIP_4

void MrcpInitialize::disableServer(int zLine, int zPort, string xRequestURI)
{
	int		i;
	struct UriInfo	tmpUriInfo;

	tmpUriInfo.inService = 0;
	time(&(tmpUriInfo.lastFailureTime));
	tmpUriInfo.serverIP.assign("-1");
	tmpUriInfo.requestURI.assign("-1");

	for (i = 0; i < mrcpCfgInfo.serverInfo[SR].numUris; i++)
	{
		if ( xRequestURI == mrcpCfgInfo.serverInfo[SR].uri[i].requestURI )
		{
			setUriInfo(zPort, i, &tmpUriInfo);

			mrcpClient2Log(__FILE__, __LINE__, zPort, "disableServer",
				REPORT_DETAIL, MRCP_2_BASE, WARN, 
				"[from %d] MRCP SR Server (%s) is disabled.", zLine, xRequestURI.c_str());
			return;
		}
	}
	mrcpClient2Log(__FILE__, __LINE__, zPort, "disableServer",
			REPORT_DETAIL, MRCP_2_BASE, WARN, 
			"[from %d] Unable to disable MRCP SR Server (%s).  Not found.",
			zLine, xRequestURI.c_str());

} // END: disableServer()

void MrcpInitialize::enableServer(int zLine, int zPort, string xRequestURI)
{
    int     i;
	struct UriInfo	tmpUriInfo;

	tmpUriInfo.inService = 1;
	tmpUriInfo.lastFailureTime = 0;
	tmpUriInfo.serverIP.assign("-1");
	tmpUriInfo.requestURI.assign("-1");

    for (i = 0; i < mrcpCfgInfo.serverInfo[SR].numUris; i++)
    {
        if ( xRequestURI == mrcpCfgInfo.serverInfo[SR].uri[i].requestURI )
        {
			setUriInfo(zPort, i, &tmpUriInfo);

            mrcpClient2Log(__FILE__, __LINE__, zPort, "enableServer",
                REPORT_VERBOSE, MRCP_2_BASE, INFO,
                "[from %d] MRCP SR Server (%s) is enabled.", zLine, xRequestURI.c_str());
            return;
        }
    }
    mrcpClient2Log(__FILE__, __LINE__, zPort, "enableServer",
            REPORT_NORMAL, MRCP_2_BASE, WARN,
            "[from %d] Unable to enable MRCP SR Server (%s).  Not found.",
            zLine, xRequestURI.c_str());

} // END: enableServer()

void MrcpInitialize::setUriInfo (int xPort, int xIndex, struct UriInfo *xUriInfo)
{
	static char		mod[]="setUriInfo";

	if ( ( xIndex < 0 ) || ( xIndex >= mrcpCfgInfo.serverInfo[SR].numUris ) )
	{
    	mrcpClient2Log(__FILE__, __LINE__, xPort, mod,
			REPORT_NORMAL, MRCP_2_BASE, ERR, "Invalid index received (%d).  Must be between 0 and %d.",
			xIndex, mrcpCfgInfo.serverInfo[SR].numUris - 1);	
		return;
	}
	
    pthread_mutex_lock(&uriInfoLock);

	mrcpClient2Log(__FILE__, __LINE__, -1, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Setting UriInfo(%d, %ld, %s, %s)", xUriInfo->inService,
		xUriInfo->lastFailureTime, xUriInfo->serverIP.c_str(), xUriInfo->requestURI.c_str());
	// 
	// only set values which are not -1
	//
	if ( xUriInfo->inService != -1 )
	{
		mrcpCfgInfo.serverInfo[SR].uri[xIndex].inService = xUriInfo->inService;
	}

	if ( xUriInfo->lastFailureTime != -1 )
	{
		mrcpCfgInfo.serverInfo[SR].uri[xIndex].lastFailureTime = xUriInfo->lastFailureTime;
	}

#if 0
	if( xUriInfo->serverIP.compare("-1") != 0)
	{
		mrcpCfgInfo.serverInfo[SR].uri[xIndex].serverIP.assign(xUriInfo->serverIP);
	}

	if( xUriInfo->requestURI.compare("-1") != 0)
	{
		mrcpCfgInfo.serverInfo[SR].uri[xIndex].requestURI.assign(xUriInfo->requestURI);
	}
#endif
    pthread_mutex_unlock(&uriInfoLock);

} // END: setUriInfo()

int MrcpInitialize::readCfg() 
{
	char		yMod[]="readCfg";
	char		yConfigFile[256];
	char		*ispbase;
	FILE		*pFp;
	int			i;
	int			yRc;
	char		yTmpBuf[128];
	char		yServiceStr[8];
	int			numURIs;
	int			clientId;

	/* Get config file name */
	if((ispbase = (char *)getenv("ISPBASE")) == NULL)
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Env. var. ISPBASE is not set.  Unable to continue.");
		return(-1);
	}

	sprintf(yConfigFile, "%s/Telecom/Tables/arcMRCP2.cfg", ispbase);

	if ((pFp = fopen(yConfigFile,"r")) == NULL )
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Failed to open config file (%s) for reading, [%d, %s].",
				yConfigFile, errno, strerror(errno));
		return(-1);
	}
	mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"Successfully opened config file (%s) for reading.",
				yConfigFile);
	mrcpCfgInfo.cfgFile = yConfigFile;
	/*********************** Debug Info **************************/

	yRc = myGetProfileString("MRCP", "sipVersion", "SIP/2.0", 
			yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	mrcpCfgInfo.sipVersion = yTmpBuf;

	yRc = myGetProfileString("MRCP", "mrcpVersion", "MRCP/2.0", 
			yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	mrcpCfgInfo.mrcpVersion = yTmpBuf;

	memset((char *)yTmpBuf, '\0', sizeof(yTmpBuf));
	yRc = myGetProfileString("MRCP", "localIP", "", 
			yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	mrcpCfgInfo.localIP = yTmpBuf;

	i=SR;
	sprintf(yServiceStr, "%s", "MRCP-SR");
	yRc = myGetProfileString(yServiceStr, "sipPort", "5070", 
				yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	mrcpCfgInfo.serverInfo[i].sipPort = atoi(yTmpBuf);

	yRc = myGetProfileString(yServiceStr, "transportLayer", "",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	mrcpCfgInfo.serverInfo[i].transportLayer = yTmpBuf;
	
	yRc = myGetProfileString(yServiceStr, "requestURI", "",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	yRc = populateUris(i, yTmpBuf);

	numURIs = gMrcpInit.getNumURIs();
	clientId = gMrcpInit.getClientId();
	if ( numURIs > 1 )
	{
		if ( clientId % 2 == 0 )
		{
			mrcpCfgInfo.serverInfo[SR].uriIndex = 0;
		}
		else
		{
			mrcpCfgInfo.serverInfo[SR].uriIndex = 1;
		}
	}
	mrcpClient2Log(__FILE__, __LINE__, -1, "readCfg", 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"numURIs=%d, clientId=%d.  Set the uniIndex to %d.",
		numURIs, clientId, mrcpCfgInfo.serverInfo[SR].uriIndex);
	
	yRc = myGetProfileString(yServiceStr, "via", "", 
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	if ( ( strcmp( yTmpBuf, "NOTUSED" ) == 0 ) ||
	     ( yRc != 0 ) )
	{
		memset((char *)yTmpBuf, '\0', sizeof(yTmpBuf));
	}
	mrcpCfgInfo.serverInfo[i].via = yTmpBuf;
	
	yRc = myGetProfileString(yServiceStr, "payloadType", "96",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	mrcpCfgInfo.serverInfo[i].payloadType = yTmpBuf;
	
	yRc = myGetProfileString(yServiceStr, "codecType", "8",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	mrcpCfgInfo.serverInfo[i].codecType = yTmpBuf;
	
	yRc = myGetProfileString(yServiceStr, "waveform-URI", "0",
		yTmpBuf, sizeof(yTmpBuf) - 1, pFp, yConfigFile);
	mrcpCfgInfo.serverInfo[i].waveformURI = atoi(yTmpBuf);

	fclose(pFp);

	if ( sVerifyConfig(yConfigFile) == -1 )
	{
		return(-1);
	}
	
	return(0);
} // END: readCfg

// -- gets
int MrcpInitialize::getNumURIs() const
{
    return(mrcpCfgInfo.serverInfo[SR].numUris);
} // END: getNumURIs()

int MrcpInitialize::getServerUri (int zPort, int zType, int zStartingIndex,
			string &requestURI, string &serverIP)
{
	static char		mod[] = "getServerUri";
	int				i;
	int				rc;
	int				counter;
	time_t			currentTime;
	int				inService;
	struct UriInfo	tmpUriInfo;

	if ( zStartingIndex != -1 )
	{
		i = zStartingIndex;
	}
	else
	{
		pthread_mutex_lock(&uriIndexLock);
		i = mrcpCfgInfo.serverInfo[SR].uriIndex;
		pthread_mutex_unlock(&uriIndexLock);
	}

	counter=0;
	for (;;)
	{
		if ( counter >=  mrcpCfgInfo.serverInfo[SR].numUris )
		{
			break;
		}
		counter++;

		if ( (rc = getUriInfo(zPort, i, &tmpUriInfo)) == -1)
		{
			return(-1);
		}
		inService = tmpUriInfo.inService;

		if ( ! inService )
		{
			time(&currentTime);

			// hardcoded to 5 minutes.
			// MR 4366 - changed 300 seconds to 10 seconds
			if ( ( tmpUriInfo.lastFailureTime + 10 ) < currentTime )
			{
				enableServer(__LINE__, zPort, tmpUriInfo.requestURI);

				mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO,
					"Retry time expired: setting previously disabled "
					"URI(%s) is now enabled.", 
					tmpUriInfo.requestURI.c_str());
			}
		}

		if ( mrcpCfgInfo.serverInfo[SR].uri[i].inService )
		{
			requestURI.assign(tmpUriInfo.requestURI);
			serverIP.assign(tmpUriInfo.serverIP);

    		pthread_mutex_lock(&uriIndexLock);
			if ( (i + 1) >= mrcpCfgInfo.serverInfo[SR].numUris )
			{
				mrcpCfgInfo.serverInfo[SR].uriIndex = 0;
			}
			else
			{
				mrcpCfgInfo.serverInfo[SR].uriIndex = i + 1;
			}
		    pthread_mutex_unlock(&uriIndexLock);

			return(i);
		}

#if 0
			else
			{
				mrcpClient2Log(__FILE__, __LINE__, zPort, mod, 
					REPORT_VERBOSE, MRCP_2_BASE, INFO,
					"URI(%d:%s) is currently disabled. Skipping.",
					i, tmpUriInfo.requestURI.c_str());
				continue;
			}
		}
		else
		{
			requestURI.assign(tmpUriInfo.requestURI);
			serverIP.assign(tmpUriInfo.serverIP);
			return(i);
		}

#endif
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod,
			REPORT_DETAIL, MRCP_2_BASE, WARN, 
			"MRCP SR Server (%d:%s) is not in service; skipping.",
			i, tmpUriInfo.requestURI.c_str());
		if ( (i + 1) >= mrcpCfgInfo.serverInfo[SR].numUris )
		{
			i = 0;
		}
		else
		{
			i += 1;
		}

	}
	return(-1);

} // END: getServerUri()

string MrcpInitialize::getVia (int zType) const
{
    if ( zType == SR )
    {
        return mrcpCfgInfo.serverInfo[SR].via;
    }
    if ( zType == TTS )
    {
        return mrcpCfgInfo.serverInfo[TTS].via;
    }

    mrcpClient2Log(__FILE__, __LINE__, -1, "getVia",
                    REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Invalid type (%d) received.  Must be "
                    "either SR (%d) or TTS (%d).", zType, SR, TTS);
    return "";

} // END: getVia()

string MrcpInitialize::getCfgFile () const
{
	return mrcpCfgInfo.cfgFile;
} // END: getCfgFile() const

string MrcpInitialize::getSipVersion () const
{
	return mrcpCfgInfo.sipVersion;
} // END: getSipVersion() const

string MrcpInitialize::getMrcpVersion () const
{
	return mrcpCfgInfo.mrcpVersion;
} // END: getMrcpVersion()

string MrcpInitialize::getLocalIP () const
{
	return mrcpCfgInfo.localIP;
} // END: getLocalIP()

int MrcpInitialize::getSipPort (int zType) const
{
//#ifdef SDM
	return mrcpCfgInfo.serverInfo[zType].sipPort + clientId;/*DDN: Added for multiple clients*/
//#endif


#if 0
	return mrcpCfgInfo.serverInfo[zType].sipPort;
#endif

} // END: getSipVersion() const

string MrcpInitialize::getTransportLayer (int zType) const
{
    if ( zType == SR )
    {
        return mrcpCfgInfo.serverInfo[zType].transportLayer;
    }

    return "";

} // END: getTransportLayer()

string MrcpInitialize::getPayloadType (int zType) const
{
    if ( zType == SR )
    {
        return mrcpCfgInfo.serverInfo[zType].payloadType;
    }
    if ( zType == TTS )
    {
        return mrcpCfgInfo.serverInfo[zType].payloadType;
    }

    mrcpClient2Log(__FILE__, __LINE__, -1, "getPayloadType",
                    REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Invalid type (%d) received.  Must be "
                    "either SR (%d) or TTS (%d).", zType, SR, TTS);
    return "";

} // END: getPayloadType()

string MrcpInitialize::getCodecType (int zType) const
{
    if ( zType == SR )
    {
        return mrcpCfgInfo.serverInfo[zType].codecType;
    }
    if ( zType == TTS )
    {
        return mrcpCfgInfo.serverInfo[zType].codecType;
    }

    mrcpClient2Log(__FILE__, __LINE__, -1, "getCodecType",
                    REPORT_NORMAL, MRCP_2_BASE, ERR,
                    "Invalid type (%d) received.  Must be "
                    "either SR (%d) or TTS (%d).", zType, SR, TTS);
    return "";

} // END: getCodecType()

int MrcpInitialize::getwaveformURI () const
{
    return mrcpCfgInfo.serverInfo[SR].waveformURI;
} // END: getCodecType()

string  MrcpInitialize::getMrcpFifoDir() const
{
	return mrcpFifoDir;
} // END: getMrcpFifoDir()

string  MrcpInitialize::getDynmgrFifoDir() const
{
	return dynmgrFifoDir;
} // END: getMrcpFifoDir()

string  MrcpInitialize::getApplFifoDir() const
{
	return applFifoDir;
} // END: getMrcpFifoDir()

int MrcpInitialize::getUriInfo (int xPort, int xIndex, struct UriInfo *xUriInfo)
{
	static char		mod[]="getUriInfo";

	if ( ( xIndex < 0 ) || ( xIndex >= mrcpCfgInfo.serverInfo[SR].numUris ) )
	{
    	mrcpClient2Log(__FILE__, __LINE__, xPort, "getUriIndex",
			REPORT_NORMAL, MRCP_2_BASE, ERR, "Invalid index received (%d).  Must be between 0 and %d.",
			xIndex, mrcpCfgInfo.serverInfo[SR].numUris - 1);	
		return(-1);
	}
	
    pthread_mutex_lock(&uriInfoLock);

	xUriInfo->inService = mrcpCfgInfo.serverInfo[SR].uri[xIndex].inService;
	xUriInfo->lastFailureTime = mrcpCfgInfo.serverInfo[SR].uri[xIndex].lastFailureTime;
	xUriInfo->serverIP = mrcpCfgInfo.serverInfo[SR].uri[xIndex].serverIP;
	xUriInfo->requestURI = mrcpCfgInfo.serverInfo[SR].uri[xIndex].requestURI;

	mrcpClient2Log(__FILE__, __LINE__, xPort, mod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Retrieved UriInfo[%d].(%d, %ld, %s, %s)", xIndex,
		xUriInfo->inService, xUriInfo->lastFailureTime,
		xUriInfo->serverIP.c_str(), xUriInfo->requestURI.c_str());

    pthread_mutex_unlock(&uriInfoLock);

	return(0);
} // END: getUriIndex()

#ifdef EXOSIP_4
struct eXosip_t* MrcpInitialize::getExcontext()
{
    return excontext;
}
#endif // EXOSIP_4

string  MrcpInitialize::getMrcpFifo() const
{
	return mrcpFifo;
} // END: getMrcpFifo()

/*------------------------------------------------------------------------------
sVerifyConfig():
------------------------------------------------------------------------------*/
int MrcpInitialize::sVerifyConfig(char *zConfigFile)
{
	char	yMod[]="sVerifyConfig";
	int		i;

	if ( mrcpCfgInfo.localIP.empty() )
	{
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Value for (MRCP:localIP) not specified in config file (%s).  "
				"Correct and retry.", zConfigFile);
		return(-1);
	}
	for (i=SR; i<=SR; i++)
	{

		if ( mrcpCfgInfo.serverInfo[i].numUris <= 0 )
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Value for (SR:%s) not specified in config file (%s).",
				"requestURI", zConfigFile);
			return(-1);
		}

		if ( mrcpCfgInfo.serverInfo[i].uri[0].requestURI.empty() )
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Value for (%s:%s) not specified in config file (%s).",
				"SR", "requestURI", zConfigFile);
			return(-1);
		}

		if ( mrcpCfgInfo.serverInfo[i].uri[0].serverIP.empty() )
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
				"Value for (%s:%s) not specified in config file (%s).",
				"SR", "requestURI", zConfigFile);
			return(-1);
		}

		if ( mrcpCfgInfo.serverInfo[i].payloadType.empty() )
		{
			
			mrcpCfgInfo.serverInfo[i].payloadType="96";
            mrcpClient2Log(__FILE__, __LINE__, -1, yMod,
                REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Warning: Value for (%s:%s) not specified in config file "
				"(%s).  Defaulting to (%s). ", "SR", "payloadType",
				zConfigFile, mrcpCfgInfo.serverInfo[i].payloadType.c_str());
		}

        if ( mrcpCfgInfo.serverInfo[i].transportLayer.empty() )
        {
            mrcpCfgInfo.serverInfo[i].transportLayer="TCP";
            mrcpClient2Log(__FILE__, __LINE__, -1, yMod,
                REPORT_DETAIL, MRCP_2_BASE, WARN,
                "Warning: Value for (%s:%s) not specified in config file "
                "(%s).  Defaulting to (%s). ", "SR", "transportLayer",
                zConfigFile, mrcpCfgInfo.serverInfo[i].transportLayer.c_str());
        }
        if ((strcmp(mrcpCfgInfo.serverInfo[i].transportLayer.c_str(), "TCP")) &&
            (strcmp(mrcpCfgInfo.serverInfo[i].transportLayer.c_str(),"UDP")))
        {
            mrcpCfgInfo.serverInfo[i].transportLayer="TCP";
            mrcpClient2Log(__FILE__, __LINE__, -1, yMod,
                REPORT_DETAIL, MRCP_2_BASE, WARN,
                "Warning: Invalid value (%s) for (%s:%s) not specified in "
                "config file (%s).  Defaulting to (%s). ",
                mrcpCfgInfo.serverInfo[i].transportLayer.c_str(),
                "SR", "transportLayer",
                zConfigFile, mrcpCfgInfo.serverInfo[i].transportLayer.c_str());
        }

	}

	if ( mrcpCfgInfo.sipVersion.empty() )
	{
		mrcpCfgInfo.sipVersion="SIP/2.0";
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod,
                REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Warning: Value for (%s:%s) not specified in config file "
				"(%s).  Defaulting to (%s). ", "SR", "sipVersion",
				zConfigFile, mrcpCfgInfo.sipVersion.c_str());
	}

	if ( mrcpCfgInfo.mrcpVersion.empty() )
	{
		mrcpCfgInfo.mrcpVersion="MRCP/2.0";
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod,
                REPORT_DETAIL, MRCP_2_BASE, WARN,
				"Warning: Value for (%s:%s) not specified in config file "
				"(%s).  Defaulting to (%s). ", "SR", "sipVersion",
				zConfigFile, mrcpCfgInfo.mrcpVersion.c_str());
	}

	return(0);

} /* END: sVerifyConfig() */

/*------------------------------------------------------------------------------
printInitInfo():
------------------------------------------------------------------------------*/
void MrcpInitialize::printInitInfo()
{
	static char		yMod[] = "printInitInfo";
	int				i, j;
	char 			yServiceStr[32];

	mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, "[%s]:mrcpCfg.sipVersion:(%s)", 
			"MRCP", mrcpCfgInfo.sipVersion.c_str());
	mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, "[%s]:mrcpCfg.mrcpVersion:(%s)", 
			"MRCP", mrcpCfgInfo.mrcpVersion.c_str());
	mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO, "[%s]:mrcpCfg.localIP:(%s)", 
			"MRCP", mrcpCfgInfo.localIP.c_str());

	// for (i=ARC_MRCP_SR; i<=ARC_MRCP_TTS; i++)  // Not supporting TTS
	for (i=SR; i<=SR; i++)
	{
		sprintf(yServiceStr, "%s", "SR");

		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"[%s]:mrcpCfg.sipPort:(%d)", yServiceStr, 
				mrcpCfgInfo.serverInfo[i].sipPort);
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod,
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"[%s]:mrcpCfg.transportLayer:(%s)", yServiceStr,
				mrcpCfgInfo.serverInfo[i].transportLayer.c_str());
		for (j=0; j < mrcpCfgInfo.serverInfo[i].numUris; j++)
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"[%s]:mrcpCfg.uri[%d].serverIP:(%s)", 
				yServiceStr, j,
				mrcpCfgInfo.serverInfo[i].uri[j].serverIP.c_str());
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"[%s]:mrcpCfg.uri[%d].requestURI:(%s)", 
				yServiceStr, j,
				mrcpCfgInfo.serverInfo[i].uri[j].requestURI.c_str());
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"[%s]:mrcpCfg.uri[%d].inService:(%d)", 
				yServiceStr, j, mrcpCfgInfo.serverInfo[i].uri[j].inService);
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_VERBOSE, MRCP_2_BASE, INFO,
				"[%s]:mrcpCfg.uri[%d].lastFailureTime:(%ld)", 
				yServiceStr, j,
				mrcpCfgInfo.serverInfo[i].uri[j].lastFailureTime);
		}
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"[%s]:mrcpCfg.serverInfo.via:(%s)", 
			yServiceStr, mrcpCfgInfo.serverInfo[i].via.c_str());
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"[%s]:mrcpCfg.payloadType:(%s)", 
			yServiceStr, mrcpCfgInfo.serverInfo[i].payloadType.c_str());
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
			REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"[%s]:mrcpCfg.codecType:(%s)", 
			yServiceStr, mrcpCfgInfo.serverInfo[i].codecType.c_str());
        mrcpClient2Log(__FILE__, __LINE__, -1, yMod,
            REPORT_VERBOSE, MRCP_2_BASE, INFO,
            "[%s]:mrcpCfg.waveformURI:(%d)",
            yServiceStr, mrcpCfgInfo.serverInfo[i].waveformURI);
	}

	mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO, "mrcpFifoDir:(%s)",
		mrcpFifoDir.c_str());
	mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO, "dynmgrFifoDir:(%s)",
		dynmgrFifoDir.c_str());
	mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
		REPORT_VERBOSE, MRCP_2_BASE, INFO, "applFifoDir:(%s)",
		applFifoDir.c_str());

} // END: printInitInfo()

int MrcpInitialize::populateUris(int zType, char *zUriStr)
{
	static char		yMod[]="populateUris";
	char			*pUri;
	char			yTmpUri[1024];
	char			yUriStr[1024];
	char			yServerIp[64];
	char			*p;
	char			*p2;
	int				yIndex;
	int				i;
	char			yStrTokBuff[4096];

	yStrTokBuff[0] = 0;

	mrcpCfgInfo.serverInfo[zType].numUris = 0;
	mrcpCfgInfo.serverInfo[zType].uriIndex = 0;

	sprintf(yTmpUri, "%s", zUriStr);

	if ((pUri = (char *)strtok_r(yTmpUri, ",", (char**) &yStrTokBuff)) == (char *)NULL)
    {
		mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
        		"strtok(%s,\",\") returned NULL.  Unable to "
				"retrieve URIs.", yTmpUri);
        return(-1);
    }

	yIndex = 0;
	for(;;)
	{
		sprintf(yUriStr, "%s", pUri);
		if ( (p = (char *)index((const char *)yUriStr, ':')) == NULL )
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
        		"Invalid URI string found (%s).  Verify requestURI in "
				"arcMRCP2.cfg and retry.", yUriStr);
        	return(-1);
		}
		p2 = p + 1;

		mrcpCfgInfo.serverInfo[zType].uri[yIndex].requestURI	= yUriStr;

		if ( (p = (char *)rindex((const char *)yUriStr, ':')) == NULL )
		{
			mrcpClient2Log(__FILE__, __LINE__, -1, yMod, 
				REPORT_NORMAL, MRCP_2_BASE, ERR,
        		"Invalid URI string found (%s).  Verify requestURI in "
				"arcMRCP2.cfg and retry.", yUriStr);
        	return(-1);
		}
		*p = '\0';
		mrcpCfgInfo.serverInfo[zType].uri[yIndex].serverIP			= p2;
		mrcpCfgInfo.serverInfo[zType].uri[yIndex].inService     	= 1;
		mrcpCfgInfo.serverInfo[zType].uri[yIndex].lastFailureTime	= 0;
		yIndex++;

		mrcpCfgInfo.serverInfo[zType].numUris += 1;

		if ((pUri = (char *)strtok_r(NULL, ",", (char**) &yStrTokBuff)) == NULL)
		{
			break;
		}
	}

	return(0);
} // END: populateUris()

bool MrcpInitialize::IsTooManyFailures_shutErDown() const
{
	return tooManyFailures_shutErDown;
}

void  MrcpInitialize::setTooManyFailures_shutErDown(bool xTooManyFailures_shutErDown)
{
    tooManyFailures_shutErDown = xTooManyFailures_shutErDown;
}

