#ifndef RETEST_TESTENGINE_H
#define RETEST_TESTENGINE_H

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

namespace retest
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

// 把源文件编译成LLVM IR并添加驱动文件和插桩后的可执行文件
class InitEngine {
private:
    std::string srcFile;
    std::string functionName;
    std::string baseName;
    std::string workDir;

public:
    InitEngine(const std::string& srcfile, const std::string& function){
        this->srcFile = srcfile;
        this->functionName = function;
        this->baseName = getBaseName(srcfile);
        this->workDir = getDirPath(srcfile);
    }

    bool run(){
        std::string irFile = workDir + baseName + ".ll";
        if(!compileSrcToIR(srcFile, irFile)){
            return false;
        }
        if(!addDriver()){
            return false;
        }
        return true;
    }

    // bool compileToIR() {
    //     if(!fileExists(srcFile.c_str())){
    //         std::cout << "Cannot find source file: " << srcFile << std::endl;
    //         return false;
    //     }
    //     const std::string cmd = "clang -S -emit-llvm -g " + srcFile + " -o " + workDir + "/" + baseName + ".ll";
    //     int ret = system(cmd.c_str());
    //     if(ret != 0){
    //         std::cout << "Compile source file to llvm IR failed" << std::endl;
    //         return false;
    //     }
    //     return true;
    // }

    bool addDriver() {
        DriverGenerator driverGenerator(srcFile);
        return driverGenerator.generate(functionName, DRIVER_TYPE::DRIVER_EXECUTABLE);
    }

    bool compileDriverAndInstrument(){
        // 将驱动文件编译为IR文件
        std::string driverFile = workDir + "/" + baseName + "_driver.c";
        if(!fileExists(driverFile.c_str())){
            std::cout << "Cannot find driver file: " << driverFile << std::endl;
            return false;
        }
        std::string irDriverFile = getDirPath(driverFile) + getBaseName(driverFile) + ".ll";
        if(!compileSrcToIR(driverFile, irDriverFile)) {
            std::cout << "Compile driver file to llvm IR failed" << std::endl;
            return false;
        }
        // 对IR文件进行插桩
        std::string irInstrumentedFile = getDirPath(driverFile) + getBaseName(driverFile) + "_instrumented.ll";
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
        std::string exeFile = getDirPath(srcFile) + getBaseName(srcFile) + "_driver_instrumented";
        if(!compileIRToExec(irInstrumentedFile, exeFile)){
            return false;
        }
        return true;
    }

    // bool compileIRToExe(){
    //     std::string irInstrumentedFile = getDirPath(srcFile) + getBaseName(srcFile) + "_driver_instrumented.ll";
    //     std::string exeFile = getDirPath(srcFile) + getBaseName(srcFile) + "_driver_instrumented";
    //     if(fileExists(exeFile.c_str())) {
    //         std::cout << "Executable file already exists: " << exeFile << std::endl;
    //         return true;
    //     }
    //     const std::string cmd = "clang " + irInstrumentedFile + " -o " + exeFile;
    //     int ret = system(cmd.c_str());
    //     if(ret != 0){
    //         std::cout << "Compile IR file to executable file failed" << std::endl;
    //         return false;
    //     }
    //     return true;
    // }

    static bool modifyMainFunction(const std::string& fileName) {
        std::vector<std::string> lines;
        if(!readLinesFromFile(fileName, lines, false)){
            std::cout << "Cannot read file: " << fileName << std::endl;
            return false;
        }
        std::ofstream outputFile(fileName);
        if(!outputFile.is_open()){
            std::cout << "Cannot open file: " << fileName << std::endl;
            return false;
        }
        for(auto& line : lines){
            if(line.find("main") != std::string::npos && line.find('(') != std::string::npos){
                // 给line中的"main"两侧加上"__"
                size_t pos = line.find("main");
                if(pos != std::string::npos && (pos - 1 > 0 && line[pos - 1] != '_')) {
                    line.insert(pos, "__");
                    line.insert(pos + 6, "__");
                }
            }
            outputFile << line << std::endl;
        }
        outputFile.close();
        return true;
    }

    static std::string getFunctionDeclaration(const std::string& fileName, const std::string& functionName) {
        std::ifstream input(fileName);
        if(!input.is_open()){
            std::cout << "Cannot open file: " << fileName << std::endl;
            return {};
        }
        std::string line;
        while(std::getline(input, line)) {
            if (line.find(functionName) != std::string::npos && line.find('{') != std::string::npos) {
                return line;
            }
        }
        return {};
    }

