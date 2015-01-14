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

PDBool iconv_unicode_mb_to_uc_fb_called = false;
PDBool iconv_unicode_uc_to_mb_fb_called = false;

void pdstring_iconv_unicode_mb_to_uc_fallback(const char* inbuf, size_t inbufsize,
                                              void (*write_replacement) (const unsigned int *buf, size_t buflen,
                                                                         void* callback_arg),
                                              void* callback_arg,
                                              void* data)
{
    iconv_unicode_mb_to_uc_fb_called = true;
}

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
