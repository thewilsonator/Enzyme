;RUN: %opt < %s %loadEnzyme -enzyme -mem2reg -instsimplify -simplifycfg -S | FileCheck %s

define double @wrapper(i32 %n, double* %x, i32 %incx) {
entry:
  %call = tail call double @cblas_dnrm2(i32 %n, double* %x, i32 %incx)
  ret double %call
}

declare double @cblas_dnrm2(i32, double*, i32)

define void @caller(i32 %n, double* %x, double* %_x, i32 %incx) {
entry:
  %call = tail call double (i8*, ...) @__enzyme_autodiff(i8* bitcast (double (i32, double*, i32)* @wrapper to i8*), i32 %n, double* %x, double* %_x, i32 %incx)
  ret void
}

declare double @__enzyme_autodiff(i8*, ...)

;CHECK:define internal void @diffewrapper(i32 %n, double* %x, double* %"x'", i32 %incx, double %differeturn) {
;CHECK-NEXT:entry:
;CHECK-NEXT:  %call = tail call double @cblas_dnrm2(i32 %n, double* %x, i32 %incx)
;CHECK-NEXT:  %0 = fdiv fast double %differeturn, %call
;CHECK-NEXT:  call void @cblas_daxpy(i32 %n, double %0, double* %x, i32 %incx, double* %"x'", i32 %incx)
;CHECK-NEXT:  ret void
;CHECK-NEXT:}
