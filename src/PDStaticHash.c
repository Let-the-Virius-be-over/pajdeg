//
// PDStaticHash.c
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
#include "PDStaticHash.h"
#include "pd_pdf_private.h"

void PDStaticHashDestroy(PDStaticHashRef sh)
{
    free(sh->table);
    if (! sh->leaveKeys)   free(sh->keys);
    if (! sh->leaveValues) free(sh->values);
}

PDStaticHashRef PDStaticHashCreate(PDInteger entries, void **keys, void **values)
{
    PDStaticHashRef sh = PDAlloc(sizeof(struct PDStaticHash), PDStaticHashDestroy, false);
#define converterTableHash(key) static_hash_idx(converterTableMask, converterTableShift, key)
    
    sh->entries = entries;
    sh->keys = keys;
    sh->values = values;
    sh->leaveKeys = 0;
    sh->leaveValues = keys == values;

    PDInteger i, hash;
    PDInteger converterTableMask = 0;
    PDInteger converterTableShift = 0;
    PDInteger converterTableSize = 4;
    
    // determine no-collision table size, if necessary (everything is constant throughout execution, so we only do this once, no matter what)
    PDInteger maxe = entries << 7;
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

void PDStaticHashDisownKeysValues(PDStaticHashRef sh, PDBool disownKeys, PDBool disownValues)
{
    sh->leaveKeys |= disownKeys;
    sh->leaveValues |= disownValues;
}
