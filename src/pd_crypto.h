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
 Supply a user password. 
 
 @param crypto The crypto object.
 @param password The user password.
 @return true if the password was valid, false if not
 */
extern PDBool pd_crypto_authenticate_user(pd_crypto crypto, const char *password);

/**
 Encrypt the value of strIn of length lenIn and store the value in strOut and the length of the value in lenOut.
 
 @param crypto Crypto instance.
 @param obid The object ID of the object whose content is being encrypted.
 @param genid The generation number of the object whose content is being encrypted.
 @param data The data to encrypt. The content will be in-place replaced with the new data.
 @param len Length of data in bytes.
 */
extern void pd_crypto_encrypt(pd_crypto crypto, PDInteger obid, PDInteger genid, char *data, PDInteger len);

/**
 Decrypt the value of strIn of length lenIn and store the value in strOut and the length of the value in lenOut.
 
 @param crypto Crypto instance.
 @param obid The object ID of the object whose content is being decrypted.
 @param genid The generation number of the object whose content is being decrypted.
 @param data The data to decrypt. The content will be in-place replaced with the new data.
 @param len Length of data in bytes.
 */
extern void pd_crypto_decrypt(pd_crypto crypto, PDInteger obid, PDInteger genid, char *data, PDInteger len);

#endif

/** @} */
