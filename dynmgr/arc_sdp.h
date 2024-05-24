#ifndef ARC_SDP_DOT_H
#define ARC_SDP_DOT_H

#define ARC_SDP_MAX_ENTRIES 128 

#ifdef __cplusplus
extern "C" {
#endif

struct arc_sdp_t
{
  int count;
  char *sdp[ARC_SDP_MAX_ENTRIES];
};


int arc_sdp_init (struct arc_sdp_t *sdp);

int arc_sdp_push (struct arc_sdp_t * sdp, char *fmt, ...);

int arc_sdp_finalize (struct arc_sdp_t * sdp, char *dest, int destsize);

#ifdef __cplusplus
}
#endif

#endif
