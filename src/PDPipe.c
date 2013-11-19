//
// PDPipe.c
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

#include "Pajdeg.h"

#include "pd_internal.h"
#include "PDTwinStream.h"
#include "PDReference.h"
#include "PDBTree.h"
#include "pd_stack.h"
#include "PDStaticHash.h"
#include "PDObjectStream.h"
#include "PDXTable.h"

/*PDTaskResult PDPipeAppendFilterFunc(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info)
{
    PDPipeAddTask(pipe, task);
    return PDTaskSkipRest;
}

PDTaskFunc PDPipeAppendFilter = &PDPipeAppendFilterFunc;*/

//
//
//

void PDPipeDestroy(PDPipeRef pipe)
{
    PDTaskRef task;
    
    if (pipe->opened) {
        fclose(pipe->fi);
        fclose(pipe->fo);
        PDRelease(pipe->stream);
        PDRelease(pipe->parser);
    }
    free(pipe->pi);
    free(pipe->po);
    PDRelease(pipe->filter);
    //pd_btree_destroy_with_deallocator(pipe->filter, PDRelease);
    
    while (NULL != (task = (PDTaskRef)pd_stack_pop_identifier(&pipe->unfilteredTasks))) {
        PDRelease(task);
    }
}

PDPipeRef PDPipeCreateWithFilePaths(const char * inputFilePath, const char * outputFilePath)
{
    FILE *fi;
    FILE *fo;

    // input must be set
    if (inputFilePath == NULL) return NULL;
    // we do want to support NULL output for 'readonly mode' but we don't, now; NIX users can pass "/dev/null" to get this behavior
    if (outputFilePath == NULL) return NULL;
    
    // files must not be the same
    if (!strcmp(inputFilePath, outputFilePath)) return NULL;
    
    fi = fopen(inputFilePath, "r");
    if (NULL == fi) return NULL;
    
    fo = fopen(outputFilePath, "w+");
    if (NULL == fo) {
        fclose(fi);
        return NULL;
    }
    
    fclose(fi);
    fclose(fo);
    
    PDPipeRef pipe = PDAlloc(sizeof(struct PDPipe), PDPipeDestroy, true);
    pipe->pi = strdup(inputFilePath);
    pipe->po = strdup(outputFilePath);
    return pipe;
}

PDTaskResult PDPipeObStreamMutation(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info)
{
    PDTaskRef subTask;
    PDObjectRef ob;
    PDObjectStreamRef obstm;
    pd_stack mutators;
    pd_stack iter;
    char *stmbuf;
    
    obstm = PDObjectStreamCreateWithObject(object);
    stmbuf = PDParserFetchCurrentObjectStream(pipe->parser, object->obid);
    PDObjectStreamParseExtractedObjectStream(obstm, stmbuf);
    
    mutators = info;
    iter = mutators;
    
    while (iter) {
        // each entry is a PDTask that is a filter on a specific object, and the object is located inside of the stream
        subTask = iter->info;
        PDAssert(subTask->isFilter);
        ob = PDObjectStreamGetObjectByID(obstm, subTask->value);
        PDAssert(ob);
        if (PDTaskFailure == PDTaskExec(subTask->child, pipe, ob)) {
            return PDTaskFailure;
        }
        iter = iter->prev;
    }
    
    PDObjectStreamCommit(obstm);
    
    return PDTaskDone;
}

