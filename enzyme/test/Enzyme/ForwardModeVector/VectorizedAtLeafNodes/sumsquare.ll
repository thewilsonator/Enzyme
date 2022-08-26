; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --function-signature --include-generated-funcs
; RUN: %opt < %s %loadEnzyme -enzyme -enzyme-preopt=false -enzyme-vectorize-at-leaf-nodes -inline -mem2reg -instsimplify -adce -loop-deletion -correlated-propagation -simplifycfg -early-cse -S | FileCheck %s

; Function Attrs: nounwind
declare <3 x double> @__enzyme_fwddiff(double (double*, i64)*, ...)

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local double @sumsquare(double* nocapture readonly %x, i64 %n) #0 {
entry:
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body
  ret double %add

for.body:                                         ; preds = %entry, %for.body
  %indvars.iv = phi i64 [ 0, %entry ], [ %indvars.iv.next, %for.body ]
  %total.011 = phi double [ 0.000000e+00, %entry ], [ %add, %for.body ]
  %arrayidx = getelementptr inbounds double, double* %x, i64 %indvars.iv
  %0 = load double, double* %arrayidx
  %mul = fmul fast double %0, %0
  %add = fadd fast double %mul, %total.011
  %indvars.iv.next = add nuw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv, %n
  br i1 %exitcond, label %for.cond.cleanup, label %for.body
}

; Function Attrs: nounwind uwtable
define dso_local <3 x double> @dsumsquare(double* %x, <3 x double>* %xp, i64 %n) local_unnamed_addr #1 {
entry:
  %0 = tail call <3 x double> (double (double*, i64)*, ...) @__enzyme_fwddiff(double (double*, i64)* nonnull @sumsquare, metadata !"enzyme_width", i64 3, double* %x, <3 x double>* %xp, i64 %n)
  ret <3 x double> %0
}


attributes #0 = { norecurse nounwind readonly uwtable }
attributes #1 = { nounwind uwtable }
attributes #2 = { nounwind }


; CHECK: define dso_local <3 x double> @dsumsquare(double* %x, <3 x double>* %xp, i64 %n) local_unnamed_addr #1 {
; CHECK-NEXT: entry:
; CHECK-NEXT:   br label %for.body.i

; CHECK: for.body.i:                                       ; preds = %for.body.i, %entry
; CHECK-NEXT:   %0 = phi fast <3 x double> [ zeroinitializer, %entry ], [ %4, %for.body.i ]
; CHECK-NEXT:   %iv.i = phi i64 [ %iv.next.i, %for.body.i ], [ 0, %entry ]
; CHECK-NEXT:   %iv.next.i = add nuw nsw i64 %iv.i, 1
; CHECK-NEXT:   %"arrayidx'ipg.i" = getelementptr inbounds <3 x double>, <3 x double>* %xp, i64 %iv.i
; CHECK-NEXT:   %arrayidx.i = getelementptr inbounds double, double* %x, i64 %iv.i
; CHECK-NEXT:   %"'ipl.i" = load <3 x double>, <3 x double>* %"arrayidx'ipg.i", align 8
; CHECK-NEXT:   %1 = load double, double* %arrayidx.i, align 8
; CHECK-NEXT:   %.splatinsert.i = insertelement <3 x double> poison, double %1, i32 0
; CHECK-NEXT:   %.splat.i = shufflevector <3 x double> %.splatinsert.i, <3 x double> poison, <3 x i32> zeroinitializer
; CHECK-NEXT:   %2 = fmul fast <3 x double> %"'ipl.i", %.splat.i
; CHECK-NEXT:   %3 = fadd fast <3 x double> %2, %2
; CHECK-NEXT:   %4 = fadd fast <3 x double> %3, %0
; CHECK-NEXT:   %exitcond.i = icmp eq i64 %iv.i, %n
; CHECK-NEXT:   br i1 %exitcond.i, label %fwddiffe3sumsquare.exit, label %for.body.i

; CHECK: fwddiffe3sumsquare.exit:                          ; preds = %for.body.i
; CHECK-NEXT:   ret <3 x double> %4
; CHECK-NEXT: }