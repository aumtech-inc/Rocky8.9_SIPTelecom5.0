/*  filename: mrcp2_HeaderList.cpp  */

#include "mrcp2_HeaderList.hpp"
#include <string>

using namespace std;


///////////////////////////////////// 
// class MrcpHeader  
///////////////////////////////////// 
MrcpHeader::MrcpHeader():name(""), value("") 
{}

MrcpHeader::MrcpHeader(const string& zName, const string& zValue)
:name(zName), value(zValue) 
{}


string MrcpHeader::getName()
{
	return name;
}

string MrcpHeader::getValue()
{
	return value;
}

void MrcpHeader::setNameValue(const string& zName, const string& zValue)
{
	name  = zName;
	value = zValue;
}

