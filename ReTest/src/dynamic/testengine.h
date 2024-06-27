#ifndef PCTRT_TESTENGINE_H
#define PCTRT_TESTENGINE_H

#include <vector>
#include <thread>
#include <unistd.h>
#include <iostream>
#include <string>
#include <cstdio>
#include <algorithm>
#include "utils/common.h"
#include "instrument.h"
#include "static/testcase.h"
#include "static/cfg.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <generate/drivergenerator.h>

namespace PCTRT
{

class ConcurrentExecutor {
private:
    struct Task {
        std::string command;
        size_t resultIndex{};
    };

    const size_t maxThreads = 50;
    std::vector<std::string> results;
    std::vector<std::thread> threads;
    std::list<Task> tasks;
    std::mutex mtx;

public:
    explicit ConcurrentExecutor(const std::vector<std::string>& cmds) {
        for(size_t i = 0; i < cmds.size(); ++i) {
            tasks.push_back({cmds[i], i});
        }
        results.resize(cmds.size());
    }

    ~ConcurrentExecutor() {
        for (auto &thread : threads){
            if (thread.joinable()){
                thread.join();
            }
        }
    }

    void execute() {
        size_t threadCount = std::min(tasks.size(), maxThreads);
        threads.resize(threadCount);
        for(auto& thread : threads) {
            thread = std::thread(&ConcurrentExecutor::threadFunc, this);
        }
    }

    std::vector<std::string> getResults(){
        for (auto &thread : threads){
            if (thread.joinable()){
                thread.join();
            }
        }
        return results;
    }

private:
    void threadFunc() {
        while(true) {
            Task task;
            // Get the next task
            {
                std::lock_guard<std::mutex> lock(mtx);
                if(tasks.empty())
                    return;
                task = tasks.front();
                tasks.pop_front();
            }

            // Execute and write the results
            executeAndGetResults(task.command, results[task.resultIndex]);
        }
    }

    static void executeAndGetResults(const std::string& command, std::string& result){
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::cout << "Cannot execute command: " << command << std::endl;
            return;
        }
        char buffer[128];
        while (!feof(pipe)) {
            if(fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
        }
        pclose(pipe);
    }
};

class SequentialExecutor {
private:
    std::vector<std::string> cmds;
    std::vector<std::string> results;

    void executeAndGetResults(const std::string& command, std::string& result) {
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::cout << "Cannot execute command: " << command << std::endl;
            return;
        }
        char buffer[128];
        while (!feof(pipe)) {
            if(fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
        }
        pclose(pipe);
    }

public:
    explicit SequentialExecutor(const std::vector<std::string>& cmds) {
        this->cmds = cmds;
        results.resize(cmds.size());
    }

    void execute() {
        for(int i = 0; i < cmds.size(); ++i){
            executeAndGetResults(cmds[i], results[i]);
        }
    }

    std::vector<std::string> getResults(){
        return results;
    }
};

class TestEngine {
private:
    std::string srcFile;        // 待测源文件
    std::string irFile;         // 待测源文件对应的IR文件
    std::string functionName;   // 待测函数名
    CFG cfg;                    // 待测函数的CFG

    std::string driverFile;     // 生成的驱动文件
    std::string irInstrumentedFile; // 添加了路径标记的IR文件
    std::string exeFile;        // 生成的可执行文件

//    std::unique_ptr<ConcurrentExecutor> executor;
    std::unique_ptr<SequentialExecutor> executor;

public:
    TestEngine(const std::string& srcfile, const std::string& function) {
        this->srcFile = srcfile;
        this->functionName = function;
        init();
        initCFG();
    }

    ~TestEngine() = default;

    CFG& getCFG(){
        return cfg;
    }

    void setDriverFile(const std::string& driver) {
        this->driverFile = driver;
        compileDriverAndInstrument();
    }

    void setDriverFile(){
        this->driverFile = getDirPath(srcFile) + getBaseName(srcFile) + "_driver.c";
        compileDriverAndInstrument();
    }

    bool init(){
        // 将源文件编译成IR文件
        irFile = getDirPath(srcFile) + getBaseName(srcFile) + ".ll";
        if(!compileSrcToIR(srcFile, irFile)){
            return false;
        }
        // 添加驱动函数文件
        DriverGenerator driverGenerator(srcFile);
        auto driverFile = driverGenerator.generate(functionName, DRIVER_TYPE::DRIVER_EXECUTABLE);
        if(driverFile.empty()){
            std::cout << "Generate driver file failed, please input executable diver file name:" << std::endl;
            std::cin >> driverFile;
            if(!fileExists(driverFile.c_str())){
                std::cout << "Cannot find driver file: " << driverFile << std::endl;
                exit(1);
            }
        }
        return true;
    }

