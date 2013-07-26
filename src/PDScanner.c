//
//  PDScanner.c
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

#include "Pajdeg.h"
#include "PDScanner.h"
#include "PDOperator.h"
#include "PDState.h"
#include "PDInternal.h"
#include "PDEnv.h"
#include "PDStack.h"
#include "PDPortableDocumentFormatState.h" // <-- not ideal

static PDInteger PDScannerScanAttemptCap = -1;
static PDScannerBufFunc bufFunc = NULL;
static void *bufFuncInfo;
static PDStackRef scannerContextStack = NULL;

void PDScannerOperate(PDScannerRef scanner, PDOperatorRef op);
void PDScannerScan(PDScannerRef scanner);

PDScannerRef PDScannerCreateWithStateAndPopFunc(PDStateRef state, PDScannerPopFunc popFunc)
{
    PDScannerRef scanner = calloc(1, sizeof(struct PDScanner));
    scanner->env = PDEnvCreateWithState(state);
    scanner->popFunc = popFunc;
    return scanner;
}

PDScannerRef PDScannerCreateWithState(PDStateRef state)
{
    return PDScannerCreateWithStateAndPopFunc(state, &PDScannerPopSymbol);
}

void PDScannerContextPush(void *ctxInfo, PDScannerBufFunc ctxBufFunc)
{
    if (bufFunc) {
        PDStackPushIdentifier(&scannerContextStack, (PDID)bufFunc);
        PDStackPushIdentifier(&scannerContextStack, (PDID)bufFuncInfo);
    }
    bufFunc = ctxBufFunc;
    bufFuncInfo = ctxInfo;
}

void PDScannerContextPop(void)
{
    bufFuncInfo = PDStackPopIdentifier(&scannerContextStack);
    bufFunc = (PDScannerBufFunc)PDStackPopIdentifier(&scannerContextStack);
}

void PDScannerSetLoopCap(PDInteger cap)
{
    PDScannerScanAttemptCap = cap;
}

void PDScannerDestroy(PDScannerRef scanner)
{
    if (scanner->env) 
        PDEnvDestroy(scanner->env);

    PDStackDestroy(scanner->envStack);
    PDStackDestroy(scanner->resultStack);
    PDStackDestroy(scanner->symbolStack);
    PDStackDestroy(scanner->garbageStack);

    free(scanner->sym);
    free(scanner);
}

void PDScannerAlign(PDScannerRef scanner, PDOffset offset)
{
    PDStackRef s;
    PDScannerSymbolRef sym;
    
    scanner->buf += offset;
    
    // can most likely skip fake checks entirely as it only happens during a scan, not after, but what if a fake symbol is the final symbol in the list after scan? hm
    sym = scanner->sym;
    if (sym && (sym->stype ^ PDOperatorSymbolExtFake)) sym->sstart += offset;
    for (s = scanner->symbolStack; s; s = s->prev) {
        sym = s->info;
        if (sym->stype ^ PDOperatorSymbolExtFake)
            sym->sstart += offset;
    }
}

void PDScannerTrim(PDScannerRef scanner, PDOffset bytes)
{
    if (bytes > scanner->bsize) {
        // we skipped content and scanner iterated beyond its buffer, so we reset
        PDScannerReset(scanner);
        return;
    }
    if (bytes > 0) {
        scanner->bsize -= bytes;
        scanner->boffset -= bytes;
        scanner->buf += bytes;
    }
}

void PDScannerReset(PDScannerRef scanner)
{
    scanner->boffset = scanner->bsize = 0;
    // scanner->btrail = 0;
    scanner->buf = NULL;
    PDStackDestroy(scanner->symbolStack);
    PDStackDestroy(scanner->resultStack);
}

void PDScannerSkip(PDScannerRef scanner, PDSize bytes)
{
    //scanner->bsize += bytes;
    scanner->boffset += bytes;
    //scanner->btrail = scanner->boffset;
}

