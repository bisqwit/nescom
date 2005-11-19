#ifndef bqtBinPackerHH
#define bqtBinPackerHH

#include <vector>

// Bisqwit's 1d Bin Packing algorithm implementation (C++).
// Version 1.1.0
// 
// Copyright (C) 1992,2003 Bisqwit (http://iki.fi/bisqwit/)

// This function organizes items to holes
// so that the following goals are met:
//
//  1) a hole is selected for all items
//  2) hole may contain multiple items
//  3) total size of items in one hole
//     may not exceed the size of hole
//
// If possible, this goal is desirable:
//  4) free space is gathered together instead
//     of being scattered in different holes

// The algorithm behind this is as follows:
//
//   Items are handled in size order, biggest-first.
//   Each item is assigned to the fullest bin at the moment.
//
// This produces quite good results in general cases
// in O(n*log(n)) complexity, but may sometimes fail
// when all bins have very little space.

// Users are welcomed to improve the algorithm.

template<typename sizetype>
// Return value: itemno=>binno
const std::vector<unsigned> PackBins
(
   const std::vector<sizetype> &bins, // binno=>size
   const std::vector<sizetype> &items // itemno=>size
);

// User should verify the results:
//  result.size() is always guaranteed to be items.size(),
//  but bins might be overfilled.

// Implementation is in binpacker.tcc .
#include "binpacker.tcc"

#endif
