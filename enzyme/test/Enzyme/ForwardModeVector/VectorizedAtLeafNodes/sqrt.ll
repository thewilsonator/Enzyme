; RUN: %opt < %s %loadEnzyme -enzyme -enzyme-preopt=false -enzyme-vectorize-at-leaf-nodes -O3 -S | FileCheck %s

; Function Attrs: nounwind readnone uwtable
define double @tester(double %x) {
entry:
  %0 = tail call fast double @llvm.sqrt.f64(double %x)
  ret double %0
}

define <3 x double> @test_derivative(double %x) {
entry:
  %0 = tail call <3 x double> (double (double)*, ...) @__enzyme_fwddiff(double (double)* nonnull @tester, metadata !"enzyme_width", i64 3, double %x, <3 x double> <double 1.0, double 2.0, double 3.0>)
  ret <3 x double> %0
}

; Function Attrs: nounwind readnone speculatable
declare double @llvm.sqrt.f64(double)

; Function Attrs: nounwind
declare <3 x double> @__enzyme_fwddiff(double (double)*, ...)


; CHECK: define <3 x double> @test_derivative(double %x)
; CHECK-NEXT: entry
; CHECK-NEXT:   %0 = fcmp fast oeq double %x, 0.000000e+00
; CHECK-NEXT:   %1 = tail call fast double @llvm.sqrt.f64(double %x)
; CHECK-NEXT:   %2 = fdiv fast double 5.000000e-01, %1
; CHECK-NEXT:   %3 = select {{(fast )?}}i1 %0, double 0.000000e+00, double %2
; CHECK-NEXT:   %4 = fdiv fast double 1.000000e+00, %1
; CHECK-NEXT:   %5 = select {{(fast )?}}i1 %0, double 0.000000e+00, double %4
; CHECK-NEXT:   %6 = fdiv fast double 1.500000e+00, %1
; CHECK-NEXT:   %7 = select {{(fast )?}}i1 %0, double 0.000000e+00, double %6
; CHECK-NEXT:   %8 = insertvalue <3 x double> zeroinitializer, double %3, 0
; CHECK-NEXT:   %9 = insertvalue <3 x double> %8, double %5, 1
; CHECK-NEXT:   %10 = insertvalue <3 x double> %9, double %7, 2
; CHECK-NEXT:   ret <3 x double> %10
; CHECK-NEXT: }