void PDScannerPopSymbol(PDScannerRef scanner)
{
    if (scanner->symbolStack) {
        // a symbol on stack is ready for use, so we use that
        if (scanner->sym) free(scanner->sym);
        scanner->sym = PDStackPopFreeable(&scanner->symbolStack);
        return;
    }
    
    PDScannerSymbolRef sym;
    unsigned char c;
    char *buf;
    short hash;
    PDInteger   bsize, len, i;
    char  prevtype, type;
    PDBool numeric, real, escaped;
    
    if (scanner->bsize < scanner->boffset) {
        // we aren't anchored to the source anymore, because we've iterated beyond "sight", so we reset, which means the source will set us up from scratch the next time we pull
        PDScannerReset(scanner);
    }
    
    sym = scanner->sym;
    if (NULL == sym) 
        scanner->sym = sym = malloc(sizeof(struct PDScannerSymbol));
    buf = scanner->buf;
    bsize = scanner->bsize;
    i = scanner->boffset;
    len = 0;
    hash = 0;
    numeric = true;
    real = false;
    escaped = false;
    
    prevtype = type = PDOperatorSymbolGlobWhitespace;

    // we want to move past whitespace, and we want to stop immediately on (prev=) delimiter, and we want to parse until the end of regular
    while (true) {
        if (bsize <= i) {
            (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, 0);
            if (bsize <= i) break;
        }
        prevtype = type;
        c = buf[i];
        type = escaped ? PDOperatorSymbolGlobRegular : PDOperatorSymbolGlob[c];
        escaped = !escaped && c == '\\';
        
        if (prevtype != PDOperatorSymbolGlobDelimiter && (prevtype == PDOperatorSymbolGlobWhitespace || prevtype == type)) {
            if (type != PDOperatorSymbolGlobWhitespace) {
                len ++;
                hash -= (type - 1) * c;
                PDSymbolUpdateNumeric(numeric, real, c, len == 1);
                /*numeric &= ((c >= '0' && c <= '9')
                            ||
                            (len == 1 && (c == '-' || c == '+'))
                            ||
                            (! real && (real |= c == '.')));*/
            }
        } else break;
        i++;
    }
    sym->sstart = buf + i - len;
    sym->slen = len;
    sym->stype = len == 0 ? PDOperatorSymbolExtEOB : prevtype == PDOperatorSymbolGlobRegular && numeric ? PDOperatorSymbolExtNumeric : prevtype;
    sym->shash = 10 * abs(hash) + len;

    scanner->buf = buf;
    scanner->bsize = bsize;
    scanner->boffset = i;
    
    // we also want to bump offset past whitespace
    while (scanner->boffset < scanner->bsize && PDOperatorSymbolGlob[(unsigned char)scanner->buf[scanner->boffset]] == PDOperatorSymbolGlobWhitespace) 
        scanner->boffset++;
    
#ifdef DEBUG_SCANNER_SYMBOLS
    char *str = strndup(sym->sstart, sym->slen);
    printf("\t\t\t★ %s ★\n", str);
    free(str);
#endif
}

