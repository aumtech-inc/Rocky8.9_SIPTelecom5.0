#include <iostream>
//#include <soundtouch/SoundTouch.h>
#include <SoundTouch.h>

#include "arc_prompt_control.hh"

#include <string.h>


#if 0
class ArcPromptControl {
  private:
   float currentSpeed;
   float currentPitch;
   char *path;
   soundtouch::SoundTouch StInstance;
  public:
   ArcPromptControl();
   ~ArcPromptControl();
   float setSpeed(float newSpeed);
   float setPitch(float newPitch);
   size_t getRemaining();
   size_t putBuff(char *buff, size_t size);
   size_t getBuff(char *dest, size_t size);
   void reset();
};

#endif


ArcPromptControl::ArcPromptControl(){
   //std::cout << " Call contructor for Speed Control util" << std::endl;
}


ArcPromptControl::~ArcPromptControl(){
   //std::cout << " Call de-contructor for Speed Control util" << std::endl;
}

void
ArcPromptControl::set(){

   char silence[1024];

   this->reads = 0;
   this->currentSpeed = 0.0;
   this->currentPitch = 0.0;
   this->StInstance.setRate(1.0);
   this->StInstance.setChannels(1);
   this->StInstance.setSampleRate(8000);
   this->StInstance.setTempo(1.0);
   this->StInstance.setPitch(1.0);
   this->flushed = 0;

   memset(silence, 0, sizeof(silence));

   // std::cerr << __func__ << " was called for prompt control " << std::endl;

   // this->StInstance.putSamples((SAMPLETYPE *)silence, sizeof(silence) / 2);
   
}


void 
ArcPromptControl::reset(){

  // clear everything 
  this->currentSpeed = 0.0;
  this->currentPitch = 0.0;
  this->StInstance.setRate(1.0);
  this->StInstance.setChannels(1);
  this->StInstance.setSampleRate(8000);
  this->StInstance.setTempo(1.0);
  this->StInstance.setPitch(1.0);
  this->StInstance.clear();
  this->flushed = 0;
  return;
}

void 
ArcPromptControl::flush(){
   if(this->flushed == 0){
     this->StInstance.flush();
     this->flushed = 1;
   }
}

int 
ArcPromptControl::is_flushed(){
   return this->flushed;
   // std::cerr << "remaining=" << this->getRemaining() << std::endl;
}


float 
ArcPromptControl::setSpeed(float newSpeed){

  if(newSpeed == this->currentSpeed){
    return this->currentSpeed;
  }

  if(newSpeed < -50){
     this->currentSpeed = -50.0;
     std::cerr << "already at minimum setting" << std::endl;
  } else  
  if(newSpeed > 50.0){
     this->currentSpeed = 50.0;
     std::cerr << "already at maximum setting" << std::endl;
  } else {
     this->currentSpeed = newSpeed;
  }

  this->StInstance.setTempoChange(this->currentSpeed);
  // std::cerr << __func__ << "() current speed=" << this->currentSpeed * 2 << "%" << std::endl;

  return this->currentSpeed;
}

float 
ArcPromptControl::setPitch(float newPitch){



  if(newPitch == this->currentPitch){
    return this->currentPitch;
  }

  if(newPitch < .5){
    this->currentPitch = .5;
    //std::cerr << "already at minimum setting" << std::endl;
  } else 
  if(newPitch > 2.0){
     this->currentPitch = 2.0;
     //std::cerr << "already at maximum setting" << std::endl;
  } else {
     this->currentPitch = newPitch;
  }

  this->StInstance.setPitch(this->currentPitch);
  //std::cerr << __func__ << "() current pitch=" << this->currentPitch << std::endl;

  return this->currentPitch;
}

size_t
ArcPromptControl::putBuff(char *buff, size_t size){

  this->StInstance.putSamples((SAMPLETYPE *)buff, (uint)size / 2);
  //std::cout << __func__ << "()" << " buff=" << (void *)buff << " size=" << size << " rc=" << size << std::endl;
  return size;
}

