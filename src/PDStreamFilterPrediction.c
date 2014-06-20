//
// PDStreamFilterPrediction.c
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

#include "pd_internal.h"
#include "pd_stack.h"
#include "PDDictionary.h"
#include "PDNumber.h"

#include "PDStreamFilterPrediction.h"

typedef struct PDPredictor *PDPredictorRef;
/**
 Predictor settings, including options and state data for an ongoing predictor/unpredictor operation.
 */
struct PDPredictor {
    unsigned char *prevRow;     ///< Previous row cache.
    PDInteger columns;          ///< Columns per row.
    PDPredictorType predictor;  ///< Predictor type (strategy)
};

PDInteger pred_init(PDStreamFilterRef filter)
{
    if (filter->initialized)
        return true;
    
    PDPredictorRef pred = malloc(sizeof(struct PDPredictor));
    pred->predictor = PDPredictorNone;
    pred->columns = 1;
    
    filter->data = pred;
    
    // parse options
    PDDictionaryRef dict = filter->options;
    PDNumberRef n;
    n = PDDictionaryGetEntry(dict, "Columns");
    if (n) pred->columns = PDNumberGetInteger(n);
    n = PDDictionaryGetEntry(dict, "Predictor");
    if (n) pred->predictor = (PDPredictorType)PDNumberGetInteger(n);
    
    // we only support given predictors; as more are encountered, support will be added
    switch (pred->predictor) {
        case PDPredictorNone:
        case PDPredictorPNG_UP:
            //case PDPredictorPNG_SUB:
            //case PDPredictorPNG_AVG:
            //case PDPredictorPNG_PAE:
            break;
            
        default:
            PDWarn("Unsupported predictor: %d\n", pred->predictor);
            return false;
    }
    
    pred->prevRow = calloc(1, pred->columns);

    filter->initialized = true;
    
    return true;
}

#define unpred_init pred_init
/*PDInteger unpred_init(PDStreamFilterRef filter)
{
    return pred_init(filter);
}*/

PDInteger pred_done(PDStreamFilterRef filter)
{
    PDAssert(filter->initialized);

    PDPredictorRef pred = filter->data;
    free(pred->prevRow);
    free(pred);

    filter->initialized = false;
    
    return true;
}

#define unpred_done pred_done
/*PDInteger unpred_done(PDStreamFilterRef filter)
{
    return pred_done(filter);
}*/

PDInteger pred_proceed(PDStreamFilterRef filter)
{
    PDInteger outputLength;
    
    PDPredictorRef pred = filter->data;
    
    if (filter->bufInAvailable == 0) {
        filter->finished = ! filter->hasInput;
        return 0;
    }
    
    if (pred->predictor == PDPredictorNone) {
        PDInteger amount = filter->bufOutCapacity > filter->bufInAvailable ? filter->bufInAvailable : filter->bufOutCapacity;
        memcpy(filter->bufOut, filter->bufIn, amount);
        filter->bufOutCapacity -= amount;
        filter->bufIn += amount;
        filter->bufInAvailable -= amount;
        return amount;
    }
    
    unsigned char *src = filter->bufIn;
    unsigned char *dst = filter->bufOut;
    PDInteger avail = filter->bufInAvailable;
    PDInteger cap = filter->bufOutCapacity;
    PDInteger bw = pred->columns;
    PDInteger rw = bw + 1;
    PDInteger i;
    
    //PDAssert(avail % bw == 0); // crash = this filter is bugged, or the input is corrupt

    unsigned char *prevRow = pred->prevRow;
    unsigned char *row = calloc(1, bw);
    
    PDAssert(pred->predictor >= 10);
    while (avail >= bw && cap >= rw) {
        memcpy(row, src, bw);
        src += bw;
        avail -= bw;

        *dst = pred->predictor - 10;
        dst++;

        for (i = 0; i < bw; i++) {
            *dst = (row[i] - prevRow[i]) & 0xff;
            prevRow[i] = row[i];
            dst++;
        }
        cap -= rw;
    }
    
    free(row);
    
    outputLength = filter->bufOutCapacity - cap;

    filter->bufIn = src;
    filter->bufOut = dst;
    filter->bufInAvailable = avail;
    filter->bufOutCapacity = cap;
    filter->needsInput = avail < bw;
    filter->finished = avail == 0 && ! filter->hasInput;

    return outputLength;
}

