#include <sax/SAXParseException.hpp>
#include "DOMTreeErrorReporter.hpp"
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <string>
#include <dom/DOMString.hpp>

using namespace std;

extern ostream& operator<<(ostream& target, const DOMString& s);


void DOMTreeErrorReporter::warning(const SAXParseException& toCatch)
{
	sprintf(errBuf, "WARNING: LINE %d, COLUMN %d", toCatch.getLineNumber(), toCatch.getColumnNumber());
}

void DOMTreeErrorReporter::error(const SAXParseException& toCatch)
{
	fSawErrors = true;

	sprintf(errBuf, "SYNTAX ERROR:LINE %d, COLUMN %d", toCatch.getLineNumber(), toCatch.getColumnNumber());

    //throw SAXParseException(toCatch); 
}

void DOMTreeErrorReporter::fatalError(const SAXParseException& toCatch)
{
	fSawErrors = true;

	//sprintf(errBuf, 	"FATAL ERROR:LINE %d, COLUMN %d", toCatch.getLineNumber(), toCatch.getColumnNumber());
	sprintf(errBuf, 	"FATAL ERROR IN XML DATA");

    //throw SAXParseException(toCatch);
}

void DOMTreeErrorReporter::resetErrors()
{
    ;
}


