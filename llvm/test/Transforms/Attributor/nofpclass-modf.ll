; RUN: opt -aa-pipeline=basic-aa -passes=attributor -attributor-manifest-internal -S < %s | FileCheck %s

declare { float, float } @llvm.modf.f32(float)
declare { <2 x float>, <2 x float> } @llvm.modf.v2f32(<2 x float>)

define float @ret_modf_frac(float %arg) {
; CHECK-LABEL: define nofpclass(inf) float @ret_modf_frac(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

define float @ret_modf_frac_nonan(float nofpclass(nan) %arg) {
; CHECK-LABEL: define nofpclass(nan inf) float @ret_modf_frac_nonan(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

define float @ret_modf_frac_nosnan(float nofpclass(snan) %arg) {
; CHECK-LABEL: define nofpclass(snan inf) float @ret_modf_frac_nosnan(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

define float @ret_modf_frac_nosubnormal(float nofpclass(sub) %arg) {
; CHECK-LABEL: define nofpclass(inf sub) float @ret_modf_frac_nosubnormal(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

define float @ret_modf_frac_positive(
    float nofpclass(nan ninf nzero nsub nnorm) %arg) {
; CHECK-LABEL: define nofpclass(nan inf nzero nsub nnorm) float @ret_modf_frac_positive(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

define float @ret_modf_frac_negative(
    float nofpclass(nan pinf pzero psub pnorm) %arg) {
; CHECK-LABEL: define nofpclass(nan inf pzero psub pnorm) float @ret_modf_frac_negative(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

; A negative fractional subnormal result may be flushed to +0.0.
define float @ret_modf_frac_negative_output_positivezero(
    float nofpclass(nan pinf pzero psub pnorm) %arg)
    denormal_fpenv(float: positivezero|ieee) {
; CHECK-LABEL: define nofpclass(nan inf psub pnorm) float @ret_modf_frac_negative_output_positivezero(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

; A negative subnormal input is flushed to +0.0.
define float @ret_modf_frac_negative_input_positivezero(
    float nofpclass(nan pinf pzero psub pnorm) %arg)
    denormal_fpenv(float: ieee|positivezero) {
; CHECK-LABEL: define nofpclass(nan inf psub pnorm) float @ret_modf_frac_negative_input_positivezero(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

define float @ret_modf_frac_negative_preservesign(
    float nofpclass(nan pinf pzero psub pnorm) %arg)
    denormal_fpenv(float: preservesign|preservesign) {
; CHECK-LABEL: define nofpclass(nan inf pzero psub pnorm) float @ret_modf_frac_negative_preservesign(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

; Positive-zero output flushing cannot introduce +0.0 when a negative-only
; input cannot be subnormal.
define float @ret_modf_frac_negative_nosubnormal_output_positivezero(
    float nofpclass(nan pinf pzero sub pnorm) %arg)
    denormal_fpenv(float: positivezero|ieee) {
; CHECK-LABEL: define nofpclass(nan inf pzero sub pnorm) float @ret_modf_frac_negative_nosubnormal_output_positivezero(
  %modf = call { float, float } @llvm.modf.f32(float %arg)
  %frac = extractvalue { float, float } %modf, 0
  ret float %frac
}

define <2 x float> @ret_modf_frac_v2_positive(
    <2 x float> nofpclass(nan ninf nzero nsub nnorm) %arg) {
; CHECK-LABEL: define nofpclass(nan inf nzero nsub nnorm) <2 x float> @ret_modf_frac_v2_positive(
  %modf = call { <2 x float>, <2 x float> } @llvm.modf.v2f32(<2 x float> %arg)
  %frac = extractvalue { <2 x float>, <2 x float> } %modf, 0
  ret <2 x float> %frac
}
