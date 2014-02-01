//
// pd_pdf_implementation.c
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

#include "pd_internal.h"
#include "PDDefines.h"
#include "PDScanner.h"
#include "PDOperator.h"
#include "pd_stack.h"
#include "pd_pdf_implementation.h"
#include "pd_pdf_private.h"
#include "PDStaticHash.h"
#include "PDStreamFilterFlateDecode.h"
#include "PDStreamFilterPrediction.h"

void PDDeallocatorNullFunc(void *ob) {}

PDInteger users = 0;
PDStateRef pdfRoot, xrefSeeker, stringStream, arbStream;

const char * PD_META       = "meta";
const char * PD_NAME       = "name";
const char * PD_OBJ        = "obj";
const char * PD_REF        = "ref";
const char * PD_HEXSTR     = "hexstr";
const char * PD_ENTRIES    = "entries";
const char * PD_DICT       = "dict";
const char * PD_DE         = "de";
const char * PD_ARRAY      = "array";
const char * PD_AE         = "ae";
const char * PD_XREF       = "xref";
const char * PD_STARTXREF  = "startxref";
const char * PD_ENDSTREAM  = "endstream";

//////////////////////////////////////////
//
// PDF complex object conversion
//

typedef void (*PDStringConverter)(pd_stack*, PDStringConvRef);
void PDStringFromMeta(pd_stack *s, PDStringConvRef scv);
void PDStringFromObj(pd_stack *s, PDStringConvRef scv);
void PDStringFromRef(pd_stack *s, PDStringConvRef scv);
void PDStringFromHexString(pd_stack *s, PDStringConvRef scv);
void PDStringFromDict(pd_stack *s, PDStringConvRef scv);
void PDStringFromDictEntry(pd_stack *s, PDStringConvRef scv);
void PDStringFromArray(pd_stack *s, PDStringConvRef scv);
void PDStringFromArrayEntry(pd_stack *s, PDStringConvRef scv);
void PDStringFromArbitrary(pd_stack *s, PDStringConvRef scv);
void PDStringFromName(pd_stack *s, PDStringConvRef scv);

void PDPDFSetupConverters();
void PDPDFClearConverters();

//////////////////////////////////////////
//
// PDF parsing
//