void PDPipeAddTask(PDPipeRef pipe, PDTaskRef task)
{
    long key;
    
    if (task->isFilter) {
        PDAssert(task->child);
        pipe->dynamicFiltering = true;
        switch (task->propertyType) {
            case PDPropertyObjectId:
                key = task->value;
                break;
            case PDPropertyInfoObject:
                if (pipe->opened == false) PDPipePrepare(pipe);
                key = pipe->parser->infoRef ? pipe->parser->infoRef->obid : -1;
                break;
            case PDPropertyRootObject:
                if (pipe->opened == false) PDPipePrepare(pipe);
                key = pipe->parser->rootRef ? pipe->parser->rootRef->obid : -1;
                break;
            case PDPropertyPage:
                if (pipe->opened == false) PDPipePrepare(pipe);
                
            /*case PDPropertyLate:
                if (pipe->onEndOfObjectsTask == NULL) {
                    pipe->onEndOfObjectsTask = PDTaskRetain(task->child);
                } else {
                    PDTaskAppendTask(pipe->onEndOfObjectsTask, task->child);
                }
                return;*/
        }
        
        if (! pipe->opened)
            if (! PDPipePrepare(pipe)) 
                return;
        
        // if this is a reference to an object inside an object stream, we have to pull that open
        PDInteger containerOb = PDParserGetContainerObjectIDForObject(pipe->parser, key);
        if (containerOb != -1) {
            // force the value into the task, in case this was a root or info req
            task->value = key;
            PDTaskRef containerTask = PDBTreeGet(pipe->filter, containerOb);
            //pd_btree_fetch(pipe->filter, containerOb);
            if (NULL == containerTask) {
                // no container task yet so we set one up
                containerTask = PDTaskCreateMutator(PDPipeObStreamMutation);
                pipe->filterCount++;
                PDBTreeInsert(pipe->filter, containerOb, containerTask);
                //pd_btree_insert(&pipe->filter, containerOb, containerTask);
                containerTask->info = NULL;
            }
            pd_stack_push_object((pd_stack *)&containerTask->info, PDRetain(task));
            return;
        }
        
        PDTaskRef sibling = PDBTreeGet(pipe->filter, key);
        //pd_btree_fetch(pipe->filter, key);
        if (sibling) {
            // same filters; merge
            PDTaskAppendTask(sibling, task->child);
        } else {
            // not same filters; include
            pipe->filterCount++;
            PDBTreeInsert(pipe->filter, key, PDRetain(task->child));
            //pd_btree_insert(&pipe->filter, key, PDRetain(task->child));
        }
        
        if (pipe->opened && ! PDParserIsObjectStillMutable(pipe->parser, key)) {
            // pipe's open and we've already passed the object being filtered
            fprintf(stderr, "*** object %ld cannot be accessed as it has already been written ***\n", key);
            PDAssert(0); // crash = logic is flawed; object in question should be fetched after preparing pipe rather than dynamically appending filters as data is obtained; worst case, do two passes (one where the id of the offending object is determined and one where the mutations are made)
        }
    } else {
        // task executes on every iteration
        pd_stack_push_identifier(&pipe->unfilteredTasks, (PDID)PDRetain(task));
#if 0
        // task executes in root; this happens right after parser is set up and has read in things like root and info refs
        if (pipe->opened) {
            // which is now; this is odd, but let's be nice
            PDTaskExec(task, pipe, NULL);
        } else if (pipe->onPrepareTasks == NULL) {
            pipe->onPrepareTasks = PDRetain(task);
        } else {
            PDTaskAppendTask(pipe->onPrepareTasks, task);
            
        }
#endif
    }
}

PDParserRef PDPipeGetParser(PDPipeRef pipe)
{
    if (! pipe->opened) 
        if (! PDPipePrepare(pipe))
            return NULL;
    
    return pipe->parser;
}

PDObjectRef PDPipeGetRootObject(PDPipeRef pipe)
{
    if (! pipe->opened) 
        if (! PDPipePrepare(pipe)) 
            return NULL;
    
    return PDParserGetRootObject(pipe->parser);
}

PDBool PDPipePrepare(PDPipeRef pipe)
{
    if (pipe->opened) {
        return true;
    }
    
    pipe->fi = fopen(pipe->pi, "r");
    if (NULL == pipe->fi) return -1;
    pipe->fo = fopen(pipe->po, "w+");
    if (NULL == pipe->fo) {
        fclose(pipe->fi);
        return false;
    }
    
    pipe->opened = true;
    
    pipe->stream = PDTwinStreamCreate(pipe->fi, pipe->fo);
    pipe->parser = PDParserCreateWithStream(pipe->stream);
    
    pipe->filter = PDBTreeCreate(PDRelease, 1, pipe->parser->mxt->count, 3);

    return pipe->stream && pipe->parser;
}

