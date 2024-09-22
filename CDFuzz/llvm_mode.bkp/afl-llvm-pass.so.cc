/*
  Copyright 2015 Google LLC All rights reserved.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at:

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/*
   american fuzzy lop - LLVM-mode instrumentation pass
   ---------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com> and
              Michal Zalewski <lcamtuf@google.com>

   LLVM integration design comes from Laszlo Szekeres. C bits copied-and-pasted
   from afl-as.c are Michal's fault.

   This library is plugged into LLVM when invoking clang through afl-clang-fast.
   It tells the compiler to add code roughly equivalent to the bits discussed
   in ../afl-as.h.
*/

#define AFL_LLVM_PASS

#include "../config.h"
#include "../debug.h"

#include <set>
#include <queue>
#include <vector>
#include <stdio.h>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <unordered_map>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include "afl-llvm-dict-analysis.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Scalar/LICM.h"
#include "llvm/Transforms/Scalar/EarlyCSE.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/IPO/InferFunctionAttrs.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;

std::unordered_map<BasicBlock *, unsigned int> basicBlockMap;

namespace {

class AFLCoverage : public ModulePass {
public:
    static char ID;
    std::error_code error;

    AFLCoverage() :
        ModulePass(ID) {
    }

    bool runOnModule(Module &M) override;

    StringRef getPassName() const override {
        return "American Fuzzy Lop Instrumentation";
    }
};

} 

char AFLCoverage::ID = 0;

/* extract the filename*/
inline std::string genFileName(const std::string &path) {
    std::string file_name = path;
    size_t idx = path.rfind('/', path.length());

    if (idx != std::string::npos)
        file_name = path.substr(idx + 1, path.length() - idx);

    file_name = file_name.substr(0, file_name.rfind('.'));
    return file_name;
}


