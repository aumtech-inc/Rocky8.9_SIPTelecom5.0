/********************************************************/
/* Copyright (C) 2001 OSS Nokalva.  All rights reserved.*/
/********************************************************/

/* THIS FILE IS PROPRIETARY MATERIAL OF OSS NOKALVA
 * AND MAY BE USED ONLY BY DIRECT LICENSEES OF OSS NOKALVA.
 * THIS FILE MAY NOT BE DISTRIBUTED. */

/* Generated for: ITXC Inc., Beaverton, OR - license 6081 for Windows NT */
/* Abstract syntax: H235 */
/* Created: Fri Aug 24 15:02:37 2001 */
/* ASN.1 compiler version: 5.3 */
/* Target operating system: Linux 2.0 or later */
/* Target machine type: Intel x86 */
/* C compiler options required: None */
/* ASN.1 compiler options and file names specified:
 * -controlfile H235.c -headerfile H235.h -errorfile
 * C:\DOCUME~1\abowman\LOCALS~1\Temp\4. -dualheader -ansi -verbose -per -ber
 * -root C:\ossasn1\win32\5.3.0\asn1dflt\asn1dflt.linux_x86
 * C:\Documents and Settings\gideons\My Documents\CommonFolderFreshie\eCM\admissionlib\H225Subset.asn
 * C:\Documents and Settings\gideons\My Documents\CommonFolderFreshie\eCM\admissionlib\H235.asn
 */

#ifndef OSS_H235
#define OSS_H235

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "asn1hdr.h"
#include "asn1code.h"

#define          CryptoH323Token_PDU 1
#define          AuthenticationMechanism_PDU 2
#define          ClearToken_PDU 3
#define          EncodedGeneralToken_PDU 4
#define          PwdCertToken_PDU 5
#define          EncodedPwdCertToken_PDU 6
#define          H235Key_PDU 7
#define          KeySignedMaterial_PDU 8
#define          H235CertificateSignature_PDU 9
#define          ReturnSig_PDU 10
#define          KeySyncMaterial_PDU 11
#define          EncodedKeySyncMaterial_PDU 12

typedef struct ObjectID {
    unsigned short  length;
    unsigned char   *value;
} ObjectID;

typedef struct GatekeeperIdentifier {
    unsigned short  length;
    unsigned short  *value;
} GatekeeperIdentifier;

typedef struct H221NonStandard {
    unsigned short  t35CountryCode;
    unsigned short  t35Extension;
    unsigned short  manufacturerCode;
} H221NonStandard;

typedef struct NonStandardIdentifier {
    unsigned short  choice;
#       define      object_chosen 1
#       define      h221NonStandard_chosen 2
    union _union {
        ObjectID        object;  /* to choose, set choice to object_chosen */
        H221NonStandard h221NonStandard;  /* to choose, set choice to
                                           * h221NonStandard_chosen */
    } u;
} NonStandardIdentifier;

typedef struct _octet1 {
    unsigned int    length;
    unsigned char   *value;
} _octet1;

typedef struct H323_MESSAGES_NonStandardParameter {
    NonStandardIdentifier nonStandardIdentifier;
    _octet1         data;
} H323_MESSAGES_NonStandardParameter;

typedef struct _octet2 {
    unsigned short  length;
    unsigned char   value[4];
} _octet2;

typedef struct _choice1 {
    unsigned short  choice;
#       define      strict_chosen 1
#       define      loose_chosen 2
    union _union {
        Nulltype        strict;  /* to choose, set choice to strict_chosen */
        Nulltype        loose;  /* to choose, set choice to loose_chosen */
    } u;
} _choice1;

typedef struct _octet5 {
    unsigned short  length;
    unsigned char   value[16];
} _octet5;

