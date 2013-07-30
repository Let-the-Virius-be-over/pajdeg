//
//  Pajdeg.h
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

/**
 @mainpage Pajdeg PDF mutation library
 
 @section intro_sec Introduction
 
 Pajdeg is a C library for modifying existing PDF documents by passing them through a stream with tasks assigned based on object ID's.
 
 Typical usage involves three things:
 
 - Setting up a PDPipe with in- and out paths.
 - Adding filters and/or mutator PDTasks to the pipe.
 - Executing.
 
 Tasks can be chained together, and appended to the stream at any time through other tasks, with the caveat that the requested object has not yet passed through the stream.
 
 @section quick_start Quick Start

 To get started with Pajdeg, you may want to 

 - check out the @link QUICKSTART quick start page@endlink.
 - check out the @link PDUSER user level module list@endlink.
 
 
 @section dependencies_sec Dependencies
 
 Pajdeg has very few dependencies. While the aim was to have none at all, certain PDFs require compression to function at all.
 
 Beyond this, Pajdeg does not contain any other dependencies beyond a relatively modern C compiler.
 
 @subsection libz_subsec libz
 
 Pajdeg can use libz for stream compression. Some PDFs, in particular PDFs made using the 1.5+ specification, require libz support to be parsed via Pajdeg, because the cross reference table can be a compressed object stream. If support for this is not desired, libz support can be disabled by removing the PD_SUPPORT_ZLIB definition in PDDefines.h.
 
 @section integrating_sec Integrating
 
 Adding Pajdeg to your project can be done by either compiling the static library using the provided Makefile, and putting the library and the .h files into your project, or by adding the .c and .h files directly. 
*/

/**
 @page QUICKSTART Quick Start
 
 @section setup_sec Setting up

 Pajdeg is meant to be included in some other project, either as a static library built e.g. using the included Makefile, or by inserting the .c and .h files directly into a project. 
 
 @subsection setup_xcode_sec Xcode
 
 For Xcode (4.x), dragging the Pajdeg folder as is into an existing project is all you need to do (and add the libz framework, if necessary), but if Xcode suggests adding a build phase for the included Makefiles, you should skip that part as the files will compile fine as is.
 
 @section minimal_sec Minimal example
 
 From @c examples/minimal.c :
 
 @include minimal.c
 
 This example takes the input PDF (first argument) and pipes it through Pajdeg to the output path (second argument) without applying any tasks to it. (The resulting PDF will most likely differ from the first, binary-wise, but they will be identical when viewed in a PDF viewer.)
 
 - PDPipeCreateWithFilePaths() sets up a new pipe, ensuring that the paths are valid,
 - PDPipeExecute() initiates the actual piping operation, which will execute all the way to the end of the input file, and
 - PDPipeDestroy() cleans up the pipe instance.
 
 @section replacemetadata_sec Replacing a PDF's metadata entry
 
 From @c examples/replace-metadata.c :
 
 @include examples/replace-metadata.c
 
 
 
 */

#ifndef INCLUDED_Pajdeg_h
    #define INCLUDED_Pajdeg_h

    #include "PDPipe.h"
    #include "PDObject.h"
    #include "PDTask.h"
    #include "PDParser.h"
    #include "PDReference.h"
    #include "PDScanner.h"
#endif
