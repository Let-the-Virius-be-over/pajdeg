//
// PDStringUTF.c
//
// Copyright (c) 2012 - 2014 Karl-Johan Alm (http://github.com/kallewoof)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <iconv.h>
#include "PDString.h"
#include "pd_internal.h"

 /* Converts, using conversion descriptor `cd', at most `*inbytesleft' bytes
   starting at `*inbuf', writing at most `*outbytesleft' bytes starting at
   `*outbuf'.
   Decrements `*inbytesleft' and increments `*inbuf' by the same amount.
   Decrements `*outbytesleft' and increments `*outbuf' by the same amount. */
size_t iconv (iconv_t /*cd*/,
	char ** __restrict /*inbuf*/,  size_t * __restrict /*inbytesleft*/,
	char ** __restrict /*outbuf*/, size_t * __restrict /*outbytesleft*/);

PDBool iconv_unicode_mb_to_uc_fb_called = false;
PDBool iconv_unicode_uc_to_mb_fb_called = false;

/* Fallback function.  Invoked when a small number of bytes could not be
 converted to a Unicode character.  This function should process all
 bytes from inbuf and may produce replacement Unicode characters by calling
 the write_replacement callback repeatedly.  */
void pdstring_iconv_unicode_mb_to_uc_fallback(const char* inbuf, size_t inbufsize,
                                              void (*write_replacement) (const unsigned int *buf, size_t buflen,
                                                                         void* callback_arg),
                                              void* callback_arg,
                                              void* data)
{
    iconv_unicode_mb_to_uc_fb_called = true;
}

/* Fallback function.  Invoked when a Unicode character could not be converted
 to the target encoding.  This function should process the character and
 may produce replacement bytes (in the target encoding) by calling the
 write_replacement callback repeatedly.  */
void pdstring_iconv_unicode_uc_to_mb_fallback(unsigned int code,
                                              void (*write_replacement) (const char *buf, size_t buflen,
                                                                         void* callback_arg),
                                              void* callback_arg,
                                              void* data)
{
    iconv_unicode_uc_to_mb_fb_called = true;
}

const struct iconv_fallbacks pdstring_iconv_fallbacks = {
    pdstring_iconv_unicode_mb_to_uc_fallback,
    pdstring_iconv_unicode_uc_to_mb_fallback,
    NULL,
    NULL,
    NULL
};

void PDStringDetermineEncoding(PDStringRef string);

static char **enc_names = NULL;

static inline void setup_enc_names() 
{
    PDAssert(__PDSTRINGENC_END == 6);
    enc_names = malloc(sizeof(char*) * __PDSTRINGENC_END);
    enc_names[0] = "ASCII";
    enc_names[1] = "UTF-8";
    enc_names[2] = "UTF-16BE";
    enc_names[3] = "UTF-16LE";
    enc_names[4] = "UTF-32";
    enc_names[5] = "MACROMAN";
}

const char *PDStringEncodingToIconvName(PDStringEncoding enc)
{
    if (enc < 1 || enc > __PDSTRINGENC_END) return NULL;
    if (enc_names == NULL) setup_enc_names();
    return enc_names[enc-1];
}

