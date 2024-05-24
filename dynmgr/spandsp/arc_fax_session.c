#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <spandsp.h>
#include <spandsp/expose.h>
#include <spandsp/fax.h>
#include <sys/time.h>

//
// current max session timeout 15 minuutes 
//

#define ARC_FAX_SESSION_TIMEOUT_MAX (60 * 15)
#define ARC_FAX_SESSION_PACKET_TIMEOUT_MAX (60)

#include "arc_fax_session.h"
#include "arc_udptl.h"



void *T38_CORE_RX = (void *) t38_core_rx_ifp_packet;

struct arc_fax_session_e
{
   struct arc_fax_session_parms_t parms;
   // 
   pthread_mutex_t lock;
   volatile int state;
   char use_locks;
   t30_state_t *t30state;
   fax_state_t fax;
   // t38 
   t38_terminal_state_t t38_terminal_state;
   t38_core_state_t *t38_core_state;
   int (*t38_tx_handler) (t38_core_state_t * s, void *user_data, const uint8_t * buf, int len, int count);
   void *t38_tx_user_data;
   int (*t38_rx_handler) (t38_core_state_t * s, void *user_data, int data_type, int field_type, const uint8_t * buf, int len);
   void *t38_rx_user_data;
   // 
   time_t start;
   time_t last;
   char result[256];
   int ecm_mode;
   int msec_interval;
};


arc_fax_session_t
arc_fax_session_alloc ()
{
   return (calloc (1, sizeof (struct arc_fax_session_e)));
}



int
arc_fax_session_t38_udptl_tx (void *s, void *user_data, const uint8_t * buf, int len, int count)
{

   int rc = -1;
   arc_udptl_tx_t udptl_tx = (arc_udptl_tx_t) user_data;
   // t38_core_state_t *core_state = (t38_core_state_t *)s;
   int i;

   if (udptl_tx == NULL) {
      fprintf (stderr, " %s() udptl transport not set up\n", __func__);
      return -1;
   }

   for (i = 0; i < count; i++) {
      rc = arc_udptl_tx (udptl_tx, (char *) buf, len, count);
      // arc_udptl_tx_inc_seqno (udptl_tx);
   }

   return rc;
}


/*
    return the raw ifp packet or whatever spandsp wants.
    we call the rx_ifp routine ourselves. 
*/


int
arc_fax_session_t38_udptl_rx (void *s, void *user_data, int data_type, int field_type, const uint8_t * buf, int len)
{

   int rc = -1;
   arc_udptl_rx_t udptl_rx = (arc_udptl_rx_t) user_data;
   // t38_core_state_t *core_state = (t38_core_state_t *)s;

   //fprintf (stderr, " %s() called, core state=%p user data=%p data type=%d field type=%d buff=%p, len=%d\n",
   //         __func__, s, user_data, data_type, field_type, buf, len);

   if (udptl_rx == NULL) {
      fprintf (stderr, " %s() udptl transport not set up\n", __func__);
      return -1;
   }

   rc = arc_udptl_rx (udptl_rx, (char *) buf, len, data_type, field_type);

   switch (data_type) {
   case 0x00:
      //fprintf(stderr, " %s() data_type=%02x\n", __func__, data_type);
      break;

   default:
      //fprintf(stderr, " %s() data type=%02x\n", __func__, data_type);
      break;
   }

   switch (field_type) {
   default:
      //fprintf(stderr, " %s() field_type=%d\n", __func__, field_type);
      break;
   }

   return rc;
}


/*
   these are trnasports callbacks for rtp user data will most likely 
   be the session info and from ortp 
*/

int
arc_fax_session_t38_rtp_tx (t38_core_state_t * s, void *user_data, const uint8_t * buf, int len, int count)
{
   int rc = -1;
   fprintf (stderr, " %s() state=%p user data=%p buff=%p len=%d, count=%d\n", __func__, s, user_data, buf, len, count);
   return rc;
}

int
arc_fax_session_t38_rtp_rx (t38_core_state_t * s, void *user_data, int data_type, int field_type, const uint8_t * buf, int len)
{

   int rc = -1;

   fprintf (stderr, " %s() called, core state=%p user data=%p data type=%d field type=%d buff=%p, len=%d\n",
            __func__, s, user_data, data_type, field_type, buf, len);

   return rc;
}

void
arc_fax_session_t38_set_tx_handlers (arc_fax_session_t fs, void *handler, void *data)
{

   if (!fs) {
      return;
   }

   if (!handler) {
      return;
   }

   fs->t38_tx_handler = handler;
   fs->t38_tx_user_data = data;

   return;
}

void
arc_fax_session_t38_set_rx_handlers (arc_fax_session_t fs, void *handler, void *data)
{

   if (!fs) {
      return;
   }

   if (!handler) {
      return;
   }

   fs->t38_rx_handler = handler;
   fs->t38_rx_user_data = data;

   return;
}


