#ifndef CALL_FREE_LIST_DOT_HPP
#define CALL_FREE_LIST_DOT_HPP

#include <iostream>
#include <map>

#include <pthread.h>

enum CallListState_e {
  CALL_FIRST_PARTY_HANGUP, 
  CALL_SECOND_PARTY_HANGUP 
};

class CallListElement {
   public:
   int zCall;
   int callID;
   int did;
   int state;
   time_t ts;
};

class CallFreeList {

   private:

   pthread_mutex_t lock;

   typedef std::map<int, CallListElement> Map;
   typedef std::pair<int, CallListElement> Pair;

   Map map;

   public:
    int Init();
    void Lock();
    void UnLock();
	int Add(int zCall, int callID, int did, int state); // add with initial reason why 
    bool Has(int callID);                      // returns bool 
    int GetDid(int callID);
    int State(int callID);                     // removes state or reason why it was queued 
    int NewState(int callID, int newState);    // updates state or reason why it was queued 
    void Del(int callID);                      // removes from hash
    void Expire(int nSecs);                    // removes from hash based on timeout
    void Free();                               // frees up the map and deletes everything

};

#endif