static inline PDBool PDPipeRunUnfilteredTasks(PDPipeRef pipe, PDParserRef parser)
{
    PDTaskRef task;
    PDTaskResult result;
    pd_stack unfilteredIter;
    pd_stack prevStack = NULL;

    pd_stack_for_each(pipe->unfilteredTasks, unfilteredIter) {
        task = unfilteredIter->info;
        result = PDTaskExec(task, pipe, PDParserConstructObject(parser));
        if (PDTaskFailure == result) return false;
        if (PDTaskUnload == result) {
            // note that task unloading only reaches PDPipe for unfiltered tasks; filtered task unloading is always caught by the PDTask implementation
            if (prevStack) {
                prevStack->prev = unfilteredIter->prev;
                unfilteredIter->prev = NULL;
                pd_stack_destroy(unfilteredIter);
                unfilteredIter = prevStack;
            } else {
                // since we're dropping the top item, we've lost our iteration variable, so we recall ourselves to start over (which means continuing, as this was the first item)
                pd_stack_pop_identifier(&pipe->unfilteredTasks);
                PDRelease(task);
                
                return PDPipeRunUnfilteredTasks(pipe, parser);
            }
        }
        prevStack = unfilteredIter;
    }
    return true;
}

PDInteger PDPipeExecute(PDPipeRef pipe)
{
    // if pipe is closed, we need to prepare
    if (! pipe->opened && ! PDPipePrepare(pipe)) 
        return -1;
    
    PDParserRef parser = pipe->parser;
    PDTaskRef task;
    
    // at this point, we set up a static hash table for O(1) filtering before the O(n) tree fetch; the SHT implementation here triggers false positives and cannot be used on its own
    PDInteger entries = pipe->filterCount;
    void **keys = malloc(entries * sizeof(void*));
    pipe->dynamicFiltering = false;
    PDBTreePopulateKeys(pipe->filter, (PDInteger*)keys);
    //pd_btree_populate_keys(pipe->filter, keys);
    
    PDStaticHashRef sht = PDStaticHashCreate(entries, keys, keys);
    
    long fpos = 0;
    long tneg = 0;
    PDBool proceed = true;
    PDInteger seen = 0;
    do {
        PDFlush();
        
        seen++;

        // run unfiltered tasks
        if (! (proceed &= PDPipeRunUnfilteredTasks(pipe, parser))) 
            break;
        
        // check filtered tasks
        if (pipe->dynamicFiltering || PDStaticHashValueForKey(sht, parser->obid)) {
            task = PDBTreeGet(pipe->filter, parser->obid);
            //pd_btree_fetch(pipe->filter, parser->obid);
            if (task) {
                //printf("* task: object #%lu @ offset %lld *\n", parser->obid, PDTwinStreamGetInputOffset(parser->stream));
                proceed &= PDTaskFailure != PDTaskExec(task, pipe, PDParserConstructObject(parser));
            } else fpos++;
        } else { 
            tneg++;
            PDAssert(!PDBTreeGet(pipe->filter, parser->obid));
                     //pd_btree_fetch(pipe->filter, parser->obid));
        }
    } while (proceed && PDParserIterate(parser));
    PDRelease(sht);
    PDFlush();
    
    proceed &= parser->success;
    
    //if (proceed && pipe->onEndOfObjectsTask) 
    //   proceed &= PDTaskFailure != PDTaskExec(pipe->onEndOfObjectsTask, pipe, NULL);
    
    if (proceed) 
        PDParserDone(parser);
    
    PDRelease(pipe->filter);
    PDRelease(parser);
    PDRelease(pipe->stream);
    
    pipe->filter = NULL;
    pipe->parser = NULL;
    pipe->stream = NULL;
    
    fclose(pipe->fi);
    fclose(pipe->fo);
    pipe->opened = false;
    
    return proceed ? seen : -1;
}

const char *PDPipeGetInputFilePath(PDPipeRef pipe)
{
    return pipe->pi;
}

const char *PDPipeGetOutputFilePath(PDPipeRef pipe)
{
    return pipe->po;
}
