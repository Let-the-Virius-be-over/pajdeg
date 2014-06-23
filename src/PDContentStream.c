//
// PDContentStream.c
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

#include "PDContentStream.h"
#include "pd_internal.h"
#include "PDObject.h"
#include "PDSplayTree.h"
#include "PDOperator.h"
#include "PDArray.h"
#include "pd_stack.h"
#include "PDString.h"

void PDContentStreamOperationDestroy(PDContentStreamOperationRef op)
{
    free(op->name);
    pd_stack_destroy(&op->state);
}

PDContentStreamOperationRef PDContentStreamOperationCreate(char *name, pd_stack state)
{
    PDContentStreamOperationRef op = PDAlloc(sizeof(struct PDContentStreamOperation), PDContentStreamOperationDestroy, false);
    op->name = name;
    op->state = state;
    return op;
}

//----

void PDContentStreamDestroy(PDContentStreamRef cs)
{
    while (cs->deallocators) {
        PDDeallocator deallocator = (PDDeallocator) pd_stack_pop_identifier(&cs->deallocators);
        void *userInfo = (void *) pd_stack_pop_identifier(&cs->deallocators);
        (*deallocator)(userInfo);
    }
    pd_stack_destroy(&cs->resetters);
    PDRelease(cs->ob);
    PDRelease(cs->opertree);
    pd_stack_destroy(&cs->opers);
    PDRelease(cs->args);
//    pd_array_destroy(cs->args);
    
//    PDOperatorSymbolGlobClear();
}

PDContentStreamRef PDContentStreamCreateWithObject(PDObjectRef object)
{
    PDOperatorSymbolGlobSetup();
    
    PDContentStreamRef cs = PDAlloc(sizeof(struct PDContentStream), PDContentStreamDestroy, false);
    cs->ob = PDRetain(object);
    cs->opertree = PDSplayTreeCreateWithDeallocator(free);
    cs->opers = NULL;
    cs->deallocators = NULL;
    cs->resetters = NULL;
    cs->args = PDArrayCreateWithCapacity(8);//pd_array_with_capacity(8);
    return cs;
}

void PDContentStreamAttachOperator(PDContentStreamRef cs, const char *opname, PDContentOperatorFunc op, void *userInfo)
{
    void **arr = malloc(sizeof(void*) << 1);
    arr[0] = op;
    arr[1] = userInfo;
    if (opname) {
        unsigned long oplen = strlen(opname);
        
        PDSplayTreeInsert(cs->opertree, PDST_KEY_STR(opname, oplen), arr);
    } else {
        // catch all
        PDSplayTreeInsert(cs->opertree, 0, arr);
    }
}

void PDContentStreamAttachDeallocator(PDContentStreamRef cs, PDDeallocator deallocator, void *userInfo)
{
    pd_stack_push_identifier(&cs->deallocators, (PDID)userInfo);
    pd_stack_push_identifier(&cs->deallocators, (PDID)deallocator);
}

void PDContentStreamAttachResetter(PDContentStreamRef cs, PDDeallocator resetter, void *userInfo)
{
    pd_stack_push_identifier(&cs->resetters, (PDID)userInfo);
    pd_stack_push_identifier(&cs->resetters, (PDID)resetter);
}

void PDContentStreamAttachOperatorPairs(PDContentStreamRef cs, void *userInfo, const void **pairs)
{
    for (PDInteger i = 0; pairs[i]; i += 2) {
        PDContentStreamAttachOperator(cs, pairs[i], pairs[i+1], userInfo);
    }
}

PDSplayTreeRef PDContentStreamGetOperatorTree(PDContentStreamRef cs)
{
    return cs->opertree;
}

void PDContentStreamSetOperatorTree(PDContentStreamRef cs, PDSplayTreeRef operatorTree)
{
    PDAssert(operatorTree); // crash = null operatorTree which is not allowed
    PDRetain(operatorTree);
    PDRelease(cs->opertree);
    cs->opertree = operatorTree;
}