void PDScannerPopSymbolRev(PDScannerRef scanner)
{
    if (scanner->symbolStack) {
        // a symbol on stack is ready for use, so we use that
        if (scanner->sym) free(scanner->sym);
        scanner->sym = PDStackPopFreeable(&scanner->symbolStack);
        return;
    }
    
    PDScannerSymbolRef sym;
    unsigned char c;
    char *buf;
    short hash;
    PDInteger   bsize, len, i;
    char  prevtype, type;
    PDBool numeric;
    
    sym = scanner->sym;
    if (NULL == sym) 
        scanner->sym = sym = malloc(sizeof(struct PDScannerSymbol));
    buf = scanner->buf;
    bsize = scanner->bsize;
    i = scanner->boffset;
    len = 0;
    hash = 0;
    numeric = true;
    
    prevtype = type = PDOperatorSymbolGlobWhitespace;
    
    // we want to move past whitespace, and we want to stop immediately on (prev=) delimiter, and we want to parse until the end of regular
    while (true) {
        if (i <= 0) {
            (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, 0);
            if (bsize <= 1) break;
            i = bsize - 1;
        }
        prevtype = type;
        c = buf[i];
        type = PDOperatorSymbolGlob[c];
        
        if (prevtype != PDOperatorSymbolGlobDelimiter && (prevtype == PDOperatorSymbolGlobWhitespace || prevtype == type)) {
            if (type != PDOperatorSymbolGlobWhitespace) {
                len ++;
                hash -= (type - 1) * c;
                numeric &= c >= '0' && c <= '9'; // we do not go to similar extents here
            }
        } else break;
        i--;
    }
    sym->sstart = buf + i + 1;
    sym->slen = len;
    sym->stype = prevtype == PDOperatorSymbolGlobRegular && numeric ? PDOperatorSymbolExtNumeric : prevtype;
    sym->shash = 10 * abs(hash) + len;
    
    scanner->buf = buf;
    scanner->bsize = bsize;
    scanner->boffset = i;
    
    //char *str = strndup(sym->sstart, sym->slen);
    //printf("popped rev symbol: %s\n", str);
    //free(str);
}

void PDScannerReadUntilDelimiter(PDScannerRef scanner, PDBool delimiterIsNewline)
{
    char *buf;
    PDInteger   bsize, i;
    PDBool escaped;
    PDScannerSymbolRef sym = scanner->sym;
    i = scanner->boffset;
    escaped = false;

    // if we have a symbol stack we want to pop it all and rewind back to where it was, or we will end up skipping content; we do not reset 'i', however, as we don't want to SCAN from the start, we just want to include everything from the start
    if (scanner->symbolStack) {
        while (scanner->symbolStack) {
            if (sym) free(sym);
            sym = PDStackPopFreeable(&scanner->symbolStack);
        }
        scanner->sym = sym;
        scanner->boffset = sym->sstart - scanner->buf;
    }
    buf = scanner->buf;
    bsize = scanner->bsize;
    while (true) {
        if (bsize <= i) {
            (*bufFunc)(bufFuncInfo, scanner, &buf,  &bsize, 0);
            if (bsize <= i) break;
        }
        if (! escaped && 
            ((delimiterIsNewline && (buf[i] == '\n' || buf[i] == '\r')) ||
             (!delimiterIsNewline && PDOperatorSymbolGlob[(unsigned char)buf[i]] == PDOperatorSymbolGlobDelimiter)))
            break;
        escaped = !escaped && buf[i] == '\\';
        i++;
    }
    
    if (NULL == sym) 
        scanner->sym = sym = malloc(sizeof(struct PDScannerSymbol));
    
    sym->sstart = buf + scanner->boffset;
    sym->slen = i - scanner->boffset;
    sym->stype = PDOperatorSymbolGlobRegular;
    sym->shash = 0;

    scanner->buf = buf;
    scanner->bsize = bsize;
    
    // absorb whitespace if any
    while (PDOperatorSymbolGlob[(unsigned char)buf[i]] == PDOperatorSymbolGlobWhitespace) {
        i++;
        if (bsize <= i) {
            (*bufFunc)(bufFuncInfo, scanner, &buf,  &bsize, 0);
            if (bsize <= i) break;
        }
    }

    scanner->boffset = i;
}

#ifdef PDSCANNER_OPERATOR_DEBUG
static PDInteger SOSTATES = 0;
#   define SOLog(msg...) printf("[op] " msg)
#   define SOEnt() \
        SOSTATES++; \
        SOL(" >>> PUSH #%d: %s (%p)", SOSTATES, op->pushedState->name, op->pushedState)
#   define SOExt() \
        SOL(" <<< POP  #%d: %s (%p)", SOSTATES, scanner->env->state->name, scanner->env->state); \
        SOSTATES--
#   define SOShowStack(descr, stack) \
        printf("%s", descr); \
        PDStackShow(stack)
#else
#   define SOL(msg...) 
#   define SOEnt()
#   define SOExt()
#   define SOShowStack(descr, stack) 
#endif

