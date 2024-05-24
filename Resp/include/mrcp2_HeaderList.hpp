#ifndef MRCP2_HEADERLIST_HPP 
#define MRCP2_HEADERLIST_HPP 

#include <string> 
#include <list> 

using namespace std;

//using namespace mrcp2client;

class MrcpHeader
{
public:
	MrcpHeader();
	MrcpHeader(const string& zName, const string& zValue);
	~MrcpHeader(){};

	string getName();
	string getValue();
	void setNameValue(const string& zName, const string& zValue);

private:
	string name; 
	string value;
};

typedef list<MrcpHeader>     MrcpHeaderList;


#endif
