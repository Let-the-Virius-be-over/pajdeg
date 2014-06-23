//
// pd_pdf_implementation.h
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
 @defgroup PDPDF_GRP Pajdeg PDF Implementation
 
 The PDF implementation in Pajdeg makes use of states and operators to set up how the scanner should interpret the source data. 
 
 States in Pajdeg are constant definitions for given states in the input file, and operators are chained together actions to take based on some given input to a state.
 
 There are two main root states in the implementation: pdfRoot, for parsing PDF content as normal, and xrefSeeker, for locating the starting byte offset of the primary XREF table. There are a few additional root states, stringStream (currently unused) and arbStream (used to parse object streams).
 
 These states are made up of operators which chain states together to form the complete specification implementation.
 
 @see STATE_GRP
 @see OPERATOR_GRP
 \n
 @{
 */

/**
 @file pd_pdf_implementation.h
 */

#ifndef INCLUDED_pd_pdf_implementation_h
#define INCLUDED_pd_pdf_implementation_h

#include "PDState.h"

extern const char * PD_META;            ///< %%meta entry
extern const char * PD_STRING;          ///< A string
extern const char * PD_NUMBER;          ///< A number, boolean, or null value
extern const char * PD_NAME;            ///< /Name entry
extern const char * PD_OBJ;             ///< an object (with definition)
extern const char * PD_REF;             ///< an object reference
extern const char * PD_HEXSTR;          ///< a \<abcdef1234567890\> hex string
extern const char * PD_ENTRIES;         ///< entries in a dictionary or array
extern const char * PD_DICT;            ///< a dictionary
extern const char * PD_DE;              ///< a dictionary entry
extern const char * PD_ARRAY;           ///< an array
extern const char * PD_AE;              ///< an array entry
extern const char * PD_XREF;            ///< an XREF
extern const char * PD_STARTXREF;       ///< the "startxref" symbol
extern const char * PD_ENDSTREAM;       ///< the "endstream" symbol


/**
 Convenience macro for comparing a given value to one of the PD_ entries.
 */
#define PDIdentifies(key, pdtype) ((PDID)key == &pdtype)

/**
 Retain (constructing, if no previous retains were made) the PDF implementation.
 */
extern void pd_pdf_implementation_use();

/**
 Release (destructing, if no other retains remain) the PDF implementation.
 */
extern void pd_pdf_implementation_discard();

/**
 Retain the PDF conversion table.
 */
extern void pd_pdf_conversion_use();

/**
 Release the PDF conversion table.
 */
extern void pd_pdf_conversion_discard();

/**
 The root PDF state.
 */
extern PDStateRef pdfRoot;

/**
 The PDF array state, useful for iterating over stuff.
 */
//extern PDStateRef pdfArrayRoot;

/**
 The root XREF seeking state.
 */
extern PDStateRef xrefSeeker;

/**
 String stream state. Instead of attempting to generate complex objects using definitions, this state 
 always returns the next symbol (whitespace separated) as a string.
 */
extern PDStateRef stringStream;

/**
 Arbitrary object stream (e.g. object stream post-header) state.
 */
extern PDStateRef arbStream;

/**
 Convert stack representation of complex object into PDF string.
 
 @warning Destroys target stack unless preserve flag is set.
 
 @todo Change name to reflect that returned value is allocated and must be freed.
 
 @see pd_stack
 @see pd_stack_set_global_preserve_flag
 */
extern char *PDStringFromComplex(pd_stack *complex);

/**
 *  Convert stack representation of complex object into an appropriate object.
 *
 *  @note Returned strings are returned as PDString instances.
 *  @note Returned entry must be PDRelease()'d
 *
 *  @param complex Stack representation 
 *
 *  @return An appropriate object. Use PDResolve() to determine its type.
 */
extern void *PDInstanceCreateFromComplex(pd_stack *complex);

/**
 Determine object type from identifier.
 
 @param identifier One of the PD_ identifiers.
 */
extern PDObjectType PDObjectTypeFromIdentifier(PDID identifier);

/**
 A generic null deallocator.
 */
extern PDDeallocator PDDeallocatorNull;


#endif

/** @} */
