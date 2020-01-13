/*
 * TypeAnalysis.cpp - Type Analysis Detection Utilities
 * 
 * Copyright (C) 2019 William S. Moses (enzyme@wsmoses.com) - All Rights Reserved
 *
 * For commercial use of this code please contact the author(s) above.
 *
 * For research use of the code please use the following citation.
 *
 * \misc{mosesenzyme,
    author = {William S. Moses, Tim Kaler},
    title = {Enzyme: LLVM Automatic Differentiation},
    year = {2019},
    howpublished = {\url{https://github.com/wsmoses/Enzyme/}},
    note = {commit xxxxxxx}
 */


#include <cstdint>
#include <deque>

#include <llvm/Config/llvm-config.h>

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

#include "llvm/IR/InstIterator.h"

#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/InlineAsm.h"

#include "TypeAnalysis.h"
#include "Utils.h"

#include "TBAA.h"


        //TODO keep type information that is striated
        // e.g. if you have an i8* [0:Int, 8:Int] => i64* [0:Int, 1:Int]
        // After a depth len into the index tree, prune any lookups that are not {0} or {-1}
        // Todo handle {double}** to double** where there is a 0 removed
        ValueData ValueData::KeepForCast(const llvm::DataLayout& dl, llvm::Type* from, llvm::Type* to) const {

            ValueData vd;

            for(auto &pair : mapping) {

                ValueData vd2;
                
                //llvm::errs() << " considering casting from " << *from << " to " << *to << " fromidx: " << to_string(pair.first) << " dt:" << pair.second.str() << " fromsize: " << fromsize << " tosize: " << tosize << "\n";

                if (pair.first.size() == 0) {
                    vd2.insert(pair.first, pair.second);
                    goto add;
                }
                {
                uint64_t fromsize = dl.getTypeSizeInBits(from) / 8;
                assert(fromsize > 0);
                uint64_t tosize = dl.getTypeSizeInBits(to) / 8;
                assert(tosize > 0);

                // If the sizes are the same, whatever the original one is okay [ since tomemory[ i*sizeof(from) ] indeed the start of an object of type to since tomemory is "aligned" to type to
                if (fromsize == tosize) {
                    vd2.insert(pair.first, pair.second);
                    goto add;
                }

                // If the offset doesn't leak into a later element, we're fine to include
                if (pair.first[0] != -1 && pair.first[0] < tosize) {
                    vd2.insert(pair.first, pair.second);
                    goto add;
                }

                if (pair.first[0] != -1) {
                    vd.insert(pair.first, pair.second);
                    goto add;
                } else {
                    //pair.first[0] == -1
                
                    if (fromsize < tosize) {
                        if (tosize % fromsize == 0) {
                            //TODO should really be at each offset do a -1
                            vd.insert(pair.first, pair.second);
                            goto add;
                        } else {
                            auto tmp(pair.first);
                            tmp[0] = 0;
                            vd.insert(tmp, pair.second);
                            goto add;
                        }
                    } else {
                        //fromsize > tosize
                        // TODO should really insert all indices which are multiples of fromsize
                        auto tmp(pair.first);
                        tmp[0] = 0;
                        vd.insert(tmp, pair.second);
                        goto add;
                    }
                }
                }

                continue;
                add:;
                //llvm::errs() << " casting from " << *from << " to " << *to << " fromidx: " << to_string(pair.first) << " toidx: " << to_string(pair.first) << " dt:" << pair.second.str() << "\n";
                vd |= vd2;
            }
            return vd;
        }


cl::opt<bool> printtype(
            "enzyme_printtype", cl::init(false), cl::Hidden,
            cl::desc("Print type detection algorithm"));

DataType parseTBAA(Instruction* inst) {
        auto typeNameStringRef = getAccessNameTBAA(inst);
        if (typeNameStringRef == "long long" || typeNameStringRef == "long" || typeNameStringRef == "int" || typeNameStringRef == "bool") {// || typeNameStringRef == "omnipotent char") {
            if (printtype) {
                llvm::errs() << "known tbaa " << *inst << " " << typeNameStringRef << "\n";
            }
            return DataType(IntType::Integer);
        } else if (typeNameStringRef == "any pointer" || typeNameStringRef == "vtable pointer") {// || typeNameStringRef == "omnipotent char") {
            if (printtype) {
                llvm::errs() << "known tbaa " << *inst << " " << typeNameStringRef << "\n";
            }
            return DataType(IntType::Pointer);
        } else if (typeNameStringRef == "float") {
            if (printtype)
                llvm::errs() << "known tbaa " << *inst << " " << typeNameStringRef << "\n";
            return Type::getFloatTy(inst->getContext());
        } else if (typeNameStringRef == "double") {
            if (printtype)
                llvm::errs() << "known tbaa " << *inst << " " << typeNameStringRef << "\n";
            return Type::getDoubleTy(inst->getContext());
        } else {
            return DataType(IntType::Unknown);
        }
}

