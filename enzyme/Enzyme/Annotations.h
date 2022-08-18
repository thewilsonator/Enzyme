//===- Annotations.h - Wrappers determining the context in which a LLVM value is used
//---===//
//
//                             Enzyme Project
//
// Part of the Enzyme Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// If using this code in an academic setting, please cite the following:
// @incollection{enzymeNeurips,
// title = {Instead of Rewriting Foreign Code for Machine Learning,
//          Automatically Synthesize Fast Gradients},
// author = {Moses, William S. and Churavy, Valentin},
// booktitle = {Advances in Neural Information Processing Systems 33},
// year = {2020},
// note = {To appear in},
// }
//
//===----------------------------------------------------------------------===//
//
// This file declares a base helper class CacheUtility that manages the cache
// of values from the forward pass for later use.
//
//===----------------------------------------------------------------------===//

#ifndef ANNOTATIONS_H
#define ANNOTATIONS_H


#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Triple.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

#include "llvm/Support/Casting.h"

#include "GradientUtils.h"

using namespace llvm;

template <typename T> struct Size {
private:
  T *value;

public:
  Size(T *value) : value(value) {}

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    if (value)
      switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtRootNode:
        return value;
      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
        return Builder.CreateMul(Builder.getInt32(width), value);
      }
  }

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return value;
  }

  bool isVector() { return false; }
};

template <typename T> struct Condition {
private:
  T *cond;

public:
  Condition(T *cond) : cond(cond) {}

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    return cond;
  }

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return cond;
  }

  bool isVector() { return false; }
};

template <typename T> struct Primal {
private:
  T *value;

public:
  Primal(T *value) : value(value) {}

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return value;
    case VectorModeMemoryLayout::VectorizeAtLeafNodes:
      return Builder.CreateVectorSplat(width, value);
    }
  }

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return getValue(Builder, memoryLayout, width);
  }

  bool isVector() { return value->getType()->isVectorTy(); }
};

template <> struct Primal<Type> {
private:
  Type *type;

public:
  Primal(Type *type) : type(type) {}

  Type *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                 unsigned width) {
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return type;
    case VectorModeMemoryLayout::VectorizeAtLeafNodes:
      return FixedVectorType::get(type, width);
    }
  }

  Type *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                 unsigned width, unsigned i) {
    return getValue(Builder, memoryLayout, width);
  }

  bool isVector() { return type->isVectorTy(); }
};

template <> struct Primal<ArrayType> {
private:
  ArrayType *type;

public:
  Primal(ArrayType *type) : type(type) {}

  ArrayType *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                      unsigned width) {
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return type;
      case VectorModeMemoryLayout::VectorizeAtLeafNodes: {
        Type *ty = GradientUtils::getShadowType(type->getElementType(), width, memoryLayout);
        return ArrayType::get(ty, width);
      }
    }
  }

  ArrayType *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                      unsigned width, unsigned i) {
    return getValue(Builder, memoryLayout, width);
  }

  bool isVector() { return type->isVectorTy(); }
};

template <> struct Primal<FixedVectorType> {
private:
  FixedVectorType *type;

public:
  Primal(FixedVectorType *type) : type(type) {}

  FixedVectorType *getValue(IRBuilder<> &Builder,
                            VectorModeMemoryLayout memoryLayout,
                            unsigned width) {
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return type;
    case VectorModeMemoryLayout::VectorizeAtLeafNodes:
      return FixedVectorType::get(type->getElementType(),
                                  width * type->getNumElements());
    }
  }

  FixedVectorType *getValue(IRBuilder<> &Builder,
                            VectorModeMemoryLayout memoryLayout, unsigned width,
                            unsigned i) {
    return getValue(Builder, memoryLayout, width);
  }

  bool isVector() { return true; }
};

template <> struct Primal<Constant> {
private:
  Constant *c;

public:
  Primal(Constant *c) : c(c) {}

  Constant *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                     unsigned width) {
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return c;
    case VectorModeMemoryLayout::VectorizeAtLeafNodes:
      std::vector<Constant *> cs(width, c);
      return ConstantVector::get(cs);
    }
  }

  Constant *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                     unsigned width, unsigned i) {
    return getValue(Builder, memoryLayout, width);
  }

  bool isVector() { return c->getType()->isVectorTy(); }
};

