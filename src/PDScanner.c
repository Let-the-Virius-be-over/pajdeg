//
// PDScanner.c
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

#include "Pajdeg.h"
#include "PDScanner.h"
#include "PDOperator.h"
#include "PDState.h"
#include "pd_internal.h"
#include "PDEnv.h"
#include "pd_stack.h"
#include "PDStreamFilter.h"
#include "pd_pdf_implementation.h" // <-- not ideal

static PDInteger PDScannerScanAttemptCap = -1;
static PDScannerBufFunc bufFunc = NULL;
static void *bufFuncInfo;
static pd_stack scannerContextStack = NULL;

void PDScannerOperate(PDScannerRef scanner, PDOperatorRef op);
void PDScannerScan(PDScannerRef scanner);

void PDScannerContextPush(void *ctxInfo, PDScannerBufFunc ctxBufFunc)
{
    if (bufFunc) {
        pd_stack_push_identifier(&scannerContextStack, (PDID)bufFunc);
        pd_stack_push_identifier(&scannerContextStack, (PDID)bufFuncInfo);
    }
    bufFunc = ctxBufFunc;
    bufFuncInfo = ctxInfo;
}

void PDScannerContextPop(void)
{
    bufFuncInfo = pd_stack_pop_identifier(&scannerContextStack);
    bufFunc = (PDScannerBufFunc)pd_stack_pop_identifier(&scannerContextStack);
}

void PDScannerSetLoopCap(PDInteger cap)
{
    PDScannerScanAttemptCap = cap;
}

void PDScannerDestroy(PDScannerRef scanner)
{
    PDScannerDetachFilter(scanner);
    
    if (scanner->env) PDEnvDestroy(scanner->env);
    
    while (scanner->envStack) {
        PDEnvDestroy((PDEnvRef)pd_stack_pop_identifier(&scanner->envStack));
    }
    
    pd_stack_destroy(scanner->resultStack);
    pd_stack_destroy(scanner->symbolStack);
    pd_stack_destroy(scanner->garbageStack);
    
    free(scanner->sym);
}

PDScannerRef PDScannerCreateWithStateAndPopFunc(PDStateRef state, PDScannerPopFunc popFunc)
{
    PDScannerRef scanner = PDAlloc(sizeof(struct PDScanner), PDScannerDestroy, true);
    scanner->env = PDEnvCreate(state);
    scanner->popFunc = popFunc;
    return scanner;
}

PDScannerRef PDScannerCreateWithState(PDStateRef state)
{
    return PDScannerCreateWithStateAndPopFunc(state, &PDScannerPopSymbol);
}

void PDScannerAttachFixedSizeBuffer(PDScannerRef scanner, char *buf, PDInteger len)
{
    scanner->fixedBuf = true;
    scanner->buf = buf;
    scanner->boffset = 0;
    scanner->bsize = len;
    scanner->bresoffset = 0;
}

void PDScannerAlign(PDScannerRef scanner, PDOffset offset)
{
    pd_stack s;
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
    pd_stack_destroy(scanner->symbolStack);
    pd_stack_destroy(scanner->resultStack);
}

void PDScannerSkip(PDScannerRef scanner, PDSize bytes)
{
    //scanner->bsize += bytes;
    scanner->boffset += bytes;
    //scanner->btrail = scanner->boffset;
}

/*PDInteger PDScannerPassSequence(PDScannerRef scanner, char *sequence)
{
    char *buf;
    PDInteger len, i, bsize, seqi, seql;

    seqi = 0;
    seql = strlen(sequence);
    buf = scanner->buf;
    bsize = scanner->bsize;
    i = scanner->boffset;

    len = 0;

    while (seqi < seql && bsize > i) {
        len++;
        i++;
        seqi = buf[i] == sequence[seqi] ? seqi + 1 : 0;
    }
    scanner->boffset = i;
    return len;
}*/

