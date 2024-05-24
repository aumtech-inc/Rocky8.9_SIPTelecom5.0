#ifndef __MRTPPACKET_H__
#define __MRTPPACKET_H__

#include "mpacket.h"
#include "mrtptypes.h"
#include "platform.h"

class M_IMPORT CMRTPPacket : public CMPacket
{
protected:
    MRTPHEADER  hdr;
    MRTPPAYLOADHEADER  plhdr;
    int                     szFixedHdr;
    int                     szPayloadHdr;
    MOBJECTID               media_type;

protected:  // methods
public:
    CMRTPPacket(int sz=0);
    ~CMRTPPacket();
    virtual inline MRTPHEADER &          Header() {return hdr;}
    virtual inline MRTPPAYLOADHEADER &   PayloadHeader() {return plhdr;}
    virtual inline BOOL     Full(){return (BytePosition() >= MaxPayloadSize());}
    void                    ShallowCopy(CMRTPPacket & p_Packet);

    // virtual
    virtual BOOL            DataMakeSpace(int n);
    virtual void            SetHeaders(void *hdr, void *plhdr);
    virtual void            GetHeaders(void *hdr, void *plhdr);
    virtual void            ConfigPayload(int ts);
    virtual void            Packetise(int ts);
    virtual int             Segmentalise(int inbytes);
    virtual int             FixedHdrSize() {return szFixedHdr;}
    virtual int             PayloadHdrSize() {return szPayloadHdr;}
    inline void             SetFixedHdrSize(int size_i) {szFixedHdr=size_i;}
    inline void             SetPayloadHdrSize(int size_i) {szPayloadHdr=size_i;}

    inline void             SetMediaType(int mtype) {media_type=(MOBJECTID)mtype;}
    inline int              MediaType() {return media_type;}


	void 							setRTPBuffer(MOCTET &buf);
	const unsigned char* Mmemrse(unsigned char* dest, const unsigned char *src, int n);

#ifdef _PTS
    void                    CopyPacket(CMRTPPacket* p_MewPacket);
#endif
};

#endif // !__MRTPPACKET_H__
