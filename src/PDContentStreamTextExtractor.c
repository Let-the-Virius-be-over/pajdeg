//
// PDContentStreamTextExtractor.c
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

#include "Pajdeg.h"

#include "PDContentStreamTextExtractor.h"
#include "pd_internal.h"
#include "PDObject.h"
#include "PDSplayTree.h"
#include "PDOperator.h"
#include "PDArray.h"
#include "pd_stack.h"
#include "PDString.h"
#include "PDNumber.h"

/*
 TABLE A.1 PDF content stream operators [PDF spec 1.7, p. 985 - 988]
 -------------------------------------------------------------------------------
 OPERATOR   DESCRIPTION                                                         
 b          Close, fill, and stroke path using nonzero winding number rule
 B          Fill and stroke path using nonzero winding number rule                             
 b*         Close, fill, and stroke path using even-odd rule
 B*         Fill and stroke path using even-odd rule
 BDC        (PDF 1.2) Begin marked-content sequence with property list 
 BI         Begin inline image object
 BMC        (PDF 1.2) Begin marked-content sequence
 BT         Begin text object                                                                  
 BX         (PDF 1.1) Begin compatibility section                                              
 c          Append curved segment to path (three control points)                               
 cm         Concatenate matrix to current transformation matrix                                
 CS         (PDF 1.1) Set color space for stroking operations                                  
 cs         (PDF 1.1) Set color space for nonstroking operations                               
 d          Set line dash pattern                                                              
 d0         Set glyph width in Type 3 font
 d1         Set glyph width and bounding box in Type 3 font
 Do         Invoke named XObject                                                               
 DP         (PDF 1.2) Define marked-content point with property list
 EI         End inline image object
 EMC        (PDF 1.2) End marked-content sequence
 ET         End text object                                                                    
 EX         (PDF 1.1) End compatibility section                                                
 f          Fill path using nonzero winding number rule                                        
 F          Fill path using nonzero winding number rule (obsolete)
 f*         Fill path using even-odd rule
 G          Set gray level for stroking operations                                             
 g          Set gray level for nonstroking operations                                          
 gs         (PDF 1.2) Set parameters from graphics state parameter dictionary                  
 h          Close subpath                                                                      
 i          Set flatness tolerance
 ID         Begin inline image data
 j          Set line join style                                                                
 J          Set line cap style                                                                 
 K          Set CMYK color for stroking operations                                             
 k          Set CMYK color for nonstroking operations                                          
 l          Append straight line segment to path                                               
 m          Begin new subpath                                                                  
 M          Set miter limit                                                                    
 MP         (PDF 1.2) Define marked-content point
 n          End path without filling or stroking                                               
 q          Save graphics state                                                                
 Q          Restore graphics state                                                             
 re         Append rectangle to path                                                           
 RG         Set RGB color for stroking operations
 rg         Set RGB color for nonstroking operations                                           
 ri         Set color rendering intent
 s          Close and stroke path
 S          Stroke path                                                                        
 SC         (PDF 1.1) Set color for stroking operations                                        
 sc         (PDF 1.1) Set color for nonstroking operations                                     
 SCN        (PDF 1.2) Set color for stroking operations (ICCBased and special color spaces)    
 scn        (PDF 1.2) Set color for nonstroking operations (ICCBased and special color spaces) 
 sh         (PDF 1.3) Paint area defined by shading pattern                                    
 T*         Move to start of next text line
 Tc         Set character spacing                                                              
 Td         Move text position                                                                 
 TD         Move text position and set leading                                                 
 Tf         Set text font and size                                                             
 Tj         Show text                                                                          
 TJ         Show text, allowing individual glyph positioning                                   
 TL         Set text leading
 Tm         Set text matrix and text line matrix                                               
 Tr         Set text rendering mode
 Ts         Set text rise
 Tw         Set word spacing                                                                   
 Tz         Set horizontal text scaling
 v          Append curved segment to path (initial point replicated)                           
 w          Set line width                                                                     
 W          Set clipping path using nonzero winding number rule                                
 W*         Set clipping path using even-odd rule                                              
 y          Append curved segment to path (final point replicated)                             
 '          Move to next line and show text
 "          Set word and character spacing, move to next line, and show text
 */

typedef struct PDContentStreamTextExtractorUI *PDContentStreamTextExtractorUI;
struct PDContentStreamTextExtractorUI {
    char **result;
    char *buf;
    PDInteger offset;
    PDInteger size;
    PDReal TM[6];   // text matrix; all except TM_x and TM_y are currently undefined!
#define TM_x 4
#define TM_y 5
};

void PDContentStreamTextExtractorUIDealloc(void *tui)
{
    free(tui);
}

static inline void PDContentStreamTextExtractorPrint(PDContentStreamTextExtractorUI tui, const char *str) 
{
    PDInteger len = strlen(str);
    
    if (tui->buf[tui->offset-1] == '\n') {
        while (str[0] == ' ') {
            str = &str[1];
            len--;
        }
        if (len == 0) return;
    }
    
    if (len + 1 >= tui->size - tui->offset) {
        // must alloc
        tui->size = (tui->size + len) * 2;
        *tui->result = tui->buf = realloc(tui->buf, tui->size);
    }
    
    // note: this relies on LTR writing
    tui->TM[TM_x] += 10 * len;
    
    strcpy(&tui->buf[tui->offset], str);
    tui->offset += len;
    tui->buf[tui->offset] = 0;
}

