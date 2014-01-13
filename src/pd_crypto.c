//
// pd_crypto.c
//
// Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "pd_crypto.h"
#include "pd_internal.h"
#include "pd_dict.h"
#include "pd_md5.h"

#ifdef PD_SUPPORT_CRYPTO

/*
struct pd_crypto {
    // common values
    char     *filter;           ///< filter name
    char     *subfilter;        ///< sub-filter name
    PDInteger version;          ///< algorithm version (V key in PDFs)
    PDInteger length;           ///< length of the encryption key, in bits; must be a multiple of 8 in the range 40 - 128; default = 40
    
    // standard security handler 
    PDInteger revision;         ///< revision ("R") of algorithm: 2 if version < 2 and perms have no 3 or greater values, 3 if version is 2 or 3, or P has rev 3 stuff, 4 if version = 4
    char     *owner;            ///< owner string ("O"), 32-byte string based on owner and user passwords, used to compute encryption key and determining whether a valid owner password was entered
    char     *user;             ///< user string ("U"), 32-byte string based on user password, used in determining whether to prompt the user for a password and whether given password was a valid user or owner password
    PDInteger privs;            ///< privileges (see Table 3.20 in PDF spec v 1.7, p. 123-124)
};
*/

static const unsigned char pd_crypto_pad [] = {
    0x28, 0xBF, 0x4E, 0x5E, 
    0x4E, 0x75, 0x8A, 0x41, 
    0x64, 0x00, 0x4E, 0x56, 
    0xFF, 0xFA, 0x01, 0x08, 
    0x2E, 0x2E, 0x00, 0xB6, 
    0xD0, 0x68, 0x3E, 0x80, 
    0x2F, 0x0C, 0xA9, 0xFE, 
    0x64, 0x53, 0x69, 0x7A
};

#define strdup_null(v) (v ? strdup(v) : NULL)

static unsigned char S[256];

void pd_crypto_rc4(pd_crypto crypto, const char *key, int keylen, char *data, long datalen)
{
    // to avoid allocating S every time, this function may not be multithreaded; if issues arise, ensure that bg thread calls do not result in rc4 mangling itself
    // to check if this is the case, put a static int rc4iter initialized to 0, ensure it's 0 on all calls, and increment it on start and decrement at end
    long l;
    int i, j;
    for (i = 0; i < 256; i++) 
        S[i] = i;
    j = 0;
    unsigned char t;
    for (i = 0; i < 256; i++) {
        j = (j + S[i] + key[i % keylen]) & 0xff;
        t = S[i]; S[i] = S[j]; S[j] = t;
    }
    
    i = 0;
    j = 0;
    l = 0;
    while (l < datalen) {
        i = (i + 1) & 0xff;
        j = (j + S[i]) & 0xff;
        t = S[i]; S[i] = S[j]; S[j] = t;
        data[l] = data[l] ^ S[(S[i] + S[j]) & 0xff];
        l++;
    }
}

void pd_crypto_generate_enckey(pd_crypto crypto, const char *user_pass)
{
    // concat user and padding into buffer, and crop it down to 32 bytes.
    unsigned char buf[1024];
    memcpy(buf, user_pass, strlen(user_pass));
    memcpy(&buf[strlen(user_pass)], pd_crypto_pad, 32);
    
    pd_md5_ctx md5ctx;
    pd_md5_init(&md5ctx);
    pd_md5_update(&md5ctx, buf, 32);
    
    // pass owner hash
    pd_md5_update(&md5ctx, crypto->owner.d, crypto->owner.l);
    
    // append privs as 4-byte int, LSB first
    buf[0] = crypto->privs & 0xff;
    buf[1] = (crypto->privs >> 8) & 0xff;
    buf[2] = (crypto->privs >> 16) & 0xff;
    buf[3] = (crypto->privs >> 24) & 0xff;
    pd_md5_update(&md5ctx, buf, 4);
    
    // pass file identifier to md5
    pd_md5_update(&md5ctx, crypto->identifier.d, crypto->identifier.l);
    
    if (crypto->revision > 3 && !crypto->encryptMetadata) {
        buf[0] = buf[1] = buf[2] = buf[3] = 0xff;
        pd_md5_update(&md5ctx, buf, 4);
    }
    
    pd_md5_final(buf, &md5ctx);
    
    int eklen = crypto->length/8;
    if (crypto->revision > 2) {
        for (int i = 0; i < 50; i++) {
            pd_md5(buf, eklen, buf);
        }
    }
    
    // the first length/8 bytes are the encryption key
    unsigned char *enckey = malloc(1 + eklen);
    memcpy(enckey, buf, eklen);
    enckey[eklen] = 0;
    crypto->enckey.d = enckey;
    crypto->enckey.l = crypto->length/8;
}

