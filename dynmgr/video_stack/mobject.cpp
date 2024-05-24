
#include "mobject.h"


// for detecting memory leak
#if defined(_DEBUG) & defined(_AFXDLL)
  #if !defined (MEMCHECK)
    #define new DEBUG_NEW
  #endif // !MEMCHECK
  #undef THIS_FILE
  static char THIS_FILE[] = __FILE__;
#endif



CMObjectPtrQueue::CMObjectPtrQueue()
{
    queue_first = queue_last = NULL;
    queue_length = 0;
}

CMObjectPtrQueue::CMObjectPtrQueue(const CMObjectPtrQueue &from)
{
    queue_first = queue_last = NULL;
    queue_length = 0;
    Append(from);
}


// CMObjectPtrQueue::Insert(): insert object to top of queue
int CMObjectPtrQueue::Insert(CMObject * o)
{
    if (! o)
        return -1;

    if (queue_first)
        o->next = queue_first;
    else
        queue_last = o;
    queue_first = o;
    return queue_length++;
}

int CMObjectPtrQueue::Add(CMObject * o)
{
    if (! o)
        return -1;

    if (queue_last)
        queue_last->next = o;
    else
        queue_first = o;
    queue_last = o;
    o->next = NULL;
    return queue_length++;
}

// Remove - will remove the element indexed by i. 
CMObject * CMObjectPtrQueue::Remove(int i)
{
    // ensure i is valid            
    if ((i < 0) || (i >= Length()) || (queue_first == NULL))
        return NULL;

    CMObject *r, *l = NULL;
    int j;
    
    for (j=0, r = queue_first; (r != NULL) && (j < Length()); j++, r=r->next) {
        if (j == i) {
            if (l) {    // not first in queue
                l->next = r->next;
            }
            else {
                queue_first = r->next;
            }
            if (r == queue_last) {
                queue_last = l;
            }
            queue_length--;
            return r;
        }
        l = r;
    }
    return NULL;
}

// Remove - will remove the object i. 
CMObject * CMObjectPtrQueue::Remove(CMObject* o)
{
    // ensure o is valid            
    if ((o == NULL) || (queue_first == NULL))
        return NULL;

    CMObject *r, *l = NULL;
    
    for (r = queue_first; r != NULL; r = r->next) {
        if (r == o) {
            if (l) {    // not first in queue
                l->next = r->next;
            }
            else {
                queue_first = r->next;
            }
            if (r == queue_last) {
                queue_last = l;
            }
            queue_length--;
            return r;
        }
        l = r;
    }
    return NULL;
}

// Append - will 
void CMObjectPtrQueue::Append(const CMObjectPtrQueue &from)
{
    for(CMObjectPtr l_object = from; l_object; ++l_object)
        AddCopy(*l_object);
}

// operator[] - returns the element indexed by i
CMObject * CMObjectPtrQueue::operator [] (int i) const
{
    // ensure i is valid
    if ( (i < 0) || (i >= Length()) )
        return NULL;

    CMObject *r;
    int j;
    for(j=0, r=queue_first; ((r != NULL) && (j < Length())); j++, r=r->next)
    {
        if (j == i)
            return r;
    }
    return NULL;
}

bool CMObjectPtrQueue::Member(CMObject *o) const
{
    if (o == NULL)
        return false;

    for(CMObject* r=queue_first; r != NULL; r=r->next)
    {
        if (r == o)
            return true;
    }

    return false;
}

void CMObjectPtrQueue::Clear(bool deleteObjects)
{
    while(queue_first != NULL)
    {
        CMObject * l_object = Remove(0);
        if (deleteObjects)
            delete l_object;
    }
}


