/* 
 * filename: mrcp2_Event.hpp  
 */

#include <string> 
using namespace std;

class Mrcp2_Event
{

public:

	Mrcp2_Event();

	~Mrcp2_Event(); 

	string getVersion();
	void setVersion(const string zVersion);

	long getMessageLength();

	string getEventName();

	unsigned int getRequestId();
	void setRequestId(const unsigned int zRequestId);

	string getRequestState();
	void setRequestState(const string zState);

	// headerList: rows of request-id and status value
	MrcpHeaderList getHeaderList();

	// messageBody includes event line and headers
	void addMessageBody(const string& zMsgBody);

private:

	string version;

	long msgLength;

	string eventName;

	unsigned int requestId;

	string state;

	string msgBody;

	MrcpHeaderList headerList;

}; // END: class Mrcp2_Event

