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

/// Used for example for the size in bytes to allocate
template <typename T> struct Size {
private:
  T *value;

public:
  Size(T *value) : value(value) {}

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    
      if (!value)
        return nullptr;
        
      if (width == 1)
        return value;
    
      switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtRootNode:
        return value;
      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
          return Builder.CreateMul(ConstantInt::get(value->getType(), width), value, value->getName() + ".vecsize");
      }
    
    return value;
  }

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return value;
  }
};

template <> struct Size<ConstantInt> {
private:
  ConstantInt *value;

public:
  Size(ConstantInt *value) : value(value) {}

  ConstantInt *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    if (!value)
      return nullptr;
      
    if (width == 1)
      return value;
    
      switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtRootNode:
        return value;
      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
          return Builder.getInt(value->getValue() * width);
      }
    
    return value;
  }

  ConstantInt *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return value;
  }
};

/// Used for example to determine the number of elements allocated.
template <typename T> struct Count {
private:
  T *value;

public:
  Count(T *value) : value(value) {}

  T *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    return value;
  }

  T *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return value;
  }
};

template<typename T>
struct ShuffleMask {
private:
  SmallVector<int,0> mask;

public:
  ShuffleMask(T mask) : mask(mask.begin(), mask.end()) {}

  SmallVector<int,0> getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    if (width == 1)
      return mask;
    
    switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtRootNode:
        return mask;
      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
        SmallVector<int,0> new_mask;
        int vector_width = mask.size();
        for (int i = 0; i < width; ++i) {
          for (int j = 0; j < vector_width; ++j) {
            new_mask.push_back(mask[j] + i * vector_width);
          }
        }
        return new_mask;
      }
    
    return mask;
  }

  SmallVector<int,0> getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return mask;
  }
};

template<>
struct ShuffleMask<Value*> {
private:
  Value* mask;

public:
  ShuffleMask(Value* mask) : mask(mask) {}

  Value* getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    if (width == 1)
      return mask;
    
    switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtRootNode:
        return mask;
      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
        auto vec = cast<ConstantVector>(mask);
          unsigned vector_width = vec->getType()->getNumElements();
          SmallVector<Constant*,0> new_mask;
          for (int i = 0; i < width; ++i) {
            for (int j = 0; j < vector_width; ++j) {
              auto val = cast<ConstantInt>(vec->getOperand(j))->getValue();
              auto add = val + i * vector_width;
              new_mask.push_back(Builder.getInt(add));
            }
          }
          return ConstantVector::get(new_mask);
        }
    return mask;
  }

  Value* getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return mask;
  }
};

/// Unlike ShuffleMask, BitMask is non-constant
struct BitMask {
private:
  Value* mask;

public:
  BitMask(Value* mask) : mask(mask) {}

  Value* getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    if (width == 1)
      return mask;
    
    if (!mask)
      return nullptr;
    
    switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtRootNode:
        return mask;
      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
        auto vty = cast<FixedVectorType>(mask->getType());
        unsigned vector_width = vty->getNumElements();
        auto splatMask = GradientUtils::CreateVectorSplatMask(vector_width, width);
        return Builder.CreateShuffleVector(mask, splatMask, mask->getName() + ".vecsplat");
    }
    
    return mask;
  }

  Value* getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return mask;
  }
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
};

template <typename T> struct Primal {
private:
  T *value;

public:
  Primal(T *value) : value(value) {}

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width) {
    
    if (width == 1)
      return value;
    
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return value;
    case VectorModeMemoryLayout::VectorizeAtLeafNodes:
        if (auto vty = dyn_cast<VectorType>(value->getType())) {
          unsigned vector_width = vty->getElementCount().getKnownMinValue();
          return Builder.CreateShuffleVector(value, GradientUtils::CreateVectorSplatMask(vector_width, width), value->getName() + ".vecsplat");
        } else if (auto sty = dyn_cast<StructType>(value->getType())) {
          auto vsty = GradientUtils::getShadowType(sty, width, memoryLayout);
          Value *vecstruct = UndefValue::get(vsty);
          for (unsigned i = 0; i < sty->getNumElements(); ++i) {
            auto elem = Builder.CreateExtractValue(value, {i});
            auto splat = Builder.CreateVectorSplat(width, elem);
            vecstruct = Builder.CreateInsertValue(vecstruct, splat, {i});
          }
          return vecstruct;
        }
        return Builder.CreateVectorSplat(width, value);
    }
  }

  Value *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                  unsigned width, unsigned i) {
    return value;
  }
};

template <> struct Primal<Type> {
private:
  Type *type;

public:
  Primal(Type *type) : type(type) {}

  Type *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                 unsigned width) {
    
    if (width == 1)
      return type;
    
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return type;
    case VectorModeMemoryLayout::VectorizeAtLeafNodes:
        return GradientUtils::getShadowType(type, width, memoryLayout);
    }
  }

  Type *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                 unsigned width, unsigned i) {
    return type;
  }
};

template <> struct Primal<ArrayType> {
private:
  ArrayType *type;

public:
  Primal(ArrayType *type) : type(type) {}

  ArrayType *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                      unsigned width) {
    if (width == 1)
      return type;
    
    switch (memoryLayout) {
    case VectorModeMemoryLayout::VectorizeAtRootNode:
      return type;
      case VectorModeMemoryLayout::VectorizeAtLeafNodes: {
        Type *ty = GradientUtils::getShadowType(type->getElementType(), width, memoryLayout);
        return ArrayType::get(ty, type->getNumElements());
      }
    }
  }

  ArrayType *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                      unsigned width, unsigned i) {
    return type;
  }
};

