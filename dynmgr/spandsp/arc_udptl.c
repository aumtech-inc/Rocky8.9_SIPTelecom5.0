#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "arc_udptl.h"

#define PACKET_SLEEP 0


// -----------------------------------------------------------------//
// OK all this really does is parse the seq no and sequnece         //
// the packets, a read produces whatever was after the sequence no  //
// hopefully it'll be sufficient enough to pass to span dsp         //
// -----------------------------------------------------------------//

/* from spandsp */

#define READ_SEQ 0
#define WRITE_SEQ 1

#define ARC_UDPTL_SET_SEQNO(header, seq)\
do {short *ptr; ptr = (short *)&header[0]; *ptr = htons(seq);} while(0)

#define ARC_UDPTL_GET_SEQNO(header, seq)\
do {short *ptr; ptr = (short *)&header[0]; seq = ntohs(*ptr);} while(0)

#define ARC_UDPTL_SET_TYPE_ID(header, type)\
do {char *ptr; ptr = &header[2]; *ptr = (unsigned char)type;} while(0)


// 
//  returns sequence number or -1 of it doesn't parse
//

static int
build_fec_buff (unsigned char *fec, size_t fec_len, unsigned char *src, size_t len)
{

   int rc = -1;
   int i;

   memset (fec, 0, fec_len);

   if (fec_len < len) {
      for (i = 0; i < fec_len; i++) {
         fec[i] ^= src[i];
      }
      for (; i < len; i++) {
         fec[i] ^= src[i];
      }
      rc = len;
   }

   if (fec_len == len) {
      for (i = 0; i < len; i++) {
         fec[i] ^= src[i];
      }
      rc = 0;
   }

   return rc;
}

enum fec_check_opts_e
{
   FEC_CHECK = 0,
   FEC_REPAIR
};

static int
repair_fec_buff (unsigned char *dst, size_t dst_len, unsigned char *fec, size_t fec_len, int opts)
{

   int rc = 0;
   int i;
   unsigned char val;

   if (dst_len != fec_len) {
      // fprintf (stderr, " %s() dest len=%d fec_len=%d\n", __func__, dst_len, fec_len);
      return rc;
   }

   switch (opts) {

   case FEC_CHECK:

      for (i = 0; i < dst_len; i++) {
         val = dst[i] ^ fec[i];
         if (val != dst[i]) {
            // fprintf (stderr, " %s() offset=%d dest[%i]=%02x fec[%i]=%02x\n", __func__, i, i, dst[i], i, val);
            rc++;
         }
      }
      break;

   case FEC_REPAIR:

      for (i = 0; i < dst_len; i++) {
         if ((dst[i] ^ fec[i]) != 0) {
            // fprintf(stderr, " %s() need to repair byte\n", __func__);
            val = dst[i] ^ fec[i];
            dst[i] = val;
         }
      }

      break;
   }


   return rc;
}



/* macros from spandsp */

static int
decode_length (const uint8_t * buf, int limit, int *len, int *pvalue)
{
   if (*len >= limit)
      return -1;
   if ((buf[*len] & 0x80) == 0) {
      *pvalue = buf[(*len)++];
      return 0;
   }
   if ((buf[*len] & 0x40) == 0) {
      if (*len >= limit - 1)
         return -1;
      *pvalue = (buf[(*len)++] & 0x3F) << 8;
      *pvalue |= buf[(*len)++];
      return 0;
   }
   *pvalue = (buf[(*len)++] & 0x3F) << 14;
   return 1;
}


static int
encode_length (uint8_t * buf, int *len, int value)
{
   int multiplier;

   if (value < 0x80) {
      buf[(*len)++] = value;
      return value;
   }
   if (value < 0x4000) {
      buf[(*len)++] = ((0x8000 | value) >> 8) & 0xFF;
      buf[(*len)++] = value & 0xFF;
      return value;
   }
   multiplier = (value < 0x10000) ? (value >> 14) : 4;
   buf[(*len)++] = 0xC0 | multiplier;
   return multiplier << 14;
}


static int
encode_open_type (uint8_t * buf, int *len, const uint8_t * data, int num_octets)
{
   int enclen;
   int octet_idx;
   uint8_t zero_byte;

   if (num_octets == 0) {
      zero_byte = 0;
      data = &zero_byte;
      num_octets = 1;
   }
   for (octet_idx = 0;; num_octets -= enclen, octet_idx += enclen) {
      if ((enclen = encode_length (buf, len, num_octets)) < 0)
         return -1;
      if (enclen > 0) {
         memcpy (&buf[*len], &data[octet_idx], enclen);
         *len += enclen;
      }
      if (enclen >= num_octets)
         break;
   }

   return 0;
}


static int
decode_open_type (const uint8_t * buf, int limit, int *len, const uint8_t ** p_object, int *p_num_octets)
{
   int octet_cnt;
   int octet_idx;
   int stat;
   int i;
   const uint8_t **pbuf;

   for (octet_idx = 0, *p_num_octets = 0;; octet_idx += octet_cnt) {
      if ((stat = decode_length (buf, limit, len, &octet_cnt)) < 0)
         return -1;
      if (octet_cnt > 0) {
         *p_num_octets += octet_cnt;

         pbuf = &p_object[octet_idx];
         i = 0;
         if ((*len + octet_cnt) > limit)
            return -1;

         *pbuf = &buf[*len];
         *len += octet_cnt;
      }
      if (stat == 0)
         break;
   }
   return 0;
}

/*
   Returns new offset in packet 
*/


static inline int
set_secondary_ifp_length (char *rawbuff, size_t size, int start, int len)
{

   int rc = -1;

   if (size < start + 1) {
      fprintf (stderr, " %s() \n", __func__);
   }

   // for now we are not going to set them 

   rawbuff[start] = '\0';
   rawbuff[start + 1] = '\0';

   return rc;
}