void PDScannerOperate(PDScannerRef scanner, PDOperatorRef op)
{
    PDScannerSymbolRef sym;
    PDStackRef *var;
    //PDStackRef ref;
    char *buf;
    PDInteger len;
    
    while (op) {
        sym = scanner->sym;
        var = &scanner->env->varStack;
        switch (op->type) {
            case PDOperatorPushState:
            case PDOperatorPushWeakState:
                SOEnt();
                //SOL(">>> push state #%d=%s (%p)\n", statez, op->pushedState->name, op->pushedState);
                PDStackPushEnv(&scanner->envStack, scanner->env);
                scanner->env = calloc(1, sizeof(struct PDEnv));
                scanner->env->state = op->pushedState;
                scanner->env->entryOffset = scanner->boffset;
                PDScannerScan(scanner);
                break;

            case PDOperatorPopState:
                SOExt();
                //printf("<<< pop state #%d=%s (%p)\n", statez, scanner->env->state->name, scanner->env->state);
                PDEnvDestroy(scanner->env);
                scanner->env = PDStackPopEnv(&scanner->envStack);
                break;
                
            case PDOperatorPushResult:      // put symbol on results stack
                /// @todo CLANG doesn't like complex logic that prevents a condition from occurring due to a specification; however, this may very well happen for seriously broken (or odd) PDFs and should be plugged
                PDStackPushKey(&scanner->resultStack, strndup(sym->sstart, sym->slen));
                SOShowStack("results [PUSH] > ", scanner->resultStack);
                break;

            case PDOperatorAppendResult:    // append to top result on stack, which is expected to be a string symbol
                PDAssert(scanner->resultStack->type == PDSTACK_STRING);
                buf = scanner->resultStack->info;
                len = strlen(buf);
                /// @todo CLANG doesn't like complex logic that prevents a condition from occurring due to a specification; however, this may very well happen for seriously broken (or odd) PDFs and should be plugged
                scanner->resultStack->info = buf = realloc(buf, 1 + len + sym->slen);
                strncpy(&buf[len], sym->sstart, sym->slen);
                buf[len+sym->slen] = 0;
                break;
                
            case PDOperatorPushContent:     // push entire buffer from state entry to current pos
                PDAssert(scanner->env->entryOffset < scanner->boffset);
                PDStackPushKey(&scanner->resultStack, strndup(&scanner->buf[scanner->env->entryOffset], scanner->boffset - scanner->env->entryOffset));
                SOShowStack("results [PUSHC] > ", scanner->resultStack);
                break;
                
            case PDOperatorPopVariable:     // take value off of results stack and put into variable stack
                // push value, key, which gives us key, value, when popping later
                SOShowStack("popping from results: ", scanner->resultStack);
                PDStackPopInto(var, &scanner->resultStack);
                PDStackPushIdentifier(var, op->identifier);
                SOShowStack("var [POP VAR] > ", *var);
                break;
                
            case PDOperatorPopValue:          // take value off of results stack and put in variable stack, without a name
                SOShowStack("popping from results ", scanner->resultStack);
                PDStackPopInto(var, &scanner->resultStack);
                SOShowStack("var [POP VAL] > ", *var);
                break;
                
            case PDOperatorPullBuildVariable: // use build stack as a variable with key as name
                PDStackPushStack(var, scanner->env->buildStack);
                PDStackPushIdentifier(var, op->identifier);
                scanner->env->buildStack = NULL;
                SOShowStack("var [PULL BUILD VAR] > ", *var);
                break;
                
            case PDOperatorPushbackValue:
                if (NULL == scanner->sym) 
                    scanner->sym = malloc(sizeof(struct PDScannerSymbol));
                sym = scanner->sym;
                if (scanner->resultStack->type == PDSTACK_STRING) {
                    sym->sstart = PDStackPopKey(&scanner->resultStack);
                    PDStackPushKey(&scanner->garbageStack, sym->sstart); // string will leak if we don't keep it around, as sym always refers directly into buf except here
                } else {
                    sym->sstart = (char *)*PDStackPopIdentifier(&scanner->resultStack);
                    // todo: verify that this doesn't break by-ref stringing
                }
                sym->slen = strlen(sym->sstart);
                sym->stype = PDOperatorSymbolExtFake | PDOperatorSymbolGlobDefine(sym->sstart);
                
                // fall through to pushback symbol that we just created
                
            case PDOperatorPushbackSymbol:  // rewind scanner, in a sense, so that we read this symbol again the next scan
                PDAssert(sym);
                PDStackPushFreeable(&scanner->symbolStack, sym);
                scanner->sym = NULL;
                break;
                
            case PDOperatorStoveComplex:    // add ["type", <variable stack>] to build stack; varStack is reset
                // we do this from the tail end, or we get reversed entries in every dictionary and array
                PDStackPushIdentifier(var, op->identifier);
                PDStackUnshiftStack(&scanner->env->buildStack, *var);
                scanner->env->varStack = NULL;
                SOShowStack("build [P/S COMPLEX] > ", scanner->buildStack);
                break;
                
            case PDOperatorPushComplex:     // add ["type", <variable stack>] to results; varStack is reset as well
                PDStackPushIdentifier(var, op->identifier);
                PDStackPushStack(&scanner->resultStack, *var);
                scanner->env->varStack = NULL;
                SOShowStack("result [P COMPLEX] > ", scanner->resultStack);
                break;
                
            case PDOperatorPopLine:
                PDScannerReadUntilDelimiter(scanner, true);
                break;
            
            case PDOperatorReadToDelimiter:
                PDScannerReadUntilDelimiter(scanner, false);
                break;

            case PDOperatorNOP:
                break;
                
                //
                // debugging
                //
                
            case PDOperatorBreak:
                fprintf(stderr, "*** BREAKPOINT DESIRED ***\n");
                break;
        }
        op = op->next;
    }
}

