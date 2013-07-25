//
//  PDTask.c
//
//  Copyright (c) 2013 Karl-Johan Alm (http://github.com/kallewoof)
// 
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
// 
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include "PDInternal.h"

#include "PDTask.h"

void PDTaskDealloc(void *ob)
{
    PDTaskRef task = ob;
    if (task->child)  
        PDTaskRelease(task->child);
    free(task);
}

PDTaskResult PDTaskExec(PDTaskRef task, PDPipeRef pipe, PDObjectRef object)
{
    PDTaskResult res = PDTaskDone;
    while (task && PDTaskDone == (res = (*task->func)(pipe, task, object))) {
        task = task->child;
    }
    return res;
}

PDTaskRef PDTaskCreateFilterWithValue(PDPropertyType propertyType, int value)
{
    PDTaskRef task = malloc(sizeof(struct PDTask));
    task->deallocator  = &PDTaskDealloc;
    task->users        = 1;
    task->isFilter     = 1;
    task->func         = PDPipeAppendFilter;
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
    PDTaskRef task = malloc(sizeof(struct PDTask));
    task->deallocator  = &PDTaskDealloc;
    task->users        = 1;
    task->isFilter     = 0;
    task->func         = mutatorFunc;
    task->child        = NULL;
    return task;
}

PDTaskRef PDTaskRetain(PDTaskRef task)
{
    task->users++;
    return task;
}

void PDTaskRelease(PDTaskRef task)
{
    task->users--;
    if (task->users == 0)
        (*task->deallocator)(task);
}

void PDTaskAppendTask(PDTaskRef parentTask, PDTaskRef childTask)
{
    while (childTask->child) 
        childTask = childTask->child;
    childTask->child = parentTask->child;
    parentTask->child = PDTaskRetain(childTask);
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

PDTaskRef PDTaskCreateMutatorForPropertyTypeWithValue(PDPropertyType propertyType, int value, PDTaskFunc mutatorFunc)
{
    PDTaskRef filter;
    PDTaskRef mutator;
    
    filter = PDTaskCreateFilterWithValue(propertyType, value);
    mutator = PDTaskCreateMutator(mutatorFunc);
    PDTaskAppendTask(filter, mutator);
    PDTaskRelease(mutator);
    
    return filter;
}

