//===- InstCombineSimplifyDemandedFP.cpp ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains logic for simplifying floating-point instructions based
// on which floating-point classes their users demand.
//
//===----------------------------------------------------------------------===//

#include "InstCombineInternal.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/KnownFPClass.h"
#include "llvm/Transforms/InstCombine/InstCombiner.h"

using namespace llvm;
using namespace llvm::PatternMatch;

#define DEBUG_TYPE "instcombine"
