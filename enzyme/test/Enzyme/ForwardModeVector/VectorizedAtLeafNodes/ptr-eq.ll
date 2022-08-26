; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --function-signature --include-generated-funcs
; RUN: %opt < %s %loadEnzyme -enzyme -enzyme-preopt=false -enzyme-vectorize-at-leaf-nodes -mem2reg -S | FileCheck %s

declare void @__enzyme_fwddiff(void (double*,double*)*, ...)
declare void @free(i8*)

define void @f(double* %x, double* %y) {
entry:
  %val = load double, double* %x
  store double %val, double* %y
  %ptr = bitcast double* %x to i8*
  call void @free(i8* %ptr)
  ret void
}

define void @df(double* %x, <3 x double>* %xp, double* %y, <3 x double>* %dy) {
entry:
  tail call void (void (double*,double*)*, ...) @__enzyme_fwddiff(void (double*,double*)* nonnull @f, metadata !"enzyme_width", i64 3, double* %x, <3 x double>* %xp, double* %y, <3 x double>* %dy)
  ret void
}

; CHECK: define {{[^@]+}}@fwddiffe3f(double* [[X:%.*]], [3 x double*] %"x'", double* [[Y:%.*]], [3 x double*] %"y'") 
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = extractvalue [3 x double*] %"x'", 0
; CHECK-NEXT:    %"val'ipl" = load double, double* [[TMP0]]
; CHECK-NEXT:    [[TMP1:%.*]] = insertvalue [3 x double] undef, double %"val'ipl", 0
; CHECK-NEXT:    [[TMP2:%.*]] = extractvalue [3 x double*] %"x'", 1
; CHECK-NEXT:    %"val'ipl1" = load double, double* [[TMP2]]
; CHECK-NEXT:    [[TMP3:%.*]] = insertvalue [3 x double] [[TMP1]], double %"val'ipl1", 1
; CHECK-NEXT:    [[TMP4:%.*]] = extractvalue [3 x double*] %"x'", 2
; CHECK-NEXT:    %"val'ipl2" = load double, double* [[TMP4]]
; CHECK-NEXT:    [[TMP5:%.*]] = insertvalue [3 x double] [[TMP3]], double %"val'ipl2", 2
; CHECK-NEXT:    [[VAL:%.*]] = load double, double* [[X]]
; CHECK-NEXT:    store double [[VAL]], double* [[Y]]
; CHECK-NEXT:    [[TMP6:%.*]] = extractvalue [3 x double*] %"y'", 0
; CHECK-NEXT:    store double %"val'ipl", double* [[TMP6]]
; CHECK-NEXT:    [[TMP8:%.*]] = extractvalue [3 x double*] %"y'", 1
; CHECK-NEXT:    store double %"val'ipl1", double* [[TMP8]]
; CHECK-NEXT:    [[TMP10:%.*]] = extractvalue [3 x double*] %"y'", 2
; CHECK-NEXT:    store double %"val'ipl2", double* [[TMP10]]
; CHECK-NEXT:    [[TMP12:%.*]] = extractvalue [3 x double*] %"x'", 0
; CHECK-NEXT:    %"ptr'ipc" = bitcast double* [[TMP12]] to i8*
; CHECK-NEXT:    [[TMP13:%.*]] = insertvalue [3 x i8*] undef, i8* %"ptr'ipc", 0
; CHECK-NEXT:    [[TMP14:%.*]] = extractvalue [3 x double*] %"x'", 1
; CHECK-NEXT:    %"ptr'ipc3" = bitcast double* [[TMP14]] to i8*
; CHECK-NEXT:    [[TMP15:%.*]] = insertvalue [3 x i8*] [[TMP13]], i8* %"ptr'ipc3", 1
; CHECK-NEXT:    [[TMP16:%.*]] = extractvalue [3 x double*] %"x'", 2
; CHECK-NEXT:    %"ptr'ipc4" = bitcast double* [[TMP16]] to i8*
; CHECK-NEXT:    [[TMP17:%.*]] = insertvalue [3 x i8*] [[TMP15]], i8* %"ptr'ipc4", 2
; CHECK-NEXT:    [[PTR:%.*]] = bitcast double* [[X]] to i8*
; CHECK-NEXT:    call void @free(i8* [[PTR]]) 
; CHECK-NEXT:    [[TMPZ4:%.*]] = icmp ne i8* [[PTR]], %"ptr'ipc"
; CHECK-NEXT:    br i1 [[TMPZ4]], label [[FREE0:%.*]], label [[END:%.*]]
; CHECK:       free0.i:
; CHECK-NEXT:    call void @free(i8* %"ptr'ipc") 
; CHECK-NEXT:    [[TMPZ5:%.*]] = icmp ne i8* %"ptr'ipc", %"ptr'ipc3"
; CHECK-NEXT:    [[TMPZ6:%.*]] = icmp ne i8* %"ptr'ipc3", %"ptr'ipc4"
; CHECK-NEXT:    [[TMPZ7:%.*]] = and i1 [[TMPZ6]], [[TMPZ5]]
; CHECK-NEXT:    br i1 [[TMPZ7]], label [[FREE1:%.*]], label [[END]]
; CHECK:       free1.i:
; CHECK-NEXT:    call void @free(i8* %"ptr'ipc3")
; CHECK-NEXT:    call void @free(i8* %"ptr'ipc4")
; CHECK-NEXT:    br label [[END]]
; CHECK:       __enzyme_checked_free_3.exit:
; CHECK-NEXT:    ret void