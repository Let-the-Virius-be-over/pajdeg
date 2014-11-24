//
//  PDHashMap.c
//  pajdeg
//
//  Created by Karl-Johan Alm on 22/11/14.
//  Copyright (c) 2014 Kalle Alm. All rights reserved.
//

#include <math.h>

#include "PDHashMap.h"
#include "pd_internal.h"
#include "pd_pdf_implementation.h"
#include "pd_stack.h"
#include "PDArray.h"
#include "PDString.h"

#define PDHM_PROF // if set, profiling is done (and printed to stdout occasionally) about how well the hash map is performing

#ifdef PDHM_PROF
static int prof_counter = 0; // cycles 0..1023 and prints profile info at every 0

static unsigned long long 
creations = 0,              // # of hash map creations
destroys = 0,               // # of hash map destructions
node_creations = 0,         // # of nodes created
node_destroys = 0,          // # of nodes deleted
totbucks = 0,               // total number of buckets created
totemptybucks = 0,          // total number of empty buckets (even after destruction)
totpopbucks = 0,            // total number of populated buckets (1 or more)
totcollbucks = 0,           // total number of buckets with > 1 node
totfinds = 0,               // total number of bucket find ops
totsets = 0,                // total number of set operations
totgets = 0,                // total number of get operations
totdels = 0,                // total number of delete operations
totreplaces = 0,            // total number of replacements (i.e. sets for pre-existing keys)
totcolls = 0,               // total number of collisions (insertions into populated buckets)
totsetcolls = 0,            // total number of collisions on set ops
totgetcolls = 0,            // total number of collisions on get ops
topbucksize = 0,            // biggest observed bucket size (in nodes)
cstring_hashgens = 0,       // number of c string hash generations
cstring_hashcomps = 0;      // number of c string hash comparisons

#define BS_TRACK_CAP 8
static unsigned long long buckets_sized[BS_TRACK_CAP] = {0};
#define reg_buck_insert(bucket) do { \
            PDSize bsize = PDArrayGetCount(bucket);\
            PDSize bscapped = bsize > BS_TRACK_CAP-1 ? BS_TRACK_CAP-1 : bsize;\
            if (bsize > 0) {\
                totcolls++; \
                totcollbucks += bsize == 1;\
                if (bsize + 1 > topbucksize) topbucksize = bsize + 1; \
                buckets_sized[bscapped]--; \
            }\
            buckets_sized[bscapped+1]++; \
        } while(0)

#define reg_buck_delete(bucket) do {\
            PDSize bsize = PDArrayGetCount(bucket);\
            if (bsize > BS_TRACK_CAP-1) bsize = BS_TRACK_CAP-1;\
            PDAssert(bsize > 0); \
            buckets_sized[bsize]--;\
            buckets_sized[bsize-1]++;\
        } while(0)

static inline void prof_report()
{
    printf("                     HASH MAP PROFILING\n"
           "=================================================================================================\n"
           "creations  : %10llu   destroys   : %10llu\n"
           "nodes      : %10llu              : %10llu\n"
           "bucket sum : %10llu   top sized  : %10llu\n"
           "   empty   : %10llu   populated  : %10llu  w/ collisions : %10llu\n"
           "set ops    : %10llu   get ops    : %10llu     deletions  : %10llu\n"
           "find ops   : %10llu   replace ops: %10llu\n"
           "C string g : %10llu   C string c : %10llu\n"
           "collisions : %10llu\n"
           "   on set  : %10llu   on get     : %10llu\n"
           "collision ratio : %f\n"
           "   set collisions : %f   get/del collisions : %f\n"
           , creations, destroys
           , node_creations, node_destroys
           , totbucks, topbucksize
           , totemptybucks, totpopbucks, totcollbucks
           , totsets, totgets, totdels
           , totfinds, totreplaces
           , cstring_hashgens, cstring_hashcomps
           , totcolls
           , totsetcolls, totgetcolls
           , (float)totcolls / (float)(totsets + totgets + totdels)
           , (float)totsetcolls / (float)totsets, (float)totgetcolls / (float)(totgets+totdels));
    printf("bucket sizes:\n"
           "        0        1        2        3        4        5        6        7+\n");
    for (PDSize i = 0; i < BS_TRACK_CAP; i++) {
        printf(" %8llu", buckets_sized[i]);
    }
    printf("\n"
           "=================================================================================================\n");
}

#   define  prof(args...) do {\
                args;\
                if (!(prof_counter = (prof_counter + 1) & 7))\
                    prof_report();\
            } while(0)
#else
#   define reg_buck_insert(bucket) 
#   define prof(args...) 
#   define prof_report() 
#endif

