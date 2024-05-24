
#include "mrcp2_HeaderList.hpp" 
#include <string> 
#include <list> 

using namespace std;

class Mrcp2_Request
{

public:
	Mrcp2_Request();
	~Mrcp2_Request(); 
	string			getVersion();
	long			getMessageLength();
	string			getMethod();
	unsigned int	getRequestId();

/*
 * request message body: 
 * 1 request line (version msgLength method RequestID)
 * 1 or more rows of headers in name:value pairs.
 * 1 empty line
 */
	void				addMessageBody(const string& zMessageBody);
	MrcpHeaderList		getHeaderList();

private:
	unsigned int		requestId;
	long				msgLength;
	string				version;
	string				method;

  // Each item in headerList is a name_value pair.
  // if methodName != "SET_PARAMS", then value = "".

	MrcpHeaderList headerList;

  // Buffer of message body should be msgLength + 1
	string msgBody;

}; // END: class MRCP2Request

