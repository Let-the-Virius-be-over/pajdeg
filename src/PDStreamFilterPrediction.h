//
// PDStreamFilterPrediction.h
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

/**
 @file PDStreamFilterPrediction.h
 
 @ingroup PDSTREAMFILTERPREDICTION

 @defgroup PDSTREAMFILTERPREDICTION PDStreamFilterPrediction
 
 @brief Prediction filter
 
 @ingroup PDINTERNAL
 
 @implements PDSTREAMFILTER

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
extern PDStreamFilterRef PDStreamFilterUnpredictionCreate(pd_stack options);
/**
 Set up a stream filter for prediction based on inputEnd boolean. 
 */
extern PDStreamFilterRef PDStreamFilterPredictionConstructor(PDBool inputEnd, pd_stack options);

#endif

/** @} */

