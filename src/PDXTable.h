//
//  PDXTable.h
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/27/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#ifndef INCLUDED_PDXTable_h
#define INCLUDED_PDXTable_h

#include <sys/types.h>
#include "PDDefines.h"

typedef enum {
    PDXTableFormatText   = 0, // PDF-1.4 and below, using 20-byte leading zero space delimited entries
    PDXTableFormatBinary = 1, // PDX-1.5 and forward, using variable width object stream format
} PDXFormat;

enum {
    PDXTypeFreed    = 0,    // Freed object ('f' for text format entries)
    PDXTypeUsed     = 1,    // Used object ('n' for text format entries)
    PDXTypeComp     = 2,    // Compressed object (no text format equivalent)
};
typedef unsigned char PDXType;
typedef u_int32_t PDXOffsetType;
typedef unsigned char PDXGenType;

#define PDXTypeSize     1   // note: this must not be changed
#define PDXOffsSize     4   // note: changing this requires updating PDXRefGet/SetOffsetForID
#define PDXGenSize      1   // note: this must not be changed

#define PDXTypeAlign    0
#define PDXOffsAlign    PDXTypeSize
#define PDXGenAlign     (PDXOffsAlign + PDXOffsSize)
#define PDXWidth        (PDXGenAlign + PDXGenSize)

#define PDXWEntry       "[ 1 4 1 ]" // why can't I make this use the quoted value of the #defines above...?

struct PDXTable {
    char       *xrefs;      // xref entries stored as a chunk of memory
    
    PDXFormat   format;     // format
    PDBool      linearized; // if set, unexpected XREF entries in the PDF are silently ignored by the parser
    PDParserRef parser;     // parser
    PDSize      cap;        // capacity
    PDSize      count;      // # of objects
    PDSize      pos;        // byte-wise position in the PDF where the xref (and subsequent trailer) begins; reaching this point means the xref cease to apply
    
    PDXTableRef prev;       // previous (older) table
    PDXTableRef next;       // next (newer) table
};

extern PDXOffsetType PDXRefGetOffsetForID(char *xrefs, PDInteger obid);
extern void PDXRefSetOffsetForID(char *xrefs, PDInteger obid, PDXOffsetType offset);

#define PDXRefTypeForID(xrefs, id)       (PDXType*)&((xrefs)[id*PDXWidth])
//#define PDXRefOffsetForID(xrefs, id)     (PDXOffsetType*)&((xrefs)[PDXOffsAlign+id*PDXWidth])
#define PDXRefGenForID(xrefs, id)        (PDXGenType*)&((xrefs)[PDXGenAlign+id*PDXWidth])

#define PDXTableTypeForID(xtable, id)       PDXRefTypeForID(xtable->xrefs, id)
#define PDXTableOffsetForID(xtable, id)     PDXRefGetOffsetForID(xtable->xrefs, id)
#define PDXTableGenForID(xtable, id)        PDXRefGenForID(xtable->xrefs, id)

#define PDXTableIsIDFree(xtable, id)        (PDXTypeFreed == *PDXTableTypeForID(xtable, id))
#define PDXTableSetIDFree(xtable, id, isf)  *PDXTableTypeForID(xtable, id) = (isf ? PDXTypeFreed : PDXTypeUsed)
#define PDXTableSetOffset(xtable, id, offs) PDXRefSetOffsetForID(xtable->xrefs, id, offs)
#define PDXTableSetGen(xtable, id, gen)     *PDXTableGenForID(xtable, id) = gen

// deprecated (plain text format versions)
/*
#define PDXOffset(pdx)      fast_mutative_atol(pdx, 10)
#define PDXGenId(pdx)       fast_mutative_atol(&pdx[11], 5)
#define PDXUsed(pdx)        (pdx[17] == 'n')
#define PDXSetUsed(pdx,u)    pdx[17] = (u ? 'n' : 'f')
#define PDXUndefined(pdx)   (pdx[10] != ' ') // we use the space between ob and gen as indicator for whether the PDX was defined (everything is zeroed due to realloc)
#define PDXSetUndefined(pdx) pdx[10] = 0;
*/

extern PDBool PDXTableFetchXRefs(PDParserRef parser);

extern void PDXTableDestroy(PDXTableRef xtable);

/**
 @note This is only called for old-style plaintext XREF tables; new style XREF tables appear as regular objects with streams
 */
extern PDBool PDXTablePassoverXRefEntry(PDParserRef parser, PDStackRef stack, PDBool includeTrailer);

extern PDBool PDXTableInsert(PDParserRef parser);

#endif