void pd_pdf_implementation_use()
{
    static PDBool first = true;
    if (first) {
        first = false;
        // register predictor handler, even if flate decode is not available
        PDStreamFilterRegisterDualFilter("Predictor", PDStreamFilterPredictionConstructor);
#ifdef PD_SUPPORT_ZLIB
        // register FlateDecode handler
        PDStreamFilterRegisterDualFilter("FlateDecode", PDStreamFilterFlateDecodeConstructor);
#endif
        // set null deallocator
        PDDeallocatorNull = PDDeallocatorNullFunc;
    }
    
    if (users == 0) {
        
        pd_pdf_conversion_use();
        
        PDOperatorSymbolGlobSetup();

#define s(name) name = PDStateCreate(#name)
        
        s(pdfRoot);
        s(xrefSeeker);
        s(stringStream);
        s(arbStream);
        
        PDStateRef s(comment_or_meta);     // %anything (comment) or %%EOF (meta)
        PDStateRef s(object_reference);    // "obid genid reftype"
        PDStateRef s(dict_hex);            // dictionary (<<...>>) or hex string (<A-F0-9.*>)
        PDStateRef s(dict);                // dictionary
        PDStateRef s(name_str);            // "/" followed by symbol, potentially wrapped in parentheses
        PDStateRef s(dict_hex_term);       // requires '>' termination
        PDStateRef s(name);                // "/" followed by a name_str, as a proper "complex"
        PDStateRef s(paren);               // "(" followed by any number of nested "(" and ")" ending with a ")"
        PDStateRef s(arb);                 // arbitrary value (number, array, dict, etc)
        PDStateRef s(number_or_obref);     // number OR number number reftype
        PDStateRef s(number);              // number
        PDStateRef s(array);               // array ([ arb arb arb ... ])
        PDStateRef s(xref);                // xref table state
        PDStateRef s(end_numeric);         // for xref seeker, when a number is encountered, find out who refers to it
        
        //pdfArrayRoot = array;
        
        // this #define is disgusting, but it removes two billion
        // 'initialization makes pointer from integer without a cast'
        // warnings outside of gnu99 mode
#define and (void*)

        //
        // PDF root
        // This begins the PDF environment, and detects objects, their definitions, their streams, etc.
        //
        
        pdfRoot->iterates = true;
        PDStateDefineOperatorsWithDefinition(pdfRoot, 
                                             PDDef("S%", 
                                                   PDDef(PDOperatorPushState, comment_or_meta),
                                                   "F",
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorPushState, arb),
                                                   "Sstream",
                                                   PDDef(PDOperatorPushResult),
                                                   
                                                   // endstream, ndstream, or dstream; this is all depending on how the stream length was defined; some PDF writers say that the length is from the end of "stream" to right before the "endstrean", which includes newlines; other PDF writers exclude the initial newline after "stream" in the length; these will come out as "ndstream" here; if they are visitors from the past and use DOS newline formatting (\r\n), they will come out as "dstream"
                                                   // one might fear that this would result in extraneous "e" or "en" in the written stream, but this is never the case, because Pajdeg either passes everything through to the output as is (in which case it will come out as it was), or Pajdeg replaces the stream, in which case it writes its own "endstream" keyword
                                                   "Sendstream",
                                                   PDDef(PDOperatorPushComplex, &PD_ENDSTREAM),
                                                   "Sndstream",
                                                   PDDef(PDOperatorPushComplex, &PD_ENDSTREAM),
                                                   "Sdstream",
                                                   PDDef(PDOperatorPushComplex, &PD_ENDSTREAM),
                                                   "Sxref",
                                                   PDDef(PDOperatorPushState, xref),
                                                   "Strailer",
                                                   PDDef(PDOperatorPushResult),
                                                   "Sstartxref",
                                                   PDDef(PDOperatorPushState, number,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_STARTXREF),
                                                   "Sendobj",
                                                   PDDef(PDOperatorPushResult))
                                             );
        
        //
        // comment or meta
        //
        
        PDStateDefineOperatorsWithDefinition(comment_or_meta, 
                                             PDDef("S%",
                                                   PDDef(PDOperatorPopLine,
                                                         and PDOperatorPushResult,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_META,
                                                         and PDOperatorPopState),
                                                   "F", 
                                                   PDDef(PDOperatorPopLine,
                                                         and PDOperatorPopState))
                                             );
        
        //
        // arb: arbitrary value
        //
        
        PDStateDefineOperatorsWithDefinition(arb, 
                                             PDDef("N",
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPushState, number_or_obref,
                                                         and PDOperatorPopState),
                                                   "Strue",
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPopState),
                                                   "Sfalse",
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPopState),
                                                   "Snull",
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPopState),
                                                   "S(",
                                                   PDDef(//PDOperatorBreak,
                                                         PDOperatorMark,
                                                         and PDOperatorPushState, paren,
                                                         and PDOperatorPopState),
                                                   "S[",
                                                   PDDef(PDOperatorPushState, array,
                                                         and PDOperatorPopState),
                                                   "S/",
                                                   PDDef(//PDOperatorBreak,
                                                         PDOperatorPushState, name_str,
                                                         and PDOperatorPushState, name,
                                                         and PDOperatorPopState),
                                                   "S<",
                                                   PDDef(PDOperatorPushState, dict_hex,
                                                         and PDOperatorPopState)));
        
        
        //
        // xref state
        //
        
        xref->iterates = true;
        PDStateDefineOperatorsWithDefinition(xref, 
                                             PDDef("N", 
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPushWeakState, number,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_XREF),
                                                   "F",
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorPopState)));

        //
        // number: a number
        //
        
        PDStateDefineOperatorsWithDefinition(number, 
                                             PDDef("N",
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPopState)));

        //
        // number or an obref
        //
        
        PDStateDefineOperatorsWithDefinition(number_or_obref, 
                                             PDDef("N",
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPushState, object_reference,
                                                         and PDOperatorPopState),
                                                   "F",
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorPopState)));
        
        //
        // paren: something that ends with ")" and optionally contains nested ()s
        //

        PDStateDefineOperatorsWithDefinition(paren, 
                                             PDDef("S(", 
                                                   PDDef(PDOperatorPushWeakState, paren,
                                                         and PDOperatorPopValue),
                                                   "S)", 
                                                   PDDef(PDOperatorPushMarked,
                                                         and PDOperatorPopState),
                                                   "F",
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorReadToDelimiter)));
        
        
        //
        // array
        //

        PDStateDefineOperatorsWithDefinition(array, 
                                             PDDef("S]", 
                                                   PDDef(PDOperatorPullBuildVariable, &PD_ENTRIES,
                                                         and PDOperatorPushComplex, &PD_ARRAY,
                                                         and PDOperatorPopState),
                                                   "F",
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorPushWeakState, arb,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorStoveComplex, &PD_AE)));
        

        //
        // name_str: a symbol, or a delimiter that initiates a symbol
        //
        
        PDStateDefineOperatorsWithDefinition(name_str, 
                                             PDDef("S(", 
                                                   PDDef(PDOperatorMark,
                                                         and PDOperatorPushWeakState, paren,
                                                         and PDOperatorPopState),
                                                   "F",
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPopState)));
        
        //
        // name: a symbol, or a delimiter that initiates a symbol
        //

        PDStateDefineOperatorsWithDefinition(name, 
                                             PDDef(/*"S(", 
                                                    PDDef(PDOperatorPushResult,
                                                    PDOperatorPushState, paren,
                                                    PDOperatorPopValue,
                                                    PDOperatorPushComplex, &PD_NAME,
                                                    PDOperatorPopState),*/
                                                   "F",
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_NAME,
                                                         and PDOperatorPopState)));
        
        
        
        //
        // dict_hex: enters "dict" for "<" and falls back to hex otherwise
        //
        
        PDStateDefineOperatorsWithDefinition(dict_hex, 
                                             PDDef("S<",
                                                   PDDef(PDOperatorPushState, dict,
                                                         and PDOperatorPopState),
                                                   "S>", 
                                                   PDDef(PDOperatorPushEmptyString,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_HEXSTR,
                                                         and PDOperatorPopState),
                                                   "F",
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorReadToDelimiter,
                                                         and PDOperatorPushResult,
                                                         and PDOperatorPushState, dict_hex_term,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_HEXSTR,
                                                         and PDOperatorPopState)));
        
        //
        // object references (<id> <gen> <type>)
        //
        
        // object reference; expects results stack to hold genid and obid, and expects "obj" or "R" as the symbol
        PDStateDefineOperatorsWithDefinition(object_reference, 
                                             PDDef("Sobj",
                                                   PDDef(PDOperatorPopValue,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_OBJ,
                                                         and PDOperatorPopState),
                                                   "SR", 
                                                   PDDef(PDOperatorPopValue,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_REF,
                                                         and PDOperatorPopState),
                                                   "F", 
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorPushbackValue,
                                                         and PDOperatorPopState))); // this was not an obj ref
        
        
        //
        // dict: reads pairs of /name <arbitrary> /name ... until '>>'
        //
        
        PDStateDefineOperatorsWithDefinition(dict, 
                                             PDDef("S>",
                                                   PDDef(PDOperatorPushWeakState, dict_hex_term,
                                                         and PDOperatorPullBuildVariable, &PD_ENTRIES,
                                                         and PDOperatorPushComplex, &PD_DICT,
                                                         and PDOperatorPopState),
                                                   "S/",
                                                   PDDef(PDOperatorPushWeakState, name_str,
                                                         and PDOperatorPushWeakState, arb,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorPopValue,
                                                         and PDOperatorStoveComplex, &PD_DE)));

        //
        // dict_hex_term: expects '>'
        //
        
        PDStateDefineOperatorsWithDefinition(dict_hex_term, 
                                             PDDef("S>",
                                                   PDDef(PDOperatorPopState)));
        
        //
        // STRING STREAM
        //
        
        stringStream->iterates = true;
        PDStateDefineOperatorsWithDefinition(stringStream, 
                                             PDDef("F",
                                                   PDDef(PDOperatorPushResult)));
        
        //
        // ARBITRARY STREAM
        //
        
        arbStream->iterates = true;
        PDStateDefineOperatorsWithDefinition(arbStream, 
                                             PDDef("F",
                                                   PDDef(PDOperatorPushbackSymbol,
                                                         and PDOperatorPushState, arb)));
        
        // 
        // XREF SEEKER
        // This is a tiny environment used to discover the initial XREF
        //
        
        xrefSeeker->iterates = true;
        PDStateDefineOperatorsWithDefinition(xrefSeeker, 
                                             PDDef("F", 
                                                   PDDef(PDOperatorNOP),
                                                   "N",
                                                   PDDef(PDOperatorPushResult,
                                                         and PDOperatorPushState, end_numeric),
                                                   "S>",
                                                   PDDef(PDOperatorPopState)));
        
        
        //
        // startxref grabber for xrefSeeker
        //
        
        PDStateDefineOperatorsWithDefinition(end_numeric, 
                                             PDDef("Sstartxref", 
                                                   PDDef(PDOperatorPopValue,
                                                         and PDOperatorPushComplex, &PD_STARTXREF,
                                                         and PDOperatorPopState)));
        
        
        PDStateCompile(pdfRoot);
        
        PDStateCompile(xrefSeeker);
        
        PDStateCompile(arbStream);
        
        PDStateCompile(stringStream);
        
