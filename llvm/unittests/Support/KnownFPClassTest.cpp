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

static KnownFPClass bitcast(const fltSemantics &Semantics, const APInt &Value) {
  return KnownFPClass::bitcast(Semantics, KnownBits::makeConstant(Value));
}

static void expectKnown(FPClassTest ExpectedClasses,
                        std::optional<bool> ExpectedSignBit,
                        const fltSemantics &Semantics, const KnownBits &Bits) {
  KnownFPClass Known = KnownFPClass::bitcast(Semantics, Bits);
  EXPECT_EQ(ExpectedClasses, Known.KnownFPClasses);
  EXPECT_EQ(ExpectedSignBit, Known.SignBit);
}

static void expectConstant(const char *SemanticsName, const char *ValueName,
                           const fltSemantics &Semantics, APInt ValueBits,
                           FPClassTest PositiveClass, bool Negative) {
  if (Negative)
    ValueBits.setBit(Semantics.sizeInBits - 1);

  SCOPED_TRACE(testing::Message()
               << SemanticsName << ' ' << (Negative ? "negative " : "positive ")
               << ValueName);
  KnownFPClass Known = bitcast(Semantics, ValueBits);
  FPClassTest ExpectedClass =
      Negative ? llvm::fneg(PositiveClass) : PositiveClass;
  EXPECT_EQ(ExpectedClass, Known.KnownFPClasses);
  EXPECT_EQ(Negative, Known.SignBit);
}

TEST(KnownFPClassTest, BitcastExhaustiveIEEEHalf) {
  const fltSemantics &Semantics = APFloat::IEEEhalf();

  for (uint64_t RawBits = 0; RawBits != (1u << 16); ++RawBits) {
    APInt ValueBits(16, RawBits);
    KnownFPClass Known = bitcast(Semantics, ValueBits);
    KnownFPClass Expected(APFloat(Semantics, ValueBits));

    ASSERT_EQ(Expected.KnownFPClasses, Known.KnownFPClasses) << RawBits;
    ASSERT_EQ(Expected.SignBit, Known.SignBit) << RawBits;
  }
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
        SemanticsCase{"ieee_binary128", &APFloat::IEEEquad()}}) {
    const fltSemantics &Semantics = *TestCase.Semantics;
    const unsigned BitWidth = Semantics.sizeInBits;
    const unsigned MantissaBits = Semantics.precision - 1;
    const APInt ExponentMask =
        APInt::getBitsSet(BitWidth, MantissaBits, BitWidth - 1);
    const APInt MantissaMask = APInt::getLowBitsSet(BitWidth, MantissaBits);
    const APInt QuietBit = APInt::getOneBitSet(BitWidth, MantissaBits - 1);

    for (bool Negative : {false, true}) {
      expectConstant(TestCase.Name, "0.0", Semantics, APInt::getZero(BitWidth),
                     fcPosZero, Negative);
      expectConstant(TestCase.Name, "min_subnormal", Semantics,
                     APInt(BitWidth, 1), fcPosSubnormal, Negative);
      expectConstant(TestCase.Name, "max_subnormal", Semantics, MantissaMask,
                     fcPosSubnormal, Negative);
      expectConstant(TestCase.Name, "min_normal", Semantics,
                     APInt::getOneBitSet(BitWidth, MantissaBits), fcPosNormal,
                     Negative);
      expectConstant(TestCase.Name, "1.0", Semantics,
                     APFloat::getOne(Semantics).bitcastToAPInt(), fcPosNormal,
                     Negative);
      expectConstant(TestCase.Name, "max_normal", Semantics,
                     APFloat::getLargest(Semantics).bitcastToAPInt(),
                     fcPosNormal, Negative);
      expectConstant(TestCase.Name, "inf", Semantics, ExponentMask, fcPosInf,
                     Negative);

      // An sNaN has a clear quiet bit and a non-zero payload.
      expectConstant(TestCase.Name, "snan_mostly_zero", Semantics,
                     ExponentMask | APInt(BitWidth, 1), fcSNan, Negative);

      // A qNaN has a set quiet bit. The remaining payload bits may be zero.
      expectConstant(TestCase.Name, "qnan_mostly_zero", Semantics,
                     ExponentMask | QuietBit, fcQNan, Negative);

      expectConstant(TestCase.Name, "snan_mostly_one", Semantics,
                     ExponentMask | (MantissaMask & ~QuietBit), fcSNan,
                     Negative);
      expectConstant(TestCase.Name, "qnan_mostly_one", Semantics,
                     ExponentMask | MantissaMask, fcQNan, Negative);
    }
  }
}

TEST(KnownFPClassTest, BitcastPartialIEEESingle) {
  const fltSemantics &Semantics = APFloat::IEEEsingle();
  const unsigned BitWidth = Semantics.sizeInBits;
  const unsigned MantissaBits = Semantics.precision - 1;
  const APInt SignMask = APInt::getSignMask(BitWidth);
  const APInt ExponentMask =
      APInt::getBitsSet(BitWidth, MantissaBits, BitWidth - 1);
  const APInt MantissaMask = APInt::getLowBitsSet(BitWidth, MantissaBits);

  KnownBits Bits(BitWidth);
  // We should know nothing if everything is unknown.
  expectKnown(fcAllFlags, std::nullopt, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.Zero |= SignMask;
  expectKnown(fcAllFlags & ~fcNegative, false, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.One |= SignMask;
  expectKnown(fcAllFlags & ~fcPositive, true, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.Zero |= ExponentMask;
  expectKnown(fcZero | fcSubnormal, std::nullopt, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.One |= ExponentMask;
  expectKnown(fcInf | fcNan, std::nullopt, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.One.setBit(MantissaBits);
  Bits.Zero.setBit(MantissaBits + 1);
  expectKnown(fcNormal, std::nullopt, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.One.setBit(MantissaBits);
  expectKnown(fcNormal | fcInf | fcNan, std::nullopt, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.Zero.setBit(MantissaBits);
  expectKnown(fcZero | fcSubnormal | fcNormal, std::nullopt, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.Zero |= MantissaMask;
  expectKnown(fcZero | fcNormal | fcInf, std::nullopt, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.Zero.setBit(0);
  expectKnown(fcAllFlags, std::nullopt, Semantics, Bits);

  Bits = KnownBits(BitWidth);
  Bits.One.setBit(0);
  expectKnown(fcSubnormal | fcNormal | fcNan, std::nullopt, Semantics, Bits);

  // A set quiet bit makes any possible NaN quiet. It also proves that the
  // mantissa is non-zero.
  Bits = KnownBits(BitWidth);
  Bits.One.setBit(MantissaBits - 1);
  expectKnown(fcSubnormal | fcNormal | fcQNan, std::nullopt, Semantics, Bits);

  // A clear quiet bit rules out qNaN
  Bits = KnownBits(BitWidth);
  Bits.Zero.setBit(MantissaBits - 1);
  expectKnown(fcAllFlags & ~fcQNan, std::nullopt, Semantics, Bits);
  // Infinity and sNaN remain possible when the exponent is all ones.
  Bits.One |= ExponentMask;
  expectKnown(fcInf | fcSNan, std::nullopt, Semantics, Bits);
  // A non-zero payload distinguishes sNaN from infinity.
  Bits.One.setBit(0);
  expectKnown(fcSNan, std::nullopt, Semantics, Bits);
}

} // end anonymous namespace
