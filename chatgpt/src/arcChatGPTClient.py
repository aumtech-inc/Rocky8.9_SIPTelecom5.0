#---------------------------------------------------------------------------
# File:     arcChatGPTClient.py
# Purpose:  Aumtech's chatGPT client.
# Author:   Aumtech, Inc.
#---------------------------------------------------------------------------
import signal
from aiCommon import *
#from dataclasses import dataclass
from aiThread import AIInfo
from typing import Dict
from functools import partial

gKeepRunning = True

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
class AIThreadArray:
    def __init__(self):
        self.threads = []
        self.keepRunning = True

    def addThread(self, theThread):
        self.threads.append(theThread)
        aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
                    "Appended Thread: %s"%theThread)
    
    def checkThreads(self) -> int:
        mod="checkTheads"
        numActiveThreads = 0
        for xThread in self.threads:
#            aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
#                    "Checking Thread: %s"% xThread)
            if not xThread.is_alive():
                aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
                    "Removing thread [%s] from list."%xThread)
                self.threads.remove(xThread)
            else:
                numActiveThreads += 1
#                aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
#                    "Thread: %s"%xThread)
        return(numActiveThreads)

    def stopAllThreads(self) -> None:
        mod="stopAllThreads"
        for xThread in self.threads:
            if xThread.is_alive():
                aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
                        "Sending stop to %s."%xThread)
                xThread.stop()
                self.threads.remove(xThread)
            
gThreadList = AIThreadArray()

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
def signal_handler(signum:int, frame) -> None:

    rc = gThreadList.checkThreads()
    if rc > 0:
        aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
                        "Received signal (%d). Sending stop to %d currently active thread(s)." %(signum, rc))
        gThreadList.stopAllThreads()
    else:
        aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
                        "Received signal (%d). No active threads exist. Shutting down." %(signum))

    gThreadList.keepRunning = False

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
def initGetConfig() -> None:
#        ChatGPT_model=gpt-3.5-turbo
#        ChatGPT_temperature=0.8
#        ChatGPT_max_tokens=2000
#        ChatGPT_api_key=None
#    aiConfig = AI_Config

    ispBase = os.getenv('ISPBASE', '/home/arc/.ISP')
    telCfg=ispBase + '/' + 'Telecom/Tables/.TEL.cfg'

    with open(telCfg, 'r') as t:
        ln=t.readline()
        while (ln):
            if ln[0] == '#' or ln[0] == ' ' or ln[0] == '[' or ln[0] == '\n':
                ln=t.readline()
                continue

            lnClean=ln.replace('\n', '')
            lnList=lnClean.split('=')

            if lnList[0] == "ChatGPT_model":
                aiConfig.ChatGPT_model = lnList[1]
            if lnList[0] == "ChatGPT_temperature":
                aiConfig.ChatGPT_temperature= float(lnList[1])
            if lnList[0] == "ChatGPT_max_tokens":
                aiConfig.ChatGPT_max_tokens = int(lnList[1])
            if lnList[0] == "ChatGPT_api_key":
    #            aiConfig.ChatGPT_api_key = lnList[1]
                tmpKey = ln.split('=', 1)[1]
                tmpKey2 = ctypes.create_string_buffer(bytes(tmpKey, 'utf-8'))

                aiGlobals.clibrary.c_decryptChatGPT_api_key.argtypes = [
                    ctypes.c_int,               # __LINE__
                    ctypes.c_char_p,            # __FILE__
                    ctypes.c_int,               # zCall
                    ctypes.c_char_p             # encoded string 
                ]
                
                aiGlobals.clibrary.c_decryptChatGPT_api_key.restype = ctypes.c_int

                f=FILE(__file__)
                rc = aiGlobals.clibrary.c_decryptChatGPT_api_key(LINE(), bytes(f, 'utf-8'),
                        -1, tmpKey2)
                if rc == -1:
                    aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
                        "Failed to decrypt the ChatGPT_api_key from %s.  "
                        "Correct and retry." % telCfg )
                    exit()

                aiConfig.ChatGPT_api_key = tmpKey2.value.decode()

            ln=t.readline()

    aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
            "AI.model=%s"%aiConfig.ChatGPT_model)
    aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
            "AI.temperature=%0.1f"%aiConfig.ChatGPT_temperature)
    aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
            "AI.max_tokens=%d"%aiConfig.ChatGPT_max_tokens)
    aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
            "AI.api_key=...")

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
def initAILogging() -> None:
    path = os.getcwd()
    aiGlobals.clibrary = ctypes.CDLL(os.path.join(path, "libarcAICommon.so"))
 
    aiGlobals.c_aiLog = aiGlobals.clibrary.c_aiLog
    aiGlobals.c_aiLog.argtypes = [    ctypes.c_int,      # zLine
                            ctypes.c_char_p,    # zFile
                            ctypes.c_int,       # zCall
                            ctypes.c_char_p,    # zMod
                            ctypes.c_int,       # zMode
                            ctypes.c_int,       # zMsgId
                            ctypes.c_int,       # zMsgType
                            ctypes.c_char_p ]     # zMsg