PDStringRef PDUTF8String(PDStringRef string)
{
    PDStringRef source = string;
    
    // we get a lot of "( )" strings, so we check that first off
    PDBool onlySpace = true;
    for (int i = string->wrapped; onlySpace && i < string->length - string->wrapped; i++) 
        onlySpace = string->data[i] == ' ';
    if (onlySpace) {
        string->enc = PDStringEncodingUTF8;
        return string;
    }
    
    // does the string have an utf8 alternative already?
    if (string->alt && string->alt->enc == PDStringEncodingUTF8) return string->alt;
    
    // we need an escaped or binary representation of the string
    if (PDStringTypeBinary != string->type && PDStringTypeEscaped != string->type) {
        source = PDAutorelease(PDStringCreateBinaryFromString(string));
    }
    
//    // first guess: UTF8
//    {
//        UTF8 *sourceData = (UTF8 *)&source->data[string->wrapped];
//        UTF8 *sourceEnd = &sourceData[(source->length-(string->wrapped<<1))];
//        
//        if (isLegalUTF8String((const UTF8 **)&sourceData, sourceEnd)) {
//            string->enc = PDStringEncodingUTF8;
//            return string;
//        }
//    }
    
//    ConversionResult cr;
    
    iconv_t cd;
    
    PDInteger cap = (3 * source->length)>>1;
    char *results = malloc(sizeof(char) * cap);
    
    for (PDStringEncoding enc = PDStringEncodingUTF8; enc > 0; enc -= 1 + (enc-1 == PDStringEncodingUTF8)) {
        size_t targetLeft = cap;
        char *targetStart = results;

        cd = iconv_open(enc == PDStringEncodingUTF8 ? "UTF-16" : "UTF-8", PDStringEncodingToIconvName(enc));
        
        iconvctl(cd, ICONV_SET_FALLBACKS, (void*)&pdstring_iconv_fallbacks);
        
        char *sourceData = (char *)&source->data[string->wrapped];
        size_t sourceLeft = source->length - (string->wrapped<<1);
        size_t oldSourceLeft;
        
        while (1) {
            iconv_unicode_mb_to_uc_fb_called = iconv_unicode_uc_to_mb_fb_called = false;
            oldSourceLeft = sourceLeft;
            iconv(cd, &sourceData, &sourceLeft, &targetStart, &targetLeft);
            if (oldSourceLeft == sourceLeft || sourceLeft == 0 || iconv_unicode_uc_to_mb_fb_called || iconv_unicode_mb_to_uc_fb_called) break;
            
            targetLeft += cap;
            cap <<= 1;
            PDSize size = targetStart - results;
            results = realloc(results, cap);
            targetStart = results + size;
        }
        
        iconv_close(cd);
        
        if (sourceLeft == 0 && !iconv_unicode_mb_to_uc_fb_called && !iconv_unicode_uc_to_mb_fb_called) {
            string->enc = enc;
            if (string->enc == PDStringEncodingUTF8) {
                free(results);
                return string;
            }
            
            PDRelease(string->alt);
            string->alt = PDStringCreateBinary((char *)results, (targetStart-results));
            string->alt->enc = PDStringEncodingUTF8;
            return string->alt;
        }
        
        if (enc == PDStringEncodingUTF8) enc = __PDSTRINGENC_END + 1;
    }
    
//    // second guess: mac roman
//    {
//        
//        char *sourceData = (char *)&source->data[string->wrapped];
//        size_t sourceLeft = source->length - (string->wrapped<<1);
//        
//        while (1) {
//            if (0 == iconv(cd, &sourceData, &sourceLeft, &targetStart, &targetLeft)) break;
//            if (sourceLeft == 0) break;
//            
//            cap <<= 1;
//            PDSize size = targetStart - results;
//            results = realloc(results, cap);
//            targetStart = results + size;
//        }
//        
//        if (sourceLeft == 0) {
//            string->enc = PDStringEncodingMacRoman;
//            PDRelease(string->alt);
//            string->alt = PDStringCreateBinary((char *)results, (targetStart-results));
//            string->alt->enc = PDStringEncodingUTF8;
//            return string->alt;
//        }
//    }
//    //    if (isProbablyMacRoman((const char **)sourceData, (const char *)sourceEnd)) {
//    //        string->enc = PDStringEncodingMacRoman;
//    //        return string;
//    //    }
//    
//    // third guess: UTF16
//    {
//        cd = iconv_open("UTF-8", "UTF-16BE");
//        
//        char *sourceData = (char *)&source->data[string->wrapped];
//        size_t sourceLeft = source->length - (string->wrapped<<1);
//        while (1) {
//            if (0 == iconv(cd, &sourceData, &sourceLeft, &targetStart, &targetLeft)) break;
//            if (sourceLeft == 0) break;
//            
//            cap <<= 1;
//            PDSize size = targetStart - results;
//            results = realloc(results, cap);
//            targetStart = results + size;
//        } 
//        
//        if (sourceLeft == 0) {
//            string->enc = PDStringEncodingUTF16;
//            PDRelease(string->alt);
//            string->alt = PDStringCreateBinary((char *)results, (targetStart-results));
//            string->alt->enc = PDStringEncodingUTF8;
//            return string->alt;
//        }
//    }
//    
//    // fourth guess: UTF32
//    {
//        UTF32 *sourceData = (UTF32 *)&source->data[string->wrapped];
//        UTF32 *sourceEnd = &sourceData[(source->length-(string->wrapped<<1))/4];
//        while (1) {
//            cr = ConvertUTF32toUTF8((const UTF32 **)&sourceData, sourceEnd, &targetStart, targetEnd, strictConversion);
//            if (cr != targetExhausted) break;
//            
//            cap <<= 1;
//            PDSize size = targetStart - results;
//            results = realloc(results, cap);
//            targetStart = results + size;
//            targetEnd = targetStart + cap;
//        }
//        
//        if (cr == conversionOK) {
//            string->enc = PDStringEncodingUTF32;
//            PDRelease(string->alt);
//            string->alt = PDStringCreateBinary((char *)results, (targetStart-results));
//            string->alt->enc = PDStringEncodingUTF8;
//            return string->alt;
//        }
//    }
    
    free(results);
    
    // unable to determine string type
    string->enc = PDStringEncodingUndefined;
    return NULL;
}

PDStringRef PDUTF16String(PDStringRef string)
{
    PDStringRef source = string;
    
    // does the string have an utf16 alternative already?
    if (string->alt && string->alt->enc == PDStringEncodingUTF16BE) return string->alt;
    
    // we need an escaped or binary representation of the string
    if (PDStringTypeBinary != string->type && PDStringTypeEscaped != string->type) {
        source = PDAutorelease(PDStringCreateBinaryFromString(string));
    }
    
    // we need a defined encoding
    if (PDStringEncodingDefault == string->enc) PDStringDetermineEncoding(string);
    
    // is the string UTF16 already?
    if (PDStringEncodingUTF16BE == string->enc) return string;
    
    iconv_t cd;
    
    PDInteger cap = (3 * source->length)>>1;
    char *results = malloc(sizeof(char) * cap);
    
    for (PDStringEncoding enc = PDStringEncodingUTF8; enc > 0; enc -= 1 + (enc-1 == PDStringEncodingUTF8)) {
        size_t targetLeft = cap;
        char *targetStart = results;

        cd = iconv_open(enc == PDStringEncodingUTF16BE ? "UTF-8" : "UTF-16BE", PDStringEncodingToIconvName(enc));
        
        char *sourceData = (char *)&source->data[string->wrapped];
        size_t sourceLeft = source->length - (string->wrapped<<1);
        size_t oldSourceLeft;
        
        while (1) {
            iconv_unicode_mb_to_uc_fb_called = iconv_unicode_uc_to_mb_fb_called = false;
            oldSourceLeft = sourceLeft;
            iconv(cd, &sourceData, &sourceLeft, &targetStart, &targetLeft);
            if (oldSourceLeft == sourceLeft || sourceLeft == 0 || iconv_unicode_uc_to_mb_fb_called || iconv_unicode_mb_to_uc_fb_called) break;
            
            targetLeft += cap;
            cap <<= 1;
            PDSize size = targetStart - results;
            results = realloc(results, cap);
            targetStart = results + size;
        }
        
        iconv_close(cd);
        
        if (sourceLeft == 0 && !iconv_unicode_mb_to_uc_fb_called && !iconv_unicode_uc_to_mb_fb_called) {
            string->enc = enc;
            if (string->enc == PDStringEncodingUTF16BE) {
                free(results);
                return string;
            }
            
            PDRelease(string->alt);
            string->alt = PDStringCreateBinary((char *)results, (targetStart-results));
            string->alt->enc = PDStringEncodingUTF16BE;
            return string->alt;
        }
        
        if (enc == PDStringEncodingUTF16BE) enc = __PDSTRINGENC_END + 1;
    }
    
    free(results);
    
    // failure
    return NULL;
}

