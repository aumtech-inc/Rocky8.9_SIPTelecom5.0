#ifndef CALL_INFO_DOT_H
#define CALL_INFO_DOT_H

#include <iostream>
#include <map>

#include <pthread.h>
#include <eXosip2/eXosip.h>

//////////////////////////////////////////////////////////////////////////////
// There will be one of these classes for every port in SIP Call Manager
//
// Basically This stores per call informatio that gets deleted with the
// call, I plan on using this to store Supported and Requires type information
// per call. This can be later search to test for specific options
// passed in by the calling side.
//
// Joe S.
//////////////////////////////////////////////////////////////////////////////

using namespace std;

class CallOptions {
  private:
	pthread_mutex_t lock;
    map <string, string> Flags;

  public:
    CallOptions();
    ~CallOptions();
    bool HasOption(string name);
    string GetOptionStr(string name);
    int  GetOptionInt(string name);
    void SetOption(string name, string option);
    void ClearOption(string name);
    int ProcessInvite(eXosip_event_t *evnt);
    int ProcessInviteResponse(osip_message_t *msg, int status);
    bool OptionHasStr(string name, string str);
    void Clear();
    void Dump();
};



#endif

