#ifndef OPTIONS_DOT_HXX
#define OPTIONS_DOT_HXX

#include <iostream>
#include <map>
#include <pthread.h>

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


using namespace std;

class Options {
  
  private:

   pthread_mutex_t lock;
   map<string, bool> BoolOptionMap;
   map<string, int>  IntOptionMap;
   map<string, string> StringOptionMap;

  public:

  enum option_type_e {
    OPTION_BOOL, 
    OPTION_INT, 
    OPTION_STRING,
    OPTION_ALL
  };
 
  Options();
  ~Options();
  void AddOption(const string& name, const string& value, const string& defaults, int type);
  void DelOption(const string& name, int type);
  bool HasOption(const string& name, int type);
  bool GetBoolOption(const string& name);
  int  GetIntOption(const string& name);
  string GetStringOption(const string& name);
  void Clear();

};

#endif