PDInteger PDScannerPassSymbolCharacterType(PDScannerRef scanner, PDInteger symbolCharType)
{
    char *buf;
    PDInteger len, i, bsize;
    
    buf = scanner->buf;
    bsize = scanner->bsize;
    i = scanner->boffset;
    
    len = 0;
    
    while (bsize > i && PDOperatorSymbolGlob[(unsigned char)buf[i++]] != symbolCharType) {
        len++;
    }
    scanner->boffset = i;
    return len;
}

void PDScannerPopSymbol(PDScannerRef scanner)
{
    if (scanner->symbolStack) {
        // a symbol on stack is ready for use, so we use that
        if (scanner->sym) free(scanner->sym);
        scanner->sym = pd_stack_pop_freeable(&scanner->symbolStack);
        return;
    }
    
    PDScannerSymbolRef sym;
    unsigned char c;
    char *buf;
    short hash;
    PDInteger   bsize, len, i, I;
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
            if (! scanner->fixedBuf) 
                (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, 0);
            if (bsize <= i) 
                break;
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
    // we also want to bump offset past whitespace, until we hit newline (next line may be byte counted)
    I = i;
    i--;
    do {
        c = (buf[i] == '\r' || buf[i] == '\n');
        i++;
        if (bsize <= i) {
            if (! scanner->fixedBuf)
                (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, 0);
            if (bsize <= i) 
                break;
        }
    } while ((!c || buf[i] == '\r' || buf[i] == '\n') && PDOperatorSymbolGlob[(unsigned char)buf[i]] == PDOperatorSymbolGlobWhitespace);

    sym->sstart = buf + I - len;
    sym->slen = len;
    sym->stype = len == 0 ? PDOperatorSymbolExtEOB : prevtype == PDOperatorSymbolGlobRegular && numeric ? PDOperatorSymbolExtNumeric : prevtype;
    sym->shash = 10 * abs(hash) + len;
    
    scanner->buf = buf;
    scanner->bsize = bsize;
    scanner->boffset = i;
    
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
        scanner->sym = pd_stack_pop_freeable(&scanner->symbolStack);
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
            if (! scanner->fixedBuf)
                (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, 0);
            if (bsize <= 1) 
                break;
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
    //static int bop = 0; bop++; if (bop > 400) { 
    //    printf("");
    //}
    char *buf;
    PDInteger   bsize, i;
    PDBool escaped;
    PDScannerSymbolRef sym = scanner->sym;
    i = scanner->boffset;
    escaped = false;
    
    // if we have a symbol stack we want to pop it all and rewind back to where it was, or we may end up skipping content; we do not reset 'i' (the cursor) however, or we may end up looping infinitely; if this is a newline delimiter operation, however, we do need to reset 'i' as well, or we may end up trampling past the newline character for cases where the line to be skipped is a single symbol (e.g. "PDF-1.4"); in these cases, we also rewind beyond 'sym', and not just beyond symbol stack content
    
    if ((delimiterIsNewline && sym) || scanner->symbolStack) {
        while (scanner->symbolStack) {
            if (sym) free(sym);
            sym = pd_stack_pop_freeable(&scanner->symbolStack);
        }
        scanner->sym = sym;
        scanner->boffset = sym->sstart - scanner->buf;
        if (delimiterIsNewline) i = scanner->boffset;
    }
    
    buf = scanner->buf;
    bsize = scanner->bsize;
    while (true) {
        if (bsize <= i) {
            if (! scanner->fixedBuf)
                (*bufFunc)(bufFuncInfo, scanner, &buf,  &bsize, 0);
            if (bsize <= i)
                break;
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
            if (! scanner->fixedBuf)
                (*bufFunc)(bufFuncInfo, scanner, &buf,  &bsize, 0);
            if (bsize <= i) 
                break;
        }
    }

    scanner->boffset = i;
}

