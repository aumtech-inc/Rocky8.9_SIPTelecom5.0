#include <string>
#include <iostream>
#include "mrcp2_parser_utils.hpp"

using namespace std;

extern "C"
{
	#include <stdlib.h>
	#include <string.h>
}

/*
 * Return number of characters from the current position to
 * the first deliminator (not include the deliminator)
 */
int
getFieldLen(string data, int current, string delim)
{
  int space = 0;

  if (string::npos != data.find(delim, current) )
  {
    space = (int) data.find(delim, current);
  }
  else if( strcmp(delim.c_str(), "\n") == 0)
    space = current;
  else // Fixme: should consider multiple space later
    space = (int) data.size();

  return (space - current);
}

/* 0 = success, -1 = failure */
bool
verify_length(string field_name, 
	              int field_length, 
	              int valid_length, 
	              char symbol)
{
  try {
		string	errMsg="";
		switch(symbol)
		{
		case '>':
			errMsg = field_name + " length can not be shorter than ";
    	if( field_length <= valid_length) throw(errMsg.c_str() );
			break;
		case '=':
			errMsg = field_name + " length must be ";
    	if( field_length != valid_length) throw(errMsg.c_str() );
			break;
		case '<':
			errMsg = field_name + " length can not be longer than ";
    	if( field_length >= valid_length) throw(errMsg.c_str() );
			break;

		default:
    	throw("Invalid operation in verify_length()." );
		} /* end switch */
  }
  catch(char const* s){
    cout << "***Error: " << s 
				 << valid_length << ". ***" << endl;
		exit(-1);
    //return false;
  }

	return true;
}

