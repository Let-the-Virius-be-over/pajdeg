//
// pd_crypto.h
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


/**
 @file pd_crypto.h Cryptography header file.
 
 @ingroup pd_crypto
 
 @defgroup pd_crypto pd_crypto
 
 @brief Cryptography related functionality to deal with encrypted PDF content.
 
 @ingroup PDALGO
 
 The pd_crypto object.
 
 @{
 */

#ifndef INCLUDED_PD_CRYPTO_H
#define INCLUDED_PD_CRYPTO_H

#include <sys/types.h>
#include "PDDefines.h"

#ifdef PD_SUPPORT_CRYPTO

/**
 Create crypto object with given configuration. 
 
 @param trailerDict The pd_dict of the Trailer dictionary of the given PDF. Needed to obtain the /ID key.
 @param options The pd_dict of the Encrypt dictionary of the given PDF.
 @return Instance with given options or NULL if unsupported.
 */
extern pd_crypto pd_crypto_create(pd_dict trailerDict, pd_dict options);

/**
 Destroy crypto object, freeing up resources.
 */
extern void pd_crypto_destroy(pd_crypto crypto);

/**
 Unescape a PDF string which may optionally be wrapped in parentheses. The result is not wrapped in parentheses. The string is unescaped in-place and NUL-terminated.
 
 Strings, in particular encrypted strings, are stored using escaping to prevent null termination in the middle of strings and PDF misinterpretations and other nastiness.
 
 Escaping is done to control chars, such as \r, \n, \t, \b, and unreadable ascii characters using \octal(s) (1, 2 or 3).
 
 @param str The string.
 @return The length of the unescaped string.
 */
extern PDInteger pd_crypto_unescape(char *str);

/**
 Escape a string. The result will be wrapped in parentheses.
 
 Strings, in particular encrypted strings, are stored using escaping to prevent null termination in the middle of strings and PDF misinterpretations and other nastiness.
 
 Escaping is done to control chars, such as \r, \n, \t, \b, and unreadable ascii characters using \octal(s) (1, 2 or 3).
 
 @param dst Pointer to destination string. Should not be pre-allocated.
 @param src String to escape.
 @param srcLen Length of string.
 @return The length of the escaped string.
 */
extern PDInteger pd_crypto_escape(char **dst, const char *src, PDInteger srcLen);

/**
 Supply a user password. 
 
 @param crypto The crypto object.
 @param password The user password.
 @return true if the password was valid, false if not
 */
extern PDBool pd_crypto_authenticate_user(pd_crypto crypto, const char *password);

/**
 Encrypt the value of src of length len and store the value in dst, escaped and parenthesized.
 
 @param crypto Crypto instance.
 @param obid The object ID of the object whose content is being encrypted.
 @param genid The generation number of the object whose content is being encrypted.
 @param dst Pointer to char buffer into which results will be stored. Should not be pre-allocated.
 @param src The data to encrypt. The content will be in-place encrypted but escaped results go into *dst.
 @param len Length of data in bytes.
 @return Length of encrypted string, including parentheses and escaping.
 */
extern PDInteger pd_crypto_encrypt(pd_crypto crypto, PDInteger obid, PDInteger genid, char **dst, char *src, PDInteger len);

/**
 Decrypt, unescape and NUL-terminate the value of data in-place.
 
 @param crypto Crypto instance.
 @param obid The object ID of the object whose content is being decrypted.
 @param genid The generation number of the object whose content is being decrypted.
 @param data The data to decrypt. The content will be in-place replaced with the new data.
 */
extern void pd_crypto_decrypt(pd_crypto crypto, PDInteger obid, PDInteger genid, char *data);

/**
 Convert data of length len owned by object obid with generation number genid to/from encrypted version.
 
 This is the "low level" function used by the above two methods to encrypt/decrypt content. This version does not 
 escape/unescape or add/remove parentheses, which the above ones do. This version is used directly for streams
 which aren't escaped.
 
 @param crypto Crypto instance.
 @param obid Object ID of owning object.
 @param genid Generation number of owning object.
 @param data Data to convert.
 @param len Length of data.
 */
extern void pd_crypto_convert(pd_crypto crypto, PDInteger obid, PDInteger genid, char *data, PDInteger len);

#endif

#endif

/** @} */