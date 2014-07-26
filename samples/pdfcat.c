#include "Pajdeg/Pajdeg.h"
#include <stdio.h>
#include <stdlib.h>

PDTaskResult printer(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info);

PDInteger bufSize;
char *buf;

int main(int argc, char *argv[]) 
{
    if (argc < 2) {
        printf("syntax: %s <pdf path> [...]\n", argv[0]);
        return -1;
    }

    // set up buffer; it's used in printer below
    bufSize = 1024;
    buf = malloc(1024);

    for (int i = 1; i < argc; i++) {

        PDPipeRef pipe = PDPipeCreateWithFilePaths(argv[i], "/dev/null");
    
        PDParserRef parser = PDPipeGetParser(pipe);

        // create mutator task without a filter; this means the task will run on ALL objects
        PDTaskRef task = PDTaskCreateMutator(printer);

        PDPipeAddTask(pipe, task);

        PDRelease(task);

        PDPipeExecute(pipe);

        PDRelease(pipe);

    }

    return 0;
}

PDTaskResult printer(PDPipeRef pipe, PDTaskRef task, PDObjectRef object, void *info)
{
    // grab definition for the object and updated bufSize if it exceeded the current one
    PDInteger defSize = PDObjectGenerateDefinition(object, &buf, bufSize);
    if (defSize > bufSize) {
        bufSize = defSize;
    }
    fputs(buf, stdout);

    if (PDObjectHasStream(object)) {
        PDParserRef parser = PDPipeGetParser(pipe);
        char *stream = PDParserFetchCurrentObjectStream(parser, PDObjectGetObID(object));

        puts("stream");
        if (PDObjectHasTextStream(object)) {
            puts(stream);
        } else {
            printf("[%ld bytes of binary data]\n", PDObjectGetExtractedStreamLength(object));
        }
        puts("endstream");
    }

    puts("endobj");

    return PDTaskDone;
}