static inline int
set_length (char *rawbuff, size_t size, int len)
{

   unsigned short slen;
   unsigned short *sptr;
   int rc = -1;

   if (size < 3) {
      fprintf (stderr, " %s() setting len cannot be done, packet too small\n", __func__);
      return -1;
   }

   if (len < 128) {
      rawbuff[2] = len;
      rc = 1;
   }
   else if (len < 16384) {
      rawbuff[2] = 0x80;
      slen = len;
      sptr = (unsigned short *) &rawbuff[2];
      *sptr = slen;
      rawbuff[2] |= 0x80;
      rc = 2;
   }

   return rc;

}

/*
  Returns new offset in packet or -1 if something looks bad
*/

static inline int
get_length (char *rawbuff, size_t size, int *len)
{

   int rc = -1;
   unsigned short *ptr = NULL;
   unsigned short slen;

   if (rawbuff[2] & 0x80) {
      ptr = (unsigned short *) &rawbuff[2];
      slen = *ptr;
      *len = slen;
      rc = 2;
   }
   else {
      slen = rawbuff[2];
      *len = slen;
   }

   fprintf (stderr, " %s() slen=%d\n", __func__, slen);

   return rc;
}


struct arc_udptl_tx_e
{
   int debug;
   int sock;
   int nblocking;
   int pf_inet;
   int seqno;
   // ipv6 
   struct sockaddr_in6 local6;
   struct sockaddr_in6 remote6;
   // ipv4
   struct sockaddr_in local;
   struct sockaddr_in remote;
   // 
   char ringbuffer[MTU_RINGBUFFER_SIZE][MTU_BUFFER_SIZE];
   int posts[MTU_RINGBUFFER_SIZE];
   int seq_nos[MTU_RINGBUFFER_SIZE];
   int (*cb) (arc_udptl_tx_t tx, char *rawbuff, int len, int opts);
   int ecm_mode;                /* 0  = NONE, 1 = REDUNDANCY, 2 = FEC */
   int ecm_span;                /* some small number < 5 */
   int ecm_entries;             /* another small number < */
   int msec_interval;
};

struct arc_udptl_rx_e
{
   int debug;
   int nblocking;
   int pf_inet;
   int sock;
   // ipv6
   struct sockaddr_in6 local6;
   struct sockaddr_in6 remote6;
   // ipv4
   struct sockaddr_in local;
   struct sockaddr_in remote;
   // 
   char ringbuffer[MTU_RINGBUFFER_SIZE][MTU_BUFFER_SIZE];
   int posts[MTU_RINGBUFFER_SIZE];
   int seq_nos[MTU_RINGBUFFER_SIZE];
   // 
   char fec_buf[MTU_RINGBUFFER_SIZE][MTU_BUFFER_SIZE];
   int fec_len[MTU_RINGBUFFER_SIZE];
   int fec_entries[MTU_RINGBUFFER_SIZE];
   //
   unsigned short seq[2];
   void *spandsp_data;
   int (*spandsp_cb) (void *state, const uint8_t * buf, int len, uint16_t seq_no);
   // the below does simple checks on the validity of the packet 
   int (*cb) (arc_udptl_rx_t rx, char *rawbuff, int len, int opts);
   int ecm_mode;                /* 0  = NONE, 1 = REDUNDANCY, 2 = FEC */
   int ecm_span;
   int msec_interval;
};


arc_udptl_rx_t
arc_udptl_rx_alloc ()
{
   return calloc (1, sizeof (struct arc_udptl_rx_e));
}

arc_udptl_tx_t
arc_udptl_tx_alloc ()
{
   return calloc (1, sizeof (struct arc_udptl_tx_e));
}

//
// more of less the business end of these routines 
//

static int
arc_udptl_rx_cb (arc_udptl_rx_t rx, char *rawbuff, int len, int opts)
{

   int i;
   int seq = -1;
   unsigned char val;
   int plen;
   int offset;

   ARC_UDPTL_GET_SEQNO (rawbuff, seq);
   offset = get_length (rawbuff, len, &plen);

   if (rx->debug) {

      fprintf (stderr, " %s() rx sequence = %d\n", __func__, seq);
      fprintf (stderr, " %s() packet seq=%d header = ", __func__, seq);
      for (i = 0; i < len; i++) {
         val = rawbuff[i];
         fprintf (stderr, " 0x%02hx ", val);
      }
      fprintf (stderr, "\n");
   }
   // add type id for debugging and sequencing 


#if 0
   switch () {


   }
#endif


   // end type id

   return seq;
}


static int
arc_udptl_tx_cb (arc_udptl_tx_t tx, char *rawbuff, int len, int count)
{

   int rc = -1;
   int i;
   unsigned char val;
   int offset;
   unsigned char fec_buf[MTU_BUFFER_SIZE];
   int idx;

   if (len < 1 || len > MTU_BUFFER_SIZE) {
      fprintf (stderr, " %s() len=%d failed test, return -1\n", __func__, len);
      return -1;
   }

   memset (fec_buf, 0, sizeof (fec_buf));

   idx = tx->seqno & (MTU_RINGBUFFER_SIZE - 1);


   ARC_UDPTL_SET_SEQNO (rawbuff, tx->seqno);
   offset = set_length (rawbuff, 8, len);
   set_secondary_ifp_length (rawbuff, 20, 4, 0);

   if (tx->debug) {

      fprintf (stderr, " %s() tx=%p rawbuff=%p len=%d count=%d seqno=%d\n", __func__, tx, rawbuff, len, count, tx->seqno);
      for (i = 0; i < len; i++) {
         val = rawbuff[i];
         fprintf (stderr, " 0x%02hx ", val);
      }
      fprintf (stderr, "\n");
   }

#if 0

   switch (type_id) {
   case 0x00:
      fprintf (stderr, " %s() type=%02x  %s\n", __func__, type_id, "no-signal");
      break;
   default:
      fprintf (stderr, " %s() unhandled type id %02X\n", __func__, type_id);
      break;
   }

#endif


   if (offset > 0) {
      rc = offset;
   }

   return offset;
}

