#include <stdio.h>
#include <srtp/srtp.h>
#include <ortp.h>


int
arc_srtp_lib_init ()
{

  int rc = -1;

  rc = srtp_init ();

  if (rc) {
    fprintf (stderr, " %s() failed to init srtp library\n", __func__);
  }

  return rc;
}

char *
arc_srtp_err_status_to_str (int err)
{

  char *rc = "None specified...";

  char *rc_codes[] = {
    " err_status_ok: nothing to report",
    " err_status_fail: unspecified failure ",
    " err_status_bad_param: unsupported parameter",
    " err_status_alloc_fail: couldn't allocate memory",
    " err_status_dealloc_fail: couldn't deallocate properly",
    " err_status_init_fail: couldn't initialize",
    " err_status_terminus: can't process as much data as requested",
    " err_status_auth_fail: authentication failure",
    " err_status_cipher_fail: cipher failure",
    " err_status_replay_fail: replay check failed (bad index)",
    " err_status_replay_old: replay check failed (index too old)",
    " err_status_algo_fail: algorithm failed test routine",
    " err_status_no_such_op: unsupported operation",
    " err_status_no_ctx: no appropriate context found",
    " err_status_cant_check: unable to perform desired validation",
    " err_status_key_expired: can't use key any more",
    " err_status_socket_err: error in use of socket",
    " err_status_signal_err: error in use POSIX signals",
    " err_status_nonce_bad: nonce check failed",
    " err_status_read_fail: couldn't read data",
    " err_status_write_fail: couldn't write data",
    " err_status_parse_err: error pasring data",
    " err_status_encode_err: error encoding data",
    " err_status_semaphore_err: error while using semaphores",
    " err_status_pfkey_err: error while using pfkey"
  };

  if (err >= 0 && err < 25) {
    rc = rc_codes[err];
  }

  return rc;
}


void
arc_srtp_lib_free ()
{
  // don't see a free for this and it reports a little memory leak   
  return;
}

struct arc_srtp_t
{
  char key[256];
  size_t keylen;
  srtp_t session;
  srtp_policy_t policy;
  crypto_policy_t crypto;
};

struct arc_srtp_t *
arc_srtp_alloc ()
{

  struct arc_srtp_t *rc = NULL;
  rc = calloc (1, sizeof (struct arc_srtp_t));
  return rc;
}

void
arc_srtp_free (struct arc_srtp_t *srtp)
{

  if (!srtp) {
    return;
  }
  srtp_dealloc (srtp->session);
  free (srtp);
}

enum arc_srtp_ssrc_type_e
{
  ARC_SRTP_SSRC_UNDEFINED = ssrc_undefined,
  ARC_SRTP_SSRC_SPECIFIC = ssrc_specific,
  ARC_SRTP_SSRC_ANY_INBOUND = ssrc_any_inbound,
  ARC_SRTP_SSRC_ANY_OUTBOUND = ssrc_any_outbound
};


int
arc_srtp_add_policy (struct arc_srtp_t *srtp, uint32_t ssrc, int type, unsigned char *key, int keylen)
{

  int rc = -1;

  if (!srtp || !key) {
    return -1;
  }

  srtp->policy.ssrc.type = type;
  srtp->policy.ssrc.value = ssrc;

  snprintf (srtp->key, sizeof (srtp->key), "%s", key);

  srtp->policy.key = (unsigned char *)srtp->key;
  srtp->policy.next = NULL;

  return rc;
}

enum arc_crypto_type_e
{
  ARC_SRTP_CRYPTO_SET_DEFAULT = 0,
  ARC_SRTP_CRYPTO_TYPE_AES_CM_128_HMAC_32,
  ARC_SRTP_CRYPTO_END
};

