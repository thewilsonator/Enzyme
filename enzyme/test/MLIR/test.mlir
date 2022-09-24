// RUN: %eopt --enzyme %s | FileCheck %s

module {
  func.func @square(%x : f64) -> f64{
    %y = arith.mulf %x, %x : f64
    return %y : f64
  }
  func.func @dsq(%x : f64, %dx : f64) -> f64 {
    %r = enzyme.fwddiff @square(%x, %dx) { activity=[#enzyme<activity enzyme_dup>] } : (f64, f64) -> (f64)
    return %r : f64
  }
}

// CHECK:   func.func @dsq(%arg0: f64, %arg1: f64) -> f64 {
// CHECK-NEXT:     %0 = call @fwddiffesquare(%arg0, %arg1) : (f64, f64) -> f64
// CHECK-NEXT:     return %0 : f64
// CHECK-NEXT:   }
// CHECK:   func.func private @fwddiffesquare(%arg0: f64, %arg1: f64) -> f64 {
// CHECK-NEXT:     %0 = arith.mulf %arg1, %arg0 : f64
// CHECK-NEXT:     %1 = arith.mulf %arg1, %arg0 : f64
// CHECK-NEXT:     %2 = arith.addf %0, %1 : f64
// CHECK-NEXT:     %3 = arith.mulf %arg0, %arg0 : f64
// CHECK-NEXT:     return %2 : f64
// CHECK-NEXT:   }
