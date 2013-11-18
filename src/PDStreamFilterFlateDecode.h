//
// PDStreamFilterFlateDecode.h
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
 @file PDStreamFilterFlateDecode.h
 
 @ingroup PDSTREAMFILTERFLATEDECODE
 
 @defgroup PDSTREAMFILTERFLATEDECODE PDStreamFilterFlateDecode
 
 @brief Flate Decode (compression/decompression) stream filter
 
 @ingroup PDINTERNAL

 PDF streams are often compressed using FlateDecode. Normally this isn't a problem unless the user needs to change or insert stream content in a stream that requires compression, or, more importantly, when a PDF is using the 1.5+ object stream feature for the XREF table, and is not providing a fallback option for 1.4- readers.
 
 @warning This filter depends on the zlib library. It is enabed through the PD_SUPPORT_ZLIB define in PDDefines.h. Disabling this feature means zlib is not required for Pajdeg, but means that some PDFs (1.5 with no fallback) will not be parsable via Pajdeg!
 
 @see PDDefines.h
 
 @{
 */

#ifndef INCLUDED_PDStreamFilterFlateDecode_h
#define INCLUDED_PDStreamFilterFlateDecode_h

#include "PDStreamFilter.h"

#ifdef PD_SUPPORT_ZLIB

/**
 Set up a stream filter for FlateDecode compression.
 */
extern PDStreamFilterRef PDStreamFilterFlateDecodeCompressCreate(pd_stack options);

/**
 Set up stream filter for FlateDecode decompression.
 */
extern PDStreamFilterRef PDStreamFilterFlateDecodeDecompressCreate(pd_stack options);

/**
 Set up a stream filter for FlateDecode based on inputEnd boolean. 
 */
extern PDStreamFilterRef PDStreamFilterFlateDecodeConstructor(PDBool inputEnd, pd_stack options);

#endif

#endif

/** @} */

