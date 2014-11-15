/**
 * Pajdeg
 * Minimal example C file.
 *
 * This example takes a PDF as input and a filename as output and passes the input PDF through
 * Pajdeg to the output file.
 *
 * No tasks are set up, which theoretically means nothing will happen. In practice, however, Pajdeg may 
 * remove deleted / deprecated objects or rearrange PDFs that have been chopped into multiple parts through
 * appending.
 *
 * Note: taking "minimal" to heart, this example has no error checks except for `argc`.
 */

#include "../src/Pajdeg.h"

int main(int argc, char *argv[])
{
    // want in and out files as arguments
    if (argc != 3)
        return -1;

    // create, execute, and clean up pipe
    PDPipeRef pipe = PDPipeCreateWithFilePaths(argv[1], argv[2]);
    PDPipeExecute(pipe);
    PDRelease(pipe);
    
    return 0;
}