template <> struct Primal<FixedVectorType> {
private:
  FixedVectorType *type;

public:
  Primal(FixedVectorType *type) : type(type) {}

  FixedVectorType *getValue(IRBuilder<> &Builder,
                            VectorModeMemoryLayout memoryLayout,
                            unsigned width) {
    if (width == 1)
      return type;
    
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
    return type;
  }
};

template <> struct Primal<Constant> {
private:
  Constant *c;

public:
  Primal(Constant *c) : c(c) {}

  Constant *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                     unsigned width) {
    if (width == 1)
      return c;
    
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
    return c;
  }
};

template <> struct Primal<ConstantInt> {
private:
  ConstantInt *c;

public:
  Primal(ConstantInt *c) : c(c) {}

  Constant *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
                     unsigned width) {
    if (width == 1)
      return c;
    
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
    return c;
  }
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
    return cv;
  }
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
    return cv;
  }
};

template <typename T> struct Gradient {
private:
  T *value;

public:
  Gradient(T *value) : value(value) {}

  T *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
              unsigned width, unsigned i) {
    if (width == 1 || !value)
      return value;
    
    if (!value)
      return nullptr;
  
    switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtRootNode:
        assert(cast<ArrayType>(value->getType())->getNumElements() == width);
        return GradientUtils::extractMeta(Builder, value, i);
      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
        if (auto vty = dyn_cast<VectorType>(value->getType())) {
          unsigned vector_width = vty->getElementCount().getKnownMinValue();
          if (vector_width / width > 1) {
            return Builder.CreateShuffleVector(value, GradientUtils::CreateExtractSubvectorMask(vector_width, width, i), value->getName() + ".subvector." + Twine(i));
          } else {
            return Builder.CreateExtractElement(value, i);
          }
        }
        return value;
    }

    return value;
  }

  T *getValue(IRBuilder<> &Builder, VectorModeMemoryLayout memoryLayout,
              unsigned width) {
    if (width == 1 || !value)
      return value;
    
    if (!value)
      return nullptr;
    
    if (value && memoryLayout == VectorModeMemoryLayout::VectorizeAtRootNode)
      assert(cast<ArrayType>(value->getType())->getNumElements() == width);

    return value;
  }
};

//template <> struct Gradient<Constant> {
//private:
//  Constant *c;
//
//public:
//  Gradient(Constant *c) : c(c) {}
//
//  Constant *getValue(IRBuilder<> &Builder,
//                                VectorModeMemoryLayout memoryLayout,
//                                unsigned width, unsigned i) {
//    if (!c)
//      return nullptr;
//    
//    if (width == 1)
//      return c;
//    
//    switch (memoryLayout) {
//      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
//        assert(false); //TODO
//        break;
//      case VectorModeMemoryLayout::VectorizeAtRootNode:
//        assert(cast<ConstantArray>(c)->getType()->getNumElements() == width);
//        return cast<ConstantArray>(c)->getAggregateElement(i);
//    }
//
//    return c;
//  }
//
//  Constant *getValue(IRBuilder<> &Builder,
//                                VectorModeMemoryLayout memoryLayout,
//                                unsigned width) {
//    if (!c)
//      return nullptr;
//    
//    if (width == 1)
//      return c;
//    
//    switch (memoryLayout) {
//      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
//        break;
//      case VectorModeMemoryLayout::VectorizeAtRootNode:
//        assert(cast<ArrayType>(c->getType())->getNumElements() == width);
//        break;
//    }
//
//    return c;
//  }
//  
//};

template <> struct Gradient<ArrayRef<Constant *>> {
private:
  ArrayRef<Constant *> values;

public:
  Gradient(ArrayRef<Constant *> values) : values(values) {}

  std::vector<Constant *> getValue(IRBuilder<> &Builder,
                                VectorModeMemoryLayout memoryLayout,
                                unsigned width, unsigned i) {
    if (width == 1)
      return values;
    
    switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtRootNode: {
        std::vector<Constant*> vals;
        for (auto &&val : values) {
          if (val)
            vals.push_back(cast<Constant>(GradientUtils::extractMeta(Builder, val, i)));
          else
            vals.push_back(nullptr);
        }
        return vals;
      }
      case VectorModeMemoryLayout::VectorizeAtLeafNodes: {
        std::vector<Constant*> vals;
        for (auto &&val : values) {
          vals.push_back(cast_or_null<Constant>(val));
        }
        return vals;
      }
    }

    return values;
  }

  ArrayRef<Constant *> getValue(IRBuilder<> &Builder,
                                VectorModeMemoryLayout memoryLayout,
                                unsigned width) {
    if (width == 1)
      return values;
    
    switch (memoryLayout) {
      case VectorModeMemoryLayout::VectorizeAtLeafNodes:
        break;
      case VectorModeMemoryLayout::VectorizeAtRootNode:
        for (auto &&val : values) {
          assert(cast<ArrayType>(val->getType())->getNumElements() == width);
        }
        break;
    }

    return values;
  }
};


#endif