void PDStringDetermineEncoding(PDStringRef string)
{
    PDStringRef source = string;
    
    if (string->enc != PDStringEncodingDefault) return;
    
    // we need an escaped or binary representation of the string
    if (PDStringTypeBinary != string->type && PDStringTypeEscaped != string->type) {
        source = PDAutorelease(PDStringCreateBinaryFromString(string));
    }
    
    // we try to create UTF8 string; the string will have its encoding set on return
    PDStringRef u8string = PDUTF8String(string);
    
    if (u8string) return;
    
    PDWarn("Undefined string encoding encountered");
}

PDStringRef PDStringCreateUTF8Encoded(PDStringRef string)
{
    if (string->enc == PDStringEncodingDefault) {
        PDStringDetermineEncoding(string);
    }
    
    switch (string->enc) {
        case PDStringEncodingUndefined:
            return NULL;
        case PDStringEncodingDefault:
        case PDStringEncodingASCII:
        case PDStringEncodingUTF8:
            return PDRetain(string);
        default:
            return PDRetain(PDUTF8String(string));
    }
}

PDStringRef PDStringCreateUTF16Encoded(PDStringRef string)
{
    if (string->enc == PDStringEncodingDefault) {
        PDStringDetermineEncoding(string);
    }
    
    switch (string->enc) {
        case PDStringEncodingUndefined:
            return NULL;
        case PDStringEncodingUTF16BE:
            return PDRetain(string);
        default:
            return PDRetain(PDUTF16String(string));
    }
}

#if 0

// Parts of this file were based on code by Unicode, Inc.

/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */


#include "PDString.h"
#include "pd_internal.h"
 
typedef unsigned int    UTF32;  /* at least 32 bits */
typedef unsigned short  UTF16;  /* at least 16 bits */
typedef unsigned char   UTF8;   /* typically 8 bits */

/* Some fundamental constants */
#define UNI_REPLACEMENT_CHAR    (UTF32)0x0000FFFD
#define UNI_MAX_BMP             (UTF32)0x0000FFFF
#define UNI_MAX_UTF16           (UTF32)0x0010FFFF
#define UNI_MAX_UTF32           (UTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32     (UTF32)0x0010FFFF

#define UNI_MAX_UTF8_BYTES_PER_CODE_POINT 4

#define UNI_UTF16_BYTE_ORDER_MARK_NATIVE  0xFEFF
#define UNI_UTF16_BYTE_ORDER_MARK_SWAPPED 0xFFFE

static const int halfShift  = 10; /* used for shifting by 10 bits */

static const UTF32 halfBase = 0x0010000UL;
static const UTF32 halfMask = 0x3FFUL;

#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF

typedef enum {
    conversionOK,           /* conversion successful */
    sourceExhausted,        /* partial character in source, but hit end */
    targetExhausted,        /* insuff. room in target for conversion */
    sourceIllegal           /* source sequence is illegal/malformed */
} ConversionResult;

typedef enum {
    strictConversion = 0,
    lenientConversion
} ConversionFlags;

ConversionResult ConvertUTF8toUTF16 (
                                     const UTF8** sourceStart, const UTF8* sourceEnd,
                                     UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags);

/**
 * Convert a partial UTF8 sequence to UTF32.  If the sequence ends in an
 * incomplete code unit sequence, returns \c sourceExhausted.
 */