//#define PDSCANNER_OPERATOR_DEBUG
#ifdef PDSCANNER_OPERATOR_DEBUG
static PDInteger SOSTATES = 0;
#   define SOLog(msg...) //printf("[op] " msg)
#   define SOEnt() \
        SOSTATES++ //; \
        SOLog(" >>> PUSH #%ld: %s (%p)\n", SOSTATES, op->pushedState->name, op->pushedState)
#   define SOExt() \
        /*SOLog(" <<< POP  #%ld: %s (%p)\n", SOSTATES, scanner->env->state->name, scanner->env->state);*/ \
        SOSTATES--
#   define SOShowStack(descr, stack) \
        //printf("%s", descr); \
        pd_stack_show(stack)
#else
#   define SOLog(msg...) 
#   define SOEnt()
#   define SOExt()
#   define SOShowStack(descr, stack) 
#endif

void PDScannerOperate(PDScannerRef scanner, PDOperatorRef op)
{
    PDScannerSymbolRef sym;
    pd_stack *var;
    //pd_stack ref;

    while (op) {
        sym = scanner->sym;
        var = &scanner->env->varStack;
        switch (op->type) {
            case PDOperatorPushState:
            case PDOperatorPushWeakState:
                SOEnt();
                //SOL(">>> push state #%d=%s (%p)\n", statez, op->pushedState->name, op->pushedState);
                pd_stack_push_identifier(&scanner->envStack, (PDID)scanner->env);
                scanner->env = PDEnvCreate(op->pushedState);
                //scanner->env->entryOffset = scanner->boffset;
                PDScannerScan(scanner);
                if (scanner->failed) return;
                break;

            case PDOperatorPopState:
                SOExt();
                //printf("<<< pop state #%d=%s (%p)\n", statez, scanner->env->state->name, scanner->env->state);
                PDEnvDestroy(scanner->env);
                scanner->env = (PDEnvRef)pd_stack_pop_identifier(&scanner->envStack);
                break;
                
            case PDOperatorPushEmptyString:
                pd_stack_push_key(&scanner->resultStack, strdup(""));
                break;
                
            case PDOperatorPushResult:      // put symbol on results stack
                /// @todo CLANG doesn't like complex logic that prevents a condition from occurring due to a specification; however, this may very well happen for seriously broken (or odd) PDFs and should be plugged
                pd_stack_push_key(&scanner->resultStack, strndup(sym->sstart, sym->slen));
                SOShowStack("results [PUSH] > ", scanner->resultStack);
                break;

            /*case PDOperatorAppendResult:    // append to top result on stack, which is expected to be a string symbol
                PDAssert(scanner->resultStack->type == PD_STACK_STRING);
                buf = scanner->resultStack->info;
                len = strlen(buf);
                /// @todo CLANG doesn't like complex logic that prevents a condition from occurring due to a specification; however, this may very well happen for seriously broken (or odd) PDFs and should be plugged
                scanner->resultStack->info = buf = realloc(buf, 1 + len + sym->slen);
                strncpy(&buf[len], sym->sstart, sym->slen);
                buf[len+sym->slen] = 0;
                break;*/
                
            /*case PDOperatorPushContent:     // push entire buffer from state entry to current pos
                PDAssert(scanner->env->entryOffset < scanner->boffset);
                pd_stack_push_key(&scanner->resultStack, strndup(&scanner->buf[scanner->env->entryOffset], scanner->boffset - scanner->env->entryOffset));
                SOShowStack("results [PUSHC] > ", scanner->resultStack);
                break;*/
                
            case PDOperatorPopVariable:     // take value off of results stack and put into variable stack
                // push value, key, which gives us key, value, when popping later
                SOShowStack("popping from results: ", scanner->resultStack);
                pd_stack_pop_into(var, &scanner->resultStack);
                pd_stack_push_identifier(var, op->identifier);
                SOShowStack("var [POP VAR] > ", *var);
                break;
                
            case PDOperatorPopValue:          // take value off of results stack and put in variable stack, without a name
                SOShowStack("popping from results ", scanner->resultStack);
                pd_stack_pop_into(var, &scanner->resultStack);
                SOShowStack("var [POP VAL] > ", *var);
                break;
                
            case PDOperatorPullBuildVariable: // use build stack as a variable with key as name
                pd_stack_push_stack(var, scanner->env->buildStack);
                pd_stack_push_identifier(var, op->identifier);
                scanner->env->buildStack = NULL;
                SOShowStack("var [PULL BUILD VAR] > ", *var);
                break;
                
            case PDOperatorPushbackValue:
                if (NULL == scanner->sym) 
                    scanner->sym = malloc(sizeof(struct PDScannerSymbol));
                sym = scanner->sym;
                if (scanner->resultStack->type == PD_STACK_STRING) {
                    sym->sstart = pd_stack_pop_key(&scanner->resultStack);
                    //printf("pushing %p onto garbage stack (%s)\n", sym->sstart, sym->sstart);
                    pd_stack_push_key(&scanner->garbageStack, sym->sstart); // string will leak if we don't keep it around, as sym always refers directly into buf except here
                } else {
                    sym->sstart = (char *)*pd_stack_pop_identifier(&scanner->resultStack);
                    // todo: verify that this doesn't break by-ref stringing
                }
                sym->slen = strlen(sym->sstart);
                sym->stype = PDOperatorSymbolExtFake | PDOperatorSymbolGlobDefine(sym->sstart);
                
                // fall through to pushback symbol that we just created
                
            case PDOperatorPushbackSymbol:  // rewind scanner, in a sense, so that we read this symbol again the next scan
                PDAssert(sym);
                pd_stack_push_freeable(&scanner->symbolStack, sym);
                scanner->sym = NULL;
                break;
                
            case PDOperatorStoveComplex:    // add ["type", <variable stack>] to build stack; varStack is reset
                // we do this from the tail end, or we get reversed entries in every dictionary and array
                pd_stack_push_identifier(var, op->identifier);
                pd_stack_unshift_stack(&scanner->env->buildStack, *var);
                scanner->env->varStack = NULL;
                SOShowStack("build [P/S COMPLEX] > ", scanner->env->buildStack);
                break;
                
            case PDOperatorPushComplex:     // add ["type", <variable stack>] to results; varStack is reset as well
                pd_stack_push_identifier(var, op->identifier);
                pd_stack_push_stack(&scanner->resultStack, *var);
                scanner->env->varStack = NULL;
                SOShowStack("result [P COMPLEX] > ", scanner->resultStack);
                break;
                
            case PDOperatorPopLine:
                PDScannerReadUntilDelimiter(scanner, true);
                break;
            
            case PDOperatorReadToDelimiter:
                PDScannerReadUntilDelimiter(scanner, false);
                break;
                
            case PDOperatorMark:
                scanner->bmark = scanner->boffset - sym->slen;
                break;
                
            case PDOperatorPushMarked:
                PDAssert(scanner->bmark < scanner->boffset);
                pd_stack_push_key(&scanner->resultStack, strndup(&scanner->buf[scanner->bmark], sym->sstart - scanner->buf - scanner->bmark + sym->slen));
                SOShowStack("results [PUSH] > ", scanner->resultStack);
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
        //   |hash entry exists   |hash is not missing   |symbol does not match hash entry
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
            pd_stack_destroy(scanner->resultStack);
            scanner->resultStack = NULL;
            scanner->failed = true;
            return;
        }
    } while (scanner->env == env && !env->state->iterates);
    
    scanner->bresoffset = bresoffset;
}



