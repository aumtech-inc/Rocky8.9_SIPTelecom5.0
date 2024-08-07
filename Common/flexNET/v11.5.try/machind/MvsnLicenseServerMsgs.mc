;//****************************************************************************
;//	COPYRIGHT (c) 2003 by Macrovision Corporation.	
;//	This software has been provided pursuant to a License Agreement	
;//	containing restrictions on its use.  This software contains
;//	valuable trade secrets and proprietary information of
;//	Macrovision Corporation and is protected by law.
;//	It may 	not be copied or distributed in any form or medium, disclosed
;//	to third parties, reverse engineered or used in any manner not
;//	provided for in said License Agreement except with the prior
;//	written authorization from Macrovision Corporation.
;//*****************************************************************************/
;//*	$Id: MvsnLicenseServerMsg.mc,v 1.2 2003/06/06 19:32:50 jwong Exp $     */
;//*****************************************************************************/

;// Mvsn Event Log Message File

;// Message ID Type

MessageIdTypedef=DWORD

;// Severity Names

SeverityNames=(Success       = 0x0:STATUS_SEVERITY_SUCCESS
               Informational = 0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning       = 0x2:STATUS_SEVERITY_WARNING
               Error         = 0x3:STATUS_SEVERITY_ERROR)

;// Facility Names

FacilityNames=(System  = 0x0:FACILITY_SYSTEM
               Runtime = 0x2:FACILITY_RUNTIME
               Stubs   = 0x3:FACILITY_STUBS
               Io      = 0x4:FACILITY_IO_ERROR_CODE)

;// Language Support

LanguageNames=(English = 0x409:MvsnLicenseServerMsgs409)

MessageId=0x1
Severity=Success
SymbolicName=CAT_INTERNAL_ERROR
Language=English
Internal Software Logic
.

MessageId=0x2
Severity=Success
SymbolicName=CAT_SYSTEM
Language=English
OS System Configuration
.

MessageId=0x3
Severity=Success
SymbolicName=CAT_ENVIRONMENT
Language=English
Environmental Configuration
.

MessageId=0x4
Severity=Success
SymbolicName=CAT_FLEXLM_LMGRD
Language=English
FLEXnet License Server Manager
.

MessageId=0x5
Severity=Success
SymbolicName=CAT_FLEXLM_LICENSE_SERVER
Language=English
FLEXnet License Server System
.

MessageId=0x6
Severity=Success
SymbolicName=CAT_FLEXLM_LICENSE_FILE
Language=English
FLEXnet License File
.

MessageId=0x7
Severity=Success
SymbolicName=CAT_FLEXLM_OPTIONS_FILE
Language=English
FLEXnet Licensing Vendor Options File
.

MessageId=0x8
Severity=Success
SymbolicName=CAT_FLEXLM_LMGRD_PERFORMANCE
Language=English
FLEXnet License Server Manager Performance
.

MessageId=0x9
Severity=Success
SymbolicName=CAT_FLEXLM_LICENSE_SERVER_PERFORMANCE
Language=English
FLEXnet License Server System Performance
.

MessageId=0xA
Severity=Success
SymbolicName=CAT_FLEXLM_LMGRD_HEALTH
Language=English
FLEXnet License Server Manager Health
.

MessageId=0xB
Severity=Success
SymbolicName=CAT_FLEXLM_SERVER_HEALTH
Language=English
FLEXnet License Server System Health
.

MessageId=0xC
Severity=Success
SymbolicName=CAT_FLEXLM_NETWORK_COMM
Language=English
FLEXnet Network Communications
.

MessageId=0xD
Severity=Success
SymbolicName=CAT_FLEXLM_DEBUGLOG
Language=English
FLEXnet Debug Log
.

MessageId=0xE
Severity=Success
SymbolicName=CAT_FLEXLM_SERVER_REPORTLOG
Language=English
FLEXnet License Server System Report Log
.

MessageId=0xF
Severity=Success
SymbolicName=CAT_FLEXLM_LMGRD_EVENT
Language=English
FLEXnet License Server Manager Event
.

MessageId=0x10
Severity=Success
SymbolicName=CAT_FLEXLM_SERVER_EVENT
Language=English
FLEXnet License Server System Event
.

MessageId=0x11
Severity=Success
SymbolicName=CAT_FLEXLM_EVENT_BROKER
Language=English
FLEXnet Event Broker
.

MessageId=0x12
Severity=Success
SymbolicName=CAT_FLEXLM_EVENTLOG
Language=English
FLEXnet Event Log Engine
.

MessageId=0x13
Severity=Success
SymbolicName=CAT_FLEXLM_AGENT
Language=English
FLEXnet Remote Agent
.

