#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace {
    constexpr unsigned LimitInstrCount = 15;
    constexpr unsigned LimitRecDepth = 3;

    unsigned getIRFunctionSize(const Function *F) {
        unsigned TotalInst = 0;
        for (const auto &Block : *F) {
            TotalInst += Block.size();
        }
        return TotalInst;
    }

    bool checkSelfRecursion(const Function *F) {
        for (const auto &Block : *F) {
            for (const auto &Inst : Block) {
                if (const auto *CallInst = dyn_cast<CallBase>(&Inst)) {
                    if (CallInst->getCalledFunction() == F) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
}

namespace {

class VolkovInlinePass : public MachineFunctionPass {
public:
    static char ID;
    VolkovInlinePass() : MachineFunctionPass(ID) {}

    StringRef getPassName() const override {
        return "Volkov A. - Machine Function Inliner";
    }

    bool runOnMachineFunction(MachineFunction &MF) override {
        bool IsMutated = false;
        DenseMap<const Function*, unsigned> RecursionDepthTracker;
        
        SmallVector<MachineInstr*, 16> CallsToInline;

        for (auto &BasicBlock : MF) {
            for (auto &MachInst : BasicBlock) {
                if (!MachInst.isCall()) 
                    continue;

                if (shouldInlineCall(MachInst, RecursionDepthTracker)) {
                    CallsToInline.push_back(&MachInst);
                }
            }
        }

        for (auto *CallInstr : CallsToInline) {
            CallInstr->eraseFromParent();
            IsMutated = true;
        }

        return IsMutated;
    }

private:
    bool shouldInlineCall(MachineInstr &CallInst, DenseMap<const Function*, unsigned> &DepthMap) {
        const Function *TargetFunc = nullptr;

        for (const auto &Op : CallInst.operands()) {
            if (Op.isGlobal()) {
                TargetFunc = dyn_cast<Function>(Op.getGlobal());
                if (TargetFunc) break;
            }
        }

        if (!TargetFunc || TargetFunc->isDeclaration()) {
            return false;
        }

        if (getIRFunctionSize(TargetFunc) > LimitInstrCount) {
            return false;
        }

        if (checkSelfRecursion(TargetFunc)) {
            unsigned CurrentDepth = DepthMap[TargetFunc];
            if (CurrentDepth >= LimitRecDepth) {
                return false;
            }
            DepthMap[TargetFunc] = CurrentDepth + 1;
        }

        return true;
    }
};

char VolkovInlinePass::ID = 0;

}

RegisterPass<VolkovInlinePass> X("volkov-inline", "Volkov A. Function Inlining Pass");