void PDHashMapDestroy(PDHashMapRef hm)
{
    prof(destroys++);
    free(hm->buckets);
    PDRelease(hm->populated);
}

PDSize PDHashGeneratorCString(const char *key) 
{
    prof(cstring_hashgens++);
    // from http://c.learncodethehardway.org/book/ex37.html
    size_t len = strlen(key);
    PDSize hash = 0;
    PDSize i = 0;
    
    for(hash = i = 0; i < len; ++i) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    
    return hash;
}

#ifdef PDHM_PROF
int PDHashComparatorCString(const char *key1, const char *key2)
{
    prof(cstring_hashcomps++);
    return strcmp(key1, key2);
}
#else
#   define PDHashComparatorCString  (PDHashComparator)strcmp
#endif

#define PD_HASHMAP_DEFAULT_BUCKETS  64

PDHashMapRef _PDHashMapCreateWithSettings(PDSize bucketc)
{
    prof(creations++; totbucks += bucketc; totemptybucks += bucketc);
    PDHashMapRef hm = PDAllocTyped(PDInstanceTypeHashMap, sizeof(struct PDHashMap), PDHashMapDestroy, false);
    hm->count = 0;
    hm->bucketc = bucketc;   // 128 (0b10000000)
    hm->bucketm = bucketc-1; // 127 (0b01111111)
    hm->buckets = calloc(sizeof(PDArrayRef), hm->bucketc);
    hm->populated = PDArrayCreateWithCapacity(bucketc);
    hm->ci = NULL;
    return hm;
}

PDHashMapRef PDHashMapCreateWithBucketCount(PDSize bucketCount)
{
    if (bucketCount < 16) bucketCount = 16;
    PDAssert(bucketCount < 1 << 24); // crash = absurd bucket count (over 16777216)
    PDSize bits = ceilf(log2f(bucketCount)); // ceil(log2(100)) == ceil(6.6438) == 7
    PDSize bucketc = 1 << bits; // 1 << 7 == 128
    return _PDHashMapCreateWithSettings(bucketc);
}

PDHashMapRef PDHashMapCreate()
{
    return _PDHashMapCreateWithSettings(PD_HASHMAP_DEFAULT_BUCKETS);
}

PDHashMapRef PDHashMapCreateWithComplex(pd_stack stack)
{
    PDHashMapRef hm = _PDHashMapCreateWithSettings(PD_HASHMAP_DEFAULT_BUCKETS);
    PDHashMapAddEntriesFromComplex(hm, stack);
    return hm;
}

void PDHashMapAddEntriesFromComplex(PDHashMapRef hm, pd_stack stack)
{
    if (stack == NULL) {
        // empty dictionary
        return;
    }
    
    // this may be an entry that is a dictionary inside another dictionary; if so, we should see something like
    /*
stack<0x15778480> {
   0x532c24 ("de")
   DecodeParms
    stack<0x157785e0> {
       0x532c20 ("dict")
       0x532c1c ("entries")
        stack<0x15778520> {
            stack<0x15778510> {
               0x532c24 ("de")
               Columns
               3
            }
            stack<0x15778590> {
               0x532c24 ("de")
               Predictor
               12
            }
        }
    }
}
     */
    if (PDIdentifies(stack->info, PD_DE)) {
        stack = stack->prev->prev->info;
    }
    
    // we may have the DICT identifier
    if (PDIdentifies(stack->info, PD_DICT)) {
        if (stack->prev->prev == NULL) 
            stack = NULL;
        else 
            stack = stack->prev->prev->info;
    }
    
    /*
 stack<0x1136ba20> {
   0x3f9998 ("de")
   Kids
    stack<0x11368f20> {
       0x3f999c ("array")
       0x3f9990 ("entries")
        stack<0x113ea650> {
            stack<0x113c5c10> {
               0x3f99a0 ("ae")
                stack<0x113532b0> {
                   0x3f9988 ("ref")
                   557
                   0
                }
            }
            stack<0x113d49b0> {
               0x3f99a0 ("ae")
                stack<0x113efa50> {
                   0x3f9988 ("ref")
                   558
                   0
                }
            }
            stack<0x113f3c40> {
               0x3f99a0 ("ae")
                stack<0x1136df80> {
                   0x3f9988 ("ref")
                   559
                   0
                }
            }
            stack<0x113585b0> {
               0x3f99a0 ("ae")
                stack<0x11368e30> {
                   0x3f9988 ("ref")
                   560
                   0
                }
            }
            stack<0x1135df20> {
               0x3f99a0 ("ae")
                stack<0x113f3470> {
                   0x3f9988 ("ref")
                   1670
                   0
                }
            }
        }
    }
}
     */
    
    pd_stack s = stack;
    pd_stack entry;
    
    pd_stack_set_global_preserve_flag(true);
    while (s) {
        entry = as(pd_stack, s->info)->prev;
        
        // entry must be a string; it's the key value
        char *key = strdup(entry->info);
        entry = entry->prev;
        if (entry->type == PD_STACK_STRING) {
            PDHashMapSet(hm, key, PDStringCreate(strdup(entry->info)));
        } else {
            entry = entry->info;
            pd_stack_set_global_preserve_flag(true);
            void *v = PDInstanceCreateFromComplex(&entry);
            pd_stack_set_global_preserve_flag(false);
#ifdef PD_SUPPORT_CRYPTO
            if (v && hm->ci) (*PDInstanceCryptoExchanges[PDResolve(v)])(v, hm->ci, true);
#endif
            PDHashMapSet(hm, key, v);
        }
        s = s->prev;
    }
    pd_stack_set_global_preserve_flag(false);
}