int
arc_srtp_add_ssrc (struct arc_srtp_t *srtp, uint32_t ssrc, enum arc_srtp_ssrc_type_e type)
{

  int rc = -1;

  if (!srtp) {
    fprintf (stderr, " %s() \n", __func__);
    return -1;
  }

  switch (type) {
  case ARC_SRTP_SSRC_UNDEFINED:
    srtp->policy.ssrc.type = ssrc_undefined;
    srtp->policy.ssrc.value = ssrc;
    rc = 0;
    break;
  case ARC_SRTP_SSRC_SPECIFIC:
    srtp->policy.ssrc.type = ssrc_specific;
    srtp->policy.ssrc.value = ssrc;
    rc = 0;
    break;
  case ARC_SRTP_SSRC_ANY_INBOUND:
    srtp->policy.ssrc.type = ssrc_any_inbound;
    srtp->policy.ssrc.value = ssrc;
    rc = 0;
    break;
  case ARC_SRTP_SSRC_ANY_OUTBOUND:
    srtp->policy.ssrc.type = ssrc_any_outbound;
    srtp->policy.ssrc.value = ssrc;
    rc = 0;
    break;
  default:
    fprintf (stderr, " %s() unknown ssrc type !, returning -1\n", __func__);
    break;
  }

  return rc;
}


/* convert key and store in alloced memory */

int
arc_srtp_set_key (struct arc_srtp_t *srtp, char *key, int keylen)
{

  int rc = -1;

  if (!srtp) {
    fprintf (stderr, " %s() no srtp struct supplied, cannot continue\n", __func__);
    return -1;
  }

  if (!key) {
    fprintf (stderr, " %s() no srtp key supplied, cannot continue\n", __func__);
    return -1;
  }

  rc = hex_string_to_octet_string (srtp->key, key, keylen);
  return rc;
}

enum arc_srtp_cipher_type_e
{
  ARC_SRTP_CIPHER_TYPE_END
};

int
arc_srtp_rtp_set_cipher (struct arc_srtp_t *srtp, enum arc_srtp_cipher_type_e type, int keylen)
{

  int rc = -1;

  if (!srtp) {
    fprintf (stderr, " %s() \n", __func__);
    return -1;
  }

  switch (type) {

  default:
    fprintf (stderr, " %s() hit dfault case in selecting type, returning -1\n", __func__);
    break;
  }

  return rc;
}

enum arc_srtp_auth_type_e
{
  ARC_SRTP_AUTH_TYPE_END
};

int
arc_srtp_rtp_set_auth (struct arc_srtp_t *srtp, enum arc_srtp_auth_type_e type, int auth_keylen)
{

  if (!srtp) {
    fprintf (stderr, " %s() no srtp context supllied, returning -1\n", __func__);
    return -1;
  }

  switch (type) {


  default:
    fprintf (stderr, " %s() default case in selecting type, returning -1", __func__);
  }


  return 0;
}


/*
 * We won't be using rtcp so we just set them to all the defaults 
 * so the lib works... but we won't be using it 
 */

int
arc_srtp_rtcp_set_defaults (struct arc_srtp_t *srtp)
{


  if (!srtp) {
    fprintf (stderr, " %s() no srtp structure supplied, cannot continue...\n", __func__);
    return -1;
  }

  srtp->policy.rtcp.cipher_type = NULL_CIPHER;
  srtp->policy.rtcp.cipher_key_len = 0;
  srtp->policy.rtcp.auth_type = NULL_AUTH;
  srtp->policy.rtcp.auth_key_len = 0;
  srtp->policy.rtcp.auth_tag_len = 0;
  srtp->policy.rtcp.sec_serv = sec_serv_none;

  return 0;
}



static int
arc_setup_context (struct arc_srtp_t *srtp, unsigned char *key, uint32_t ssrc)
{

  int rc = 0;

  // ssrc 
  srtp->policy.key = (uint8_t *) key;
  srtp->policy.ssrc.type = ssrc_specific;
  srtp->policy.ssrc.value = ssrc;
  // rtp 
  srtp->policy.rtp.cipher_type = NULL_CIPHER;
  srtp->policy.rtp.cipher_key_len = 0;
  srtp->policy.rtp.auth_type = NULL_AUTH;
  srtp->policy.rtp.auth_key_len = 0;
  srtp->policy.rtp.auth_tag_len = 0;
  srtp->policy.rtp.sec_serv = sec_serv_none;
  // rtcp 
  srtp->policy.rtcp.cipher_type = NULL_CIPHER;
  srtp->policy.rtcp.cipher_key_len = 0;
  srtp->policy.rtcp.auth_type = NULL_AUTH;
  srtp->policy.rtcp.auth_key_len = 0;
  srtp->policy.rtcp.auth_tag_len = 0;
  srtp->policy.rtcp.sec_serv = sec_serv_none;
  // structure element 
  srtp->policy.next = NULL;
  /*
   */

  return rc;
}