typedef struct TransportAddress {
    unsigned short  choice;
#       define      ipAddress_chosen 1
#       define      ipSourceRoute_chosen 2
#       define      ipxAddress_chosen 3
#       define      ip6Address_chosen 4
#       define      netBios_chosen 5
#       define      nsap_chosen 6
#       define      nonStandardAddress_chosen 7
    union _union {
        struct _seq1 {
            _octet2         ip;
            unsigned short  port;
        } ipAddress;  /* to choose, set choice to ipAddress_chosen */
        struct _seq2 {
            _octet2         ip;
            unsigned short  port;
            struct _seqof1 {
                struct _seqof1  *next;
                _octet2         value;
            } *route;
            _choice1        routing;
        } ipSourceRoute;  /* to choose, set choice to ipSourceRoute_chosen */
        struct _seq3 {
            struct _octet3 {
                unsigned short  length;
                unsigned char   value[6];
            } node;
            _octet2         netnum;
            struct _octet4 {
                unsigned short  length;
                unsigned char   value[2];
            } port;
        } ipxAddress;  /* to choose, set choice to ipxAddress_chosen */
        struct _seq4 {
            _octet5         ip;
            unsigned short  port;
        } ip6Address;  /* to choose, set choice to ip6Address_chosen */
        _octet5         netBios;  /* to choose, set choice to netBios_chosen */
        struct _octet6 {
            unsigned short  length;
            unsigned char   value[20];
        } nsap;  /* to choose, set choice to nsap_chosen */
        H323_MESSAGES_NonStandardParameter nonStandardAddress;  /* to choose,
                                   * set choice to nonStandardAddress_chosen */
    } u;
} TransportAddress;

typedef struct PublicTypeOfNumber {
    unsigned short  choice;
#       define      PublicTypeOfNumber_unknown_chosen 1
#       define      internationalNumber_chosen 2
#       define      nationalNumber_chosen 3
#       define      networkSpecificNumber_chosen 4
#       define      subscriberNumber_chosen 5
#       define      PublicTypeOfNumber_abbreviatedNumber_chosen 6
    union _union {
        Nulltype        unknown;  /* to choose, set choice to
                                   * PublicTypeOfNumber_unknown_chosen */
        Nulltype        internationalNumber;  /* to choose, set choice to
                                               * internationalNumber_chosen */
        Nulltype        nationalNumber;  /* to choose, set choice to
                                          * nationalNumber_chosen */
        Nulltype        networkSpecificNumber;  /* to choose, set choice to
                                              * networkSpecificNumber_chosen */
        Nulltype        subscriberNumber;  /* to choose, set choice to
                                            * subscriberNumber_chosen */
        Nulltype        abbreviatedNumber;  /* to choose, set choice to
                               * PublicTypeOfNumber_abbreviatedNumber_chosen */
    } u;
} PublicTypeOfNumber;

typedef char            NumberDigits[129];

typedef struct PublicPartyNumber {
    PublicTypeOfNumber publicTypeOfNumber;
    NumberDigits    publicNumberDigits;
} PublicPartyNumber;

typedef struct PrivateTypeOfNumber {
    unsigned short  choice;
#       define      PrivateTypeOfNumber_unknown_chosen 1
#       define      level2RegionalNumber_chosen 2
#       define      level1RegionalNumber_chosen 3
#       define      pISNSpecificNumber_chosen 4
#       define      localNumber_chosen 5
#       define      PrivateTypeOfNumber_abbreviatedNumber_chosen 6
    union _union {
        Nulltype        unknown;  /* to choose, set choice to
                                   * PrivateTypeOfNumber_unknown_chosen */
        Nulltype        level2RegionalNumber;  /* to choose, set choice to
                                               * level2RegionalNumber_chosen */
        Nulltype        level1RegionalNumber;  /* to choose, set choice to
                                               * level1RegionalNumber_chosen */
        Nulltype        pISNSpecificNumber;  /* to choose, set choice to
                                              * pISNSpecificNumber_chosen */
        Nulltype        localNumber;  /* to choose, set choice to
                                       * localNumber_chosen */
        Nulltype        abbreviatedNumber;  /* to choose, set choice to
                              * PrivateTypeOfNumber_abbreviatedNumber_chosen */
    } u;
} PrivateTypeOfNumber;

typedef struct PrivatePartyNumber {
    PrivateTypeOfNumber privateTypeOfNumber;
    NumberDigits    privateNumberDigits;
} PrivatePartyNumber;

