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
#include "PDBTree.h"
#include "PDOperator.h"
#include "PDArray.h"
#include "pd_stack.h"

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
    PDRelease(cs->ob);
    PDRelease(cs->opertree);
    pd_stack_destroy(&cs->opers);
    pd_array_destroy(cs->args);
    
//    PDOperatorSymbolGlobClear();
}

PDContentStreamRef PDContentStreamCreateWithObject(PDObjectRef object)
{
    PDOperatorSymbolGlobSetup();
    
    PDContentStreamRef cs = PDAlloc(sizeof(struct PDContentStream), PDContentStreamDestroy, false);
    cs->ob = PDRetain(object);
    cs->opertree = PDBTreeCreate(free, 0, 10000000, 4);
    cs->opers = NULL;
    cs->args = pd_array_with_capacity(8);
    return cs;
}

void PDContentStreamAttachOperator(PDContentStreamRef cs, const char *opname, PDContentOperatorFunc op, void *userInfo)
{
    void **arr = malloc(sizeof(void*) << 1);
    arr[0] = op;
    arr[1] = userInfo;
    if (opname) {
        unsigned long oplen = strlen(opname);
        
        PDBTreeInsert(cs->opertree, PDBT_KEY_STR(opname, oplen), arr);
    } else {
        // catch all
        PDBTreeInsert(cs->opertree, 0, arr);
    }
}

void PDContentStreamAttachOperatorPairs(PDContentStreamRef cs, void *userInfo, const void **pairs)
{
    for (PDInteger i = 0; pairs[i]; i += 2) {
        PDContentStreamAttachOperator(cs, pairs[i], pairs[i+1], userInfo);
    }
}

PDBTreeRef PDContentStreamGetOperatorTree(PDContentStreamRef cs)
{
    return cs->opertree;
}

void PDContentStreamSetOperatorTree(PDContentStreamRef cs, PDBTreeRef operatorTree)
{
    PDAssert(operatorTree); // crash = null operatorTree which is not allowed
    PDRetain(operatorTree);
    PDRelease(cs->opertree);
    cs->opertree = operatorTree;
}

void PDContentStreamExecute(PDContentStreamRef cs)
{
    void **catchall;
    PDBool argValue;
    PDBool termed;
    char termChar;
    PDContentOperatorFunc op;
    PDContentStreamOperationRef operation;
    PDInteger strlen;
    PDInteger argc;
    void **arr;
    const char **args;
    char *str;
    pd_stack inStack, outStack;

    catchall = PDBTreeGet(cs->opertree, 0);
    termChar = 0;
    termed   = false;
    
    const char *stream = PDObjectGetStream(cs->ob);
    PDInteger   len    = PDObjectGetExtractedStreamLength(cs->ob);
    PDInteger   mark   = 0;

    pd_stack *opers = &cs->opers;
    pd_stack_destroy(opers);
    pd_array_clear(cs->args);

    for (PDInteger i = 0; i <= len; i++) {
        if (termChar) {
            termed = (i + 1 >= len || stream[i] == termChar);
            termChar *= !termed;
        }
        else if (i < len && i == mark && (stream[i] == '[' || stream[i] == '(')) {
            switch (stream[i]) {
                case '[': termChar = ']'; break;
                case '(': termChar = ')'; break;
            }
        }
        
        if (termChar == 0 && (termed || i == len || PDOperatorSymbolGlob[stream[i]] == PDOperatorSymbolGlobWhitespace)) {
            if (termed + i > mark) {
                strlen = termed + i - mark;
                str = strndup(&stream[mark], strlen);
                argValue = ((str[0] >= '0' && str[0] <= '9') || str[0] == '/' || str[0] == '.' || str[0] == '[' || str[0] == '(' || str[0] == '-' || str[0] == ']');
                arr = PDBTreeGet(cs->opertree, PDBT_KEY_STR(str, strlen));
                
                // if we did not get an operator, switch to catchall
                if (arr == NULL && ! argValue) arr = catchall;
                
                // have we matched a string to an operator?
                if (arr) {
                    argc     = pd_array_get_count(cs->args);
                    args     = pd_array_create_args(cs->args);
                    outStack = NULL;
                    inStack  = NULL;

                    if (cs->opers) {
                        operation = cs->opers->info;
                        inStack = operation->state;
                    }
                    
                    cs->lastOperator = str;
                    op = arr[0];
                    PDOperatorState state = (*op)(cs, arr[1], args, argc, inStack, &outStack);
                    
                    free(args);
                    pd_array_clear(cs->args);
                    
                    if (state == PDOperatorStatePush) {
                        operation = PDContentStreamOperationCreate(strdup(str), outStack);
                        pd_stack_push_object(opers, operation);
//                        pd_stack_push_key(opers, str);
                    } else if (state == PDOperatorStatePop) {
                        PDAssert(cs->opers != NULL); // crash = imbalanced push/pops (too many pops!)
                        PDRelease(pd_stack_pop_object(opers));
//                        free(pd_stack_pop_key(opers));
                    }
                }
                
                // we conditionally stuff arguments for numeric values and '/' somethings only; we do this to prevent a function from getting a ton of un-handled operators as arguments
                else if (argValue) {
                    pd_array_append(cs->args, str);
                } 
                
                // here, we believe we've run into an operator, so we throw away accumulated arguments and start anew
                else {
                    pd_array_clear(cs->args);
                }
                
                free(str);
            }
            
            mark = i + 1;
            termed = false;
        }
    }
}