PDInteger unpred_proceed(PDStreamFilterRef filter)
{
    PDInteger outputLength;
    
    PDPredictorRef pred = filter->data;
    
    if (filter->bufInAvailable == 0) {
        filter->finished = ! filter->hasInput;
        return 0;
    }
    
    if (pred->predictor == PDPredictorNone) {
        PDInteger amount = filter->bufOutCapacity > filter->bufInAvailable ? filter->bufInAvailable : filter->bufOutCapacity;
        memcpy(filter->bufOut, filter->bufIn, amount);
        filter->bufOutCapacity -= amount;
        filter->bufIn += amount;
        filter->bufInAvailable -= amount;
        return amount;
    }
    
    unsigned char *src = filter->bufIn;
    unsigned char *dst = filter->bufOut;
    PDInteger avail = filter->bufInAvailable;
    PDInteger cap = filter->bufOutCapacity;
    PDInteger bw = pred->columns;
    PDInteger rw = bw + 1;
    PDInteger i;
    
    // this throws incorrectly if input is incomplete
    //PDAssert(avail % rw == 0); // crash = this filter is bugged, or the input is corrupt
    
    unsigned char *prevRow = pred->prevRow;
    unsigned char *row = calloc(1, bw);
    
    while (avail >= rw && cap >= bw) {
        PDAssert(src[0] == pred->predictor - 10);
        memcpy(row, &src[1], bw);
        src += rw;
        avail -= rw;
        
        for (i = 0; i < bw; i++) {
            *dst = prevRow[i] = (row[i] + prevRow[i]) & 0xff;
            dst++;
        }
        cap -= bw;
    }
    
    free(row);
    
    outputLength = filter->bufOutCapacity - cap;
    
    filter->bufIn = src;
    filter->bufOut = dst;
    filter->bufInAvailable = avail;
    filter->bufOutCapacity = cap;
    filter->needsInput = avail < rw;
    filter->finished = avail == 0 && ! filter->hasInput;

    return outputLength;
}

PDInteger pred_begin(PDStreamFilterRef filter)
{
    return pred_proceed(filter);
}

PDInteger unpred_begin(PDStreamFilterRef filter)
{
    //if (as(PDPredictorRef, filter->data)->predictor >= 10)
    //   filter->bufInAvailable -= 10; // crc
    return unpred_proceed(filter);
}

PDStreamFilterRef pred_invert_with_options(PDStreamFilterRef filter, PDBool inputEnd)
{
    PDPredictorRef pred = filter->data;
    
    PDDictionaryRef opts = PDDictionaryCreateWithCapacity(2);
    PDDictionarySetEntry(opts, "Predictor", PDNumberWithInteger(pred->predictor));
    PDDictionarySetEntry(opts, "Columns", PDNumberWithInteger(pred->columns));
    
    return PDStreamFilterPredictionConstructor(inputEnd, opts);
}

PDStreamFilterRef pred_invert(PDStreamFilterRef filter)
{
    return pred_invert_with_options(filter, true);
}

PDStreamFilterRef unpred_invert(PDStreamFilterRef filter)
{
    return pred_invert_with_options(filter, false);
}

PDStreamFilterRef PDStreamFilterUnpredictionCreate(PDDictionaryRef options)
{
    return PDStreamFilterCreate(unpred_init, unpred_done, unpred_begin, unpred_proceed, unpred_invert, options);
}

PDStreamFilterRef PDStreamFilterPredictionCreate(PDDictionaryRef options)
{
    return PDStreamFilterCreate(pred_init, pred_done, pred_begin, pred_proceed, pred_invert, options);
}

PDStreamFilterRef PDStreamFilterPredictionConstructor(PDBool inputEnd, PDDictionaryRef options)
{
    return (inputEnd
            ? PDStreamFilterUnpredictionCreate(options)
            : PDStreamFilterPredictionCreate(options));
}
