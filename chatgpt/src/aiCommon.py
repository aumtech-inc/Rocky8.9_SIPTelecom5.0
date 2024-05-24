import ctypes
import sys
import threading
import time
import os
import json
from inspect import currentframe, getframeinfo
from decimal import Decimal

DMOP_AIINIT=140
DMOP_AIPROCESS_CONTENT=141
DMOP_AIEXIT=142

REPORT_NORMAL = 1
REPORT_VERBOSE = 2
REPORT_DETAIL = 128

ERR=0
WARN=1
INFO=2

AI_BASE=2600

INPUT_FIFO_RW_SIZE=255

class ReaderInfo:
    def __init__(self):
#        self.lock = threading.lock()
        self.opcode = -1
        self.appCallNum = -1
        self.appPid = -1
        self.appName = ""
        self.data = ""
        self.rThread = threading.Thread()

class AI_Globals:
    def __init__(self):
        clibrary = None
        c_aiLog = None

class AI_Config:
    def __init__(self):
        ChatGPT_model='gpt-3.5-turbo'
        ChatGPT_temperature=Decimal('0.8')
        ChatGPT_max_tokens=int(2000)
        ChatGPT_api_key=None
    
#    def setAILogging():
#        c_aiLog = self.clibrary.c_aiLog
#        c_aiLog.argtypes = [ ctypes.c_int,      # zLine
##                            ctypes.c_char_p,    # zFile
#                            ctypes.c_int,       # zCall
#                            ctypes.c_char_p,    # zMod
##                            ctypes.c_int,       # zMode
#                            ctypes.c_int,       # zMsgId
#                            ctypes.c_int,       # zMsgType
#                            ctypes.c_char_p ]     # zMsg
#        print("Set the AI Logging c_aiLog")

aiGlobals = AI_Globals()
aiConfig = AI_Config()

#----------------------------------------------------------------------------
#----------------------------------------------------------------------------
def LINE():
    return sys._getframe(1).f_lineno

def FILE(fullPath):
    return os.path.basename(fullPath)

#void c_aiLog (int zLine, char *zFile, int zCall, char *zMod, int zMode, int zMsgId, int zMsgType, char *zMsg)

#----------------------------------------------------------------------------
#----------------------------------------------------------------------------
def aiLog(zLine, zFile, zCall, zMod, zMode, zMsgId, zMsgType, zMsg):
#    print("[%s, %d] mod=%s mode=%d msgid=%d msgType=%d, msg=%s"%(zFile, zLine, zMod, zMode, zMsgId, zMsgType, zMsg))
    aiGlobals.c_aiLog(zLine, bytes(zFile, 'utf-8'), zCall, bytes(zMod, 'utf-8'), zMode, zMsgId, zMsgType, bytes(zMsg, 'utf-8'))

def getLineNo():
    return(getframeinfo(currentframe()).lineno)

#--------------------------------------------------------------
# openFifo()
#--------------------------------------------------------------
def openFifo(zCall:int, aiFifoName:int, flags:int):
    mod = "openFifo"
    if not os.path.exists(aiFifoName):
        try:
            os.mkfifo(aiFifoName)
        except Exception as e:
            aiLog(LINE(), FILE(__file__), zCall, mod, REPORT_NORMAL, AI_BASE, ERR, e)
            return(-1)

    # Open the FIFO for reading
    aiLog(LINE(), FILE(__file__), zCall, mod, REPORT_VERBOSE, AI_BASE, INFO, "Attempting to open %s."%aiFifoName)
    try:
        aiFifoFd = os.open(aiFifoName, flags)
        #aiFifoFd = os.open(aiFifoName, os.O_RDONLY)
        #aiFifoFd = os.open(aiFifoName, os.O_RDONLY | os.O_NONBLOCK)
    except Exception as e:
        aiLog(LINE(), FILE(__file__), zCall, mod, REPORT_NORMAL, AI_BASE, ERR, e)
        # aiLog(getframeinfo(currentframe()).lineno, zCall, mod, 0, 0, 0, e)
        return(-1)
    else:
        aiLog(LINE(), FILE(__file__), zCall, mod, REPORT_VERBOSE, AI_BASE, INFO, "Successfully opened %s for reading"%aiFifoName)
        return(aiFifoFd)
# END openFifo()
        