static void PDHashMapNodeDestroy(PDHashMapNodeRef n)
{
    PDRelease(n->data);
    free(n->key);
    prof(node_destroys++);
}

static inline PDHashMapNodeRef PDHashMapNodeCreate(PDSize hash, char *key, void *data)
{
    prof(node_creations++);
    PDHashMapNodeRef n = PDAlloc(sizeof(struct PDHashMapNode), PDHashMapNodeDestroy, false);
    n->hash = hash;
    n->key = key; // owned! freed on node destruction!
    n->data = PDRetain(data);
    return n;
}

static inline PDArrayRef PDHashMapFindBucket(PDHashMapRef hm, const char *key, PDSize hash, PDBool create, PDInteger *outIndex)
{
    prof(totfinds++);
    PDSize bucketIndex = hash & hm->bucketm;
    
    PDArrayRef bucket = hm->buckets[bucketIndex];
    *outIndex = -1;
    
    if (NULL == bucket) {
        if (! create) return NULL;
        prof(totemptybucks--; totpopbucks++); // when a bucket is asked to be created, it means it will be non-empty and populated, so we prof that here
        bucket = PDArrayCreateWithCapacity(4);
        hm->buckets[bucketIndex] = bucket;
        PDArrayAppend(hm->populated, bucket);
        PDRelease(bucket);
    } else {
        PDHashMapNodeRef node;
        PDInteger len = PDArrayGetCount(bucket);
        for (PDInteger i = 0; i < len; i++) {
            node = PDArrayGetElement(bucket, i);
            if (node->hash == hash && !PDHashComparatorCString(key, node->key)) {
                *outIndex = i;
                break;
            }
            prof(if (create) totsetcolls++; else totgetcolls++);
        }
    }
    
    return bucket;
}

void PDHashMapSet(PDHashMapRef hm, const char *key, void *value)
{
    PDAssert(value != NULL); // crash = value is NULL; use delete to remove keys
    
#ifdef PD_SUPPORT_CRYPTO
    if (hm->ci) (*PDInstanceCryptoExchanges[PDResolve(value)])(value, hm->ci, false);
#endif

    prof(totsets++);
    PDSize hash = PDHashGeneratorCString(key);
    PDInteger nodeIndex;
    PDArrayRef bucket = PDHashMapFindBucket(hm, key, hash, true, &nodeIndex);
    PDAssert(bucket != NULL);

    if (nodeIndex != -1) {
        prof(totreplaces++);
        PDHashMapNodeRef node = PDArrayGetElement(bucket, nodeIndex);
        PDRetain(value);
        PDRelease(node->data);
        node->data = value;
    } else {
        reg_buck_insert(bucket);
        hm->count++;
        PDHashMapNodeRef node = PDHashMapNodeCreate(hash, strdup(key), value);
        PDArrayAppend(bucket, node);
        PDRelease(node);
    }
}

void *PDHashMapGet(PDHashMapRef hm, const char *key)
{
    prof(totgets++);
    PDSize hash = PDHashGeneratorCString(key);
    PDInteger nodeIndex;
    PDArrayRef bucket = PDHashMapFindBucket(hm, key, hash, false, &nodeIndex);
    return nodeIndex > -1 ? ((PDHashMapNodeRef)PDArrayGetElement(bucket, nodeIndex))->data : NULL;
}

