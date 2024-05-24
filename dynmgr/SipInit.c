#include <stdio.h>
#include <eXosip2/eXosip.h>

#include "dynVarLog.h"

static void sVerifyTLS();

#define IMPORT_GLOBAL_EXTERNS
#include "ArcSipCommon.h"

//
// this is here after some changes to the 
// way the config file handles the bind port and 
// address  -- joes.
//

#define LOAD_CONFIG_TO_GLOBAL_STR(dest, srcstr, defaultstr) \
	if(srcstr && srcstr[0]){ \
      snprintf(dest, sizeof(dest), "%s", srcstr); \
    } else { \
      snprintf(dest, sizeof(dest), "%s", defaultstr); \
    } \
\



int
SipLoadConfig ()
{

 int rc = 0;
 int initcount = 0;
 int i;
	char		tmpB[30];

 if ((geXcontext = eXosip_malloc()) == NULL)
 {
    dynVarLog (__LINE__, -1, (char *)__func__, REPORT_NORMAL, DYN_BASE, ERR,
			"Unable to allocate memorty for eXosip: eXosip_malloc() failed.");
    exit(1);
 }
 

 while (1) {

  //EXOSIP2
  rc = eXosip_init (geXcontext);

  if (rc < 0) {
   if (initcount > 10) {
    dynVarLog (__LINE__, -1, (char *)__func__, REPORT_NORMAL, DYN_BASE, ERR, "eXosip init failed");
    exit(1);
   }
   else {
    dynVarLog (__LINE__, -1, (char *)__func__, REPORT_NORMAL, DYN_BASE, ERR, "Failed osip_init  err(%d) try(%d). Retry after 5 sec.", rc, initcount);
   }

   sleep (3);
   continue;
  }
  else {
   break;
  }

   initcount++;
 }

// global sip configs and exosip listening sockets  

 char *sip_config_names[] = {
  "BindAddress",
  "BindInterface",
  "BindPort",
  "Transport",
  "UserAgentString",
  "SipRedirection",
  "FromUri",
  "SipProxy",
  "OutboundProxy",
  "RTPStartPort",
  "CacheDir",
  "RFC2833Payload",
  "InterdigitDuration",
  "OutputDTMF",
  "PreferredCodec",
  "VideoSupport",
  "MaxCallDuration",
  "VFUDuration",
  "BeepDuration",
  "SimulateCallProgress",
  "EnableEnum",
  "EnumTLD",
  "SignaledDigits",
  "SignaledDigitsMinimumDuration",
  "EnableIPv6Support",
  "MinimumSessionExpires",
  "SessionExpires",
  "SendKeepAlive",
  "SipUserAgent",       // BT-62
  "RegisterTruncatePort",       // BT-175
  //
  NULL
 };

 enum sip_config_names_e
 {
  SIP_BIND_ADDRESS = 0,
  SIP_BIND_INTERFACE,
  SIP_BIND_PORT,
  SIP_TRANSPORT,
  SIP_USERAGENT,
  SIP_REDIRECTION,
  SIP_FROM_URI,
  SIP_PROXY,
  SIP_OUTBOUND_PROXY,
  SIP_RTP_START_PORT,
  SIP_CACHE_DIR,
  SIP_RFC_2833_PAYLOAD,
  SIP_INTERDIGIT_DURATION,
  SIP_OUTPUT_DTMF,
  SIP_PREFERRED_CODEC,
  SIP_VIDEO_SUPPORT,
  SIP_MAX_CALL_DURATION,
  SIP_VFU_DURATION,
  SIP_BEEP_DURATION,
  SIP_SIMULATE_CALL_PROGRESS,
  SIP_ENABLE_ENUM, 
  SIP_ENUM_TOP_LEVEL_DOMAIN,
  SIP_SIGNALED_DIGITS_ENABLE,
  SIP_SIGNALED_DIGITS_MIN_DURATION,
  SIP_ENABLE_IPV6,
  SIP_MINIMUM_SESSION_EXPIRES,
  SIP_SESSION_EXPIRES,
  SIP_SEND_KEEP_ALIVE,
  SIP_USER_AGENT,           // BT-62
  SIP_REGISTER_TRUNCATE_PORT, // BT-175
  //
  SIP_CONFIG_END
 };

 char sip_config_values[SIP_CONFIG_END][256];
 unsigned int portno = 5060;
 char *transport = "udp";
 char *user_agentstr = "SipIvr";
 char *sip_redir = "Off";
 char errstr[256];
 int secure = 0;
 int ipproto = IPPROTO_UDP;


 memset (sip_config_values, 0, sizeof (sip_config_values));


 for (i = 0; sip_config_names[i]; i++) {
  if (get_name_value ("SIP", sip_config_names[i], "", sip_config_values[i], sizeof (sip_config_values[i]), gIpCfg, errstr)) {
   //fprintf (stderr, " %s: error context=<%s> name=<%s> value=<%s>\n", __func__, "SIP", sip_config_names[i], sip_config_values[i]);
   //fprintf (stderr, " %s: config err string=%s\n", __func__, errstr);
  }
  else {
   //fprintf (stderr, " %s: context=<%s> name=<%s> config value=<%s>\n", __func__, "SIP", sip_config_names[i], sip_config_values[i]);
  }
 }

 if (sip_config_values[SIP_BIND_ADDRESS][0] == '\0') {
   dynVarLog (__LINE__, -1, (char *)__func__, REPORT_DETAIL, DYN_BASE, WARN, "you need to specify a valid BindAddress in your [SIP] config");
   exit(1);
 }
 else {
  snprintf (gHostIp, sizeof (gHostIp), "%s", sip_config_values[SIP_BIND_ADDRESS]);
 }

 if(sip_config_values[SIP_BIND_INTERFACE][0] != '\0') {
   snprintf(gHostIf, sizeof(gHostIf), "%s", sip_config_values[SIP_BIND_INTERFACE]);
 }

 if (sip_config_values[SIP_BIND_PORT][0]) {
  portno = atoi (sip_config_values[SIP_BIND_PORT]);
 }


 if (sip_config_values[SIP_TRANSPORT][0]) {
  transport = sip_config_values[SIP_TRANSPORT];
  if (!strcasecmp (transport, "udp")) {
   snprintf(gHostTransport, sizeof(gHostTransport), "%s", "udp");
   ipproto = IPPROTO_UDP;
  }
  else if (!strcasecmp (transport, "tcp")) {
   snprintf(gHostTransport, sizeof(gHostTransport), "%s", "tcp");
   ipproto = IPPROTO_TCP;
  }
  else if (!strcasecmp (transport, "tls")) {
   snprintf(gHostTransport, sizeof(gHostTransport), "%s", "tls");

    eXosip_tls_ctx_t tls_info;
    memset(&tls_info, 0, sizeof(eXosip_tls_ctx_t));
    
    sVerifyTLS();
    snprintf(tls_info.client.cert, sizeof(tls_info.client.cert), "c.pem");
    snprintf(tls_info.client.priv_key, sizeof(tls_info.client.priv_key), "ckey.pem");
    snprintf(tls_info.client.priv_key_pw, sizeof(tls_info.client.priv_key_pw), "password");
    
    snprintf(tls_info.server.cert, sizeof(tls_info.server.cert), "s.pem");
    snprintf(tls_info.server.cert, sizeof(tls_info.server.priv_key), "skey.pem");
    snprintf(tls_info.server.priv_key_pw, sizeof(tls_info.server.priv_key_pw), "password");
    
    snprintf(tls_info.root_ca_cert, sizeof(tls_info.root_ca_cert), "ca.pem");
    
    i = eXosip_set_option (geXcontext, EXOSIP_OPT_SET_TLS_CERTIFICATES_INFO, (void*)&tls_info);
   
   snprintf(gHostTransport, sizeof(gHostTransport), "%s", "tls");
   ipproto = IPPROTO_TCP;
   secure = 1;
  } else {
	   dynVarLog (__LINE__, -1, (char *)__func__, REPORT_NORMAL, DYN_BASE, ERR,
        "Invalid \"Transport\" (%s) specified (%s).  Must be either \"tcp\", \"udp\", or \"tls\". "
        "Correct and restart SIP Telecom.",
        transport, gIpCfg);
    exit(1);


  }
 }

 // sip session expiry stuff

      if(sip_config_values[SIP_MINIMUM_SESSION_EXPIRES][0])
      {
        gSipMinimumSessionExpires = atoi(sip_config_values[SIP_MINIMUM_SESSION_EXPIRES]);
        if(gSipMinimumSessionExpires < 90 )
        {
			dynVarLog (__LINE__, -1, (char *)__func__, REPORT_DETAIL, DYN_BASE, WARN,
				"MinimumSessionExpires (%d) is below minimum of 90.  Setting to default of 90.", gSipMinimumSessionExpires);
          gSipMinimumSessionExpires = 90;
		}

        gSipSessionExpires = atoi(sip_config_values[SIP_SESSION_EXPIRES]);
        if(gSipSessionExpires < 90 )
        {
			dynVarLog (__LINE__, -1, (char *)__func__, REPORT_DETAIL, DYN_BASE, WARN,
				"SessionExpires (%d) is below minimum of 90.  Setting to default of 1800.", gSipSessionExpires);
          gSipSessionExpires = 1800;
		}
     }
     dynVarLog (__LINE__, -1, (char *)__func__, REPORT_VERBOSE, DYN_BASE, INFO,
			"gSipMinimumSessionExpires=%d, gSipSessionExpires=%d",
			gSipMinimumSessionExpires, gSipSessionExpires);

 //keep alive
 if(sip_config_values[SIP_SEND_KEEP_ALIVE][0]){
   if(!strcasecmp(sip_config_values[SIP_SEND_KEEP_ALIVE], "ON"))
      gSipSendKeepAlive = 1;
 }

// BT-62 - sipUserAgent
    if (sip_config_values[SIP_USER_AGENT][0])
    {
        sprintf(gSipUserAgent, "%s", sip_config_values[SIP_USER_AGENT]);
        dynVarLog (__LINE__, -1, (char *)__func__, REPORT_DETAIL, DYN_BASE, WARN,
                "SipUserAgent set to (%s)", gSipUserAgent);
    }

// BT-175 - registerTruncatePort
    if (sip_config_values[SIP_REGISTER_TRUNCATE_PORT][0])
    {
		if(!strcasecmp(sip_config_values[SIP_REGISTER_TRUNCATE_PORT], "ON"))
		{
			setenv((const char *)"RegisterTruncatePort", (const char *)"1", 1);
		}
    }

 // sip signaled digits 

 if(sip_config_values[SIP_SIGNALED_DIGITS_ENABLE][0]){
   if(!strcasecmp(sip_config_values[SIP_SIGNALED_DIGITS_ENABLE], "ON"))
      gSipSignaledDigits++;
 }
 

 if(sip_config_values[SIP_SIGNALED_DIGITS_MIN_DURATION][0]){
   int val =  atoi(sip_config_values[SIP_SIGNALED_DIGITS_MIN_DURATION]);
   if(val > 0){
      gSipSignaledDigitsMinDuration =  val;
   }
 }

 // end sip signaled digits 

 // e-164 

 if(sip_config_values[SIP_ENABLE_ENUM][0]){
   if(!strcasecmp(sip_config_values[SIP_ENABLE_ENUM], "ON"))
      gSipEnableEnum++;
 }

 if(sip_config_values[SIP_ENUM_TOP_LEVEL_DOMAIN][0]){
   snprintf(gSipEnumTLD, sizeof(gSipEnumTLD), "%s", sip_config_values[SIP_ENUM_TOP_LEVEL_DOMAIN]);
 }

 // end e-164 

 // enable ipv6 
 if(sip_config_values[SIP_ENABLE_IPV6][0]){
   if(!strcasecmp(sip_config_values[SIP_ENABLE_IPV6], "ON"))
      gEnableIPv6Support++;
 }
 // end ipv6 

 if (sip_config_values[SIP_USERAGENT][0]) {
  user_agentstr = sip_config_values[SIP_USERAGENT];
  eXosip_set_user_agent (geXcontext, user_agentstr);
 }


 if (sip_config_values[SIP_PROXY][0]) {
  if (!strcasecmp (sip_config_values[SIP_PROXY], "On")) {
   gSipProxy = 1;
   // fprintf(stderr, "**********\n");
  }
  else if (!strcasecmp (sip_config_values[SIP_PROXY], "Off")) {
   gSipProxy = 0;
  }
  else {
   gSipProxy = 0;
  }
 }
 else {
  gSipProxy = 0;
 }

if (sip_config_values[SIP_REDIRECTION][0]) {
	  if (!strcasecmp (sip_config_values[SIP_REDIRECTION], "On")) {
	   gSipRedirection = 1;
	   // fprintf(stderr, "**********\n");
	  }
	  else if (!strcasecmp (sip_config_values[SIP_REDIRECTION], "Off")) {
	   gSipRedirection = 0;
	  }
	  else {
	   gSipRedirection = 0;
	  }
	 }
	 else {
	  gSipRedirection = 0;
	 }

if ( gSipProxy == 1 )
{
   gSipRedirection = 0;
}


 if (gSipRedirection || gSipProxy) {
  portno += gDynMgrId + 1;
 }
 else {
  portno = portno + gDynMgrId;
 }

 gSipPort = portno;

 int af = AF_INET;

 if(gEnableIPv6Support){
	rc = 1;
	eXosip_set_option(geXcontext, EXOSIP_OPT_ENABLE_IPV6, ( const void *) &rc);
  af = AF_INET6;
 } else {
	rc = 0;
	eXosip_set_option(geXcontext, EXOSIP_OPT_ENABLE_IPV6, ( const void *) &rc);
 }

 char *ifptr = NULL;

 if(gHostIf[0] != 0){
   ifptr = gHostIf;
 }

 eXosip_set_option(geXcontext, EXOSIP_OPT_SET_IPV4_FOR_GATEWAY, ( const void *) gHostIp);

 //if (eXosip_listen_addr (geXcontext, ipproto, gHostIp, gSipPort, af, secure, ifptr) < 0) {
 if (eXosip_listen_addr (geXcontext, ipproto, gHostIp, gSipPort, af, secure) < 0) {
  dynVarLog (__LINE__, -1, (char *)__func__, REPORT_NORMAL, DYN_BASE, ERR, "eXosip init failed");
  exit(1);
 }

 return rc;
}

static void sVerifyTLS()
{
    static char     mod[]="sVerifyTLS";
    int             rc;
    int             i;

    char    tlsFile[5][64] =
    {
        "c.pem",
        "ckey.pem",
        "s.pem",
        "skey.pem",
        ""
    };


    for(i=0; i<4; i++)
    {
        if (access (tlsFile[i], R_OK) != 0)
        {
            dynVarLog (__LINE__, -1, (char *)__func__, REPORT_NORMAL, DYN_BASE, ERR,
                "TLS is turned on, but unable to access (%s). [%d, %s] Correct and restart SIP Telecom.",
                tlsFile[i], errno, strerror(errno));
        }
    }
} // END: sVerifyTLS()

