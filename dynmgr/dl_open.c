#include <stdio.h> 
#include <unistd.h> 
#include <dlfcn.h> 

static void *handle = NULL;

int 
arc_dbg_open(const char *progname){

   int rc = -1; 
   char path[512] = "";
   int (*init_func)() = NULL;


   snprintf(path, sizeof(path), "./.%s_debug.so", progname);

   if(access(path, R_OK) == 0){
      handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
      if(!handle){
       fprintf(stderr, "Debug Lib found and Trying to Open Debug Lib but couldn't -- handle =%p error=%s\n", handle, dlerror());
      }
      init_func = (int (*)())dlsym(handle, "arc_debug_init");
      if(init_func != NULL){
	rc = init_func();
      }
   } 

   return rc;
}

int arc_dbg_close(){

  void (*free_func)();

  if(handle != NULL){
   free_func = (void (*)())dlsym(handle, "arc_debug_free");
   if(free_func != NULL){
     free_func();
   }
   dlclose(handle);
  }

	return(0);
}


#ifdef TEST 

int 
main(){

   arc_dbg_open();
   test_load();
   sleep(60);
   arc_dbg_close();

   return 0;
}

#endif 