void PDScannerScan(PDScannerRef scanner)
{
    PDEnvRef env;
    PDStateRef state;
    PDScannerSymbolRef sym;
    PDOperatorRef op;
    char **symbol;
    short symindices;
    short hash;
    PDInteger *symindex;
    PDInteger bresoffset = scanner->boffset;
    env = scanner->env;
    state = env->state;
    
    do {
        symindices = state->symindices;
        symbol = state->symbol;
        symindex = state->symindex;
        
        (*scanner->popFunc)(scanner);
        sym = scanner->sym;
        hash = sym->shash & (symindices-1);
        //    |hash entry exists   |hash is not missing   |symbol does not match hash entry
        while (hash < symindices && symindex[hash] != 0 && strncmp(symbol[symindex[hash]-1], sym->sstart, sym->slen)) 
            hash++;
        op = (hash < symindices && symindex[hash]
              ? state->symbolOp[symindex[hash]-1]
              : (sym->stype & PDOperatorSymbolExtNumeric) && state->numberOp
              ? state->numberOp
              : (sym->stype & PDOperatorSymbolGlobDelimiter) && state->delimiterOp
              ? state->delimiterOp
              : state->fallbackOp);
        if (op) {
            //char *str = strndup(sym->sstart, sym->slen);
            //printf("state %s operator(%s) [\n", state->name, str);
            PDScannerOperate(scanner, op);
            //printf("] // state %s operator(%s)\n", state->name, str);
            //free(str);
        } else {
            if (sym->stype == PDOperatorSymbolExtEOB) {
                printf("unexpected end of buffer encountered; resetting scanner\n");
            } else {
                printf("scanner failure! resetting!\n");
            }
            struct PDOperator resetter;
            resetter.type = PDOperatorPopState;
            resetter.next = NULL;
            while (scanner->env) PDScannerOperate(scanner, &resetter);
            return;
        }
    } while (scanner->env == env && !env->state->iterates);
    
    scanner->bresoffset = bresoffset;
}



