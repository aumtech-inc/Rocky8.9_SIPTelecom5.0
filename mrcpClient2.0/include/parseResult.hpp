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
	char		instanceName[2048];
	char		instanceValue[2048];
} MyXMLInstanceInfo;

typedef struct mrcpResult
{
	int					numResults;
	int 				confidence; // VXML always takes it as integer
	int 				numInstanceChildren;
	char				resultGrammar[2048];
	char				inputValue[2048];
	char				instance[2048];
	char				inputMode[32];
//	string				xmlResult;
	char				literalTimings[512];
	MyXMLInstanceInfo	instanceChild[MAX_INSTANCE_CHILDREN + 1];
}XmlResultInfo;

int	parseXMLResultString(int zPort, const char * data, int dataSize, 
						XmlResultInfo * pResult, char * zErrMsg);

#endif
