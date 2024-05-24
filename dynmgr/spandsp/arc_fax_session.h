#ifndef ARC_FAX_SESSION_DOT_H
#define ARC_FAX_SESSION_DOT_H


#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

   extern void *T38_CORE_RX;

//
// current max session timeout 15 minuutes 
//

   enum arc_fax_state_e
   {
      ARC_FAX_SESSION_STATE_START = 0,
      ARC_FAX_SESSION_T30_AUDIO_START,
      ARC_FAX_SESSION_T38_START,
      ARC_FAX_SESSION_OVERALL_TIMEOUT,
      ARC_FAX_SESSION_PACKET_TIMEOUT,
      ARC_FAX_SESSION_SUCCESS,
      ARC_FAX_SESSION_FAILURE,
      ARC_FAX_SESSION_END
   };

   enum arc_fax_type_e
   {
      ARC_FAX_SESSION_INBOUND = 0,
      ARC_FAX_SESSION_OUTBOUND = 1
   };

   struct arc_fax_session_parms_t
   {
      char filename[256];       /* must be set */
      char dirpath[256];        /* not used at this point, if we ever use a spool */
      char stationid[100];      /* user set */
      char headernfo[100];      /* user set */
      int type;                 /* sending/recieving */
      int ecm;                  /* zero or one */
      int debug;                /* sets logging */
      /*                        *** t38 params***    */
      int use_t38;              /* flag to turn on t38 */
      int t38_fax_version;      /* i understand this is 0,1,2,3 */
      int t38_max_buffer;       /* default is 1024 */
      int t38_max_datagram;     /* default is 1500 typically the udp mtu */
      int t38_max_bitrate;      /* 14400 */
      int t38_fax_rate_mgmt;    /* 1 or 2    */
      int t38_fax_transport;    /* ??  0 = UDPTL 1 = RTP  ?? */
      int t38_udp_ec;           /* rtp or udptl */
   };

   typedef struct arc_fax_session_e *arc_fax_session_t;

   arc_fax_session_t arc_fax_session_alloc ();

   void arc_fax_session_set_parms (struct arc_fax_session_parms_t *parms, char *dirpath, char *filename, int type, int ecm, int use_t38);

   void arc_fax_session_set_debug (struct arc_fax_session_parms_t *parms);

   int arc_fax_session_check_state (arc_fax_session_t fs);

   int arc_fax_session_init (arc_fax_session_t fs, struct arc_fax_session_parms_t *parms, int use_locks);

   int arc_fax_session_process_buff (arc_fax_session_t fs, char *buff, size_t buffsize, int opts);

   int arc_fax_session_close (arc_fax_session_t fs);

   int arc_fax_session_get_resultstr (arc_fax_session_t fs, char *buff, size_t size);

   void arc_fax_session_free (arc_fax_session_t fs);

   void arc_fax_session_t38_set_tx_handlers (arc_fax_session_t fs, void *handler, void *data);

   void arc_fax_session_t38_set_rx_handlers (arc_fax_session_t fs, void *handler, void *data);

   int arc_fax_session_t38_udptl_tx (void *s, void *user_data, const uint8_t * buf, int len, int count);

   int arc_fax_session_t38_udptl_rx (void *s, void *user_data, int data_type, int field_type, const uint8_t * buf, int len);

   void *arc_fax_session_get_t38_state (arc_fax_session_t fs);

   void arc_fax_session_packet_interval (arc_fax_session_t fs, int msecs);


#ifdef __cplusplus
};
#endif

#endif