static void
phase_e_handler (t30_state_t * s, void *user_data, int result)
{
   t30_stats_t t;
   const char *local_ident;
   const char *far_ident;
   arc_fax_session_t fs = (arc_fax_session_t) user_data;
   char result_str[256];

   t30_get_transfer_statistics (s, &t);
   local_ident = t30_get_tx_ident (s);
   far_ident = t30_get_rx_ident (s);

   switch (result) {
   case T30_ERR_OK:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_OK = 0", "OK");
      fs->state = ARC_FAX_SESSION_SUCCESS;
      break;
   case T30_ERR_CEDTONE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_CEDTONE", "! The CED tone exceeded 5s");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_T0_EXPIRED:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_T0_EXPIRED", "! Timed out waiting for initial communication");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_T1_EXPIRED:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_T1_EXPIRED", "! Timed out waiting for the first message");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_T3_EXPIRED:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_T3_EXPIRED", "! Timed out waiting for procedural interrupt");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_HDLC_CARRIER:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_HDLC_CARRIER", "! The HDLC carrier did not stop in a timely manner");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_CANNOT_TRAIN:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_CANNOT_TRAIN", "! Failed to train with any of the compatible modems");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_OPER_INT_FAIL:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_OPER_INT_FAIL", "! Operator intervention failed");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_INCOMPATIBLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_INCOMPATIBLE", "! Far end is not compatible");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_INCAPABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_INCAPABLE", "! Far end is not able to receive");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_INCAPABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_INCAPABLE", "! Far end is not able to transmit");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_NORESSUPPORT:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_NORESSUPPORT", "! Far end cannot receive at the resolution of the image");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_NOSIZESUPPORT:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_NOSIZESUPPORT", "! Far end cannot receive at the size of image");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_UNEXPECTED:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_UNEXPECTED", "! Unexpected message received");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_BADDCS:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_BADDCS", "! Received bad response to DCS or training");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_BADPG:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_BADPG", "! Received a DCN from remote after sending a page");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_ECMPHD:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_ECMPHD", "! Invalid ECM response received from receiver");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_GOTDCN:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_GOTDCN", "! Received a DCN while waiting for a DIS");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_INVALRSP:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_INVALRSP", "! Invalid response after sending a page");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_NODIS:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_NODIS", "! Received other than DIS while waiting for DIS");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_PHDDEAD:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_PHDDEAD", "! No response after sending a page");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TX_T5EXP:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TX_T5EXP", "! Timed out waiting for receiver ready (ECM mode)");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_ECMPHD:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_ECMPHD", "! Invalid ECM response received from transmitter");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_GOTDCS:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_GOTDCS", "! DCS received while waiting for DTC");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_INVALCMD:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_INVALCMD", "! Unexpected command after page received");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_NOCARRIER:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_NOCARRIER", "! Carrier lost during fax receive");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_NOEOL:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_NOEOL", "! Timed out while waiting for EOL (end Of line)");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_NOFAX:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_NOFAX", "! Timed out while waiting for first line");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_T2EXPDCN:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_T2EXPDCN", "! Timer T2 expired while waiting for DCN");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_T2EXPD:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_T2EXPD", "! Timer T2 expired while waiting for phase D");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_T2EXPFAX:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_T2EXPFAX", "! Timer T2 expired while waiting for fax page");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_T2EXPMPS:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_T2EXPMPS", "! Timer T2 expired while waiting for next fax page");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_T2EXPRR:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_T2EXPRR", "! Timer T2 expired while waiting for RR command");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_DCNWHY:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_DCNWHY", "! Unexpected DCN while waiting for DCS or DIS");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_DCNDATA:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_DCNDATA", "! Unexpected DCN while waiting for image data");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_DCNPHD:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_DCNPHD", "! Unexpected DCN after EOM or MPS sequence");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_DCNRRD:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_DCNRRD", "! Unexpected DCN after RR/RNR sequence");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RX_DCNNORTN:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RX_DCNNORTN", "! Unexpected DCN after requested retransmission");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_FILEERROR:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_FILEERROR", "! TIFF/F file cannot be opened");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_NOPAGE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_NOPAGE", "! TIFF/F page not found");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_BADTIFF:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_BADTIFF", "! TIFF/F format is not compatible");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_BADPAGE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_BADPAGE", "! TIFF/F page number tag missing");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_BADTAG:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_BADTAG", "! Incorrect values for TIFF/F tags");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_BADTIFFHDR:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_BADTIFFHDR", "! Bad TIFF/F header - incorrect values in fields");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_NOMEM:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_NOMEM", "! Cannot allocate memory for more pages");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_RETRYDCN:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_RETRYDCN", "! Disconnected after permitted retries");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_CALLDROPPED:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_CALLDROPPED", "! The call dropped prematurely");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_NOPOLL:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_NOPOLL", "! Poll not accepted");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_IDENT_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_IDENT_UNACCEPTABLE", "! Far end's ident is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_SUB_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_SUB_UNACCEPTABLE", "! Far end's sub-address is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_SEP_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_SEP_UNACCEPTABLE", "! Far end's selective polling address is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_PSA_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_PSA_UNACCEPTABLE", "! Far end's polled sub-address is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_SID_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_SID_UNACCEPTABLE", "! Far end's sender identification is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_PWD_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_PWD_UNACCEPTABLE", "! Far end's password is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_TSA_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_TSA_UNACCEPTABLE",
                "! Far end's transmitting subscriber internet address is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_IRA_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_IRA_UNACCEPTABLE", "! Far end's internet routing address is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_CIA_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_CIA_UNACCEPTABLE", "! Far end's calling subscriber internet address is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   case T30_ERR_ISP_UNACCEPTABLE:
      snprintf (result_str, sizeof (result_str), "%s: %s", "T30_ERR_ISP_UNACCEPTABLE", "! Far end's internet selective polling address is not acceptable");
      fs->state = ARC_FAX_SESSION_FAILURE;
      break;
   default:
      snprintf (result_str, sizeof (result_str), " %s() hit unhandled default case in switch statement", __func__);
      fs->state = ARC_FAX_SESSION_FAILURE;
   }

   // fprintf (stderr, " %s() result=%d result string=[%s]\n", __func__, result, result_str);


   snprintf (result_str, sizeof (result_str),
             " localid[%s], remotid[%s] pages=%d bitrate=%d state=%d result=%d reason=%s",
             local_ident, far_ident, t.pages_rx, t.bit_rate, fs->state, result, t30_completion_code_to_str (result));
   snprintf (fs->result, sizeof (fs->result), "%s", result_str);

   return;
}


