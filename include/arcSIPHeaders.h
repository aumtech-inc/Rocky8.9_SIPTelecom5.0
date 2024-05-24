/*-----------------------------------------------------------------------------
Program:        arcSIPHeaders.h
Purpose:        Contains structures, etc pertaining to registering and 
				setting SIP headers.
Update:	10/27/2006 djb	Created the file.
------------------------------------------------------------------------------*/
#ifndef ARCSIPHEADERS_H
#define ARCSIPHEADERS_H

struct sipHeaderInfo
{
	char	globalStringName[64];
    char    sipHeader[64];
};

struct sipHeaderInfo GV_SipHeaderInfo[] = 
{
	{ "$SIPHDR_ACCEPT",                 "Accept" },         // GCEV_EXTENSION
	{ "$SIPHDR_ACCEPT_ENCODING",        "Accept-Encoding" },// GCEV_EXTENSION
	{ "$SIPHDR_ACCEPT_LANGUAGE",        "Accept-Language" },// GCEV_EXTENSION
	{ "$SIPHDR_ALLOW",                  "Allow" },          // GCEV_EXTENSION
	{ "$SIPHDR_ALLOW_EVENTS",           "Allow-Events" },
	{ "$SIPHDR_AUTHENTICATION",         "Authentication" },
	{ "$SIPHDR_AUTHENTICATION_INFO",    "Authentication-Info" },
	{ "$SIPHDR_AUTHORIZATION",          "Authorization" },
	{ "$SIPHDR_CALL_ID",                "Call-ID" },        // GCEV_OFFERED
	{ "$SIPHDR_CONTACT",                "Contact" },        // GCEV_OFFERED
	{ "$SIPHDR_CONTENT_DISPOSITION",    "Content-Disposition" },// GC_CALLINFO
	{ "$SIPHDR_CONTENT_ENCODING",       "Content-Encoding" },   // GC_CALLINFO
	{ "$SIPHDR_CONTENT_LANGUAGE",       "Content-Language" },   // GC_CALLINFO
	{ "$SIPHDR_CSEQ",                   "CSeq" },
	{ "$SIPHDR_DATE",                   "Date" },
	{ "$SIPHDR_DIVERSION",              "Diversion" },      // GCEV_EXTENSION
	{ "$SIPHDR_EVENT",                  "Event" },          // GCEV_EXTENSION
	{ "$SIPHDR_EXPIRES",                "Expires" },        // GCEV_EXTENSION
	{ "$SIPHDR_FROM",                   "From" },           // GCEV_EXTENSION
	{ "$SIPHDR_MAX_FORWARDS",           "Max-Forwards" },
	{ "$SIPHDR_MIN_EXPIRES",            "Min-Expires" },
	{ "$SIPHDR_MIN_SE",                 "Min-SE" },
	{ "$SIPHDR_PROXY_AUTHENTICATE",     "Proxy-Authenticate" },
	{ "$SIPHDR_PROXY_AUTHORIZATION",    "Proxy-Authorization" },
	{ "$SIPHDR_RACK",                   "RAck" },
	{ "$SIPHDR_REFERRED_BY",            "Referred-By" },    // GCEV_OFFERED
	{ "$SIPHDR_REFER_TO",               "Refer-To" },       // GCEV_OFFERED
	{ "$SIPHDR_REPLACES",               "Replaces" },       // GCEV_OFFERED
	{ "$SIPHDR_REQUEST_URI",            "Request-URI" },    // GCEV_OFFERED
	{ "$SIPHDR_REQUIRE",                "Require" },        // GCEV_EXTENSION
	{ "$SIPHDR_RETRY_AFTER",            "Retry-After" },
	{ "$SIPHDR_ROUTE",                  "Route" },
	{ "$SIPHDR_RSEQ",                   "RSeq" },
	{ "$SIPHDR_SESSION_EXPIRES",        "Session-Expires" },
	{ "$SIPHDR_SUBSCRIPTION_STATE",     "Subscription-State" },
	{ "$SIPHDR_SUPPORTED",              "Supported" },      // GCEV_EXTENSION
	{ "$SIPHDR_TO",                     "To" },             // GCEV_EXTENSION
	{ "$SIPHDR_UNSUPPORTED",            "Unsupported" },
	{ "$SIPHDR_VIA",                    "Via" },
	{ "$SIPHDR_WARNING",                "Warning" },
	{ "$SIPHDR_WWW_AUTHENTICATE",       "WWW-Authenticate" },
	{ "$SIPHDR_ALL_NONEMPTY",           "n/a" },            // AUMTECH
	{ "",                               "" }
};
#endif
