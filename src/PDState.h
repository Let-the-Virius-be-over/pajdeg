//
// PDState.h
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
 @file PDState.h The state header.
 
 @ingroup PDSTATE

 @defgroup PDSTATE PDState
 
 @brief A state.
 
 @ingroup PDSCANNER_CONCEPT
 
 A state in Pajdeg is a definition of a given set of conditions.
 
 @see pd_pdf_implementation.h
 
 @{
 */

#ifndef INCLUDED_PDState_h
#define INCLUDED_PDState_h

#include "PDDefines.h"

/**
 Create a new state with the given name.
 
 @param name The name of the state, e.g. "dict".
 */
extern PDStateRef PDStateCreate(char *name);

/**
 Compile a state.
 
 @param state The state.
 */
extern void PDStateCompile(PDStateRef state);

/**
 Define state operators using a PDDef definition. 
 
 @param state The state.
 @param defs PDDef definitions.
 */
extern void PDStateDefineOperatorsWithDefinition(PDStateRef state, const void **defs);

#endif

/** @} */
