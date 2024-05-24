#include <iostream>
#include <map>

#include <pthread.h>
#include <time.h>

#include "callFreeList.hpp"

int
CallFreeList::Init(){
   pthread_mutex_init(&this->lock, NULL);
   return 0;
}

void
CallFreeList::Free(){
  pthread_mutex_destroy(&this->lock);
}

void 
CallFreeList::Lock(){
  pthread_mutex_lock(&this->lock);
}


void
CallFreeList::UnLock(){
  pthread_mutex_unlock(&this->lock);
}

int 
CallFreeList::Add(int zCall, int callID, int did, int state){

  CallListElement x;

  x.zCall = zCall;
  x.callID = callID;
  x.did = did;
  x.state = state;

  time(&x.ts);
  // 
  this->Lock();
  this->map.insert(Pair(callID, x));
  this->UnLock();
  return callID;
}

bool 
CallFreeList::Has(int callID){

   bool rc = false;
   Map::iterator itr;


   this->Lock();
   itr = map.find(callID);

   if(itr != map.end()){
      rc = true;
   }
   this->UnLock();

   return rc;
}


int
CallFreeList::GetDid(int callID){

   int rc = -1;
   CallListElement x;

   if(this->Has(callID) == true){
     this->Lock();
     x = map[callID];
     this->UnLock();
     rc = x.did;
   }

   return rc;
}



int
CallFreeList::State(int callID){

   int rc = -1;
   CallListElement x;

   if(this->Has(callID) == true){
     this->Lock();
     x = map[callID];
     this->UnLock();
     rc = x.state;
   }
  
   return rc;
}

int 
CallFreeList::NewState(int callID, int newState){

   int rc = -1;

   CallListElement x;

   if(this->Has(callID) == true){
     this->Lock();
     x = map[callID];
     x.state = newState;
     map.erase(callID);
     map.insert(Pair(callID, x));
     this->UnLock();
     rc = newState;
   }

   return rc;
}

void
CallFreeList::Del(int callID){

   if(this->Has(callID)){
     this->Lock();
     map.erase(callID);
     this->UnLock();
   }
}


//
// Expire any old 
// call ids lingering around 
//

void
CallFreeList::Expire(int nSecs){

  Map::iterator itr;
  
  time_t now;
  Pair y;
  CallListElement x;

  time(&now);

  this->Lock();
  for(itr = map.begin(); itr != map.end(); itr++){
    y = *itr;
    x = y.second;
    if(now >= (x.ts + nSecs)){
      std::cout << __func__ << x.callID << std::endl;
      this->UnLock();
      this->Del(x.callID);
      this->Lock();
    }
  }
  this->UnLock();

  return;
}




#ifdef MAIN 

int main(){

  CallFreeList t;


  t.Init();

  for(int i = 0; i < 500; i++){
   std::cout << " t.add returns " << t.Add(i, i, i, 3) << std::endl;
   std::cout << " t.has returns " << t.Has(2) << std::endl;
   std::cout << " t.getdid returns " << t.GetDid(2) << std::endl;
   t.Expire(30);
   std::cout << " t.state returns " << t.State(2) << std::endl;
   t.NewState(2, 4);
   std::cout << " t.state returns " << t.State(2) << std::endl;
   t.Del(1);
   sleep(1);
  }
  t.Free();
  

  return 0;

}

#endif

