#ifndef CALL_OPTION_DOT_HXX
#define CALL_OPTION_DOT_HXX


#include <iostream>
#include <eXosip2/eXosip.h>
#include <time.h>

#include "Options.hxx"

///////////////////////////////////////////////////////
//
// This class processes call options, namely 
// plucking and enabling features based on the
// user agents headers and header params 
//
// Note that you need to use the methods from Options::
// to access the maps used for storage 
//
// Note that if you add data to this structure 
// you will have to lock it: multiple threads will 
// use this class. Since there are inly functions 
// the data for now is safe 
//
///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
//
// This is largely an attempt t create RFC compliant
// responses outside of the stack 
//
///////////////////////////////////////////////////////


class CallOptions : public Options {

  private:

  public:
   CallOptions();
   ~CallOptions();
   void Init();
   int ProcessInvite(eXosip_event_t *event);
   int ProcessInviteResponse(osip_message_t *resp, int status);
   int ProcessSessionTimers(char *zMod, int zLine, int zCall, eXosip_event_t *event);
   int ProcessUpdateResponse(osip_message_t *resp, int status);
};

#endif 