static void
arc_fax_session_logger (int lvl, const char *msg)
{
   fprintf (stderr, " %s(lvl=%d) %s", __func__, lvl, msg);
   return;
}

void
arc_fax_session_set_parms (struct arc_fax_session_parms_t *parms, char *dirpath, char *filename, int type, int ecm, int use_t38)
{

   memset (parms, 0, sizeof (struct arc_fax_session_parms_t));

   snprintf (parms->dirpath, sizeof (parms->dirpath), "%s", dirpath);
   snprintf (parms->filename, sizeof (parms->filename), "%s", filename);
   snprintf (parms->stationid, sizeof (parms->stationid), "Arc FAX Station");
   snprintf (parms->headernfo, sizeof (parms->headernfo), "Arc FAX Header Info");
   // only on outbound t38 can be used from the get go
   parms->use_t38 = use_t38;
   parms->type = type;
   parms->ecm = ecm;

   return;
}

void
arc_fax_session_set_debug (struct arc_fax_session_parms_t *parms)
{
   if (parms) {
      parms->debug = 1;
   }
   return;
}


int
arc_fax_session_check_state (arc_fax_session_t fs)
{

   /*
      compare time started and last packet here
      and overall timeout and remove them from the 
      tx and rx routines 
    */


   switch (fs->state) {
   case ARC_FAX_SESSION_STATE_START:
   case ARC_FAX_SESSION_T30_AUDIO_START:
   case ARC_FAX_SESSION_T38_START:
      return 0;
      break;

   case ARC_FAX_SESSION_OVERALL_TIMEOUT:
      return fs->state;
      break;
   case ARC_FAX_SESSION_PACKET_TIMEOUT:
      return fs->state;
      break;
   case ARC_FAX_SESSION_SUCCESS:
      return fs->state;
      break;
   case ARC_FAX_SESSION_FAILURE:
      return fs->state;
      break;
   case ARC_FAX_SESSION_END:
      return fs->state;
      break;
   default:
      fprintf (stderr, " %s() hit default case, returning 0 anyway stat=%d\n", __func__, fs->state);
      break;
   }

   return 0;

}