typedef struct PartyNumber {
    unsigned short  choice;
#       define      publicNumber_chosen 1
#       define      dataPartyNumber_chosen 2
#       define      telexPartyNumber_chosen 3
#       define      privateNumber_chosen 4
#       define      nationalStandardPartyNumber_chosen 5
    union _union {
        PublicPartyNumber publicNumber;  /* to choose, set choice to
                                          * publicNumber_chosen */
        NumberDigits    dataPartyNumber;  /* to choose, set choice to
                                           * dataPartyNumber_chosen */
        NumberDigits    telexPartyNumber;  /* to choose, set choice to
                                            * telexPartyNumber_chosen */
        PrivatePartyNumber privateNumber;  /* to choose, set choice to
                                            * privateNumber_chosen */
        NumberDigits    nationalStandardPartyNumber;  /* to choose, set choice
                                     * to nationalStandardPartyNumber_chosen */
    } u;
} PartyNumber;

typedef struct _char2 {
    unsigned short  length;
    char            *value;
} _char2;

typedef struct AliasAddress {
    unsigned short  choice;
#       define      e164_chosen 1
#       define      h323_ID_chosen 2
#       define      url_ID_chosen 3
#       define      transportID_chosen 4
#       define      email_ID_chosen 5
#       define      partyNumber_chosen 6
    union _union {
        char            e164[129];  /* to choose, set choice to e164_chosen */
        struct _char1 {
            unsigned short  length;
            unsigned short  *value;
        } h323_ID;  /* to choose, set choice to h323_ID_chosen */
        _char2          url_ID;  /* to choose, set choice to url_ID_chosen */
        TransportAddress transportID;  /* to choose, set choice to
                                        * transportID_chosen */
        _char2          email_ID;  /* to choose, set choice to
                                    * email_ID_chosen */
        PartyNumber     partyNumber;  /* to choose, set choice to
                                       * partyNumber_chosen */
    } u;
} AliasAddress;

typedef unsigned int    TimeStamp;

typedef struct IV8 {
    unsigned short  length;
    unsigned char   value[8];
} IV8;

typedef struct Params {
    unsigned char   bit_mask;
#       define      ranInt_present 0x80
#       define      iv8_present 0x40
    int             ranInt;  /* optional; set in bit_mask ranInt_present if
                              * present */
    IV8             iv8;  /* optional; set in bit_mask iv8_present if present */
} Params;

typedef struct Password {
    unsigned short  length;
    unsigned short  *value;
} Password;

typedef struct _bit1 {
    unsigned short  length;  /* number of significant bits */
    unsigned char   *value;
} _bit1;

typedef struct DHset {
    _bit1           halfkey;
    _bit1           modSize;
    _bit1           generator;
} DHset;

typedef struct ChallengeString {
    unsigned short  length;
    unsigned char   value[128];
} ChallengeString;

typedef int             RandomVal;

typedef struct TypedCertificate {
    ObjectID        type;
    _octet1         certificate;
} TypedCertificate;

typedef struct Identifier {
    unsigned short  length;
    unsigned short  *value;
} Identifier;

typedef struct H235_SECURITY_MESSAGES_NonStandardParameter {
    ObjectID        nonStandardIdentifier;
    _octet1         data;
} H235_SECURITY_MESSAGES_NonStandardParameter;

typedef struct ClearToken {
    unsigned char   bit_mask;
#       define      ClearToken_timeStamp_present 0x80
#       define      password_present 0x40
#       define      dhkey_present 0x20
#       define      challenge_present 0x10
#       define      random_present 0x08
#       define      ClearToken_certificate_present 0x04
#       define      generalID_present 0x02
#       define      nonStandard_present 0x01
    ObjectID        tokenOID;
    TimeStamp       timeStamp;  /* optional; set in bit_mask
                                 * ClearToken_timeStamp_present if present */
    Password        password;  /* optional; set in bit_mask password_present if
                                * present */
    DHset           dhkey;  /* optional; set in bit_mask dhkey_present if
                             * present */
    ChallengeString challenge;  /* optional; set in bit_mask challenge_present
                                 * if present */
    RandomVal       random;  /* optional; set in bit_mask random_present if
                              * present */
    TypedCertificate certificate;  /* optional; set in bit_mask
                                    * ClearToken_certificate_present if
                                    * present */
    Identifier      generalID;  /* optional; set in bit_mask generalID_present
                                 * if present */
    H235_SECURITY_MESSAGES_NonStandardParameter nonStandard;  /* optional; set
                                   * in bit_mask nonStandard_present if
                                   * present */
} ClearToken;