TypeAnalyzer::TypeAnalyzer(Function* function, const NewFnTypeInfo& fn, TypeAnalysis& TA) : function(function), fntypeinfo(fn), interprocedural(TA) {
    for(auto &BB: *function) {
        for(auto &inst : BB) {
	        workList.push_back(&inst);
        }
    }
    for(auto &BB: *function) {
        for(auto &inst : BB) {
            for(auto& op : inst.operands()) {
                addToWorkList(op);
            }
        }
    }
}

ValueData TypeAnalyzer::getAnalysis(Value* val) {
	if (val->getType()->isIntegerTy() && cast<IntegerType>(val->getType())->getBitWidth() == 1) return ValueData(DataType(IntType::Integer));
    if (isa<ConstantData>(val)) {
		if (auto ci = dyn_cast<ConstantInt>(val)) {	
			if (ci->getLimitedValue() >=1 && ci->getLimitedValue() <= 4096) {
				return ValueData(DataType(IntType::Integer));
			}
            if (ci->getType()->getBitWidth() == 8 && ci->getLimitedValue() == 0) {
				return ValueData(DataType(IntType::Integer));
            }
		}
		return ValueData(DataType(IntType::Anything));
	}

	Type* vt = val->getType();
	if (vt->isPointerTy()) {
		vt = cast<PointerType>(vt)->getElementType();
	}

    /*
	DataType dt = IntType::Unknown;

    if (vt->isPointerTy()) {
		dt = DataType(IntType::Pointer);
    }
    //if (vt->isFPOrFPVectorTy()) {
	//	dt = DataType(vt->getScalarType());
    //}
	if (dt.isKnown()) {
		if (val->getType()->isPointerTy()) {
			return ValueData(dt).Only({0});
		} else {
			return ValueData(dt);
		}
	}
    */

    if (isa<Argument>(val) || isa<Instruction>(val) || isa<ConstantExpr>(val)) return analysis[val];
    //TODO consider other things like globals perhaps?
    return ValueData();
}

void TypeAnalyzer::updateAnalysis(Value* val, IntType data, Value* origin) {
	updateAnalysis(val, ValueData(DataType(data)), origin);
}

void TypeAnalyzer::addToWorkList(Value* val) {
	if (!isa<Instruction>(val) && !isa<Argument>(val) && !isa<ConstantExpr>(val)) return;
    if (std::find(workList.begin(), workList.end(), val) != workList.end()) return;
	workList.push_back(val);
}

void TypeAnalyzer::updateAnalysis(Value* val, ValueData data, Value* origin) {
    if (isa<ConstantData>(val)) {
        return;
    }

    if (printtype) {
		llvm::errs() << "updating analysis of val: " << *val << " current: " << analysis[val].str() << " new " << data.str();
		if (origin) llvm::errs() << " from " << *origin;
		llvm::errs() << "\n";
	}

    if (isa<GetElementPtrInst>(val) && data[{}] == IntType::Integer) {
        llvm::errs () << "illegal gep update\n";
        assert(0 && "illegal gep update");
    }

    if (val->getType()->isPointerTy() && data[{}] == IntType::Integer) {
        llvm::errs () << "illegal gep update\n";
        assert(0 && "illegal gep update");
    }

    if (analysis[val] |= data) {
    	//Add val so it can explicitly propagate this new info, if able to
    	if (val != origin)
    		addToWorkList(val);

    	//Add users and operands of the value so they can update from the new operand/use
        for (User* use : val->users()) {
            if (use != origin) {
                addToWorkList(use);
            }
        }

        if (User* me = dyn_cast<User>(val)) {
            for (Value* op : me->operands()) {
                if (op != origin) {
                    addToWorkList(op);
                }
            }
        }
    }
}

