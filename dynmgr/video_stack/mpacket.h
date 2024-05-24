/***************************************************************************
        File:           $Archive: /stacks/include/mpacket.h $
        Revision:       $Revision: 1.1.1.1 $
        Date:           $Date: 2005/11/04 05:06:11 $
        Last modified:  $Modtime:  $
        Authors:        Marwan Jabri
        Synopsys:       CMPacket definition
        Copyright 1997-2003 by Dilithium Networks.
The material in this file is subject to copyright. It may not
be used, copied or transferred by any means without the prior written
approval of Dilithium Networks.
DILITHIUM NETWORKS DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL DILITHIUM NETWORKS BE LIABLE FOR
ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
****************************************************************************/
#ifndef __MPACKET_H__
#define __MPACKET_H__

/** @file mpacket.h */

#include "mbuffer.h"

/**
  * @brief Provides basic packet (buffer) support. 
  *
  * It is also used as a base class for other packet oriented classes.
  */

class M_IMPORT CMPacket : public CMBuffer
{
private:
    // Don't allow copy constructors or assignment at this level
    CMPacket(const CMPacket &) {}
    void operator=(const CMPacket &) {}

protected:
    MOCTET      *buffer;        // space for packetisation
    int         buffer_count;
    int         max_payload_size;
    int         error;

public:

    /**
      * Constructs object. By default the packet buffer is not allocated. See DataMakeSpace() for 
      * later allocation/growing of packet buffer.
      */
    CMPacket(
        int sz=0        ///< Initial size of the packet buffer. Default is 0, which means 
                        ///< the buffer is not allocated any memory resource initially

    );

    /**
      * Destroys object. 
      *
      * Note packet buffer is not freed in the destructor. It should be explicitly freed 
      * by a call to the member function CMPacket::Free().
      */
    ~CMPacket();

    /**
      * Returns the buffer pointer.
      *
      *
      * @return Pointer to packet buffer.
      */
    virtual MOCTET *Buffer() const;

    /**
      * 
      * @return Buffer count.
      */
    virtual int BufferCount() const;

    /**
      * Set buffer count.
      */
    virtual void BufferCount(int n);

    /**
      *
      * @return Error number in packet. This is a Wrapper function.
      */
    virtual inline int&     Error(){return error;}

    /**
      * @return value of the maximum payload size field (data member of the class) 
      * of the packet. This is a wrapper function
      */
    virtual inline int      MaxPayloadSize(){return max_payload_size;}

    /**
      * This is a wrapper function that can be overloaded by classes derived from CMPacket. 
      */
    virtual void            SetHeaders(
        void *hdr,      ///< Pointer to a header.
        void *plhdr     ///< Pointer to a payload header.
    );

    /**
      * This is a wrapper function that can be overloaded by classes derived from CMPacket. 
      */
    virtual void            GetHeaders(
        void *hdr,      ///< Pointer to a header.
        void *plhdr     ///< Pointer to a payload header.
    );

    /**
      * This is a wrapper function that can be overloaded by classes derived from CMPacket. 
      */
    virtual void            ConfigPayload(
        int ts          ///< Timestamp (reserved).
    );

    /**
      * This is a wrapper function that can be overloaded by classes derived from CMPacket. 
      */
    virtual void            Packetise(
        int ts          ///< Timestamp (reserved)
    );

    /**
      * This is a wrapper function that can be overloaded by classes derived from CMPacket. 
      */
    virtual int             Segmentalise(
        int inbytes     ///< Number of bytes read in.
    );

    /**
      * Frees packet buffer.
      *
      * This may need to be called prior to destruction of the packet as this is not done automatically.
      */
    virtual void            Free();

    /**
      * Allocates (or Reallocates) the buffer if the current buffer does not have space to fit n octets. 
      *
      * @return TRUE on success and FALSE on failure.
      */
    virtual BOOL            DataMakeSpace(
        int n           ///< New size of the packet data buffer in octets
    );
};

#endif // !__MPACKET_H__
