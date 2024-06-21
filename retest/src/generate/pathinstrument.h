#ifndef RETEST_PATHINSTRUMENT_H
#define RETEST_PATHINSTRUMENT_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include "static/cfg.h"
#include "utils/common.h"

namespace retest
{

class RollingHashIndex {
private:
    size_t A;
    const size_t M = 1e9+7;
    std::unordered_map<size_t, std::vector<int>> index;
    std::vector<std::vector<int>> sequences;
    int hash_len {1};

    size_t hash(size_t hashA, size_t hashB) const {
        auto h1 = hashA + 1LL;
        auto h2 = hashB + 1LL;
        return (h1 * A + h2) % M;
    }

    void updateIndex(int len) {
        if(hash_len >= len) {
            return;
        }
        hash_len++;
        for(int idx = 0; idx < sequences.size(); ++idx){
            const auto& seq = sequences[idx];
            int n = static_cast<int>(seq.size());
            if(n < hash_len){
                continue;
            }
            for(int i = 0; i <= n - hash_len; ++i){
                size_t h = 0;
                for(int j = i; j < i + hash_len; ++j){
                    h = hash(h, seq[j]) % M;
                }
                index[h].push_back(idx);
//                std::cout << "Insert ";
//                for(int k = i; k < i + hash_len; ++k){
//                    if(k != i){
//                        std::cout << " -> ";
//                    }
//                    std::cout << seq[k];
//                }
//                std::cout << " in path " << idx << " with hashV " << h << "\n";
            }
        }
    }

public:
    explicit RollingHashIndex(const std::vector<std::vector<int>>& seqs, size_t max_item_size = 131)
    : sequences(seqs), A(max_item_size) {
        updateIndex(2);
    }

    std::vector<int> getShortestUniqueSubSeq(int id) {
        RETEST_ASSERT(id >= 0 && id < sequences.size(), "Invalid sequence id");
        const std::vector<int>& seq = sequences[id];
        int n = static_cast<int>(seq.size());
        for(int l = 2; l <= seq.size(); ++l) {
            for(int i = 0; i <= n - l; ++i){
                size_t h = 0;
                for(int j = i; j < i + l; ++j){
                    h = hash(h, seq[j]);
                }
                if(index.count(h) == 0){
                    std::cout << "Not found seq from " << i << " to " << i + l - 1 << " in path " << id << std::endl;
                    return {};
                }
                if(index[h].size() == 1 && index[h].front() == id){
                    return {seq.begin() + i, seq.begin() + i + l};
                }
            }
            updateIndex(l + 1);
        }
        return {};
    }

};

class PathInstrument {
private:
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<CFG> cfg;
    std::string functionName;
    llvm::Function* function {nullptr};

    std::unordered_map<int, llvm::BasicBlock*> blockMap;
    std::vector<std::vector<int>> paths;

    std::unordered_set<int> triggerBlocks;  // 需要插桩访问的基本块
    std::unordered_set<int> exitBlocks; // 需要插桩退出搜索的基本块

    std::unique_ptr<RollingHashIndex> rollingHashIndex {nullptr};
    llvm::FunctionType *triggerFuncType {nullptr};
    llvm::FunctionType *exitFuncType {nullptr};

public:
    explicit PathInstrument(std::unique_ptr<llvm::Module> mod, std::string funcName)
    : module(std::move(mod))
    , functionName(std::move(funcName))
    {
        function = module->getFunction(functionName);
        RETEST_ASSERT(function != nullptr, "Function not found");
        cfg = std::make_unique<CFG>();
        cfg->initGraphFromFunction(function);
        init();
    }

    ~PathInstrument() = default;

    void init(){
        int cnt = 0;
        for(auto& bb : *function){
            blockMap[cnt++] = &bb;
        }
        triggerFuncType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(module->getContext()),
            { llvm::Type::getInt32Ty(module->getContext()) },
            false
        );
        exitFuncType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(module->getContext()),
            { llvm::Type::getInt32Ty(module->getContext()) },
            false
        );
        initPaths();
    }

    void initPaths(){
        const auto& cfgPaths = cfg->getPaths();
        for(auto& path : cfgPaths){
            paths.push_back(path.to_vector_of_nodes());
        }
        rollingHashIndex = std::make_unique<RollingHashIndex>(paths, cfg->getSize());
    }

    // 设置目标路径中需要插桩的基本块
    bool setPathToInstrument (int pathId) {
        if(pathId < 0 || pathId >= cfg->getPaths().size()){
            return false;
        }
        auto seq = cfg->getPaths()[pathId].to_vector_of_nodes();
        auto subSeq = rollingHashIndex->getShortestUniqueSubSeq(pathId);
        if(subSeq.empty()){
            return false;
        }
        // 将seq中有每个其他后继节点的基本块后继节点插入到exitBlocks中
        for(int i = 0; i < seq.size() - 1; ++i){
            auto successors = cfg->getBlockSuccessors(seq[i]);
            int nextId = seq[i + 1];
            for(auto& succ : successors){
                if (succ != nextId) {
                    exitBlocks.insert(succ);
                }
            }
        }
        for(int i = 0; i < subSeq.size(); ++i){
            int mask = 0;
            mask |= (static_cast<int>(i != 0) << 1);             // 边的到达点
            mask |= (static_cast<int>(i != subSeq.size() - 1));  // 边的起始点
            insertTriggerFunctionCall(blockMap[subSeq[i]], mask);
        }
        // 对exitBlocks中的每个基本块都插桩
        for(auto& bbId : exitBlocks){
            auto bb = blockMap[bbId];
            insertExitFunctionCall(bb, 0);
        }
        return true;
    }

    void insertTriggerFunctionCall(llvm::BasicBlock* bb, int arg){
        // 为基本块插入trigger函数调用
        llvm::FunctionCallee triggerFunc = module->getOrInsertFunction(
            "klee_path_trigger",
            triggerFuncType
        );
        llvm::IRBuilder<> irBuilder(&*bb->getFirstInsertionPt());
        // 插入trigger函数调用，参数是arg
        irBuilder.CreateCall(triggerFunc, {llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, arg))});
    }

    void insertExitFunctionCall(llvm::BasicBlock* bb, int arg){
        // 为基本块插入exit函数调用
        llvm::FunctionCallee exitFunc = module->getOrInsertFunction(
            "klee_path_conditional_exit",
            exitFuncType
        );
        llvm::IRBuilder<> irBuilder(&*bb->getFirstInsertionPt());
        irBuilder.CreateCall(exitFunc, {llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, arg))});
    }

    // 为需要的目标路径生成插桩后的IR文件
    void generateInstrumentedIR(int pathId, const std::string& filePath){
        if(setPathToInstrument(pathId)){
            std::error_code EC;
            llvm::raw_fd_ostream os(filePath, EC);
            module->print(os, nullptr);
            os.close();
        }
    }

};

}

#endif //RETEST_PATHINSTRUMENT_H
