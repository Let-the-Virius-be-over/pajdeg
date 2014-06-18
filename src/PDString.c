//
// PDString.c
//
// Copyright (c) 2012 - 2014 Karl-Johan Alm (http://github.com/kallewoof)
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

#include "Pajdeg.h"
#include "PDString.h"
#include "pd_stack.h"
#include "pd_crypto.h"

#include "pd_internal.h"

// Private declarations

char *PDStringWrappedValue(const char *string, PDSize len, char left, char right);          ///< "..."                  -> "L...R"
char *PDStringUnwrappedValue(const char *string, PDSize len);                               ///< "(...)", "<...>", .... -> "..."
char *PDStringHexToBinary(char *string, PDSize len, PDBool wrapped, PDSize *outLength);     ///< "abc123"               -> 01101010
char *PDStringHexToEscaped(char *string, PDSize len, PDBool wrapped);                       ///< "abc123"               -> "foo\123"
char *PDStringEscapedToHex(char *string, PDSize len, PDBool wrapped);                       ///< "foo\123"              -> "abc123"
char *PDStringEscapedToBinary(char *string, PDSize len, PDBool wrapped, PDSize *outLength); ///< "foo\123"              -> 01101010
char *PDStringBinaryToHex(char *string, PDSize len, PDBool wrapped);                        ///< 01101010               -> "abc123"
char *PDStringBinaryToEscaped(char *string, PDSize len, PDBool wrapped);                    ///< 01101010               -> "foo\123"

// Public

void PDStringDestroy(PDStringRef string)
{
    PDRelease(string->alt);
    free(string->data);
}

#ifdef DEBUG
// This method exists to test the ownership of strings. It only catches accidental passes of constant strings.
void PDStringVerifyOwnership(char *string, PDSize len)
{
    char s = string[0];
    char e = string[len];
    string[0] = 0;
    string[len] = 0;
    string[0] = 255;
    string[len] = 255;
    string[0] = s;
    string[len] = e;
}
#endif

PDStringRef PDStringCreate(char *string)
{
#ifdef DEBUG
    PDStringVerifyOwnership(string, strlen(string));
#endif
    
    PDStringRef res = PDAllocTyped(PDInstanceTypeString, sizeof(struct PDString), PDStringDestroy, false);
    res->data = string;
    res->alt = NULL;
    res->length = strlen(string);
    res->type = PDStringTypeRegular;
    res->wrapped = (string[0] == '(' && string[res->length-1] == ')');
#ifdef PD_SUPPORT_CRYPTO
    res->ci = NULL;
#endif
    return res;
}

PDStringRef PDStringCreateBinary(char *data, PDSize length)
{
#ifdef DEBUG
    PDStringVerifyOwnership(data, length);
#endif

    PDStringRef res = PDAllocTyped(PDInstanceTypeString, sizeof(struct PDString), PDStringDestroy, false);
    res->data = data;
    res->alt = NULL;
    res->length = length;
    res->type = PDStringTypeBinary;
    res->wrapped = false;
#ifdef PD_SUPPORT_CRYPTO
    res->ci = NULL;
#endif
    return res;
}

PDStringRef PDStringCreateWithHexString(char *hex)
{
#ifdef DEBUG
    PDStringVerifyOwnership(hex, strlen(hex));
#endif
    PDStringRef res = PDAllocTyped(PDInstanceTypeString, sizeof(struct PDString), PDStringDestroy, false);
    res->data = hex;
    res->alt = NULL;
    res->length = strlen(hex);
    res->type = PDStringTypeHex;
    res->wrapped = (hex[0] == '<' && hex[res->length-1] == '>');
#ifdef PD_SUPPORT_CRYPTO
    res->ci = NULL;
#endif
    return res;
}