ConversionResult ConvertUTF8toUTF32Partial(
                                           const UTF8** sourceStart, const UTF8* sourceEnd,
                                           UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

/**
 * Convert a partial UTF8 sequence to UTF32.  If the sequence ends in an
 * incomplete code unit sequence, returns \c sourceIllegal.
 */
ConversionResult ConvertUTF8toUTF32(
                                    const UTF8** sourceStart, const UTF8* sourceEnd,
                                    UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF16toUTF8 (
                                     const UTF16** sourceStart, const UTF16* sourceEnd,
                                     UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF32toUTF8 (
                                     const UTF32** sourceStart, const UTF32* sourceEnd,
                                     UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF16toUTF32 (
                                      const UTF16** sourceStart, const UTF16* sourceEnd,
                                      UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF32toUTF16 (
                                      const UTF32** sourceStart, const UTF32* sourceEnd,
                                      UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags);

PDBool isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd);

PDBool isLegalUTF8String(const UTF8 **source, const UTF8 *sourceEnd);

unsigned getNumBytesForUTF8(UTF8 firstByte);

/* --------------------------------------------------------------------- */

void PDStringDetermineEncoding(PDStringRef string);

//PDBool isProbablyMacRoman(const char **source, const char *sourceEnd);

ConversionResult ConvertMacRomantoUTF8 (
                                        const unsigned char** sourceStart, const unsigned char* sourceEnd,
                                        UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

PDStringRef PDUTF8String(PDStringRef string)
{
    PDStringRef source = string;
    
    // does the string have an utf8 alternative already?
    if (string->alt && string->alt->enc == PDStringEncodingUTF8) return string->alt;
    
    // we need an escaped or binary representation of the string
    if (PDStringTypeBinary != string->type && PDStringTypeEscaped != string->type) {
        source = PDAutorelease(PDStringCreateBinaryFromString(string));
    }

    // first guess: UTF8
    {
        UTF8 *sourceData = (UTF8 *)&source->data[string->wrapped];
        UTF8 *sourceEnd = &sourceData[(source->length-(string->wrapped<<1))];
        
        if (isLegalUTF8String((const UTF8 **)&sourceData, sourceEnd)) {
            string->enc = PDStringEncodingUTF8;
            return string;
        }
    }
    
    ConversionResult cr;
    PDInteger cap = (3 * source->length)>>1;
    UTF8 *targetStart = malloc(sizeof(UTF8) * cap);
    UTF8 *targetEnd = targetStart + cap;
    UTF8 *results = targetStart;
    
    // second guess: mac roman
    {
        UTF8 *sourceData = (UTF8 *)&source->data[string->wrapped];
        UTF8 *sourceEnd = &sourceData[(source->length-(string->wrapped<<1))];
        
        while (1) {
            cr = ConvertMacRomantoUTF8((const unsigned char **)&sourceData, (const unsigned char *)sourceEnd, &targetStart, targetEnd, strictConversion);
            if (cr != targetExhausted) break;
            
            cap <<= 1;
            PDSize size = targetStart - results;
            results = realloc(results, cap);
            targetStart = results + size;
            targetEnd = targetStart + cap;
        }
        
        if (cr == conversionOK) {
            string->enc = PDStringEncodingMacRoman;
            PDRelease(string->alt);
            string->alt = PDStringCreateBinary((char *)results, (targetStart-results));
            string->alt->enc = PDStringEncodingUTF8;
            return string->alt;
        }
    }
//    if (isProbablyMacRoman((const char **)sourceData, (const char *)sourceEnd)) {
//        string->enc = PDStringEncodingMacRoman;
//        return string;
//    }
    
    // third guess: UTF16
    {
        UTF16 *sourceData = (UTF16 *)&source->data[string->wrapped];
        UTF16 *sourceEnd = &sourceData[(source->length-(string->wrapped<<1))/2];
        while (1) {
            cr = ConvertUTF16toUTF8((const UTF16 **)&sourceData, sourceEnd, &targetStart, targetEnd, strictConversion);
            if (cr != targetExhausted) break;
            
            cap <<= 1;
            PDSize size = targetStart - results;
            results = realloc(results, cap);
            targetStart = results + size;
            targetEnd = targetStart + cap;
        } 
        
        if (cr == conversionOK) {
            string->enc = PDStringEncodingUTF16;
            PDRelease(string->alt);
            string->alt = PDStringCreateBinary((char *)results, (targetStart-results));
            string->alt->enc = PDStringEncodingUTF8;
            return string->alt;
        }
    }
    
    // fourth guess: UTF32
    {
        UTF32 *sourceData = (UTF32 *)&source->data[string->wrapped];
        UTF32 *sourceEnd = &sourceData[(source->length-(string->wrapped<<1))/4];
        while (1) {
            cr = ConvertUTF32toUTF8((const UTF32 **)&sourceData, sourceEnd, &targetStart, targetEnd, strictConversion);
            if (cr != targetExhausted) break;
            
            cap <<= 1;
            PDSize size = targetStart - results;
            results = realloc(results, cap);
            targetStart = results + size;
            targetEnd = targetStart + cap;
        }
        
        if (cr == conversionOK) {
            string->enc = PDStringEncodingUTF32;
            PDRelease(string->alt);
            string->alt = PDStringCreateBinary((char *)results, (targetStart-results));
            string->alt->enc = PDStringEncodingUTF8;
            return string->alt;
        }
    }
    
    free(results);
    
    // unable to determine string type
    string->enc = PDStringEncodingUndefined;
    return NULL;
}

PDStringRef PDUTF16String(PDStringRef string)
{
    PDStringRef source = string;
    
    // does the string have an utf16 alternative already?
    if (string->alt && string->alt->enc == PDStringEncodingUTF16) return string->alt;
    
    // we need an escaped or binary representation of the string
    if (PDStringTypeBinary != string->type && PDStringTypeEscaped != string->type) {
        source = PDAutorelease(PDStringCreateBinaryFromString(string));
    }
    
    // we need a defined encoding
    if (PDStringEncodingDefault == string->enc) PDStringDetermineEncoding(string);
    
    // is the string UTF16 already?
    if (PDStringEncodingUTF16 == string->enc) return string;
    
    ConversionResult cr;
    PDInteger cap = (3 * source->length)/2;
    UTF16 *targetStart = malloc(sizeof(UTF16) * cap);
    UTF16 *targetEnd = targetStart + cap;
    UTF16 *results = targetStart;
    
    if (PDStringEncodingUTF8 == string->enc || PDStringEncodingASCII == string->enc) {
        UTF8 *sourceData = (UTF8 *)&source->data[string->wrapped];
        UTF8 *sourceEnd = &sourceData[source->length];
        while (1) {
            cr = ConvertUTF8toUTF16((const UTF8 **)&sourceData, sourceEnd, &targetStart, targetEnd, strictConversion);
            if (cr != targetExhausted) break;
            
            cap <<= 1;
            PDSize size = targetStart - results;
            results = realloc(results, cap);
            targetStart = results + size;
            targetEnd = targetStart + cap;
        } 
        
        if (cr == conversionOK) {
            PDRelease(string->alt);
            string->alt = PDStringCreateBinary((char *)results, (targetEnd-results));
            string->alt->enc = PDStringEncodingUTF16;
            return string->alt;
        }
    }
    
    if (PDStringEncodingUTF32 == string->enc) {
        UTF32 *sourceData = (UTF32 *)&source->data[string->wrapped];
        UTF32 *sourceEnd = &sourceData[source->length/4];
        while (1) {
            cr = ConvertUTF32toUTF16((const UTF32 **)&sourceData, sourceEnd, &targetStart, targetEnd, strictConversion);
            if (cr != targetExhausted) break;
            
            cap <<= 1;
            PDSize size = targetStart - results;
            results = realloc(results, cap);
            targetStart = results + size;
            targetEnd = targetStart + cap;
        }
        
        if (cr == conversionOK) {
            PDRelease(string->alt);
            string->alt = PDStringCreateBinary((char *)results, (targetEnd-results));
            string->alt->enc = PDStringEncodingUTF16;
            return string->alt;
        }
    }
    
    free(results);
    
    // failure
    return NULL;
}

void PDStringDetermineEncoding(PDStringRef string)
{
    PDStringRef source = string;
    
    if (string->enc != PDStringEncodingDefault) return;
    
    // we need an escaped or binary representation of the string
    if (PDStringTypeBinary != string->type && PDStringTypeEscaped != string->type) {
        source = PDAutorelease(PDStringCreateBinaryFromString(string));
    }
    
    // we try to create UTF8 string; the string will have its encoding set on return
    PDStringRef u8string = PDUTF8String(string);
    
    if (u8string) return;
    
    // try UTF8
    UTF8 *sourceData = (UTF8 *)&string->data[string->wrapped];
    if (isLegalUTF8Sequence(sourceData, &sourceData[string->length])) {
        string->enc = PDStringEncodingUTF8;
        return;
    }
    
    PDWarn("Undefined string encoding encountered");
}

/* --------------------------------------------------------------------- */

static const uint16_t macRomanUTF8Seqs[256] = {
    // 0 1 2 3 4 5 6 7 8 9 A B C D E F  0 1 2 3 4 5 6 7 8 9 A B C D E F
    0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0xC4,  0xC5,  0xC7,  0xC9,  0xD1,  0xD6,  0xDC,  0xE1,  0xE0,  0xE2,  0xE4,  0xE3,  0xE5,  0xE7,  0xE9,  0xE8,
    0xEA,  0xEB,  0xED,  0xEC,  0xEE,  0xEF,  0xF1,  0xF3,  0xF2,  0xF4,  0xF6,  0xF5,  0xFA,  0xF9,  0xFB,  0xFC,
    0x2020,0xB0,  0xA2,  0xA3,  0xA7,  0x2022,0xB6,  0xDF,  0xAE,  0xA9,  0x2122,0xB4,  0xA8,  0x2260,0xC6,  0xD8,
    0x221E,0xB1,  0x2264,0x2265,0xA5,  0xB5,  0x2202,0x2211,0x220F,0x03C0,0x222B,0xAA,  0xBA,  0x03A9,0xE6,  0xF8,
    0xBF,  0xA1,  0xAC,  0x221A,0x0192,0x2248,0x2206,0xAB,  0xBB,  0x2026,0xA0,  0xC0,  0xC3,  0xD5,  0x0152,0x0153,
    0x2013,0x2014,0x201C,0x201C,0x2018,0x2019,0xF7,  0x25CA,0xFF,  0x0178,0x2044,0x20AC,0x2039,0x203A,0xFB01,0xFB02,
    0x2021,0xB7,  0x201A,0x201E,0x2030,0xC2,  0xCA,  0xC1,  0xCB,  0xC8,  0xCD,  0xCE,  0xCF,  0xCC,  0xD3,  0xD4,
    0xF8FF,0x00D2,0x00DA,0x00DB,0x00D9,0x0131,0x02C6,0x02DC,0xAF,  0x02D8,0x02D9,0x02DA,0xB8,  0x02DD,0x02DB,0x02C7,
};

//PDBool isProbablyMacRoman(const char **source, const char *sourceEnd)
//{
//    const char *str = *source;
//    if (str > sourceEnd) return false; // invalid input!
//    while (str < sourceEnd) {
//        if (0 == macRomanUTF8Seqs[str[0]]) return false;
//        str++;
//    }
//    return true;
//}

ConversionResult ConvertMacRomantoUTF8 (
                                        const unsigned char** sourceStart, const unsigned char* sourceEnd,
                                        UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags)
{
    ConversionResult result = conversionOK;
    const unsigned char* source = *sourceStart;
    UTF8* target = *targetStart;
    while (source < sourceEnd) {
        uint16_t utfval = macRomanUTF8Seqs[*source];
        
        if (! utfval) {
            result = sourceIllegal;
            break;
        }
        
        if (utfval == 1) utfval = *source;
        
        if (utfval > 0x7f) {
            // 11 or 16 bits of code point?
            if (utfval < 0x800) {
                // 11
                if (target >= targetEnd + 1) { result = targetExhausted; break; }
                *target++ = 0xC0 | (utfval >> 6);
            } else {
                // 16
                if (target >= targetEnd + 2) { result = targetExhausted; break; }
                *target++ = 0xE0 | (utfval >> 12);
                *target++ = 0x80 | ((utfval >> 6) & 0x3f);
            }
            *target++ = 0x80 | (utfval & 0x3f);
        } else {
            if (target >= targetEnd) { result = targetExhausted; break; }
            *target++ = (utfval & 0xff);
        }
        
        source++;
    }
    *sourceStart = source;
    *targetStart = target;
    if (conversionOK == result && target < targetEnd) *target++ = 0;
    
    return result;
}

/* --------------------------------------------------------------------- */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
 * left as-is for anyone who may want to do such conversion, which was
 * allowed in earlier algorithms.
 */
static const char trailingBytesForUTF8[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static const UTF32 offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL, 
    0x03C82080UL, 0xFA082080UL, 0x82082080UL };

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const UTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

/* --------------------------------------------------------------------- */

/* The interface converts a whole buffer to avoid function-call overhead.
 * Constants have been gathered. Loops & conditionals have been removed as
 * much as possible for efficiency, in favor of drop-through switches.
 * (See "Note A" at the bottom of the file for equivalent code.)
 * If your compiler supports it, the "isLegalUTF8" call can be turned
 * into an inline function.
 */


/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF32toUTF16 (
                                      const UTF32** sourceStart, const UTF32* sourceEnd, 
                                      UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF32* source = *sourceStart;
    UTF16* target = *targetStart;
    while (source < sourceEnd) {
        UTF32 ch;
        if (target >= targetEnd) {
            result = targetExhausted; break;
        }
        ch = *source++;
        if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
            /* UTF-16 surrogate values are illegal in UTF-32; 0xffff or 0xfffe are both reserved values */
            if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                if (flags == strictConversion) {
                    --source; /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                } else {
                    *target++ = UNI_REPLACEMENT_CHAR;
                }
            } else {
                *target++ = (UTF16)ch; /* normal case */
            }
        } else if (ch > UNI_MAX_LEGAL_UTF32) {
            if (flags == strictConversion) {
                result = sourceIllegal;
            } else {
                *target++ = UNI_REPLACEMENT_CHAR;
            }
        } else {
            /* target is a character in range 0xFFFF - 0x10FFFF. */
            if (target + 1 >= targetEnd) {
                --source; /* Back up source pointer! */
                result = targetExhausted; break;
            }
            ch -= halfBase;
            *target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
            *target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
        }
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF16toUTF32 (
                                      const UTF16** sourceStart, const UTF16* sourceEnd, 
                                      UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF16* source = *sourceStart;
    UTF32* target = *targetStart;
    UTF32 ch, ch2;
    while (source < sourceEnd) {
        const UTF16* oldSource = source; /*  In case we have to back up because of target overflow. */
        ch = *source++;
        /* If we have a surrogate pair, convert to UTF32 first. */
        if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
            /* If the 16 bits following the high surrogate are in the source buffer... */
            if (source < sourceEnd) {
                ch2 = *source;
                /* If it's a low surrogate, convert to UTF32. */
                if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
                    ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
                    + (ch2 - UNI_SUR_LOW_START) + halfBase;
                    ++source;
                } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
                    --source; /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                }
            } else { /* We don't have the 16 bits following the high surrogate. */
                --source; /* return to the high surrogate */
                result = sourceExhausted;
                break;
            }
        } else if (flags == strictConversion) {
            /* UTF-16 surrogate values are illegal in UTF-32 */
            if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
                --source; /* return to the illegal value itself */
                result = sourceIllegal;
                break;
            }
        }
        if (target >= targetEnd) {
            source = oldSource; /* Back up source pointer! */
            result = targetExhausted; break;
        }
        *target++ = ch;
    }
    *sourceStart = source;
    *targetStart = target;
#ifdef CVTUTF_DEBUG
    if (result == sourceIllegal) {
        fprintf(stderr, "ConvertUTF16toUTF32 illegal seq 0x%04x,%04x\n", ch, ch2);
        fflush(stderr);
    }
#endif
    return result;
}
ConversionResult ConvertUTF16toUTF8 (
                                     const UTF16** sourceStart, const UTF16* sourceEnd, 
                                     UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF16* source = *sourceStart;
    UTF8* target = *targetStart;
    while (source < sourceEnd) {
        UTF32 ch;
        unsigned short bytesToWrite = 0;
        const UTF32 byteMask = 0xBF;
        const UTF32 byteMark = 0x80; 
        const UTF16* oldSource = source; /* In case we have to back up because of target overflow. */
        ch = *source++;
        /* If we have a surrogate pair, convert to UTF32 first. */
        if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
            /* If the 16 bits following the high surrogate are in the source buffer... */
            if (source < sourceEnd) {
                UTF32 ch2 = *source;
                /* If it's a low surrogate, convert to UTF32. */
                if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
                    ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
                    + (ch2 - UNI_SUR_LOW_START) + halfBase;
                    ++source;
                } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
                    --source; /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                }
            } else { /* We don't have the 16 bits following the high surrogate. */
                --source; /* return to the high surrogate */
                result = sourceExhausted;
                break;
            }
        } else if (flags == strictConversion) {
            /* UTF-16 surrogate values are illegal in UTF-32 */
            if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
                --source; /* return to the illegal value itself */
                result = sourceIllegal;
                break;
            }
        }
        /* Figure out how many bytes the result will require */
        if (ch < (UTF32)0x80) {      bytesToWrite = 1;
        } else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
        } else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
        } else if (ch < (UTF32)0x110000) {  bytesToWrite = 4;
        } else {                            bytesToWrite = 3;
            ch = UNI_REPLACEMENT_CHAR;
        }
        
        target += bytesToWrite;
        if (target > targetEnd) {
            source = oldSource; /* Back up source pointer! */
            target -= bytesToWrite; result = targetExhausted; break;
        }
        switch (bytesToWrite) { /* note: everything falls through. */
            case 4: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 3: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 2: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 1: *--target =  (UTF8)(ch | firstByteMark[bytesToWrite]);
        }
        target += bytesToWrite;
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF32toUTF8 (
                                     const UTF32** sourceStart, const UTF32* sourceEnd, 
                                     UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF32* source = *sourceStart;
    UTF8* target = *targetStart;
    while (source < sourceEnd) {
        UTF32 ch;
        unsigned short bytesToWrite = 0;
        const UTF32 byteMask = 0xBF;
        const UTF32 byteMark = 0x80; 
        ch = *source++;
        if (flags == strictConversion ) {
            /* UTF-16 surrogate values are illegal in UTF-32 */
            if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                --source; /* return to the illegal value itself */
                result = sourceIllegal;
                break;
            }
        }
        /*
         * Figure out how many bytes the result will require. Turn any
         * illegally large UTF32 things (> Plane 17) into replacement chars.
         */
        if (ch < (UTF32)0x80) {      bytesToWrite = 1;
        } else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
        } else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
        } else if (ch <= UNI_MAX_LEGAL_UTF32) {  bytesToWrite = 4;
        } else {                            bytesToWrite = 3;
            ch = UNI_REPLACEMENT_CHAR;
            result = sourceIllegal;
        }
        
        target += bytesToWrite;
        if (target > targetEnd) {
            --source; /* Back up source pointer! */
            target -= bytesToWrite; result = targetExhausted; break;
        }
        switch (bytesToWrite) { /* note: everything falls through. */
            case 4: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 3: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 2: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
            case 1: *--target = (UTF8) (ch | firstByteMark[bytesToWrite]);
        }
        target += bytesToWrite;
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* --------------------------------------------------------------------- */

