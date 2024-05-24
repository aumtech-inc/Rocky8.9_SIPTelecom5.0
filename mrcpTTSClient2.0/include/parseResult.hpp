/* ----------------------------------------------------------------------
Program: parseResult.hpp
Purpose: Include file for parsing the xml result.   
Author : Aumtech, In.c
Date   : 08/01/2006  djb  Created the file from mrcpV1.
--------------------------------------------------------------------------*/
#ifndef PARSE_RESULT_HPP
#define PARSE_RESULT_HPP

// #include <string>
#define	MAX_INSTANCE_CHILDREN		20

typedef struct
{
	char		instanceName[128];
	char		instanceValue[128];
} MyXMLInstanceInfo;

typedef struct mrcpResult
{
	int					numResults;
	int 				confidence; // VXML always takes it as integer
	int 				numInstanceChildren;
	char				resultGrammar[256];
	char				inputValue[256];
	char				instance[256];
	char				inputMode[32];
//	string				xmlResult;
	char				literalTimings[512];
	MyXMLInstanceInfo	instanceChild[MAX_INSTANCE_CHILDREN + 1];
}XmlResultInfo;

int	parseXMLResultString(const char * data, int dataSize, 
						XmlResultInfo * pResult, char * zErrMsg);

#endif
