#include <set>
#include <queue>
#include <vector>
#include <stdio.h>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <unordered_map>

#include "afl-llvm-dict-analysis.h"

raw_fd_ostream *rfo_dict;
raw_fd_ostream *rfo_debug;

#define npos std::string::npos

#define INTERESTING_8                                      \
    -128,    /* Overflow signed 8-bit when decremented  */ \
        -1,  /*                                         */ \
        0,   /*                                         */ \
        1,   /*                                         */ \
        16,  /* One-off with common buffer size         */ \
        32,  /* One-off with common buffer size         */ \
        64,  /* One-off with common buffer size         */ \
        100, /* One-off with common buffer size         */ \
        127  /* Overflow signed 8-bit when incremented  */

#define INTERESTING_16                                      \
    -32768,   /* Overflow signed 16-bit when decremented */ \
        -129, /* Overflow signed 8-bit                   */ \
        128,  /* Overflow signed 8-bit                   */ \
        255,  /* Overflow unsig 8-bit when incremented   */ \
        256,  /* Overflow unsig 8-bit                    */ \
        512,  /* One-off with common buffer size         */ \
        1000, /* One-off with common buffer size         */ \
        1024, /* One-off with common buffer size         */ \
        4096, /* One-off with common buffer size         */ \
        32767 /* Overflow signed 16-bit when incremented */

#define INTERESTING_32                                            \
    -2147483648LL,  /* Overflow signed 32-bit when decremented */ \
        -100663046, /* Large negative number (endian-agnostic) */ \
        -32769,     /* Overflow signed 16-bit                  */ \
        32768,      /* Overflow signed 16-bit                  */ \
        65535,      /* Overflow unsig 16-bit when incremented  */ \
        65536,      /* Overflow unsig 16 bit                   */ \
        100663045,  /* Large positive number (endian-agnostic) */ \
        2147483647  /* Overflow signed 32-bit when incremented */

static int64_t interesting_32[] = {INTERESTING_8, INTERESTING_16, INTERESTING_32};

#define FUNC2S(I) (isa<BinaryOperator>(I) || isa<OverflowingBinaryOperator>(I) || isa<PossiblyExactOperator>(I))

bool diffInterestingValue(int64_t constVal) {
    for (int64_t i : interesting_32) {
        if (i == constVal) return false;
    }
    return true;
}

bool isI2S(Instruction &UseInst) {
    bool isI2SRst = true;
    std::queue<Instruction *> icmpUseQueue;
    std::set<Instruction *> icmpOpSet;
    icmpUseQueue.push(&UseInst);
    while (!icmpUseQueue.empty() && isI2SRst) {
        Instruction *currUseInst = icmpUseQueue.front();
        icmpUseQueue.pop();

        if (icmpOpSet.count(currUseInst) == 0) {
            icmpOpSet.insert(currUseInst);
        } else {
            continue;
        }

        if (FUNC2S(currUseInst)) isI2SRst = false;

        for (Value *currValue : currUseInst->operand_values()) {
            if (Instruction *currUseOpInst = dyn_cast<Instruction>(currValue)) {
                icmpUseQueue.push(currUseOpInst);
            }
        }
    }
    return isI2SRst;
}

// check icmp instruction def-use chain
int64_t magicNumDefUseChain(Instruction &I) {
    int64_t cmpValue = 0;

    for (Value *value : I.operand_values()) {
        if (ConstantInt *ci = dyn_cast<ConstantInt>(value)) {
            if (!diffInterestingValue(ci->getSExtValue())) return 0;
            cmpValue = ci->getSExtValue();
        } 
        // else if (Instruction *UseInst = dyn_cast<Instruction>(value)) {
            // if (!isI2S(*UseInst)) return 0;
        // }
    }
    return cmpValue;
}

void magicNumHandler(Instruction &I) {
    if (CmpInst *cmpInst = dyn_cast<CmpInst>(&I)) {
        int64_t cmpValue = magicNumDefUseChain(I);
        // the cmp value is invalid or the cmp value is not input-to-state
        if (!cmpValue) return;

        for (User *U : I.users()) {
            if (BranchInst *brInst = dyn_cast<BranchInst>(U)) {
                if (brInst->isUnconditional()) continue;
                BasicBlock *succBB = nullptr;

                switch (cmpInst->getPredicate()) {
                case CmpInst::ICMP_EQ:
                    succBB = brInst->getSuccessor(1);
                    break;
                case CmpInst::ICMP_NE:
                    succBB = brInst->getSuccessor(0);
                    break;
                default:
                    break;
                }

                if (!succBB) break;

                unsigned int currBBID = basicBlockMap[cmpInst->getParent()];
                unsigned int succBBID = basicBlockMap[succBB];

                int succEdge = currBBID >> 1 ^ succBBID;

                *rfo_debug << "icmp: " << cmpValue << "," << succEdge << "\n";
                *rfo_debug << *(cmpInst->getParent()) << "\n";
                *rfo_debug << "[bbID]: " << currBBID << "\n\n";

                *rfo_dict << "icmp:" << cmpValue << "," << succEdge << "," << currBBID << "," << succBBID << "\n";
            }
        }
    }
}

