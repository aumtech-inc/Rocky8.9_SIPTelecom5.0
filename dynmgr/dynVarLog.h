#ifndef DYNVARLOG_DOT_H
#define DYNVARLOG_DOT_H
#include "TEL_LogMessages.h"

int dynVarLog (int zLine, int zCall, const char *zpMod, int mode, int msgId, 
	int msgType, const char *msgFormat, ...);

#endif


