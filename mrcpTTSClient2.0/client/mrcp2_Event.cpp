/* filename: Mrcp2_Event.cpp */
/*
 * event-line  =    mrcp-version SP message-length SP event-name 
					SP request-id  SP request-state CRLF
 */

//#include <fstream>
#include "mrcp2_HeaderList.hpp"
#include "mrcp2_Event.hpp"
#include "mrcp2_parser_utils.hpp"
#include "mrcpCommon2.hpp"


using namespace std;
using namespace mrcp2client;

extern "C"
{
	#include <stdlib.h>
}

/* constructor */
Mrcp2_Event::Mrcp2_Event()
{
	 version = "";
	 msgLength = 0;
	 eventName = "";
	 requestId = 0;
	 state = "";

	 msgBody = "";
     headerList.clear();
}


Mrcp2_Event::~Mrcp2_Event()
{
    headerList.clear();
}


string Mrcp2_Event::getVersion()
{
	return version;
}

void Mrcp2_Event::setVersion(string zVersion)
{
	version = zVersion;
}


long Mrcp2_Event::getMessageLength()
{
	return msgLength;
}


string Mrcp2_Event::getEventName()
{
	return eventName;
}

unsigned int Mrcp2_Event::getRequestId()
{
	return requestId;
}


void Mrcp2_Event::setRequestId(const unsigned int zRequestId)
{
	requestId = zRequestId;
}


string Mrcp2_Event::getRequestState()
{
	return state;
}

void Mrcp2_Event::setRequestState(const string zState)
{
	state = zState;
}


/* Scan message body and parse the event line.
 * Message body includes the event line and
 * rows of headers in name:value format.
 */
