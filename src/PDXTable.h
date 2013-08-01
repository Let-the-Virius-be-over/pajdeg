//
//  PDXTable.h
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/27/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

/**
 @file PDXTable.h PDF XRef table header.
 
 @defgroup PDXTABLE PDXTable
 
 @brief PDF XRef (cross reference) table.
 
 @ingroup PDINTERNAL
 
 @{
 */


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

/**
 PDF XRef (cross reference) table
 */
struct PDXTable {
    char       *xrefs;      ///< XRef entries stored as a chunk of memory
    
    PDXFormat   format;     ///< Original format of this entry, which can be text (PDF 1.4-) or binary (PDF 1.5+). Internally, there is no difference to how the data is maintained, but Pajdeg will use the same format used in the original in its own output.
    PDBool      linearized; ///< If set, unexpected XREF entries in the PDF are silently ignored by the parser.
    PDSize      cap;        ///< Capacity of the table's xrefs buffer, in entries.
    PDSize      count;      ///< Number of objects held by the XRef.
    PDSize      pos;        ///< Byte-wise position in the PDF where the XRef (and subsequent trailer, if text format) begins; reaching this point means the XRef ceases to apply
    
    PDXTableRef prev;       ///< previous (older) table (mostly debug related)
    PDXTableRef next;       ///< next (newer) table (mostly debug related)
};

extern PDXOffsetType PDXRefGetOffsetForID(char *xrefs, PDInteger obid);
extern void PDXRefSetOffsetForID(char *xrefs, PDInteger obid, PDXOffsetType offset);

#define PDXRefGetTypeForID(xrefs, id)        (PDXType)((xrefs)[id*PDXWidth])
#define PDXRefSetTypeForID(xrefs, id, t)    *(PDXType*)&((xrefs)[id*PDXWidth]) = t
#define PDXRefGetGenForID(xrefs, id)         (PDXGenType)((xrefs)[PDXGenAlign+id*PDXWidth])
#define PDXRefSetGenForID(xrefs, id, gen)   *(PDXGenType*)&((xrefs)[PDXGenAlign+id*PDXWidth]) = gen

#define PDXTableGetTypeForID(xtable, id)       PDXRefGetTypeForID(xtable->xrefs, id)
#define PDXTableGetOffsetForID(xtable, id)     PDXRefGetOffsetForID(xtable->xrefs, id)
#define PDXTableGetGenForID(xtable, id)        PDXRefGetGenForID(xtable->xrefs, id)

#define PDXTableIsIDFree(xtable, id)        (PDXTypeFreed == PDXTableGetTypeForID(xtable, id))
#define PDXTableSetOffset(xtable, id, offs) PDXRefSetOffsetForID(xtable->xrefs, id, offs)
/*#define PDXTableSetIDFree(xtable, id, isf)  *PDXTableTypeForID(xtable, id) = (isf ? PDXTypeFreed : PDXTypeUsed)
#define PDXTableSetGen(xtable, id, gen)     *PDXTableGenForID(xtable, id) = gen*/

extern PDBool PDXTableFetchXRefs(PDParserRef parser);

extern void PDXTableDestroy(PDXTableRef xtable);

/**
 @note This is only called for old-style plaintext XREF tables; new style XREF tables appear as regular objects with streams
 */
extern PDBool PDXTablePassoverXRefEntry(PDParserRef parser, PDStackRef stack, PDBool includeTrailer);

extern PDBool PDXTableInsert(PDParserRef parser);

#endif

/** @} */
