//===- ValueTrackingInternal.h - Shared ValueTracking helpers ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Helpers shared between ValueTracking.cpp and ValueTrackingFP.cpp. These are
// not part of the public ValueTracking interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_ANALYSIS_VALUETRACKINGINTERNAL_H
#define LLVM_LIB_ANALYSIS_VALUETRACKINGINTERNAL_H

#include <tuple>

namespace llvm {

class APInt;
class Instruction;
class PHINode;
class ShuffleVectorInst;
class Use;
class Value;
struct SimplifyQuery;

bool getShuffleDemandedElts(const ShuffleVectorInst *Shuf,
                            const APInt &DemandedElts, APInt &DemandedLHS,
                            APInt &DemandedRHS);

void breakSelfRecursivePHI(const Use *U, const PHINode *PHI, Value *&ValOut,
                           Instruction *&CtxIOut,
                           const PHINode **PhiOut = nullptr);

std::tuple<int, int, int>
computeKnownExponentRangeFromContext(const Value *V, const SimplifyQuery &Q);

} // namespace llvm

#endif // LLVM_LIB_ANALYSIS_VALUETRACKINGINTERNAL_H