void TypeAnalyzer::prepareArgs() {
    for(auto &pair: fntypeinfo.first) {
        assert(pair.first->getParent() == function);
        updateAnalysis(pair.first, pair.second, nullptr);
    }

    for(auto &arg : function->args()) {
    	//Get type and other information about argument
        updateAnalysis(&arg, getAnalysis(&arg), &arg);
    }
    
    //Propagate return value type information
    for(auto &BB: *function) {
        for(auto &inst : BB) {
            if (auto ri = dyn_cast<ReturnInst>(&inst)) {
                if (auto rv = ri->getReturnValue()) {
                    updateAnalysis(rv, fntypeinfo.second, nullptr);
                }
            }
        }
    }
}

void TypeAnalyzer::considerTBAA() {
    for(auto &BB: *function) {
        for(auto &inst : BB) {
            auto dt = parseTBAA(&inst);
            if (!dt.isKnown()) continue;

            if (auto call = dyn_cast<CallInst>(&inst)) {
                if (call->getCalledFunction() && (call->getCalledFunction()->getIntrinsicID() == Intrinsic::memcpy || call->getCalledFunction()->getIntrinsicID() == Intrinsic::memmove)) {
                    if (auto ci = dyn_cast<ConstantInt>(call->getOperand(2))) {
                        for(int i=0; i<(int)ci->getLimitedValue(); i++) {
                            updateAnalysis(call->getOperand(0), ValueData(dt).Only({i}), call);
                            updateAnalysis(call->getOperand(1), ValueData(dt).Only({i}), call);
                        }
                        continue;
                    }
                    updateAnalysis(call->getOperand(0), ValueData(dt).Only({0}), call);
                    updateAnalysis(call->getOperand(1), ValueData(dt).Only({0}), call);
                } else if (call->getType()->isPointerTy()) {
                    updateAnalysis(call, ValueData(dt).Only({-1}), call);
                } else {
                    assert(0 && "unknown tbaa call instruction user");
                }
            } else if (auto si = dyn_cast<StoreInst>(&inst)) {
                //TODO why?
                if (dt == IntType::Pointer) continue;
                updateAnalysis(si->getPointerOperand(), ValueData(dt).Only({0}), si);
                updateAnalysis(si->getValueOperand(), ValueData(dt), si);
            } else if (auto li = dyn_cast<LoadInst>(&inst)) {
                updateAnalysis(li->getPointerOperand(), ValueData(dt).Only({0}), li);
                updateAnalysis(li, ValueData(dt), li);
            } else {
                assert(0 && "unknown tbaa instruction user");
            }
        }
    }
}



void TypeAnalyzer::run() {
	std::deque<CallInst*> pendingCalls;

	do {

    while (workList.size()) {
        auto todo = workList.front();
        workList.pop_front();
        if (auto ci = dyn_cast<CallInst>(todo)) {
        	pendingCalls.push_back(ci);
        	continue;
        }
        visitValue(*todo);
    }

    if (pendingCalls.size() > 0) {
    	auto todo = pendingCalls.front();
    	pendingCalls.pop_front();
    	visitValue(*todo);
    	continue;
    } else break;

	}while(1);
}

void TypeAnalyzer::visitValue(Value& val) {
    if (isa<ConstantData>(&val)) {
        return;
    }

    if (auto ce = dyn_cast<ConstantExpr>(&val)) {
        auto ae = ce->getAsInstruction();
        ae->insertBefore(function->getEntryBlock().getTerminator());
        analysis[ae] = getAnalysis(ce);
        visit(*ae);
        for(auto& a : workList) {
            if (a == ae) {
                a = ce;
            }
        }
        updateAnalysis(ce, analysis[ae], ce);
        analysis.erase(ae);
        ae->eraseFromParent();
        return;
    }

    if (!isa<Argument>(&val) && !isa<Instruction>(&val)) return;

    //TODO add no users integral here

    if (auto inst = dyn_cast<Instruction>(&val)) {
		visit(*inst);
	}
}

void TypeAnalyzer::visitAllocaInst(AllocaInst &I) {
    updateAnalysis(I.getArraySize(), IntType::Integer, &I);
    //todo consider users
}

void TypeAnalyzer::visitLoadInst(LoadInst &I) {
    updateAnalysis(I.getOperand(0), getAnalysis(&I).Only({0}), &I);
    updateAnalysis(&I, getAnalysis(I.getOperand(0)).Lookup({0}), &I);
}