//
// end of callbacks routines
//


int
arc_udptl_tx_init (arc_udptl_tx_t tx)
{

   int rc = -1;

   if (!tx) {
      return -1;
   }

   tx->cb = arc_udptl_tx_cb;

   return rc;
}

void
arc_udptl_tx_set_debug (arc_udptl_tx_t tx)
{

   if (tx != NULL) {
      tx->debug++;
   }
}

void
arc_udptl_tx_set_packet_interval (arc_udptl_tx_t tx, int msecs)
{

   if (msecs < 0 || msecs > 1000) {
      return;
   }

   if (tx != NULL) {
      tx->msec_interval = msecs;
   }
}

void
arc_udptl_rx_set_packet_interval (arc_udptl_rx_t rx, int msecs)
{

   if (msecs < 0 || msecs > 1000) {
      return;
   }

   if (rx != NULL) {
      rx->msec_interval = msecs;
   }
}

void
arc_udptl_tx_set_ecm (arc_udptl_tx_t tx, int ecm, int span, int entries)
{

   if (tx != NULL) {
      tx->ecm_mode = ecm;
      tx->ecm_span = span;
      tx->ecm_entries = entries;
   }

   if (tx->debug) {
      fprintf (stderr, " %s() setting ecm mode=%d span=%d\n", __func__, ecm, span);
   }

   return;
}

void
arc_udptl_rx_set_debug (arc_udptl_rx_t rx)
{

   if (rx != NULL) {
      rx->debug++;
   }

}


#if 0

// may not be needed ecm should be decoded automatically 
// by the _rx decoders 

void
arc_udptl_rx_set_ecm (arc_udptl_rx_t rx, int ecm, int span)
{

   if (rx != NULL) {
      rx->ecm_mode = ecm;
      rx->ecm_span = span;
   }

   if (rx->debug) {
      fprintf (stderr, " %s() setting udptl ecm_mode=%d span=%d\n", __func__, ecm, span);
   }

   return;
}

#endif

void
arc_udptl_rx_set_spandsp_cb (arc_udptl_rx_t rx, void *cb, void *data)
{

   if (rx != NULL) {
      rx->spandsp_cb = cb;
      rx->spandsp_data = data;
   }

   return;
}


int
arc_udptl_rx_init (arc_udptl_rx_t rx)
{

   int rc = -1;
   // int i;

   if (!rx) {
      return -1;
   }

   rx->cb = arc_udptl_rx_cb;

   return rc;
}


/*
** open and bin udp socket for udptl transport
**
** only ipv4 for now
*/

int
arc_udptl_tx_open (arc_udptl_tx_t tx, int nblocking, int af)
{

   int flags;

   if (!tx) {
      return -1;
   }

   tx->sock = socket (af, SOCK_DGRAM, 0);
   tx->pf_inet = af;

   if (tx->sock != -1 && nblocking) {
      if (-1 == (flags = fcntl (tx->sock, F_GETFL, 0))) {
         fprintf (stderr, " %s() unable to get flags from fcntl, returning -1\n", __func__);
         close (tx->sock);
         tx->sock = -1;
         return -1;
      }

      if (-1 == fcntl (tx->sock, F_SETFL, flags | O_NONBLOCK)) {
         fprintf (stderr, " %s() failed to set non blocking flag on socket, returning -1\n", __func__);
         close (tx->sock);
         tx->sock = -1;
         return -1;
      }

      tx->nblocking = nblocking;

   }

   return tx->sock;
}

int
arc_udptl_rx_open (arc_udptl_rx_t rx, int nblocking, int af)
{

   int flags;

   if (!rx) {
      return -1;
   }

   rx->sock = socket (af, SOCK_DGRAM, 0);
   rx->pf_inet = af;

   if (rx->sock != -1 && nblocking) {
      if (-1 == (flags = fcntl (rx->sock, F_GETFL, 0))) {
         fprintf (stderr, " %s() unable to get flags from fcntl, returning -1\n", __func__);
         close (rx->sock);
         rx->sock = -1;
         return -1;
      }

      if (-1 == fcntl (rx->sock, F_SETFL, flags | O_NONBLOCK)) {
         fprintf (stderr, " %s() failed to set non blocking flag on socket, returning -1\n", __func__);
         close (rx->sock);
         rx->sock = -1;
         return -1;
      }

      rx->nblocking = nblocking;
   }

   return rx->sock;
}


int
arc_udptl_tx_clone_symmetric (arc_udptl_tx_t tx, arc_udptl_rx_t rx)
{

   int rc = 0;

   if (tx->pf_inet != rx->pf_inet) {
      fprintf (stderr, " %s() address families do not match tx->pf=%d rx->pf=%d\n", __func__, tx->pf_inet, tx->pf_inet);
      return -1;
   }

   if (tx->sock == -1) {
      fprintf (stderr, " %s() socket is no opened, returning -1\n", __func__);
      //return -1;
   }

   if (tx != NULL && rx != NULL) {
      close (tx->sock);
      tx->sock = rx->sock;
      switch (tx->pf_inet) {
      case PF_INET:
         memcpy (&tx->local, &rx->local, sizeof (struct sockaddr_in));
         memcpy (&tx->remote, &rx->remote, sizeof (struct sockaddr_in));
         break;
      case PF_INET6:
         memcpy (&tx->local6, &rx->local6, sizeof (struct sockaddr_in6));
         memcpy (&tx->remote6, &rx->remote6, sizeof (struct sockaddr_in6));
         break;
      default:
         fprintf (stderr, " %s() unmatched case statement while cloninf sockaddr structures\n", __func__);
         break;
      }
   }


   return rc;
}