PDBool PDScannerPollType(PDScannerRef scanner, char type)
{
    pd_stack_destroy(scanner->garbageStack);
    scanner->garbageStack = NULL;
    while (!scanner->failed && scanner->env && !scanner->resultStack) {
        if (PDScannerScanAttemptCap > -1 && PDScannerScanAttemptCap-- == 0) 
            return false;
        PDScannerScan(scanner);
    }
    
    PDScannerScanAttemptCap = -1;
    
    return (!scanner->failed && scanner->resultStack && scanner->resultStack->type == type);
}

PDBool PDScannerPopString(PDScannerRef scanner, char **value)
{
    if (PDScannerPollType(scanner, PD_STACK_STRING)) {
        *value = pd_stack_pop_key(&scanner->resultStack);
        return true;
    }
    return false;
}

PDBool PDScannerPopStack(PDScannerRef scanner, pd_stack *value)
{
    if (PDScannerPollType(scanner, PD_STACK_STACK)) {
        *value = pd_stack_pop_stack(&scanner->resultStack);
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
        return;
    }
    if (strcmp(result, value)) {
        fprintf(stderr, "* scanner assertion : (input) \"%s\" != (expected) \"%s\" *\n", result, value);
        PDAssert(0);
    }
    free(result);
}

void PDScannerAssertStackType(PDScannerRef scanner)
{
    pd_stack stack;
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
        pd_stack_destroy(stack);
    }
}