#undef s
#define s(s) PDRelease(s)
        s(comment_or_meta);
        s(object_reference);
        s(dict_hex);
        s(name_str);
        s(dict);
        s(dict_hex_term);
        s(name);
        s(paren);
        s(arb);
        s(number_or_obref);
        s(number);
        s(array);
        s(xref);
        s(end_numeric);
        
#if 0
        pd_stack s = NULL;
        pd_stack o = NULL;
        pd_btree seen = NULL;
        pd_stack_push_identifier(&s, (PDID)pdfRoot);
        pd_stack_push_identifier(&s, (PDID)xrefSeeker);
        PDStateRef t;
        PDOperatorRef p;
        while (NULL != (t = (PDStateRef)pd_stack_pop_identifier(&s))) {
            pd_btree_insert(&seen, t, t);
            printf("%s\n", t->name);
            for (PDInteger i = 0; i < t->symbols; i++)
                pd_stack_push_identifier(&o, (PDID)t->symbolOp[i]);
            if (t->numberOp) pd_stack_push_identifier(&o, (PDID)t->numberOp);
            if (t->delimiterOp) pd_stack_push_identifier(&o, (PDID)t->delimiterOp);
            if (t->fallbackOp) pd_stack_push_identifier(&o, (PDID)t->fallbackOp);

            while (NULL != (p = (PDOperatorRef)pd_stack_pop_identifier(&o))) {
                while (p) {
                    assert(p->type < PDOperatorBreak);
                    assert(p->type > 0);
                    if (p->type == PDOperatorPushState && ! pd_btree_fetch(seen, p->pushedState)) {
                        pd_btree_insert(&seen, t, t);
                        pd_stack_push_identifier(&s, (PDID)p->pushedState);
                    }
                    p = p->next;
                }
            }
        }
        pd_btree_destroy(seen);
        pd_stack_destroy(&s);
