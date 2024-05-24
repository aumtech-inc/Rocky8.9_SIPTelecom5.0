#ifndef CALL_TERMINATE_HXX
#define CALL_TERMINATE_HXX

#include <iostream>
#include <queue>

#include <pthread.h>

struct CallTerminate_t {
  int CallID;
  int DialogID;
};

class CallTerminateList {

     private:
       pthread_mutex_t lock;
       std::queue<CallTerminate_t > call_list;

     public:
       CallTerminateList();
       ~CallTerminateList();
       void Push(int cid, int did);
       bool Pop(int *cid, int *did);
};

#endif 