void PDScannerAssertComplex(PDScannerRef scanner, const char *identifier)
{
    pd_stack stack;
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
        pd_stack_assert_expected_key(&stack, identifier);
        
        pd_stack_destroy(stack);
    }
}


PDBool PDScannerAttachFilter(PDScannerRef scanner, PDStreamFilterRef filter)
{
    if (scanner->filter) {
        PDRelease(scanner->filter);
    }
    scanner->filter = filter;
    return 0 != PDStreamFilterInit(filter);
}

void PDScannerDetachFilter(PDScannerRef scanner)
{
    if (scanner->filter) {
        PDRelease(scanner->filter);
    }
    scanner->filter = NULL;
}

PDInteger PDScannerReadStream(PDScannerRef scanner, PDInteger bytes, char *dest, PDInteger capacity)
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
            if (! scanner->fixedBuf) {
                (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, 0);
                scanner->buf = buf;
                scanner->bsize = bsize;
            }
        }
        i += (buf[i] == '\r' || buf[i] == '\n');
    } while (buf[i] == '\r' || buf[i] == '\n');
    
    if (bsize - i < bytes) {
        if (! scanner->fixedBuf) {
            (*bufFunc)(bufFuncInfo, scanner, &buf, &bsize, bytes + i - bsize);
            scanner->buf = buf;
            scanner->bsize = bsize;
        }
    }
    if (bsize - i < bytes) 
        bytes = bsize - i;
    
    scanner->boffset = i + bytes;
    
    if (scanner->filter) {
        PDStreamFilterRef filter = scanner->filter;
        filter->bufIn = (unsigned char *)&buf[i];
        filter->bufInAvailable = bytes;
        filter->bufOut = (unsigned char *)dest;
        filter->bufOutCapacity = capacity;
        
        // we set bytes to the results of the filter process call, as that gives us the # of bytes stored (which is what we return too)
        bytes = PDStreamFilterBegin(filter);
        
        // if filter has a next filter, we 
    } else {
        memcpy(dest, &buf[i], bytes);
    }
    
    return bytes;
}

PDInteger PDScannerReadStreamNext(PDScannerRef scanner, char *dest, PDInteger capacity)
{
    if (scanner->filter) {
        PDStreamFilterRef filter = scanner->filter;
        filter->bufOut = (unsigned char *)dest;
        filter->bufOutCapacity = capacity;
        return  PDStreamFilterProceed(filter);
    }
    
    PDWarn("Call to PDScannerReadStreamNext despite no filter attached.\n");
    return 0;
}

void PDScannerPrintStateTrace(PDScannerRef scanner)
{
    pd_stack s;
    PDInteger i = 1;
    if (scanner->env) 
        printf("#0: %s\n", scanner->env->state->name);
    for (s = scanner->envStack; s; i++, s = s->prev) 
        printf("#%ld: %s\n", i, as(PDEnvRef, s->info)->state->name);
}