void PDContentStreamInheritContentStream(PDContentStreamRef dest, PDContentStreamRef source)
{
    PDContentStreamSetOperatorTree(dest, PDContentStreamGetOperatorTree(source));
    PDAssert(dest->resetters == NULL); // crash = attempt to inherit from a non-clean content stream; inheriting is limited to the act of copying a content stream into multiple other content streams, and is not meant to be used to supplement a pre-configured stream (e.g. by inheriting multiple content streams) -- configure the stream *after* inheriting, if in the case of only one inherit
    dest->resetters = pd_stack_copy(source->resetters);
    // note that we do not copy the deallocators from dest; dest is the "master", and it is the responsibility of the caller to ensure that master CS'es remain alive until the spawned CS'es are all done
}

void PDContentStreamExecute(PDContentStreamRef cs)
{
    void **catchall;
    PDBool argValue;
    PDStringRef arg;
    PDBool termed, escaped;
    char termChar;
    PDContentOperatorFunc op;
    PDContentStreamOperationRef operation;
    PDInteger slen;
    void **arr;
    char *str;
    char ch;
    pd_stack inStack, outStack, termStack;

    catchall = PDSplayTreeGet(cs->opertree, 0);
    termChar = 0;
    termStack = NULL;
    termed   = false;
    escaped  = false;
    
    const char *stream = PDObjectGetStream(cs->ob);
    PDInteger   len    = PDObjectGetExtractedStreamLength(cs->ob);
    PDInteger   mark   = 0;

    pd_stack *opers = &cs->opers;
    pd_stack_destroy(opers);
    PDArrayClear(cs->args);
//    pd_array_clear(cs->args);

    for (PDInteger i = 0; i <= len; i++) {
        if (escaped) {
            escaped = false;
        } else {
            escaped = stream[i] == '\\';
            if (termChar) {
                termed = (i + 1 >= len || stream[i] == termChar);
                if (termed) {
                    if (termStack) 
                        termChar = (char)pd_stack_pop_identifier(&termStack);
                    else
                        termChar = 0;
                }
                
            }
            
            if (i < len && /*i == mark &&*/ (stream[i] == '[' || stream[i] == '(')) {
                if (termChar) {
                    pd_stack_push_identifier(&termStack, (PDID)(PDInteger)termChar);
                }
                switch (stream[i]) {
                    case '[': termChar = ']'; break;
                    case '(': termChar = ')'; break;
                }
            }
            
            if (termChar == 0 && (termed || i == len || PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobDelimiter || PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobWhitespace)) {
                if (termed + i > mark) {
                    ch = stream[mark];
                    argValue = ((ch >= '0' && ch <= '9') || ch == '<' || ch == '/' || ch == '.' || ch == '[' || ch == '(' || ch == '-' || ch == ']');
                    if (argValue && PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobDelimiter) 
                        continue;
                    
                    slen = termed + i - mark;
                    str = strndup(&stream[mark], slen);
                    arr = PDSplayTreeGet(cs->opertree, PDST_KEY_STR(str, slen));
                    
                    // if we did not get an operator, switch to catchall
                    if (arr == NULL && ! argValue) arr = catchall;
                    
                    // have we matched a string to an operator?
                    if (arr) {
                        //                    argc     = pd_array_get_count(cs->args);
                        //                    args     = pd_array_create_args(cs->args);
                        outStack = NULL;
                        inStack  = NULL;
                        
                        if (cs->opers) {
                            operation = cs->opers->info;
                            inStack = operation->state;
                        }
                        
                        cs->lastOperator = str;
                        op = arr[0];
                        PDOperatorState state = (*op)(cs, arr[1], cs->args, inStack, &outStack);
                        
                        PDArrayClear(cs->args);
                        
                        if (state == PDOperatorStatePush) {
                            operation = PDContentStreamOperationCreate(strdup(str), outStack);
                            pd_stack_push_object(opers, operation);
                            //                        pd_stack_push_key(opers, str);
                        } else if (state == PDOperatorStatePop) {
                            //                        PDAssert(cs->opers != NULL); // crash = imbalanced push/pops (too many pops!)
                            PDRelease(pd_stack_pop_object(opers));
                            //                        free(pd_stack_pop_key(opers));
                        }
                    }
                    
                    // we conditionally stuff arguments for numeric values and '/' somethings only; we do this to prevent a function from getting a ton of un-handled operators as arguments
                    else if (argValue) {
                        arg = (str[0] == '<'
                               ? PDStringCreateWithHexString(str)
                               : PDStringCreate(str));
                        PDArrayAppend(cs->args, arg);
                        PDRelease(arg);
                        str = NULL;
                    } 
                    
                    // here, we believe we've run into an operator, so we throw away accumulated arguments and start anew
                    else {
                        PDArrayClear(cs->args);
                    }
                    
                    if (str) {
                        free(str);
                    }
                }
                
                // skip over white space, but do not skip over delimiters; these are parts of arguments
                mark = i + (termed || PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobWhitespace);
                
                // we also rewind i if this was a term char; ideally we would rewind for all delimiters before above and just do i + 1 always, but that would result in endless loops for un-termchar delims
                i -= (stream[i] == '(' || stream[i] == '[');
                termed = false;
            }
        }
    }

    for (pd_stack iter = cs->resetters; iter; iter = iter->prev->prev) {
        PDDeallocator resetter = (PDDeallocator) iter->info;
        void *userInfo = (void *) iter->prev->info;
        (*resetter)(userInfo);
    }
}

