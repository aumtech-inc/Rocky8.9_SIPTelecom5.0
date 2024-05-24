/*DDN Created 11222004 6:00 PM*/

#ifndef __EXOSIPARCCOMMON_H__
#define __EXOSIPARCCOMMON_H__

#ifdef __cplusplus
extern "C"
{
#endif


typedef enum eXosip_notify_type 
{
	ARCSIP_NOTIFY_TYPE_UNKNOWN,
	ARCSIP_NOTIFY_TYPE_DTMF,
	ARCSIP_NOTIFY_TYPE_CISCO_TRANSFER_RESULT
	//ARCSIP_NOTIFY_TYPE_CISCO_TRANSFER_FAILURE

}eXosip_notify_type_t;



#ifdef __cplusplus
}
#endif

#endif