PDStringRef PDStringCreateFromStringWithType(PDStringRef string, PDStringType type, PDBool wrap)
{
    if (string->type == type && (string->type == PDStringTypeBinary || string->wrapped == wrap))
        return PDRetain(string);
    
    if (string->alt && string->alt->type == type && (string->alt->type == PDStringTypeBinary || string->alt->wrapped == wrap))
        return PDRetain(string->alt);
    
    char *res;
    char *buf;
    PDSize len;
    PDStringRef result;
    switch (type) {
        case PDStringTypeRegular:
            result = PDStringCreate(strdup(PDStringEscapedValue(string, wrap)));
            break;

        case PDStringTypeHex:
            result = PDStringCreateWithHexString(strdup(PDStringHexValue(string, wrap)));
            break;
            
        default:
            res = PDStringBinaryValue(string, &len);
            buf = malloc(len);
            memcpy(buf, res, len);
            result = PDStringCreateBinary(buf, len);
    }
    
#ifdef PD_SUPPORT_CRYPTO
    if (string->ci) PDStringAttachCryptoInstance(result, string->ci, string->encrypted);
#endif
    return result;
}

void PDStringForceWrappedState(PDStringRef string, PDBool wrapped)
{
    PDAssert(string->type == PDStringTypeRegular); // crash = attempt to set wrapped state for a string whose wrapping is never ambiguous (only regular/escaped strings are)
    string->wrapped = wrapped;
}

char *PDStringEscapedValue(PDStringRef string, PDBool wrap)
{
    if (string == NULL) return NULL;
    
    // see if we have what is asked for already
    if (string->type == PDStringTypeRegular && string->wrapped == wrap) {
        return string->data;
    } else if (string->alt && string->alt->type == PDStringTypeRegular && string->alt->wrapped == wrap) {
        return string->alt->data;
    } 
    
    // we don't, so set up alternative; we use the PDString which offers the easiest conversion, which is
    //  escaped strings, then binary strings, then hex strings
    PDStringRef source = ((string->alt && (string->alt->type == PDStringTypeRegular || string->type == PDStringTypeHex))
                          ? string->alt
                          : string);
    
    char *data;
    if (source->type == PDStringTypeBinary)
        data = PDStringBinaryToEscaped(source->data, source->length, wrap);
    else if (source->type == PDStringTypeHex)
        data = PDStringHexToEscaped(source->data, source->length, wrap);
    else if (wrap) 
        data = PDStringWrappedValue(source->data, source->length, '(', ')');
    else 
        data = PDStringUnwrappedValue(source->data, source->length);

    PDRelease(string->alt);
    string->alt = PDStringCreate(data);
#ifdef PD_SUPPORT_CRYPTO
    if (string->ci) PDStringAttachCryptoInstance(string->alt, string->ci, string->encrypted);
#endif
    return data;
}

char *PDStringBinaryValue(PDStringRef string, PDSize *outLength)
{
    if (string == NULL) return NULL;
    
    // see if we have what is asked for already
    if (string->type == PDStringTypeBinary) {
        return string->data;
    } else if (string->alt && string->alt->type == PDStringTypeBinary) {
        return string->alt->data;
    } 
    
    // we don't, so set up alternative; we use the PDString which offers the easiest conversion, which is
    //  hex strings, then regular strings
    PDStringRef source = ((string->alt && string->type == PDStringTypeRegular)
                          ? string->alt
                          : string);
    
    char *data;
    PDSize len;
    if (source->type == PDStringTypeRegular)
        data = PDStringEscapedToBinary(source->data, source->length, source->wrapped, &len);
    else
        data = PDStringHexToBinary(source->data, source->length, source->wrapped, &len);
    
    *outLength = len;
    
    PDRelease(string->alt);
    string->alt = PDStringCreateBinary(data, len);
#ifdef PD_SUPPORT_CRYPTO
    if (string->ci) PDStringAttachCryptoInstance(string->alt, string->ci, string->encrypted);
#endif
    return data;
}

