#include <iostream>
#include <map>

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "Options.hxx"

#define OPTIONS_LOCK pthread_mutex_lock(&this->lock)
#define OPTIONS_UNLOCK pthread_mutex_unlock(&this->lock)

////////////////////////////////////////////////////////////////
//
// This would be the easy way to do it being that there 
// is one map per type. I guess maybe this way too the trees
// that sort the actual data remain small, and in another way
// it enables a multi map where names can have different values
// depending on type ... OK so i guess it was a good idea ..
//
// Joe S.
//
////////////////////////////////////////////////////////////////


Options::Options(){
  pthread_mutex_init(&this->lock, NULL);
}


Options::~Options(){

  OPTIONS_LOCK;

  this->BoolOptionMap.clear();
  this->IntOptionMap.clear();
  this->StringOptionMap.clear();

  OPTIONS_UNLOCK;
}


void
Options::Clear(){
  OPTIONS_LOCK;

  this->BoolOptionMap.clear();
  this->IntOptionMap.clear();
  this->StringOptionMap.clear();

  OPTIONS_UNLOCK;
  return;
}

void 
Options::AddOption(const string& name, const string& value, const string& defaults, int type){


   // always pair these with true and then false 
   // it is very important to do so 

   char *bool_strings[] = {
     "ON", 
     "OFF", 
     "TRUE", 
     "FALSE", 
     "HIGH",
     "LOW",
     "Enabled",
     "Disabled",
     "1",
     "0",
     NULL
   };

   bool opts[] = {
      true,
      false
   };

   int val = 0;
   int index;


  OPTIONS_LOCK;

  switch(type){
   case OPTION_BOOL:
    for(index = 0; bool_strings[index]; index++){
      if(!strcasecmp(value.c_str(), bool_strings[index])){
        this->BoolOptionMap[name] = opts[index % 2];
        break;
      }
    }
    break;
   case OPTION_INT:
    val = atoi(value.c_str());
    this->IntOptionMap[name] = val;
    break;
   case OPTION_STRING:
    this->StringOptionMap[name] = value;
    break;
   default:
   break;
  }

  OPTIONS_UNLOCK;
  return;
}

void 
Options::DelOption(const string& name, int type){

  OPTIONS_LOCK;

  switch(type){
   case OPTION_BOOL:
     this->BoolOptionMap.erase(name);
     break;
   case OPTION_INT:
     this->IntOptionMap.erase(name);
     break;
   case OPTION_STRING:
     this->StringOptionMap.erase(name);
     break;
   case OPTION_ALL:
     this->BoolOptionMap.erase(name);
     this->IntOptionMap.erase(name);
     this->StringOptionMap.erase(name);
     break;
   default:
     break;
  }

  OPTIONS_UNLOCK;
  return;
}

bool
Options::HasOption(const string& name, int type){

  bool rc = false;

  map<string, bool>::iterator bool_it;
  map<string, int>::iterator int_it;
  map<string, string>::iterator string_it;

  OPTIONS_LOCK;

  switch(type){
    case OPTION_BOOL:
      bool_it = BoolOptionMap.find(name); 
      if(bool_it != BoolOptionMap.end()){
        rc = true;
      }
      break;
    case OPTION_INT:
      int_it = IntOptionMap.find(name);
      if(int_it != IntOptionMap.end()){
        rc = true;
      }
      break;
    case OPTION_STRING:
      string_it = StringOptionMap.find(name);
      if(string_it != StringOptionMap.end()){
        rc = true;
      }
      break;
    case OPTION_ALL:
      bool_it = BoolOptionMap.find(name); 
      if(bool_it != BoolOptionMap.end()){
        rc = true;
        break;
      }
      int_it = IntOptionMap.find(name);
      if(int_it != IntOptionMap.end()){
        rc = true;
        break;
      }
      string_it = StringOptionMap.find(name);
      if(string_it != StringOptionMap.end()){
        rc = true;
        break;
      }
      break;
    default:
      break;

  }
  OPTIONS_UNLOCK;

  return rc;
}

bool 
Options::GetBoolOption(const string& name){

   int rc = false;

   OPTIONS_LOCK;
   rc = this->BoolOptionMap[name];
   OPTIONS_UNLOCK;
 
   return rc;
}

int 
Options::GetIntOption(const string& name){

   int rc = 0;

   OPTIONS_LOCK;
   rc = IntOptionMap[name];
   OPTIONS_UNLOCK;

  return rc;
}

string 
Options::GetStringOption(const string& name){

  string rc;

  OPTIONS_LOCK;
  rc = StringOptionMap[name];
  OPTIONS_UNLOCK;

  return rc;
}

#undef OPTIONS_LOCK 
#undef OPTIONS_UNLOCK 

#ifdef MAIN 

#include <string>


int main(){

  Options test;
  char name[256];
  char value[256];
  char defaults[256]  = "";
  int i;


  for(i=0;i<255;i++){
    snprintf(name, sizeof(name), "test%d", i);
    snprintf(value, sizeof(value), "%d", i);
    test.AddOption(name, value, defaults, test.OPTION_INT);
  }  //end 

  for(i=0;i<255;i++){
    snprintf(name, sizeof(name), "test%d", i);
    snprintf(value, sizeof(value), "test%d", i);
    test.AddOption(name, value, defaults, test.OPTION_STRING);
  }  // end

  for(i=0;i<255;i++){
    snprintf(name, sizeof(name), "test%d", i);
    if(i % 2){
      snprintf(value, sizeof(value), "%d", i % 2);
    } else {
      snprintf(value, sizeof(value), "%d", i % 2);
    }
    test.AddOption(name, value, defaults, test.OPTION_BOOL);
  }  // 

  for(i=0;i<255;i++){
    snprintf(name, sizeof(name), "test%d", i);
    int rc = test.GetIntOption(name);
    cerr << rc << ":" << i << endl;
  } // end

  for(i=0;i<255;i++){
    snprintf(name, sizeof(name), "test%d", i);
    string rc = test.GetStringOption(name);
    cerr << rc << ":" << i <<  endl;
  }

  for(i = 0;i < 255; i++){
    snprintf(name, sizeof(name), "test%d", i);
    bool rc = test.GetBoolOption(name);
    cerr << rc << ":" << i <<  endl;
  }

  return 0;
}

#endif