void
Mrcp2_Event::addMessageBody(const string& zMsgBody)
{
	string requestLine; 
	int current = 0;
	int field_len = 0;

//	char debugMsg[1024];

	msgBody = zMsgBody;
	field_len = getFieldLen(msgBody, current, "\n");

//	memset(debugMsg, 0, sizeof(debugMsg));
//	sprintf(debugMsg, "Before request line:  msg body length = (%d), "
//		"msg body data = (%s), current = (%d), field length = (%d)",
//		msgBody.length(), msgBody.data(), current, field_len);
//
//	mrcpClient2Log(__FILE__, __LINE__, -1,
//      	"addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO, "%s",
//		debugMsg);
	requestLine = msgBody.substr(current, field_len); 


    /* YYQ: the message sent from client to server has line end
     * with "\n" or "\r\n", the message received from server may
     * have line end "\r\n". We convert alll "\r\n" with "\n".
     */
    int pos = msgBody.find("\r\n");
	while ( pos > 0)
    {
        int rsize = 2;
        msgBody.replace(pos, rsize, "\n");

        pos = 0;
        pos = msgBody.find("\r\n");
    } // END: while( pos>0)

    field_len = getFieldLen(msgBody, current, "\n");
//	memset(debugMsg, 0, sizeof(debugMsg));
//	sprintf(debugMsg, "Before 2nd request line:  msg body length = (%d), "
//		"msg body data = (%s), current = (%d), field length = (%d)",
//		msgBody.length(), msgBody.data(), current, field_len);
//
//	mrcpClient2Log(__FILE__, __LINE__, -1,
//      	"addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO, "%s",
//		debugMsg);
    requestLine = msgBody.substr(current, field_len);

//	mrcpClient2Log(__FILE__, __LINE__, -1,
//        "addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO,
//        "Length of msg body = %d, processed  msgBody=(%s)",
//        msgBody.length(), msgBody.data());

	/* Parse requestLine  */
	const string delim = " ";

	while (current < (int) requestLine.size() )
	{
		int field_len = 0;

		/* version */
		field_len = getFieldLen(requestLine, current, delim);
//		memset(debugMsg, 0, sizeof(debugMsg));
//		sprintf(debugMsg, "Before version:  msg body length = (%d), "
//			"msg body data = (%s), current = (%d), field lengh = (%d)",
//		msgBody.length(), msgBody.data(), current, field_len);
//
//		mrcpClient2Log(__FILE__, __LINE__, -1,
//      		"addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO, "%s",
//			debugMsg);
		version = requestLine.substr(current, field_len); 

		/* msgLength */
		current += field_len + 1;
		field_len = getFieldLen(requestLine, current, delim);
//		memset(debugMsg, 0, sizeof(debugMsg));
//		sprintf(debugMsg, "Before msgLength:  msg body length = (%d), "
//			"msg body data = (%s), current = (%d), field lengh = (%d)",
//		msgBody.length(), msgBody.data(), current, field_len);
//
//		mrcpClient2Log(__FILE__, __LINE__, -1,
//      		"addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO, "%s",
//			debugMsg);
		string str_msgLen = requestLine.substr(current, field_len); 
		msgLength = atol(str_msgLen.c_str());

		/* event-name */
		current += field_len + 1; 
		field_len = getFieldLen(requestLine, current, delim);
//		memset(debugMsg, 0, sizeof(debugMsg));
//		sprintf(debugMsg, "Before event name:  msg body length = (%d), "
//			"msg body data = (%s), current = (%d), field length = (%d)",
//		msgBody.length(), msgBody.data(), current, field_len);
//
//		mrcpClient2Log(__FILE__, __LINE__, -1,
//      		"addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO, "%s",
//			debugMsg);
		eventName = requestLine.substr(current, field_len); 
		if( ! verify_length("eventName ", 
			(int)eventName.size(), MRCP2_MAX_LEN_EVENT_NAME , '<'))
		{
    		return;
		}

		/* RequestId */
		current += field_len + 1; 
		field_len = getFieldLen(requestLine, current, delim);
//		memset(debugMsg, 0, sizeof(debugMsg));
//		sprintf(debugMsg, "Before request ID:  msg body length = (%d), "
//			"msg body data = (%s), current = (%d), field length = (%d)",
//		msgBody.length(), msgBody.data(), current, field_len);
//
//		mrcpClient2Log(__FILE__, __LINE__, -1,
//      		"addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO, "%s",
//			debugMsg);
		string str_requestId = requestLine.substr(current, field_len); 
		requestId = atoi(str_requestId.c_str());

		/* request state: COMPLETE, IN-PROGRESS, PENDING */
		current += field_len + 1; 
		field_len = getFieldLen(requestLine, current, delim);
//		memset(debugMsg, 0, sizeof(debugMsg));
//		sprintf(debugMsg, "Before state:  msg body length = (%d), "
//			"msg body data = (%s), current = (%d), field length = (%d)",
//		msgBody.length(), msgBody.data(), current, field_len);
//
//		mrcpClient2Log(__FILE__, __LINE__, -1,
//      		"addMessageBody", REPORT_VERBOSE, MRCP_2_BASE, INFO, "%s",
//			debugMsg);
		state = requestLine.substr(current, field_len); 

    	current += field_len + 1;
	} // END: while(...) 

} // End: addMessageBody() 


/* parse message body and put each header
 * in the headerList.
 */
//Mrcp2_HeaderList* Mrcp2_Event::getHeaderList()
MrcpHeaderList Mrcp2_Event::getHeaderList()
{

	string nameValues; 
	int current, content_size, field_len, listCount;
	int num_msgLine = 0;

	current = 0;
	field_len = getFieldLen(msgBody, current, "\n");

    if(field_len <= 0)
    {
        return headerList;
    }

	current = field_len + 1;
	field_len = (int) msgBody.size() - current;

	nameValues = msgBody.substr(current, field_len); 

	current = 0;
	listCount = 0;
	content_size = nameValues.size() - 1; /* remove tailing '\0'  */

	num_msgLine = 1; /* The header line counts 1. */

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

		MrcpHeader header(name, value);
		headerList.push_back(header);

		num_msgLine++ ;
		current += val_len + 1;
		name_len = 0;
		val_len = 0;

	} // END while()

	return headerList;

} // End: getHeaderList() 