int
arc_udptl_rx_clone_symmetric (arc_udptl_rx_t rx, arc_udptl_tx_t tx)
{

   int rc = -1;

   if (tx->pf_inet != rx->pf_inet) {
      fprintf (stderr, " %s() address families do not match tx->pf=%d rx->pf=%d\n", __func__, tx->pf_inet, tx->pf_inet);
      return -1;
   }

   if (rx->sock == -1) {
      fprintf (stderr, " %s() socket is no opened, returning -1\n", __func__);
      //return -1;
   }

   if (tx != NULL && rx != NULL) {
      close (rx->sock);
      rx->sock = tx->sock;
      switch (tx->pf_inet) {
      case PF_INET:
         memcpy (&rx->local, &tx->local, sizeof (struct sockaddr_in));
         memcpy (&rx->remote, &tx->remote, sizeof (struct sockaddr_in));
         break;
      case PF_INET6:
         memcpy (&rx->local6, &tx->local6, sizeof (struct sockaddr_in6));
         memcpy (&rx->remote6, &tx->remote6, sizeof (struct sockaddr_in6));
         break;
      default:
         fprintf (stderr, " %s() unmatched case statement while cloninf sockaddr structures\n", __func__);
         break;
      }

   }

   return rc;
}

int
arc_udptl_tx_setsockinfo (arc_udptl_tx_t tx, char *ipaddr, unsigned short port, int who)
{

   int rc = 0;
   struct sockaddr_in *ptr = NULL;

   if (!tx) {
      return -1;
   }

   switch (who) {

   case ARC_UDPTL_LOCAL:
      ptr = &tx->local;
      break;
   case ARC_UDPTL_REMOTE:
      ptr = &tx->remote;
      break;
   default:
      fprintf (stderr, " %s() hit  default case in processing who parameters\n", __func__);
      return -1;
   }

   memset (ptr, 0, sizeof (struct sockaddr_in));
   ptr->sin_family = PF_INET;
   if (inet_pton (AF_INET, ipaddr, &ptr->sin_addr) < 1) {
      fprintf (stderr, " %s() failed to convert ip address into something meaningful...\n", __func__);
      return -1;
   }
   ptr->sin_port = htons (port);


#if 0
   if (tx->sock != -1 && who == ARC_UDPTL_LOCAL) {
      if (bind (tx->sock, (struct sockaddr *) &tx->local, sizeof (tx->local))
          != 0) {
         return -1;
      }
      else {
         rc = 0;
      }
   }

#endif

   return rc;
}


int
arc_udptl_tx_bind (arc_udptl_tx_t tx)
{
   int rc = -1;
   rc = bind (tx->sock, (struct sockaddr *) &tx->local, sizeof (tx->local));
   return rc;
}



int
arc_udptl_rx_setsockinfo (arc_udptl_rx_t rx, char *ipaddr, unsigned short port, int who)
{

   int rc = 0;
   struct sockaddr_in *ptr;

   if (!rx) {
      return -1;
   }

   switch (who) {
   case ARC_UDPTL_LOCAL:
      ptr = &rx->local;
      break;
   case ARC_UDPTL_REMOTE:
      ptr = &rx->remote;
      break;
   default:
      fprintf (stderr, " %s() hit default case in processing who parameters\n", __func__);
      break;
   }

   memset (ptr, 0, sizeof (struct sockaddr_in));
   ptr->sin_family = PF_INET;
   if (inet_pton (AF_INET, ipaddr, &ptr->sin_addr) < 1) {
      fprintf (stderr, " %s() failed to convert ip address into something meaningful...\n", __func__);
      return -1;
   }
   ptr->sin_port = htons (port);


#if 0

   if (rx->sock != -1 && who == ARC_UDPTL_LOCAL) {
      if (bind (rx->sock, (struct sockaddr *) &rx->local, sizeof (rx->local))
          != 0) {
         return -1;
      }
      else {
         rc = 0;
      }
   }

#endif

   return rc;
}



int
arc_udptl_rx_bind (arc_udptl_rx_t rx)
{
   int rc = -1;
   rc = bind (rx->sock, (struct sockaddr *) &rx->local, sizeof (rx->local));
   return rc;
}



int
arc_udptl_rx_getsockinfo (arc_udptl_rx_t rx, char *ip, size_t size, unsigned short *port, int who)
{

   int rc = -1;
   socklen_t socklen = sizeof (struct sockaddr_in);

   if (rx->sock != -1) {

      switch (who) {

      case ARC_UDPTL_LOCAL:
         rc = getsockname (rx->sock, (struct sockaddr *) &rx->local, &socklen);
         if (rc == 0) {
            //fprintf (stderr, " %s() ipaddress=%s anmd port=%d\n", __func__, inet_ntoa (rx->local.sin_addr), ntohs (rx->local.sin_port));
            snprintf (ip, size, "%s", inet_ntoa (rx->local.sin_addr));
            *port = ntohs (rx->local.sin_port);
         }
         break;
      case ARC_UDPTL_REMOTE:
         //fprintf (stderr, " %s() ipaddress=%s and port=%d\n", __func__, inet_ntoa (rx->remote.sin_addr), ntohs (rx->remote.sin_port));
         snprintf (ip, size, "%s", inet_ntoa (rx->remote.sin_addr));
         *port = ntohs (rx->remote.sin_port);
         break;
      default:
         fprintf (stderr, " %s() hit default case in processing who parameters[%d]\n", __func__, who);
         break;
      }
   }

   return rc;
}

