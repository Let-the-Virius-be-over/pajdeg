//
//  PDStreamFilterPrediction.h
//  ICViewer
//
//  Created by Karl-Johan Alm on 7/27/13.
//  Copyright (c) 2013 Alacrity Software. All rights reserved.
//

/**
 @defgroup FILTER_PREDICTION_GRP Prediction Filter
 
 @brief Prediction filter
 
 @see PDDefines.h
 
 @{
 */

#ifndef INCLUDED_PDStreamFilterPrediction_h
#define INCLUDED_PDStreamFilterPrediction_h

#include "PDStreamFilter.h"

/**
 Set up a stream filter for prediction.
 */
//extern PDStreamFilterRef PDStreamFilterPredictionCreate(void);

/**
 Set up stream filter for unprediction.
 */
extern PDStreamFilterRef PDStreamFilterUnpredictionCreate(PDStackRef options);
/**
 Set up a stream filter for prediction based on inputEnd boolean. 
 */
extern PDStreamFilterRef PDStreamFilterPredictionConstructor(PDBool inputEnd, PDStackRef options);

#endif
