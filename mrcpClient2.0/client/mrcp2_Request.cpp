/*------------------------------------------------------------------------------Program Name:   Mrcp2_Request.cpp
Purpose     :   Routines for processing mrcp 2.0 requests.
Author      :   Aumtech, Inc.
Update: 06/21/06 yyq Created the file.

Description:
	request line:
		mrcp-version SP message-length SP method-name SP request-id CRLF 
		
		mrcp-version = MRCP/2.0
		message-length = length of the message including the start-line
		request-id = unsigned 32-bit int
------------------------------------------------------------------------------*/
#include <fstream>
#include <iostream>
#include <string>
#include "mrcp2_HeaderList.hpp"
#include "mrcp2_Request.hpp"
#include "mrcp2_parser_utils.hpp"

using namespace std;
using namespace mrcp2client;

extern "C"
{
	#include <stdlib.h>
	#include <string.h>
}

/* Constructor */
Mrcp2_Request::Mrcp2_Request()
{
	 version = "";
	 msgLength = 0;
	 method = "";
	 requestId = 0;
	 msgBody = "";
	 headerList.clear();
}


/* Destructor */
Mrcp2_Request::~Mrcp2_Request()
{
	headerList.clear();
}

string Mrcp2_Request::getVersion()
{
	return version;
}


long Mrcp2_Request::getMessageLength()
{
	return msgLength;
}


string Mrcp2_Request::getMethod()
{
	return method;
}

unsigned int Mrcp2_Request::getRequestId()
{
	return requestId;
}


/* Scan message body and parse the request line.
 * Message body includes the request line and 
 * rows of headers in name:value format.
 */ 
void
Mrcp2_Request::addMessageBody(const string& zMsgBody)
{
	string requestLine; 
	int current = 0;
	int field_len = 0;

	msgBody = zMsgBody;
	field_len = getFieldLen(msgBody, current, "\n");
	requestLine = msgBody.substr(current, field_len); 


	/* Parse requestLine: version, msgLength, method, RequestID */
	const string delim = " ";

	while (current < (int)requestLine.size() )
	{
		int field_len = 0;

	  /*  version */
		field_len = getFieldLen(requestLine, current, delim);
		version = requestLine.substr(current, field_len); 
		if( ! verify_length("version", field_len-1, MRCP2_MAX_LEN_VERSION, '<'))
		{
			return;
		}

		/*  msgLength */
		current += field_len + 1;
		field_len = getFieldLen(requestLine, current, delim);
		string str_msgLen = requestLine.substr(current, field_len); 
		msgLength = atol(str_msgLen.c_str());

		/*  method */
		current += field_len + 1;
		field_len = getFieldLen(requestLine, current, delim);
		method = requestLine.substr(current, field_len); 
		if( ! verify_length("method", field_len-1, MRCP2_MAX_LEN_METHOD_NAME, '<'))
		{
			return;
		}

		/*  requestId */
		current += field_len + 1; 
		field_len = getFieldLen(requestLine, current, delim);
		if( ! verify_length("requestId", field_len-1, MRCP2_MAX_LEN_REQUEST_ID,'<'))
		  return;
		string str_requestId = requestLine.substr(current, field_len); 
		requestId = atoi(str_requestId.c_str());
		
		current += field_len + 1;
	}

} // End: addMessageBody()



/* parse message body and put each header
 * in the headerList. 
 */
MrcpHeaderList Mrcp2_Request::getHeaderList()
{
	string nameValues; 
	int current, content_size, field_len, listCount;

	current = 0;
	field_len = getFieldLen(msgBody, current, "\n");
	current = field_len + 1;
	field_len = (int) msgBody.size() - current;

	nameValues = msgBody.substr(current, field_len); 

	current = 0;
	listCount = 0;
	content_size = (int)nameValues.size() - 1; // remove tailing '\0' 

	while (current < content_size )
	{
		int name_len = 0, val_len = 0;
		string value = "";	

		name_len = getFieldLen(nameValues, current, ":");
		verify_length("header name", name_len-1, MRCP2_MAX_LEN_HEADER_NAME, '<');

		string name = nameValues.substr(current, name_len); 

		current += name_len + 1;
		if(current >= content_size )
		{
			value = "";
			
			MrcpHeader header(name, value);
			headerList.push_back(header);

			current += val_len + 1;
			name_len = 0;
			val_len = 0;
			break;
		}
		val_len = getFieldLen(nameValues, current, "\n");

		if(strcmp(method.c_str(),"SET_PARAMS") == 0)
		{
			value = nameValues.substr(current, val_len); 
		}

		MrcpHeader header(name,value);
		headerList.push_back(header);

		current += val_len + 1;
		name_len = 0;
		val_len = 0;

	} // End: while()

	return headerList;

} // End: getHeaderList() 

