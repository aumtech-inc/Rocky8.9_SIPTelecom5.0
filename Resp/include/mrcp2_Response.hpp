/* filename: mrcp2_Response.hpp  */

/* 
 * mrcp2_response message format:  startline  + headers
 * startline:
 *   version msglen requestId statuscode request-state
 * headers:
 *   name:value
 * note:
 *  msglength include start line and headers
 */

#include <string> 
#include "mrcp2_HeaderList.hpp"

using namespace std;

class Mrcp2_Response
{
public:

	Mrcp2_Response();

	~Mrcp2_Response(); 

	void setVersion(const string zVersion);
	void setRequestId(const unsigned int zRequestId);
	void setStatusCode(const string zStatusCode);
	void setRequestState(const string zState);

	void addMessageBody(const string& zMsgBody);

	unsigned int getRequestId();
	long   getMessageLength();
	string getVersion();
	string getStatusCode();
	string getRequestState();

	MrcpHeaderList getHeaderList();

private:
	Mrcp2_Response(const Mrcp2_Response& rhs);
	Mrcp2_Response& operator=(const Mrcp2_Response& rhs);

	string version;
	long msgLength;
	unsigned int requestId;
	string statusCode;

	string requestState;

	string msgBody;

	MrcpHeaderList headerList;

}; // END: class MRCP2_Response

