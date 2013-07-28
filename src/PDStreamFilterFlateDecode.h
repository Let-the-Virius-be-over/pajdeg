//
//  PDStreamFilterFlateDecode.h
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/26/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

/**
 @defgroup FILTER_FLATE_GRP Flate Filter
 
 @brief Flate Decode (compression/decompression) stream filter
 
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
extern PDStreamFilterRef PDStreamFilterFlateDecodeCompressCreate(PDStackRef options);

/**
 Set up stream filter for FlateDecode decompression.
 */
extern PDStreamFilterRef PDStreamFilterFlateDecodeDecompressCreate(PDStackRef options);

/**
 Set up a stream filter for FlateDecode based on inputEnd boolean. 
 */
extern PDStreamFilterRef PDStreamFilterFlateDecodeConstructor(PDBool inputEnd, PDStackRef options);

#endif

#endif