int
arc_udptl_tx_getsockinfo (arc_udptl_tx_t tx, char *ip, size_t size, unsigned short *port, int who)
{

   int rc = -1;
   socklen_t socklen = sizeof (struct sockaddr_in);

   if (tx->sock != -1) {

      switch (who) {

      case ARC_UDPTL_LOCAL:
         rc = getsockname (tx->sock, (struct sockaddr *) &tx->local, &socklen);
         if (rc == 0) {
            //fprintf (stderr, " %s() ipaddress=%s anmd port=%d\n", __func__, inet_ntoa (sock_in.sin_addr), ntohs (sock_in.sin_port));
            snprintf (ip, size, "%s", inet_ntoa (tx->local.sin_addr));
            *port = ntohs (tx->local.sin_port);
         }
         break;
      case ARC_UDPTL_REMOTE:
         //fprintf (stderr, " %s() ipaddress=%s anmd port=%d\n", __func__, inet_ntoa (tx->remote.sin_addr), ntohs (tx->remote.sin_port));
         snprintf (ip, size, "%s", inet_ntoa (tx->remote.sin_addr));
         *port = ntohs (tx->remote.sin_port);
         break;
      default:
         fprintf (stderr, " %s() hit default case in processing who parameters[%d]\n", __func__, who);
         break;
      }
   }

   return rc;
}

int
arc_udptl_tx_inc_seqno (arc_udptl_tx_t tx)
{

   if (tx) {
      tx->seqno++;
   }
   return tx->seqno;
}


int
arc_udptl_rx_inc_seqno (arc_udptl_rx_t rx)
{

   if (rx) {
      rx->seq[READ_SEQ]++;
   }
   return rx->seq[READ_SEQ];
}


int
arc_udptl_tx (arc_udptl_tx_t tx, char *buff, size_t size, int count)
{

   int rc = -1;
   socklen_t soklen = sizeof (struct sockaddr_in);
   int status = -1;
   int idx;
   uint8_t tmp[MTU_BUFFER_SIZE];
   uint8_t fec[MTU_BUFFER_SIZE];
   int fec_len = 0;
   int32_t len = 0;
   struct timeval tv;

   if (!tx) {
      return -1;
   }

   if (!buff || !size) {
      fprintf (stderr, " %s() failed initial parameter testing, buff=%p size=%zu\n", __func__, buff, size);
      return -1;
   }

   if (size > (MTU_BUFFER_SIZE - 2)) {
      fprintf (stderr, " %s() buffer[%zu] is larger than max size[%d]\n", __func__, size, MTU_BUFFER_SIZE);
      return -1;
   }

   memset (tmp, 0, sizeof (tmp));

   idx = tx->seqno & (MTU_RINGBUFFER_SIZE - 1);

   tx->posts[idx] = size;
   memcpy (tx->ringbuffer[idx], buff, size);

   // sequennce no 

   ARC_UDPTL_SET_SEQNO (tmp, tx->seqno);
   len += 2;

   status = encode_open_type (tmp, &len, (uint8_t *) buff, size);

   if (status == -1) {
      fprintf (stderr, " %s() status=%d len=%d\n", __func__, status, len);
      return -1;
   }

   switch (tx->ecm_mode) {
   case ARC_UDPTL_ECM_NONE:
      //fprintf (stderr, " %s() ecm mode=none\n", __func__);
      tmp[len++] = '\0';
      encode_length (tmp, &len, 0);
      break;
   case ARC_UDPTL_ECM_REDUNDANCY:
      count = 0;
      int i, j;
      //fprintf (stderr, " %s() ecm mode=redundancy len=%d span=%d\n", __func__, len, tx->ecm_span);
      tmp[len++] = 0x00;
      if (tx->seqno > tx->ecm_entries) {
         count = tx->ecm_entries;
      }
      else {
         count = tx->seqno;
      }

      //fprintf (stderr, " %s() encoding redundant packet count=%d idx=%d\n", __func__, count, idx);
      if (encode_length (tmp, &len, count) < 0) {
         fprintf (stderr, " %s() error encoding length\n", __func__);
         return -1;
      }
      if (count == 0) {
         break;
      }
      for (i = 0; i < count; i++) {
         j = (idx - i - 1) & (MTU_RINGBUFFER_SIZE - 1);
         if (len > MTU_BUFFER_SIZE) {
            fprintf (stderr, " len has exeeded buffer size\n");
            return -1;
         }
         // fprintf (stderr, " %s() encoding tx->post=%d j=%d idx=%d len=%d\n", __func__, tx->posts[j], j, idx, len);
         if (encode_open_type (tmp, &len, (uint8_t *) tx->ringbuffer[j], tx->posts[j]) < 0) {
            fprintf (stderr, " %s() error encoding redunant packets idx[%d] count=%d\n", __func__, j, count);
            return -1;
         }
      }
      break;

   case ARC_UDPTL_ECM_FEC:
      ;;                        //fprintf (stderr, " %s() ecm mode=fec\n", __func__);
      int entries = tx->ecm_entries;
      int span = tx->ecm_span;
      int limit;

      if (tx->seqno < tx->ecm_entries * tx->ecm_span) {
         entries = tx->seqno / tx->ecm_span;
         if (tx->seqno < tx->ecm_span) {
            span = 0;
         }
      }

      tmp[len++] = 0x80;
      tmp[len++] = 1;
      tmp[len++] = span & 15;
      tmp[len++] = entries & 15;

      for (i = 0; i < entries; i++) {
         fec_len = 0;
         memset (fec, 0, sizeof (fec));
         limit = (entries + i) & (MTU_RINGBUFFER_SIZE - 1);
         for (j = (limit - span * entries); j != limit; j = (j + entries) & (MTU_RINGBUFFER_SIZE - 1)) {
            // fec_len = build_fec_buff (fec, fec_len, (unsigned char *) tx->ringbuffer[i], tx->posts[i]);
            fec_len = build_fec_buff (fec, fec_len, (unsigned char *) tx->ringbuffer[j], tx->posts[j]);
            // fprintf(stderr, " %s() encoding fec open type, len=%d fec_len=%d idx=%d limit=%d\n", __func__, len, fec_len, j, limit);
            if ((status = encode_open_type (tmp, &len, fec, fec_len)) < 0) {
               fprintf (stderr, " %s() error fec encoding status == -1, returning -1 fec_len=%d\n", __func__, fec_len);
               break;
            }
         }
      }
      break;
   default:
      fprintf (stderr, " %s() unhandled case statement tx->ecm_mode=%d\n", __func__, tx->ecm_mode);
      return -1;
   }

   // 
   //
   // 
   //
   //
   // 
   //
   //
   //
   //

   rc = sendto (tx->sock, tmp, len, 0, (struct sockaddr *) &tx->remote, soklen);
   if (rc <= 0) {
      fprintf (stderr, " %s() error sending len=%d errstr=%s\n", __func__, len, strerror (errno));
      return -1;
   }

   tx->seqno++;


   if (tx->msec_interval > 0) {
      tv.tv_sec = 0;
      tv.tv_usec = (tx->msec_interval * 1000) % 1000000;
      select (0, NULL, NULL, NULL, &tv);
   }


   return rc;
}