/*
 * Utility routine to tell whether a sequence of bytes is legal UTF-8.
 * This must be called with the length pre-determined by the first byte.
 * If not calling this from ConvertUTF8to*, then the length can be set by:
 *  length = trailingBytesForUTF8[*source]+1;
 * and the sequence is illegal right away if there aren't that many bytes
 * available.
 * If presented with a length > 4, this returns false.  The Unicode
 * definition of UTF-8 goes up to 4-byte sequences.
 */

static PDBool isLegalUTF8(const UTF8 *source, int length) {
    UTF8 a;
    const UTF8 *srcptr = source+length;
    switch (length) {
        default: return false;
            /* Everything else falls through when "true"... */
        case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
        case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
        case 2: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
            
            switch (*source) {
                    /* no fall-through in this inner switch */
                case 0xE0: if (a < 0xA0) return false; break;
                case 0xED: if (a > 0x9F) return false; break;
                case 0xF0: if (a < 0x90) return false; break;
                case 0xF4: if (a > 0x8F) return false; break;
                default:   if (a < 0x80) return false;
            }
            
        case 1: if (*source >= 0x80 && *source < 0xC2) return false;
    }
    if (*source > 0xF4) return false;
    return true;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return whether a UTF-8 sequence is legal or not.
 * This is not used here; it's just exported.
 */
PDBool isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd) {
    int length = trailingBytesForUTF8[*source]+1;
    if (length > sourceEnd - source) {
        return false;
    }
    return isLegalUTF8(source, length);
}

