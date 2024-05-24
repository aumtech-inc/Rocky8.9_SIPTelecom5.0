#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

using namespace std;

namespace mrcp2client
{
// Buffer Limits
const int  MRCP2_MAX_LEN_HEADER_NAME    =   64;
const int  MRCP2_MAX_LEN_EVENT_NAME     =   64;
const int  MRCP2_MAX_LEN_METHOD_NAME    =   64;
const int  MRCP2_MAX_LEN_REQUEST_ID     =   10;
const int  MRCP2_MAX_LEN_REQUEST_STATE  =   64;
const int  MRCP2_MAX_LEN_STATUS_CODE    =   10;
const int  MRCP2_MAX_LEN_VERSION        =   10;
}

/*
 * Return number of characters from the current position to
 * the first deliminator (not include the deliminator)
 */
int getFieldLen(string data, int current, string delim);

bool verify_length(string field_name,
                int field_length,
                int valid_length,
                char symbol);
#endif