int
arc_udptl_rx (arc_udptl_rx_t rx, char *buff, size_t bufsize, int data_type, int field_type)
{

   int rc = -1;
   int bytes;
   int status = -1;
   int idx = 0;
   socklen_t soklen = sizeof (struct sockaddr_in);
   uint8_t tmp[MTU_BUFFER_SIZE];
   // uint8_t fec_repaired[MTU_BUFFER_SIZE];
   int errno;
   // for ifp callback 
   int32_t ifp_len = 0;
   int ifp_seqno = 0;
   const uint8_t *ifp_ptr = NULL;
   // redundant packets 
   const unsigned char *ifp_ptrs[16] = { NULL };
   int32_t ifp_lens[16] = { 0 };
   int32_t fec_idx[16];
   int32_t len = 0;
   uint8_t fec_buf[16][MTU_BUFFER_SIZE];
   uint8_t fec_len[16];
   uint8_t fec_span[16];
   fd_set fdset;
   struct timeval tv;
   int nfds;


   if (!rx) {
      return -1;
   }

   if (!buff || !bufsize) {
      fprintf (stderr, " %s() failed initial parameter testing, buff=%p size=%zu\n", __func__, buff, bufsize);
      return -1;
   }

   // fprintf(stderr, " %s() data type=%d field type=%d\n", __func__, data_type, field_type);

   FD_ZERO (&fdset);
   FD_SET (rx->sock, &fdset);

   tv.tv_sec = 0;
   tv.tv_usec = PACKET_SLEEP;

   if (rx->msec_interval > 0) {
      tv.tv_usec = (rx->msec_interval * 1000) % 1000000;
      nfds = select (rx->sock + 1, &fdset, NULL, NULL, &tv);
      if (nfds < 0) {
         fprintf (stderr, " select returned %d, reason=%s\n", nfds, strerror (errno));
         return nfds;
      }
   }

// fprintf(stderr, " %s() sleeping for %d usecs\n", __func__, (int)tv.tv_usec);

//   nfds = select(rx->sock + 1, &fdset, NULL, NULL, &tv);

//   if(nfds < 0){
//     fprintf(stderr, " select returned %d, reason=%s\n", nfds, strerror(errno));
//     return nfds;
//   }

   bytes = recvfrom (rx->sock, tmp, MTU_BUFFER_SIZE, 0, (struct sockaddr *) &rx->remote, &soklen);
   // bytes = recv(rx->sock, tmp, MTU_BUFFER_SIZE, 0);

   //fprintf(stderr, " %s() sock=%d bytes=%d\n", __func__, rx->sock, bytes);

   if (bytes == -1 && (errno != EAGAIN)) {
      fprintf (stderr, " %s() sock=%d bytes recieved=%d \n", __func__, rx->sock, bytes);
      fprintf (stderr, " %s() sock error =[%s]\n", __func__, strerror (errno));
   }
   else {
      if (bytes > 0) {
         ;;                     // fprintf(stderr, " %s() sock=%d bytes recieved=%d \n", __func__, rx->sock, bytes);
      }
   }

   if (bytes == -1) {
      switch (errno) {
      case EAGAIN:
         return 0;
         break;
      default:
         fprintf (stderr, " %s() recvfrom rc = -1 fd=%d, strerr=%s", __func__, rx->sock, strerror (errno));
         break;
      }
      return -1;
   }

   if (bytes > 0) {

      ifp_ptr = &tmp[0];

      ARC_UDPTL_GET_SEQNO (tmp, ifp_seqno);

      //fprintf(stderr, " DDNDEBUG %s() incoming ifp sequence no = [%d] bytes=%d\n", __func__, ifp_seqno, bytes);

      // check sequence no 
      if (ifp_seqno > (rx->seq[WRITE_SEQ] + MTU_RINGBUFFER_SIZE)
          || ifp_seqno < (rx->seq[WRITE_SEQ] - MTU_RINGBUFFER_SIZE)) {
         fprintf (stderr, " %s() packet looks suspect, sequence no=%d\n", __func__, ifp_seqno);
         return -1;
      }

      rx->seq[WRITE_SEQ] = ifp_seqno;
      idx = rx->seq[WRITE_SEQ] & (MTU_RINGBUFFER_SIZE - 1);

      len += 2;

      ifp_ptr = &tmp[len];

      status = decode_open_type ((unsigned char *) tmp, MTU_BUFFER_SIZE, &len, &ifp_ptr, &ifp_len);
      if (status == -1) {
         fprintf (stderr, "failed to decode packet len\n");
         return -1;
      }
      else {
         ;;                     // fprintf (stderr, " %s() decode_open_type -- bytes=%d ifp_len=%d\n", __func__, bytes, ifp_len);
      }

      if (ifp_len > MTU_BUFFER_SIZE) {
         fprintf (stderr, " %s() ifp packet too large got buffer, ifp_lrn=%d\n", __func__, ifp_len);
         return -1;
      }

      memset (rx->ringbuffer[idx], 0, MTU_BUFFER_SIZE);
      memcpy (rx->ringbuffer[idx], ifp_ptr, ifp_len);
      rx->posts[idx] = ifp_len;
      rx->seq_nos[idx] = ifp_seqno;
      // fprintf (stderr, " %s() copied %d bytes to ringbuffer idx=%d for sequence no. %d\n", __func__, ifp_len, idx, ifp_seqno);
      //fprintf(stderr, " DDNDEBUG %s() incoming ifp idx no = [%d] ifp_seqno=%d ifp_len=%d\n", __func__, idx, ifp_seqno, ifp_len);

   }

   // sequence any redundant ifp packets packets 

   if ((tmp[len++] & 0x80) == 0) {

      //fprintf (stderr, " %s() tmp[%d]=%d indicates ifp redundancy packets\n", __func__, len, tmp[len]);
      int stat;
      int count = 0;
      int i, j;

      stat = decode_length (tmp, MTU_BUFFER_SIZE, &len, &count);

      if (stat < 0) {
         fprintf (stderr, " %s() failed to decode length of redundancy packets \n", __func__);
         return -1;
      }
      else {
         // fprintf (stderr, " %s() redundant packet count=%d stat=%d\n", __func__, count, stat);
         for (i = 0; i < count; i++) {
            stat = decode_open_type (tmp, MTU_BUFFER_SIZE, &len, &ifp_ptrs[i], &ifp_lens[i]);
            // fprintf (stderr, " %s() stat=%d count=%d ifp_len=%d\n", __func__, stat, count, ifp_lens[i]);
            if (stat == -1) {
               break;
            }
         }

         if (len != bytes) {
            fprintf (stderr, " %s() redundancy mode: len != bytes, and it should... len=%d bytes=%d\n", __func__, len, bytes);
         }

         // copy redundant packets 
         if (count > 0 && count < 16) {
            for (i = 0; i < count; i++) {
               j = (idx - i - 1) & (MTU_RINGBUFFER_SIZE - 1);
               // fprintf (stderr, " j = %d idx=%d \n", j, idx);
               if (ifp_ptrs[i] && (ifp_lens[i] > 0) && (rx->posts[j] == 0)) {
                  memcpy (rx->ringbuffer[j], ifp_ptrs[i], ifp_lens[i]);
                  rx->posts[j] = ifp_lens[i];
                  rx->seq_nos[j] = ifp_seqno - i - 1;
                  // fprintf (stderr, " redundant seq no=%d ifp_lens=%d\n", ifp_seqno - i - 1, ifp_lens[i]);
               }
            }
         }
         // end copying redundant packets 
      }
   }
   else {
      //  decode fec packets ?
      int span = 0;
      int entries = 0;
      int stat;

      if ((len + 2) > MTU_BUFFER_SIZE) {
         fprintf (stderr, " %s(%d) \n", __func__, __LINE__);
         return -1;
      }

      if (tmp[len++] != 1) {
         fprintf (stderr, " %s(%d) \n", __func__, __LINE__);
         return -1;
      }

      span = tmp[len++];
      idx = ifp_seqno & (MTU_RINGBUFFER_SIZE - 1);

      memset (fec_idx, 0, sizeof (fec_idx));
      fec_idx[idx] = 1;

      if ((len + 1) > MTU_BUFFER_SIZE) {
         fprintf (stderr, " %s(%d) \n", __func__, __LINE__);
         return -1;
      }

      entries = tmp[len++];
      if (entries >= 16) {
         fprintf (stderr, " %s() unusually high number of entries, more that I plan on handling, returing -1\n", __func__);
         return -1;
      }

      int i;

      memset (fec_len, 0, sizeof (fec_len));
      memset (fec_buf, 0, sizeof (fec_buf));
      memset (fec_span, 0, sizeof (fec_span));

      for (i = 0; i < entries; i++) {

         if ((stat = decode_open_type (tmp, MTU_BUFFER_SIZE, &len, &ifp_ptrs[i], &ifp_lens[i])) != 0) {
            fprintf (stderr, " %s() fec status returned -1, returning -1\n", __func__);
            return -1;
         }

         if (ifp_lens[i] > MTU_BUFFER_SIZE) {
            fprintf (stderr, " %s() ifp_lens[%d]=%d way too big\n", __func__, i, ifp_lens[i]);
            return -1;
         }

         if (ifp_lens[i] < 1024) {
            memcpy (fec_buf[i], ifp_ptrs[i], ifp_lens[i]);
            fec_len[i] = ifp_lens[i];
            fec_span[i] = span;
         }
         else {
            fprintf (stderr, " %s() buffsize too small, exception line=%d\n", __func__, __LINE__);
         }

      }

      // try and patch up any missing slots with fec data 
      idx = ifp_seqno & (MTU_RINGBUFFER_SIZE - 1);
      // fprintf(stderr, " %s() loop entry=%d loop condition=%d\n", __func__, idx, idx - (16 - (span * entries)) & (MTU_RINGBUFFER_SIZE - 1));
      int j = 0;

#define REPAIR_FEC_BUFF
#ifdef REPAIR_FEC_BUFF
      for (i = idx; i != ((idx - (16 - span * entries)) & (MTU_RINGBUFFER_SIZE - 1)); i = (i - 1) & (MTU_RINGBUFFER_SIZE - 1)) {

         if (fec_len[i] <= 0)
            continue;
         // if missing slot fix it up 
         // if(rx->posts[idx] <= 0)
         if (rx->posts[i] > 0) {
            if (repair_fec_buff ((uint8_t *) rx->ringbuffer[i], rx->posts[i], fec_buf[j], fec_len[j], FEC_CHECK) != 0) {
               // fprintf (stderr, " attempting fec recovery needed for ringbuff idx=%d current seq=%d fec idx=%d\n", i, idx, j);
               // repair_fec_buff (rx->ringbuffer[i], rx->posts[i], fec_buf[j], fec_len[j], FEC_REPAIR);
            }
         }
         if (fec_len[j] == 0) {
            break;
         }
         j++;
      }
      // fprintf(stderr, "exited fec recovery loop\n");
#endif
   }
   // end fec 

   // see if it is something we are ready to read 
   // based on our sequence no.
   // idx = rx->seq[READ_SEQ] & (MTU_RINGBUFFER_SIZE - 1);

   idx = rx->seq[READ_SEQ] & (MTU_RINGBUFFER_SIZE - 1);

   if (rx->posts[idx] > 0) {

      //fprintf(stderr, " DDNDEBUG #1 %s() incoming ifp idx no = [%d] ifp_seqno=%d ifp_len=%d\n", __func__, idx, ifp_seqno, ifp_len);
      rx->seq_nos[idx] = ifp_seqno;     //DDN Added
      rc = rx->spandsp_cb (rx->spandsp_data, (unsigned char *) rx->ringbuffer[idx], rx->posts[idx], rx->seq_nos[idx]);
      //fprintf(stderr, " DDNDEBUG #2 %s() incoming ifp idx no = [%d] ifp_seqno=%d ifp_len=%d\n", __func__, idx, ifp_seqno, ifp_len);

      //fprintf(stderr, " %s() cb rc=%d rx->seq[READ_SEQ]=%d posted size=%d\n", __func__, rc, rx->seq[READ_SEQ], rx->posts[idx]);
      //if (rc == 0) { // if it has parsed then it was copied to the ringbuffer 
      rx->posts[idx] = 0;
      rx->seq_nos[idx] = 0;
      rx->seq[READ_SEQ]++;
      //}
   }

   return bytes;
}


