//
// PDTask.c
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

#include "pd_internal.h"

#include "PDTask.h"

void PDTaskDealloc(void *ob)
{
    PDTaskRef task = ob;
    PDRelease(task->child);
}

PDTaskResult PDTaskExec(PDTaskRef task, PDPipeRef pipe, PDObjectRef object)
{
    PDTaskRef parent = NULL;
    
    PDTaskResult res = PDTaskDone;
    
    while (task) {
        if (task->isActive)
            res = (*task->func)(pipe, task, object, task->info);
        else 
            res = PDTaskDone;
        
        if (PDTaskUnload == res) {
            // we can remove this task internally
            task->isActive = false;
            if (parent) {
                // we can remove it for real
                PDTaskRef child = task->child;
                task->child = NULL;
                parent->child = child;
                PDRelease(task);
                task = parent;
            }
        } 
        else if (PDTaskDone != res) {
            break;
        }
        
        parent = task;
        task = task->child;
    }
    
//    while (task && PDTaskDone == (res = (*task->func)(pipe, task, object, task->info))) {
//        parent = task;
//        task = task->child;
//    }
//    
//    if (parent && task && PDTaskUnload == res) {
//        // we can remove this task internally
//        res = task->child == NULL ? PDTaskDone : PDTaskSkipRest;
//        parent->child = NULL;
//        PDRelease(task);
//    }
    
    return res;
}

void PDTaskDestroy(PDTaskRef task)
{
    (*task->deallocator)(task);
}

PDTaskRef PDTaskCreateFilterWithValue(PDPropertyType propertyType, PDInteger value)
{
    PDTaskRef task = PDAlloc(sizeof(struct PDTask), PDTaskDestroy, false);
    task->isActive     =  true;
    task->deallocator  = &PDTaskDealloc;
    task->isFilter     = 1;
    task->propertyType = propertyType;
    task->value        = value;
    task->child        = NULL;
    return task;
}

PDTaskRef PDTaskCreateFilter(PDPropertyType propertyType)
{
    return PDTaskCreateFilterWithValue(propertyType, -1);
}

PDTaskRef PDTaskCreateMutator(PDTaskFunc mutatorFunc)
{
    PDTaskRef task = PDAlloc(sizeof(struct PDTask), PDTaskDestroy, false);
    task->isActive     =  true;
    task->deallocator  = &PDTaskDealloc;
    task->isFilter     = 0;
    task->func         = mutatorFunc;
    task->child        = NULL;
    task->info         = NULL;
    return task;
}

void PDTaskAppendTask(PDTaskRef parentTask, PDTaskRef childTask)
{
    while (childTask->child) 
        childTask = childTask->child;
    childTask->child = parentTask->child;
    parentTask->child = PDRetain(childTask);
}

void PDTaskSetInfo(PDTaskRef task, void *info)
{
    if (task->isFilter)
        task = task->child;
    task->info = info;
}

//
// Convenience non-core
//

PDTaskRef PDTaskCreateMutatorForObject(long objectID, PDTaskFunc mutatorFunc)
{
    return PDTaskCreateMutatorForPropertyTypeWithValue(PDPropertyObjectId, objectID, mutatorFunc);
}

PDTaskRef PDTaskCreateMutatorForPropertyType(PDPropertyType propertyType, PDTaskFunc mutatorFunc)
{
    return PDTaskCreateMutatorForPropertyTypeWithValue(propertyType, -1, mutatorFunc);
}

PDTaskRef PDTaskCreateMutatorForPropertyTypeWithValue(PDPropertyType propertyType, PDInteger value, PDTaskFunc mutatorFunc)
{
    PDTaskRef filter;
    PDTaskRef mutator;
    
    filter = PDTaskCreateFilterWithValue(propertyType, value);
    mutator = PDTaskCreateMutator(mutatorFunc);
    PDTaskAppendTask(filter, mutator);
    PDRelease(mutator);
    
    return filter;
}

