#ifndef ARC_PROMPT_CONTROL
#define ARC_PROMPT_CONTROL

#include "SoundTouch.h"

using namespace soundtouch;

class ArcPromptControl {
  private:
   int reads;
   float currentSpeed;
   float currentPitch;
   char *path;
   SoundTouch StInstance;
   int flushed;
  public:
   ArcPromptControl();
   ~ArcPromptControl();
   float setSpeed(float newSpeed);
   float setPitch(float newPitch);
   size_t getRemaining();
   size_t putBuff(char *buff, size_t size);
   int getBuff(char *dest, size_t size);
   void flush();
   int is_flushed();
   void reset();
   void set();
};

#endif 