static int
arc_fax_init_t38 (arc_fax_session_t fs, struct arc_fax_session_parms_t *parms)
{

   int rc = -1;
   char path[1024];

   if (fs->parms.use_t38) {

      memset (&fs->t38_terminal_state, 0, sizeof (fs->t38_terminal_state));

      if (fs->t38_tx_handler && fs->t38_tx_user_data) {

         switch (fs->parms.type) {
         case ARC_FAX_SESSION_INBOUND:
            t38_terminal_init (&fs->t38_terminal_state, 0, fs->t38_tx_handler, fs->t38_tx_user_data);

            break;
         case ARC_FAX_SESSION_OUTBOUND:
            t38_terminal_init (&fs->t38_terminal_state, 1, fs->t38_tx_handler, fs->t38_tx_user_data);

            break;
         default:
            fprintf (stderr, " %s() hit default case \n", __func__);
         }
      }
      else {
         fprintf (stderr, " %s() before setting t38 you need to set tx and rx handlers\n", __func__);
         return -1;
      }

      if (fs->parms.debug) {
         span_log_set_message_handler (&fs->t38_terminal_state.logging, arc_fax_session_logger);
         span_log_set_level (&fs->t38_terminal_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
      }


      // fs->t38_core_state = &fs->t38_terminal_state.t38_fe.t38;
      fs->t38_core_state = t38_terminal_get_t38_core_state (&fs->t38_terminal_state);

      if (fs->t38_core_state) {
         // t38_core_init(fs->t38_core_state, NULL, arc_fax_rx_t38, NULL, NULL, arc_fax_tx_t38, NULL);
         //
         if (fs->parms.t38_max_datagram > 0) {
            t38_set_max_datagram_size (fs->t38_core_state, fs->parms.t38_max_datagram);
         }
         else {
#define T38_MAX_DATAGRAM 2000
            t38_set_max_datagram_size (fs->t38_core_state, T38_MAX_DATAGRAM);
            fs->parms.t38_max_datagram = T38_MAX_DATAGRAM;
         }
         t38_set_fill_bit_removal (fs->t38_core_state, TRUE);
         t38_set_mmr_transcoding (fs->t38_core_state, TRUE);
         t38_set_jbig_transcoding (fs->t38_core_state, TRUE);
         t38_set_t38_version (fs->t38_core_state, fs->parms.t38_fax_version);

         t38_set_data_rate_management_method (fs->t38_core_state, fs->parms.t38_fax_rate_mgmt);
         t38_set_data_transport_protocol (fs->t38_core_state, fs->parms.t38_fax_transport);
         //
         if (fs->parms.debug) {
            span_log_set_message_handler (&fs->t38_terminal_state.logging, arc_fax_session_logger);
            span_log_set_level (&fs->t38_terminal_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

            span_log_set_message_handler (&fs->t38_core_state->logging, arc_fax_session_logger);
            span_log_set_level (&fs->t38_core_state->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
         }
      }
      else {
         fprintf (stderr, " %s() core state set to null !!!\n", __func__);
      }
   }

   fs->t30state = t38_terminal_get_t30_state (&fs->t38_terminal_state);

   if (fs->t30state != NULL) {

      t30_set_tx_ident (fs->t30state, fs->parms.stationid);
      t30_set_tx_page_header_info (fs->t30state, fs->parms.headernfo);

      snprintf (path, sizeof (path), "%s", fs->parms.filename);

      t30_set_phase_e_handler (fs->t30state, phase_e_handler, fs);

      t30_set_ecm_capability (fs->t30state, fs->parms.ecm);

      t30_set_supported_compressions (fs->t30state, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);

      t30_set_supported_image_sizes (fs->t30state,
                                     T30_SUPPORT_US_LETTER_LENGTH |
                                     T30_SUPPORT_US_LEGAL_LENGTH |
                                     T30_SUPPORT_UNLIMITED_LENGTH | T30_SUPPORT_215MM_WIDTH | T30_SUPPORT_255MM_WIDTH | T30_SUPPORT_303MM_WIDTH);
      t30_set_supported_resolutions (fs->t30state,
                                     T30_SUPPORT_STANDARD_RESOLUTION |
                                     T30_SUPPORT_FINE_RESOLUTION | T30_SUPPORT_SUPERFINE_RESOLUTION | T30_SUPPORT_R8_RESOLUTION | T30_SUPPORT_R16_RESOLUTION);



      switch (fs->parms.type) {
      case ARC_FAX_SESSION_INBOUND:
         //fprintf (stderr, " %s() setting up file with path %s for inbound fax rx\n", __func__, path);
         t30_set_rx_file (fs->t30state, path, -1);
         break;
      case ARC_FAX_SESSION_OUTBOUND:
         // fprintf (stderr, " %s() setting up file with path %s for outbound fax tx\n", __func__, path);
         t30_set_tx_file (fs->t30state, path, -1, -1);
         break;
      default:
         fprintf (stderr, " %s() hit default case in handling fax file [%s]\n", __func__, path);
         return -1;
         break;
      }

      if (fs->parms.debug != 0) {
         span_log_set_message_handler (&fs->t30state->logging, arc_fax_session_logger);
         span_log_set_level (&fs->t30state->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
      }

      return 0;

   }

   return rc;
}


static int
arc_fax_init_audio (arc_fax_session_t fs, struct arc_fax_session_parms_t *parms)
{


   int rc = -1;
   char path[1024];

   switch (fs->parms.type) {

   case ARC_FAX_SESSION_INBOUND:
      fprintf (stderr, " %s() setting up for inbound FAX session\n", __func__);
      if (fax_init (&fs->fax, 0) == NULL) {
         fprintf (stderr, " %s() some kind of problem happened for INBOUND faxes\n", __func__);
         return -1;
      }
      break;
   case ARC_FAX_SESSION_OUTBOUND:
      fprintf (stderr, " %s() setting up for outbound FAX session\n", __func__);
      if (fax_init (&fs->fax, 1) == NULL) {
         fprintf (stderr, " %s() some kind of problem happened for OUTBOUND faxes\n", __func__);
         return -1;
      }
      break;
   default:
      fprintf (stderr, " %s() hit default case in handling fax type\n", __func__);
      return -1;
      break;
   }

   if (fs->parms.debug) {
      span_log_set_message_handler (&fs->fax.logging, arc_fax_session_logger);
      span_log_set_level (&fs->fax.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
   }

   fax_set_transmit_on_idle (&fs->fax, TRUE);

   fs->t30state = fax_get_t30_state (&fs->fax);

   if (fs->t30state != NULL) {

      t30_set_tx_ident (fs->t30state, fs->parms.stationid);
      t30_set_tx_page_header_info (fs->t30state, fs->parms.headernfo);

      snprintf (path, sizeof (path), "%s", fs->parms.filename);

      t30_set_phase_e_handler (fs->t30state, phase_e_handler, fs);

      t30_set_ecm_capability (fs->t30state, fs->parms.ecm);

      t30_set_supported_compressions (fs->t30state, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);

      t30_set_supported_image_sizes (fs->t30state,
                                     T30_SUPPORT_US_LETTER_LENGTH |
                                     T30_SUPPORT_US_LEGAL_LENGTH |
                                     T30_SUPPORT_UNLIMITED_LENGTH | T30_SUPPORT_215MM_WIDTH | T30_SUPPORT_255MM_WIDTH | T30_SUPPORT_303MM_WIDTH);
      t30_set_supported_resolutions (fs->t30state,
                                     T30_SUPPORT_STANDARD_RESOLUTION |
                                     T30_SUPPORT_FINE_RESOLUTION | T30_SUPPORT_SUPERFINE_RESOLUTION | T30_SUPPORT_R8_RESOLUTION | T30_SUPPORT_R16_RESOLUTION);

      span_log_set_message_handler (&fs->t30state->logging, arc_fax_session_logger);
      span_log_set_level (&fs->t30state->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

      switch (fs->parms.type) {
      case ARC_FAX_SESSION_INBOUND:
         fprintf (stderr, " %s() setting up file with path %s for inbound fax rx\n", __func__, path);
         t30_set_rx_file (fs->t30state, path, -1);
         break;
      case ARC_FAX_SESSION_OUTBOUND:
         fprintf (stderr, " %s() setting up file with path %s for outbound fax tx\n", __func__, path);
         t30_set_tx_file (fs->t30state, path, -1, -1);
         break;
      default:
         fprintf (stderr, " %s() hit default case in handling fax file [%s]\n", __func__, path);
         return -1;
         break;
      }
      rc = 0;
   }

   return rc;
}




int
arc_fax_session_init (arc_fax_session_t fs, struct arc_fax_session_parms_t *parms, int use_locks)
{

   int rc = -1;

   if (!parms || !fs) {
      fprintf (stderr, " %s() fs[%p] or parms[%p] are null, cannot continue\n", __func__, fs, parms);
      return -1;
   }

   if (use_locks) {
      pthread_mutex_init (&fs->lock, NULL);
      fs->use_locks++;
   }

   memcpy (&fs->parms, parms, sizeof (struct arc_fax_session_parms_t));

   // assign initial state and initialize  

   if (!fs->parms.use_t38) {
      fs->state = ARC_FAX_SESSION_T30_AUDIO_START;
      rc = arc_fax_init_audio (fs, &fs->parms);
   }
   else {
      fs->state = ARC_FAX_SESSION_T38_START;
      rc = arc_fax_init_t38 (fs, &fs->parms);
   }

   // default value for packetization 
   fs->msec_interval = 0;

   return rc;
}


int
arc_fax_session_process_buff (arc_fax_session_t fs, char *buff, size_t buffsize, int opts)
{

   int rc = -1;
   time_t now;

   if (!fs) {
      fprintf (stderr, " %s() fs=%p returnng -1\n", __func__, fs);
      return -1;
   }

   if (fs->use_locks) {
      pthread_mutex_lock (&fs->lock);
   }

   if (fs->start == 0) {
      time (&fs->start);
   }

   time (&now);


   // this can be set by default or set on fly for receiving 
   if (fs->state == ARC_FAX_SESSION_T38_START) {

      t38_terminal_send_timeout (&fs->t38_terminal_state, fs->msec_interval * (8000 / 1000));

      fs->t38_core_state = &fs->t38_terminal_state.t38_fe.t38;

      if (fs->t38_core_state != NULL) {
         if (fs->t38_rx_handler && fs->t38_rx_user_data) {
            rc = fs->t38_rx_handler (fs->t38_core_state, fs->t38_rx_user_data, 0, 0, (unsigned char *) buff, buffsize);
            time (&fs->last);
         }
         else {
            fprintf (stderr, " %s() handler not set !!!\n", __func__);
         }
      }
      else {
         fprintf (stderr, " %s() t38 core state set to NULL !!\n", __func__);
      }
   }
   else if (fs->state == ARC_FAX_SESSION_T30_AUDIO_START) {
      switch (fs->parms.type) {
      case ARC_FAX_SESSION_INBOUND:
         rc = fax_rx (&fs->fax, (short *) buff, buffsize / 2);
         rc = fax_tx (&fs->fax, (short *) buff, buffsize / 2);
         time (&fs->last);
         break;
      case ARC_FAX_SESSION_OUTBOUND:
         rc = fax_rx (&fs->fax, (short *) buff, buffsize / 2);
         rc = fax_tx (&fs->fax, (short *) buff, buffsize / 2);
         time (&fs->last);
         break;
      default:
         fprintf (stderr, " %s() hit default case in handling faw session type\n", __func__);
         break;
      }
   }
   else {
      fprintf (stderr, " %s() invalid FAX session state %d, you should not see this\n", __func__, fs->state);
      return -1;
   }

   if ((fs->start + ARC_FAX_SESSION_TIMEOUT_MAX) < fs->last) {
      fprintf (stderr, " %s() triggering a session timeout start=%d + %d last=%d\n", __func__, (int) fs->start, ARC_FAX_SESSION_TIMEOUT_MAX, (int) fs->last);
      fs->state = ARC_FAX_SESSION_OVERALL_TIMEOUT;
   }

   if ((fs->last + ARC_FAX_SESSION_PACKET_TIMEOUT_MAX) < now) {
      fprintf (stderr, " %s() trigering a packet timout for fax transmission, last=%d now=%d\n", __func__, (int) fs->last, (int) now);
      fs->state = ARC_FAX_SESSION_PACKET_TIMEOUT;
   }

   if (fs->use_locks) {
      pthread_mutex_unlock (&fs->lock);
   }

   //fprintf (stderr, " %s(%d) rc=%d\n", __func__, fs->parms->type, rc);

   return rc;
}


int
arc_fax_session_close (arc_fax_session_t fs)
{

   int rc = -1;

   if (fs->use_locks) {
      pthread_mutex_lock (&fs->lock);
   }

   fs->state = ARC_FAX_SESSION_END;

   if (fs->use_locks) {
      pthread_mutex_unlock (&fs->lock);
   }

   return rc;
}

void *
arc_fax_session_get_t38_state (arc_fax_session_t fs)
{

   void *rc = NULL;

   if (fs != NULL && fs->t38_core_state != NULL) {
      rc = (void *) fs->t38_core_state;
   }

   return rc;
}


int
arc_fax_session_get_resultstr (arc_fax_session_t fs, char *buff, size_t size)
{

   int rc = -1;

   if (!buff || !size) {
      return -1;
   }

   if (fs && fs->result[0]) {
      snprintf (buff, size, "%s", fs->result);
      rc = 0;
   }

   return rc;
}

void
arc_fax_session_packet_interval (arc_fax_session_t fs, int msecs)
{

   if (msecs < 0 || msecs > 1000) {
      fs->msec_interval = 0;
   }
   else {
      fs->msec_interval = msecs;
   }

   return;
}


void
arc_fax_session_free (arc_fax_session_t fs)
{

   if (!fs) {
      return;
   }

   if (fs->use_locks) {
      pthread_mutex_lock (&fs->lock);
   }

   t38_terminal_release (&fs->t38_terminal_state);
   fax_release (&fs->fax);

   free (fs);

   return;
}



#ifdef MAIN
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/soundcard.h>

#include "arc_udptl.h"


int
audio_init (const char *dev, int channels, int wordsize, int bitrate, int mode)
{

   int fd;
   int status;
   int arg;

   fd = open (dev, mode);

   if (fd > 0) {

      /* call ioctls on for word size and bitrate */

      arg = channels;

      status = ioctl (fd, SOUND_PCM_WRITE_CHANNELS, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set to single channel: reason = %s", __func__, strerror (errno));
         goto error;
      }

      arg = wordsize;

      status = ioctl (fd, SOUND_PCM_WRITE_BITS, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set word size for audio device reason = %s", __func__, strerror (errno));
         goto error;
      }

      arg = bitrate;

      status = ioctl (fd, SOUND_PCM_WRITE_RATE, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set bit rate for audio device reason = %s", __func__, strerror (errno));
         goto error;
      }
   }

   return fd;

 error:
   close (fd);
   return -1;
}



#define SPOOLPATH "/tmp"

//
// span DSP uses a lot of opaque types
// I had to wrap them the same way because I was 
// having compilation problems 
//

#define _XOPEN_SOURCE 500
#include <unistd.h>

#include <getopt.h>
#include <libgen.h>


void
usage (char *progname, int status)
{

   fprintf (stderr, "\n");

   fprintf (stderr, " %s  spandsp FAX Test Driver use this to add or debug FAX features \n\n", progname);
   fprintf (stderr, " usage: %s <various options>\n\n", progname);
   fprintf (stderr, "  -i infile \n");
   fprintf (stderr, "  -o outfile \n");
   fprintf (stderr, "  -e t38 ecm mode (0,1,2) \n");
   fprintf (stderr, "  -E t30 ecm mode \n");
   fprintf (stderr, "  -t use t38 \n");
   fprintf (stderr, "  -s  span width \n");
   fprintf (stderr, "  -S  ecm  width \n");
   fprintf (stderr, "  -a  audio device name, ex /dev/dsp\n");
   fprintf (stderr, "  -d  turn on debugging\n");
   fprintf (stderr, "\n");

   exit (status);
}

struct opts_t
{
   int debug;
   int use_t38;
   int t38_ecm_mode;
   int t38_span;
   int t38_entries;
   int t30_ecm;
   char infile[256];
   char outfile[256];
   char audio_dev[256];
} opts;

int
get_opts (struct opts_t *opts, int argc, char **argv)
{

   int rc = -1;

   if (opts) {
      memset (opts, 0, sizeof (struct opts_t));
   }
   else {
      return -1;
   }

   // some defaults 
   opts->use_t38 = 0;
   opts->t30_ecm = 0;
   opts->t38_ecm_mode = ARC_UDPTL_ECM_NONE;
   opts->t38_span = 0;

   int c;
   fprintf (stderr, "\n\n");

   while (1) {

      c = getopt (argc, argv, "i:o:ts:S:Ee:a:d");

      if (c == -1) {
         break;
      }

      switch (c) {
      case 'a':
         snprintf (opts->audio_dev, 256, "%s", optarg);
         fprintf (stderr, " using %s as audio device optarg=%s\n", opts->audio_dev, optarg);
         break;
      case 'd':
         opts->debug++;
         break;
      case 'i':
         snprintf (opts->infile, 256, "%s", optarg);
         fprintf (stderr, " using infile=%s\n", opts->infile);
         break;
      case 'o':
         snprintf (opts->outfile, 256, "%s", optarg);
         fprintf (stderr, " using outfile=%s\n", opts->outfile);
         break;
      case 't':
         opts->use_t38 = 1;
         if (opts->use_t38) {
            fprintf (stderr, " using t38 encoding\n");
         }
         else {
            fprintf (stderr, " using t30 audio encoding\n");
         }
         break;
      case 's':
         opts->t38_span = atoi (optarg);
         fprintf (stderr, " span for redundancy mode=%d\n", opts->t38_span);
         break;
      case 'S':
         opts->t38_entries = atoi (optarg);
         fprintf (stderr, " entries for redundancy mode=%d\n", opts->t38_entries);
         break;
      case 'E':
         opts->t30_ecm = 1;
         fprintf (stderr, " t30 core ecm mode is %s\n", (opts->t30_ecm ? "yes" : "no"));
         break;
      case 'e':
         opts->t38_ecm_mode = atoi (optarg);
         break;
         /* everything else */
      case '?':
      case 'h':
      default:
         usage (basename (argv[0]), 0);
         break;
      }
   }

   return rc;
}


int
main (int argc, char **argv)
{

   int audio_fd = -1;
   int rc;
   char result[1024];
   arc_udptl_rx_t in_rx = arc_udptl_rx_alloc ();
   arc_udptl_tx_t in_tx = arc_udptl_tx_alloc ();

   arc_udptl_rx_t out_rx = arc_udptl_rx_alloc ();
   arc_udptl_tx_t out_tx = arc_udptl_tx_alloc ();

   char buff[320] = "";

   arc_fax_session_t infax = arc_fax_session_alloc ();
   arc_fax_session_t outfax = arc_fax_session_alloc ();

   if (argc < 3) {
      usage (basename (argv[0]), 1);
   }

   get_opts (&opts, argc, argv);

   arc_udptl_rx_init (in_rx);
   arc_udptl_tx_init (in_tx);
   arc_udptl_rx_init (out_rx);
   arc_udptl_tx_init (out_tx);

   // arc_udptl_rx_set_ecm (in_rx, opts.t38_ecm_mode, opts.t38_span);
   arc_udptl_tx_set_ecm (in_tx, opts.t38_ecm_mode, opts.t38_span, opts.t38_entries);
   // arc_udptl_rx_set_ecm (out_rx, opts.t38_ecm_mode, opts.t38_span);
   arc_udptl_tx_set_ecm (out_tx, opts.t38_ecm_mode, opts.t38_span, opts.t38_entries);

   if (opts.debug) {
      arc_udptl_tx_set_debug (in_tx);
      arc_udptl_rx_set_debug (in_rx);
      arc_udptl_tx_set_debug (out_tx);
      arc_udptl_rx_set_debug (out_rx);
   }

   arc_udptl_rx_open (in_rx, 1);
   arc_udptl_tx_open (in_tx, 1);
   arc_udptl_rx_open (out_rx, 1);
   arc_udptl_tx_open (out_tx, 1);

   arc_udptl_rx_setsockinfo (in_rx, "127.0.0.1", 12346, ARC_UDPTL_LOCAL);
   arc_udptl_rx_setsockinfo (in_rx, "127.0.0.1", 12347, ARC_UDPTL_REMOTE);
   arc_udptl_rx_bind (in_rx);
   arc_udptl_tx_clone_symmetric (in_tx, in_rx);

   arc_udptl_rx_setsockinfo (out_rx, "127.0.0.1", 12347, ARC_UDPTL_LOCAL);
   arc_udptl_rx_setsockinfo (out_rx, "127.0.0.1", 12346, ARC_UDPTL_REMOTE);
   arc_udptl_rx_bind (out_rx);
   arc_udptl_tx_clone_symmetric (out_tx, out_rx);

   struct arc_fax_session_parms_t inparms;
   struct arc_fax_session_parms_t outparms;

   if (opts.audio_dev[0] != 0) {
      audio_fd = audio_init (opts.audio_dev, 1, 16, 8000, O_WRONLY);
   }

   arc_fax_session_set_parms (&inparms, SPOOLPATH, opts.outfile, ARC_FAX_SESSION_INBOUND, opts.t30_ecm, opts.use_t38);
   arc_fax_session_set_parms (&outparms, SPOOLPATH, opts.infile, ARC_FAX_SESSION_OUTBOUND, opts.t30_ecm, opts.use_t38);
   //
   if (opts.debug != 0) {
      arc_fax_session_set_debug (&inparms);
      arc_fax_session_set_debug (&outparms);
   }
   //
   arc_fax_session_t38_set_tx_handlers (infax, (void *) arc_fax_session_t38_udptl_tx, (void *) in_tx);
   arc_fax_session_t38_set_rx_handlers (infax, (void *) arc_fax_session_t38_udptl_rx, (void *) in_rx);
   //
   arc_fax_session_t38_set_tx_handlers (outfax, (void *) arc_fax_session_t38_udptl_tx, (void *) out_tx);
   arc_fax_session_t38_set_rx_handlers (outfax, (void *) arc_fax_session_t38_udptl_rx, (void *) out_rx);
   // 
   arc_fax_session_init (infax, &inparms, 0);
   arc_fax_session_init (outfax, &outparms, 0);
   // 
   arc_udptl_rx_set_spandsp_cb (in_rx, (void *) t38_core_rx_ifp_packet, arc_fax_session_get_t38_state (infax));
   arc_udptl_rx_set_spandsp_cb (out_rx, (void *) t38_core_rx_ifp_packet, arc_fax_session_get_t38_state (outfax));

   arc_udptl_tx_set_packet_interval (in_tx, 30);
   arc_udptl_tx_set_packet_interval (out_tx, 30);
   arc_udptl_rx_set_packet_interval (in_rx, 30);
   arc_udptl_rx_set_packet_interval (out_rx, 30);

   arc_fax_session_packet_interval (outfax, 30);
   arc_fax_session_packet_interval (infax, 30);

   while (!arc_fax_session_check_state (outfax)
          && !arc_fax_session_check_state (infax)) {

      rc = arc_fax_session_process_buff (outfax, buff, sizeof (buff), 0);
      //fprintf(stderr, " %s() rc1=%d\n", __func__, rc);
      rc = arc_fax_session_process_buff (infax, buff, sizeof (buff), 0);
      //fprintf(stderr, " %s() rc2=%d\n", __func__, rc);

      // if audio present 
      if (audio_fd != -1) {
         write (audio_fd, buff, sizeof (buff));
      }
      else {
         ;;                     // usleep (200000);
      }
   }

   arc_fax_session_get_resultstr (infax, result, sizeof (result));
   fprintf (stderr, " %s() infax result str = %s\n", __func__, result);
   arc_fax_session_get_resultstr (outfax, result, sizeof (result));
   fprintf (stderr, " %s() outfax result str = %s\n", __func__, result);

   arc_fax_session_close (infax);
   arc_fax_session_close (outfax);

   arc_fax_session_free (infax);
   arc_fax_session_free (outfax);

   arc_udptl_rx_close (in_rx);
   arc_udptl_tx_close (in_tx);
   arc_udptl_rx_close (out_rx);
   arc_udptl_tx_close (out_tx);

   arc_udptl_rx_free (in_rx);
   arc_udptl_tx_free (in_tx);
   arc_udptl_rx_free (out_rx);
   arc_udptl_tx_free (out_tx);

   return -1;

}

#endif