void PDHashMapDelete(PDHashMapRef hm, const char *key)
{
    prof(totdels++);
    PDSize hash = PDHashGeneratorCString(key);
    PDInteger nodeIndex;
    PDArrayRef bucket = PDHashMapFindBucket(hm, key, hash, false, &nodeIndex);
    if (! bucket || nodeIndex == -1) return;
    reg_buck_delete(bucket);
    hm->count--;
    PDArrayDeleteAtIndex(bucket, nodeIndex);
}

PDSize PDHashMapGetCount(PDHashMapRef hm)
{
    return hm->count;
}

void PDHashMapIterate(PDHashMapRef hm, PDHashIterator it, void *ui)
{
    PDBool shouldStop = false;
    PDInteger blen = PDArrayGetCount(hm->populated);
    for (PDInteger i = 0; i < blen; i++) {
        PDArrayRef bucket = PDArrayGetElement(hm->populated, i);
        PDInteger len = PDArrayGetCount(bucket);
        for (PDInteger j = 0; j < len; j++) {
            PDHashMapNodeRef node = PDArrayGetElement(bucket, j);
            it(node->key, node->data, ui, &shouldStop);
            if (shouldStop) return;
        }
    }
}

typedef struct hm_keygetter {
    int i;
    char **res;
} hm_keygetter;

typedef struct hm_printer {
    char **bv, **buf;
    PDInteger *cap;
    PDInteger *offs;
} hm_printer;

void pd_hm_getkeys(char *key, void *val, hm_keygetter *userInfo, PDBool *shouldStop)
{
    userInfo->res[userInfo->i++] = key;
}

void pd_hm_print(char *key, void *val, hm_printer *p, PDBool *shouldStop)
{
    char *bv = *p->bv;
    PDInteger *cap = p->cap;

    char **buf = p->buf;
    PDInteger offs = *p->offs;
    
    PDInteger klen = strlen(key);
    PDInstancePrinterRequire(klen + 10, klen + 10);
    bv = *buf;
    bv[offs++] = '/';
    strcpy(&bv[offs], key);
    offs += klen;
    bv[offs++] = ' ';
    offs = (*PDInstancePrinters[PDResolve(val)])(val, buf, offs, cap);
    PDInstancePrinterRequire(4, 4);
    bv = *buf;
    bv[offs++] = ' ';

    *p->bv = bv;
    *p->offs = offs;
    
//    printf("\t%s: %s\n", key, val);
}

void PDHashMapPopulateKeys(PDHashMapRef hm, char **keys)
{
    hm_keygetter kg;
    kg.i = 0;
    kg.res = keys;
    PDHashMapIterate(hm, (PDHashIterator)pd_hm_getkeys, &kg);
}

char *PDHashMapToString(PDHashMapRef hm)
{
    PDInteger len = 6 + 20 * hm->count;
    char *str = malloc(len);
    PDHashMapPrinter(hm, &str, 0, &len);
    return str;
}

void PDHashMapPrint(PDHashMapRef hm)
{
    char *str = PDHashMapToString(hm);
    puts(str);
    free(str);
}

PDInteger PDHashMapPrinter(void *inst, char **buf, PDInteger offs, PDInteger *cap)
{
    hm_printer p;
    PDInstancePrinterInit(PDHashMapRef, 0, 1);
    PDInteger len = 6 + 20 * i->count;
    PDInstancePrinterRequire(len, len);
    char *bv = *buf;
    bv[offs++] = '<';
    bv[offs++] = '<';
    bv[offs++] = ' ';
    
    p.buf = buf;
    p.cap = cap;

    p.bv = &bv;
    p.offs = &offs;
    
    PDHashMapIterate(i, (PDHashIterator)pd_hm_print, &p);
    
    bv[offs++] = '>';
    bv[offs++] = '>';
    bv[offs] = 0;
    return offs;
}

#ifdef PD_SUPPORT_CRYPTO

void pd_hm_encrypt(void *key, void *val, PDCryptoInstanceRef ci, PDBool *shouldStop)
{
    (*PDInstanceCryptoExchanges[PDResolve(val)])(val, ci, false);
}

void PDHashMapAttachCrypto(PDHashMapRef hm, pd_crypto crypto, PDInteger objectID, PDInteger genNumber)
{
    hm->ci = PDCryptoInstanceCreate(crypto, objectID, genNumber);
    PDHashMapIterate(hm, (PDHashIterator)pd_hm_encrypt, hm->ci);
}

void PDHashMapAttachCryptoInstance(PDHashMapRef hm, PDCryptoInstanceRef ci, PDBool encrypted)
{
    hm->ci = PDRetain(ci);
    PDHashMapIterate(hm, (PDHashIterator)pd_hm_encrypt, hm->ci);
}

#endif