#---------------------------------------------------------------------------
# processAIInitOpcode
#---------------------------------------------------------------------------
def processAIInitOpcode(jData: Dict) -> None:
    mod = "processAIInitOpcode"

#    jData = json.loads(jsonData)

    opcode = jData['opcode']
    appCallNum = jData['appCallNum']
    appPid = jData['appPid']
    appRef = jData['appRef']
    appPassword = jData['appPassword']
    appFifo = jData['appFifo']
    """
#    aiContext = AiContext(**jData)
#    logger = partial(aiLog, zline=Line(), zfile=FILE(__file__), zCall="")
#    logger(zMsgType="logging message")
    """

    aiList = [ opcode, appCallNum, appPid, appRef, appPassword, appFifo ]
    aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO, 
            "Parsed: opcode:%d,appCallNum:%d,appPid:%d,appRef:%d,appPassword:%d,data:%s"\
                %(opcode,appCallNum,appPid,appRef,appPassword,appFifo))
        
    stopEvent = threading.Event()
    newThread = AIInfo(aiList, stopEvent)
    newThread.start()
    gThreadList.addThread(newThread)
    rc = gThreadList.checkThreads()

#--------------------------------------------------------------
# readNextInitRequest()
#--------------------------------------------------------------
def readNextInitRequest(aiFifoFd: int) -> None:
    mod = "readNextInitRequest"
    requestFifo="/tmp/ai.request.fifo"
    i = 0

    while gThreadList.keepRunning == True:
        # aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
        #             "Attempting to read max %d from %s."%(INPUT_FIFO_RW_SIZE, requestFifo))
        try:
            data = os.read(aiFifoFd, INPUT_FIFO_RW_SIZE)  # TODO: non-blocking, error handle)
            if not data:
                rc = gThreadList.checkThreads()
                time.sleep(0.5)
                continue  # Break if no data is read (FIFO is closed)

            aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
                    "read data[%d,%s]"%(len(data),data))
            jsonData = data.decode()
            aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, "data: %s"%jsonData)
        
            jData = json.loads(jsonData)
            opcode = jData['opcode']
            if opcode == DMOP_AIINIT:
                processAIInitOpcode(jData)
            else:
                aiLog(LINE(), FILE(__file__), -1, mod, REPORT_DETAIL, AI_BASE, WARN,
                    "Unexpected opcode (%d) received. What the stink?"%opcode)
                time.sleep(0.5)
        except BlockingIOError:
            rc = gThreadList.checkThreads()
            time.sleep(0.5)
            continue  # Break if no data is read (FIFO is closed)

#---------------------------------------------------------------------------
# main()
#---------------------------------------------------------------------------

if __name__ == "__main__":
    readerInfo = ReaderInfo()
    mod = "main"
    requestFifo="/tmp/ai.request.fifo"

    initAILogging()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGQUIT, signal_handler)
    signal.signal(signal.SIGABRT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, "Set signals")
    initGetConfig()
    aiFifoFd = openFifo(-1, requestFifo, os.O_RDONLY | os.O_NONBLOCK)
    if aiFifoFd == -1:
        aiLog(LINE(), FILE(__file__), -1, mod, REPORT_NORMAL, AI_BASE, ERR, "Failed to open/create FIFO %s.  Exiting."%requestFifo);
        time.sleep(3)
        exit

    readNextInitRequest(aiFifoFd)
    os.close(aiFifoFd)
    os.unlink(requestFifo)   

    while True:
        rc = gThreadList.checkThreads()
        if rc == 0:
            break
        if rc > 0:
            aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
                "Waiting to exit.  Number of active threads is %d." % rc)
            time.sleep(0.5)

    aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, "Exiting.")
    exit(0)
