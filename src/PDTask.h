//
// PDTask.h
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
 @file PDTask.h Task header file.
 
 @ingroup PDTASK
 
 @defgroup PDTASK PDTask
 
 @brief A Pajdeg pipe task.
 
 @ingroup PDPIPE_CONCEPT
 
 @{
 */

#ifndef INCLUDED_PDTask_h
#define INCLUDED_PDTask_h

#include "PDDefines.h"

///---------------------------------------
/// @name Creating and manipulating filters
///---------------------------------------

/**
 Create a filtering task. Filtering tasks find objects in a pipe based on given criteria and forward these to their child tasks.
 */
extern PDTaskRef PDTaskCreateFilter(PDPropertyType propertyType);

/**
 Create a filtering task with the given argument.
 */
extern PDTaskRef PDTaskCreateFilterWithValue(PDPropertyType propertyType, PDInteger value);

/**
 Create a mutator task. Mutator tasks receive objects from the pipe if their parent tasks pass them through. A mutator task can be a filter, if it uses PDTaskSkipRest to abort for objects that should be skipped.
 */
extern PDTaskRef PDTaskCreateMutator(PDTaskFunc mutatorFunc);

/**
 Append a child task to a task. Child tasks are executed after their parent tasks, unless the parent decides to stop the chain. Note that childTask's child will be set to whatever value parentTask's child is, and parentTask will take childTask as its new child. Thus, adding child A then child B to parent P will result in the execution order
 
    P -> B -> A
 
 */
extern void PDTaskAppendTask(PDTaskRef parentTask, PDTaskRef childTask);

/**
 Set the info object for a task. 
 
 The info object is passed as is to the task upon execution. 
 
 @param task The task.
 @param info The info object.
 */
extern void PDTaskSetInfo(PDTaskRef task, void *info);

/**
 Execute a task, possibly resulting in a chain of tasks executing if the task has children.
 */
extern PDTaskResult PDTaskExec(PDTaskRef task, PDPipeRef pipe, PDObjectRef object);

///---------------------------------------
/// @name Convenience functions
///---------------------------------------

/**
 Create a mutator for the given object ID.
 */
extern PDTaskRef PDTaskCreateMutatorForObject(long objectID, PDTaskFunc mutatorFunc);

/**
 Create a mutator for the given (value-less) property type.
 */
extern PDTaskRef PDTaskCreateMutatorForPropertyType(PDPropertyType propertyType, PDTaskFunc mutatorFunc);

/**
 Create a mutator for the given property type with the given value.
 */
extern PDTaskRef PDTaskCreateMutatorForPropertyTypeWithValue(PDPropertyType propertyType, PDInteger value, PDTaskFunc mutatorFunc);

#endif

/** @} */

/** @} */