int
arc_udptl_rx_close (arc_udptl_rx_t rx)
{

   if (!rx) {
      return -1;
   }

   if (rx->sock != -1) {
      close (rx->sock);
      rx->sock = -1;
   }

   return 0;
}



void
arc_udptl_rx_free (arc_udptl_rx_t rx)
{

   // int i;

   if (!rx) {
      return;
   }

   free (rx);
   return;
}


int
arc_udptl_tx_close (arc_udptl_tx_t tx)
{

   if (!tx) {
      return -1;
   }

   if (tx->sock != -1) {
      close (tx->sock);
      tx->sock = -1;
   }

   return 0;

}

void
arc_udptl_tx_free (arc_udptl_tx_t tx)
{

   if (!tx) {
      return;
   }

   free (tx);

   return;
}


#ifdef MAIN


char string[] = " ****** testing callback routine ***";

int
cb (void *data, char *buf, int len, int seq)
{
   fprintf (stderr, "%s data=%p\n", string, data);
   return 0;
}



int
main (int argc, char **argv)
{

   arc_udptl_tx_t tx = arc_udptl_tx_alloc ();
   arc_udptl_rx_t rx = arc_udptl_rx_alloc ();

   char buff[20] = "";
   int i = 0;
   unsigned short port;

   arc_udptl_tx_init (tx);
   arc_udptl_rx_init (rx);

   arc_udptl_rx_set_debug (rx);
   arc_udptl_tx_set_debug (tx);

   if (arc_udptl_tx_open (tx, 1) == -1) {
      fprintf (stderr, " %s() failed to open udptl sock\n", __func__);
      exit (1);
   }

   if (arc_udptl_rx_open (rx, 1) == -1) {
      fprintf (stderr, " %s() failed to open udptl sock\n", __func__);
      exit (1);
   }

   arc_udptl_tx_setsockinfo (tx, "127.0.0.1", 12345, ARC_UDPTL_LOCAL);
   arc_udptl_tx_setsockinfo (tx, "127.0.0.1", 12345, ARC_UDPTL_REMOTE);
   //
   arc_udptl_rx_clone_symmetric (rx, tx);
   //
   //arc_udptl_rx_setsockinfo (rx, "127.0.0.1", 1236, ARC_UDPTL_LOCAL);
   //arc_udptl_rx_setsockinfo (rx, "127.0.0.1", 1234, ARC_UDPTL_REMOTE);

   arc_udptl_rx_set_spandsp_cb (rx, cb, string);

   for (i = 0; i < 300; i++) {
      arc_udptl_tx (tx, buff, 10, 0);
      arc_udptl_rx (rx, buff, 10, 0, 0);
   }

   arc_udptl_rx_getsockinfo (rx, buff, sizeof (buff), &port, ARC_UDPTL_REMOTE);
   fprintf (stderr, " %s() ip=%s port=%d\n", __func__, buff, port);
   arc_udptl_rx_getsockinfo (rx, buff, sizeof (buff), &port, ARC_UDPTL_LOCAL);
   fprintf (stderr, " %s() ip=%s port=%d\n", __func__, buff, port);
   arc_udptl_tx_getsockinfo (tx, buff, sizeof (buff), &port, ARC_UDPTL_REMOTE);
   fprintf (stderr, " %s() ip=%s port=%d\n", __func__, buff, port);
   arc_udptl_tx_getsockinfo (tx, buff, sizeof (buff), &port, ARC_UDPTL_LOCAL);
   fprintf (stderr, " %s() ip=%s port=%d\n", __func__, buff, port);

   arc_udptl_tx_close (tx);
   arc_udptl_rx_close (rx);

   arc_udptl_tx_free (tx);
   arc_udptl_rx_free (rx);

   return 0;
}



#endif