static inline void PDContentStreamTextExtractorNewline(PDContentStreamTextExtractorUI tui) 
{
    if (tui->buf[tui->offset-1] == '\n') return;
    
    if (2 >= tui->size - tui->offset) {
        // must alloc
        tui->size = (tui->size + 2) * 2;
        *tui->result = tui->buf = realloc(tui->buf, tui->size);
    }
    
    tui->buf[tui->offset++] = '\n';
    tui->buf[tui->offset] = 0;
}

PDOperatorState PDContentStreamTextExtractor_Tj(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    // these should have a single string as arg but we won't whine if that's not the case
    PDInteger argc = PDArrayGetCount(args);
    for (PDInteger i = 0; i < argc; i++) {
        PDContentStreamTextExtractorPrint(userInfo, PDStringBinaryValue(PDArrayGetElement(args, i), NULL));
    }
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamTextExtractor_TJ(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    // these are arrays of strings and offsets; we don't care about offsets
    PDInteger argc = PDArrayGetCount(args);
    if (argc == 1) {
        void *value = PDArrayGetElement(args, 0);
        if (PDResolve(value) == PDInstanceTypeArray) {
            args = value;
            argc = PDArrayGetCount(args);
        }
    }
    for (PDInteger i = 0; i < argc; i++) {
        void *v = PDArrayGetElement(args, i);
        if (PDResolve(v) == PDInstanceTypeString) {
            PDContentStreamTextExtractorPrint(userInfo, PDStringBinaryValue(v, NULL));
        }
    }
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamTextExtractor_Tm(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    // Set text matrix and text line matrix; we care mostly about TM_x and TM_y, as they may indicate a newline
    PDInteger argc = PDArrayGetCount(args);
    if (argc != 6) {
        PDWarn("invalid number of arguments in Tm call; ignoring command");
        return PDOperatorStateIndependent;
    }

    PDReal newX = PDNumberGetReal(PDArrayGetElement(args, TM_x));
    PDReal newY = PDNumberGetReal(PDArrayGetElement(args, TM_y));
    PDReal diffX = newX - userInfo->TM[TM_x];
    PDReal diffY = newY - userInfo->TM[TM_y];
    userInfo->TM[TM_x] = newX;
    userInfo->TM[TM_y] = newY;
    if ((diffX < -1 || diffX > 1) && (diffY < -1 || diffY > 1)) {
        // a change in both x and y is considered a newline; the reasoning behind this odd decision is that text may flow in any direction depending on the language, but a newline is the only case where both axes are changed (for LTR text, the X position is set to "left margin" and the Y position is decremented (Y points up) to the line below the current one)
        PDContentStreamTextExtractorNewline(userInfo);
    }
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamTextExtractor_Td(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    // Move to the start of the next line, offset from the start of the current line by the arguments (tx, ty)
    PDInteger argc = PDArrayGetCount(args);
    if (argc != 2) {
        PDWarn("invalid number of arguments in Td call; ignoring command");
        return PDOperatorStateIndependent;
    }
    
    // since we're not actually tracking the TM, we simply make sure it's not accidentally set to something that causes oddnesses in rare occasions
    userInfo->TM[TM_x] = userInfo->TM[TM_y] = -1;
    
    PDContentStreamTextExtractorNewline(userInfo);
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamTextExtractor_Tstar(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    // Move to the start of the next line.
    userInfo->TM[TM_x] = userInfo->TM[TM_y] = -1;
    PDContentStreamTextExtractorNewline(userInfo);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamTextExtractor_Tapostrophe(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    // move to the next line and show string
    userInfo->TM[TM_x] = userInfo->TM[TM_y] = -1;
    PDContentStreamTextExtractorNewline(userInfo);
    return PDContentStreamTextExtractor_Tj(cs, userInfo, args, inState, outState);
}

PDOperatorState PDContentStreamTextExtractor_Tcite(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    if (PDArrayGetCount(args) != 3) {
        PDWarn("invalid number of arguments in T\" call; ignoring command");
        return PDOperatorStateIndependent;
    }
    
    // move to the next line and show string using given word and char spacing
    userInfo->TM[TM_x] = userInfo->TM[TM_y] = -1;
    PDContentStreamTextExtractorNewline(userInfo);
    PDArrayDeleteAtIndex(args, 1);
    PDArrayDeleteAtIndex(args, 0);
    return PDContentStreamTextExtractor_Tj(cs, userInfo, args, inState, outState);
}

PDContentStreamRef PDContentStreamCreateTextExtractor(PDObjectRef object, char **result)
{
    PDContentStreamRef cs = PDContentStreamCreateWithObject(object);
    PDContentStreamTextExtractorUI teUI = malloc(sizeof(struct PDContentStreamTextExtractorUI));
    teUI->result = result;
    *result = teUI->buf = malloc(128);
    teUI->buf[0] = '\n';
    teUI->offset = 1;
    teUI->size = 128;
    teUI->TM[TM_y] = teUI->TM[TM_x] = -1;

#define pair2(name, suff) #name, PDContentStreamTextExtractor_##suff
#define pair(name) pair2(name, name)
    PDContentStreamAttachOperatorPairs(cs, teUI, 
                                       PDDef(pair(Tj),
                                             pair(TJ),
                                             "T'", PDContentStreamTextExtractor_Tapostrophe,
                                             "T\"", PDContentStreamTextExtractor_Tcite,
                                             pair2(T*, Tstar),
                                             pair(Td),
                                             pair2(TD, Td),
                                             pair(Tm)
                                             )
                                       );
    
    PDContentStreamAttachDeallocator(cs, PDContentStreamTextExtractorUIDealloc, teUI);

    return cs;
}
