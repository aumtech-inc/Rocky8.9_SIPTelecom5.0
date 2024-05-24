/*****************************************************************************/
/* Copyright (C) 1989-2001 Open Systems Solutions, Inc.  All rights reserved.*/
/*****************************************************************************/

/* THIS FILE IS PROPRIETARY MATERIAL OF OPEN SYSTEMS SOLUTIONS, INC.
 * AND MAY BE USED ONLY BY DIRECT LICENSEES OF OPEN SYSTEMS SOLUTIONS, INC.
 * THIS FILE MAY NOT BE DISTRIBUTED. */

/**************************************************************************/
/*
 * FILE: @(#)coderror.h	9.6  00/12/24
# @(#)errindex.sh	1.5 94/06/22
 */
/* FILE: @(#)ossber.hh	9.7  01/04/02                                 */
/*                                                                        */
/* function: Define the interfaces to the routines in ossber.a for the    */
/* OSS optimized encoder and decoder.                                     */
/*                                                                        */
/*                                                                        */
/* changes:                                                               */
/*      06/28/90  pet   created                                           */
/*      08/26/97  ard   added iconv support                               */
/*                                                                        */
/**************************************************************************/

#ifndef ossber_hdr_file
#define ossber_hdr_file

#ifdef EBCDIC
#include <iconv.h>
#endif

#include <setjmp.h>
#include "asn1hdr.h"

#if defined(_MSC_VER) && (defined(_WIN32) || defined(WIN32))
#pragma pack(push, ossPacking, 4)
#elif defined(_MSC_VER) && (defined(_WINDOWS) || defined(_MSDOS))
#pragma pack(1)
#elif defined(__BORLANDC__) && defined(__MSDOS__)
#ifdef _BC31
#pragma option -a-
#else
#pragma option -a1
#endif /* _BC31 */
#elif defined(__BORLANDC__) && defined(__WIN32__)
#pragma option -a4
#elif defined(__IBMC__) && (defined(__WIN32__) || defined(__OS2__))
#pragma pack(4)
#elif defined(__WATCOMC__) && defined(__NT__)
#pragma pack(push, 4)
#elif defined(__WATCOMC__) && (defined(__WINDOWS__) || defined(__DOS__))
#pragma pack(push, 1)
#endif /* _MSC_VER && _WIN32 */

#ifdef macintosh
#pragma options align=mac68k
#endif

#define OSS_TOED_API_LEVEL 3

#define _dstd_parms_def char **_pos, long *_buf_len, long _length
#define _dstd_parms _pos, _buf_len, _length
#define _sdstd_parms_def char **_pos, long *_buf_len
#define _sdstd_parms _pos, _buf_len

#define _std_parms_def char **_pos, long *_max_len, ossBoolean _userbuf
#define _std_parms _pos, _max_len, _userbuf

#if defined(macintosh) && defined(__CFM68K__)
#pragma import on
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* Entry Points for the Interface Routines */

#ifndef _OSSNOANSI

#if defined(__WATCOMC__) && defined(__DOS__)
typedef    int       (*_Encode_function)(struct ossGlobal *world, int pdunum,
	                void *inbuf, char **outbuf, long *outlen,
	                void *ctl_tbl, unsigned flags,
	                char errmsg[]);

typedef    int       (*_Decode_function)(struct ossGlobal *world, int *pduNum,
	                char **inbuf, long *inlen, void **outbuf,
	                long *outlen, void *ctl_tbl, unsigned flags,
	                char errmsg[]);

typedef    int       (*_FreePDU_function)(struct ossGlobal *world, int pdunum,
                        void *data, void *ctl_tbl);

typedef    void      (*_Ossinit_function)(struct ossGlobal *);
#else
typedef    int       (DLL_ENTRY_FPTR *_System _Encode_function)(struct ossGlobal *world, int pdunum,
	                void *inbuf, char **outbuf, long *outlen,
	                void *ctl_tbl, unsigned flags,
	                char errmsg[]);

typedef    int       (DLL_ENTRY_FPTR *_System _Decode_function)(struct ossGlobal *world, int *pduNum,
	                char **inbuf, long *inlen, void **outbuf,
	                long *outlen, void *ctl_tbl, unsigned flags,
	                char errmsg[]);

typedef    int       (DLL_ENTRY_FPTR *_System _FreePDU_function)(struct ossGlobal *world, int pdunum,
                        void *data, void *ctl_tbl);

typedef    void      (DLL_ENTRY_FPTR *_System _Ossinit_function)(struct ossGlobal *);
#endif /* __WATCOMC__ && __DOS__ */

typedef    int       (DLL_ENTRY_FPTR *_System _PrintPDU_function)(struct ossGlobal *, int pdunum,
                        void *data, void *ctl_tbl);

#else

typedef    int       (*_Encode_function)();

typedef    int       (*_Decode_function)();

typedef    int       (*_FreePDU_function)();

typedef    void      (*_Ossinit_function)();

typedef    int       (*_PrintPDU_function)();

#endif /* _OSSNOANSI */


typedef struct _entry_point_struct_ {
	_Ossinit_function	_ossinit_function;
	_Encode_function        _ber_encode_function;
	_Decode_function        _ber_decode_function;
	_FreePDU_function	_freePDU_function;
	unsigned short int      _oss_ber_version_number;
	_Encode_function        _per_encode_function;
	_Decode_function        _per_decode_function;
	unsigned short int      _oss_per_version_number;
	unsigned int            _oss_pa_count;
	struct _char_data       *_chars_ptr;
	_PrintPDU_function      _printPDU_function;
}               _entry_point_struct;





/* Error Handling */


/* The index to the error messages.  NOTE:  The enumerated constants here
   must match those in coderror.h. */

enum _err_index {
_no_msg, _small_buffer, _type_not_impl, _pdu_range,
_bad_argument, _version_mismatch_toed, _version_mismatch_soed,
_out_stor, _bad_choice, _bad_der_time, _stor_error,
_int_too_long, _uint_too_long, _bit_str_comp,
_access_serialization, _constrainedBy, _singleValue_integer,
_singleValue_uinteger, _singleValue_real, _singleValue_string,
_singleValue_void, _valueRange_integer, _valueRange_uinteger,
_valueRange_real, _sizeConstraint_length,
_sizeConstraint_number, _oPA_constraint, _absence_constraint,
_presence_constraint, _openType_error,
_openType_user_field_error, _tableConstraint,
_componentRelation, _not_enumerated, _null_tbl, _null_fcn,
_corrupted_tbl, _neg_uinteger, _typeConstraint,
_bit_str_too_long, _pdu_range_recursive, _enc_oid_too_long,
_more_input, _zero_bytes_requested, _size_too_big,
_out_of_memory, _invalid_object, _memmgr_undefined_err,
_bad_encrules_err, _toed_rules_not_impl_err,
_unlinked_encrules_err, _table_mismatch, _type_not_supported,
_type_not_supported_decreal, _real_dll_failed,
_indef_length_not_supported, _cstrain_dll_failed,
_cmp_dll_failed, _cmp_code_not_linked, _real_code_not_linked,
_tracing_code_not_linked, _oid_dll_failed,
_type_not_supported_der_toed, _toed_rules_not_supp_err,
_load_library_failed, _unload_library_failed, _mandatory_field,
_input_file_open, _bad_objid, _bad_pointer, _bad_time,
_time_bad_month, _time_bad_day, _time_bad_hour,
_time_bad_minute, _time_bad_second, _time_bad_millisec,
_too_long, _toom_field, _zero_length_ANY, _zero_length_OpenType,
_inval_dec_real, _inval_mixed_real,
_per_mandatory_field_in_group, _ber_mandatory_field_in_group,
_UTF8_bad_conversion, _AUTOMATIC_ENCDEC_is_not_specified,
_pduNum_is_not_initialized, _length_is_not_initialized,
_tag_long_nec, _tag_too_long, _exp_long_nec, _exp_too_long,
_int_long_nec, _subid_long_nec, _subid_too_long,
_length_too_long, _size_too_long, _non_std_eoc, _unknown_size,
_inconsis_len, _tag_mismatch, _pdu_mismatch, _extra_bits,
_inval_enc, _no_dec_dig, _no_exp_dig, _inval_nr1, _inval_nr2,
_inval_nr3, _real_long_zero, _undef_enc, _truncate,
_null_in_nlltrm, _unknown_field, _field_repeat, _field_omit,
_expec_eoc, _indef_len_prim, _expec_constr_fnd_prim,
_expec_prim_fnd_constr, _unknown_type, _cannot_happen,
_neg_not_allowed, _expect_tag, _more_value, _real_too_large,
_too_many_subids, _too_many_optional_fields,
_too_many_ext_additions, _too_many_optional_in_group,
_extension_encode_error, _indef_length_der, _bad_UTF8_char,
_ber_field_removed, _per_field_removed, _unexpected_end_of_pdu,
_input_file_eof, _not_multiple_charwidth, _objid_is_constructed,
_small_stack, _stack_sharing, _printper_dll_failed,
_small_tmp_buffer, _bad_tmp_buf_free, _hindmost_error
};



