/* filename: Mrcp2_Response.cpp */
/*
 * response-line  =    mrcp-version SP message-length SP request-id
 *                                   SP status-code SP request-state CRLF
 * status code: 3-digit
 * request-state: COMPLETE, IN-PROGRESS, PENDING
 */

//#include <fstream>
#include <string>
#include "mrcp2_HeaderList.hpp"
#include "mrcp2_Response.hpp"
#include "mrcp2_parser_utils.hpp"
#include "mrcpCommon2.hpp"


using namespace std;
using namespace mrcp2client;

extern "C"
{
	#include <stdlib.h>
	#include <string.h>
}

/* constructor */
Mrcp2_Response::Mrcp2_Response()
{
	version = "";
	msgLength = 0;
	requestId = 0;

	statusCode = "";
	requestState = "";
	 msgBody = "";

	headerList.clear();
}

/* destructor */
Mrcp2_Response::~Mrcp2_Response()
{
	//if (headerList != NULL) delete headerList;
	headerList.clear();
}


/* setters */
void Mrcp2_Response::setVersion(string zVersion)
{
	version = zVersion;
}

void Mrcp2_Response::setRequestId(const unsigned int zRequestId)
{
	requestId = zRequestId;
}

void Mrcp2_Response::setStatusCode(const string zStatus)
{
	statusCode = zStatus;
}

void Mrcp2_Response::setRequestState(const string zState)
{
	requestState = zState;
}


/* getters */

unsigned int Mrcp2_Response::getRequestId()
{
	return requestId;
}

string Mrcp2_Response::getVersion()
{
	return version;
}

long Mrcp2_Response::getMessageLength()
{
	return msgLength;
}

string Mrcp2_Response::getStatusCode()
{
	return statusCode;
}

string Mrcp2_Response::getRequestState()
{
	return requestState;
}



/* Scan message body and parse the response line.
 * Message body includes the response line and
 * rows of headers in name:value format.
 */
void
Mrcp2_Response::addMessageBody(const string& zMsgBody)
{
	string requestLine; 
	int current = 0;
	int field_len = 0;

	msgBody = zMsgBody;

#if 0
   mrcpClient2Log(__FILE__, __LINE__, -1,
        "addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO,
		"Length of msg body = %d, input msgBody=(%s)", 
		msgBody.length(), msgBody.data());
#endif

	/* YYQ: the message sent from client to server has line end 
	 * with "\n" or "\r\n", the message received from server may
	 * have line end "\r\n". We convert alll "\r\n" with "\n".
	 */
	int pos = msgBody.find("\r\n");
	while ( pos > 0)
	{
#if 0
    	mrcpClient2Log(__FILE__, __LINE__, -1,
        	"addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO,
			"pos = %d.", pos); 
#endif
		int rsize = 2;
		msgBody.replace(pos, rsize, "\n");

		pos = 0;
		pos = msgBody.find("\r\n");
	} // END: while(pos>0)

	field_len = getFieldLen(msgBody, current, "\n");
	requestLine = msgBody.substr(current, field_len); 

//	mrcpClient2Log(__FILE__, __LINE__, -1,
//       "addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"Length of msg body = %d, processed  msgBody=(%s)", 
//		msgBody.length(), msgBody.data());

	/* Parse requestLine  */
	const string delim = " ";


	while (current < (int) requestLine.size() )
	{
		int field_len = 0;

		/* version */
		field_len = getFieldLen(requestLine, current, delim);
		version = requestLine.substr(current, field_len); 

		/* msgLength */
		current += field_len + 1;
		field_len = getFieldLen(requestLine, current, delim);
		string str_msgLen = requestLine.substr(current, field_len); 
		msgLength = atol(str_msgLen.c_str());

		/* RequestId */
		current += field_len + 1; 
		field_len = getFieldLen(requestLine, current, delim);
		string str_requestId = requestLine.substr(current, field_len); 
		requestId = atoi(str_requestId.c_str());
		
		/* statusCode */
		current += field_len + 1; 
		field_len = getFieldLen(requestLine, current, delim);
		statusCode = requestLine.substr(current, field_len); 
		int valid_statusCode = 3;
		if( ! verify_length("status code ", 
			(int)statusCode.size(), valid_statusCode , '='))
		{
    	return;
		}

		/* requestState: COMPLETE, IN-PROGRESS, PENDING */
		current += field_len + 1; 
		field_len = getFieldLen(requestLine, current, delim);
		requestState = requestLine.substr(current, field_len); 

		current += field_len + 1;
	} // END: while(...)

} // End: addMessageBody



/* parse message body and put each header
 * in the headerList.
 */
MrcpHeaderList Mrcp2_Response::getHeaderList()
{

	string nameValues; 
	int current, content_size, field_len, listCount;
	int num_msgLine = 0;

	current = 0;
	field_len = getFieldLen(msgBody, current, "\n");

	current = field_len + 1;
	field_len = (int) msgBody.size() - current;

	nameValues = msgBody.substr(current, field_len); 

	current = 0;
	listCount = 0;
	content_size = (int) nameValues.size() - 1; /* remove tailing '\0'  */

	num_msgLine = 1; /* 1: requestLine */

	while (current < content_size )
	{
		int name_len = 0, val_len = 0;
		string value = "";	

		name_len = getFieldLen(nameValues, current, ":");

		string name = nameValues.substr(current, name_len); 

		current += name_len + 1;

		if(current >= content_size )
		{
			value = "";
			
			MrcpHeader header(name, value);
			headerList.push_back(header);

			num_msgLine++ ; /* 1: requestLine */

			current += val_len + 1;
			name_len = 0;
			val_len = 0;
			break;
		}

		val_len = getFieldLen(nameValues, current, "\n");

		if(val_len <=0 )
		{
			value = "";
		}
		else if(current >= content_size )
		{
			value = "";
		}
		else
		{

			value = nameValues.substr(current, val_len); 
		}

		/* Add to headerList */

		MrcpHeader header(name, value);
		headerList.push_back(header);

		num_msgLine++ ; /* 1: requestLine */

		current += val_len + 1;
		name_len = 0;
		val_len = 0;

	} /* end while() */

#if 0

	//  DO NOT VERIFY below: the assumption for following implementatiuon is not true !!!
	verify_length("Total message ", 
		msgBody.size() - num_msgLine - 1, 
		msgLength , '>');
#endif

//	mrcpClient2Log(__FILE__, __LINE__, -1,
//        "getHeaderList", REPORT_VERBOSE, MRCP_2_BASE, INFO,
//		"headerList built.");

	return headerList;

} /* End: getHeaderList() */

