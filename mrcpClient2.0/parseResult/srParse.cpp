#include <iostream>
#include <util/PlatformUtils.hpp>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "parseResult.hpp"

using namespace std;

int main(int argc, char* argv[])
{

	XmlResultInfo	yMrcpResult;
	char		yErrorMsg[256];
	int			yRc;
	char		*pData;
	struct stat	yStatBuf;
	int			fd;
	int			bytesRead;
	int			i;

	//
	// First, get the data.
	//
	if ( argc != 2 )
	{
		cout << "Usage: " << argv[0] << " <xml file>" << endl;
		return(2);
	}

	if ( access(argv[1], R_OK) != 0 )
	{
		cout << "Error: " << argv[1] << " is not readable." << endl;
		return(2);
	}

	
	if ( (yRc = stat(argv[1], &yStatBuf)) != 0)
	{
		cout << "Unable to stat (" << argv[1] << ")" << endl;
		return(0);
	}

	if ( yStatBuf.st_size <= 0 )
	{
		cout << "Something wrong here.\n";
		return(0);
	}

	pData = (char *)calloc(yStatBuf.st_size + 256, sizeof(char));
	if (pData == NULL)
	{
		cout << "memory allocation failed." << endl;
	}

	if ((fd = open(argv[1], O_RDONLY)) == -1)
	{
		cout << "open failed\n";
		return(0);
	}

	bytesRead = read(fd, pData, yStatBuf.st_size);
	if (bytesRead <= 0)
	{
		cout << "read failed.";
		return(0);
	}
	
//	cout << "Successfully read (" << pData << ")" << endl;
	cout << "\nAttempting to parse (" << pData << ")  len=" << strlen(pData) << endl; 

	yRc = parseXMLResultString(0, pData, strlen(pData), &yMrcpResult, yErrorMsg);
//	yRc = parseMRCPResult(data, sizeof(data), &yMrcpResult, yErrorMsg);

	if(yRc == 0)
	{
		cout<< "confidence=" <<  yMrcpResult.confidence << endl;
		cout<< "resultGrammar=" <<  yMrcpResult.resultGrammar << endl;
		cout<< "inputValue=" <<  yMrcpResult.inputValue << endl;
		cout<< "inputMode=" <<  yMrcpResult.inputMode << endl;
		cout<< "instance=" <<  yMrcpResult.instance << endl;
		cout<< "literalTimings=" <<  yMrcpResult.literalTimings << endl;
		for (i = 0; i < yMrcpResult.numInstanceChildren; i++)
		{
			cout<<"instance["<< i <<"].instanceName : "<<
						yMrcpResult.instanceChild[i].instanceName<<endl;
			cout<<"instance["<< i <<"].instanceValue : "<<
						yMrcpResult.instanceChild[i].instanceValue<<endl;
		}

	}
	else
	{
		cout<<"FAILED TO PARSE MRCP GRAMMAR "<<endl;
		cout<<"ERROR: "<<yErrorMsg<<endl;
	}
}