/* typedef's for the context information */

typedef enum _context_type {
    _boolean_type, _integer_type, _enumerated_type, _real_type,
    _bitstring_type, _octetstring_type, _null_type, _sequence_type,
    _sequence_of_type, _set_type, _set_of_type, _choice_type,
    _charstring_type, _object_identifier_type, _time_type, _any_type,
    _opentype, _embedded_pdv_type, _unrestr_charstring_type,
    _relative_oid_type, _contconstr_type
}               _Context_Type;

typedef struct _context_id {
    char           *_element_name;
    char           *_element_type;
    _Context_Type   _type;
}               _Context_Id;


typedef struct _context_element {
    struct _context_element *_parent;
    _Context_Id    *_id;
    long            _occurrence;/* For SET OF or SEQUENCE OF */
}               _Context_Element;

typedef enum _context_state {
    _encoding_tag, _encoding_length, _encoding_data,
    _decoding_tag, _decoding_length, _decoding_data
}               _Context_State;

typedef struct _context_anchor {
    _Context_Element *_last;
    int             _pdu_number;
    _Context_Id    *_pdu_id;
    _Context_State  _state;
}               _Context_Anchor;

typedef  char _Per_Inverted_Index[128];


extern _Context_Anchor _oss_context_anchor;

/* The error routines */

#ifndef _OSSNOANSI

extern void DLL_ENTRY _oss_enc_error(struct ossGlobal *g, ossBoolean _jump,
                               enum _err_index _error_code, long _data);

#else

extern void     _oss_enc_error();

#endif /* _OSSNOANSI */


#define _oss_dec_error _oss_enc_error



#ifndef _OSSNOANSI

/* Encode Routines */

extern long DLL_ENTRY _oss_encd_length(struct ossGlobal *g, _std_parms_def,
                                 unsigned long _data);
extern void DLL_ENTRY _oss_enci_length(struct ossGlobal *g, _std_parms_def,
                                 unsigned long _data);
extern long DLL_ENTRY _oss_encd_tag(struct ossGlobal *g, _std_parms_def,
			      unsigned short _data, ossBoolean _constructed);
extern void DLL_ENTRY _oss_enci_tag(struct ossGlobal *g, _std_parms_def,
			      unsigned short _data, ossBoolean _constructed);
extern long DLL_ENTRY _oss_encd_bool(struct ossGlobal *g, _std_parms_def,
                               ossBoolean _data);
extern void DLL_ENTRY _oss_enci_bool(struct ossGlobal *g, _std_parms_def,
                               ossBoolean _data);
extern long DLL_ENTRY _oss_encd_int(struct ossGlobal *g, _std_parms_def,
				LONG_LONG _data);
extern void DLL_ENTRY _oss_enci_int(struct ossGlobal *g, _std_parms_def,
				LONG_LONG _data);
extern long DLL_ENTRY _oss_encd_uint(struct ossGlobal *g, _std_parms_def,
				ULONG_LONG _data);
extern void DLL_ENTRY _oss_enci_uint(struct ossGlobal *g, _std_parms_def,
				ULONG_LONG _data);
extern long DLL_ENTRY _oss_encd_huge_int(struct ossGlobal *g, _std_parms_def,
			       void *_data);
extern long DLL_ENTRY _oss_encd_uhuge_int(struct ossGlobal *g, _std_parms_def,
			       void *_data);
extern long DLL_ENTRY _oss_encd_real(struct ossGlobal *g, _std_parms_def,
                               double _data);
extern long DLL_ENTRY _oss_encd_creal(struct ossGlobal *g, _std_parms_def,
			       char *_data);
extern long DLL_ENTRY _oss_encd_mreal(struct ossGlobal *g, _std_parms_def,
			       MixedReal *_data);
extern void DLL_ENTRY _oss_enci_real(struct ossGlobal *g, _std_parms_def,
                               double _data);
extern void DLL_ENTRY _oss_enci_creal(struct ossGlobal *g, _std_parms_def,
			       char *_data);
extern void DLL_ENTRY _oss_enci_mreal(struct ossGlobal *g, _std_parms_def,
			       MixedReal *_data);
extern long DLL_ENTRY _oss_encd_alobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, short _array_size);
extern long DLL_ENTRY _oss_encd_asobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, short _array_size);
extern long DLL_ENTRY _oss_encd_aiobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, short _array_size);
extern void DLL_ENTRY _oss_enci_alobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, short _array_size);
extern void DLL_ENTRY _oss_enci_asobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, short _array_size);
extern void DLL_ENTRY _oss_enci_aiobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, short _array_size);
extern long DLL_ENTRY _oss_encd_llobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern long DLL_ENTRY _oss_encd_lsobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern long DLL_ENTRY _oss_encd_liobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern void DLL_ENTRY _oss_enci_llobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern void DLL_ENTRY _oss_enci_lsobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern void DLL_ENTRY _oss_enci_liobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern long DLL_ENTRY _oss_encd_ulobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern long DLL_ENTRY _oss_encd_usobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern long DLL_ENTRY _oss_encd_uiobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern void DLL_ENTRY _oss_enci_ulobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern void DLL_ENTRY _oss_enci_usobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern void DLL_ENTRY _oss_enci_uiobjid(struct ossGlobal *g, _std_parms_def,
                                  void *_data, long _size_c);
extern long DLL_ENTRY _oss_encd_pbit(struct ossGlobal *g, _std_parms_def,
                               void *_data, long _bit_count);
extern void DLL_ENTRY _oss_enci_pbit(struct ossGlobal *g, _std_parms_def,
                               void *_data, long _bit_count);
