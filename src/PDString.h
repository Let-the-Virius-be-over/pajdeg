//
// PDString.h
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

/**
 @file PDString.h String wrapper
 
 @ingroup PDSTRING
 
 @defgroup PDSTRING PDString
 
 @brief A wrapper around PDF strings.

 PDString objects exist to provide a unified way to maintain and convert between different string types. In particular, it is responsible for providing a means to escape and unescape content, as well as convert to and from hex strings (written as <abc123> in PDFs).
 
 @{
 */

#ifndef INCLUDED_PDSTRING_H
#define INCLUDED_PDSTRING_H

#include "PDDefines.h"

/**
 *  Create a PDString from an existing, escaped string. 
 *
 *  @note Ownership of the string is taken, and the string is freed when the PDString object is released.
 *  @note Use PDStringCreateBinary for strings that need to be escaped, even if they're proper NUL-terminated strings.
 *
 *  @param string NUL-terminated, escaped string optionally wrapped in parentheses.
 *
 *  @return New PDString instance for string
 */
extern PDStringRef PDStringCreate(char *string);

/**
 *  Create a PDString from an existing, unescaped or binary string of the given length.
 *
 *  @note Ownership of the data is taken, and the data is freed when the PDString object is released.
 *
 *  @param data   Data containing binary string
 *  @param length Length of the data, in bytes
 *
 *  @return New PDString instance for data
 */
extern PDStringRef PDStringCreateBinary(char *data, PDSize length);

/**
 *  Create a PDString from an existing hex string.
 *
 *  @note Ownership of the data is taken, and the data is freed when the PDString object is released.
 *
 *  @param hex Hex string, optionally wrapped in less/greater-than signs.
 *
 *  @return New PDString instance for hex
 */
extern PDStringRef PDStringCreateWithHexString(char *hex);

/**
 *  Create a PDString from an existing PDString instance, with an explicit type.
 *  In effect, this creates a new PDString object whose type matches the given type. 
 *  This method is recommended for converting between types, unless the C string value is explicitly desired.
 *
 *  @note If string is already of the given type, it is retained and returned as is.
 *
 *  @param string PDString instance used in transformation
 *  @param type   String type to transform to
 *
 *  @return A PDString whose value is identical to that of string, and whose type is the given type
 */
extern PDStringRef PDStringCreateFromStringWithType(PDStringRef string, PDStringType type);

/**
 *  Generate a C string containing the escaped contents of string and return it. 
 *  If wrap is set, the string is wrapped in parentheses.
 *
 *  @note The returned object must be freed.
 *
 *  @param string PDString instance
 *  @param wrap   Whether or not the returned string should be enclosed in parentheses
 *
 *  @return Escaped NUL-terminated C string
 */
extern char *PDStringEscapedValue(PDStringRef string, PDBool wrap);

/**
 *  Generate the binary value of string, writing its length to the PDSize pointed to by outLength and returning the 
 *  binary value.
 *
 *  @note The returned value must be freed.
 *
 *  @param string    PDString instance
 *  @param outLength Pointer to PDSize object into which the length of the returned binary data is to be written. May be NULL.
 *
 *  @return C string pointer to binary data
 */
extern char *PDStringBinaryValue(PDStringRef string, PDSize *outLength);

/**
 *  Generate a hex string based on the value of string, returning it.
 *
 *  @note The returned object must be freed.
 *
 *  @param string PDString instance
 *  @param wrap   Whether or not the returned string should be enclosed in less/greater-than signs
 *
 *  @return Hex string
 */
extern char *PDStringHexValue(PDStringRef string, PDBool wrap);

#ifdef PD_SUPPORT_CRYPTO

/**
 *  Attach a crypto object to the string, and associate the array with a specific object. 
 *  The encrypted flag is used to determine if the PDString is encrypted or not in its current state.
 *
 *  @param string    PDString instance whose crypto object is to be set
 *  @param crypto    pd_crypto object
 *  @param objectID  The object ID of the owning object
 *  @param genNumber Generation number of the owning object
 *  @param encrypted Whether string is encrypted or not, currently
 */
extern void PDStringAttachCrypto(PDStringRef string, pd_crypto crypto, PDInteger objectID, PDInteger genNumber, PDBool encrypted);

/**
 *  Create a PDString by encrypting string.
 *
 *  @note If string is already encrypted, or if no crypto has been attached, the string is retained and returned as is.
 *
 *  @param string PDString instance to encrypt
 *
 *  @return A retained encrypted PDStringRef for string
 */
extern PDStringRef PDStringCreateEncrypted(PDStringRef string);

/**
 *  Create a PDString by decrypting string.
 *
 *  @note If string is not encrypted, it is retained and returned as is.
 *
 *  @param string PDString instance
 *
 *  @return A retained decrypted PDStringRef for string
 */
extern PDStringRef PDStringCreateDecrypted(PDStringRef string);

extern PDInteger PDStringPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap);

#endif // PD_SUPPORT_CRYPTO

#endif // INCLUDED_PDSTRING_H

/** @} */