template <> struct Primal<ConstantInt> {
private:
  ConstantInt *c;

public:
  Primal(ConstantInt *c) : c(c) {}

  Constant *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                     unsigned width) {
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return c;
    case VectorModeMemoryLayout::VectorizeAtLeafNodes:
      std::vector<Constant *> cs(width, c);
      return ConstantVector::get(cs);
    }
  }

  Constant *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                     unsigned width, unsigned i) {
    return getValue(Builder, memoryLayout, width);
  }

  bool isVector() { return false; }
};

template <> struct Primal<ConstantVector> {
private:
  ConstantVector *cv;

public:
  Primal(ConstantVector *cv) : cv(cv) {}

  ConstantVector *getValue(IRBuilder<> &Builder,
                           VectorModeMemoryLayout memoryLayout,
                           unsigned width) {
    return cv;
  }

  ConstantVector *getValue(IRBuilder<> &Builder,
                           VectorModeMemoryLayout memoryLayout, unsigned width,
                           unsigned i) {
    return getValue(Builder, memoryLayout, width);
  }

  bool isVector() { return true; }
};

template <> struct Primal<ConstantDataVector> {
private:
  ConstantDataVector *cv;

public:
  Primal(ConstantDataVector *cv) : cv(cv) {}

  ConstantDataVector *getValue(IRBuilder<> &Builder,
                               VectorModeMemoryLayout memoryLayout,
                               unsigned width) {
    return cv;
  }

  ConstantDataVector *getValue(IRBuilder<> &Builder,
                               VectorModeMemoryLayout memoryLayout,
                               unsigned width, unsigned i) {
    return getValue(Builder, memoryLayout, width);
  }

  bool isVector() { return true; }
};

template <typename T> struct Gradient {
private:
  T *value;

public:
  Gradient(T *value) : value(value) {}

  T *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
              unsigned width, unsigned i) {
    if ((value &&
         memoryLayout == VectorModeMemoryLayout::VectorizeAtRootNode) ||
        (isVector() &&
         memoryLayout == VectorModeMemoryLayout::VectorizeAtLeafNodes))
      assert(cast<ArrayType>(value->getType())->getNumElements() == width);
    if (value)
      return GradientUtils::extractMeta(Builder, value, i);

    return value;
  }

  T *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
              unsigned width) {
    if (isVector() &&
        memoryLayout == VectorModeMemoryLayout::VectorizeAtLeafNodes)
      assert(false);
    if (value && memoryLayout == VectorModeMemoryLayout::VectorizeAtRootNode)
      assert(cast<ArrayType>(value->getType())->getNumElements() == width);

    return value;
  }

  bool isVector() { return value->getType()->isVectorTy(); }
};

template <> struct Gradient<ArrayRef<Constant *>> {
private:
  ArrayRef<Constant *> values;

public:
  Gradient(ArrayRef<Constant *> values) : values(values) {}

  ArrayRef<Constant *> getValue(IRBuilder<> &Builder,
                                VectorModeMemoryLayout memoryLayout,
                                unsigned width, unsigned i) {
    llvm::SmallVector<Constant *, 3> vals;

    if (memoryLayout == VectorModeMemoryLayout::VectorizeAtRootNode ||
        memoryLayout == VectorModeMemoryLayout::VectorizeAtLeafNodes) {
      for (auto &&val : values) {
        assert(cast<ArrayType>(val->getType())->getNumElements() == width);
        if (val)
          vals.push_back(cast<Constant>(GradientUtils::extractMeta(Builder, val, i)));
      }
    }

    return values;
  }

  ArrayRef<Constant *> getValue(IRBuilder<> &Builder,
                                VectorModeMemoryLayout memoryLayout,
                                unsigned width) {
    if (memoryLayout == VectorModeMemoryLayout::VectorizeAtLeafNodes)
      assert(false);

    if (memoryLayout == VectorModeMemoryLayout::VectorizeAtRootNode) {
      for (auto &&val : values) {
        assert(cast<ArrayType>(val->getType())->getNumElements() == width);
      }
    }

    return values;
  }

  bool isVector() { return false; }
};


#endif