    static std::string getFunctionCallString(const std::string& functionDecl, const std::string& functionName) {
        std::stringstream ss;
        auto start = functionDecl.find(functionName);
        std::string returnType = functionDecl.substr(0, start);
        std::vector<std::pair<std::string, std::string>> params;
        auto paraStart = functionDecl.find('(', start);
        auto paraEnd = functionDecl.find(')', paraStart);
        RETEST_ASSERT(paraStart != std::string::npos && paraEnd != std::string::npos, true);
        if(paraStart != paraEnd - 1) {
            //用逗号分隔所有参数
            std::vector<int> commaPos;
            for (auto i = paraStart; i < paraEnd; ++i) {
                if (functionDecl[i] == ',') {
                    commaPos.push_back(static_cast<int>(i));
                }
            }
            commaPos.push_back(static_cast<int>(paraEnd));
            int lastPos = static_cast<int>(paraStart) + 1;
            for (int idx: commaPos) {
                int endPos = idx;
                while (functionDecl[lastPos] == ' ') {
                    lastPos++;
                }
                while (functionDecl[endPos] == ' ' || functionDecl[endPos] == ',' || functionDecl[endPos] == ')') {
                    endPos--;
                }
                int blankPos = endPos;
                while (functionDecl[blankPos] != ' ' && functionDecl[blankPos] != '*') {
                    blankPos--;
                }
                std::string type, name;
                for (int i = lastPos; i <= blankPos; ++i) {
                    if (functionDecl[i] != ' ') {
                        type += functionDecl[i];
                    }
                }
                for (int i = blankPos + 1; i <= endPos; ++i) {
                    if (functionDecl[i] != ' ') {
                        name += functionDecl[i];
                    }
                }
                // std::cout << "type: " << type << ", name: " << name << ", nameSize: " << name.size() << std::endl;
                params.emplace_back(type, name);
                lastPos = idx + 1;
            }
            int idx = 1;
            for (const auto &[type, name]: params) {
                ss << TEMPLATE_BLANK_STRING << type << " " << name << " = ";
                if (type == "int") {
                    ss << "atoi(argv[" << idx++ << "]);\n";
                } else if (type == "uint32_t") {
                    ss << "atou(argv[" << idx++ << "]);\n";
                } else if (type == "int*") {
                    ss << "parse_string_to_array(argv[" << idx++ << "]);\n";
                } else if (type == "char*") {
                    ss << "copy(argv[" << idx++ << "]);\n";
                } else if (type == "char") {
                    ss << "argv[" << idx++ << "][0];\n";
                } else {
                    ss << "argv[" << idx++ << "];\n";
                }
            }
        }
        // 调用函数
        ss << TEMPLATE_BLANK_STRING << returnType << "retVal = " << functionName << "(";
        for(int i = 0; i < params.size(); ++i){
            ss << params[i].second;
            if(i != params.size() - 1){
                ss << ", ";
            }
        }
        ss << ");\n";
        // 如果参数中有指针类型，需要释放内存
        for(const auto& [type, name] : params){
            if(type.find('*') != std::string::npos){
                ss << TEMPLATE_BLANK_STRING << "free(" << name << ");\n";
            }
        }
        // 如果返回值是指针类型，需要释放内存
        if(returnType.find('*') != std::string::npos){
            ss << TEMPLATE_BLANK_STRING << "free(retVal);\n";
        }
        return ss.str();
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
        // auto initEngine = std::make_unique<InitEngine>(srcfile, function);
        // initEngine->run();
        // compileSrcToIR();
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

    // // 将源文件编译为IR文件
    // bool compileSrcToIR(){
    //     if(!fileExists(srcFile.c_str())){
    //         std::cout << "Cannot find source file: " << srcFile << std::endl;
    //         return false;
    //     }
        
    //     if(fileExists(irFile.c_str())) {
    //         std::cout << "IR file already exists: " << irFile << std::endl;
    //         return true;
    //     }
    //     const std::string cmd = "clang -S -emit-llvm -g " + srcFile + " -o " + irFile;
    //     int ret = system(cmd.c_str());
    //     if(ret != 0){
    //         std::cout << "Compile source file to llvm IR failed" << std::endl;
    //         return false;
    //     }
    //     return true;
    //     return compileSrcToIR(srcFile, irFile);
    // }

    bool init(){
        // 将源文件编译成IR文件
        irFile = getDirPath(srcFile) + getBaseName(srcFile) + ".ll";
        if(!compileSrcToIR(srcFile, irFile)){
            return false;
        }
        // 添加驱动函数文件
        std::string driverFile = getDirPath(srcFile) + getBaseName(srcFile) + "_driver.c";
        DriverGenerator driverGenerator(srcFile);
        if(!driverGenerator.generate(functionName, DRIVER_TYPE::DRIVER_EXECUTABLE)){
            std::cout << "Generate driver file failed" << std::endl;
            return false;
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

    // bool compileIRToExe() {
    //     exeFile = getDirPath(irInstrumentedFile) + getBaseName(irInstrumentedFile);
    //     if(fileExists(exeFile.c_str())) {
    //         std::cout << "Executable file already exists: " << exeFile << std::endl;
    //         return true;
    //     }
    //     const std::string cmd = "clang " + irInstrumentedFile + " -o " + exeFile;
    //     int ret = system(cmd.c_str());
    //     if(ret != 0){
    //         std::cout << "Compile IR file to executable file failed" << std::endl;
    //         return false;
    //     }
    //     return true;
    // }

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
//        for(int i = 0; i < outputs.size(); ++i){
//            std::cout << "cmd " << i + 1 << ": " << cmds[i] << "\tresult: " << outputs[i] << std::endl;
//        }
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

}; // namespace retest

#endif //RETEST_TESTENGINE_H