char *PDStringHexValue(PDStringRef string, PDBool wrap)
{
    if (string == NULL) return NULL;
    
    // see if we have what is asked for already
    if (string->type == PDStringTypeHex && string->wrapped == wrap) {
        return string->data;
    } else if (string->alt && string->alt->type == PDStringTypeHex && string->alt->wrapped == wrap) {
        return string->alt->data;
    } 
    
    // we don't, so set up alternative; we use the PDString which offers the easiest conversion, which is
    //  hex strings, then binary strings, then regular strings
    PDStringRef source = ((string->alt && (string->alt->type == PDStringTypeHex || string->type == PDStringTypeRegular))
                          ? string->alt
                          : string);
    
    char *data;
    if (source->type == PDStringTypeBinary)
        data = strdup(PDStringBinaryToHex(source->data, source->length, wrap));
    else if (source->type == PDStringTypeRegular)
        data = strdup(PDStringEscapedToHex(source->data, source->length, wrap));
    else if (wrap) 
        data = PDStringWrappedValue(source->data, source->length, '<', '>');
    else 
        data = PDStringUnwrappedValue(source->data, source->length);
    
    PDRelease(string->alt);
    string->alt = PDStringCreateWithHexString(data);
#ifdef PD_SUPPORT_CRYPTO
    if (string->ci) PDStringAttachCryptoInstance(string->alt, string->ci, string->encrypted);
#endif
    return data;
}

// Private

char *PDStringWrappedValue(const char *string, PDSize len, char left, char right)
{
    char *res = malloc(len + 3);
    res[0] = left;
    strcpy(&res[1], string);
    res[len+1] = right;
    res[len+2] = 0;
    return res;
}

char *PDStringUnwrappedValue(const char *string, PDSize len)
{
    return strndup(&string[1], len-2);
}

char *PDStringHexToBinary(char *string, PDSize len, PDBool wrapped, PDSize *outLength)
{
    char *csr = string + (wrapped);
    PDSize ix = len - (wrapped<<1);
    PDSize rescap = 2 + ix/2;
    PDSize reslen = 0;
    char *res = malloc(rescap);
    PDSize i;
    
    for (i = 0; i < ix; i += 2) {
//        PDInteger a = PDOperatorSymbolGlobHex[csr[i]];
//        PDInteger b = PDOperatorSymbolGlobHex[csr[i+1]];
//        PDInteger c = (a << 4) + b;
//        PDInteger d = (PDOperatorSymbolGlobHex[csr[i]] << 4) + PDOperatorSymbolGlobHex[csr[i+1]];
        res[reslen++] = (PDOperatorSymbolGlobHex[csr[i]] << 4) + PDOperatorSymbolGlobHex[csr[i+1]];
        
        if (reslen == rescap) {
            // in theory we should never end up here; if we do, we just make sure we don't crash
            PDError("unexpectedly exhausted result cap in PDStringHexToBinary for input \"%s\"", string);
            rescap += 10 + (ix - i);
            res = realloc(res, rescap);
        }
    }
    
    res[reslen] = 0;
    *outLength = reslen;
    return res;
}

char *PDStringEscapedToBinary(char *string, PDSize len, PDBool wrapped, PDSize *outLength)
{
    const char *str = string + (wrapped);
    PDSize ix = len - (wrapped<<1);
    char *res = malloc(len);
    PDBool esc = false;
    PDSize si = 0;
    int escseq;
    for (int i = 0; i < ix; i++) {
        if (str[i] == '\\' && ! esc) {
            esc = true;
        } else {
            if (esc) {
                if (str[i] >= '0' && str[i] <= '9') {
                    res[si] = 0;
                    for (escseq = 0; escseq < 3 && str[i] >= '0' && str[i] <= '9'; escseq++, i++)
                        res[si] = (res[si] << 4) + (res[i] - '0');
                    i--;
                } else switch (str[i]) {
                    case '\n':
                    case '\r':
                        si--; // ignore newline by nulling the si++ below
                        break;
                    case 't': res[si] = '\t'; break;
                    case 'r': res[si] = '\r'; break;
                    case 'n': res[si] = '\n'; break;
                    case 'b': res[si] = '\b'; break;
                    case 'f': res[si] = '\f'; break;
                    case '0': res[si] = '\0'; break;
                    case 'a': res[si] = '\a'; break;
                    case 'v': res[si] = '\v'; break;
                    case 'e': res[si] = '\e'; break;
                        
                        // a number of things are simply escaped escapings (\, (, ))
                    case '%':
                    case '\\':
                    case '(':
                    case ')': 
                        res[si] = str[i]; break;
                        
                    default: 
                        PDError("unknown escape sequence: \\%c\n", str[i]);
                        res[si] = str[i]; 
                        break;
                }
                esc = false;
            } else {
                res[si] = str[i];
            }
            si++;
        }
    }
    res[si] = 0;
    *outLength = si;
    return res;
}

