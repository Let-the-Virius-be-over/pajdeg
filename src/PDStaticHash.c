//
//  PDStaticHash.c
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
#include "PDStaticHash.h"
#include "PDPDFPrivate.h"

PDStaticHashRef PDStaticHashCreate(int entries, void **keys, void **values)
{
    PDStaticHashRef sh = malloc(sizeof(struct PDStaticHash));
#define converterTableHash(key) static_hash_idx(converterTableMask, converterTableShift, key)
    
    sh->users = 1;
    sh->entries = entries;
    sh->keys = keys;
    sh->values = values;
    sh->leaveKeys = sh->leaveValues = 0;

    int i, hash;
    int converterTableMask = 0;
    int converterTableShift = 0;
    int converterTableSize = 4;
    
    // determine no-collision table size, if necessary (everything is constant throughout execution, so we only do this once, no matter what)
    int maxe = entries << 7;
    if (maxe < 128) maxe = 128;
    char *coll = calloc(1, maxe);
    char k = 0;
    char sizeBits = 2;
    
    do {
        i = 0;
        sizeBits++;
        converterTableSize <<= 1;
        
        PDAssert(converterTableSize < maxe); // crash = this entire idea is rotten and should be thrown out and replaced with a real dictionary handler
        converterTableMask = converterTableSize - 1;
        for (converterTableShift = sizeBits; i < entries && converterTableShift < 30; converterTableShift++) {
            k++;
            //printf("#%d: <<%d, & %d\n", k, converterTableShift, converterTableMask);
            for (i = 0; i < entries && k != coll[converterTableHash(keys[i])]; i++)  {
                //printf("\t%d = %ld", i, converterTableHash(keys[i]));
                coll[converterTableHash(keys[i])] = k;
            }
            //if (i < entries) printf("\t%d = %ld", i, converterTableHash(keys[i]));
            //printf("\n");
        }
        //printf("<<%d, ts %d\n", converterTableShift, converterTableSize);
    } while (i < entries);
    free(coll);
    
    converterTableShift--;
    
    void **converterTable = calloc(sizeof(void*), converterTableMask + 1);
    for (i = 0; i < entries; i++) {
        hash = converterTableHash(keys[i]);
        PDAssert(NULL == converterTable[hash]);
        converterTable[hash] = values[i];
    }
    
    sh->table = converterTable;
    sh->mask = converterTableMask;
    sh->shift = converterTableShift;
    
    return sh;
}

PDStaticHashRef PDStaticHashRetain(PDStaticHashRef sh)
{
    sh->users++;
    return sh;
}

void PDStaticHashRelease(PDStaticHashRef sh)
{
    sh->users--;
    if (sh->users == 0) {
        free(sh->table);
        if (! sh->leaveKeys)   free(sh->keys);
        if (! sh->leaveValues) free(sh->values);
        free(sh);
    }
}

void PDStaticHashDisownKeysValues(PDStaticHashRef sh, PDBool disownKeys, PDBool disownValues)
{
    sh->leaveKeys |= disownKeys;
    sh->leaveValues |= disownValues;
}