size_t 
ArcPromptControl::getRemaining(){

  size_t rc = 0;
  rc =  StInstance.numSamples();
  //std::cout << __func__ << "() " << "remaining=" << rc << std::endl;

  return rc;
}

int
ArcPromptControl::getBuff(char *dest, size_t size){

  size_t rc = 0;

  rc = StInstance.receiveSamples((SAMPLETYPE *)dest, size / 2);

  if(rc > 0){
    rc *= 2; // back to bytes for buffer size
    this->reads++;
  }

  if(rc && (this->reads < 6)){
     memset(dest, 0, size);
  }

  //std::cerr << __func__ << "()" << " buff=" << (void *)dest << " size=" << size << " rc=" << rc << std::endl;

  return rc;
}

#ifdef MAIN 


#include <ctype.h>
#include <linux/soundcard.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>


int
audio_init (const char *dev, int channels, int wordsize, int bitrate, int mode)
{

   int fd;
   int status;
   int arg;

   fd = open (dev, mode);

   if (fd > 0) {

      /* call ioctls on for word size and bitrate */

      arg = channels;

      status = ioctl (fd, SOUND_PCM_WRITE_CHANNELS, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set to single channel: reason = %s", __func__, strerror (errno));
         goto error;
      }

      arg = wordsize;

      status = ioctl (fd, SOUND_PCM_WRITE_BITS, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set word size for audio device reason = %s", __func__, strerror (errno));
         goto error;
      }

      arg = bitrate;

      status = ioctl (fd, SOUND_PCM_WRITE_RATE, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set bit rate for audio device reason = %s", __func__, strerror (errno));
         goto error;
      }
   }

   return fd;

 error:
   close (fd);
   return -1;
}




int 
main(int argc, char **argv){

  ArcPromptControl sc;
  float speed = 100.0;
  float pitch = 100.0;
  char buff[320];
  char out_buff[320];
  int audio_fd = -1;
  FILE *input;
  int nCount = 0;
  int elem = 1;

  if(argc < 2){
     std::cout << std::endl;
     std::cout << " need to specify a file to use for input" << std::endl;
     std::cout << std::endl;
     exit(1);
  }

  if(argv[1] != NULL){
     input = fopen(argv[1], "r");
     if(!input){
         std::cerr << " could not open input audio file" << std::endl;
         exit(1);
     }
  }

  audio_fd = audio_init("/dev/dsp", 1, 16, 8000, O_WRONLY);

  if(audio_fd == -1){
    std::cerr << " failed to open audio deveice exiting ...." << std::endl;
    exit(1);
  }

  memset(out_buff, 0, sizeof(out_buff));

  fprintf(stderr, "SAMPLETUPE size=%ld bytes\n", sizeof(SAMPLETYPE));

  while(1){

   if(elem){
     elem = fread(buff, 1, sizeof(buff), input);
     if(elem == 0){
      std::cerr << " end of file " << std::endl;
      memset(buff, sizeof(buff), 0);
      sc.flush();
     }
   }

   sc.setSpeed(speed);
   // sc.setPitch(pitch);
   if(elem != 0){
     sc.putBuff(buff, sizeof(buff));
   }

   do {
    if(elem == 0){
      std::cerr << sc.getRemaining() << std::endl;
    }
    nCount = sc.getBuff(out_buff, sizeof(out_buff));
    if(nCount > 0){
     write(audio_fd, out_buff, nCount);
    } else 
    if(nCount == -1){
      std::cerr << " end of buffer " << std::endl;
      goto end;
      break;
    } 
    // std::cerr << "remaining=" << sc.getRemaining() << std::endl;
   } while(nCount);

   speed -= 1;
   pitch -= 1;
  }

  return 0;

  end:
      return 0;
}

#endif






