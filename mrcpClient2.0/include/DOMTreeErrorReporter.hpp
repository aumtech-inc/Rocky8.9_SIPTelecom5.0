/* ----------------------------------------------------------------------
Program: DOMTreeErrorReporter.hpp
Purpose: Error reporter for xml parsing.
Author : Aumtech, Inc.
Date   : 01/05/2004  ddn  Created the file.
--------------------------------------------------------------------------*/
#include <iostream>
#include <DOMString.hpp>
#include <SAXParseException.hpp>
#include <XercesDefs.hpp>
#include <xercesc/sax/ErrorHandler.hpp>


extern "C"
{
	#include <stdio.h>
}

XERCES_CPP_NAMESPACE_USE

class DOMTreeErrorReporter : public ErrorHandler
{

	public:
    	DOMTreeErrorReporter(char *zErrMsg) : fSawErrors(false)
    	{
			if ( zErrMsg != NULL )
			{
				sprintf(errBuf, "%s", zErrMsg);
			}
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