const pd_stack PDContentStreamGetOperators(PDContentStreamRef cs)
{
    return cs->opers;
}

//typedef PDOperatorState (*PDContentOperatorFunc)(PDContentStreamRef cs, void *userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState);

//PDOperatorState PDContentStreamTextSearch_
//
//PDContentStreamRef PDContentStreamCreateTextSearch(PDObjectRef object, const char *searchString, PDTextSearchOperatorFunc callback)
//{
//    
//}

/*
 TABLE A.1 PDF content stream operators [PDF spec 1.7, p. 985 - 988]
 ------------------------------------------------------------------------------------------------------------------
 OPERATOR   DESCRIPTION                                                                         PRINTED
 b          Close, fill, and stroke path using nonzero winding number rule
 B          Fill and stroke path using nonzero winding number rule                              *
 b*         Close, fill, and stroke path using even-odd rule
 B*         Fill and stroke path using even-odd rule
 BDC        (PDF 1.2) Begin marked-content sequence with property list 
 BI         Begin inline image object
 BMC        (PDF 1.2) Begin marked-content sequence
 BT         Begin text object                                                                   *
 BX         (PDF 1.1) Begin compatibility section                                               *
 c          Append curved segment to path (three control points)                                *
 cm         Concatenate matrix to current transformation matrix                                 *
 CS         (PDF 1.1) Set color space for stroking operations                                   *
 cs         (PDF 1.1) Set color space for nonstroking operations                                *
 d          Set line dash pattern                                                               *
 d0         Set glyph width in Type 3 font
 d1         Set glyph width and bounding box in Type 3 font
 Do         Invoke named XObject                                                                *
 DP         (PDF 1.2) Define marked-content point with property list
 EI         End inline image object
 EMC        (PDF 1.2) End marked-content sequence
 ET         End text object                                                                     *
 EX         (PDF 1.1) End compatibility section                                                 *
 f          Fill path using nonzero winding number rule                                         *
 F          Fill path using nonzero winding number rule (obsolete)
 f*         Fill path using even-odd rule
 G          Set gray level for stroking operations                                              *
 g          Set gray level for nonstroking operations                                           *
 gs         (PDF 1.2) Set parameters from graphics state parameter dictionary                   *
 h          Close subpath                                                                       *
 i          Set flatness tolerance
 ID         Begin inline image data
 j          Set line join style                                                                 *
 J          Set line cap style                                                                  *
 K          Set CMYK color for stroking operations                                              *
 k          Set CMYK color for nonstroking operations                                           *
 l          Append straight line segment to path                                                *
 m          Begin new subpath                                                                   *
 M          Set miter limit                                                                     *
 MP         (PDF 1.2) Define marked-content point
 n          End path without filling or stroking                                                *
 q          Save graphics state                                                                 *
 Q          Restore graphics state                                                              *
 re         Append rectangle to path                                                            *
 RG         Set RGB color for stroking operations
 rg         Set RGB color for nonstroking operations                                            *
 ri         Set color rendering intent
 s          Close and stroke path
 S          Stroke path                                                                         *
 SC         (PDF 1.1) Set color for stroking operations                                         *
 sc         (PDF 1.1) Set color for nonstroking operations                                      *
 SCN        (PDF 1.2) Set color for stroking operations (ICCBased and special color spaces)     *
 scn        (PDF 1.2) Set color for nonstroking operations (ICCBased and special color spaces)  *
 sh         (PDF 1.3) Paint area defined by shading pattern                                     *
 T*         Move to start of next text line
 Tc         Set character spacing                                                               *
 Td         Move text position                                                                  *
 TD         Move text position and set leading                                                  *
 Tf         Set text font and size                                                              *
 Tj         Show text                                                                           *
 TJ         Show text, allowing individual glyph positioning                                    *
 TL         Set text leading
 Tm         Set text matrix and text line matrix                                                *
 Tr         Set text rendering mode
 Ts         Set text rise
 Tw         Set word spacing                                                                    *
 Tz         Set horizontal text scaling
 v          Append curved segment to path (initial point replicated)                            *
 w          Set line width                                                                      *
 W          Set clipping path using nonzero winding number rule                                 *
 W*         Set clipping path using even-odd rule                                               *
 y          Append curved segment to path (final point replicated)                              *
 '          Move to next line and show text
 "          Set word and character spacing, move to next line, and show text
 */