extern long DLL_ENTRY _oss_encd_ubit(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern void DLL_ENTRY _oss_enci_ubit(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern long DLL_ENTRY _oss_encd_vbit(struct ossGlobal *g, _std_parms_def,
                               void *_data, long _array_size,
                               char _lengthsize);
extern void DLL_ENTRY _oss_enci_vbit(struct ossGlobal *g, _std_parms_def,
                               void *_data, long _array_size,
                               char _lengthsize);
extern long DLL_ENTRY _oss_encd_nstr(struct ossGlobal *g, _std_parms_def,
                               char *_data, long _size_c);
extern void DLL_ENTRY _oss_enci_nstr(struct ossGlobal *g, _std_parms_def,
                               char *_data, long _size_c);
extern long DLL_ENTRY _oss_encd_pstr(struct ossGlobal *g, _std_parms_def,
                               char *_data, long _byte_count);
extern void DLL_ENTRY _oss_enci_pstr(struct ossGlobal *g, _std_parms_def,
                               char *_data, long _byte_count);
extern long DLL_ENTRY _oss_encd_ustr(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern void DLL_ENTRY _oss_enci_ustr(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern long DLL_ENTRY _oss_encd_vstr(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern void DLL_ENTRY _oss_enci_vstr(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern long DLL_ENTRY _oss_encd_uoct(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern void DLL_ENTRY _oss_enci_uoct(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern long DLL_ENTRY _oss_encd_voct(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern void DLL_ENTRY _oss_enci_voct(struct ossGlobal *g, _std_parms_def,
                               void *_data, char _lengthsize, long _size_c);
extern long DLL_ENTRY _oss_encd_gtime(struct ossGlobal *g, _std_parms_def,
                                GeneralizedTime*_data);
extern void DLL_ENTRY _oss_enci_gtime(struct ossGlobal *g, _std_parms_def,
                                GeneralizedTime *_data);
extern long DLL_ENTRY _oss_encd_utime(struct ossGlobal *g, _std_parms_def,
                                UTCTime *_data);
extern void DLL_ENTRY _oss_enci_utime(struct ossGlobal *g, _std_parms_def,
                                UTCTime *_data);
extern long DLL_ENTRY _oss_encd_uany(struct ossGlobal *g, _std_parms_def,
                               void *_data);
extern void DLL_ENTRY _oss_enci_uany(struct ossGlobal *g, _std_parms_def,
                               void *_data);
extern long DLL_ENTRY _oss_encd_opentype(struct ossGlobal *g, _std_parms_def,
                               void *_data);
extern long DLL_ENTRY _oss_encd_defer(struct ossGlobal *g, _std_parms_def,
				void *data, ossBoolean *constructed);
extern void DLL_ENTRY _oss_enci_opentype(struct ossGlobal *g, _std_parms_def,
                               void *_data);
extern long DLL_ENTRY _oss_encd_bmpstr(struct ossGlobal *g, _std_parms_def,
                    void *data, char lengthsize,
		    long size_c);
extern long DLL_ENTRY _oss_enci_bmpstr(struct ossGlobal *g, _std_parms_def,
                    void *data, char lengthsize,
		    long size_c);
extern long DLL_ENTRY _oss_encd_unistr(struct ossGlobal *g, _std_parms_def,
                    void *data, char lengthsize,
		    long size_c);
extern long DLL_ENTRY _oss_enci_unistr(struct ossGlobal *g, _std_parms_def,
                    void *data, char lengthsize,
		    long size_c);

extern long DLL_ENTRY _oss_encd_eobjid(struct ossGlobal *g, _std_parms_def,
				  void *data, long size_c);

extern long DLL_ENTRY _oss_encd_ereloid(struct ossGlobal *g, _std_parms_def,
				  void *data, long size_c);

extern long DLL_ENTRY _oss_encd_dubit(struct ossGlobal *g, _std_parms_def,
		  void *data, char lengthsize, long size_c, ossBoolean named_bits);

extern long DLL_ENTRY _oss_encd_dvbit(struct ossGlobal *g, _std_parms_def,
		    void *data, long array_size,
		  char lengthsize, ossBoolean named_bits);

extern long DLL_ENTRY _oss_encd_utf8unistr(struct ossGlobal *g, _std_parms_def,
                    void *data, char lengthsize,
		    long size_c);

extern long DLL_ENTRY _oss_encd_utf8bmpstr(struct ossGlobal *g, _std_parms_def,
                    void *data, char lengthsize,
		    long size_c);

/* Decode Routines */
/* these return values */
extern void DLL_ENTRY _oss_dec_tag(struct ossGlobal *g, _sdstd_parms_def,
			     unsigned short *tag, ossBoolean *con);
extern long DLL_ENTRY _oss_dec_length(struct ossGlobal *g, _sdstd_parms_def);


extern ossBoolean DLL_ENTRY _oss_dec_bool(struct ossGlobal *g, _dstd_parms_def);
extern LONG_LONG DLL_ENTRY _oss_dec_llint(struct ossGlobal *g, _dstd_parms_def);
extern long DLL_ENTRY _oss_dec_lint(struct ossGlobal *g, _dstd_parms_def);
extern int  DLL_ENTRY _oss_dec_iint(struct ossGlobal *g, _dstd_parms_def);
extern short DLL_ENTRY _oss_dec_sint(struct ossGlobal *g, _dstd_parms_def);
extern void DLL_ENTRY _oss_dec_hint(struct ossGlobal *g, _dstd_parms_def,
				ossBoolean userbuf, void *data);
extern ULONG_LONG DLL_ENTRY _oss_dec_ullint(struct ossGlobal *g, _dstd_parms_def);
extern unsigned long DLL_ENTRY _oss_dec_ulint(struct ossGlobal *g, _dstd_parms_def);
extern unsigned int DLL_ENTRY _oss_dec_uiint(struct ossGlobal *g, _dstd_parms_def);
extern unsigned short DLL_ENTRY _oss_dec_usint(struct ossGlobal *g, _dstd_parms_def);

/* The code for this one is so huge that it would be criminal to
 * repeat it three times.  Caller should cast the returned value
 * to what she wants.
 */
extern void DLL_ENTRY _oss_dec_real(struct ossGlobal *g, _dstd_parms_def,
				double *data);
extern void DLL_ENTRY _oss_dec_freal(struct ossGlobal *g, _dstd_parms_def,
				float *data);
extern void DLL_ENTRY _oss_dec_creal(struct ossGlobal *g, _dstd_parms_def,
				  ossBoolean _userbuf, char **_data);
extern void DLL_ENTRY _oss_dec_mreal(struct ossGlobal *g, _dstd_parms_def,
				  ossBoolean _userbuf, MixedReal *_data);

/* Two versions of these functions.  If the type has the pointer
 * directive, then its size is not fixed--the function will allocate
 * the memory.  Otherwise the caller allocates the memory.  (Actually,
 * the type will probably be enclosed in another type, and therefore
 * the memory has already been allocated.)
 */
extern void DLL_ENTRY _oss_dec_alobjid_ptr(struct ossGlobal *g, _dstd_parms_def,
				     ossBoolean _userbuf, void **_data,
                                     short _array_size);
extern void DLL_ENTRY _oss_dec_alobjid(struct ossGlobal *g, _dstd_parms_def,
                                 void *_data, short _array_size);

extern void DLL_ENTRY _oss_dec_aiobjid_ptr(struct ossGlobal *g, _dstd_parms_def,
				     ossBoolean _userbuf, void **_data,
                                     short _array_size);
extern void DLL_ENTRY _oss_dec_aiobjid(struct ossGlobal *g, _dstd_parms_def,
                                 void *_data, short _array_size);

extern void DLL_ENTRY _oss_dec_asobjid_ptr(struct ossGlobal *g, _dstd_parms_def,
				     ossBoolean _userbuf, void **_data,
                                     short _array_size);
extern void DLL_ENTRY _oss_dec_asobjid(struct ossGlobal *g, _dstd_parms_def,
                                 void *_data, short _array_size);

/* These allocate all their own memory.  */
extern void DLL_ENTRY _oss_dec_llobjid(struct ossGlobal *g, _dstd_parms_def,
				 ossBoolean _userbuf, void **_data, long _size_c);
extern void DLL_ENTRY _oss_dec_liobjid(struct ossGlobal *g, _dstd_parms_def,
				 ossBoolean _userbuf, void **_data, long _size_c);
extern void DLL_ENTRY _oss_dec_lsobjid(struct ossGlobal *g, _dstd_parms_def,
				 ossBoolean _userbuf, void **_data, long _size_c);

/*  These are passed the structure containing the count, but allocate
  *  space for the data itself. */
extern void DLL_ENTRY _oss_dec_ulobjid(struct ossGlobal *g, _dstd_parms_def,
				 ossBoolean _userbuf, void *_data, short _size_c);
extern void DLL_ENTRY _oss_dec_uiobjid(struct ossGlobal *g, _dstd_parms_def,
				 ossBoolean _userbuf, void *_data, short _size_c);
extern void DLL_ENTRY _oss_dec_usobjid(struct ossGlobal *g, _dstd_parms_def,
				 ossBoolean _userbuf, void *_data, short _size_c);

/* Caller allocates memory for this one.  Call for both e_pad & e_big_pad. */
extern void DLL_ENTRY _oss_dec_pbit(struct ossGlobal *g, _dstd_parms_def,
                              void *_data, unsigned long _bit_count,
			      ossBoolean _cons, ossBoolean _named_bits);

/* See comment before dec_ulobjid. */
extern void DLL_ENTRY _oss_dec_ubit(struct ossGlobal *g, _dstd_parms_def,
			      ossBoolean _userbuf, void *_data,
			      char _lengthsize, long _size_c, ossBoolean _cons);

/* Two versions of this one too.  */
extern void DLL_ENTRY _oss_dec_vbit_ptr(struct ossGlobal *g, _dstd_parms_def,
				  ossBoolean _userbuf, void **_data,
                                  long _array_size, char _lengthsize,
				  ossBoolean _cons);
extern void DLL_ENTRY _oss_dec_vbit(struct ossGlobal *g, _dstd_parms_def,
                              void *_data, long _array_size,
			      char _lengthsize, ossBoolean _cons);

extern void DLL_ENTRY _oss_dec_nstr_ptr(struct ossGlobal *g, _dstd_parms_def,
				  ossBoolean _userbuf, char **_data, long _size_c,
				  ossBoolean _cons);
extern void DLL_ENTRY _oss_dec_nstr(struct ossGlobal *g, _dstd_parms_def,
			      char *_data, long _size_c, ossBoolean _cons);

extern void DLL_ENTRY _oss_dec_pstr(struct ossGlobal *g, _dstd_parms_def,
			      char *_data, long _byte_count, ossBoolean _cons);
extern void DLL_ENTRY _oss_dec_ustr(struct ossGlobal *g, _dstd_parms_def,
			      ossBoolean _userbuf, void *_data,
			      char _lengthsize, long _size_c, ossBoolean _cons);

extern void DLL_ENTRY _oss_dec_vstr_ptr(struct ossGlobal *g, _dstd_parms_def,
				  ossBoolean _userbuf, void **_data,
				  char _lengthsize, long _size_c, ossBoolean _cons);
extern void DLL_ENTRY _oss_dec_vstr(struct ossGlobal *g, _dstd_parms_def,
                              void *_data, char _lengthsize, long _array_size,
			      ossBoolean _cons);

extern void DLL_ENTRY _oss_dec_uoct(struct ossGlobal *g, _dstd_parms_def,
			      ossBoolean _userbuf, void *_data,
			      char _lengthsize, long _size_c, ossBoolean _cons);

extern void DLL_ENTRY _oss_dec_voct_ptr(struct ossGlobal *g, _dstd_parms_def,
				  ossBoolean _userbuf, void **_data,
				  char _lengthsize, long _size_c, ossBoolean _cons);
extern void DLL_ENTRY _oss_dec_voct(struct ossGlobal *g, _dstd_parms_def,
                              void *_data, char _lengthsize, long _array_size,
			      ossBoolean _cons);

extern void DLL_ENTRY _oss_dec_gtime(struct ossGlobal *g, _dstd_parms_def,
                               GeneralizedTime *_data);
extern void DLL_ENTRY _oss_dec_utime(struct ossGlobal *g, _dstd_parms_def,
                               UTCTime *_data);

extern void DLL_ENTRY _oss_dec_uany(struct ossGlobal *g, _dstd_parms_def,
			      ossBoolean _userbuf, void *_data);

extern void DLL_ENTRY _oss_dec_opentype(struct ossGlobal *g, _dstd_parms_def,
			      ossBoolean _userbuf, void *_data);

extern void DLL_ENTRY _oss_dec_defer(struct ossGlobal *g, _dstd_parms_def,
		    ossBoolean userbuf, void *data,
		    short tag, ossBoolean constructed);

extern void DLL_ENTRY _oss_dec_bmpstr(struct ossGlobal *g, _dstd_parms_def,
		    ossBoolean userbuf, void *data,
		    char lengthsize, long size_c, ossBoolean cons);
extern void DLL_ENTRY _oss_dec_unistr(struct ossGlobal *g, _dstd_parms_def,
		    ossBoolean userbuf, void *data,
		    char lengthsize, long size_c, ossBoolean cons);

extern void DLL_ENTRY _oss_dec_eobjid(struct ossGlobal *g, _dstd_parms_def,
		    ossBoolean userbuf, void *data,
		    long size_c);

extern void DLL_ENTRY _oss_dec_ereloid(struct ossGlobal *g, _dstd_parms_def,
		    ossBoolean userbuf, void *data,
		    long size_c);

extern void DLL_ENTRY _oss_dec_utf8bmpstr(struct ossGlobal *g, _dstd_parms_def,
                    ossBoolean userbuf, void *data,
		    char lengthsize, long size_c, ossBoolean cons);

extern void DLL_ENTRY _oss_dec_utf8unistr(struct ossGlobal *g, _dstd_parms_def,
                    ossBoolean userbuf, void *data,
		    char lengthsize, long size_c, ossBoolean cons);

/* Other functions */

/* called to decode unknown extension additions in relaysafe-mode */
extern void DLL_ENTRY _oss_dec_rsf_ext(struct ossGlobal *g, _dstd_parms_def,
    ossBoolean _userbuf, void **_data, char **rsf_pos, long *rsf_len,
    unsigned int *count);
extern void DLL_ENTRY _oss_free_rsf_ext(struct ossGlobal *g, void *_data);

/* called to count the items for set of or sequence of. */
extern long DLL_ENTRY _oss_count_setof_items(struct ossGlobal *g, _dstd_parms_def);

/* called to skip past items for the EXTENSIBLE directive */
extern void DLL_ENTRY _oss_skip_past_item(struct ossGlobal *g, _dstd_parms_def);

extern void DLL_ENTRY _oss_beginBlock(struct ossGlobal *g, long count,
    char **pos, long *max_len);

extern void DLL_ENTRY _oss_nextItem(struct ossGlobal *g, long *max_len);

extern void DLL_ENTRY _oss_endBlock(struct ossGlobal *g, char ** pos, long *max_len,
    unsigned char ct);

/* called to perform limited check for default value */
extern ossBoolean DLL_ENTRY _oss_not_dflt(struct ossGlobal *g, void *data, void *dflt,
   long totalsize, long max_length, int kind);

extern void DLL_ENTRY _oss_chk_enum(struct ossGlobal *g, long data,
	 struct _enum_data *enums);
extern void DLL_ENTRY _oss_chk_uenum(struct ossGlobal *g, unsigned long data,
	 struct _enum_data *enums);
extern void DLL_ENTRY _oss_chk_pac(struct ossGlobal *g, char *value,
		    ULONG_LONG length, struct _char_data *cptr);
extern void DLL_ENTRY _oss_chk_pac2(struct ossGlobal *g, unsigned short *value,
		    ULONG_LONG length, struct _char_data *cptr);
extern void DLL_ENTRY _oss_chk_pac4(struct ossGlobal *g, OSS_INT32 *value,
		    ULONG_LONG length, struct _char_data *cptr);

extern void DLL_ENTRY _oss_prt_bin(struct ossGlobal *world, char *before,
    unsigned char *s, long l, char *after);
extern void DLL_ENTRY _oss_prt_bit(struct ossGlobal *g, void *data,
    unsigned long length);
extern void DLL_ENTRY _oss_prt_char(struct ossGlobal *g, char *data,
    unsigned long length);
extern void DLL_ENTRY _oss_prt_creal(struct ossGlobal *g, char *a);
extern void DLL_ENTRY _oss_prt_gentime(struct ossGlobal *world,
    GeneralizedTime *gt);
extern void DLL_ENTRY _oss_prt_hex(struct ossGlobal *world, char *before,
    unsigned char *s, long l, char *after);
extern void DLL_ENTRY _oss_prt_nchar(struct ossGlobal *g, char *data);
extern void DLL_ENTRY _oss_prt_oct(struct ossGlobal *g, void *data,
    unsigned long length);
extern void DLL_ENTRY _oss_prt_opentype(struct ossGlobal *world,
    OpenType *data);
extern void DLL_ENTRY _oss_prt_pbit(struct ossGlobal *g, void *data,
    unsigned long totalsize, unsigned long max_length);
extern void DLL_ENTRY _oss_prt_real(struct ossGlobal *g, double d);
extern void DLL_ENTRY _oss_prt_utctime(struct ossGlobal *world,
    UTCTime *ut);
extern void DLL_ENTRY _oss_indent(struct ossGlobal *g, int change);
extern void DLL_ENTRY _oss_prt_mbchar(struct ossGlobal *world,
     unsigned char *s, long l, unsigned short width);
extern char* DLL_ENTRY _oss_getdisp_bin(struct ossGlobal *world, char *before,
    unsigned char *s, long l, char *after);
extern char* DLL_ENTRY _oss_getdisp_bit(struct ossGlobal *g, void *data,
    unsigned long length);
extern char* DLL_ENTRY _oss_getdisp_char(struct ossGlobal *g, char *data,
    unsigned long length);
extern char* DLL_ENTRY _oss_getdisp_creal(struct ossGlobal *g, char *a);
extern char* DLL_ENTRY _oss_getdisp_gentime(struct ossGlobal *world,
    GeneralizedTime *gt);
extern char* DLL_ENTRY _oss_getdisp_hex(struct ossGlobal *world, char *before,
    unsigned char *s, long l, char *after);
extern char* DLL_ENTRY _oss_getdisp_nchar(struct ossGlobal *g, char *data);
extern char* DLL_ENTRY _oss_getdisp_oct(struct ossGlobal *g, void *data,
    unsigned long length);
extern char* DLL_ENTRY _oss_getdisp_pbit(struct ossGlobal *g, void *data,
    unsigned long totalsize, unsigned long max_length);
extern char* DLL_ENTRY _oss_getdisp_real(struct ossGlobal *g, double d);
extern char* DLL_ENTRY _oss_getdisp_utctime(struct ossGlobal *world,
    UTCTime *ut);
extern char* DLL_ENTRY _oss_getdisp_mbchar(struct ossGlobal *world,
     unsigned char *s, long l, unsigned short width);
extern char* DLL_ENTRY _oss_getdisp_int(struct ossGlobal *world,
     LONG_LONG l);
extern char* DLL_ENTRY _oss_getdisp_uint(struct ossGlobal *world,
     ULONG_LONG l);
extern void DLL_ENTRY _oss_free_disp(struct ossGlobal *world,
     void *s);
extern int  DLL_ENTRY ossInitXObjectSetTable(struct ossGlobal *, const void * ,
				unsigned int , const void * , unsigned int, void *);
extern ossBoolean DLL_ENTRY _ossCmpPrimitiveValue(struct ossGlobal *, void *,
                          void *, long, long, int);

#else

/* Encode Routines */

extern long     _oss_encd_length();
extern void     _oss_enci_length();
extern long     _oss_encd_tag();
extern void     _oss_enci_tag();


extern long     _oss_encd_bool();
extern void     _oss_enci_bool();
extern long     _oss_encd_int();
extern void     _oss_enci_int();
extern long     _oss_encd_uint();
extern void     _oss_enci_uint();
extern void     _oss_encd_huge_int();
extern void     _oss_encd_uhuge_int();
extern long     _oss_encd_real();
extern void     _oss_enci_real();
extern long     _oss_encd_alobjid();
extern long     _oss_encd_asobjid();
extern long     _oss_encd_aiobjid();
extern void     _oss_enci_alobjid();
extern void     _oss_enci_asobjid();
extern void     _oss_enci_aiobjid();
extern long     _oss_encd_llobjid();
extern long     _oss_encd_lsobjid();
extern long     _oss_encd_liobjid();
extern void     _oss_enci_llobjid();
extern void     _oss_enci_lsobjid();
extern void     _oss_enci_liobjid();
extern long     _oss_encd_ulobjid();
extern long     _oss_encd_usobjid();
extern long     _oss_encd_uiobjid();
extern void     _oss_enci_ulobjid();
extern void     _oss_enci_usobjid();
extern void     _oss_enci_uiobjid();
extern long     _oss_encd_eobjid();
extern long     _oss_encd_ereloid();
extern long     _oss_encd_pbit();
extern void     _oss_enci_pbit();
extern long     _oss_encd_ubit();
extern void     _oss_enci_ubit();
extern long     _oss_encd_vbit();
extern void     _oss_enci_vbit();
extern long     _oss_encd_nstr();
extern void     _oss_enci_nstr();
extern long     _oss_encd_pstr();
extern void     _oss_enci_pstr();
extern long     _oss_encd_ustr();
extern void     _oss_enci_ustr();
extern long     _oss_encd_vstr();
extern void     _oss_enci_vstr();
extern long     _oss_encd_uoct();
extern void     _oss_enci_uoct();
extern long     _oss_encd_voct();
extern void     _oss_enci_voct();
extern long     _oss_encd_gtime();
extern void     _oss_enci_gtime();
extern long     _oss_encd_utime();
extern void     _oss_enci_utime();
extern long     _oss_encd_uany();
extern void     _oss_enci_uany();
extern long     _oss_encd_opentype();
extern void     _oss_enci_opentype();
extern long     _oss_encd_defer();
extern long     _oss_encd_bmpstr();
extern long     _oss_enci_bmpstr();
extern long     _oss_encd_unistr();
extern long     _oss_enci_unistr();
extern long     _oss_encd_utf8unistr();
extern long     _oss_encd_utf8bmpstr();

/* Decode Routines */
/* these return values */
extern void     _oss_dec_tag();
extern long     _oss_dec_length();


extern ossBoolean _oss_dec_bool();
extern long     _oss_dec_lint();
extern int      _oss_dec_iint();
extern short    _oss_dec_sint();
extern void     _oss_dec_hint();
extern unsigned long _oss_dec_ulint();
extern unsigned int _oss_dec_uiint();
extern unsigned short _oss_dec_usint();

/* The code for this one is so huge that it would be criminal to
 * repeat it three times.  Caller should cast the returned value
 * to what she wants.
 */
extern double   _oss_dec_real();

/* Two versions of these functions.  If the type has the pointer
 * directive, then its size is not fixed--the function will allocate
 * the memory.  Otherwise the caller allocates the memory.  (Actually,
 * the type will probably be enclosed in another type, and therefore
 * the memory has already been allocated.)
 */
extern void     _oss_dec_alobjid_ptr();
extern void     _oss_dec_alobjid();

extern void     _oss_dec_aiobjid_ptr();
extern void     _oss_dec_aiobjid();

extern void     _oss_dec_asobjid_ptr();
extern void     _oss_dec_asobjid();

/* These allocate all their own memory.  */
extern void     _oss_dec_llobjid();
extern void     _oss_dec_liobjid();
extern void     _oss_dec_lsobjid();

/*  These are passed the structure containing the count, but allocate
  *  space for the data itself. */
extern void     _oss_dec_ulobjid();
extern void     _oss_dec_uiobjid();
extern void     _oss_dec_usobjid();

/* Caller allocates memory for this one.  Call for both e_pad & e_big_pad. */
extern void     _oss_dec_pbit();

/* See comment before dec_ulobjid. */
extern void     _oss_dec_ubit();

/* Two versions of this one too.  */
extern void     _oss_dec_vbit_ptr();
extern void     _oss_dec_vbit();

extern void     _oss_dec_nstr_ptr();
extern void     _oss_dec_nstr();

extern void     _oss_dec_pstr();
extern void     _oss_dec_ustr();

extern void     _oss_dec_vstr_ptr();
extern void     _oss_dec_vstr();

extern void     _oss_dec_uoct();

extern void     _oss_dec_voct_ptr();
extern void     _oss_dec_voct();

extern void     _oss_dec_gtime();
extern void     _oss_dec_utime();

extern void     _oss_dec_uany();

extern void     _oss_dec_opentype();
extern void     _oss_dec_defer();

extern void     _oss_dec_bmpstr();
extern void     _oss_dec_unistr();
extern void     _oss_dec_eobjid();
extern void     _oss_dec_ereloid();
extern void     _oss_dec_utf8bmpstr();
extern void     _oss_dec_utf8unistr();

/* Other functions */

/* called to decode unknown extension additions in relaysafe-mode */
extern void _oss_dec_rsf_ext();
extern void _oss_free_rsf_ext();

/* called to count the items for set of or sequence of. */
extern long     _oss_count_setof_items();

/* called to skip past items for the EXTENSIBLE directive */
extern void     _oss_skip_past_item();

/* called to perform limited check for default value */
extern ossBoolean _oss_not_dflt();

extern void _oss_chk_enum();
extern void _oss_chk_uenum();
extern void _oss_chk_pac();
extern void _oss_chk_pac2();
extern void _oss_chk_pac4();

extern void _oss_prt_bin();
extern void _oss_prt_bit();
extern void _oss_prt_char();
extern void _oss_prt_creal();
extern void _oss_prt_gentime();
extern void _oss_prt_hex();
extern void _oss_prt_nchar();
extern void _oss_prt_oct();
extern void _oss_prt_opentype();
extern void _oss_prt_pbit();
extern void _oss_prt_real();
extern void _oss_prt_utctime();
extern void _oss_indent();
extern void _oss_prt_mbchar();
extern char* _oss_getdisp_bin();
extern char* _oss_getdisp_bit();
extern char* _oss_getdisp_char();
extern char* _oss_getdisp_creal();
extern char* _oss_getdisp_gentime();
extern char* _oss_getdisp_hex();
extern char* _oss_getdisp_nchar();
extern char* _oss_getdisp_oct();
extern char* _oss_getdisp_pbit();
extern char* _oss_getdisp_real();
extern char* _oss_getdisp_utctime();
extern char* _oss_getdisp_mbchar();
extern char* _oss_getdisp_int();
extern char* _oss_getdisp_uint();
extern void  _oss_free_disp();
extern int   ossInitXObjectSetTable();
extern ossBoolean _ossCmpPrimitiveValue();

#endif /* _OSSNOANSI */

typedef struct ToedDataExt {
    long      bytes_read_by_decoder;
    int       (*ossUserVprint)(struct ossGlobal *, const char *format, 
				va_list ap);
    void     *(DLL_ENTRY_FPTR *_System mallocpSaved)(size_t sz);
    void      (DLL_ENTRY_FPTR *_System freepSaved)(void *p);
    OssBuf    tmpBuffer;
    _mem_array	   _oss_stack;
    _mem_array*	   _oss_stack_tail;
    unsigned char  padding_bits;
    unsigned int    _iOsetNum;	   /* total number of objectsets */
    void	   *_piOsetInfo;   /* Inextensible objectsets info */
    unsigned int    _xOsetNum;	   /* number of extensible object sets */
    XOSetInfoEntry * _pxOsetInfo;  /* Extensible objectsets info */
    int		   (*pcmpInfoObj)(struct ossGlobal *, int , void *, void *, int);
    void           *so_handle;
} ToedDataExt;

typedef struct _EncDecGlobals {

   jmp_buf	  *_oss_err_env;
#ifndef _OSSNOANSI
   void		 (*_old_sigsegv)(int);
   void		 (*_old_sigbus)(int);
#else
   void		 (*_old_sigsegv)();
   void		 (*_old_sigbus)();
#endif
   int		   _oss_asn1err;
   char		  *_oss_err_msg;
  ossBoolean            _buffer_provided;
  ossBoolean            _tag_decoded;
  ossBoolean            _restrain_mode;
   long		   _restrain_size;
  _Context_Anchor  _oss_context_anchor;
   char		  *_oss_outbufpos;
   long		   _oss_outbuflen;
  _mem_array	   _oss_mem;
  _mem_array	  *_oss_mem_tail;
  _mem_array	  *_oss_temp_tail;
  _mem_array	   _oss_temp_mem;
   long		   _oss_mem_alloc;
   long		   _oss_out_alloc;
   unsigned int     relax_ber: 1;
   unsigned int     der: 1;
   unsigned char    man[sizeof(double)];
#if defined(_WINDOWS) || (defined(_DLL)? !defined(ONE_DLL): 0) ||\
    defined(OS2_DLL)  || defined(NETWARE_DLL)
   void (DLL_ENTRY_FPTR *_System _oss_enc_error)(struct ossGlobal *g,
			ossBoolean _jump, enum _err_index _error_code, long _data);
#endif /* _WINDOWS || _DLL || OS2_DLL || NETWARE_DLL */
    ossBoolean            _aligned;
    int             _bitpos;
    struct ossGlobal *_prev;
    char           *_oss_inbufpos;
    long            _oss_inbuflen;
    void           *_prevbuf;
    long             encoding_length;
    void           *_blockptr;
    struct ossGlobal *_next;
    FILE           *asn1outSaved;
    int		  (*asn1prntSaved)(FILE *stream, const char *format, ...);
    int		  (*ossUserPrintp)(struct ossGlobal *, const char *format, ...);
    void	*(*ossUserMallocp)(void *, size_t);
    void	*(*ossUserReallocp)(void *, void *, size_t);
    void	 (*ossUserFreep)(void *, void *);

#ifdef EBCDIC
    iconv_t     toAscii;
    iconv_t     toEbcdic;
#endif
    int indent_level;

    union {
	double size[4];
	void *pointer;
	struct {
	    unsigned short preset_flags;
	    ToedDataExt   *ext;
	    char  *rsf_tagpos;
	    long   rsf_taglen;
	} fields;
    } reserved;

} _EncDecGlobals;

#ifndef relax_per
#define relax_per relax_ber
#endif /* relax_per */

#ifdef __cplusplus
}
#endif

#if defined(macintosh) && defined(__CFM68K__)
#pragma import reset
#endif

#if defined(_MSC_VER) && (defined(_WIN32) || defined(WIN32))
#pragma pack(pop, ossPacking)
#elif defined(_MSC_VER) && (defined(_WINDOWS) || defined(_MSDOS))
#pragma pack()
#elif defined(__BORLANDC__) && (defined(__WIN32__) || defined(__MSDOS__))
#pragma option -a.
#elif defined(__IBMC__) && (defined(__WIN32__) || defined(__OS2__))
#pragma pack()
#elif defined(__WATCOMC__) && (defined(__NT__) || defined(__WINDOWS__) || defined(__DOS__))
#pragma pack(pop)
#endif /* _MSC_VER && _WIN32 */

#ifdef macintosh
#pragma options align=reset
#endif

	/*
	 * The following macros must be #defined if you compile the ASN.1
	 * compiler generated files (.c files) and link-edit with the import
	 * library ossapit.lib, i.e the OSS DLLs ossapit.dll, toedapi.dll,
	 * and toedber.dll/toedper.dll are used.  The following must not
	 * be #defined if you link-edit with the static library toedcode.lib
	 * or toedcomd.lib.  If you use /MD and link with toedcomd.lib,
	 * #define ONE_DLL on the C-compiler command line to disable the
	 * below macros.
	 */
#if (defined(_DLL)? !defined(ONE_DLL): 0) || defined(OS2_DLL) ||\
   (defined(_WINDOWS) && !defined(_WIN32)) || defined(NETWARE_DLL)
#define _oss_count_setof_items (*_g->ft.berTbl->_oss_count_setof_itemsp)
#define _oss_dec_aiobjid       (*_g->ft.berTbl->_oss_dec_aiobjidp)
#define _oss_dec_aiobjid_ptr   (*_g->ft.berTbl->_oss_dec_aiobjid_ptrp)
#define _oss_dec_alobjid       (*_g->ft.berTbl->_oss_dec_alobjidp)
#define _oss_dec_alobjid_ptr   (*_g->ft.berTbl->_oss_dec_alobjid_ptrp)
#define _oss_dec_asobjid       (*_g->ft.berTbl->_oss_dec_asobjidp)
#define _oss_dec_asobjid_ptr   (*_g->ft.berTbl->_oss_dec_asobjid_ptrp)
#define _oss_dec_bool          (*_g->ft.berTbl->_oss_dec_boolp)
#define _oss_dec_creal         (*_g->ft.berTbl->_oss_dec_crealp)
#define _oss_dec_freal         (*_g->ft.berTbl->_oss_dec_frealp)
#define _oss_dec_getmem        (*_g->ft.apiTbl->_oss_dec_getmemp)
#define _oss_dec_getmem_internal (*_g->ft.apiTbl->_oss_dec_getmem_internalp)
#define _oss_dec_gtime         (*_g->ft.berTbl->_oss_dec_gtimep)
#define _oss_dec_hint          (*_g->ft.berTbl->_oss_dec_hintp)
#define _oss_dec_iint          (*_g->ft.berTbl->_oss_dec_iintp)
#define _oss_dec_length        (*_g->ft.berTbl->_oss_dec_lengthp)
#define _oss_dec_lint          (*_g->ft.berTbl->_oss_dec_lintp)
#define _oss_dec_liobjid       (*_g->ft.berTbl->_oss_dec_liobjidp)
#define _oss_dec_llint         (*_g->ft.berTbl->_oss_dec_llintp)
#define _oss_dec_llobjid       (*_g->ft.berTbl->_oss_dec_llobjidp)
#define _oss_dec_lsobjid       (*_g->ft.berTbl->_oss_dec_lsobjidp)
#define _oss_dec_mreal         (*_g->ft.berTbl->_oss_dec_mrealp)
#define _oss_dec_nstr          (*_g->ft.berTbl->_oss_dec_nstrp)
#define _oss_dec_nstr_ptr      (*_g->ft.berTbl->_oss_dec_nstr_ptrp)
#define _oss_dec_opentype      (*_g->ft.berTbl->_oss_dec_opentypep)
#define _oss_dec_pbit          (*_g->ft.berTbl->_oss_dec_pbitp)
#define _oss_dec_pstr          (*_g->ft.berTbl->_oss_dec_pstrp)
#define _oss_dec_real          (*_g->ft.berTbl->_oss_dec_realp)
#define _oss_dec_sint          (*_g->ft.berTbl->_oss_dec_sintp)
#define _oss_dec_tag           (*_g->ft.berTbl->_oss_dec_tagp)
#define _oss_dec_uany          (*_g->ft.berTbl->_oss_dec_uanyp)
#define _oss_dec_ubit          (*_g->ft.berTbl->_oss_dec_ubitp)
#define _oss_dec_uiint         (*_g->ft.berTbl->_oss_dec_uiintp)
#define _oss_dec_uiobjid       (*_g->ft.berTbl->_oss_dec_uiobjidp)
#define _oss_dec_ulint         (*_g->ft.berTbl->_oss_dec_ulintp)
#define _oss_dec_ullint        (*_g->ft.berTbl->_oss_dec_ullintp)
#define _oss_dec_ulobjid       (*_g->ft.berTbl->_oss_dec_ulobjidp)
#define _oss_dec_uoct          (*_g->ft.berTbl->_oss_dec_uoctp)
#define _oss_dec_usint         (*_g->ft.berTbl->_oss_dec_usintp)
#define _oss_dec_usobjid       (*_g->ft.berTbl->_oss_dec_usobjidp)
#define _oss_dec_ustr          (*_g->ft.berTbl->_oss_dec_ustrp)
#define _oss_dec_utime         (*_g->ft.berTbl->_oss_dec_utimep)
#define _oss_dec_vbit          (*_g->ft.berTbl->_oss_dec_vbitp)
#define _oss_dec_vbit_ptr      (*_g->ft.berTbl->_oss_dec_vbit_ptrp)
#define _oss_dec_voct          (*_g->ft.berTbl->_oss_dec_voctp)
#define _oss_dec_voct_ptr      (*_g->ft.berTbl->_oss_dec_voct_ptrp)
#define _oss_dec_vstr          (*_g->ft.berTbl->_oss_dec_vstrp)
#define _oss_dec_vstr_ptr      (*_g->ft.berTbl->_oss_dec_vstr_ptrp)
#define _oss_dec_defer         (*_g->ft.berTbl->_oss_dec_deferp)
#define _oss_enc_error         (*_g->ft.berTbl->_oss_enc_errorp)
#define _oss_enc_pop           (*_g->ft.apiTbl->_oss_enc_popp)
#define _oss_enc_push          (*_g->ft.apiTbl->_oss_enc_pushp)
#define _oss_encd_aiobjid      (*_g->ft.berTbl->_oss_encd_aiobjidp)
#define _oss_encd_alobjid      (*_g->ft.berTbl->_oss_encd_alobjidp)
#define _oss_encd_asobjid      (*_g->ft.berTbl->_oss_encd_asobjidp)
#define _oss_encd_bool         (*_g->ft.berTbl->_oss_encd_boolp)
#define _oss_encd_creal        (*_g->ft.berTbl->_oss_encd_crealp)
#define _oss_encd_gtime        (*_g->ft.berTbl->_oss_encd_gtimep)
#define _oss_encd_huge_int     (*_g->ft.berTbl->_oss_encd_huge_intp)
#define _oss_encd_uhuge_int    (*_g->ft.berTbl->_oss_encd_uhuge_intp)
#define _oss_encd_int          (*_g->ft.berTbl->_oss_encd_intp)
#define _oss_encd_length       (*_g->ft.berTbl->_oss_encd_lengthp)
#define _oss_encd_liobjid      (*_g->ft.berTbl->_oss_encd_liobjidp)
#define _oss_encd_llobjid      (*_g->ft.berTbl->_oss_encd_llobjidp)
#define _oss_encd_lsobjid      (*_g->ft.berTbl->_oss_encd_lsobjidp)
#define _oss_encd_mreal        (*_g->ft.berTbl->_oss_encd_mrealp)
#define _oss_encd_nstr         (*_g->ft.berTbl->_oss_encd_nstrp)
#define _oss_encd_opentype     (*_g->ft.berTbl->_oss_encd_opentypep)
#define _oss_encd_pbit         (*_g->ft.berTbl->_oss_encd_pbitp)
#define _oss_encd_pstr         (*_g->ft.berTbl->_oss_encd_pstrp)
#define _oss_encd_real         (*_g->ft.berTbl->_oss_encd_realp)
#define _oss_encd_tag          (*_g->ft.berTbl->_oss_encd_tagp)
#define _oss_encd_uany         (*_g->ft.berTbl->_oss_encd_uanyp)
#define _oss_encd_ubit         (*_g->ft.berTbl->_oss_encd_ubitp)
#define _oss_encd_uint         (*_g->ft.berTbl->_oss_encd_uintp)
#define _oss_encd_uiobjid      (*_g->ft.berTbl->_oss_encd_uiobjidp)
#define _oss_encd_ulobjid      (*_g->ft.berTbl->_oss_encd_ulobjidp)
#define _oss_encd_uoct         (*_g->ft.berTbl->_oss_encd_uoctp)
#define _oss_encd_usobjid      (*_g->ft.berTbl->_oss_encd_usobjidp)
#define _oss_encd_ustr         (*_g->ft.berTbl->_oss_encd_ustrp)
#define _oss_encd_utime        (*_g->ft.berTbl->_oss_encd_utimep)
#define _oss_encd_vbit         (*_g->ft.berTbl->_oss_encd_vbitp)
#define _oss_encd_voct         (*_g->ft.berTbl->_oss_encd_voctp)
#define _oss_encd_vstr         (*_g->ft.berTbl->_oss_encd_vstrp)
#define _oss_encd_defer        (*_g->ft.berTbl->_oss_encd_deferp)
#define _oss_enci_aiobjid      (*_g->ft.berTbl->_oss_enci_aiobjidp)
#define _oss_enci_alobjid      (*_g->ft.berTbl->_oss_enci_alobjidp)
#define _oss_enci_asobjid      (*_g->ft.berTbl->_oss_enci_asobjidp)
#define _oss_enci_bool         (*_g->ft.berTbl->_oss_enci_boolp)
#define _oss_enci_creal        (*_g->ft.berTbl->_oss_enci_crealp)
#define _oss_enci_gtime        (*_g->ft.berTbl->_oss_enci_gtimep)
#define _oss_enci_int          (*_g->ft.berTbl->_oss_enci_intp)
#define _oss_enci_length       (*_g->ft.berTbl->_oss_enci_lengthp)
#define _oss_enci_liobjid      (*_g->ft.berTbl->_oss_enci_liobjidp)
#define _oss_enci_llobjid      (*_g->ft.berTbl->_oss_enci_llobjidp)
#define _oss_enci_lsobjid      (*_g->ft.berTbl->_oss_enci_lsobjidp)
#define _oss_enci_mreal        (*_g->ft.berTbl->_oss_enci_mrealp)
#define _oss_enci_nstr         (*_g->ft.berTbl->_oss_enci_nstrp)
#define _oss_enci_opentype     (*_g->ft.berTbl->_oss_enci_opentypep)
#define _oss_enci_pbit         (*_g->ft.berTbl->_oss_enci_pbitp)
#define _oss_enci_pstr         (*_g->ft.berTbl->_oss_enci_pstrp)
#define _oss_enci_real         (*_g->ft.berTbl->_oss_enci_realp)
#define _oss_enci_tag          (*_g->ft.berTbl->_oss_enci_tagp)
#define _oss_enci_uany         (*_g->ft.berTbl->_oss_enci_uanyp)
#define _oss_enci_ubit         (*_g->ft.berTbl->_oss_enci_ubitp)
#define _oss_enci_uint         (*_g->ft.berTbl->_oss_enci_uintp)
#define _oss_enci_uiobjid      (*_g->ft.berTbl->_oss_enci_uiobjidp)
#define _oss_enci_ulobjid      (*_g->ft.berTbl->_oss_enci_ulobjidp)
#define _oss_enci_uoct         (*_g->ft.berTbl->_oss_enci_uoctp)
#define _oss_enci_usobjid      (*_g->ft.berTbl->_oss_enci_usobjidp)
#define _oss_enci_ustr         (*_g->ft.berTbl->_oss_enci_ustrp)
#define _oss_enci_utime        (*_g->ft.berTbl->_oss_enci_utimep)
#define _oss_enci_vbit         (*_g->ft.berTbl->_oss_enci_vbitp)
#define _oss_enci_voct         (*_g->ft.berTbl->_oss_enci_voctp)
#define _oss_enci_vstr         (*_g->ft.berTbl->_oss_enci_vstrp)
#define _oss_free_creal        (*_g->ft.berTbl->_oss_free_crealp)
#define _oss_releaseMem        (*_g->ft.apiTbl->_oss_releaseMemp)
#define _oss_set_outmem_d      (*_g->ft.apiTbl->_oss_set_outmem_dp)
#define _oss_encd_eobjid       (*_g->ft.berTbl->_oss_encd_eobjidp)
#define _oss_dec_eobjid        (*_g->ft.berTbl->_oss_dec_eobjidp)
#define _oss_encd_ereloid      (*_g->ft.berTbl->_oss_encd_ereloidp)
#define _oss_dec_ereloid       (*_g->ft.berTbl->_oss_dec_ereloidp)
#define _oss_encd_dubit        (*_g->ft.berTbl->_oss_encd_dubitp)
#define _oss_encd_dvbit        (*_g->ft.berTbl->_oss_encd_dvbitp)
#define _oss_not_dflt          (*_g->ft.berTbl->_oss_not_dfltp)
#define _oss_beginBlock        (*_g->ft.apiTbl->_oss_beginBlockp)
#define _oss_nextItem          (*_g->ft.apiTbl->_oss_nextItemp)
#define _oss_endBlock          (*_g->ft.apiTbl->_oss_endBlockp)
#define _oss_chk_enum          (*_g->ft.berTbl->_oss_chk_enump)
#define _oss_chk_uenum         (*_g->ft.berTbl->_oss_chk_uenump)
#define _oss_chk_pac           (*_g->ft.berTbl->_oss_chk_pacp)
#define _oss_chk_pac2          (*_g->ft.berTbl->_oss_chk_pac2)
#define _oss_chk_pac4          (*_g->ft.berTbl->_oss_chk_pac4)
#define _oss_prt_bin           (*_g->ft.apiTbl->_oss_prt_binp)
#define _oss_prt_bit           (*_g->ft.apiTbl->_oss_prt_bitp)
#define _oss_prt_char          (*_g->ft.apiTbl->_oss_prt_charp)
#define _oss_prt_creal         (*_g->ft.apiTbl->_oss_prt_crealp)
#define _oss_prt_gentime       (*_g->ft.apiTbl->_oss_prt_gentimep)
#define _oss_prt_hex           (*_g->ft.apiTbl->_oss_prt_hexp)
#define _oss_prt_nchar         (*_g->ft.apiTbl->_oss_prt_ncharp)
#define _oss_prt_oct           (*_g->ft.apiTbl->_oss_prt_octp)
#define _oss_prt_opentype      (*_g->ft.apiTbl->_oss_prt_opentypep)
#define _oss_prt_pbit          (*_g->ft.apiTbl->_oss_prt_pbitp)
#define _oss_prt_real          (*_g->ft.apiTbl->_oss_prt_realp)
#define _oss_prt_utctime       (*_g->ft.apiTbl->_oss_prt_utctimep)
#define _oss_indent            (*_g->ft.apiTbl->_oss_indentp)
#define _oss_prt_mbchar        (*_g->ft.apiTbl->_oss_prt_mbcharp)
#define _oss_getdisp_bin       (*_g->ft.apiTbl->_oss_getdisp_binp)
#define _oss_getdisp_bit       (*_g->ft.apiTbl->_oss_getdisp_bitp)
#define _oss_getdisp_char      (*_g->ft.apiTbl->_oss_getdisp_charp)
#define _oss_getdisp_creal     (*_g->ft.apiTbl->_oss_getdisp_crealp)
#define _oss_getdisp_gentime   (*_g->ft.apiTbl->_oss_getdisp_gentimep)
#define _oss_getdisp_hex       (*_g->ft.apiTbl->_oss_getdisp_hexp)
#define _oss_getdisp_nchar     (*_g->ft.apiTbl->_oss_getdisp_ncharp)
#define _oss_getdisp_oct       (*_g->ft.apiTbl->_oss_getdisp_octp)
#define _oss_getdisp_pbit      (*_g->ft.apiTbl->_oss_getdisp_pbitp)
#define _oss_getdisp_real      (*_g->ft.apiTbl->_oss_getdisp_realp)
#define _oss_getdisp_utctime   (*_g->ft.apiTbl->_oss_getdisp_utctimep)
#define _oss_getdisp_mbchar    (*_g->ft.apiTbl->_oss_getdisp_mbcharp)
#define _oss_getdisp_int       (*_g->ft.apiTbl->_oss_getdisp_intp)
#define _oss_getdisp_uint      (*_g->ft.apiTbl->_oss_getdisp_uintp)
#define _oss_free_disp         (*_g->ft.apiTbl->_oss_free_dispp)
#define ossInitXObjectSetTable (*world->ft.apiTbl->_oss_init_oset_tablep)
#define	_ossCmpPrimitiveValue  (*_g->ft.apiTbl->_ossCmpPrimitiveValuep)
#define _oss_encd_utf8bmpstr   (*_g->ft.berTbl->_oss_encd_utf8bmpstrp)
#define _oss_encd_utf8unistr   (*_g->ft.berTbl->_oss_encd_utf8unistrp)
#define _oss_dec_utf8bmpstr    (*_g->ft.berTbl->_oss_dec_utf8bmpstrp)
#define _oss_dec_utf8unistr    (*_g->ft.berTbl->_oss_dec_utf8unistrp)
#define _oss_skip_past_item    (*_g->ft.berTbl->_oss_skip_past_itemp)
#define _oss_encd_bmpstr       (*_g->ft.berTbl->_oss_encd_bmpstrp)
#define _oss_encd_unistr       (*_g->ft.berTbl->_oss_encd_unistrp)
#define _oss_enci_bmpstr       (*_g->ft.berTbl->_oss_enci_bmpstrp)
#define _oss_enci_unistr       (*_g->ft.berTbl->_oss_enci_unistrp)
#define _oss_dec_bmpstr        (*_g->ft.berTbl->_oss_dec_bmpstrp)
#define _oss_dec_unistr        (*_g->ft.berTbl->_oss_dec_unistrp)
#define _oss_dec_rsf_ext       (*_g->ft.berTbl->_oss_dec_rsf_extp)
#define _oss_free_rsf_ext      (*_g->ft.berTbl->_oss_free_rsf_extp)
#endif /* _DLL || OS2_DLL || (_WINDOWS && !_WIN32) */

#endif /* ossber_hdr_file */