bool AFLCoverage::runOnModule(Module &M) {
    LLVMContext &C = M.getContext();

    IntegerType *Int8Ty = IntegerType::getInt8Ty(C);
    IntegerType *Int32Ty = IntegerType::getInt32Ty(C);

    /* Show a banner */

    char be_quiet = 0;

    if (isatty(2) && !getenv("AFL_QUIET")) {
        SAYF(cCYA "afl-llvm-pass " cBRI VERSION cRST " by <lszekeres@google.com>\n");

    } else
        be_quiet = 1;

    /* Decide instrumentation ratio */

    char *inst_ratio_str = getenv("AFL_INST_RATIO");
    unsigned int inst_ratio = 100;

    if (inst_ratio_str) {
        if (sscanf(inst_ratio_str, "%u", &inst_ratio) != 1 || !inst_ratio || inst_ratio > 100)
            FATAL("Bad value of AFL_INST_RATIO (must be between 1 and 100)");
    }

    /* Get globals for the SHM region and the previous location. Note that
       __afl_prev_loc is thread-local. */

    GlobalVariable *AFLMapPtr =
        new GlobalVariable(M, PointerType::get(Int8Ty, 0), false,
                           GlobalValue::ExternalLinkage, 0, "__afl_area_ptr");

    GlobalVariable *AFLPrevLoc = new GlobalVariable(
        M, Int32Ty, false, GlobalValue::ExternalLinkage, 0, "__afl_prev_loc",
        0, GlobalVariable::GeneralDynamicTLSModel, 0, false);

    /* Instrument all the things! */

    int inst_blocks = 0;

    /* Log file stream */
    std::string module_str = M.getName().str();
    std::string dict_fname_str = genFileName(module_str) + "_dict";
    StringRef dict_fname = StringRef(dict_fname_str);
    raw_fd_ostream fout_dict(dict_fname, error, sys::fs::OF_None);
    rfo_dict = &fout_dict;

    std::string debug_fname_str = genFileName(module_str) + ".debug";
    StringRef debug_fname = StringRef(debug_fname_str);
    raw_fd_ostream fout_debug(debug_fname, error, sys::fs::OF_None);
    rfo_debug = &fout_debug;

    std::string edge_fname_str = genFileName(module_str) + ".edge";
    StringRef edge_info_fname = StringRef(edge_fname_str);
    raw_fd_ostream fout_edge(edge_info_fname, error, sys::fs::OF_None);

    /* Fix the random seed */

    unsigned int rand_seed;
    char *rand_seed_str = getenv("AFL_RAND_SEED");

    if (rand_seed_str && sscanf(rand_seed_str, "%u", &rand_seed))
        srand(rand_seed);

    for (auto &F : M)
        for (auto &BB : F) {
            BasicBlock::iterator IP = BB.getFirstInsertionPt();
            IRBuilder<> IRB(&(*IP));

            if (AFL_R(100) >= inst_ratio) continue;

            /* Make up cur_loc */

            unsigned int cur_loc = AFL_R(MAP_SIZE);

            basicBlockMap.insert(std::pair<BasicBlock *, unsigned int>(&BB, cur_loc));

            ConstantInt *CurLoc = ConstantInt::get(Int32Ty, cur_loc);

            /* Load prev_loc */

            LoadInst *PrevLoc = IRB.CreateLoad(AFLPrevLoc);
            PrevLoc->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *PrevLocCasted = IRB.CreateZExt(PrevLoc, IRB.getInt32Ty());

            /* Load SHM pointer */

            LoadInst *MapPtr = IRB.CreateLoad(AFLMapPtr);
            MapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *MapPtrIdx =
                IRB.CreateGEP(MapPtr, IRB.CreateXor(PrevLocCasted, CurLoc));

            /* Update bitmap */

            LoadInst *Counter = IRB.CreateLoad(MapPtrIdx);
            Counter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
            Value *Incr = IRB.CreateAdd(Counter, ConstantInt::get(Int8Ty, 1));
            IRB.CreateStore(Incr, MapPtrIdx)
                ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

            /* Set prev_loc to cur_loc >> 1 */

            StoreInst *Store =
                IRB.CreateStore(ConstantInt::get(Int32Ty, cur_loc >> 1), AFLPrevLoc);
            Store->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

            inst_blocks++;
        }

      /* Trace the control flow */
    for (auto &F : M)
        for (auto &BB : F) {


            if (basicBlockMap.find(&BB) == basicBlockMap.end()) continue;
            /*Start BB ID*/
            unsigned int prev_loc = basicBlockMap[&BB];
            unsigned int cur_loc, edge_id;

            for (BasicBlock *SuccBB : successors(&BB)) {
              if (basicBlockMap.find(SuccBB) == basicBlockMap.end()) continue;
              /* End BB ID */
              cur_loc = basicBlockMap[SuccBB];
              edge_id = prev_loc >> 1 ^ cur_loc;

              fout_edge << edge_id << ":" << prev_loc << "," << cur_loc << "\n";
            }

            // fout_debug << "[DEBUG] bbID: " << cur_loc << "\n\n";
            // fout_debug << BB << "\n";
        }

    generateDictionary(M);

    fout_edge.close();
    fout_debug.close();
    fout_dict.close();

    /* Say something nice. */

    if (!be_quiet) {
        if (!inst_blocks)
            WARNF("No instrumentation targets found.");
        else
            OKF("Instrumented %u locations (%s mode, ratio %u%%).",
                inst_blocks, getenv("AFL_HARDEN") ? "hardened" : ((getenv("AFL_USE_ASAN") || getenv("AFL_USE_MSAN")) ? "ASAN/MSAN" : "non-hardened"), inst_ratio);
    }

    return true;
}

static void registerAFLPass(const PassManagerBuilder &,
                            legacy::PassManagerBase &PM) {
    PM.add(new AFLCoverage());
}

static RegisterStandardPasses RegisterAFLPass(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerAFLPass);

static RegisterStandardPasses RegisterAFLPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerAFLPass);