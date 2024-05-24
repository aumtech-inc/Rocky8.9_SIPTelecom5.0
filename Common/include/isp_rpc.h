/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

// #include <rpc/types.h>

#define MAXHOSTNAME 14

typedef long Int32;
typedef Bool bool_t;
bool_t xdr_Int32();

struct OBJ_TransmitParms {
	char NET_HostName[MAXHOSTNAME];
	char NET_ServiceName21[30];
	Int32 NET_ObjectType;
	Int32 NET_RequesterId;
	Int32 NET_ServiceId;
	Int32 ComHandle;
	Int32 TranNumber;
	Int32 OpReturnCode;
	Int32 OpStatusCode;
	Int32 EventOrCmdId;
	Int32 AttributeId;
	Int32 AttributeValue;
	struct {
		u_int PointData_len;
		char *PointData_val;
	} PointData;
};
typedef struct OBJ_TransmitParms OBJ_TransmitParms;
bool_t xdr_OBJ_TransmitParms();

#define TELPROG ((u_long)395562)
#define TELVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define SNAPROG ((u_long)395563)
#define SNAVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define APPPROG ((u_long)395564)
#define APPVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define DIRPROG ((u_long)395565)
#define DIRVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define TCPPROG ((u_long)395566)
#define TCPVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define NETPROG ((u_long)395567)
#define NETVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define MAILPROG ((u_long)395568)
#define MAILVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define WSSPROG ((u_long)395569)
#define WSSVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define X25PROG ((u_long)395570)
#define X25VERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();

#define USERPROG ((u_long)395571)
#define USERVERS ((u_long)1)
#define TRANSFER_OBJECT ((u_long)1)
extern int *transfer_object_1();