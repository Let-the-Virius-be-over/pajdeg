//
//  PDPortableDocumentFormatState.c
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

#include "PDInternal.h"
#include "PDDefines.h"
#include "PDScanner.h"
#include "PDOperator.h"
#include "PDStack.h"
#include "PDPortableDocumentFormatState.h"
#include "PDPDFPrivate.h"
#include "PDStaticHash.h"
#include "PDBTree.h" // remove

PDInteger users = 0;
PDStateRef pdfRoot, xrefSeeker;

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

typedef void (*PDStringConverter)(PDStackRef*, PDStringConvRef);
void PDStringFromMeta(PDStackRef *s, PDStringConvRef scv);
void PDStringFromObj(PDStackRef *s, PDStringConvRef scv);
void PDStringFromRef(PDStackRef *s, PDStringConvRef scv);
void PDStringFromHexString(PDStackRef *s, PDStringConvRef scv);
void PDStringFromDict(PDStackRef *s, PDStringConvRef scv);
void PDStringFromDictEntry(PDStackRef *s, PDStringConvRef scv);
void PDStringFromArray(PDStackRef *s, PDStringConvRef scv);
void PDStringFromArrayEntry(PDStackRef *s, PDStringConvRef scv);
void PDStringFromArbitrary(PDStackRef *s, PDStringConvRef scv);
void PDStringFromName(PDStackRef *s, PDStringConvRef scv);

void PDPDFSetupConverters();
void PDPDFClearConverters();

//////////////////////////////////////////
//
// PDF parsing
//

void PDPortableDocumentFormatStateRetain()
{
    if (users == 0) {
#define s(name) name = PDStateCreate(#name)
        
        PDPortableDocumentFormatConversionTableRetain();
        
        s(pdfRoot);
        s(xrefSeeker);
        
        PDOperatorSymbolGlobSetup();
        
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
                                                   "Sendstream",
                                                   PDDef(PDOperatorPushComplex, &PD_ENDSTREAM),
                                                   "Sndstream",
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
                                                         PDOperatorPushResult, 
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
                                                   PDDef(//PDOperatorBreak,
                                                         PDOperatorAppendResult,
                                                         and PDOperatorPushWeakState, paren,
                                                         and PDOperatorPopValue),
                                                   "S)", 
                                                   PDDef(//PDOperatorBreak,
                                                         PDOperatorAppendResult, 
                                                         and PDOperatorPopState),
                                                   "F",
                                                   PDDef(//PDOperatorBreak,
                                                         PDOperatorPushbackSymbol,
                                                         and PDOperatorReadToDelimiter,
                                                         and PDOperatorAppendResult)));
        
        
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
                                                   PDDef(PDOperatorPushResult,
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
        
#undef s
#define s(s) PDStateRelease(s)
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
        PDStackRef s = NULL;
        PDStackRef o = NULL;
        PDBTreeRef seen = NULL;
        PDStackPushIdentifier(&s, (PDID)pdfRoot);
        PDStackPushIdentifier(&s, (PDID)xrefSeeker);
        PDStateRef t;
        PDOperatorRef p;
        while (NULL != (t = (PDStateRef)PDStackPopIdentifier(&s))) {
            PDBTreeInsert(&seen, t, t);
            printf("%s\n", t->name);
            for (PDInteger i = 0; i < t->symbols; i++)
                PDStackPushIdentifier(&o, (PDID)t->symbolOp[i]);
            if (t->numberOp) PDStackPushIdentifier(&o, (PDID)t->numberOp);
            if (t->delimiterOp) PDStackPushIdentifier(&o, (PDID)t->delimiterOp);
            if (t->fallbackOp) PDStackPushIdentifier(&o, (PDID)t->fallbackOp);

            while (NULL != (p = (PDOperatorRef)PDStackPopIdentifier(&o))) {
                while (p) {
                    assert(p->type < PDOperatorBreak);
                    assert(p->type > 0);
                    if (p->type == PDOperatorPushState && ! PDBTreeFetch(seen, p->pushedState)) {
                        PDBTreeInsert(&seen, t, t);
                        PDStackPushIdentifier(&s, (PDID)p->pushedState);
                    }
                    p = p->next;
                }
            }
        }
        PDBTreeDestroy(seen);
        PDStackDestroy(s);
#endif
    }
    users++;
}