char *PDStringBinaryToHex(char *string, PDSize len, PDBool wrapped)
{
    PDSize rescap = 1 + (len << 1) + (wrapped << 1);
    if (rescap < 10) rescap = 10;
    PDSize reslen = 0;
    char *res = malloc(rescap);
    char ch;
    PDSize i;
    
    if (wrapped) res[reslen++] = '<';
    
    for (i = 0; i < len; i++) {
        ch = string[i];
        res[reslen++] = PDOperatorSymbolGlobDehex[ch >> 4];
        res[reslen++] = PDOperatorSymbolGlobDehex[ch & 0xf];
    }

    if (wrapped) res[reslen++] = '>';

    res[reslen] = 0;
    return res;
}

char *PDStringBinaryToEscaped(char *string, PDSize len, PDBool wrapped) 
{
    PDSize rescap = (len << 1) + (wrapped << 1);
    if (rescap < 10) rescap = 10;
    PDSize reslen = 0;
    char *res = malloc(rescap);
    char ch, e;
    PDSize i;
    
    if (wrapped) res[reslen++] = '(';
    
    for (i = 0; i < len; i++) {
        ch = string[i];
        e = PDOperatorSymbolGlobEscaping[ch];
        switch (e) {
            case 0: // needs escaping using code
                reslen += sprintf(&res[reslen], "\\%s%d", ch < 10 ? "00" : ch < 100 ? "0" : "", ch);
                break;
            case '1': // needs no escaping
                res[reslen++] = ch;
                break;
            default: // can be escaped with a charcode
                res[reslen++] = '\\';
                res[reslen++] = e;
        }
        if (rescap - reslen < 5) {
            rescap += 10 + (len - i) * 2;
            res = realloc(res, rescap);
        }
    }
    
    if (wrapped) res[reslen++] = ')';
    
    res[reslen] = 0;
    return res;
}

char *PDStringHexToEscaped(char *string, PDSize len, PDBool wrapped)
{
    // currently we do this by going to binary format first
    char *tmp = PDStringHexToBinary(string, len, wrapped, &len);
    char *res = PDStringBinaryToEscaped(tmp, len, wrapped);
    free(tmp);
    return res;
}

char *PDStringEscapedToHex(char *string, PDSize len, PDBool wrapped)
{
    // currently we do this by going to binary format first
    char *tmp = PDStringEscapedToBinary(string, len, wrapped, &len);
    char *res = PDStringBinaryToHex(tmp, len, wrapped);
    free(tmp);
    return res;
}

