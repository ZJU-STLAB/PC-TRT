#ifndef RETEST_INSTRUMENT_H
#define RETEST_INSTRUMENT_H

#include <memory>
#include <utility>
#include <string>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include "utils/common.h"

namespace retest
{

class IRPathMarker {
private:
    std::unique_ptr<llvm::Module> module;
    std::string functionName;
    size_t cnt {0};
    llvm::IntegerType *Int8Ty {nullptr};
    llvm::IntegerType *Int32Ty {nullptr};
    llvm::IntegerType *Int64Ty {nullptr};
    llvm::ArrayType* CharArrayTy {nullptr};
    llvm::LLVMContext* context {nullptr};
    llvm::GlobalVariable* charArray {nullptr};  // 全局的char数组

public:
    explicit IRPathMarker(std::unique_ptr<llvm::Module> mod, std::string funcName)
        : module(std::move(mod))
        , cnt(0)
        , functionName(std::move(funcName))
        {}

    ~IRPathMarker() = default;

    void run() {
        initialize();
        instrumentInTargetFunction();
        instrumentInMainFunction();
    }

    void initialize(){
        // 获取目标函数基本块数目
        auto function = module->getFunction(functionName);
        RETEST_ASSERT(function != nullptr, "Cannot find target function in module.");
        this->cnt = function->size();
        context = &(module->getContext());
        // 初始化类型
        Int8Ty = llvm::IntegerType::getInt8Ty(*context);
        Int32Ty = llvm::IntegerType::getInt32Ty(*context);
        Int64Ty = llvm::IntegerType::getInt64Ty(*context);
        CharArrayTy = llvm::ArrayType::get(Int8Ty, cnt + 1);

        // 创建全局的char数组用于存储每个block的遍历情况
        std::string str(cnt, '0');
        // llvm会自动对charArray析构
        charArray = new llvm::GlobalVariable(
            *module, CharArrayTy, false,
            llvm::GlobalValue::ExternalLinkage,
            llvm::ConstantDataArray::getString(*context, str),
            "__block_marker__"
        );
        charArray->setDSOLocal(true);
        charArray->setAlignment(llvm::Align(1));
    }

    void instrumentInTargetFunction() {
        auto function = module->getFunction(functionName);
        int idx = 0;
        // 在每个block中插桩
        for (auto& block : *function) {
            // 在第一个instruction前插入指令
            llvm::IRBuilder<> irBuilder(&*block.getFirstInsertionPt());
            // 获取字符数组地址, 并store
            llvm::Value* idxIntV = llvm::ConstantInt::get(Int64Ty, idx);
            llvm::Type* ty = charArray->getType()->getElementType();
            llvm::Value* indexes[] = { llvm::ConstantInt::get(Int32Ty, 0), idxIntV};
            auto charArrayIdxAddr = irBuilder.CreateInBoundsGEP(ty, charArray, indexes);
            irBuilder.CreateStore(llvm::ConstantInt::get(Int8Ty, 49), charArrayIdxAddr);
            idx++;
        }
    }

    void instrumentInMainFunction(){
        // "printf" function 类型
        llvm::FunctionType *printfFuncType = llvm::FunctionType::get(
            Int32Ty,                    //return type : int
            { Int8Ty->getPointerTo() }, //args: (char*, ...)
            true                        //variable args
        );

        llvm::FunctionCallee printf = module->getOrInsertFunction(
            "printf",
            printfFuncType
        );

        auto function = module->getFunction("main");
        RETEST_ASSERT(function != nullptr, "Cannot find main function!");

        llvm::BasicBlock* exit = &function->back();
        for(auto& instruction : *exit){
            // call printf function before return instruction.
            if(instruction.getOpcode() == llvm::Instruction::Ret){
                llvm::IRBuilder<> irBuilder(&instruction);
                llvm::GlobalValue* stringOutputFormat = irBuilder.CreateGlobalString("%s", "__string_fmt__");
                llvm::Type *ty = stringOutputFormat->getType()->getElementType();
                llvm::Value* zeros[] = { llvm::ConstantInt::get(Int32Ty, 0), llvm::ConstantInt::get(Int32Ty, 0)};
                auto fmtStrAddr = irBuilder.CreateInBoundsGEP(ty, stringOutputFormat, zeros);
                irBuilder.CreateCall(printf, {fmtStrAddr, charArray});
            }
        }
    }

    void print(){
        module->print(llvm::outs(), nullptr);
    }

    void dumpToFile(const std::string& file){
        std::error_code err;
        llvm::raw_fd_ostream out(file, err);
        module->print(out, nullptr);
        out.close();
    }
};
}

#endif //RETEST_INSTRUMENT_H
