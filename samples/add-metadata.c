/**
 * Pajdeg
 * Add a metadata entry to a PDF.
 *
 * This example takes a PDF as input and a filename as output and passes the input PDF through
 * Pajdeg to the output file.
 *
 * The example adds a metadata object, and sets up a task that links the PDF's so called root object to
 * this new object. 
 *
 * If the PDF already has a metadata entry, the example prints an error and exits. For a full solution
 * to this task, see replace-metadata.c.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "../src/Pajdeg.h"
#include "../src/PDDictionary.h"
#include "../src/PDString.h"

// convenient way to scream and die
#define die(msg...) do { fprintf(stderr, msg); exit(-1); } while (0)

// a "mutator" task, which is responsible for pointing the PDF's root object
// to a new metadata object that we're creating
PDTaskResult addMetadata(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info);

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

    // the document metadata entry of a PDF is some object somewhere, which 
    // is pointed to from the so called Root object, so we have to add a new 
    // object, and point Root at it!
    
    // get the pipe's parser
    PDParserRef parser = PDPipeGetParser(pipe);

    printf("- adding metadata object\n");
    
    // add a brand spanking new object to the PDF
    PDObjectRef meta = PDParserCreateNewObject(parser);

    // give it our meta string as its stream (the stream is where the 
    // metadata is located); the three flags at the end are
    // 1. "should the Length dictionary key be set automatically?", 
    // 2. "should the buffer be freed after use", and
    // 3. "is the value sent in pre-encrypted or not?"
    char *metaString = "Hello World!";
    PDObjectSetStream(meta, metaString, strlen(metaString), true, false, false);
    
    // now we need to point Root at it, but we can't just pull it out
    // and change it -- we have to make a task for it
    // (the reason we can change meta directly is because we MADE it)

    printf("- creating mutator for root object\n");

    // create a mutator for the root object
    PDTaskRef rootUpdater = 
        PDTaskCreateMutatorForPropertyType(PDPropertyRootObject,
                                           addMetadata);

    // pass it the meta object as its info
    PDTaskSetInfo(rootUpdater, meta);

    // add it to the pipe
    PDPipeAddTask(pipe, rootUpdater);

    printf("- executing pipe operation\n");

    // execute
    PDInteger obcount = PDPipeExecute(pipe);

    printf("- execution finished (%ld objects processed)\n", obcount);

    // clean up (note that we're not releasing meta until after PDPipeExecute
    // is called, or it will end up being deallocated before the task is 
    // called)
    PDRelease(pipe);
    PDRelease(rootUpdater);
    PDRelease(meta);
}

//
// task 
//

PDTaskResult addMetadata(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info)
{
    printf("- task 'addMetadata' starting\n");
    
    // get the dictionary for the object
    PDDictionaryRef dict = PDObjectGetDictionary(object);

    // we will ruthlessly explode if the object already HAS a metadata entry 
    // (see replace-metadata.c)
    if (PDDictionaryGetEntry(dict, "Metadata")) {
        // normally you would return PDTaskAbort here, instead of killing the 
        // entire application
        die("error: metadata already exists! aborting!\n");
    }

    // meta is our info object
    PDObjectRef meta = info;

    // set the Root's metadata reference; we are setting it to a PDObject 
    // but this translates to a PDF indirect reference, as dictionaries
    // cannot contain entire objects
    PDDictionarySetEntry(dict, "Metadata", meta);

    printf("- task 'addMetadata' finished updating root object\n");

    // tell task handler to continue as normal
    return PDTaskDone;
}