typedef struct PDContentStreamTextExtractorUI *PDContentStreamTextExtractorUI;
struct PDContentStreamTextExtractorUI {
    char **result;
    char *buf;
    PDInteger offset;
    PDInteger size;
};

void PDContentStreamTextExtractorUIDealloc(void *_tui)
{
    PDContentStreamTextExtractorUI tui = _tui;
    free(tui);
}

static inline void PDContentStreamTextExtractorPrint(PDContentStreamTextExtractorUI tui, const char *str) 
{
    PDInteger len = strlen(str);
    if (len > tui->size - tui->offset) {
        // must alloc
        tui->size = (tui->size + len) * 2;
        *tui->result = tui->buf = realloc(tui->buf, tui->size);
    }
    
    // for loop below required to handle \123 = char(123)

    len--;
    PDInteger eval = 0;
    PDBool escaping = false;
    PDInteger offs = tui->offset;
    char *res = tui->buf;
    
    // loop from 1 to original len - 1 == len to ignore wrapping parens
    for (PDInteger i = 1; i < len; i++) {
        if (escaping) {
            if (str[i] >= '0' && str[i] <= '9') {
                eval = 8 * eval + str[i] - '0';
            } else if (eval > 0) {
                if (eval > 255) PDWarn("overflow eval in PDContentStreamTextExtractorPrint()");
                escaping = false;
                i--;
                res[offs++] = eval;
            } else {
                PDNotice("unimplemented escape key passed as is in PDContentStreamTextExtractorPrint()");
                escaping = false;
                res[offs++] = '\\';
                i--;
            }
        } else if (str[i] == '\\') {
            escaping = true;
            eval = 0;
        } else {
            res[offs++] = str[i];
        }
    }
    res[offs++] = '\n';
    res[offs] = 0;
    
//    strcpy(&tui->buf[tui->offset], &str[1]);
    tui->offset = offs;
//    tui->buf[tui->offset-1] = '\n';
}
//(PDContentStreamRef cs, void *userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState);

