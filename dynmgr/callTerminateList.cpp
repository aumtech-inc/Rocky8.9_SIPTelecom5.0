#include <iostream>
#include <algorithm>
#include <queue>

#include <pthread.h>

#include "callTerminateList.hxx"

using namespace std;

CallTerminateList::CallTerminateList (){
     pthread_mutex_init(&this->lock, NULL);
}

CallTerminateList::~CallTerminateList (){

}


void CallTerminateList::Push (int cid, int did){

   CallTerminate_t t;

   pthread_mutex_lock(&this->lock);

   t.CallID = cid;
   t.DialogID = did;
   call_list.push(t);
   pthread_mutex_unlock(&this->lock);

}

bool CallTerminateList::Pop (int *cid, int *did){

   CallTerminate_t t;
   bool rc = false;

   pthread_mutex_lock(&this->lock);

   if(call_list.size() != 0){
     t = call_list.front();
     *cid = t.CallID;
     *did = t.DialogID;
     call_list.pop();
     rc = true;
   }

   pthread_mutex_unlock(&this->lock);

   return rc;
}


#ifdef MAIN 

int main(){

   CallTerminateList *l = new CallTerminateList;
   int i;
   int cid, did;
   bool test;

   for(i = 0; i < 1024; i++){
     l->Push(i, i + 1);
   }

   while(1){
    test = l->Pop(&cid, &did);
    if(test == true){ 
      cerr << "Callid=" << cid << " DialogID=" << did << endl;
    } else {
      break;
    }
   }
   
   return 0;

}

#endif