///////////////////////////////// Object related functions ///////////////////////////////
// Object Names
//   IMPORTANT: shall map component list in mobject.h
static const char * gs_ObjectName[] =
{                                           // MMEDIA_UNSUPPORTED=-1,
    "Null",                                 // MMEDIA_NULLDATA,
    "Non Standard",                         // MMEDIA_NONSTANDARD,
    "Unknown Object",                       // MUNKNOWN_OBJECT,

    // Comm devices
    "Modem",                                // MMODEM,
    "Internet (TCP/IP)",                    // MTCP,
    "Internet (UDP)",                       // MUDP,
    "Real Time Protocol",                   // MRTP,
    "Real-time Data Exchange(RTDX)",        // MRTDX,
    "ISDN",                                 // MISDN,
    "UART",                                 // MUART,

    // System standards
    "ITU H.320",                            // MH320,
    "ITU H.323",                            // MH323,
    "ITU H.324",                            // MH324,
    "ITU H.324I",                           // MH324I,
    "Media Gateway",                        // MGATEWAY,

    // Multiplexers
    "ITU H.221",                            // MH221,
    "ITU H.223",                            // MH223,
    "ITU H.225.0",                          // MH2250,

    // Call signaling
    "Q.931",                                // MQ931,
    "V.140",                                // MV140,

    // Command and control
    "ITU H.242",                            // MH242,
    "ITU H.242 Tx",                         // MH242TX,
    "ITU H.242 Rx",                         // MH242RX,
    "ITU H.243",                            // MH243,
    "ITU H.245",                            // MH245,
    "H.245/H.242 Transcoder",               // MH245H242XC,

    // Video codecs
    "ITU H.261",                            // MH261,
    "ITU H.262",                            // MH262,
    "ITU H.263",                            // MH263,
    "Non Standard Video",                   // MVIDEO_NONSTANDARD,
    "IS11172",                              // MVIDEO_IS11172,
    "ISO/IEC MPEG4-Video",                  // MMPEG4VIDEO, 
    "H.263/H.261 Transcoder",               // MH263H261XC,
    "H.261/H.263 Transcoder",               // MH261H263XC,

    // Audio codecs
    "Non Standard Audio",                   // MAUDIO_NONSTANDARD, 
    "ITU G.723.1",                          // MG7231, 
    "ITU G.711",                            // MG711, -- MG711 centralises ALAW and ULAW with 56K and 64K rate 
    "ITU G.711 aLaw 64k",                   // MG711ALAW64K, 
    "ITU G.711 aLaw 56k",                   // MG711ALAW56K, 
    "ITU G.711 uLaw 64k",                   // MG711ULAW64K, 
    "ITU G.711 uLaw 56k",                   // MG711ULAW56K,
    "ITU G.722 64k",                        // MG722_64K,
    "ITU G.722 56k",                        // MG722_56K,
    "ITU G.722 48k",                        // MG722_48K, 
    "ITU G.728",                            // MG728,
    "ITU G.729",                            // MG729,
    "ITU G.729A",                           // MG729_ANNEXA, 
    "IS11172",                              // MAUDIO_IS11172,
    "IS13818",                              // MAUDIO_IS13818,
    "ITU G.729 Annex A",                    // MG729_ANNEXASS,
    "ITU G.729 Annex B",                    // MG729_ANNEXAWB,
    "ITU G.723.1 Annex C",                  // MG7231_ANNEXC,
    "ETSI GSM 06.10",                       // MGSM_FULL_RATE,
    "GSM Full Rate",                        // MGSM_HALF_RATE,
    "GSM Half Rate",                        // MGSM_ENHANCED_RATE,
    "ETSI GSM-AMR",                         // MGSMAMR,
    "G.729 Extention",                      // MG729_EXT,
    "G.723.1<->G.711 Transcoder",           // MG7231G711XC,
    "G.711<->G.723.1 Transcoder",           // MG711G7231XC,

    // Data
    "Non Standard Data",                    // MDATA_DAC_NONSTANDARD,
    "DSVDCNTRL",                            // MDATA_DAC_DSVDCNTRL,
    "ITU T.120",                            // MDATA_DAC_T120,
    "DSM_CC",                               // MDATA_DAC_DSM_CC,
    "USER_DATA",                            // MDATA_DAC_USER_DATA,
    "T.84",                                 // MDATA_DAC_T84,
    "T.434",                                // MDATA_DAC_T434,
    "H.224",                                // MDATA_DAC_H224,
    "NLPD",                                 // MDATA_DAC_NLPD,
    "H.222 Data Partitioning",              // MDATA_DAC_H222_DATA_PARTITIONING, 

    // Encryption
    "Non Standard Encryption",              // MENCRYPTION_NONSTANDARD
    "H.233",                                // MH233_ENCRYPTION,
    "Media Processor",                      // MMEDIAPROCESSOR,
    "User",                                 // MUSER_OBJECT,
    "Software",                             // MSW_OBJECT,
    "Hardware",                             // MHW_OBJECT,
    "Connection",                           // MCONNECTION_OBJECT,

    // Channel
    "Channel",                              // MCHANNEL,

    // System
    "H.32X App",                            // MH32XAPP,
    "H.320 App",                            // MH320APP,
    "H.323 App",                            // MH323APP,
    "H.324 App",                            // MH324APP,

    // Media devices
    "Unknown Media",                        // MUNKNOWN_MEDIA,
    "Video Capture",                        // MVIDEOCAPTURE,
    "YUV422 File",                          // MYUV422FILE,
    "YUV422 Memory",                        // MYUV422MEM,
    "Microsoft Windows Display",            // MMSWINDOW,
    "Microsoft Multimedia Audio",           // MMSAUDIO,
    "PCM Audio File",                       // MAUDIOFILE,
    "Microsoft Direct Sound",               // MDIRECTSOUND,
    "XWindows Display",                     // MXDISPLAY,
    "Meteor Video Capture",                 // MMETEORCAP, 
    "MSMC200H263",                          // MSMC200H263,
    "MSMC200G7231",                         // MSMC200G7231,
    "MAPOLLOH263",                          // MAPOLLOH263,
    "MAPOLLOG7231",                         // MAPOLLOG7231,
    "Video Bitstream File",                 // MVIDEOSTREAMFILE,
    "Audio Bitstream File",                 // MAUDIOSTREAMFILE,

    // Additional comm devices
    "Data Pump (offline monitoring)",                            // MDATAPUMP
    "E1 Card",                              // ME1
    "Queue Pump (online monitoring)",                           // MQUEUEPUMP
    "Pre-opened",                           // MPREOPENED

    "Default Audio",                        // MDEFAULT_AUDIO,
    "Default Video",                        // MDEFAULT_VIDEO,
    "Default Data",                         // MDEFAULT_DATA,

    // User Input
    "Non Standard User Input",              // MUSERINPUT_NONSTANDARD,
    "Basic String User Input",              // MUSERINPUT_BASICSTRING,
    "IA5 String User Input",                // MUSERINPUT_IA5STRING,
    "General String User Input",            // MUSERINPUT_GENERALSTRING,
    "DTMF User Input",                      // MUSERINPUT_DTMF,
    "Hook Flash User Input",                // MUSERINPUT_HOOKFLASH,
    "Extended Alphanumeric User Input",     // MUSERINPUT_EXTENDEDALPHANUMERIC,
    "Encrypted Basic String User Input",    // MUSERINPUT_ENCRYPTEDBASICSTRING,
    "Encrypted IA5 String User Input",      // MUSERINPUT_ENCRYPTEDIA5STRING,
    "Encrypted General String User Input",  // MUSERINPUT_ENCRYPTEDGENERALSTRING,
    "Secure DTMF User Input",               // MUSERINPUT_SECUREDTMF,

    // Additional media device
    "3GPP Media File",                      // M3GPPFILE

    // ADD HERE: Other object additions

    // End of list
    ""                                      // MOBJECT_END
};

static const char gs_ObjectNoName[] = "component without builtin name";


M_EXPORT const char * MObjectName(int object_id)
{
    const char *l_pcName = NULL;

    if (object_id < 0 || object_id >= (int)(sizeof(gs_ObjectName)/sizeof(gs_ObjectName[0])))
    {
        l_pcName = gs_ObjectName[MUNKNOWN_OBJECT];
    }
    else
    {
        l_pcName = gs_ObjectName[object_id];
    }

    if (l_pcName == NULL || *l_pcName == '\0')
    {
        l_pcName = gs_ObjectNoName;
    }

    return l_pcName;
}


M_EXPORT MOBJECTID MObjectFromName(const char * p_pcName)
{
    if (p_pcName != NULL)
    {
        for (int i = 0; i < (int)(sizeof(gs_ObjectName)/sizeof(gs_ObjectName[0])); i++)
        {
            if (strcmp(p_pcName, gs_ObjectName[i]) == 0)
            {
                return (MOBJECTID)i;
            }
        }
    }

    return MUNKNOWN_OBJECT;
}