PDBool PDScannerPollType(PDScannerRef scanner, char type)
{
    while (scanner->env && !scanner->resultStack) {
        if (PDScannerScanAttemptCap > -1 && PDScannerScanAttemptCap-- == 0) 
            return false;
        PDScannerScan(scanner);
    }
    
    PDScannerScanAttemptCap = -1;
    
    return (scanner->resultStack && scanner->resultStack->type == type);
}

PDBool PDScannerPopString(PDScannerRef scanner, char **value)
{
    if (PDScannerPollType(scanner, PDSTACK_STRING)) {
        *value = PDStackPopKey(&scanner->resultStack);
        return true;
    }
    return false;
}

PDBool PDScannerPopStack(PDScannerRef scanner, PDStackRef *value)
{
    if (PDScannerPollType(scanner, PDSTACK_STACK)) {
        *value = PDStackPopStack(&scanner->resultStack);
        return true;
    }
    return false;
}

void PDScannerAssertString(PDScannerRef scanner, char *value)
{
    char *result;
    if (! PDScannerPopString(scanner, &result)) {
        fprintf(stderr, "* scanner assertion : next entry must be, but was not, a string *\n");
        PDAssert(0);
    }
    if (strcmp(result, value)) {
        fprintf(stderr, "* scanner assertion : (input) \"%s\" != (expected) \"%s\" *\n", result, value);
        PDAssert(0);
    }
    free(result);
}

void PDScannerAssertStackType(PDScannerRef scanner)
{
    PDStackRef stack;
    if (! PDScannerPopStack(scanner, &stack)) {
        char *str;
        if (! PDScannerPopString(scanner, &str)) {
            fprintf(stderr, "* scanner assertion : next entry was not a stack (expected), but it was not a string either (EOF?) *\n");
        } else {
            fprintf(stderr, "* scanner assertion : next entry was not a stack, it was the string \"%s\" *\n", str);
            free(str);
        }
        PDAssert(0);
    } else {
        PDStackDestroy(stack);
    }
}

void PDScannerAssertComplex(PDScannerRef scanner, const char *identifier)
{
    PDStackRef stack;
    if (! PDScannerPopStack(scanner, &stack)) {
        char *str;
        if (! PDScannerPopString(scanner, &str)) {
            fprintf(stderr, "* scanner assertion : next entry was not a stack (expected), but it was not a string either (EOF?) *\n");
        } else {
            fprintf(stderr, "* scanner assertion : next entry was not a stack, it was the string \"%s\" *\n", str);
            free(str);
        }
        PDAssert(0);
    } else {
        PDStackAssertExpectedKey(&stack, identifier);
        
        PDStackDestroy(stack);
    }
}

PDInteger PDScannerReadStream(PDScannerRef scanner, char *dest, PDInteger bytes)
{
    char *buf;
    PDInteger bsize, i;

    PDAssert(! scanner->symbolStack);
    
    buf = scanner->buf;
    bsize = scanner->bsize;
    i = scanner->boffset;
    
    // skip over all newlines
    do {
        if (bsize <= i) {
            (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, 0);
            scanner->buf = buf;
            scanner->bsize = bsize;
        }
        i += (buf[i] == '\r' || buf[i] == '\n');
    } while (buf[i] == '\r' || buf[i] == '\n');
    
    if (bsize - i < bytes) {
        (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, bytes + i - bsize);
        scanner->buf = buf;
        scanner->bsize = bsize;
    }
    if (bsize - i < bytes) 
        bytes = bsize - i;

    memcpy(dest, &buf[i], bytes);
    
    scanner->boffset = i + bytes;
    
    return bytes;
}

void PDScannerPrintStateTrace(PDScannerRef scanner)
{
    PDStackRef s;
    PDInteger i = 1;
    if (scanner->env) 
        printf("#0: %s\n", scanner->env->state->name);
    for (s = scanner->envStack; s; i++, s = s->prev) 
        printf("#%ld: %s\n", i, as(PDEnvRef, s->info)->state->name);
}