PDOperatorState PDContentStreamTextExtractor_Tj(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    // these should have a single string as arg but we won't whine if that's not the case
    PDInteger argc = PDArrayGetCount(args);
    for (PDInteger i = 0; i < argc; i++) {
        PDContentStreamTextExtractorPrint(userInfo, PDStringEscapedValue(PDArrayGetElement(args, i), false));
    }
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamTextExtractor_TJ(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    // these are arrays of strings and offsets; we don't care about offsets
    PDInteger argc = PDArrayGetCount(args);
    if (argc == 1 && PDStringEscapedValue(PDArrayGetElement(args, 0), false)[0] == '[') {
        PDBool inParens = false;
        PDBool escaping = false;
        const char *s = PDStringEscapedValue(PDArrayGetElement(args, 0), false);
        PDInteger ix = strlen(s);
        PDInteger j = 0;
        char *string = malloc(ix);
        string[j++] = '(';
        
        for (PDInteger i = 1; i < ix; i++) {
            if (inParens) {
                if (escaping) {
                    escaping = false;
                    string[j++] = s[i];
                } 
                else if (s[i] == '\\') escaping = true;
                else if (s[i] == ')')  inParens = false;
                else string[j++] = s[i];
            } else {
                inParens = s[i] == '(';
            }
        }
        string[j++] = ')';
        string[j] = 0;
        PDContentStreamTextExtractorPrint(userInfo, string);
        free(string);
    } else {
        for (PDInteger i = 0; i < argc; i++) {
            PDStringRef s = PDArrayGetElement(args, i);
            if (PDStringIsWrapped(s)) 
                PDContentStreamTextExtractorPrint(userInfo, PDStringEscapedValue(s, true));
        }
    }
    
    return PDOperatorStateIndependent;
}

PDContentStreamRef PDContentStreamCreateTextExtractor(PDObjectRef object, char **result)
{
    PDContentStreamRef cs = PDContentStreamCreateWithObject(object);
    PDContentStreamTextExtractorUI teUI = malloc(sizeof(struct PDContentStreamTextExtractorUI));
    teUI->result = result;
    *result = teUI->buf = malloc(128);
    teUI->offset = 0;
    teUI->size = 128;
    
    PDContentStreamAttachOperator(cs, "Tj", (PDContentOperatorFunc)PDContentStreamTextExtractor_Tj, teUI);
    PDContentStreamAttachOperator(cs, "TJ", (PDContentOperatorFunc)PDContentStreamTextExtractor_TJ, teUI);
    
    PDContentStreamAttachDeallocator(cs, PDContentStreamTextExtractorUIDealloc, teUI);

    return cs;
}

typedef struct PDContentStreamPrinterUI *PDContentStreamPrinterUIRef;
struct PDContentStreamPrinterUI {
    FILE *stream;
    PDInteger spacingIndex;
    PDInteger spacingCap;
    char *spacing;
    PDReal posX, posY;
    PDReal startX, startY;
};

#define PDContentStreamPrinterPushSpacing(ui) if (ui->spacingIndex < ui->spacingCap) { ui->spacing[ui->spacingIndex++] = ' '; ui->spacing[ui->spacingIndex] = 0; }
#define PDContentStreamPrinterPopSpacing(ui)  if (ui->spacingIndex > 0) ui->spacing[--ui->spacingIndex] = 0; else fprintf(userInfo->stream, "[[[ warning: over-deindenting ]]]")

void PDContentStreamPrinterUIDealloc(void *_pui)
{
    PDContentStreamPrinterUIRef pui = _pui;
    free(pui->spacing);
    free(pui);
}

void PDContentStreamPrinterUIReset(void *_pui)
{
    PDContentStreamPrinterUIRef pui = _pui;
    if (pui->spacingIndex > 0) {
        pui->spacing[pui->spacingIndex] = ' ';
        pui->spacingIndex = 0;
    }
}

PDOperatorState PDContentStreamPrinter_q(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sq  \tSave graphics state\n", userInfo->spacing);
    PDContentStreamPrinterPushSpacing(userInfo);
    return PDOperatorStatePush;
}

PDOperatorState PDContentStreamPrinter_Q(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDContentStreamPrinterPopSpacing(userInfo);
    fprintf(userInfo->stream, "%sQ  \tRestore graphics state\n", userInfo->spacing);
    return PDOperatorStatePop;
}

PDOperatorState PDContentStreamPrinter_re(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "%sre \tAppend rectangle to path: %s\n", userInfo->spacing, buf);
    free(buf);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_rg(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "%srg \tSet RGB color for nonstroking operations: %s\n", userInfo->spacing, buf);
    free(buf);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_w(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sw  \tSet line width: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_W(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sW  \tSet clipping path using nonzero winding number rule\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Wstar(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sW* \tSet clipping path using even-odd rule\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_n(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sn  \tEnd path without filling or stroking\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_cs(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%scs \t(PDF 1.1) Set color space for nonstroking operations: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_d(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "%sd  \tSet line dash pattern: %s\n", userInfo->spacing, buf);
    free(buf);
    return PDOperatorStateIndependent;
}


PDOperatorState PDContentStreamPrinter_CS(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sCS \t(PDF 1.1) Set color space for stroking operations: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_scn(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sscn\t(PDF 1.2) Set color for nonstroking operations (ICCBased and special color spaces):", userInfo->spacing);
    PDInteger argc = PDArrayGetCount(args);
    for (PDInteger i = 0; i < argc; i++) 
        fprintf(userInfo->stream, " %s", PDStringEscapedValue(PDArrayGetElement(args, i), false));
    fputs("\n", userInfo->stream);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_SCN(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sSCN\t(PDF 1.2) Set color for stroking operations (ICCBased and special color spaces):", userInfo->spacing);
    PDInteger argc = PDArrayGetCount(args);
    for (PDInteger i = 0; i < argc; i++) 
        fprintf(userInfo->stream, " %s", PDStringEscapedValue(PDArrayGetElement(args, i), false));
    fputs("\n", userInfo->stream);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_SC(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "%sSC \t(PDF 1.1) Set color for stroking operations: RGB=%s\n", userInfo->spacing, buf);
    free(buf);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_sc(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "%ssc \t(PDF 1.1) Set color for nonstroking operations: RGB=%s\n", userInfo->spacing, buf);
    free(buf);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_B(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sB  \tFill and stroke path using nonzero winding number rule\n", userInfo->spacing);
    PDContentStreamPrinterPushSpacing(userInfo);
    return PDOperatorStatePush;
}

PDOperatorState PDContentStreamPrinter_BT(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sBT \tBegin text object\n", userInfo->spacing);
    PDContentStreamPrinterPushSpacing(userInfo);
    return PDOperatorStatePush;
}

PDOperatorState PDContentStreamPrinter_Tm(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, 
            "%sTm \tSet text matrix and text line matrix: Tm = Tlm = [ %10s %10s 0 ]\n"
            "%s   \t                                                 [ %10s %10s 0 ]\n"
            "%s   \t                                                 [ %10s %10s 1 ]\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false), userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 2), false), PDStringEscapedValue(PDArrayGetElement(args, 3), false), userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 4), false), PDStringEscapedValue(PDArrayGetElement(args, 5), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tf(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTf \tSet text font and size: font = %s, size = %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tj(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
//    if (PDArrayGetCount(args) == 1) {
//        fprintf(userInfo->stream, "%sTj \tShow text: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), true));
//    } else {
        PDInteger len = 128;
        char *buf = malloc(len);
        PDArrayPrinter(args, &buf, 0, &len);
        fprintf(userInfo->stream, "%sTj \tShow text: %s\n", userInfo->spacing, buf);
        free(buf);
//    }
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_TJ(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "%sTJ \tShow text, allowing individual glyph positioning: %s", userInfo->spacing, buf);
    free(buf);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_ET(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDContentStreamPrinterPopSpacing(userInfo);
    fprintf(userInfo->stream, "%sET \tEnd text object\n", userInfo->spacing);
    return PDOperatorStatePop;
}

PDOperatorState PDContentStreamPrinter_m(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    const char *a0 = PDStringEscapedValue(PDArrayGetElement(args, 0), false);
    const char *a1 = PDStringEscapedValue(PDArrayGetElement(args, 1), false);
    fprintf(userInfo->stream, "%sm  \tBegin new subpath: move to (%s,%s)\n", userInfo->spacing, a0, a1);
    
    userInfo->startX = userInfo->posX = PDRealFromString(a0);
    userInfo->startY = userInfo->posY = PDRealFromString(a1);

    return PDOperatorStateIndependent;
}

//Set miter limit

PDOperatorState PDContentStreamPrinter_M(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    const char *a0 = PDStringEscapedValue(PDArrayGetElement(args, 0), false);
    fprintf(userInfo->stream, "%sM  \tSet miter limit: %s\n", userInfo->spacing, a0);
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_h(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sh  \tClose subpath: draw line from pos to start: (%.1f,%.1f) - (%.1f,%.1f)\n", userInfo->spacing, userInfo->posX, userInfo->posY, userInfo->startX, userInfo->startX);
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Td(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTd \tMove text position to start of next line with current line offset: (%s,%s)\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_TD(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTD \tMove text position and set leading: (%s,%s)\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tc(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTc \tSet character spacing: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tw(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTc \tSet word spacing: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tstar(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sT* \tMove to start of next text line\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_j(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sj  \tSet line join style: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_J(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sJ  \tSet line cap style: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_K(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "%sK  \tSet CMYK color for stroking operations: %s\n", userInfo->spacing, buf);
    free(buf);
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_k(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "%sK  \tSet CMYK color for nonstroking operations: %s\n", userInfo->spacing, buf);
    free(buf);
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_l(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sl  \tAppend straight line segment to path: line (%.1f,%.1f) - (%s,%s)\n", userInfo->spacing, userInfo->posX, userInfo->posY, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false));
    
    userInfo->posX = PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 0), false)); //PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    userInfo->posY = PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 1), false)); // PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 1), false));

    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_f(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sf  \tFill path using nonzero winding number rule\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_fstar(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sf* \tFill path using even-odd rule\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_cm(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, 
            "%scm \tConcatenate matrix to current transformation matrix: [ %10s %10s 0 ]\n"
            "%s   \t                                                     [ %10s %10s 0 ]\n"
            "%s   \t                                                     [ %10s %10s 1 ]\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false), userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 2), false), PDStringEscapedValue(PDArrayGetElement(args, 3), false), userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 4), false), PDStringEscapedValue(PDArrayGetElement(args, 5), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_gs(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sgs \t(PDF 1.2) Set parameters from graphics state parameter dictionary: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Do(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sDo \tInvoke named XObject: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_BX(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sBX \t(PDF 1.1) Begin compatibility section\n", userInfo->spacing);
    PDContentStreamPrinterPushSpacing(userInfo);
    return PDOperatorStatePush;
}

PDOperatorState PDContentStreamPrinter_EX(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDContentStreamPrinterPopSpacing(userInfo);
    fprintf(userInfo->stream, "%sEX \t(PDF 1.1) End compatibility section\n", userInfo->spacing);
    return PDOperatorStatePop;
}

PDOperatorState PDContentStreamPrinter_sh(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%ssh \t(PDF 1.3) Paint area defined by shading pattern: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_S(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sS  \tStroke path\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_g(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sg  \tSet gray level for nonstroking operations: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_G(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sG  \tSet gray level for stroking operations: %s\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false));
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_c(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sc  \tAppend curved segment to path (three control points): (%s,%s) - (%s,%s) - (%s,%s)\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false), PDStringEscapedValue(PDArrayGetElement(args, 2), false), PDStringEscapedValue(PDArrayGetElement(args, 3), false), PDStringEscapedValue(PDArrayGetElement(args, 4), false), PDStringEscapedValue(PDArrayGetElement(args, 5), false));
    
    userInfo->posX = PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 4), false));
    userInfo->posY = PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 5), false));
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_v(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sv  \tAppend curved segment to path (initial point replicated): (%.1f,%.1f) - (%s,%s) - (%s,%s)\n", userInfo->spacing, userInfo->posX, userInfo->posY, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false), PDStringEscapedValue(PDArrayGetElement(args, 2), false), PDStringEscapedValue(PDArrayGetElement(args, 3), false));
    
    userInfo->posX = PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 2), false));
    userInfo->posY = PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 3), false));

    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_y(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sy  \tAppend curved segment to path (final point replicated): (%s,%s) - (%.1f,%.1f) - (%s,%s)\n", userInfo->spacing, PDStringEscapedValue(PDArrayGetElement(args, 0), false), PDStringEscapedValue(PDArrayGetElement(args, 1), false), userInfo->posX, userInfo->posY, PDStringEscapedValue(PDArrayGetElement(args, 2), false), PDStringEscapedValue(PDArrayGetElement(args, 3), false));
    
    userInfo->posX = PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 2), false));
    userInfo->posY = PDRealFromString(PDStringEscapedValue(PDArrayGetElement(args, 3), false));
    
    return PDOperatorStateIndependent;
}

//--------------------------------

PDOperatorState PDContentStreamPrinter_catchall(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, PDArrayRef args, pd_stack inState, pd_stack *outState)
{
    PDInteger len = 128;
    char *buf = malloc(len);
    PDArrayPrinter(args, &buf, 0, &len);
    fprintf(userInfo->stream, "[warning: missing implementation for operator \"%s\": %s]\n", cs->lastOperator, buf);
    free(buf);

    return PDOperatorStateIndependent;
}

PDContentStreamRef PDContentStreamCreateStreamPrinter(PDObjectRef object, FILE *stream)
{
    PDContentStreamRef cs = PDContentStreamCreateWithObject(object);
    PDContentStreamPrinterUIRef printerUI = malloc(sizeof(struct PDContentStreamPrinterUI));
    printerUI->stream = stream;
    printerUI->spacing = strdup("                                                                                                                                                                                                                                                                     ");
    printerUI->spacingCap = strlen(printerUI->spacing) - 1;
    printerUI->spacingIndex = 0;
    PDContentStreamPrinterPushSpacing(printerUI);
    PDContentStreamAttachDeallocator(cs, PDContentStreamPrinterUIDealloc, printerUI);
    PDContentStreamAttachResetter(cs, PDContentStreamPrinterUIReset, printerUI);
    
    PDContentStreamAttachOperator(cs, NULL, (PDContentOperatorFunc)PDContentStreamPrinter_catchall, printerUI);

#define pair2(name, suff) #name, PDContentStreamPrinter_##suff
#define pair(name) pair2(name, name) //#name, PDContentStreamPrinter_##name
    PDContentStreamAttachOperatorPairs(cs, printerUI, PDDef(pair(BT),
                                                            pair(cm),
                                                            pair(cs),
                                                            pair(SC),
                                                            pair(CS),
                                                            pair(B),
                                                            pair(Do),
                                                            pair(ET),
                                                            pair(gs),
                                                            pair(h),
                                                            pair(M),
                                                            pair(J),
                                                            pair(l),
                                                            pair(m),
                                                            pair2(f*, fstar),
                                                            pair(n),
                                                            pair(q),
                                                            pair(Q),
                                                            pair(re),
                                                            pair(sc),
                                                            pair(SCN),
                                                            pair(scn),
                                                            pair(Tf),
                                                            pair(Tj),
                                                            pair(Td),
                                                            pair(j),
                                                            pair(TD),
                                                            pair(Tc),
                                                            pair(Tw),
                                                            pair2(T*, Tstar),
                                                            pair(c),
                                                            pair(K),
                                                            pair(v),
                                                            pair(y),
                                                            pair(k),
                                                            pair(rg),
                                                            pair(TJ),
                                                            pair(Tm),
                                                            pair(W),
                                                            pair(w),
                                                            pair2(W*, Wstar),
                                                            pair(BX),
                                                            pair(EX),
                                                            pair(sh),
                                                            pair(S),
                                                            pair(f),
                                                            pair(g),
                                                            pair(G),
                                                            pair(d)
                                                            ));
    return cs;
}
