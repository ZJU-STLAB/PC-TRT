#ifndef RETEST_TESTGENERATOR_H
#define RETEST_TESTGENERATOR_H

#include <string>
#include <utility>
#include <vector>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <nlohmann/json.hpp>

#include "generate/kleedrivergenerator.h"
#include "generate/drivergenerator.h"
#include "generate/pathinstrument.h"
#include "utils/config.h"
#include "utils/common.h"

namespace retest {
using json = nlohmann::json;

class TestGenerator {
private:
    std::string srcName;
    std::string functionName;
    std::vector<int> paths;

public:
    explicit TestGenerator(std::string src, std::string func, std::vector<int> paths)
      : srcName(std::move(src)), functionName(std::move(func)), paths(std::move(paths)) {}

    bool run(){
        // 首先生成驱动函数
        auto kleeDriverGenerator = DriverGenerator(srcName);
        bool success = kleeDriverGenerator.generate(functionName, DRIVER_TYPE::DRIVER_KLEE_SE);
        std::string driverFile;
        if(!success){
            // 手动输入驱动函数文件名

            return false;
        }
        // 将驱动函数文件编译为LLVM IR
        driverFile = getDirPath(srcName) + getBaseName(srcName) + "_klee_driver.c";
        std::string driverIRFile = getDirPath(srcName) + getBaseName(srcName) + "_klee_driver.ll";
        std::cout << "Compiling driver file: " << driverFile << " to " << driverIRFile << std::endl;
        bool compiled = compileSrcToIR(driverFile, driverIRFile);
        RETEST_ASSERT(compiled, "Failed to compile driver file.");
        // 对LLVM IR插桩
        std::string klee_cmd = KLEE_SCRIPT;
        for(int path_id : paths){
            llvm::LLVMContext ctx;
            llvm::SMDiagnostic err;
            auto ptr = llvm::parseIRFile(driverIRFile, err, ctx);
            if(!ptr){
                std::cerr << "Failed to parse IR file: " << driverIRFile << std::endl;
                return false;
            }
            PathInstrument pi(std::move(ptr), functionName);
            std::string irFileName = getDirPath(srcName) + functionName + "_klee_instrumented_" + std::to_string(path_id) + ".ll";
            pi.generateInstrumentedIR(path_id, irFileName);
            
            klee_cmd += " " + irFileName;
        }
        // 使用klee对插桩后的IR文件进行符号执行
        std::cout << "klee_cmd: " << klee_cmd << "\n";
        std::cout << "Running klee on instrumented IR files..." << std::endl;
        int ret = system(klee_cmd.c_str());
        if(ret != 0){
            std::cerr << "Failed to run klee on instrumented IR files." << std::endl;
            return false;
        }
        return true;
    }


}; // class TestGenerator

} // namespace retest

#endif