//
// PDEnv.h
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
 @file PDEnv.h Environment header file.
 
 @ingroup PDENV
 
 @defgroup PDENV PDEnv
 
 @brief An instance of a PDState in a scanner.
 
 @ingroup PDSCANNER_CONCEPT
 
 PDEnv objects are simple, low level instance representations of PDStateRef objects. That is to say, whenever a state is pushed onto the stack, an environment is created, wrapping that state, and the current environment is pushed onto the scanner's environment stack.
 
 In practice, environments also keep track of the build and var stacks.
 
 ### The build stack
 
 The build stack is a pd_stack of objects making up a bigger object in the process of being scanned. For instance, if the scanner has just finished reading

 @code
    <<  /Info 1 2 R 
        /Type /Metadata
        /Subtype 
 @endcode
 
 the build stack will consist of the /Info and /Type dictionary entries. 
 
 ### The var stack
 
 The var stack is very similar to the build stack, except it is made up of *one* object being parsed. In the example above, the var stack would contain the PDF name "Subtype", because the value of the dictionary entry has not yet been scanned.
 
 @{
 */

#ifndef INCLUDED_PDENV_h
#define INCLUDED_PDENV_h

#include "PDDefines.h"

/**
 Create an environment with the given state.
 
 @param state The state.
 @return The environment.
 */
extern PDEnvRef PDEnvCreate(PDStateRef state);

#endif

/** @} */