void TypeAnalyzer::visitStoreInst(StoreInst &I) {
    updateAnalysis(I.getPointerOperand(), getAnalysis(I.getValueOperand()).PurgeAnything().Only({0}), &I);
    updateAnalysis(I.getValueOperand(), getAnalysis(I.getPointerOperand()).Lookup({0}), &I);
}

//TODO gep
void TypeAnalyzer::visitGetElementPtrInst(GetElementPtrInst &gep) {
    for(auto& ind : gep.indices()) {
        updateAnalysis(ind, IntType::Integer, &gep);
    }
    
    llvm::Type *Int32Ty = llvm::Type::getInt32Ty(gep.getContext());
    std::vector<Value*> idnext;

    for(auto& a : gep.indices()) {
        if (auto ci = dyn_cast<ConstantInt>(a)) {
            idnext.push_back(ci);//(int)ci->getLimitedValue());
        } else {
            idnext.push_back(ConstantInt::get(Int32Ty, 0));
        }
    }

    auto g2 = GetElementPtrInst::Create(nullptr, gep.getOperand(0), idnext);
    APInt ai(function->getParent()->getDataLayout().getIndexSizeInBits(gep.getPointerAddressSpace()), 0);
    g2->accumulateConstantOffset(function->getParent()->getDataLayout(), ai);
    delete g2;//->eraseFromParent();

    int off = (int)ai.getLimitedValue();

	//TODO GEP
    updateAnalysis(&gep, getAnalysis(gep.getPointerOperand()).UnmergeIndices(off), &gep);

    auto merged = getAnalysis(&gep).MergeIndices(off);

    //llvm::errs() << "GEP: " << gep << " analysis: " << getAnalysis(&gep).str() << " merged: " << merged.str() << "\n";
    updateAnalysis(gep.getPointerOperand(), merged, &gep);
}

void TypeAnalyzer::visitPHINode(PHINode& phi) {
    for(auto& op : phi.incoming_values()) {
        updateAnalysis(op, getAnalysis(&phi), &phi);
    }

    assert(phi.getNumIncomingValues() > 0);
	//TODO phi needs reconsidering here 
    ValueData vd = getAnalysis(phi.getIncomingValue(0));
    //llvm::errs() << "phi: " << phi << "\n";
    for(auto& op : phi.incoming_values()) {
        //llvm::errs() << " + " << vd.str() << " ga: " << getAnalysis(op).str() << "\n";
        vd &= getAnalysis(op);
    }

    updateAnalysis(&phi, vd, &phi);
}

void TypeAnalyzer::visitTruncInst(TruncInst &I) {
	updateAnalysis(&I, getAnalysis(I.getOperand(0)), &I);
	updateAnalysis(I.getOperand(0), getAnalysis(&I), &I);
}

void TypeAnalyzer::visitZExtInst(ZExtInst &I) {
	updateAnalysis(&I, getAnalysis(I.getOperand(0)), &I);
	updateAnalysis(I.getOperand(0), getAnalysis(&I), &I);
}

void TypeAnalyzer::visitSExtInst(SExtInst &I) {
	updateAnalysis(&I, getAnalysis(I.getOperand(0)), &I);
	updateAnalysis(I.getOperand(0), getAnalysis(&I), &I);
}

void TypeAnalyzer::visitAddrSpaceCastInst(AddrSpaceCastInst &I) {
	updateAnalysis(&I, getAnalysis(I.getOperand(0)), &I);
	updateAnalysis(I.getOperand(0), getAnalysis(&I), &I);
}

void TypeAnalyzer::visitFPToUIInst(FPToUIInst &I) {
	updateAnalysis(&I, IntType::Integer, &I);
}

void TypeAnalyzer::visitFPToSIInst(FPToSIInst &I) {
	updateAnalysis(&I, IntType::Integer, &I);
}

void TypeAnalyzer::visitUIToFPInst(UIToFPInst &I) {
	updateAnalysis(I.getOperand(0), IntType::Integer, &I);
}

void TypeAnalyzer::visitSIToFPInst(SIToFPInst &I) {
	updateAnalysis(I.getOperand(0), IntType::Integer, &I);
}

void TypeAnalyzer::visitPtrToIntInst(PtrToIntInst &I) {
	updateAnalysis(&I, IntType::Pointer, &I);
}

void TypeAnalyzer::visitIntToPtrInst(IntToPtrInst &I) {
	updateAnalysis(I.getOperand(0), IntType::Pointer, &I);
}

