#ifndef ARC_UDPTL_DOT_H
#define ARC_UDPTL_DOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define MTU_BUFFER_SIZE 1500
#define MTU_RINGBUFFER_SIZE 8192
#define UDPTL_BUFFER_OFFSET_BYTES 3
// #define UDPTL_BUFFER_OFFSET_BYTES 0


   enum arc_udptl_type_id_e
   {
      ARC_UDPTL_NO_SIGNAL = 1,
      ARC_UDPTL_CNG,
      ARC_UDPTL_CEG
   };


   enum arc_udptl_direction_e
   {
      ARC_UDPTL_LOCAL = 0,
      ARC_UDPTL_REMOTE
   };

   enum arc_udptl_ecm_mode
   {
      ARC_UDPTL_ECM_NONE = 0,
      ARC_UDPTL_ECM_REDUNDANCY,
      ARC_UDPTL_ECM_FEC
   };

   /* these are opaque types - you have to free them */

   typedef struct arc_udptl_rx_e *arc_udptl_rx_t;

   typedef struct arc_udptl_tx_e *arc_udptl_tx_t;

   /* end types */

   /* allocate tx/rx types  */

   arc_udptl_rx_t arc_udptl_rx_alloc ();

   arc_udptl_tx_t arc_udptl_tx_alloc ();

   /* init the types -- args may change */

   int arc_udptl_tx_init (arc_udptl_tx_t tx);

   int arc_udptl_rx_init (arc_udptl_rx_t rx);

   /* opens the socket and optionally sets it non-blocking */

   int arc_udptl_tx_open (arc_udptl_tx_t tx, int nblocking, int af);

   int arc_udptl_rx_open (arc_udptl_rx_t rx, int nblocking, int af);

   /* sets and binds socket may fail if setting locally on a port that is in use  */
   /* remote sockets are just the destination                                     */

   int arc_udptl_tx_setsockinfo (arc_udptl_tx_t tx, char *ipaddr, unsigned short port, int who);

   int arc_udptl_rx_setsockinfo (arc_udptl_rx_t rx, char *ipaddr, unsigned short port, int who);

   /*  useful for setting port/ip details in sdp */

   int arc_udptl_rx_getsockinfo (arc_udptl_rx_t rx, char *ipaddr, size_t size, unsigned short *port, int who);

   int arc_udptl_tx_getsockinfo (arc_udptl_tx_t rx, char *ipaddr, size_t size, unsigned short *port, int who);

   /* tx/rx */

   int arc_udptl_tx (arc_udptl_tx_t tx, char *buff, size_t size, int count);

   int arc_udptl_rx (arc_udptl_rx_t rx, char *buff, size_t bufsize, int data_type, int field_type);

   /* you need this to clone a socket and destination for symmetric connections -- most are symmetric as far as I have seen */

   int arc_udptl_tx_clone_symmetric (arc_udptl_tx_t tx, arc_udptl_rx_t rx);

   int arc_udptl_rx_clone_symmetric (arc_udptl_rx_t rx, arc_udptl_tx_t tx);

   /* close first and then free */

   int arc_udptl_rx_close (arc_udptl_rx_t rx);

   int arc_udptl_tx_close (arc_udptl_tx_t tx);

   void arc_udptl_tx_free (arc_udptl_tx_t tx);

   void arc_udptl_rx_free (arc_udptl_rx_t rx);

   /* debug level */

   void arc_udptl_tx_set_debug (arc_udptl_tx_t tx);

   void arc_udptl_rx_set_debug (arc_udptl_rx_t rx);

   /* error correction modes */

   void arc_udptl_tx_set_ecm (arc_udptl_tx_t tx, int ecm, int span, int entries);

   // void arc_udptl_rx_set_ecm (arc_udptl_rx_t rx, int ecm, int span);

   /* sets t38 state_t for the udptl transport layer rx only  */

   void arc_udptl_rx_set_spandsp_cb (arc_udptl_rx_t rx, void *cb, void *data);

   /* may not be needed, span dsp may do this */

   int arc_udptl_tx_inc_seqno (arc_udptl_tx_t tx);

   int arc_udptl_rx_inc_seqno (arc_udptl_rx_t rx);

   int arc_udptl_rx_bind (arc_udptl_rx_t rx);

   int arc_udptl_tx_bind (arc_udptl_tx_t tx);

   void arc_udptl_tx_set_packet_interval (arc_udptl_tx_t tx, int msecs);

   void arc_udptl_rx_set_packet_interval (arc_udptl_rx_t rx, int msecs);

#ifdef __cplusplus
};
#endif

#endif
