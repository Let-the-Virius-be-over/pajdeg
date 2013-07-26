//
//  PDPipe.c
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

#include "Pajdeg.h"

#include "PDInternal.h"
#include "PDTwinStream.h"
#include "PDReference.h"
#include "PDBTree.h"
#include "PDStack.h"
#include "PDStaticHash.h"

PDTaskResult PDPipeAppendFilterFunc(PDPipeRef pipe, PDTaskRef task, PDObjectRef object)
{
    PDPipeAddTask(pipe, task);
    return PDTaskSkipRest;
}

PDTaskFunc PDPipeAppendFilter = &PDPipeAppendFilterFunc;

//
//
//

void PDPipeDestroy(PDPipeRef pipe)
{
    PDTaskRef task;
    
    if (pipe->opened) {
        fclose(pipe->fi);
        fclose(pipe->fo);
        PDTwinStreamDestroy(pipe->stream);
        PDParserDestroy(pipe->parser);
    }
    free(pipe->pi);
    free(pipe->po);
    PDBTreeDestroyWithDeallocator(pipe->filter, (PDDeallocator)&PDTaskRelease);
    
    while (NULL != (task = (PDTaskRef)PDStackPopIdentifier(&pipe->unfilteredTasks))) {
        PDTaskRelease(task);
    }
        
    free(pipe);
}

PDPipeRef PDPipeCreateWithFilePaths(const char * inputFilePath, const char * outputFilePath)
{
    FILE *fi;
    FILE *fo;

    // input must be set
    if (inputFilePath == NULL) return NULL;
    // we do want to support NULL output for 'readonly mode' but we don't, now
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
    
    PDPipeRef pipe = calloc(1, sizeof(struct PDPipe));
    pipe->pi = strdup(inputFilePath);
    pipe->po = strdup(outputFilePath);
    return pipe;
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
            /*case PDPropertyLate:
                if (pipe->onEndOfObjectsTask == NULL) {
                    pipe->onEndOfObjectsTask = PDTaskRetain(task->child);
                } else {
                    PDTaskAppendTask(pipe->onEndOfObjectsTask, task->child);
                }
                return;*/
        }
        PDTaskRef sibling = PDBTreeFetch(pipe->filter, key);
        if (sibling) {
            // same filters; merge
            PDTaskAppendTask(sibling, task->child);
        } else {
            // not same filters; include
            pipe->filterCount++;
            PDBTreeInsert(&pipe->filter, key, PDTaskRetain(task->child));
        }
        
        if (pipe->opened && ! PDParserIsObjectStillMutable(pipe->parser, key)) {
            // pipe's open and we've already passed the object being filtered
            fprintf(stderr, "*** object %ld cannot be accessed as it has already been written ***\n", key);
            PDAssert(0); // crash = logic is flawed; object in question should be fetched after preparing pipe rather than dynamically appending filters as data is obtained; worst case, do two passes (one where the id of the offending object is determined and one where the mutations are made)
        }
    } else {
        // task executes on every iteration
        PDStackPushIdentifier(&pipe->unfilteredTasks, (PDID)task);
#if 0
        // task executes in root; this happens right after parser is set up and has read in things like root and info refs
        if (pipe->opened) {
            // which is now; this is odd, but let's be nice
            PDTaskExec(task, pipe, NULL);
        } else if (pipe->onPrepareTasks == NULL) {
            pipe->onPrepareTasks = PDTaskRetain(task);
        } else {
            PDTaskAppendTask(pipe->onPrepareTasks, task);
            
        }
#endif
    }
}

PDParserRef PDPipeGetParser(PDPipeRef pipe)
{
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
    
    return pipe->stream && pipe->parser;
}

static inline PDBool PDPipeRunUnfilteredTasks(PDPipeRef pipe, PDParserRef parser)
{
    PDTaskRef task;
    PDTaskResult result;
    PDStackRef unfilteredIter;
    PDStackRef prevStack = NULL;

    PDStackForEach(pipe->unfilteredTasks, unfilteredIter) {
        task = unfilteredIter->info;
        result = PDTaskExec(task, pipe, PDParserConstructObject(parser));
        if (PDTaskFailure == result) return false;
        if (PDTaskUnload == result) {
            // note that task unloading only reaches PDPipe for unfiltered tasks; filtered task unloading is always caught by the PDTask implementation
            if (prevStack) {
                prevStack->prev = unfilteredIter->prev;
                unfilteredIter->prev = NULL;
                PDStackDestroy(unfilteredIter);
                unfilteredIter = prevStack;
            } else {
                // since we're dropping the top item, we've lost our iteration variable, so we recall ourselves to start over (which means continuing, as this was the first item)
                PDStackPopIdentifier(&pipe->unfilteredTasks);
                PDTaskRelease(task);
                
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
    PDBTreePopulateKeys(pipe->filter, keys);
    PDStaticHashRef sht = PDStaticHashCreate(entries, keys, keys);
    
    long fpos = 0;
    long tneg = 0;
    PDBool proceed = true;
    PDInteger seen = 0;
    do {
        seen++;

        // run unfiltered tasks
        if (! (proceed &= PDPipeRunUnfilteredTasks(pipe, parser))) 
            break;
        
        // check filtered tasks
        if (pipe->dynamicFiltering || PDStaticHashValueForKey(sht, parser->obid)) {
            task = PDBTreeFetch(pipe->filter, parser->obid);
            if (task) {
                //printf("* task: object #%lu @ offset %lld *\n", parser->obid, PDTwinStreamGetInputOffset(parser->stream));
                proceed &= PDTaskFailure != PDTaskExec(task, pipe, PDParserConstructObject(parser));
            } else fpos++;
        } else { 
            tneg++;
            PDAssert(!PDBTreeFetch(pipe->filter, parser->obid));
        }
    } while (proceed && PDParserIterate(parser));
    PDStaticHashRelease(sht);
    
    proceed &= parser->success;
    
    //if (proceed && pipe->onEndOfObjectsTask) 
    //    proceed &= PDTaskFailure != PDTaskExec(pipe->onEndOfObjectsTask, pipe, NULL);
    
    if (proceed) 
        PDParserDone(parser);
    
    PDParserDestroy(parser);
    pipe->parser = NULL;
    PDTwinStreamDestroy(pipe->stream);
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