/* --------------------------------------------------------------------- */

static unsigned
findMaximalSubpartOfIllFormedUTF8Sequence(const UTF8 *source,
                                          const UTF8 *sourceEnd) {
    UTF8 b1, b2, b3;
    
    assert(!isLegalUTF8Sequence(source, sourceEnd));
    
    /*
     * Unicode 6.3.0, D93b:
     *
     *   Maximal subpart of an ill-formed subsequence: The longest code unit
     *   subsequence starting at an unconvertible offset that is either:
     *   a. the initial subsequence of a well-formed code unit sequence, or
     *   b. a subsequence of length one.
     */
    
    if (source == sourceEnd)
        return 0;
    
    /*
     * Perform case analysis.  See Unicode 6.3.0, Table 3-7. Well-Formed UTF-8
     * Byte Sequences.
     */
    
    b1 = *source;
    ++source;
    if (b1 >= 0xC2 && b1 <= 0xDF) {
        /*
         * First byte is valid, but we know that this code unit sequence is
         * invalid, so the maximal subpart has to end after the first byte.
         */
        return 1;
    }
    
    if (source == sourceEnd)
        return 1;
    
    b2 = *source;
    ++source;
    
    if (b1 == 0xE0) {
        return (b2 >= 0xA0 && b2 <= 0xBF) ? 2 : 1;
    }
    if (b1 >= 0xE1 && b1 <= 0xEC) {
        return (b2 >= 0x80 && b2 <= 0xBF) ? 2 : 1;
    }
    if (b1 == 0xED) {
        return (b2 >= 0x80 && b2 <= 0x9F) ? 2 : 1;
    }
    if (b1 >= 0xEE && b1 <= 0xEF) {
        return (b2 >= 0x80 && b2 <= 0xBF) ? 2 : 1;
    }
    if (b1 == 0xF0) {
        if (b2 >= 0x90 && b2 <= 0xBF) {
            if (source == sourceEnd)
                return 2;
            
            b3 = *source;
            return (b3 >= 0x80 && b3 <= 0xBF) ? 3 : 2;
        }
        return 1;
    }
    if (b1 >= 0xF1 && b1 <= 0xF3) {
        if (b2 >= 0x80 && b2 <= 0xBF) {
            if (source == sourceEnd)
                return 2;
            
            b3 = *source;
            return (b3 >= 0x80 && b3 <= 0xBF) ? 3 : 2;
        }
        return 1;
    }
    if (b1 == 0xF4) {
        if (b2 >= 0x80 && b2 <= 0x8F) {
            if (source == sourceEnd)
                return 2;
            
            b3 = *source;
            return (b3 >= 0x80 && b3 <= 0xBF) ? 3 : 2;
        }
        return 1;
    }
    
    assert((b1 >= 0x80 && b1 <= 0xC1) || b1 >= 0xF5);
    /*
     * There are no valid sequences that start with these bytes.  Maximal subpart
     * is defined to have length 1 in these cases.
     */
    return 1;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return the total number of bytes in a codepoint
 * represented in UTF-8, given the value of the first byte.
 */
unsigned getNumBytesForUTF8(UTF8 first) {
    return trailingBytesForUTF8[first] + 1;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return whether a UTF-8 string is legal or not.
 * This is not used here; it's just exported.
 */
PDBool isLegalUTF8String(const UTF8 **source, const UTF8 *sourceEnd) {
    while (*source != sourceEnd) {
        int length = trailingBytesForUTF8[**source] + 1;
        if (length > sourceEnd - *source || !isLegalUTF8(*source, length))
            return false;
        *source += length;
    }
    return true;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF8toUTF16 (
                                     const UTF8** sourceStart, const UTF8* sourceEnd, 
                                     UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags) {
    ConversionResult result = conversionOK;
    const UTF8* source = *sourceStart;
    UTF16* target = *targetStart;
    while (source < sourceEnd) {
        UTF32 ch = 0;
        unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
        if (extraBytesToRead >= sourceEnd - source) {
            result = sourceExhausted; break;
        }
        /* Do this check whether lenient or strict */
        if (!isLegalUTF8(source, extraBytesToRead+1)) {
            result = sourceIllegal;
            break;
        }
        /*
         * The cases all fall through. See "Note A" below.
         */
        switch (extraBytesToRead) {
            case 5: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
            case 4: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
            case 3: ch += *source++; ch <<= 6;
            case 2: ch += *source++; ch <<= 6;
            case 1: ch += *source++; ch <<= 6;
            case 0: ch += *source++;
        }
        ch -= offsetsFromUTF8[extraBytesToRead];
        
        if (target >= targetEnd) {
            source -= (extraBytesToRead+1); /* Back up source pointer! */
            result = targetExhausted; break;
        }
        if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
            /* UTF-16 surrogate values are illegal in UTF-32 */
            if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                if (flags == strictConversion) {
                    source -= (extraBytesToRead+1); /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                } else {
                    *target++ = UNI_REPLACEMENT_CHAR;
                }
            } else {
                *target++ = (UTF16)ch; /* normal case */
            }
        } else if (ch > UNI_MAX_UTF16) {
            if (flags == strictConversion) {
                result = sourceIllegal;
                source -= (extraBytesToRead+1); /* return to the start */
                break; /* Bail out; shouldn't continue */
            } else {
                *target++ = UNI_REPLACEMENT_CHAR;
            }
        } else {
            /* target is a character in range 0xFFFF - 0x10FFFF. */
            if (target + 1 >= targetEnd) {
                source -= (extraBytesToRead+1); /* Back up source pointer! */
                result = targetExhausted; break;
            }
            ch -= halfBase;
            *target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
            *target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
        }
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

/* --------------------------------------------------------------------- */

static ConversionResult ConvertUTF8toUTF32Impl(
                                               const UTF8** sourceStart, const UTF8* sourceEnd, 
                                               UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags,
                                               PDBool InputIsPartial) {
    ConversionResult result = conversionOK;
    const UTF8* source = *sourceStart;
    UTF32* target = *targetStart;
    while (source < sourceEnd) {
        UTF32 ch = 0;
        unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
        if (extraBytesToRead >= sourceEnd - source) {
            if (flags == strictConversion || InputIsPartial) {
                result = sourceExhausted;
                break;
            } else {
                result = sourceIllegal;
                
                /*
                 * Replace the maximal subpart of ill-formed sequence with
                 * replacement character.
                 */
                source += findMaximalSubpartOfIllFormedUTF8Sequence(source,
                                                                    sourceEnd);
                *target++ = UNI_REPLACEMENT_CHAR;
                continue;
            }
        }
        if (target >= targetEnd) {
            result = targetExhausted; break;
        }
        
        /* Do this check whether lenient or strict */
        if (!isLegalUTF8(source, extraBytesToRead+1)) {
            result = sourceIllegal;
            if (flags == strictConversion) {
                /* Abort conversion. */
                break;
            } else {
                /*
                 * Replace the maximal subpart of ill-formed sequence with
                 * replacement character.
                 */
                source += findMaximalSubpartOfIllFormedUTF8Sequence(source,
                                                                    sourceEnd);
                *target++ = UNI_REPLACEMENT_CHAR;
                continue;
            }
        }
        /*
         * The cases all fall through. See "Note A" below.
         */
        switch (extraBytesToRead) {
            case 5: ch += *source++; ch <<= 6;
            case 4: ch += *source++; ch <<= 6;
            case 3: ch += *source++; ch <<= 6;
            case 2: ch += *source++; ch <<= 6;
            case 1: ch += *source++; ch <<= 6;
            case 0: ch += *source++;
        }
        ch -= offsetsFromUTF8[extraBytesToRead];
        
        if (ch <= UNI_MAX_LEGAL_UTF32) {
            /*
             * UTF-16 surrogate values are illegal in UTF-32, and anything
             * over Plane 17 (> 0x10FFFF) is illegal.
             */
            if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                if (flags == strictConversion) {
                    source -= (extraBytesToRead+1); /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                } else {
                    *target++ = UNI_REPLACEMENT_CHAR;
                }
            } else {
                *target++ = ch;
            }
        } else { /* i.e., ch > UNI_MAX_LEGAL_UTF32 */
            result = sourceIllegal;
            *target++ = UNI_REPLACEMENT_CHAR;
        }
    }
    *sourceStart = source;
    *targetStart = target;
    return result;
}

ConversionResult ConvertUTF8toUTF32Partial(const UTF8 **sourceStart,
                                           const UTF8 *sourceEnd,
                                           UTF32 **targetStart,
                                           UTF32 *targetEnd,
                                           ConversionFlags flags) {
    return ConvertUTF8toUTF32Impl(sourceStart, sourceEnd, targetStart, targetEnd,
                                  flags, /*InputIsPartial=*/true);
}

ConversionResult ConvertUTF8toUTF32(const UTF8 **sourceStart,
                                    const UTF8 *sourceEnd, UTF32 **targetStart,
                                    UTF32 *targetEnd, ConversionFlags flags) {
    return ConvertUTF8toUTF32Impl(sourceStart, sourceEnd, targetStart, targetEnd,
                                  flags, /*InputIsPartial=*/false);
}

/* ---------------------------------------------------------------------
 
 Note A.
 The fall-through switches in UTF-8 reading code save a
 temp variable, some decrements & conditionals.  The switches
 are equivalent to the following loop:
 {
 int tmpBytesToRead = extraBytesToRead+1;
 do {
 ch += *source++;
 --tmpBytesToRead;
 if (tmpBytesToRead) ch <<= 6;
 } while (tmpBytesToRead > 0);
 }
 In UTF-8 writing code, the switches on "bytesToWrite" are
 similarly unrolled loops.
 
 --------------------------------------------------------------------- */
#endif

