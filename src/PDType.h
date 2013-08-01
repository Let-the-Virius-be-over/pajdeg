//
//  PDType.h
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/31/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#ifndef ICViewer_PDType_h
#define ICViewer_PDType_h

#include "PDDefines.h"

extern void PDRelease(void *pajdegObject);
extern void *PDRetain(void *pajdegObject);
extern void *PDAutorelease(void *pajdegObject);

#endif
