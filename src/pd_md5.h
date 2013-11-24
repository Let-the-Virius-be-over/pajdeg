//
//  pd_md5.h
//
//  Source: https://www.ietf.org/rfc/rfc1321.txt
//

/* 
 Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 rights reserved.
 
 License to copy and use this software is granted provided that it
 is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 Algorithm" in all material mentioning or referencing this software
 or this function.
 
 License is also granted to make and use derivative works provided
 that such works are identified as "derived from the RSA Data
 Security, Inc. MD5 Message-Digest Algorithm" in all material
 mentioning or referencing the derived work.
 
 RSA Data Security, Inc. makes no representations concerning either
 the merchantability of this software or the suitability of this
 software for any particular purpose. It is provided "as is"
 without express or implied warranty of any kind.
 
 These notices must be retained in any copies of any part of this
 documentation and/or software.
 */

#ifndef INCLUDED_PD_MD5
#define INCLUDED_PD_MD5

#include "PDDefines.h"

#ifdef PD_SUPPORT_CRYPTO

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned long int UINT4;

/* MD5 context. */
typedef struct pd_md5_ctx pd_md5_ctx;
struct pd_md5_ctx {
    UINT4 state[4];                 ///< state (ABCD)
    UINT4 count[2];                 ///< number of bits, modulo 2^64 (lsb first)
    unsigned char buffer[64];       ///< input buffer
};

extern void pd_md5_init(pd_md5_ctx *ctx);
extern void pd_md5_update(pd_md5_ctx *ctx, unsigned char *data, unsigned int len);
extern void pd_md5_final(unsigned char *md, pd_md5_ctx *ctx);

extern void pd_md5(unsigned char *data, unsigned int len, unsigned char *result);

#endif

#endif