void switchHandler(Instruction &I) {
    if (SwitchInst *switchInst = dyn_cast<SwitchInst>(&I)) {
        // for (Value *value : I.operand_values()) {
        //     if (Instruction *UseInst = dyn_cast<Instruction>(value))
        //         if (!isI2S(*UseInst)) return;
        // }

        unsigned int currBBID = basicBlockMap[I.getParent()];
        unsigned int succBBID = basicBlockMap[switchInst->getDefaultDest()];
        int succEdge = currBBID >> 1 ^ succBBID;

        for (auto &switchCase : switchInst->cases()) {
            if (ConstantInt *ci = dyn_cast<ConstantInt>(switchCase.getCaseValue())) {
                int64_t switchCaseValue = ci->getSExtValue();
                if (!diffInterestingValue(switchCaseValue)) continue;
                *rfo_debug << "switch:" << switchCaseValue << "," << succEdge << "," << currBBID << "," << succBBID << "\n";
                *rfo_dict << "switch:" << switchCaseValue << "," << succEdge << "," << currBBID << "," << succBBID << "\n";
            }
        }
        *rfo_debug << *(I.getParent()) << "\n";
        *rfo_debug << "[bbID]: " << currBBID << "\n\n";
    }
}

void magicStringHandler(GlobalVariable &G) {
    StringRef constStrData;
    std::set<User *> stringUserSet;
    std::stack<User *> stringUserStack;

    if (!G.isConstant()) return;

    if (ConstantDataArray *constDataArray = dyn_cast<ConstantDataArray>(G.getInitializer())) {
        if (!constDataArray->isCString()) return;
        constStrData = constDataArray->getAsCString();
    } else {
        return;
    }

    for (User *U : G.users()) {
        if (stringUserSet.count(U)) continue;
        stringUserStack.push(U);
        stringUserSet.insert(U);
    }

    bool hasStrCmp = false;
    bool hasBr = false;
    BasicBlock *succBB = nullptr;
    Instruction *stringBrInst = nullptr;

    while (!stringUserStack.empty()) {
        User *currU = stringUserStack.top();
        stringUserStack.pop();

        std::string currUStr;
        raw_string_ostream rso(currUStr);
        currU->print(rso);
        if (currUStr.find("icmp") == npos && (currUStr.find("cmp") != npos || currUStr.find("strstr") != npos)) {
            hasStrCmp = true;
            *rfo_debug << "\n[DEBUG] Func " << currUStr << "\n";
        } else if (BranchInst *brInst = dyn_cast<BranchInst>(currU)) {
            if (!brInst->isUnconditional() && hasStrCmp) {
                succBB = brInst->getSuccessor(1);
                unsigned int succBBID = basicBlockMap[succBB];
                BasicBlock *currBB = brInst->getParent();
                unsigned int currBBID = basicBlockMap[currBB];
                int succEdge = currBBID >> 1 ^ succBBID;

                *rfo_dict << "strcmp:\"" << constStrData << "\"," << succEdge << "\n";
                *rfo_debug << "strcmp:\"" << constStrData << "\"," << succEdge << "," << currBBID << "," << succBBID << "\n";
                // *rfo_debug << *(brInst->getParent()) << "\n";
                // *rfo_debug << "[bbID]" << currBBID << "\n";
            }
        }

        for (User *recU : currU->users()) {
            if (stringUserSet.count(recU)) continue;
            stringUserStack.push(recU);
            stringUserSet.insert(recU);
        }
    }
}

void generateDictionary(Module &M) {
    for (auto &G : M.globals()) {
        // magicStringHandler(G);
    }

    for (auto &F : M) {
        for (auto &BB : F) {
            *rfo_debug << BB << "\n";
            *rfo_debug << "[bbID]" << basicBlockMap[&BB] << "\n";
            for (auto &I : BB) {
                magicNumHandler(I);
                // switchHandler(I);
            }
        }
    }
}