#endif
    }
    users++;
}

void pd_pdf_implementation_discard()
{
    users--;
    if (users == 0) {
        PDRelease(pdfRoot);
        PDRelease(xrefSeeker);
        PDRelease(arbStream);
        PDRelease(stringStream);
        
        PDOperatorSymbolGlobClear();
        pd_pdf_conversion_discard();
    }
}

PDInteger ctusers = 0;
void pd_pdf_conversion_use()
{
    if (ctusers == 0) {
        PDPDFSetupConverters();
    }
    ctusers++;
}

void pd_pdf_conversion_discard()
{
    ctusers--;
    if (ctusers == 0) { 
        PDPDFClearConverters();
    }
}

static PDStaticHashRef converterTable = NULL;
static PDStaticHashRef typeTable = NULL;

#define converterTableHash(key) PDStaticHashIdx(converterTable, key)

void PDPDFSetupConverters()
{
    if (converterTable) {
        return;
    }
    
    converterTable = PDStaticHashCreate(9, (void*[]) {
        (void*)PD_META,     // 1
        (void*)PD_OBJ,      // 2
        (void*)PD_REF,      // 3
        (void*)PD_HEXSTR,   // 4
        (void*)PD_DICT,     // 5
        (void*)PD_DE,       // 6 
        (void*)PD_ARRAY,    // 7
        (void*)PD_AE,       // 8
        (void*)PD_NAME,     // 9
    }, (void*[]) {
        &PDStringFromMeta,          // 1
        &PDStringFromObj,           // 2
        &PDStringFromRef,           // 3
        &PDStringFromHexString,     // 4
        &PDStringFromDict,          // 5
        &PDStringFromDictEntry,     // 6
        &PDStringFromArray,         // 7
        &PDStringFromArrayEntry,    // 8
        &PDStringFromName,          // 9
    });
    
    PDStaticHashDisownKeysValues(converterTable, true, true);
    
    typeTable = PDStaticHashCreate(9, (void*[]) {
        (void*)PD_META,     // 1
        (void*)PD_OBJ,      // 2
        (void*)PD_REF,      // 3
        (void*)PD_HEXSTR,   // 4
        (void*)PD_DICT,     // 5
        (void*)PD_DE,       // 6 
        (void*)PD_ARRAY,    // 7
        (void*)PD_AE,       // 8
        (void*)PD_NAME,     // 9
    }, (void*[]) {
        (void*)PDObjectTypeString,
        (void*)PDObjectTypeString,
        (void*)PDObjectTypeString,
        (void*)PDObjectTypeString,
        (void*)PDObjectTypeDictionary,
        (void*)PDObjectTypeString,
        (void*)PDObjectTypeArray,
        (void*)PDObjectTypeString,
        (void*)PDObjectTypeName,
    });

    PDStaticHashDisownKeysValues(typeTable, true, true);

}

