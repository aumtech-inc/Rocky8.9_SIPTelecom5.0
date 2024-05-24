/*-----------------------------------------------------------------------------
Program:SRGetXMLResult.c
Author:	Aumtech, Inc.
Update:	08/01/2006 djb Ported to mrcpV2.
------------------------------------------------------------------------------*/
#include "mrcpCommon2.hpp"
#include "mrcpSR.hpp"
// #include "parseResult.hpp"

using namespace mrcp2client;

extern "C"
{
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <errno.h>

	#include "Telecom.h"
	#include "arcSR.h"
}

//extern XmlResultInfo           parsedResults[];
extern string		           xmlResult[];

static void massageForLumenvox(int zPort, char *zXmlResult, int *didItChange); // MR 4948
/*------------------------------------------------------------------------------
SRGetXMLResult():
------------------------------------------------------------------------------*/
int SRGetXMLResult(int zPort, char *zXMLResultFile)
{
	static char		mod[] = "SRGetXMLResult";
	int				yRc;
	int				yFd;
	int				didItChange;

	if ( !gSrPort[zPort].IsTelSRInitCalled() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
            MRCP_2_BASE, INFO, "SRGetResult failed.  Must call SRInit first.");
		return(TEL_FAILURE);
	}

	if( !gSrPort[zPort].IsCallActive() )
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_DETAIL,
			MRCP_2_BASE, WARN, "Call is no longer active.");
		return(TEL_DISCONNECTED);
	}

	if((zXMLResultFile == (char *)0) || (zXMLResultFile == (char *)1))
	{
		if (access(zXMLResultFile, R_OK) > 0)
		{
			unlink(zXMLResultFile);
		}

		if ( gSrPort[zPort].IsRecognitionARecord() )			// MR 4949
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, 
				"There is no XML result to parse for a RECORD; no recognition was performed.  Returning success.");
			return(TEL_SUCCESS);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, 
				"XML result file (%s) is invalid or NULL.", zXMLResultFile);
			return(TEL_FAILURE);
		}
	}

//	if ( parsedResults[zPort].xmlResult.empty() )
	if ( xmlResult[zPort].empty() )
	{
		if ( gSrPort[zPort].IsRecognitionARecord() )			// MR 4949
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
				MRCP_2_BASE, INFO, 
				"There is no XML result to parse for a RECORD; no recognition was performed.  Returning success.");
			return(TEL_SUCCESS);
		}
		else
		{
			mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
				MRCP_2_BASE, ERR, "Error: No XML result is found.");
			return(TEL_FAILURE);
		}
	}

//	getFileAndPath(zPort, zXMLResultFile, yAbsolutePath);
	massageForLumenvox(zPort, (char *)xmlResult[zPort].c_str(), &didItChange);		// MR 4948
	if ( didItChange ) 
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "xmlResult modified for Lumenvox.");
	}

	/*
	** We have the result, now write it to yAbsolutePath.
	*/
	if ((yFd = open(zXMLResultFile, O_CREAT|O_TRUNC|O_WRONLY, 0777)) == -1)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, ERR, 
					"Unable to open XML result file (%s) for output.  [%d, %s]",
					zXMLResultFile, errno, strerror(errno));
		if (access(zXMLResultFile, F_OK) > 0)
		{
			unlink(zXMLResultFile);
		}
		return(TEL_FAILURE);
	}

//	if ( write(yFd, parsedResults[zPort].xmlResult.c_str(),
//					parsedResults[zPort].xmlResult.length()) == -1)
	if ( write(yFd, xmlResult[zPort].c_str(),
					xmlResult[zPort].length()) == -1)
	{
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_NORMAL,
			MRCP_2_BASE, INFO, 
				"Unable to write to XML result file (%s).  [%d, %s]",
				zXMLResultFile, errno, strerror(errno));

		close(yFd);
		if (access(zXMLResultFile, F_OK) > 0)
		{
			unlink(zXMLResultFile);
		}
		return(TEL_FAILURE);
	}
    if ( ! gSrPort[zPort].getHideDTMF() )
    {
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "Successfully wrote %d bytes (%s) to %s.",
			xmlResult[zPort].length(), xmlResult[zPort].c_str(),
			zXMLResultFile);
	}

	close(yFd);
	return (TEL_SUCCESS);

} /* END: SRGetXMLResult() */

/*----------------------------------------------------------------------------
  --------------------------------------------------------------------------*/
static void massageForLumenvox(int zPort, char *zXmlResult, int *didItChange) // MR 4948
{
    char    *beginP;
	static char	mod[]="massageForLumenvox";

	*didItChange = 0;
    if ( (beginP=strstr(zXmlResult, "version='1.0'")) != (char *)NULL )
    {
		*didItChange = 1;
        beginP=strstr(zXmlResult, "'1.0'"); 
        *beginP = '\"';
        beginP += 4;
        *beginP = '\"';
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "Changed xmlResult (1.0) - now (%s)",
			zXmlResult);
    }

    if ( (beginP=strstr(zXmlResult, "encoding='ISO")) != (char *)NULL )
    {
        char    *tmpP;
        int     num;

		*didItChange = 1;
        num = strcspn(beginP, " >?");

        tmpP = beginP + num + 1;
    
        sprintf(beginP, "%s", tmpP);
		mrcpClient2Log(__FILE__, __LINE__, zPort, mod, REPORT_VERBOSE,
            MRCP_2_BASE, INFO, "Removed encoding= from xmlResult - now (%s)",
			zXmlResult);
    }
} // END: massageForLumenvox()