typedef ClearToken      PwdCertToken;

typedef OpenType        EncodedPwdCertToken;

typedef OpenType        EncodedGeneralToken;

typedef struct ENCRYPTED {
    ObjectID        algorithmOID;
    Params          paramS;
    _octet1         encryptedData;
} ENCRYPTED;

typedef struct _bit2 {
    unsigned int    length;  /* number of significant bits */
    unsigned char   *value;
} _bit2;

typedef struct HASHED {
    ObjectID        algorithmOID;
    Params          paramS;
    _bit2           hash;
} HASHED;

typedef struct CryptoToken {
    unsigned short  choice;
#       define      cryptoEncryptedToken_chosen 1
#       define      cryptoSignedToken_chosen 2
#       define      cryptoHashedToken_chosen 3
#       define      cryptoPwdEncr_chosen 4
    union _union {
        struct _seq8 {
            ObjectID        tokenOID;
            ENCRYPTED       token;
        } cryptoEncryptedToken;  /* to choose, set choice to
                                  * cryptoEncryptedToken_chosen */
        struct _seq9 {
            ObjectID        tokenOID;
            struct _seq6 {
                EncodedGeneralToken toBeSigned;
                ObjectID        algorithmOID;
                Params          paramS;
                _bit2           signature;
            } token;
        } cryptoSignedToken;  /* to choose, set choice to
                               * cryptoSignedToken_chosen */
        struct _seq10 {
            ObjectID        tokenOID;
            ClearToken      hashedVals;
            HASHED          token;
        } cryptoHashedToken;  /* to choose, set choice to
                               * cryptoHashedToken_chosen */
        ENCRYPTED       cryptoPwdEncr;  /* to choose, set choice to
                                         * cryptoPwdEncr_chosen */
    } u;
} CryptoToken;

typedef struct _seq18 {
    EncodedPwdCertToken toBeSigned;
    ObjectID        algorithmOID;
    Params          paramS;
    _bit2           signature;
} _seq18;

typedef struct CryptoH323Token {
    unsigned short  choice;
#       define      cryptoEPPwdHash_chosen 1
#       define      cryptoGKPwdHash_chosen 2
#       define      cryptoEPPwdEncr_chosen 3
#       define      cryptoGKPwdEncr_chosen 4
#       define      cryptoEPCert_chosen 5
#       define      cryptoGKCert_chosen 6
#       define      nestedcryptoToken_chosen 7
    union _union {
        struct _seq14 {
            AliasAddress    alias;
            TimeStamp       timeStamp;
            HASHED          token;
        } cryptoEPPwdHash;  /* to choose, set choice to
                             * cryptoEPPwdHash_chosen */
        struct _seq15 {
            GatekeeperIdentifier gatekeeperId;
            TimeStamp       timeStamp;
            HASHED          token;
        } cryptoGKPwdHash;  /* to choose, set choice to
                             * cryptoGKPwdHash_chosen */
        ENCRYPTED       cryptoEPPwdEncr;  /* to choose, set choice to
                                           * cryptoEPPwdEncr_chosen */
        ENCRYPTED       cryptoGKPwdEncr;  /* to choose, set choice to
                                           * cryptoGKPwdEncr_chosen */
        _seq18          cryptoEPCert;  /* to choose, set choice to
                                        * cryptoEPCert_chosen */
        _seq18          cryptoGKCert;  /* to choose, set choice to
                                        * cryptoGKCert_chosen */
        CryptoToken     nestedcryptoToken;  /* to choose, set choice to
                                             * nestedcryptoToken_chosen */
    } u;
} CryptoH323Token;