void pd_crypto_destroy(pd_crypto crypto)
{
    if (crypto->filter) free(crypto->filter);
    if (crypto->subfilter) free(crypto->subfilter);
    if (crypto->owner.d) free(crypto->owner.d);
    if (crypto->user.d) free(crypto->user.d);
    if (crypto->identifier.d) free(crypto->identifier.d);
    if (crypto->enckey.d) free(crypto->enckey.d);
    free(crypto);
}

static char *HEX_TAB = NULL;

pd_crypto_param pd_crypto_decode_pdf_hex(const char *hexstr)
{
    pd_crypto_param cp;
    cp.d = NULL;
    cp.l = 0;
    if (hexstr == NULL) return cp;
    
    if (HEX_TAB == NULL) {
        HEX_TAB = malloc(256 * sizeof(char));
        for (int i = 0; i < 256; i++) HEX_TAB[i] = -1;
        for (int i = 0; i < 10; i++) HEX_TAB['0' + i] = i;
        for (int i = 10; i < 16; i++) HEX_TAB['a' + i - 10] = HEX_TAB['A' + i - 10] = i;
    }
    int rlen = 0;
    int len = strlen(hexstr);
    cp.d = malloc(len);
    int i = 0;
    
    while (i < len && -1 == HEX_TAB[hexstr[i]]) i++;
    while (i + 1 < len && -1 != HEX_TAB[hexstr[i]]) {
        cp.d[rlen++] = HEX_TAB[hexstr[i]] * 16 + HEX_TAB[hexstr[i+1]];
        i += 2;
    }
    cp.d = realloc(cp.d, rlen+1);
    cp.l = rlen;
    return cp;
}

PDInteger pd_crypto_unescape(char *str)
{
    PDBool esc = false;
    int si = 0;
    int escseq;
    int ibeg = 0;
    int iend = strlen(str);
    if (str[0] == '(' && str[iend-1] == ')') {
        ibeg = 1;
        iend --;
    }
    for (int i = ibeg; i < iend; i++) {
        if (str[i] == '\\' && ! esc) {
            esc = true;
        } else {
            if (esc) {
                if (str[i] >= '0' && str[i] <= '9') {
                    str[si] = 0;
                    for (escseq = 0; escseq < 3 && str[i] >= '0' && str[i] <= '9'; escseq++, i++)
                        str[si] = (str[si] << 4) + (str[i] - '0');
                    i--;
                } else switch (str[i]) {
                    case '\n':
                    case '\r':
                        si--; // ignore newline by nulling the si++ below
                        break;
                    case 't': str[si] = '\t'; break;
                    case 'r': str[si] = '\r'; break;
                    case 'n': str[si] = '\n'; break;
                    case 'b': str[si] = '\b'; break;
                    case 'f': str[si] = '\f'; break;
                    case '0': str[si] = '\0'; break;
                    case 'a': str[si] = '\a'; break;
                    case 'v': str[si] = '\v'; break;
                    case 'e': str[si] = '\e'; break;

                        // a number of things are simply escaped escapings (\, (, ))
                    case '\\':
                    case '(':
                    case ')': 
                        str[si] = str[i]; break;
                        
                    default: 
                        PDWarn("unknown escape sequence: encryption may break: \\%c\n", str[i]);
                        str[si] = str[i]; 
                        break;
                }
                esc = false;
            } else {
                str[si] = str[i];
            }
            si++;
        }
    }
    str[si] = 0;
    return si;
}

pd_crypto_param pd_crypto_decode_param(const char *param)
{
    // param can be a (string with \escapes) or a <hex string>
    pd_crypto_param cp;
    cp.d = NULL;
    cp.l = 0;
    if (param == NULL || strlen(param) == 0) {
        return cp;
    }
    
    if (param[0] == '(') {
        if (param[strlen(param)-1] != ')') {
            // special case; sometimes encryption keys contain \0; invalid of course, as these must be escaped, but life is life
            param = &param[1];
        }
        cp.d = (unsigned char *)strdup(param);
        cp.l = pd_crypto_unescape((char *)cp.d);
        return cp;
    }

    return pd_crypto_decode_pdf_hex(param);
}

pd_crypto pd_crypto_create(pd_dict trailerDict, pd_dict options)
{
    pd_crypto crypto = malloc(sizeof(struct pd_crypto));
    
    const char *fid = pd_dict_get(trailerDict, "ID");
    crypto->identifier = pd_crypto_decode_pdf_hex(fid);
    
    // read from dict
    crypto->filter = strdup_null(pd_dict_get(options, "Filter"));
    crypto->subfilter = strdup_null(pd_dict_get(options, "SubFilter"));
    crypto->version = PDIntegerFromString(pd_dict_get(options, "V"));
    crypto->length = PDIntegerFromString(pd_dict_get(options, "Length"));
    crypto->encryptMetadata = pd_dict_get(options, "EncryptMetadata") && 0 == strcmp(pd_dict_get(options, "EncryptMetadata"), "true");
    
    crypto->revision = PDIntegerFromString(pd_dict_get(options, "R"));
    crypto->owner = pd_crypto_decode_param(pd_dict_get(options, "O"));
    crypto->user = pd_crypto_decode_param(pd_dict_get(options, "U"));
    crypto->privs = PDIntegerFromString(pd_dict_get(options, "P"));
    
    // fix defaults where appropriate
    if (crypto->version == 0) crypto->version = 1; // we do not support the default as it is undocumented and no longer supported by the official specification
    if (crypto->length < 40 || crypto->length > 128) crypto->length = 40;
    
    crypto->enckey = (pd_crypto_param){NULL, 0};
    
    return crypto;
}

