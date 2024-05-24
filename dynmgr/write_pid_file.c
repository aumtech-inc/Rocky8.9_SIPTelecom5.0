#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int 
write_pid_file(const char *path){

 int rc = -1;
 pid_t pid;
 FILE *pidfile;

 pid = getpid();

 pidfile = fopen(path, "w");

 if(pidfile){
   fprintf(pidfile, "%d\n", pid);
   rc = pid;
   fclose(pidfile);
 }

 return rc;
}


#ifdef MAIN 

int main(){

   write_pid_file("./test.pid");

   while(1){
     fprintf(stderr, ".\n");
     sleep(1);
   }

   return 0;
}

#endif 

