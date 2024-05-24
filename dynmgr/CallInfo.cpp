#include <iostream>
#include <map>
#include <pthread.h>
#include <eXosip2/eXosip.h>

#include "CallInfo.hxx"

//////////////////////////////////////////////////////////////////////////////
//
// There will be one of these classes for every port in SIP Call Manager
//
// Basically This stores per call information that gets deleted with the 
// call, I plan on using this to store Supported: and Requires: SIP information
// per call. This can be later searchied to test for specific options 
// passed in by the calling side.
//
// Joe S.
//
//////////////////////////////////////////////////////////////////////////////

#define CALL_INFO_LOCK  pthread_mutex_lock(&this->lock)
#define CALL_INFO_UNLOCK  pthread_mutex_unlock(&this->lock)

using namespace std;

CallOptions::CallOptions(){
   pthread_mutex_init(&this->lock, NULL);
}

CallOptions::~CallOptions(){

   CALL_INFO_LOCK;
   this->Flags.clear();
   CALL_INFO_UNLOCK;
}


//////////////////////////////////////////////////////////////////////////////////////
//
//
//
//  The shorter the method the larger the comment block:
//
//
//
//////////////////////////////////////////////////////////////////////////////////////

void
CallOptions::Clear(){
   CALL_INFO_LOCK;
   this->Flags.clear();
   CALL_INFO_UNLOCK;
}

bool 
CallOptions::HasOption(string name){

  bool rc = false;
  map<string, string>::iterator it;

  CALL_INFO_LOCK;

  it = this->Flags.find(name);
  if(it != this->Flags.end())
     rc = true;

  CALL_INFO_UNLOCK;
  return rc;
}


void
CallOptions::Dump(){

   CALL_INFO_LOCK;

   map<string, string>::iterator it;

   for(it = this->Flags.begin(); it != this->Flags.end(); it++){
      cerr << __func__ << " name=" << it->first << " value=" << it->second << endl;
   } 

   CALL_INFO_UNLOCK;
}


void
CallOptions::SetOption(string name, string option){

  CALL_INFO_LOCK;

  this->Flags[name] = option;

  CALL_INFO_UNLOCK;
}

string
CallOptions::GetOptionStr(string name){

   string rc;

   CALL_INFO_LOCK;
   rc = this->Flags[name];
   CALL_INFO_UNLOCK;
  
   return rc;
}

bool 
CallOptions::OptionHasStr(string name, string str){

  string value;
  bool rc = false;
  string::size_type loc;

  value = this->Flags[name];
  loc = value.find(str);
  if(loc != string::npos)
     rc = true;

  return rc;
}

int 
CallOptions::GetOptionInt(string name){

   int rc = 0;
   string str;

   CALL_INFO_LOCK;

   str = this->Flags[name];
   rc = atoi(str.c_str());

   CALL_INFO_UNLOCK;

   return rc;
}

void
CallOptions::ClearOption(string name){

  CALL_INFO_LOCK;
  this->Flags.erase(name);
  CALL_INFO_UNLOCK;

}


int 
CallOptions::ProcessInvite(eXosip_event_t *evnt){


  char *sip_header_list[] =
  {
    "supported",
    "required",
    "session-expires",
    "user-agent",
    "p-asserted-identity",
    NULL
  };

  int counts[(sizeof(sip_header_list) / sizeof(void *)) - 1];

  osip_message_t *inv = NULL;
  sdp_message_t *sdp = NULL;
  char name[255];
  char value[255];

  if(!evnt || !evnt->request){
    return -1;
  } else {
    inv = evnt->request;
  }

  // process the sip header list 
  // from the generic headers 

  osip_list_t *list = &inv->headers;
  osip_header_t *hdr = NULL;
  int pos, i;

  memset(counts, 0, sizeof(counts));

  CALL_INFO_LOCK;

  for(pos = 0; !osip_list_eol(list, pos); pos++){
      hdr = (osip_header_t *)osip_list_get(list, pos);
      for(i = 0; sip_header_list[i]; i++){
         fprintf(stderr, " %s: %s: %s\n", __func__, hdr->hname, hdr->hvalue);
         if(!strcasecmp(hdr->hname, sip_header_list[i])){
           if(counts[i] != 0){
             snprintf(name, sizeof(name), "%s-%d", sip_header_list[i], counts[i]);
           } else {
             snprintf(name, sizeof(name), "%s", sip_header_list[i]);
             counts[i]++;
           }
           snprintf(value, sizeof(value), "%s", hdr->hvalue);
           this->Flags[string(name)] = string(value);
           fprintf(stderr, " %s: adding %s: %s\n", __func__, name, value);
         }
      }
  }

  // end sip header list 
  // process from params 
  
  CALL_INFO_UNLOCK;
  return 0;

}

////////////////////////////////////////////////////////////////////////
// This adds various headers to the response to an INVITE based on the 
// original invite values, some may already be populated by the stack's
// automatically created response but may need some additional params.
//
// In any case a sniffer would be your friend 
//
// Joe S.
////////////////////////////////////////////////////////////////////////


int
CallOptions::ProcessInviteResponse(osip_message_t *msg, int status){

  int rc = 0;
  string value = "";
  int val;

  if(!msg)
    return -1;

  CALL_INFO_LOCK;

  switch(status){
    case 200:
      // if session expires header then set refresher param to either UAC or UAS
      value = this->Flags["session-expires"];
      val = atoi(value.c_str());
      cerr << __func__ << "::" << val << "value=" << val << endl;
      rc = 200;
      break;
    case 422:
      // if session-expires header add Min-SE header using global minimum and send back
      
      rc = 422; 
      break;
    default:
      // no status handler defined yet 
      rc = -1;
      break;
  }

  CALL_INFO_UNLOCK;

  return rc;
}

#undef CALL_INFO_LOCK 
#undef CALL_INFO_UNLOCK 

#ifdef MAIN 

int main(){

   CallOptions ci;
   int i;
   char buff[256];
   char buff2[256];

   ci.Clear();

   for(i = 0; i < 1024; i++){
      snprintf(buff, sizeof(buff), "Test%d", i);
      snprintf(buff2, sizeof(buff2), "Tetst%d", i);
      ci.SetOption(string(buff), string(buff2));
   }

   if(ci.HasOption(string("Test123")))
     fprintf(stderr, "Test123=%s\n", ci.GetOptionStr(string("Test123")).c_str());

   fprintf(stderr, " %s=%s\n", "Test123", ci.GetOptionStr(string("Test123")).c_str());

   for(i = 0; i < 255; i++){
      snprintf(buff, sizeof(buff), "Test%d", i);
      std::cerr <<  buff << "=" <<  ci.GetOptionStr(string(buff)) << std::endl;
   }

   ci.Dump();
   ci.Clear();
   return 0;
}

#endif

