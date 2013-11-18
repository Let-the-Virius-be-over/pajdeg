//
// PDOperator.h
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
 @file PDOperator.h Operator header file.
 
 @ingroup PDOPERATOR

 @defgroup PDOPERATOR PDOperator
 
 @ingroup PDSCANNER_CONCEPT
 
 @brief An operator for a given symbol or outcome in a state.
 
 Operators are either tied to some symbol in a state, or to one of the fallback types (numeric, delimiter, and the all-accepting). They define some form of action that should occur, and may optionally include a "next" operator, which is executed directly following the completion of the current operator's action.
 
 @note If an operator pushes a state, its action will be regarded as ongoing until the pushed state is poppsed again.
 
 @{
 */

#ifndef INCLUDED_PDOperator_h
#define INCLUDED_PDOperator_h

#include "PDDefines.h"

#define PDOperatorSymbolGlobRegular     0   ///< PDF regular character
#define PDOperatorSymbolGlobWhitespace  1   ///< PDF whitespace character
#define PDOperatorSymbolGlobDelimiter   4   ///< PDF delimiter character
#define PDOperatorSymbolExtNumeric      8   ///< Numeric value
#define PDOperatorSymbolExtFake         16  ///< "Fake" symbol
#define PDOperatorSymbolExtEOB          32  ///< End of buffer (usually synonymous with end of file) symbol

/**
 Global symbol table, a 256 byte mapping of ASCII characters to their corresponding PDF symbol type as defined in the PDF 
 specification (v 1.7). 
 */
extern char *PDOperatorSymbolGlob;

/**
 Set up the global symbol table. Multiple calls will have a retaining effect and must have balancing PDOperatorSymbolGlobClear() calls to avoid leaks.
 */
extern void PDOperatorSymbolGlobSetup();

/**
 Clear the global symbol table. If multiple PDOperatorSymbolGlobSetup() calls were made, the table will remain.
 */
extern void PDOperatorSymbolGlobClear();

/**
 Define the given symbol. Definitions detected are delimiters, numeric (including real) values, and (regular) symbols.
 */
extern char PDOperatorSymbolGlobDefine(char *str);

/**
 Create a PDOperatorRef chain based on a definition in the form of NULL terminated arrays of operator types followed by (if any) arguments of corresponding types.
 
 @param defs Definitions, null terminated.
 */
extern PDOperatorRef PDOperatorCreateFromDefinition(const void **defs);

/**
 Compile all states referenced by the operator.
 
 @see PDStateCompile
 
 @param op The operator.
 */
extern void PDOperatorCompileStates(PDOperatorRef op);

/**
 Update the numeric and real booleans based on the character.
 
 @param numeric The numeric boolean.
 @param real The real boolean.
 @param c The character.
 @param first_character Whether or not this is the first character.
 */
#define PDSymbolUpdateNumeric(numeric, real, c, first_character) \
    numeric &= ((c >= '0' && c <= '9') \
                || \
                (first_character && (c == '-' || c == '+')) \
                || \
                (! real && (real |= c == '.')))

/**
 Set numeric and real for the given symbol over the given length.
 
 @param numeric The numeric boolean.
 @param real The real boolean.
 @param c The character.
 @param sym The symbol buffer.
 @param len The length of the symbol.
 */
#define PDSymbolDetermineNumeric(numeric, real, c, sym, len) \
    do {                                          \
        PDInteger i;                              \
        for (i = len-1; numeric && i >= 0; i--) { \
            c = sym[i];                           \
            PDSymbolUpdateNumeric(numeric, real, c, i==0); \
        }                                         \
    } while (0)

#endif

/** @} */