void PDPDFClearConverters()
{
    PDRelease(converterTable);
    PDRelease(typeTable);
    converterTable = NULL;
    typeTable = NULL;
}

char *PDStringFromComplex(pd_stack *complex)
{
    struct PDStringConv scv = (struct PDStringConv) {malloc(30), 0, 30};
    
    // generate
    PDStringFromArbitrary(complex, &scv);
    
    // null terminate
    if (scv.left < 1) scv.allocBuf = realloc(scv.allocBuf, scv.offs + 1);
    scv.allocBuf[scv.offs] = 0;
    
    return scv.allocBuf;
}

PDObjectType PDObjectTypeFromIdentifier(PDID identifier)
{
    PDAssert(typeTable); // crash = must pd_pdf_conversion_use() first
    return PDStaticHashValueForKeyAs(typeTable, *identifier, PDObjectType);
}

//////////////////////////////////////////
//
// PDF converter functions
//

void PDStringFromMeta(pd_stack *s, PDStringConvRef scv)
{}

void PDStringFromObj(pd_stack *s, PDStringConvRef scv)
{
    PDStringFromObRef("obj", 3);
}

void PDStringFromRef(pd_stack *s, PDStringConvRef scv)
{
    PDStringFromObRef("R", 1);
}

void PDStringFromName(pd_stack *s, PDStringConvRef scv)
{
    char *namestr = pd_stack_pop_key(s);
    PDInteger len = strlen(namestr);
    PDStringGrow(len + 2);
    currchi = '/';
    putstr(namestr, len);
    PDDeallocateViaStackDealloc(namestr);
}

void PDStringFromHexString(pd_stack *s, PDStringConvRef scv)
{
    char *hexstr = pd_stack_pop_key(s);
    PDInteger len = strlen(hexstr);
    PDStringGrow(2 + len);
    currchi = '<';
    putstr(hexstr, len);
    currchi = '>';
    PDDeallocateViaStackDealloc(hexstr);
}

void PDStringFromDict(pd_stack *s, PDStringConvRef scv)
{
    pd_stack entries;
    pd_stack entry;

    PDStringGrow(30);
    
    currchi = '<';
    currchi = '<';
    currchi = ' ';
    
    pd_stack_assert_expected_key(s, PD_ENTRIES);
    entries = pd_stack_pop_stack(s);
    for (entry = pd_stack_pop_stack(&entries); entry; entry = pd_stack_pop_stack(&entries)) {
        PDStringFromDictEntry(&entry, scv);
        PDStringGrow(3);
        currchi = ' ';
    }
    
    currchi = '>';
    currchi = '>';
}

void PDStringFromDictEntry(pd_stack *s, PDStringConvRef scv)
{
    char *key;
    PDInteger req = 30;
    PDInteger len;
    
    pd_stack_assert_expected_key(s, PD_DE);
    key = pd_stack_pop_key(s);
    len = strlen(key);
    req = 10 + len;
    PDStringGrow(req);
    currchi = '/';
    putstr(key, len);
    currchi = ' ';
    PDStringFromAnything();
    PDDeallocateViaStackDealloc(key);
}

void PDStringFromArray(pd_stack *s, PDStringConvRef scv)
{
    pd_stack entries;
    pd_stack entry;

    PDStringGrow(10);
    
    currchi = '[';
    currchi = ' ';
    
    pd_stack_assert_expected_key(s, PD_ENTRIES);
    entries = pd_stack_pop_stack(s);
    
    for (entry = pd_stack_pop_stack(&entries); entry; entry = pd_stack_pop_stack(&entries)) {
        PDStringFromArrayEntry(&entry, scv);
        PDStringGrow(2);
        currchi = ' ';
    }
    
    currchi = ']';
}

void PDStringFromArrayEntry(pd_stack *s, PDStringConvRef scv)
{
    pd_stack_assert_expected_key(s, PD_AE);
    PDStringFromAnything();
}

void PDStringFromArbitrary(pd_stack *s, PDStringConvRef scv)
{
    PDID type = pd_stack_pop_identifier(s);
    PDInteger hash = PDStaticHashIdx(converterTable, *type);
    (*as(PDStringConverter, PDStaticHashValueForHash(converterTable, hash)))(s, scv);
}

PDDeallocator PDDeallocatorNull;
