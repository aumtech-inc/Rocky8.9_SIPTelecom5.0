#---------------------------------------------------------------------------
# File:     arcAIClient.py
# Purpose:  Aumtech's chatGPT client.
# Author:   Aumtech, Inc.
#---------------------------------------------------------------------------
import openai
import threading
import time
import ctypes
import os
from aiCommon import *
#from functools import partial

class MsgToApp(ctypes.Structure):
    _fields_ = [("opcode", ctypes.c_int),
                ("appCallNum", ctypes.c_int),
                ("appPid", ctypes.c_int),
                ("appRef", ctypes.c_int),
                ("appPassword", ctypes.c_int), 
                ("returnCode", ctypes.c_int),
                ("vendorErrorCode", ctypes.c_int),
                ("alternateRetCode", ctypes.c_int),
                ("message", ctypes.c_char_p)]

class AIInfo(threading.Thread):
	def __init__(self, parmList, event):
		threading.Thread.__init__(self)
		self.parmList = parmList
		self.stopEvent = event
		self.clibrary = None
		self.c_writeResponseToApp = None
		self.appResponseFifo = None
		self.thisFifo = None
		self.appCallNum = -1
		self.msgToApp = MsgToApp()
		self.messages = list()

	#---------------------------------------------------------------------
	# stop()
	#---------------------------------------------------------------------
	def stop(self):
		mod = "stop"
		aiLog(LINE(), FILE(__file__), self.appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO, \
	        "Received STOP from parent.  Setting STOP event.")
		self.stopEvent.set()

	#---------------------------------------------------------------------
	# setWriteResponseToApp()
	#---------------------------------------------------------------------
	def setWriteResponseToApp(self):
		path = os.getcwd()
	 
		self.c_writeResponseToApp = aiGlobals.clibrary.c_writeResponseToApp
		self.c_writeResponseToApp.argtypes = [  ctypes.c_int,      # zLine
			ctypes.c_char_p,    # zFile
			ctypes.c_int,       # zCall
			ctypes.c_char_p,    # responseFifo - new name
			MsgToApp ]          #  structure

		self.c_writeResponseToApp.restype = ctypes.c_int
	
	#---------------------------------------------------------------------
	# writeResponseToApp()
	#---------------------------------------------------------------------
	def writeResponseToApp(self, zLine:int, zFile:str, zCall:int, msgToApp:MsgToApp) -> int:
		rc = self.c_writeResponseToApp(zLine, bytes(zFile, 'utf-8'), \
				zCall, bytes(self.appResponseFifo, 'utf-8'), msgToApp)
		return(rc)
	
	#--------------------------------------------------------------
	# readDataFromFILE()
	#--------------------------------------------------------------
	def readDataFromFILE(self, filePath:str) -> str:
		with open(filePath, "r") as file: 
			return file.read()

	#--------------------------------------------------------------
	# writeDataToFile()
	#--------------------------------------------------------------
	def writeDataToFile(self, content:str, port:int) -> str:
		mod = "writeDataToFile"

		tm = time.time()
		fle='/tmp/aiContent_toApp.%d.%d'%(port, tm%1000)

		aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
	        "fle(%s) content(%s)"%(fle, content))

		with open(fle, "w") as file_object:
			file_object.write(content)
    
		newStr="#FILE#:%s"%fle
		aiLog(LINE(), FILE(__file__), port, mod, REPORT_VERBOSE, AI_BASE, INFO, \
			"newStr is (%s)"%newStr)

		aiLog(LINE(), FILE(__file__), port, mod, REPORT_VERBOSE, AI_BASE, INFO, \
	        "Wrote (%s) to (%s). returning (%s) "%(content, fle, newStr))

		return newStr

	#---------------------------------------------------------------------
	# processExit()
	#---------------------------------------------------------------------
	def processExit(self, jData: dict) -> None:
		mod = "processExit"

		opcode = jData['opcode']
		appCallNum = jData['appCallNum']
		appPid = jData['appPid']
		appRef = jData['appRef']
		appPassword = jData['appPassword']

		aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO, \
	            "Processing DMOP_AIEXIT(%d)"%DMOP_AIEXIT)

		self.msgToApp = MsgToApp()
		self.msgToApp.opcode = opcode
		self.msgToApp.appCallNum = appCallNum
		self.msgToApp.appPid = appPid
		self.msgToApp.appRef = appRef
		self.msgToApp.appPassword = appPassword
		self.msgToApp.returnCode = 0
		self.msgToApp.vendorErrorCode = 0
		self.msgToApp.alternateRetCode = 0
		self.msgToApp.message=bytes(" ", 'utf-8')

		rc = self.writeResponseToApp(LINE(), FILE(__file__), appCallNum, self.msgToApp)
		aiLog(LINE(), "aiThread.py", appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
	            "writeResponseToApp() returned %d"%rc)

	#---------------------------------------------------------------------
	# addContent()
	#---------------------------------------------------------------------
	def addContent(self, content:str) -> None:
		mod = "addContent"

		userContent={ 'role': 'user', 'content': ' ' } 
		userContent['content'] = content
		aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
			"Appending content(%s)"%content)
		self.messages.append(userContent)
		aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
				"self.messages = %s "%self.messages)

	#---------------------------------------------------------------------
	# procesContent()
	#---------------------------------------------------------------------
	def procesContent(self, jData: dict) -> None:
		mod = "procesContent"
		FILESTR="#FILE#:"

		AI_ADD_CONTENT             = 20
		AI_ADD_CONTENT_AND_PROCESS = 21
		AI_PROCESS_LOADED          = 22
		AI_CLEAR_CONTENT           = 23
	
		opcode = jData['opcode']
		appCallNum = jData['appCallNum']
		appPid = jData['appPid']
		appRef = jData['appRef']
		appPassword = jData['appPassword']
		aiOpcode = jData['aiOpcode']
		tmpContent = jData['content']
	
		aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
			"Processing opcode %d; content of (%s)"%(aiOpcode, tmpContent))
		if tmpContent.find(FILESTR) > -1:
			flen=len(FILESTR)
			contentFile = tmpContent[flen:]
			content=self.readDataFromFILE(contentFile)
			aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
				"Read content of (%s) from (%s)"%(content, contentFile))
		else:
			content = tmpContent
	    
		if not self.messages:
			self.messages = [
				{'role': 'system', 'content': 'What is the intent of the user?'},
			]
			aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
				"Initialized messages to (%s)"%self.messages[0])
	
		self.msgToApp = MsgToApp()
		self.msgToApp.opcode = opcode
		self.msgToApp.appCallNum = appCallNum
		self.msgToApp.appPid = appPid
		self.msgToApp.appRef = appRef
		self.msgToApp.appPassword = appPassword
		self.msgToApp.returnCode = 0
		self.msgToApp.vendorErrorCode = 0
		self.msgToApp.alternateRetCode = 0

		processIt = 0
		if aiOpcode == AI_ADD_CONTENT:
			self.addContent(content)
			self.msgToApp.message=bytes("", 'utf-8')
		elif aiOpcode == AI_ADD_CONTENT_AND_PROCESS:
			self.addContent(content)
			processIt = 1
		elif aiOpcode == AI_PROCESS_LOADED:
			if len(self.messages) == 0:
				aiLog(LINE(), FILE(__file__), -1, mod, REPORT_NORMAL, AI_BASE, WARN, \
					"No messages loaded. Request to process loaded messages failed. ")
				self.msgToApp.returnCode = -1
				self.msgToApp.message=bytes("", 'utf-8')
			else:
				processIt = 1

		counter=0
		for m in self.messages:
			aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
				"[%d]  %s  "%(counter, m))
			counter += 1
	
		if processIt == 1:
			aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
				"Processing messages: %s"%self.messages)
			openai.api_key = aiConfig.ChatGPT_api_key
			try:
				completion = openai.ChatCompletion.create(
					model = aiConfig.ChatGPT_model,
					temperature = aiConfig.ChatGPT_temperature,
					max_tokens = aiConfig.ChatGPT_max_tokens,
					messages = self.messages
			)
			except Exception as e:
				aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,
					"Failed to process chatGPT content. [%s]."% e)
				self.msgToApp.returnCode = -1
				self.msgToApp.message=bytes("Failed to process chatGPT request", 'utf-8') 
				self.messages.clear()
			else:
				aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,
					"chat completion returned (%s)."%completion.choices[0].message.content)
	
				if len(completion.choices[0].message.content) > 200:
					aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,
						"calling writeDataToFile() with (%s)."%completion.choices[0].message.content)
					fContent = self.writeDataToFile(completion.choices[0].message.content, appCallNum)
					aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,
						"fContent(%s)."%fContent)
					self.msgToApp.message=bytes(fContent, 'utf-8')
				else:
					self.msgToApp.message=bytes(completion.choices[0].message.content, 'utf-8')
					        
				aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,
					"Number of AI messages=%d. Cleared"%len(self.messages))
				self.messages.clear()
		else:
			aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
				"NOT Processing...")
	
		rc = self.writeResponseToApp(LINE(), FILE(__file__), appCallNum, self.msgToApp)
		aiLog(LINE(), "aiThread.py", appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
			"writeResponseToApp() returned %d"%rc)
	
	#---------------------------------------------------------------------
	# readAndProcessRequests()
	#---------------------------------------------------------------------
	def readAndProcessRequests(self) -> None:
		mod="readAndProcessRequests"
		keepProcessing = True
	
		aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
			"Calling openFifo(%s)"%self.thisFifo)
		aiFifoFd = openFifo(self.appCallNum, self.thisFifo, os.O_RDONLY | os.O_NONBLOCK)
		if aiFifoFd == -1:
			aiLog(LINE(), FILE(__file__), -1, mod, REPORT_NORMAL, AI_BASE, ERR, 
				"Failed to open/create FIFO %s.  Exiting."%self.thisFifo);
			return()

		aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, \
		"Successfully opened (%s) ."%self.thisFifo)
	
		while keepProcessing:
