//
//  PDXTable.h
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/27/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#ifndef INCLUDED_PDXTable_h
#define INCLUDED_PDXTable_h

#include "PDDefines.h"

typedef enum {
    PDXTableFormatText = 0,     // PDF-1.4 and below, using 20-byte leading zero space delimited entries
    PDXTableFormatBinary = 1,   // PDX-1.5 and forward, using variable width object stream format
} PDXFormat;

enum {
    PDXTypeFreed    = 0,    // Freed object ('f' for text format entries)
    PDXTypeUsed     = 1,    // Used object ('n' for text format entries)
    PDXTypeComp     = 2,    // Compressed object (no text format equivalent)
};
typedef unsigned char PDXType;

struct PDXTable {
    struct {
        PDXType type;   // T entry, 1 byte
        union {
            PDSize offs;    // Byte offset (for used objects) 
            PDSize next;    // Next free object ID (for freed objects)
            PDSize stream;  // The object ID of the stream in which this (compressed) object is located
        };
        union {
            PDInteger genid; // Generation number (for freed
        }
    } entries;
    char       *xrefs;  // xref entries stored as a chunk of memory
    
    PDXFormat   format; // format
    PDParserRef parser; // parser
    PDSize      cap;    // capacity
    PDSize      count;  // # of objects
    PDSize      pos;    // byte-wise position in the PDF where the xref (and subsequent trailer) begins; reaching this point means the xref cease to apply
};

#define PDXTableOffsetForID(xtable, id) (

#define PDXOffset(pdx)      fast_mutative_atol(pdx, 10)
#define PDXGenId(pdx)       fast_mutative_atol(&pdx[11], 5)
#define PDXUsed(pdx)        (pdx[17] == 'n')
#define PDXSetUsed(pdx,u)    pdx[17] = (u ? 'n' : 'f')
#define PDXUndefined(pdx)   (pdx[10] != ' ') // we use the space between ob and gen as indicator for whether the PDX was defined (everything is zeroed due to realloc)
#define PDXSetUndefined(pdx) pdx[10] = 0;

extern PDBool PDXTableFetchXRefs(PDXTableRef xtable);

#endif
