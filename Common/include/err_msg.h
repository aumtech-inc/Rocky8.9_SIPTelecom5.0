/************************************************************************
err_msg.h : This file contain error message for responsibility.
*************************************************************************/
#define ISP_EENV 	\
	"Environment variable %s is not found/set."

#define ISP_ESIG \
	"Application %s terminated process id %d, resource = %d."

#define ISP_ESHM_AT \
	"Can't attach shared memory segment, errno = %d"

#define ISP_ESHM_DT \
	"Can't dettach shared memory segment, errno = %d"

#define ISP_ESHMID  \
	"Can't get Shared Memory Segment (%ld) Identifier. errno = %d"

#define ISP_ESHM_RM "Can't remove shared memory. errno = %d"

#define ISP_EMSGQID "Can't get Message Queue(%ld) Identifier. errno = %d"

#define ISP_EMSGQ_RM  \
	"Can't remove Message Queue %ld. errno = %d"

#define ISP_ESTOP "Stopping application %d"

#define ISP_EACTIVITY  \
	"No activity found for application, Process killed, pid = %d"

#define ISP_ESTART 	 \
	"Can't start application %s. errno = %d"

#define ISP_EMATCH "No Match found for data token1 %s"

#define ISP_EOBJ "Object %s not supported."

#define ISP_ERECV_MESG  \
	"Unable to receive message from message queue errno = %d"

#define ISP_EOPEN_PIPE  \
	"Unable to open pipe for command %s, errno = %d"

#define ISP_EOPEN_FILE  \
	"Can't access file %s, errno = %d"

#define ISP_EOPEN_DIR  \
	"Can't access directory %s, errno = %d"

#define ISP_ETVOX  \
	"Voice software (Teravox) is not running"

#define ISP_EMACHINE  \
	"Unable to get machine name, errno = %d"

#define ISP_ERULE \
	"Application = %s, Rule = %d is not supported."

#define ISP_EKILL \
	"Can't kill application %s, pid: %d, errno %d"

/**************************** eof() ***********************************/
