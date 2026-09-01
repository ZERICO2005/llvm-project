//===- KnownFPClassTest.cpp - KnownFPClass tests --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/KnownFPClass.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/FloatingPointMode.h"
#include "llvm/Support/KnownBits.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

static void expectConstant(const char *SemanticsName, const char *ValueName,
                           const fltSemantics &Semantics, APInt ValueBits,
                           FPClassTest PositiveClass, bool Negative) {
  if (Negative)
    ValueBits.setBit(Semantics.sizeInBits - 1);

  SCOPED_TRACE(testing::Message()
               << SemanticsName << ' ' << (Negative ? "negative " : "positive ")
               << ValueName);
  KnownFPClass Known =
      KnownFPClass::bitcast(Semantics, KnownBits::makeConstant(ValueBits));
  FPClassTest ExpectedClass =
      Negative ? llvm::fneg(PositiveClass) : PositiveClass;
  EXPECT_EQ(ExpectedClass, Known.KnownFPClasses);
  EXPECT_EQ(Negative, Known.SignBit);
}

static APInt makeX87Bits(unsigned Exponent, bool IntegerBit,
                         uint64_t Fraction) {
  return (APInt(80, Exponent) << 64) | (APInt(80, IntegerBit) << 63) |
         APInt(80, Fraction);
}

TEST(KnownFPClassTest, BitcastExhaustiveIEEEHalf) {
  const fltSemantics &Semantics = APFloat::IEEEhalf();

  for (uint64_t RawBits = 0; RawBits != (1u << 16); ++RawBits) {
    APInt ValueBits(16, RawBits);
    KnownFPClass Known =
        KnownFPClass::bitcast(Semantics, KnownBits::makeConstant(ValueBits));
    KnownFPClass Expected(APFloat(Semantics, ValueBits));

    ASSERT_EQ(Expected.KnownFPClasses, Known.KnownFPClasses) << RawBits;
    ASSERT_EQ(Expected.SignBit, Known.SignBit) << RawBits;
  }
}

TEST(KnownFPClassTest, BitcastConflict) {
  const fltSemantics &Semantics = APFloat::IEEEsingle();
  KnownBits Bits(Semantics.sizeInBits);
  Bits.setAllConflict();

  ASSERT_TRUE(Bits.hasConflict());
  KnownFPClass Known = KnownFPClass::bitcast(Semantics, Bits);
  EXPECT_EQ(fcAllFlags, Known.KnownFPClasses);
  EXPECT_EQ(std::nullopt, Known.SignBit);
}

TEST(KnownFPClassTest, BitcastPartialConflict) {
  const fltSemantics &Semantics = APFloat::IEEEsingle();
  KnownBits Bits(Semantics.sizeInBits);
  Bits.Zero.setAllBits();
  Bits.One.setBit(0);

  ASSERT_TRUE(Bits.hasConflict());
  KnownFPClass Known = KnownFPClass::bitcast(Semantics, Bits);
  EXPECT_EQ(fcAllFlags, Known.KnownFPClasses);
  EXPECT_EQ(std::nullopt, Known.SignBit);
}