const pd_stack PDContentStreamGetOperators(PDContentStreamRef cs)
{
    return cs->opers;
}

//typedef PDOperatorState (*PDContentOperatorFunc)(PDContentStreamRef cs, void *userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState);

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
 B          Fill and stroke path using nonzero winding number rule
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
 d          Set line dash pattern
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
 j          Set line join style
 J          Set line cap style
 K          Set CMYK color for stroking operations
 k          Set CMYK color for nonstroking operations
 l          Append straight line segment to path                                                *
 m          Begin new subpath                                                                   *
 M          Set miter limit
 MP         (PDF 1.2) Define marked-content point
 n          End path without filling or stroking                                                *
 q          Save graphics state                                                                 *
 Q          Restore graphics state                                                              *
 re         Append rectangle to path                                                            *
 RG         Set RGB color for stroking operations
 rg         Set RGB color for nonstroking operations
 ri         Set color rendering intent
 s          Close and stroke path
 S          Stroke path                                                                         *
 SC         (PDF 1.1) Set color for stroking operations
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
 W*         Set clipping path using even-odd rule
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

PDOperatorState PDContentStreamTextExtractor_Tj(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    // these should have a single string as arg but we won't whine if that's not the case
    for (PDInteger i = 0; i < argc; i++) {
        PDContentStreamTextExtractorPrint(userInfo, args[i]);
    }
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamTextExtractor_TJ(PDContentStreamRef cs, PDContentStreamTextExtractorUI userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    // these are arrays of strings and offsets; we don't care about offsets
    if (argc == 1 && args[0][0] == '[') {
        PDBool inParens = false;
        PDBool escaping = false;
        const char *s = args[0];
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
            if (args[i][0] == '(') 
                PDContentStreamTextExtractorPrint(userInfo, args[i]);
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

    return cs;
}

typedef struct PDContentStreamPrinterUI *PDContentStreamPrinterUIRef;
struct PDContentStreamPrinterUI {
    FILE *stream;
    PDInteger spacingIndex;
    char *spacing;
    PDReal posX, posY;
    PDReal startX, startY;
};

#define PDContentStreamPrinterPushSpacing(ui) ui->spacing[ui->spacingIndex++] = ' '; ui->spacing[ui->spacingIndex] = 0
#define PDContentStreamPrinterPopSpacing(ui)  ui->spacing[--ui->spacingIndex] = 0

PDOperatorState PDContentStreamPrinter_q(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sq  \tSave graphics state\n", userInfo->spacing);
    PDContentStreamPrinterPushSpacing(userInfo);
    return PDOperatorStatePush;
}

PDOperatorState PDContentStreamPrinter_Q(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    PDContentStreamPrinterPopSpacing(userInfo);
    fprintf(userInfo->stream, "%sQ  \tRestore graphics state\n", userInfo->spacing);
    return PDOperatorStatePop;
}

PDOperatorState PDContentStreamPrinter_re(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sre \tAppend rectangle to path: (%s, %s - %s, %s)\n", userInfo->spacing, args[0], args[1], args[2], args[3]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_w(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sw  \tSet line width: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_W(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sW  \tSet clipping path using nonzero winding number rule\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_n(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sn  \tEnd path without filling or stroking\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_cs(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%scs \t(PDF 1.1) Set color space for nonstroking operations: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_CS(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sCS \t(PDF 1.1) Set color space for stroking operations: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_scn(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sscn\t(PDF 1.2) Set color for nonstroking operations (ICCBased and special color spaces):", userInfo->spacing);
    for (PDInteger i = 0; i < argc; i++) 
        fprintf(userInfo->stream, " %s", args[i]);
    fputs("\n", userInfo->stream);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_SCN(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sSCN\t(PDF 1.2) Set color for stroking operations (ICCBased and special color spaces):", userInfo->spacing);
    for (PDInteger i = 0; i < argc; i++) 
        fprintf(userInfo->stream, " %s", args[i]);
    fputs("\n", userInfo->stream);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_sc(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%ssc \t(PDF 1.1) Set color for nonstroking operations: RGB=%s %s %s\n", userInfo->spacing, args[0], args[1], args[2]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_BT(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sBT \tBegin text object\n", userInfo->spacing);
    PDContentStreamPrinterPushSpacing(userInfo);
    return PDOperatorStatePush;
}

PDOperatorState PDContentStreamPrinter_Tm(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, 
            "%sTm \tSet text matrix and text line matrix: Tm = Tlm = [ %10s %10s 0 ]\n"
            "%s   \t                                                 [ %10s %10s 0 ]\n"
            "%s   \t                                                 [ %10s %10s 1 ]\n", userInfo->spacing, args[0], args[1], userInfo->spacing, args[2], args[3], userInfo->spacing, args[4], args[5]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tf(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTf \tSet text font and size: font = %s, size = %s\n", userInfo->spacing, args[0], args[1]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tj(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTj \tShow text:", userInfo->spacing);
    for (PDInteger i = 0; i < argc; i++) 
        fprintf(userInfo->stream, " %s", args[i]);
    fputs("\n", userInfo->stream);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_TJ(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTJ \tShow text, allowing individual glyph positioning:", userInfo->spacing);
    for (PDInteger i = 0; i < argc; i++) 
        fprintf(userInfo->stream, " %s", args[i]);
    fputs("\n", userInfo->stream);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_ET(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    PDContentStreamPrinterPopSpacing(userInfo);
    fprintf(userInfo->stream, "%sET \tEnd text object\n", userInfo->spacing);
    return PDOperatorStatePop;
}

PDOperatorState PDContentStreamPrinter_m(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sm  \tBegin new subpath: move to (%s,%s)\n", userInfo->spacing, args[0], args[1]);
    
    userInfo->startX = userInfo->posX = PDRealFromString(args[0]);
    userInfo->startY = userInfo->posY = PDRealFromString(args[1]);

    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_h(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sh  \tClose subpath: draw line from pos to start: (%.1f,%.1f) - (%.1f,%.1f)\n", userInfo->spacing, userInfo->posX, userInfo->posY, userInfo->startX, userInfo->startX);
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Td(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTd \tMove text position to start of next line with current line offset: (%s,%s)\n", userInfo->spacing, args[0], args[1]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_TD(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTD \tMove text position and set leading: (%s,%s)\n", userInfo->spacing, args[0], args[1]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tc(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTc \tSet character spacing: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tw(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sTc \tSet word spacing: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Tstar(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sT* \tMove to start of next text line\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_l(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sl  \tAppend straight line segment to path: line (%.1f,%.1f) - (%s,%s)\n", userInfo->spacing, userInfo->posX, userInfo->posY, args[0], args[1]);
    
    userInfo->posX = PDRealFromString(args[0]);
    userInfo->posY = PDRealFromString(args[1]);

    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_f(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sf  \tFill path using nonzero winding number rule\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_cm(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, 
            "%scm \tConcatenate matrix to current transformation matrix: [ %10s %10s 0 ]\n"
            "%s   \t                                                     [ %10s %10s 0 ]\n"
            "%s   \t                                                     [ %10s %10s 1 ]\n", userInfo->spacing, args[0], args[1], userInfo->spacing, args[2], args[3], userInfo->spacing, args[4], args[5]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_gs(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sgs \t(PDF 1.2) Set parameters from graphics state parameter dictionary: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_Do(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sDo \tInvoke named XObject: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_BX(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sBX \t(PDF 1.1) Begin compatibility section\n", userInfo->spacing);
    PDContentStreamPrinterPushSpacing(userInfo);
    return PDOperatorStatePush;
}

PDOperatorState PDContentStreamPrinter_EX(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    PDContentStreamPrinterPopSpacing(userInfo);
    fprintf(userInfo->stream, "%sEX \t(PDF 1.1) End compatibility section\n", userInfo->spacing);
    return PDOperatorStatePop;
}

PDOperatorState PDContentStreamPrinter_sh(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%ssh \t(PDF 1.3) Paint area defined by shading pattern: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_S(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sS  \tStroke path\n", userInfo->spacing);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_g(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sg  \tSet gray level for nonstroking operations: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_G(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sG  \tSet gray level for stroking operations: %s\n", userInfo->spacing, args[0]);
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_c(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sc  \tAppend curved segment to path (three control points): (%s,%s) - (%s,%s) - (%s,%s)\n", userInfo->spacing, args[0], args[1], args[2], args[3], args[4], args[5]);
    
    userInfo->posX = PDRealFromString(args[4]);
    userInfo->posY = PDRealFromString(args[5]);
    
    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_v(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sv  \tAppend curved segment to path (initial point replicated): (%.1f,%.1f) - (%s,%s) - (%s,%s)\n", userInfo->spacing, userInfo->posX, userInfo->posY, args[0], args[1], args[2], args[3]);
    
    userInfo->posX = PDRealFromString(args[2]);
    userInfo->posY = PDRealFromString(args[3]);

    return PDOperatorStateIndependent;
}

PDOperatorState PDContentStreamPrinter_y(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "%sy  \tAppend curved segment to path (final point replicated): (%s,%s) - (%.1f,%.1f) - (%s,%s)\n", userInfo->spacing, args[0], args[1], userInfo->posX, userInfo->posY, args[2], args[3]);
    
    userInfo->posX = PDRealFromString(args[2]);
    userInfo->posY = PDRealFromString(args[3]);
    
    return PDOperatorStateIndependent;
}

//--------------------------------

PDOperatorState PDContentStreamPrinter_catchall(PDContentStreamRef cs, PDContentStreamPrinterUIRef userInfo, const char **args, PDInteger argc, pd_stack inState, pd_stack *outState)
{
    fprintf(userInfo->stream, "[warning: missing implementation for operator \"%s\":", cs->lastOperator);
    for (PDInteger i = 0; i < argc; i++) 
        fprintf(userInfo->stream, " %s", args[i]);
    fputs("]\n", userInfo->stream);

    return PDOperatorStateIndependent;
}

PDContentStreamRef PDContentStreamCreateStreamPrinter(PDObjectRef object, FILE *stream)
{
    PDContentStreamRef cs = PDContentStreamCreateWithObject(object);
    PDContentStreamPrinterUIRef printerUI = malloc(sizeof(struct PDContentStreamPrinterUI));
    printerUI->stream = stream;
    printerUI->spacing = strdup("                                                                                                                                                                                                                                                                     ");
    printerUI->spacingIndex = 0;
    PDContentStreamPrinterPushSpacing(printerUI);
    
    PDContentStreamAttachOperator(cs, NULL, (PDContentOperatorFunc)PDContentStreamPrinter_catchall, printerUI);

#define pair2(name, suff) #name, PDContentStreamPrinter_##suff
#define pair(name) pair2(name, name) //#name, PDContentStreamPrinter_##name
    PDContentStreamAttachOperatorPairs(cs, printerUI, PDDef(pair(BT),
                                                            pair(cm),
                                                            pair(cs),
                                                            pair(CS),
                                                            pair(Do),
                                                            pair(ET),
                                                            pair(gs),
                                                            pair(h),
                                                            pair(l),
                                                            pair(m),
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
                                                            pair(TD),
                                                            pair(Tc),
                                                            pair(Tw),
                                                            pair2(T*, Tstar),
                                                            pair(c),
                                                            pair(v),
                                                            pair(y),
                                                            pair(TJ),
                                                            pair(Tm),
                                                            pair(W),
                                                            pair(w),
                                                            pair(BX),
                                                            pair(EX),
                                                            pair(sh),
                                                            pair(S),
                                                            pair(f),
                                                            pair(g),
                                                            pair(G)
                                                            ));
    return cs;
}
