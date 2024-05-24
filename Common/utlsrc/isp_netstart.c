#include <stdio.h>
#include "ispinc.h"
#include "messages.h"


#define netstart_base 1000
#define netstart_version 1

#define MOD_FAIL 1000

SIOF_CB_type GV_SIOF;

char *GV_netstart[] = {

"Module Returned Fail %s"

};

int GV_netstart_num = {sizeof(GV_netstart) / sizeof(GV_netstart[0]) };

int main(int argc, char *argv[]){

  int sleeptime;
  int count;
  int Subsys;
  int gone = 1;

  struct SSIO_Data *PT_OutData;

  if(argc < 2){
  
    printf("Usage: isp_netstart WaitTime\n");

    return gone;

  }

  sleeptime = atoi(argv[1]);

  Subsys = ISP_21_NET;
  
  startup("NET");

  PT_OutData = &GV_SIOF.OutputFifo.OutBoundBuffer;

  PT_OutData->SSIO_SubsysDestination = Subsys;
  PT_OutData->SSIO_BufferedData.NET_ServiceId = ISP_MSG_NET_START;

  if(UTLSIOF(SIOF_CMD_PUTELEMENT,&GV_SIOF) != ISP_SUCCESS){
    printf("add failed\n");
    return gone;
  }

  putssio();

  for(count=0;count < sleeptime;count++){

    getssio();

    if(GV_SIOF.InputFifo.BufferCount > 0){
      gone=0;
      break;
    } 

    sleep(1);
  }


  shutdown();

  return gone;
}

getssio(){

  GV_SIOF.InputFifo.MaxGets = 0;

  if(UTLSIOF(SIOF_CMD_LOADFIFO,&GV_SIOF) == ISP_FAIL){
    printf("get ssio fail \n");
    exit(1);
  }

}

putssio(){

  GV_SIOF.OutputFifo.MaxPuts = 0;

  if(UTLSIOF(SIOF_CMD_UNLOADFIFO,&GV_SIOF) == ISP_FAIL){

    printf("put ssio fail \n");
    exit(1);

  }

}

shutdown(){

  UTLSIOF(SIOF_CMD_DEREGISTER,&GV_SIOF);

  Common_shut_dispatcher();

}

startup(char *ObjectName){

  strcpy(GV_GlobalAttr.ObjectName,ObjectName);

  if(Common_init_dispatcher(GV_netstart, GV_netstart_num,
		         GV_GlobalAttr.ObjectName,
                         netstart_base,
                         netstart_version,"NS") != ISP_SUCCESS){
    printf("common init failed\n");
    exit(1);
  }

  GV_SIOF.StatusBlock.ElementName = "netreg";
  GV_SIOF.InputFifo.MaxGets = 10;
  GV_SIOF.OutputFifo.MaxPuts = 10;
  GV_SIOF.Subsystem = getpid();

  if(UTLSIOF(SIOF_CMD_REGISTER,&GV_SIOF) != ISP_SUCCESS){
    printf("ssio reg failed\n");
    exit(1);
   }


}