PDInteger pd_crypto_escape(char **dst, const char *src, PDInteger srcLen)
{
    char *str = malloc(3 + srcLen * 4); // worst case, we get \000 for every single character
    str[0] = '(';
    int si = 1;
    for (int i = 0; i < srcLen; i++) {
        str[si] = '\\';
        switch (src[i]) {
            case '\t': str[++si] = 't'; break;
            case '\b': str[++si] = 'b'; break;
            case '\r': str[++si] = 'r'; break;
            case '\n': str[++si] = 'n'; break;
            case '\f': str[++si] = 'f'; break;
            case '\a': str[++si] = 'a'; break;
                
            case '\\': 
            case '(':
            case ')':
                str[++si] = src[i];
                break;
                
            case 0: 
                str[++si] = '0';
                str[++si] = '0';
                str[++si] = '0';
                break;
                
            default:
                str[si] = src[i];
                break;
        }
        si++;
    }
    str[si++] = ')';
    str[si] = 0;
    *dst = malloc(si+1);
    memcpy(*dst, str, si+1);
    free(str);
    return si;
}

void pd_crypto_convert(pd_crypto crypto, PDInteger obid, PDInteger genid, char *data, PDInteger len)
{
//1. Obtain the object number and generation number from the object identifier of the string or stream to be encrypted (see Section 3.2.9, “Indirect Objects”). If the string is a direct object, use the identifier of the indirect object containing it.
    
    // we let the caller deal with (1)
    
//2. Treating the object number and generation number as binary integers, extend the original n-byte encryption key to n + 5 bytes by appending the low-order 3 bytes of the object number and the low-order 2 bytes of the generation number in that order, low-order byte first. (n is 5 unless the value of V in the encryption dictionary is greater than 1, in which case n is the value of Length divided by 8.)
    
    if (crypto->enckey.d == NULL) 
        pd_crypto_generate_enckey(crypto, "");
    
    PDInteger klen = crypto->length/8;
    char *key = malloc(klen > 26 ? klen + 5 : 32);
    memcpy(key, crypto->enckey.d, crypto->enckey.l);
    key[klen++] = obid & 0xff;
    key[klen++] = (obid>>8) & 0xff;
    key[klen++] = (obid>>16) & 0xff;
    key[klen++] = genid & 0xff;
    key[klen++] = (genid>>8) & 0xff;
    
    
//If using the AES algorithm, extend the encryption key an additional 4 bytes by adding the value "sAlT", which corresponds to the hexadecimal values 0x73, 0x41, 0x6C, 0x54. (This addition is done for backward compatibility and is not intended to provide additional security.)

    /// @todo: determine if, and do above
    
//3. Initialize the MD5 hash function and pass the result of step 2 as input to this function.
    
    pd_md5((unsigned char *)key, klen, (unsigned char *)key);

//￼￼￼￼4. Use the first (n + 5) bytes, up to a maximum of 16, of the output from the MD5 hash as the key for the RC4 or AES symmetric key algorithms, along with the string or stream data to be encrypted.
//If using the AES algorithm, the Cipher Block Chaining (CBC) mode, which requires an initialization vector, is used. The block size parameter is set to 16 bytes, and the initialization vector is a 16-byte random number that is stored as the first 16 bytes of the encrypted stream or string.
//The output is the encrypted data to be stored in the PDF file.
    
    if (klen > 16) klen = 16;
    key[klen] = 0; // truncate at min(16, n + 5)
    pd_crypto_rc4(crypto, key, klen, data, len);
}

PDInteger pd_crypto_encrypt(pd_crypto crypto, PDInteger obid, PDInteger genid, char **dst, char *src, PDInteger len)
{
    // We want to crop off ()s if found
    if (len > 0 && src[0] == '(' && src[len-1] == ')') {
        src = &src[1];
        len -= 2;
    }
    
    pd_crypto_convert(crypto, obid, genid, src, len);
    return pd_crypto_escape(dst, src, len);
}

void pd_crypto_decrypt(pd_crypto crypto, PDInteger obid, PDInteger genid, char *data)
{
    PDInteger len = pd_crypto_unescape(data);
    pd_crypto_convert(crypto, obid, genid, data, len);
}

#endif