    bool initCFG(){
        llvm::LLVMContext ctx;
        llvm::SMDiagnostic err;
        auto ptr = llvm::parseIRFile(irFile, err, ctx);
        if(!ptr){
            std::cout << "Parse IR file " << irFile <<  " failed" << std::endl;
            return false;
        }
        llvm::Function* function = ptr->getFunction(functionName);
        if(!function){
            std::cout << "Cannot find target function in module." << std::endl;
            return false;
        }
        cfg.initGraphFromFunction(function);
        cfg.getInfoFromSrcFile(srcFile);
        return true;
    }

    bool compileDriverAndInstrument(){
        // 将驱动文件编译为IR文件
        if(!fileExists(driverFile.c_str())){
            std::cout << "Cannot find driver file: " << driverFile << std::endl;
            return false;
        }
        std::string irDriverFile = getDirPath(driverFile) + getBaseName(driverFile) + ".ll";
        if(!compileSrcToIR(driverFile, irDriverFile)){
            std::cout << "Compile driver file to llvm IR failed" << std::endl;
            return false;
        }
        // 对IR文件进行插桩
        this->irInstrumentedFile = getDirPath(driverFile) + getBaseName(driverFile) + "_instrumented.ll";
        if(!fileExists(irInstrumentedFile.c_str())) {
            llvm::LLVMContext ctx;
            llvm::SMDiagnostic err;
            auto ptr = llvm::parseIRFile(irDriverFile, err, ctx);
            if(!ptr){
                std::cout << "Parse IR file" << irDriverFile << " failed" << std::endl;
                return false;
            }
            IRPathMarker irPathMarker (std::move(ptr), functionName);
            irPathMarker.run();
            irPathMarker.dumpToFile(irInstrumentedFile);
        }
        // 将IR文件编译为可执行文件
        exeFile = getDirPath(irInstrumentedFile) + getBaseName(irInstrumentedFile);
        if(!compileIRToExec(irInstrumentedFile, exeFile)){
            return false;
        }
        return true;
    }

    void run(TestSuite& testSuite, std::vector<std::string>& outputs){
        std::vector<std::string> cmds;
        for(auto& tc : testSuite.testCases){
            std::string cmd = exeFile;
            for(auto& arg : tc.inputs){
                cmd += " \"" + arg.data + "\"";
            }
            cmds.push_back(cmd);
        }
        executor = std::make_unique<SequentialExecutor>(cmds);
        executor->execute();
        outputs = executor->getResults();
        computeCoverage(testSuite, outputs, cfg);
    }

    static void computeCoverage(TestSuite& testSuite, const std::vector<std::string>& outputs, CFG& cfg){
        auto& testCases = testSuite.testCases;
        int total_paths = static_cast<int>(cfg.getPaths().size());

        std::unordered_map<int, int> pathTestCnt;
        for(int i = 0; i < outputs.size(); ++i){
            std::string output = outputs[i];
            removeBlanks(output);
            testCases[i].setResult(output);

            int pathId = cfg.matchPathId(output);
            if(pathId != INVALID_PATH_ID && pathTestCnt.count(pathId) == 0){
                testCases[i].setPathId(pathId);
                pathTestCnt[pathId]++;
                continue;
            }
            auto pathIds = cfg.matchPathIds(output);
            if(pathIds.empty()){
                std::cout << "Error when matching testcase \n";
                std::cout << "Cannot match path id for testcase" << testCases[i].toString() << " output: " << output << std::endl;
            }else{
                int minCnt = INT_MAX;
                int minId = INVALID_PATH_ID;
                for(auto& id : pathIds){
                    if(pathTestCnt.find(id) == pathTestCnt.end()){
                        minCnt = 0;
                        minId = id;
                        break;
                    }
                    if(pathTestCnt[id] < minCnt){
                        minCnt = pathTestCnt[id];
                        minId = id;
                    }
                }
                testCases[i].setPathId(minId);
                pathTestCnt[minId]++;
            }
        }
        testSuite.setCoverage(static_cast<double>(pathTestCnt.size()) / total_paths);
    }

    static void removeBlanks(std::string& str){
        str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char x) {return std::isspace(x);}), str.end());
    }
};

}; // namespace PCTRT

#endif //PCTRT_TESTENGINE_H