typedef struct KeyMaterial {
    unsigned short  length;  /* number of significant bits */
    unsigned char   *value;
} KeyMaterial;

typedef struct AuthenticationMechanism {
    unsigned short  choice;
#       define      dhExch_chosen 1
#       define      pwdSymEnc_chosen 2
#       define      pwdHash_chosen 3
#       define      certSign_chosen 4
#       define      ipsec_chosen 5
#       define      tls_chosen 6
#       define      nonStandard_chosen 7
    union _union {
        Nulltype        dhExch;  /* to choose, set choice to dhExch_chosen */
        Nulltype        pwdSymEnc;  /* to choose, set choice to
                                     * pwdSymEnc_chosen */
        Nulltype        pwdHash;  /* to choose, set choice to pwdHash_chosen */
        Nulltype        certSign;  /* to choose, set choice to
                                    * certSign_chosen */
        Nulltype        ipsec;  /* to choose, set choice to ipsec_chosen */
        Nulltype        tls;  /* to choose, set choice to tls_chosen */
        H235_SECURITY_MESSAGES_NonStandardParameter nonStandard;  /* to choose,
                                          * set choice to nonStandard_chosen */
    } u;
} AuthenticationMechanism;

typedef struct KeySignedMaterial {
    unsigned char   bit_mask;
#       define      srandom_present 0x80
#       define      KeySignedMaterial_timeStamp_present 0x40
    Identifier      generalId;
    RandomVal       mrandom;
    RandomVal       srandom;  /* optional; set in bit_mask srandom_present if
                               * present */
    TimeStamp       timeStamp;  /* optional; set in bit_mask
                                 * KeySignedMaterial_timeStamp_present if
                                 * present */
    ENCRYPTED       encrptval;
} KeySignedMaterial;

typedef OpenType        EncodedKeySignedMaterial;

typedef struct H235Key {
    unsigned short  choice;
#       define      secureChannel_chosen 1
#       define      sharedSecret_chosen 2
#       define      certProtectedKey_chosen 3
    union _union {
        KeyMaterial     secureChannel;  /* to choose, set choice to
                                         * secureChannel_chosen */
        ENCRYPTED       sharedSecret;  /* to choose, set choice to
                                        * sharedSecret_chosen */
        struct _seq22 {
            EncodedKeySignedMaterial toBeSigned;
            ObjectID        algorithmOID;
            Params          paramS;
            _bit2           signature;
        } certProtectedKey;  /* to choose, set choice to
                              * certProtectedKey_chosen */
    } u;
} H235Key;

typedef struct ReturnSig {
    unsigned char   bit_mask;
#       define      requestRandom_present 0x80
#       define      ReturnSig_certificate_present 0x40
    Identifier      generalId;
    RandomVal       responseRandom;
    RandomVal       requestRandom;  /* optional; set in bit_mask
                                     * requestRandom_present if present */
    TypedCertificate certificate;  /* optional; set in bit_mask
                                    * ReturnSig_certificate_present if
                                    * present */
} ReturnSig;

typedef OpenType        EncodedReturnSig;

typedef struct H235CertificateSignature {
    unsigned char   bit_mask;
#       define      requesterRandom_present 0x80
    TypedCertificate certificate;
    RandomVal       responseRandom;
    RandomVal       requesterRandom;  /* optional; set in bit_mask
                                       * requesterRandom_present if present */
    struct _seq23 {
        EncodedReturnSig toBeSigned;
        ObjectID        algorithmOID;
        Params          paramS;
        _bit2           signature;
    } signature;
} H235CertificateSignature;

typedef struct KeySyncMaterial {
    Identifier      generalID;
    KeyMaterial     keyMaterial;
} KeySyncMaterial;

typedef OpenType        EncodedKeySyncMaterial;


extern void *H235;    /* encoder-decoder control table */
#ifdef __cplusplus
}	/* extern "C" */
#endif /* __cplusplus */
#endif /* OSS_H235 */