TEST(KnownFPClassTest, BitcastConstant) {
  struct SemanticsCase {
    const char *Name;
    const fltSemantics *Semantics;
  };

  for (const SemanticsCase &TestCase :
       {SemanticsCase{"ieee_binary16", &APFloat::IEEEhalf()},
        SemanticsCase{"bfloat16", &APFloat::BFloat()},
        SemanticsCase{"ieee_binary32", &APFloat::IEEEsingle()},
        SemanticsCase{"ieee_binary64", &APFloat::IEEEdouble()},
        SemanticsCase{"ieee_binary128", &APFloat::IEEEquad()},
        SemanticsCase{"x87float80", &APFloat::x87DoubleExtended()}}) {
    const fltSemantics &Semantics = *TestCase.Semantics;
    const unsigned BitWidth = Semantics.sizeInBits;
    const APInt AllOnesPayload = APInt::getAllOnes(BitWidth);
    APFloat MaxSubnormal = APFloat::getSmallestNormalized(Semantics);
    EXPECT_EQ(APFloat::opOK, MaxSubnormal.next(/*nextDown=*/true));

    for (bool Negative : {false, true}) {
      expectConstant(TestCase.Name, "0.0", Semantics,
                     APFloat::getZero(Semantics).bitcastToAPInt(), fcPosZero,
                     Negative);
      expectConstant(TestCase.Name, "min_subnormal", Semantics,
                     APFloat::getSmallest(Semantics).bitcastToAPInt(),
                     fcPosSubnormal, Negative);
      expectConstant(TestCase.Name, "max_subnormal", Semantics,
                     MaxSubnormal.bitcastToAPInt(), fcPosSubnormal, Negative);
      expectConstant(TestCase.Name, "min_normal", Semantics,
                     APFloat::getSmallestNormalized(Semantics).bitcastToAPInt(),
                     fcPosNormal, Negative);
      expectConstant(TestCase.Name, "1.0", Semantics,
                     APFloat::getOne(Semantics).bitcastToAPInt(), fcPosNormal,
                     Negative);
      expectConstant(TestCase.Name, "max_normal", Semantics,
                     APFloat::getLargest(Semantics).bitcastToAPInt(),
                     fcPosNormal, Negative);
      expectConstant(TestCase.Name, "inf", Semantics,
                     APFloat::getInf(Semantics).bitcastToAPInt(), fcPosInf,
                     Negative);

      // An sNaN has a clear quiet bit and a non-zero payload.
      expectConstant(TestCase.Name, "snan_mostly_zero", Semantics,
                     APFloat::getSNaN(Semantics).bitcastToAPInt(), fcSNan,
                     Negative);

      // A qNaN has a set quiet bit. The remaining payload bits may be zero.
      expectConstant(TestCase.Name, "qnan_mostly_zero", Semantics,
                     APFloat::getQNaN(Semantics).bitcastToAPInt(), fcQNan,
                     Negative);

      expectConstant(
          TestCase.Name, "snan_mostly_one", Semantics,
          APFloat::getSNaN(Semantics, false, &AllOnesPayload).bitcastToAPInt(),
          fcSNan, Negative);
      expectConstant(
          TestCase.Name, "qnan_mostly_one", Semantics,
          APFloat::getQNaN(Semantics, false, &AllOnesPayload).bitcastToAPInt(),
          fcQNan, Negative);
    }
  }
}

TEST(KnownFPClassTest, BitcastNonCanonicalX87) {
  const fltSemantics &Semantics = APFloat::x87DoubleExtended();
  constexpr uint64_t QuietBit = uint64_t(1) << 62;
  constexpr unsigned ExponentMask = 0x7fff;

  for (bool Negative : {false, true}) {
    // An exponent of zero with a set integer bit is a pseudo-denormal. APFloat
    // treats both the zero- and nonzero-fraction forms as normal.
    expectConstant("x87float80", "pseudo_denormal_zero_fraction", Semantics,
                   makeX87Bits(0, true, 0), fcPosNormal, Negative);
    expectConstant("x87float80", "pseudo_denormal_nonzero_fraction", Semantics,
                   makeX87Bits(0, true, QuietBit | 1), fcPosNormal, Negative);

    // An all-ones exponent with a clear integer bit is a pseudo-infinity or
    // pseudo-NaN. APFloat classifies both as NaNs using the usual quiet bit.
    expectConstant("x87float80", "pseudo_infinity", Semantics,
                   makeX87Bits(ExponentMask, false, 0), fcSNan, Negative);
    expectConstant("x87float80", "pseudo_snan", Semantics,
                   makeX87Bits(ExponentMask, false, 1), fcSNan, Negative);
    expectConstant("x87float80", "pseudo_qnan", Semantics,
                   makeX87Bits(ExponentMask, false, QuietBit), fcQNan,
                   Negative);

    // A nonzero, non-all-ones exponent with a clear integer bit is an
    // unnormal. APFloat also classifies these using the quiet bit.
    expectConstant("x87float80", "unnormal_zero_fraction", Semantics,
                   makeX87Bits(1, false, 0), fcSNan, Negative);
    expectConstant("x87float80", "unnormal_snan", Semantics,
                   makeX87Bits(1, false, 1), fcSNan, Negative);
    expectConstant("x87float80", "unnormal_qnan", Semantics,
                   makeX87Bits(1, false, QuietBit), fcQNan, Negative);
  }
}

} // end anonymous namespace