void TypeAnalyzer::visitBitCastInst(BitCastInst &I) {
  if (I.getType()->isIntOrIntVectorTy() || I.getType()->isFPOrFPVectorTy()) {
	updateAnalysis(&I, getAnalysis(I.getOperand(0)), &I);
	updateAnalysis(I.getOperand(0), getAnalysis(&I), &I);
	return;
  }

  if (I.getType()->isPointerTy() && I.getOperand(0)->getType()->isPointerTy()) {
    Type* et1 = cast<PointerType>(I.getType())->getElementType();
    Type* et2 = cast<PointerType>(I.getOperand(0)->getType())->getElementType();
    
	updateAnalysis(&I, getAnalysis(I.getOperand(0)).KeepForCast(function->getParent()->getDataLayout(), et2, et1), &I);
	updateAnalysis(I.getOperand(0), getAnalysis(&I).KeepForCast(function->getParent()->getDataLayout(), et1, et2), &I);
  }
}

void TypeAnalyzer::visitSelectInst(SelectInst &I) {
    updateAnalysis(I.getTrueValue(), getAnalysis(&I), &I);
    updateAnalysis(I.getFalseValue(), getAnalysis(&I), &I);

    ValueData vd = getAnalysis(I.getTrueValue());
	vd &= getAnalysis(I.getFalseValue());

    updateAnalysis(&I, vd, &I);
}

void TypeAnalyzer::visitExtractElementInst(ExtractElementInst &I) {
	updateAnalysis(I.getIndexOperand(), IntType::Integer, &I);

	//int idx = -1;
    //if (auto ci = dyn_cast<ConstantInt>(I.getIndexOperand())) {
    // 	idx = (int)ci->getLimitedValue();
	//}

	//updateAnalysis(I.getVectorOperand(), getAnalysis(&I).Only({idx}), Direction::Both);
    //updateAnalysis(&I, getAnalysis(I.getVectorOperand()).Lookup({idx}), Direction::Both);
	updateAnalysis(I.getVectorOperand(), getAnalysis(&I), &I);
    updateAnalysis(&I, getAnalysis(I.getVectorOperand()), &I);
}

void TypeAnalyzer::visitInsertElementInst(InsertElementInst &I) {
	updateAnalysis(I.getOperand(2), IntType::Integer, &I);
    
	//int idx = -1;
	//if (auto ci = dyn_cast<ConstantInt>(I.getOperand(2))) {
    //	idx = (int)ci->getLimitedValue();
	//}
	
    //if we are inserting into undef/etc the anything should not be propagated
	auto res = getAnalysis(I.getOperand(0)).PurgeAnything();

	res |= getAnalysis(I.getOperand(1));
	//res |= getAnalysis(I.getOperand(1)).Only({idx});
	res |= getAnalysis(&I);

    updateAnalysis(I.getOperand(0), res, &I);
    updateAnalysis(&I, res, &I);
	updateAnalysis(I.getOperand(1), res, &I);
	//updateAnalysis(I.getOperand(1), res.Lookup({idx}), Direction::Both);
}

void TypeAnalyzer::visitShuffleVectorInst(ShuffleVectorInst &I) { 
    updateAnalysis(I.getOperand(0), getAnalysis(&I), &I);
    updateAnalysis(I.getOperand(1), getAnalysis(&I), &I);

    ValueData vd = getAnalysis(I.getOperand(0));
	vd &= getAnalysis(I.getOperand(1));

    updateAnalysis(&I, vd, &I);
}

void TypeAnalyzer::visitExtractValueInst(ExtractValueInst &I) {
	//for(auto &a : I.indices()) {
	//	updateAnalysis(a, IntType::Integer, &I);
	//}
	//TODO aggregate flow
}

void TypeAnalyzer::visitInsertValueInst(InsertValueInst &I) {
	//for(auto &a : I.indices()) {
	//	updateAnalysis(a, IntType::Integer, &I);
	//}
	//TODO aggregate flow
}