void PDPortableDocumentFormatStateRelease()
{
    users--;
    if (users == 0) {
        PDStateRelease(pdfRoot);
        PDStateRelease(xrefSeeker);
        
        PDOperatorSymbolGlobClear();
        PDPortableDocumentFormatConversionTableRelease();
    }
}

PDInteger ctusers = 0;
void PDPortableDocumentFormatConversionTableRetain()
{
    if (ctusers == 0) {
        PDPDFSetupConverters();
    }
    ctusers++;
}

void PDPortableDocumentFormatConversionTableRelease()
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
    PDStaticHashRelease(converterTable);
    PDStaticHashRelease(typeTable);
    converterTable = NULL;
    typeTable = NULL;
}

char *PDStringFromComplex(PDStackRef *complex)
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
    PDAssert(typeTable); // crash = must PDPortableDocumentFormatConversionTableRetain() first
    return PDStaticHashValueForKeyAs(typeTable, *identifier, PDObjectType);
}

//////////////////////////////////////////
//
// PDF converter functions
//

void PDStringFromMeta(PDStackRef *s, PDStringConvRef scv)
{}

void PDStringFromObj(PDStackRef *s, PDStringConvRef scv)
{
    PDStringFromObRef("obj", 3);
}

void PDStringFromRef(PDStackRef *s, PDStringConvRef scv)
{
    PDStringFromObRef("R", 1);
}

void PDStringFromName(PDStackRef *s, PDStringConvRef scv)
{
    char *namestr = PDStackPopKey(s);
    PDInteger len = strlen(namestr);
    PDStringGrow(len + 2);
    currchi = '/';
    putstr(namestr, len);
}

void PDStringFromHexString(PDStackRef *s, PDStringConvRef scv)
{
    char *hexstr = PDStackPopKey(s);
    PDInteger len = strlen(hexstr);
    PDStringGrow(2 + len);
    currchi = '<';
    putstr(hexstr, len);
    currchi = '>';
}

void PDStringFromDict(PDStackRef *s, PDStringConvRef scv)
{
    PDStackRef entries;
    PDStackRef entry;

    PDStringGrow(30);
    
    currchi = '<';
    currchi = '<';
    currchi = ' ';
    
    PDStackAssertExpectedKey(s, PD_ENTRIES);
    entries = PDStackPopStack(s);
    for (entry = PDStackPopStack(&entries); entry; entry = PDStackPopStack(&entries)) {
        PDStringFromDictEntry(&entry, scv);
        PDStringGrow(3);
        currchi = ' ';
    }
    
    currchi = '>';
    currchi = '>';
}

void PDStringFromDictEntry(PDStackRef *s, PDStringConvRef scv)
{
    char *key;
    PDInteger req = 30;
    PDInteger len;
    
    PDStackAssertExpectedKey(s, PD_DE);
    key = PDStackPopKey(s);
    len = strlen(key);
    req = 10 + len;
    PDStringGrow(req);
    currchi = '/';
    putstr(key, len);
    currchi = ' ';
    PDStringFromAnything();
}

void PDStringFromArray(PDStackRef *s, PDStringConvRef scv)
{
    PDStackRef entries;
    PDStackRef entry;

    PDStringGrow(10);
    
    currchi = '[';
    currchi = ' ';
    
    PDStackAssertExpectedKey(s, PD_ENTRIES);
    entries = PDStackPopStack(s);
    
    for (entry = PDStackPopStack(&entries); entry; entry = PDStackPopStack(&entries)) {
        PDStringFromArrayEntry(&entry, scv);
        PDStringGrow(2);
        currchi = ' ';
    }
    
    currchi = ']';
}

void PDStringFromArrayEntry(PDStackRef *s, PDStringConvRef scv)
{
    PDStackAssertExpectedKey(s, PD_AE);
    PDStringFromAnything();
}

void PDStringFromArbitrary(PDStackRef *s, PDStringConvRef scv)
{
    PDID type = PDStackPopIdentifier(s);
    PDInteger hash = PDStaticHashIdx(converterTable, *type);
    (*as(PDStringConverter, PDStaticHashValueForHash(converterTable, hash)))(s, scv);
}