#			aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO,
#					"Outer Loop.")
#			while True:
			aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO,
				"Attempting to read from %s."%self.thisFifo)
			try:
				data = os.read(aiFifoFd, INPUT_FIFO_RW_SIZE)  # TODO: non-blocking, error handle

				if not data:
					time.sleep(0.5)
					aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, "Checking for stop signal")
					if self.stopEvent.is_set():
						aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
			  				"Got the stop signal.")
						keepProcessing = False
						break

					continue  # Break if no data is read (FIFO is closed)
	    
				aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, "Read data (%s) from %s"%(data, self.thisFifo))
				jData = json.loads(data)
				opcode = jData['opcode']
				if opcode == DMOP_AIPROCESS_CONTENT:
					self.procesContent(jData)
					continue
				if opcode == DMOP_AIEXIT:
					self.processExit(jData)
					keepProcessing = False
					aiLog(LINE(), FILE(__file__), -1, mod, REPORT_NORMAL, AI_BASE, WARN,
						"Set keepProcessing to (%s)."%keepProcessing)
					break
	
				aiLog(LINE(), FILE(__file__), -1, mod, REPORT_NORMAL, AI_BASE, WARN,
						"Unexpected opcode received (%d).  Ignoring."%opcode)
			except BlockingIOError:
				if self.stopEvent.is_set():
					aiLog(LINE(), FILE(__file__), -1, mod, REPORT_VERBOSE, AI_BASE, INFO, 
			  				"Got the stop signal.")
					keepProcessing = 0
					break
				time.sleep(0.5)
	    
		os.close(aiFifoFd)
		os.unlink(self.thisFifo)
		aiLog(LINE(), FILE(__file__), -1, mod, REPORT_NORMAL, AI_BASE, WARN,
			"Closed %s FIFO."%(self.thisFifo))
	
	#---------------------------------------------------------------------
	# run()
	#---------------------------------------------------------------------
	def run(self):
		mod="run" 


		self.setWriteResponseToApp()

		opcode = self.parmList[0]
		appCallNum = self.parmList[1]
		appPid = self.parmList[2]
		appRef = self.parmList[3]
		appPassword = self.parmList[4]
		tmpData = self.parmList[5]

		aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
			"Starting Thread ID %s - received opcode:%d,appCallNum:%d,appPid:%d,appRef:%d,appPassword:%d,data:%s"\
			%(threading.get_ident(),opcode,appCallNum,appPid,appRef,appPassword,tmpData))

	#    msgToApp = MsgToApp()
		self.msgToApp.opcode = opcode
		self.msgToApp.appCallNum = appCallNum
		self.msgToApp.appPid = appPid
		self.msgToApp.appRef = appRef
		self.msgToApp.appPassword = appPassword
		self.msgToApp.returnCode = 0
		self.msgToApp.vendorErrorCode = -1
		self.msgToApp.alternateRetCode = 0
	
		aiLog(LINE(), "aiThread.py", appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
		"aiInfo.opcode=%d"%opcode)

		if opcode == DMOP_AIINIT:
			self.appCallNum = appCallNum
			self.appResponseFifo = tmpData
			self.thisFifo = "/tmp/ai.request.%d.fifo"%appCallNum
			self.msgToApp.message=bytes(self.thisFifo, 'utf-8')
			aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
				"Set appResponseFifo to %s; thisFifo=%s."%( \
				self.appResponseFifo, self.thisFifo))
			rc = self.writeResponseToApp(LINE(), FILE(__file__), self.appCallNum, self.msgToApp)
			aiLog(LINE(), "aiInfo.py", appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
				"writeResponseToApp returned %d"%rc)
		else:
			aiLog(LINE(), "aiInfo.py", appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
				"opcode is not %d.  It's %d"%(DMOP_AIINIT, self.opcode))
	
			aiLog(LINE(), "aiInfo.py", appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO,   \
				"Calling readAndProcessRequests()")

		self.readAndProcessRequests()

		aiLog(LINE(), FILE(__file__), appCallNum, mod, REPORT_VERBOSE, AI_BASE, INFO, 
				"Exiting thread %s." %threading.get_ident())
		return(0)