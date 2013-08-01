//
//  PDObjectStream.h
//  ICViewer
//
//  Created by Karl-Johan Alm on 8/1/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

/**
 @file PDObjectStream.h PDF object stream header file.
 
 @ingroup PDOBJECTSTREAM
 
 @defgroup PDOBJECTSTREAM PDObjectStream
 
 @brief A PDF object stream.
 
 @ingroup PDUSER
 
 Normally, objects are located directly inside of the PDF, but an alternative way is to keep objects as so called object streams (Chapter 3.4.6 of PDF specification v 1.7, p. 100). 
 
 @{
 */

#ifndef ICViewer_PDObjectStream_h
#define ICViewer_PDObjectStream_h

#include "PDDefines.h"

/**
 Get the object with the given ID out of the object stream. 
 
 @param obstm The object stream.
 @param obid The id of the object to fetch.
 @return The object, or NULL if not found.
 */
extern PDObjectRef PDObjectStreamGetObjectByID(PDObjectStreamRef obstm, PDInteger obid);

extern PDObjectRef PDObjectStreamGetObjectAtIndex(PDObjectStreamRef obstm, PDInteger index);

#endif

/** @} */

/** @} */