MessageId=0x14
Severity=Success
SymbolicName=MVSN_CAT_EVENTLOG_INTERNAL_ERROR
Language=English
General Software Logic
.

MessageId=0x15
Severity=Success
SymbolicName=MVSN_CAT_EVENTLOG_WIN32_NETWORK
Language=English
Windows Domain-based Networking
.

MessageId=0x16
Severity=Success
SymbolicName=MVSN_CAT_EVENTLOG_WIN32_REGISTRY
Language=English
Windows Registry Logic
.

MessageId=0x17
Severity=Success
SymbolicName=MVSN_CAT_EVENTLOG_WIN32_KERNEL
Language=English
Win32 Kernel Logic
.

MessageId=0x18
Severity=Success
SymbolicName=MVSN_CAT_EVENTLOG_GENERAL_INFO
Language=English
General Information
.

MessageId=0x19
Severity=Success
SymbolicName=MVSN_CAT_EVENTLOG_BASIC_TRACE
Language=English
Internal Basic Tracing
.

;// Message definitions
;// -------------------
MessageId=0x2BE
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_READLEN_ERROR
Language=English
(%1) An unexpected input error has been detected: %2.
.

MessageId=0x2C1
Severity=Informational
Facility=System
SymbolicName=MSG_FLEXLM_NO_FORCED_SHUTDOWN
Language=English
(%1) No forced shutdown requested.
.

MessageId=0x2D1
Severity=Informational
Facility=System
SymbolicName=MSG_FLEXLM_EVENT_LOG_ACTIVE
Language=English
(%1) FLEXnet License Server Manager started.
.

MessageId=0x2D2
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_DAEMON_NOT_FOUND
Language=English
(%1) The license server manager (lmgrd) cannot determine which  vendor 
daemon to start; lmgrd exiting.  There are no VENDOR (or DAEMON) lines in 
the license file.
.

MessageId=0x2DB
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_WIN32_SERVICE
Language=English
(%1) FLEXnet License Server System error: %2 %3.
.

MessageId=0x2DC
Severity=Informational
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_WIN32_SERVICE_STOPPED
Language=English
(%1) FLEXnet License Server System is shutting down.
.

MessageId=0x2DE
Severity=Informational
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_STARTMSG
Language=English
(%1) FLEXnet (v%2.%3%4) started on %5%6 (%7/%8/%9).
.

MessageId=0x2EA
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_CLOSE_ERROR
Language=English
(%1) TCP/IP file descriptor: %2.
Length: %3
Error number: %4
Module: %5
Line number: %6
.

MessageId=0x2EB
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_READ_ERROR
Language=English
(%1) TCP/IP read error: %2.
TCP/IP file descriptor: %3.
.

MessageId=0x2EC
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_BAD_CHECKSUM
Language=English
(%1) BAD CHECKSUM.  %2 sending WHAT command.
Client comm revision: %3
Message size: %4
.

MessageId=0x2EF
Severity=Informational
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_PORT
Language=English
(%1) The license server manager (lmgrd) is using TCP-port: %2.
.

MessageId=0x2F1
Severity=Informational
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_LICENSE_FILE
Language=English
(%1) The license server manager (lmgrd) is using license file: %2.
.

MessageId=0x2F2
Severity=Informational
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_SHUTDOWN_REQ
Language=English
(%1) License server manager (lmgrd) shutdown request from %2 at node: %3
.

MessageId=0x2F3
Severity=Informational
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_SHUTDOWN
Language=English
(%1) Shutting down vendor daemon:  %2, on node: %3
.

MessageId=0x2F4
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_STARTUP_FAILED
Language=English
(%1) License server startup system failed. The license server manager (lmgrd) 
cannot find vendor daemon:  %2
.

MessageId=0x2FB
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_VENDOR_REPORT_LOG_OPEN_ERROR
Language=English
(%1) Cannot open report log: %2; %3
.

MessageId=0x2FD
Severity=Warning
Facility=System
SymbolicName=MSG_FLEXLM_VENDOR_BORROW_OUTSTANDING
Language=English
(%1) Cannot shutdown when borrowed licenses are outstanding.  Specify "-force" to  override.
.

MessageId=0x300
Severity=Warning
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_PORT_NOT_AVAILABLE
Language=English
(%1) The TCP/IP port number, %2,  specified in the license file is already in use.
.
 
MessageId=0x301
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_LMGRD_PORT_OPEN_FAILED
Language=English
(%1) The license server manager (lmgrd) failed to open the 
TCP/IP port number specified in the license file.
.

MessageId=0x302
Severity=Error
Facility=System
SymbolicName=MSG_FLEXLM_DEBUG_LOG_OPEN_ERROR
Language=English
(%1) Cannot open debug log: %2; %3
.


 




