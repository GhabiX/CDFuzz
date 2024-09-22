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
    }
    return cmpValue;
}

bool isMagicStringCmpFunc(llvm::StringRef functName) {
    bool result = functName.contains("strcmp") || functName.contains("memcmp") || functName.contains("strncmp") || functName.contains("strcasecmp") || functName.contains("strstr") || functName.contains("strncasecmp") || functName.contains("cmp");
    return result;
}

void updateStringUsers(std::set<User *> *userSet, std::stack<User *> *userStack, User *currU) {
    if (userSet->count(currU)) return;
    userStack->push(currU);
    userSet->insert(currU);
}

char *convertIntegertoHex(int64_t data) {
    char *hexData = (char *)malloc(64);
    sprintf(hexData, "%.2lX", data);
    std::string strHexData = std::string(hexData);
    int strHexDataLen = strHexData.length();
    for (int i = strHexDataLen - 2; i >= 0; i -= 2) {
        strHexData.insert(i, "\\x");
    }
    sprintf(hexData, "%s", strHexData.c_str());
    return hexData;
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

                unsigned int succEdge = currBBID >> 1 ^ succBBID;

                *rfo_dict << "icmp_siblingEdge_" << succEdge << "_val=\"" << convertIntegertoHex(cmpValue) << "\"\n";
                *rfo_debug << "icmp:" << convertIntegertoHex(cmpValue) << "," << succEdge << "," << currBBID << "," << succBBID << "\n";
            }
        }
    }
}

void switchHandler(Instruction &I) {
    if (SwitchInst *switchInst = dyn_cast<SwitchInst>(&I)) {

        unsigned int currBBID = basicBlockMap[I.getParent()];
        unsigned int succDefaultBBID = basicBlockMap[switchInst->getDefaultDest()];
        unsigned int succDefaultEdge = currBBID >> 1 ^ succDefaultBBID;

        for (auto &switchCase : switchInst->cases()) {
            if (ConstantInt *ci = dyn_cast<ConstantInt>(switchCase.getCaseValue())) {
                int64_t switchCaseValue = ci->getSExtValue();
                if (!diffInterestingValue(switchCaseValue)) continue;
                unsigned int caseBBID = basicBlockMap[switchCase.getCaseSuccessor()];
                unsigned int caseEdge = currBBID >> 1 ^ caseBBID;

                *rfo_debug << "switch:" << switchCaseValue << "," << succDefaultEdge << "," << caseEdge << "," << currBBID << "," << caseBBID << "\n";
                *rfo_dict << "switch_defaultEdge_" << succDefaultEdge << "_caseEdge_" << caseEdge << "_val=\"" << convertIntegertoHex(switchCaseValue) << "\"\n";
            }
        }
    }
}

void magicStringHandler(GlobalVariable &G) {
    std::set<User *> userSet;
    std::stack<User *> userStack;

    if (!G.isConstant() || !G.hasInitializer() || !isa<ConstantDataArray>(G.getInitializer())) return;

    for (User *U : G.users()) updateStringUsers(&userSet, &userStack, U);

    bool hasStrCmp = false;

    while (!userStack.empty()) {
        User *currU = userStack.top();
        userStack.pop();

        if (CallInst *currUCallInst = dyn_cast<CallInst>(currU)) {
            if (currUCallInst->getCalledFunction() && currUCallInst->getCalledFunction()->hasName()) {
                StringRef functName = currUCallInst->getCalledFunction()->getName();

                if (isMagicStringCmpFunc(functName)) {
                    hasStrCmp = true;
                } else if (functName.contains("memcpy") || functName.contains("strcpy")) {
                    for (auto arg = currUCallInst->arg_begin(); arg != currUCallInst->arg_end(); ++arg) {
                        if (BitCastInst *bitCastInst = dyn_cast<BitCastInst>(arg)) {
                            for (auto op = bitCastInst->op_begin(); op != bitCastInst->op_end(); ++op) {
                                if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(op)) {
                                    if (User *allocaInstUser = dyn_cast<AllocaInst>(allocaInst))
                                        updateStringUsers(&userSet, &userStack, allocaInstUser);
                                }
                            }
                        }
                    }
                }
            }
        } else if (CmpInst *cmpInst = dyn_cast<CmpInst>(currU)) {
            for (User *cmpInstUser : cmpInst->users()) {
                if (BranchInst *brInst = dyn_cast<BranchInst>(cmpInstUser)) {

                    if (brInst->isUnconditional() || !hasStrCmp) continue;
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

                    unsigned int succEdge = currBBID >> 1 ^ succBBID;
                    StringRef constStrData;
                    ConstantDataArray *constDataArray = dyn_cast<ConstantDataArray>(G.getInitializer());

                    StringRef data = constDataArray->getRawDataValues();
                    int dataSize = data.size();
                    if (dataSize > 1024) continue;
                    char *hexData = (char *)malloc(4 * dataSize + 16);
                    char *startPtr = hexData;
                    for (auto currByte = data.bytes_begin(); currByte != data.bytes_end(); ++currByte) {
                        sprintf(startPtr, "\\x%.2X", *currByte);
                        startPtr += 4;
                    }
                    constStrData = StringRef(hexData);

                    *rfo_dict << "strcmp_siblingEdge_" << succEdge << "_val=\"" << constStrData << "\"\n";
                    *rfo_debug << "strcmp:\"" << constStrData << "\"," << succEdge << "," << currBBID << "," << succBBID << "\n";

                }
            }
        }

        for (User *recU : currU->users()) updateStringUsers(&userSet, &userStack, recU);
    }
}

void generateDictionary(Module &M) {
    for (auto &G : M.globals()) {
        magicStringHandler(G);
    }

    for (auto &F : M) {
        for (auto &BB : F) {
            for (auto &I : BB) {
                magicNumHandler(I);
                switchHandler(I);
            }
        }
    }
}