void TypeAnalyzer::visitBinaryOperator(BinaryOperator &I) {
    if (I.getOpcode() == BinaryOperator::FAdd || I.getOpcode() == BinaryOperator::FSub ||
            I.getOpcode() == BinaryOperator::FMul || I.getOpcode() == BinaryOperator::FDiv ||
            I.getOpcode() == BinaryOperator::FRem) {
        auto ty = I.getType()->getScalarType();
        assert(ty->isFloatingPointTy());
        DataType dt(ty);
        updateAnalysis(I.getOperand(0), dt, &I);
        updateAnalysis(I.getOperand(1), dt, &I);
        updateAnalysis(&I, dt, &I);
    } else if (I.getOpcode() != BinaryOperator::And && I.getOpcode() != BinaryOperator::Or) {

		updateAnalysis(I.getOperand(0), getAnalysis(&I), &I);
		updateAnalysis(I.getOperand(1), getAnalysis(&I), &I);

		ValueData vd = getAnalysis(I.getOperand(0));
		vd.pointerIntMerge(getAnalysis(I.getOperand(1)));

		updateAnalysis(&I, vd, &I);
	} else {
		ValueData vd = getAnalysis(I.getOperand(0)).JustInt();
		vd &= getAnalysis(I.getOperand(1)).JustInt();

        if (I.getOpcode() == BinaryOperator::And) {
            for(int i=0; i<2; i++)
            if (auto ci = dyn_cast<ConstantInt>(I.getOperand(i))) {
                if (ci->getLimitedValue() <= 16 && ci->getLimitedValue() >= 0) {
                    vd |= ValueData(IntType::Integer);
                }
            }
        }

		updateAnalysis(&I, vd, &I);
	}
	//TODO also can final assume integral if one is a small constant [i.e. & with 1, etc]
}

void TypeAnalyzer::visitCallInst(CallInst &call) {
	if (auto iasm = dyn_cast<InlineAsm>(call.getCalledValue())) {
		if (iasm->getAsmString() == "cpuid") {
			updateAnalysis(&call, ValueData(IntType::Integer).Only({-1}), &call);
		}
	}

	if (Function* ci = call.getCalledFunction()) {

		if (ci->getName() == "malloc") {
			updateAnalysis(call.getArgOperand(0), IntType::Integer, &call);
		}

		//If memcpy / memmove of pointer, we can propagate type information from src to dst up to the length and vice versa
		if (ci->getIntrinsicID() == Intrinsic::memcpy || ci->getIntrinsicID() == Intrinsic::memmove) {
            //TODO length enforcement
            int sz = 1;
            if (auto ci = dyn_cast<ConstantInt>(call.getArgOperand(2))) {
                sz = (int)ci->getLimitedValue();
            }

			ValueData res = getAnalysis(call.getArgOperand(0)).AtMost(sz);
            ValueData res2 = getAnalysis(call.getArgOperand(1)).AtMost(sz);
            //llvm::errs() << " memcpy: " << call << " res1: " << res.str() << " res2: " << res2.str() << "\n";
            res |= res2;

			updateAnalysis(call.getArgOperand(0), res, &call);
			updateAnalysis(call.getArgOperand(1), res, &call);

			for(unsigned i=2; i<call.getNumArgOperands(); i++) {
				updateAnalysis(call.getArgOperand(i), IntType::Integer, &call);
			}
		}

		//TODO we should handle calls interprocedurally, allowing better propagation of type information
		if (!ci->empty()) {
			visitIPOCall(call, *ci);
		}
	}

}

ValueData TypeAnalyzer::getReturnAnalysis() {
    bool set = false;
    ValueData vd;
    for(auto &BB: *function) {
        for(auto &inst : BB) {
		    if (auto ri = dyn_cast<ReturnInst>(&inst)) {
			    if (auto rv = ri->getReturnValue()) {
                    if (set == false) {
                        set = true;
                        vd = getAnalysis(rv);
                        continue;
                    }
                    vd &= getAnalysis(rv);
                }
            }
        }
    }
    return vd;
}

void TypeAnalyzer::visitIPOCall(CallInst& call, Function& fn) {
	NewFnTypeInfo typeInfo;

    int argnum = 0;
	for(auto &arg : fn.args()) {
		auto dt = getAnalysis(call.getArgOperand(argnum));
		typeInfo.first.insert(std::pair<Argument*, ValueData>(&arg, dt));
		argnum++;
	}

    typeInfo.second = getAnalysis(&call);

                        
	auto a = fn.arg_begin();
	for(size_t i=0; i<call.getNumArgOperands(); i++) {
		auto dt = interprocedural.query(a, typeInfo);
		updateAnalysis(call.getArgOperand(i), dt, &call);
		a++;
	}
    
	ValueData vd = interprocedural.getReturnAnalysis(typeInfo, &fn);
	updateAnalysis(&call, vd, &call);
}