PDInteger PDStringPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap)
{
    PDInstancePrinterInit(PDStringRef, 5 + i->length, 5 + i->length);

#ifdef PD_SUPPORT_CRYPTO
    if (i->ci && i->ci->crypto && ! i->encrypted) {
        PDStringRef enc = PDStringCreateEncrypted(i);
        offs = PDStringPrinter(enc, buf, offs, cap);
        PDRelease(enc);
        return offs;
    }
#endif
    
    char *bv = *buf;
    strcpy(&bv[offs], i->data);
    return offs + i->length;
//    
//    PDBool ownsStr = false;
//    char *str = i->data;
//    PDSize len = i->length;
//    if (i->type == PDStringTypeBinary) {
//        ownsStr = true;
//        str = PDStringBinaryToEscaped(str, len, true);
//        len = strlen(str);
//        PDInstancePrinterRequire(1 + len, 1 + len);
//    } else if (! i->wrapped) {
//        ownsStr = true;
//        str = PDStringWrappedValue(str, len, i->type == PDStringTypeHex ? '<' : '(', i->type == PDStringTypeHex ? '>' : ')');
//        len += 2;
//    }
//    
//    char *bv = *buf;
//    strcpy(&bv[offs], str);
//    if (ownsStr) free(str);
//    return offs + len;
}

PDBool PDStringEqualsCString(PDStringRef string, const char *cString)
{
    PDBool result;
    
    PDStringRef compat = PDStringCreateFromStringWithType(string, PDStringTypeRegular, false);
    
    result = 0 == strncmp(cString, string->data, string->length);
    
    PDRelease(compat);
    
    return result;
}

PDBool PDStringEqualsString(PDStringRef string, PDStringRef string2)
{
    PDBool releaseString2 = false;
    if (string->type != string2->type) {
        // if alt string fits, we use that
        if (string->alt && string->alt->type == string2->type) 
            return PDStringEqualsString(string->alt, string2);
        
        // we don't want to convert TO hex format, ever, unless both are hex already
        if (string->type == PDStringTypeHex)
            return PDStringEqualsString(string2, string);
        
        string2 = PDStringCreateFromStringWithType(string2, string->type, false);
        releaseString2 = true;
    }
    
    PDSize len1 = string->length - (string->wrapped<<1);
    PDSize len2 = string2->length - (string2->wrapped<<1);
    
    PDBool result = false;
    
    if (len1 == len2) {
        PDInteger start1 = string->wrapped;
        PDInteger start2 = string2->wrapped;
        
        result = 0 == strncmp(&string->data[start1], &string2->data[start2], len1);
    }    
    
    if (releaseString2) PDRelease(string2);
    
    return result;
}

#pragma mark - Crypto

#ifdef PD_SUPPORT_CRYPTO

PDBool PDStringIsEncrypted(PDStringRef string)
{
    return string->ci && string->encrypted;
}

void PDStringAttachCrypto(PDStringRef string, pd_crypto crypto, PDInteger objectID, PDInteger genNumber, PDBool encrypted)
{
    PDCryptoInstanceRef ci = PDCryptoInstanceCreate(crypto, objectID, genNumber, NULL);
    string->ci = ci;
    string->encrypted = encrypted;
}

void PDStringAttachCryptoInstance(PDStringRef string, PDCryptoInstanceRef ci, PDBool encrypted)
{
    string->ci = PDRetain(ci);
    string->encrypted = encrypted;
}

PDStringRef PDStringCreateEncrypted(PDStringRef string)
{
    if (NULL == string->ci || string->encrypted) return PDRetain(string);
    
    PDSize len;
    char *str = PDStringBinaryValue(string, &len);
    char *dst;
    len = pd_crypto_encrypt(string->ci->crypto, string->ci->obid, string->ci->genid, &dst, str, len);
    free(str);
    PDStringRef encrypted = PDStringCreateBinary(dst, len);
    PDStringAttachCryptoInstance(encrypted, string->ci, true);
    return encrypted;
}

PDStringRef PDStringCreateDecrypted(PDStringRef string)
{
    if (NULL == string->ci || ! string->encrypted) return PDRetain(string);
    
    PDSize len;
    char *data = PDStringBinaryValue(string, &len);
    pd_crypto_convert(string->ci->crypto, string->ci->obid, string->ci->genid, data, len);
    PDStringRef decrypted = PDStringCreate(data);
    PDStringAttachCryptoInstance(decrypted, string->ci, false);
    return decrypted;
}

#endif
