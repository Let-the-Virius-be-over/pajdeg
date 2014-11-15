#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "../src/Pajdeg.h"
#include "../src/PDDictionary.h"
#include "../src/PDString.h"

// convenient way to scream and die
#define die(msg...) do { fprintf(stderr, msg); exit(-1); } while (0)

// a "mutator" task, which is responsible for pointing the PDF's root object
// to a new metadata object that we're creating
PDTaskResult addMetadata(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info);

// a mutator task for updating a metadata object's contents
PDTaskResult updateMetadata(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info);

//
// main program
//

int main(int argc, char *argv[])
{
    // want in and out files as arguments
    if (argc != 3) die("syntax: %s <input PDF file> <output PDF name>\n", argv[0]);

    printf("creating pipe\n" 
           "input  : %s\n"
           "output : %s\n", argv[1], argv[2]);

    // create pipe
    PDPipeRef pipe = PDPipeCreateWithFilePaths(argv[1], argv[2]);
    if (NULL == pipe) die("failed to create pipe\n");

    // we now check the root object, to see if it has a metadata entry or not
    // if it does, we add a task to update the object, and if it doesn't
    // we add a new metadata object and add a /Metadata key to the root object

    // get the pipe's parser
    PDParserRef parser = PDPipeGetParser(pipe);

    // pull out the definition for the root object -- we can do this before
    // executing the pipe, but the objects are immutable (i.e. we cannot change
    // them)
    PDObjectRef root = PDParserGetRootObject(parser);
    PDDictionaryRef rootDict = PDObjectGetDictionary(root);
    PDReferenceRef metaRef = PDDictionaryGetEntry(rootDict, "Metadata");

    PDObjectRef meta = NULL;
    PDTaskRef mutator;
    if (metaRef) {
        // we have a metadata entry, so we set up a task to change it
        printf("- metadata exists - updating\n");
        mutator = PDTaskCreateMutatorForObject(PDReferenceGetObjectID(metaRef),
                                               updateMetadata);
    } else {
        // we don't have a metadata entry
        printf("- metadata missing - adding\n");

        // we are using a different method for creating the object here,
        // which appends the new object to the END of the PDF, rather than
        // putting it into the current parser position (start, in this case)
        meta = PDParserCreateAppendedObject(parser);
        
        char *metaString = "Hello World!";
        PDObjectSetStream(meta, metaString, strlen(metaString), true, false, false);
    
        // create a mutator for the root object
        mutator = PDTaskCreateMutatorForPropertyType(PDPropertyRootObject,
                                                     addMetadata);

        // and hand it the meta object
        PDTaskSetInfo(mutator, meta);
    }

    // whichever it was, we now have a task in mutator that we add to the
    // pipe and release our task
    PDPipeAddTask(pipe, mutator);
    PDRelease(mutator);

    // execute
    printf("- executing pipe\n");
    PDInteger obcount = PDPipeExecute(pipe);
    printf("- execution finished (%ld objects processed)\n", obcount);

    // clean up -- we conditionally release the meta object as it may not have
    // been set up, if there was a meta object in existence already
    // also note that we do not release root, as it's not created nor retained
    // by us
    PDRelease(pipe);
    if (meta) PDRelease(meta);
}

//
// task - adding metadata
//

PDTaskResult addMetadata(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info)
{
    printf("- performing addMetadata task\n");
    
    // get the dictionary for the object
    PDDictionaryRef dict = PDObjectGetDictionary(object);
    
    // if we get here and the root object turned out to have a metadata
    // object after all, our code is buggy
    if (PDDictionaryGetEntry(dict, "Metadata")) {
        die("error: metadata already exists! code is buggy!\n");
    }
    
    // info is our meta
    PDObjectRef meta = info;

    // set the Root's metadata reference; we are setting it to a PDObject
    // but this translates to a PDF indirect reference, as dictionaries
    // cannot contain entire objects
    PDDictionarySetEntry(dict, "Metadata", meta);    

    // tell task handler to continue as normal
    return PDTaskDone;
}

//
// task - updating metadata
//

PDTaskResult updateMetadata(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info)
{
    printf("- performing updateMetadata task\n");
    
    // we do exactly the same thing as we did when we created a new object, but
    // we do it INSIDE OF THE TASK -- the ONLY place where objects we did not 
    // create ourselves can be modified
    char *metaString = "Hello Again, World!";
    PDObjectSetStream(object, metaString, strlen(metaString), true, false, false);

    // tell task handler to continue as normal
    return PDTaskDone;
}
