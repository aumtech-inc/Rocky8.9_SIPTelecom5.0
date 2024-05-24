/* 
 * filename: mrcp2_Event.hpp  
 */

#include <string> 
using namespace std;

#ifndef CLASS_MRCP2_EVENT
#define CLASS_MRCP2_EVENT

#include "mrcp2_HeaderList.hpp"
class Mrcp2_Event
{

public:

	Mrcp2_Event();

	~Mrcp2_Event(); 

	string getVersion();
	void setVersion(const string zVersion);

	long getMessageLength();

	string getEventName();
	string getMsgBody();

	unsigned int getRequestId();
	void setRequestId(const unsigned int zRequestId);

	string getRequestState();
	void setRequestState(const string zState);

	// headerList: rows of request-id and status value
	MrcpHeaderList getHeaderList();

	// messageBody includes event line and headers
	void addMessageBody(const string& zMsgBody);

	/*Overload = operator */
	Mrcp2_Event & operator = (const Mrcp2_Event & other)
	{
		if(this != &other)
		{

			version = 	other.version;
			msgLength = other.msgLength;
			eventName = other.eventName;
			state = other.state;
			msgBody = other.msgBody;

			/*Standard C++ STL class.  should already have = operator*/
			headerList = other.headerList;
		}

		return (*this);

	}/*END: overload = operator*/

private:

	string version;

	long msgLength;

	string eventName;

	unsigned int requestId;

	string state;

	string msgBody;

	MrcpHeaderList headerList;

}; // END: class Mrcp2_Event
#endif

