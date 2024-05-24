/* ----------------------------------------------------------------------
Program: DOMTreeErrorReporter.hpp
Purpose: Error reporter for xml parsing.
Author : Aumtech, Inc.
Date   : 01/05/2004  ddn  Created the file.
--------------------------------------------------------------------------*/
#include <util/XercesDefs.hpp>
#include <sax/ErrorHandler.hpp>
#include <iostream>

using namespace std;

class DOMTreeErrorReporter : public ErrorHandler
{

	public:
    	DOMTreeErrorReporter(char *zErrMsg) : fSawErrors(false)
    	{
			sprintf(errBuf, "%s", zErrMsg);
//			errBuf = zErrMsg;
    	}
	
    	~DOMTreeErrorReporter()
    	{
    	}
	
    	void warning(const SAXParseException& toCatch);
    	void error(const SAXParseException& toCatch);
    	void fatalError(const SAXParseException& toCatch);
    	void resetErrors();
	
    	bool getSawErrors() const;
	
    	bool    fSawErrors;
	
		char 	errBuf[256];
};

inline bool DOMTreeErrorReporter::getSawErrors() const
{
    return fSawErrors;
}
