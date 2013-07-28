//
//  PDStreamFilterPrediction.c
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/27/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

#include "PDInternal.h"

#include "PDStreamFilterPrediction.h"

typedef enum {
    PDPredictorNone = 1,        // no prediction (default)
    PDPredictorTIFF2 = 2,       // TIFF predictor 2
    PDPredictorPNG_NONE = 10,   // PNG prediction (on encoding, PNG None on all rows)
    PDPredictorPNG_SUB = 11,    // PNG prediction (on encoding, PNG Sub on all rows)
    PDPredictorPNG_UP = 12,     // PNG prediction (on encoding, PNG Up on all rows)
    PDPredictorPNG_AVG = 13,    // PNG prediction (on encoding, PNG Average on all rows)
    PDPredictorPNG_PAE = 14,    // PNG prediction (on encoding, PNG Paeth on all rows)
    PDPredictorPNG_OPT = 15,    // PNG prediction (on encoding, PNG Paeth on all rows)
} PDPredictorType;

typedef struct PDPredictor *PDPredictorRef;
struct PDPredictor {
    unsigned char *prevRow;
    PDInteger columns;
    PDInteger typeWidth;
    PDInteger offsWidth;
    PDInteger genWidth;
    PDInteger byteWidth;
    PDPredictorType predictor;
};

PDInteger pred_init(PDStreamFilterRef filter)
{
    PDAssert(! filter->initialized);
    
    PDPredictorRef pred = malloc(sizeof(struct PDPredictor));
    pred->predictor = PDPredictorPNG_UP;
    pred->columns = 6;
    
    filter->data = pred;
    
    // parse options
    PDStackRef iter = filter->options;
    while (iter) {
        if (!strcmp(iter->info, "Columns"))         pred->columns = PDIntegerFromString(iter->prev->info);
        else if (!strcmp(iter->info, "Predictor"))  pred->predictor = PDIntegerFromString(iter->prev->info);
        /*else if (!strcmp(iter->info, "Widths")) {
         PDInteger *widths = iter->prev->info;
         pred->byteWidth  = (pred->typeWidth = widths[0]);
         pred->byteWidth += (pred->offsWidth = widths[1]);
         pred->byteWidth += (pred->genWidth = widths[2]);
         }*/
        else {
            PDWarn("Unknown option ignored: %s\n", iter->info);
        }
        iter = iter->prev->prev;
    }
    
    pred->byteWidth = pred->columns;
    
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
    
    pred->prevRow = calloc(1, pred->byteWidth);
    
    filter->initialized = true;
    
    return true;
}


PDInteger unpred_init(PDStreamFilterRef filter)
{
    PDAssert(! filter->initialized);
    
    PDPredictorRef pred = malloc(sizeof(struct PDPredictor));
    pred->predictor = PDPredictorNone;
    pred->columns = 1;
    
    filter->data = pred;

    // parse options
    PDStackRef iter = filter->options;
    while (iter) {
        if (!strcmp(iter->info, "Columns"))         pred->columns = PDIntegerFromString(iter->prev->info);
        else if (!strcmp(iter->info, "Predictor"))  pred->predictor = PDIntegerFromString(iter->prev->info);
        /*else if (!strcmp(iter->info, "Widths")) {
            PDInteger *widths = iter->prev->info;
            pred->byteWidth  = (pred->typeWidth = widths[0]);
            pred->byteWidth += (pred->offsWidth = widths[1]);
            pred->byteWidth += (pred->genWidth = widths[2]);
        }*/
        else {
            PDWarn("Unknown option ignored: %s\n", iter->info);
        }
        iter = iter->prev->prev;
    }
    
    pred->byteWidth = pred->columns;

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

    pred->prevRow = calloc(1, pred->byteWidth);
    
    filter->initialized = true;
    
    return true;
}

PDInteger pred_done(PDStreamFilterRef filter)
{
    PDAssert(filter->initialized);

    PDPredictorRef pred = filter->data;
    free(pred->prevRow);
    free(pred);

    filter->initialized = false;
    
    return true;
}

PDInteger unpred_done(PDStreamFilterRef filter)
{
    PDAssert(filter->initialized);
    
    PDPredictorRef pred = filter->data;
    free(pred->prevRow);
    free(pred);
    
    filter->initialized = false;
    
    return true;
}

PDInteger pred_proceed(PDStreamFilterRef filter)
{
    PDInteger outputLength;
    
    PDPredictorRef pred = filter->data;
    
    if (filter->bufInAvailable == 0) 
        return 0;
    
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
    PDInteger bw = pred->byteWidth;
    PDInteger rw = bw + 1;
    PDInteger i;
    
    PDAssert(avail % bw == 0); // crash = this filter is bugged, or the input is corrupt

    unsigned char *prevRow = pred->prevRow;
    unsigned char *row = calloc(1, bw);
    
    PDAssert(pred->predictor >= 10);
    while (avail >= bw && cap >= rw) {
        memcpy(row, src, bw);
        src += bw;
        avail -= bw;

        *dst = pred->predictor - 10;
        dst++;
        
        // 1
        // 0    1
        // 0    1
        
        // 

        for (i = 0; i < bw; i++) {
            *dst = (row[i] - prevRow[i]) & 0xff;
            prevRow[i] = row[i];
            dst++;
        }
        cap -= rw;
    }
    
    outputLength = filter->bufOutCapacity - cap;

    filter->bufIn = src;
    filter->bufOut = dst;
    filter->bufInAvailable = avail;
    filter->bufOutCapacity = cap;
    
    return outputLength;
}

PDInteger unpred_proceed(PDStreamFilterRef filter)
{
    PDInteger outputLength;
    
    PDPredictorRef pred = filter->data;
    
    if (filter->bufInAvailable == 0) 
        return 0;
    
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
    PDInteger bw = pred->byteWidth;
    PDInteger rw = bw + 1;
    PDInteger i;
    
    PDAssert(avail % rw == 0); // crash = this filter is bugged, or the input is corrupt
    
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
    
    outputLength = filter->bufOutCapacity - cap;
    
    filter->bufIn = src;
    filter->bufOut = dst;
    filter->bufInAvailable = avail;
    filter->bufOutCapacity = cap;
    
    return outputLength;
}

PDInteger pred_process(PDStreamFilterRef filter)
{
    return pred_proceed(filter);
}

PDInteger unpred_process(PDStreamFilterRef filter)
{
    //if (as(PDPredictorRef, filter->data)->predictor >= 10)
    //    filter->bufInAvailable -= 10; // crc
    
    return unpred_proceed(filter);
}

PDStreamFilterRef PDStreamFilterUnpredictionCreate(PDStackRef options)
{
    return PDStreamFilterCreate(unpred_init, unpred_done, unpred_process, unpred_proceed, options);
}

PDStreamFilterRef PDStreamFilterPredictionCreate(PDStackRef options)
{
    return PDStreamFilterCreate(pred_init, pred_done, pred_process, pred_proceed, options);
}

PDStreamFilterRef PDStreamFilterPredictionConstructor(PDBool inputEnd, PDStackRef options)
{
    return (inputEnd
            ? PDStreamFilterUnpredictionCreate(options)
            : PDStreamFilterPredictionCreate(options));
}
