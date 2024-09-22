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

using namespace llvm;


static std::set<Value *> GVSet;

extern raw_fd_ostream *rfo_dict;
extern raw_fd_ostream *rfo_debug;
extern std::unordered_map<BasicBlock *, unsigned int> basicBlockMap;

void generateDictionary(Module &M);

void magicNumHandler(Instruction &I);

int64_t magicNumDefUseChain(Instruction &I);

void switchHandler(Instruction &I);

bool isMagicStringCmpFunc(llvm::StringRef functName);

void updateStringUsers(std::set<User *> *userSet, std::stack<User *> *userStack, User *currU);