TypeResults TypeAnalysis::analyzeFunction(const NewFnTypeInfo& fn, Function* function) {
    if (analyzedFunctions.find(fn) != analyzedFunctions.end()) return TypeResults(*this, fn, function);

	auto res = analyzedFunctions.emplace(fn, TypeAnalyzer(function, fn, *this));
	auto& analysis = res.first->second;

	if (printtype) {
	    llvm::errs() << "analyzing function " << function->getName() << "\n";
	    for(auto &pair : fn.first) {
	        llvm::errs() << " + knowndata: " << *pair.first << " : " << pair.second.str() << "\n";
	    }
        llvm::errs() << " + retdata: " << fn.second.str() << "\n";
	}

    analysis.prepareArgs();
	analysis.considerTBAA();
	analysis.run();
	return TypeResults(*this, fn, function);
}

ValueData TypeAnalysis::query(Value* val, const NewFnTypeInfo& fn) {
    assert(val);
    assert(val->getType());
	
	if (isa<Constant>(val)) {
		if (auto ci = dyn_cast<ConstantInt>(val)) {	
			if (ci->getLimitedValue() >=1 && ci->getLimitedValue() <= 4096) {
				return ValueData(DataType(IntType::Integer));
			}
            if (ci->getType()->getBitWidth() == 8 && ci->getLimitedValue() == 0) {
				return ValueData(DataType(IntType::Integer));
            }
		}
		return ValueData(DataType(IntType::Anything));
	}
	Function* func = nullptr;
	if (auto arg = dyn_cast<Argument>(val)) func = arg->getParent();
	if (auto inst = dyn_cast<Instruction>(val)) func = inst->getParent()->getParent();

	if (func == nullptr) return ValueData();

    analyzeFunction(fn, func);
	return analyzedFunctions.find(fn)->second.getAnalysis(val);
}

DataType TypeAnalysis::intType(Value* val, const NewFnTypeInfo& fn, bool errIfNotFound) {
    assert(val);
    assert(val->getType());
    assert(val->getType()->isIntOrIntVectorTy());
    auto q = query(val, fn);
    auto dt = q[{}];
    //llvm::errs() << " intType for val: " << *val << " q: " << q.str() << " dt: " << dt.str() << "\n";
	if (errIfNotFound && (!dt.isKnown() || dt.typeEnum == IntType::Anything) ) {
		if (auto inst = dyn_cast<Instruction>(val)) {
			llvm::errs() << *inst->getParent()->getParent() << "\n";
			for(auto &pair : analyzedFunctions.find(fn)->second.analysis) {
				llvm::errs() << "val: " << *pair.first << " - " << pair.second.str() << "\n";
			}
		}
		llvm::errs() << "could not deduce type of integer " << *val << "\n";
		assert(0 && "could not deduce type of integer");
	}
	return dt;
}

DataType TypeAnalysis::firstPointer(Value* val, const NewFnTypeInfo& fn, bool errIfNotFound) {
    assert(val);
    assert(val->getType());
    assert(val->getType()->isPointerTy());
	auto q = query(val, fn);
	auto dt = q[{0}];
	dt |= q[{-1}];
	if (errIfNotFound && (!dt.isKnown() || dt.typeEnum == IntType::Anything) ) {
		if (auto inst = dyn_cast<Instruction>(val)) {
			llvm::errs() << *inst->getParent()->getParent() << "\n";
			for(auto &pair : analyzedFunctions.find(fn)->second.analysis) {
				llvm::errs() << "val: " << *pair.first << " - " << pair.second.str() << "\n";
			}
		}
		llvm::errs() << "could not deduce type of integer " << *val << "\n";
		assert(0 && "could not deduce type of integer");
	}
	return dt;
}

TypeResults::TypeResults(TypeAnalysis &analysis, const NewFnTypeInfo& fn, Function* function) : analysis(analysis), info(fn), function(function) {}


NewFnTypeInfo TypeResults::getAnalyzedTypeInfo() {
	NewFnTypeInfo res;
	for(auto &arg : function->args()) {
		res.first.insert(std::pair<Argument*, ValueData>(&arg, analysis.query(&arg, info)));
	}
    res.second = getReturnAnalysis();
	return res;
}

DataType TypeResults::intType(Value* val) {
	return analysis.intType(val, info);
}
    
ValueData TypeResults::getReturnAnalysis() {
    return analysis.getReturnAnalysis(info, function);
}