int
arc_srtp_add_crypto_policy (struct arc_srtp_t *srtp, enum arc_crypto_type_e type)
{

  int rc = -1;

  if (!srtp) {
    fprintf (stderr, " %s() no srtp structure supplied! return -1\n", __func__);
    return -1;
  }

  switch (type) {
  case ARC_SRTP_CRYPTO_SET_DEFAULT:
    crypto_policy_set_rtp_default (&srtp->policy.rtp);
    crypto_policy_set_rtcp_default (&srtp->policy.rtcp);
  case ARC_SRTP_CRYPTO_TYPE_AES_CM_128_HMAC_32:
    fprintf (stderr, " %s() case ARC_SRTP_CRYPTO_TYPE_AES_CM_128_HMAC_32:\n", __func__);
    crypto_policy_set_aes_cm_128_null_auth (&srtp->policy.rtp);
    crypto_policy_set_aes_cm_128_hmac_sha1_32 (&srtp->crypto);
    crypto_policy_set_rtcp_default (&srtp->policy.rtcp);
    break;
  default:
    fprintf (stderr, " %s() default case hit in cypher selection, returning -1\n", __func__);
    rc = -1;
    break;
  }

  return rc;
}

int
arc_srtp_create (struct arc_srtp_t *srtp)
{

  int rc = -1;

  if (!srtp) {
    fprintf (stderr, " %s() arcv_srtp_t pointer not supplied!, returning -1\n", __func__);
    return -1;
  }

  rc = srtp_create (&srtp->session, &srtp->policy);
  if (rc) {
    fprintf (stderr, " %s() rc=%d error str=%s\n", __func__, rc, arc_srtp_err_status_to_str (rc));
  }

  return rc;
}


int 
arc_srtp_patchup_ortp_session(RtpSession *session, struct arc_srtp_t *srtp){


   if(!session || !srtp){
     fprintf(stderr, " %s() either rtp or srtp session not supplied, cannot continue\n", __func__);
     return -1;
   }

   fprintf(stderr, " %s() RTP session =%p\n", __func__, session);

   return 0;
}



/*
 * 
 *
 *
 */

int 
arc_srtp_freeup_ortp_session(){



   return 0;
}


size_t
arc_srtp_protect (struct arc_srtp_t * srtp, char *rtpbuff, int *len)
{

  size_t rc = -1;

  rc = srtp_protect (srtp->session, rtpbuff, (int *) len);
  if (rc) {
    fprintf (stderr, " %s() err: %s\n", __func__, arc_srtp_err_status_to_str (rc));
  }
  else {
    fprintf (stderr, " %s() len=%d rc=%d\n", __func__, *len, (int)rc);
  }

  return rc;
}

size_t
arc_srtp_unprotect (struct arc_srtp_t * srtp, char *rtpbuff, int *len)
{

  int rc = -1;

  rc = srtp_unprotect (srtp->session, rtpbuff, len);

  if (rc) {
    fprintf (stderr, " %s() err: %s\n", __func__, arc_srtp_err_status_to_str (rc));
  }
  else {
    fprintf (stderr, " %s() len=%d rc=%d\n", __func__, *len, rc);
  }

  return rc;
}

#ifdef MAIN

int
main (int argc, char **argv)
{

  struct arc_srtp_t *srtp;
  int i;
  char buff[1024] = "test";
  int len = strlen (buff);

  unsigned char key[128] = "";

  arc_srtp_lib_init ();

  srtp = arc_srtp_alloc ();

  //arc_srtp_add_policy (srtp, 373313, ARC_SRTP_SSRC_ANY_INBOUND, "keykeykeykey", 12);
  //arc_srtp_add_crypto_policy (srtp, 0);
  //
  // len = hex_string_to_octet_string(key, in_key, 60);
  arc_setup_context (srtp, key, 0);

  arc_srtp_create (srtp);

  for (i = 0; i < 25; i++) {
    len = strlen (buff) + 1;
    arc_srtp_protect (srtp, buff, &len);
    fprintf (stderr, " %s() ++protected buff=[%s] len=%d\n", __func__, buff, len);
    arc_srtp_unprotect (srtp, buff, &len);
    fprintf (stderr, " %s() --unprotected buff=[%s]\n", __func__, buff);
  }

  arc_srtp_free (srtp);
  arc_srtp_lib_free ();

  return 0;
}

#endif
