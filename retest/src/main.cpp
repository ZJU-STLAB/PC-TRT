#include <string>
#include <iostream>
#include <filesystem>
#include <llvm/Support/CommandLine.h>

#include "dynamic/reuseengine.h"
#include "generate/testgenerator.h"

using namespace llvm;
using namespace retest;
namespace fs = std::filesystem;

static cl::opt<std::string> Directory("d", cl::desc("Specify the directory"), cl::value_desc("directory"));

static cl::opt<std::string> OldSrcFile("old", cl::desc("Specify the old source file"), cl::value_desc("old source file"));
static cl::opt<std::string> NewSrcFile("new", cl::desc("Specify the new source file"), cl::value_desc("new source file"));
static cl::opt<std::string> FunctionName("func", cl::desc("Specify the function name"), cl::value_desc("function name"));
static cl::opt<std::string> TestJsonFile("test", cl::desc("Specify the test json file"), cl::value_desc("test json file"));
static cl::opt<std::string> CFGoption("cfg", cl::desc("Option to draw the new cfg image"), cl::value_desc("cfg option"));

int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "My tool description\n");
    // 访问解析后的参数
    std::string oldSrcFile, newSrcFile, functionName, testJsonFile;
    // if(!Directory.empty()){
    //     // 访问目录，自动解析出文件名
    //     // 获取目录下的所有文件名
    //     std::vector<std::string> files;
    //     fs::path dirPath {Directory.c_str()};
    //     if(fs::exists(dirPath) && fs::is_directory(dirPath)){
    //         for(const auto &entry : fs::directory_iterator(dirPath)){
    //             if(entry.is_regular_file()){
    //                 files.push_back(entry.path().filename().string());
    //             }
    //         }
    //     }else{
    //         std::cerr << "Directory " << Directory << " does not exist or is not a directory\n";
    //         return 1;
    //     }
    //     for(const auto &file : files){
    //         if(file.find(".c") != std::string::npos) {
    //             if(file.find("old") == std::string::npos && file.find("driver") == std::string::npos){
    //                 newSrcFile = file;
    //             }else if(file.find("driver") == std::string::npos){
    //                 oldSrcFile = Directory + "/" + file;
    //             }
    //         }else if(file.find(".json") != std::string::npos && file.find("old") != std::string::npos){
    //             testJsonFile = Directory + "/" + file;
    //         }
    //     }
    //     functionName = newSrcFile.substr(0, newSrcFile.find(".c"));
    //     newSrcFile = Directory + "/" + newSrcFile;
    //     if(oldSrcFile.empty() || newSrcFile.empty() || functionName.empty() || testJsonFile.empty()){
    //         std::cerr << "You must make sure that the old source file, new source file and the test json file are in the directory\n";
    //         return 1;
    //     }
    // }else{
        if(OldSrcFile.empty() || NewSrcFile.empty() || FunctionName.empty() || TestJsonFile.empty()){
            std::cerr << "You must specify the old source file, new source file, function name and test json file\n";
            return 1;
        }
        // 确保OldSrcFile, NewSrcFile, TestJsonFile文件都存在
        if(!fs::exists(OldSrcFile.c_str())){
            std::cerr << "Old source file " << OldSrcFile << " does not exist\n";
            return 1;
        }
        if(!fs::exists(NewSrcFile.c_str())){
            std::cerr << "New source file " << NewSrcFile << " does not exist\n";
            return 1;
        }
        if(!fs::exists(TestJsonFile.c_str())){
            std::cerr << "Test json file " << TestJsonFile << " does not exist\n";
            return 1;
        }
        oldSrcFile = OldSrcFile;
        newSrcFile = NewSrcFile;
        functionName = FunctionName;
        testJsonFile = TestJsonFile;
    // }
    std::cout << "oldSrcFile: " << oldSrcFile << ", newSrcFile: " << newSrcFile << ", functionName: " << functionName << ", testJsonFile: " << testJsonFile << "\n";
    ReuseEngine reuseEngine;
    reuseEngine.setSrcAndFunction(oldSrcFile, newSrcFile, functionName);
    if(!CFGoption.empty()){
        reuseEngine.drawNewCFG();
    }
    TestSuite newTestSuite;
    auto info =  reuseEngine.reuseTestSuite(testJsonFile, newTestSuite);
    std::vector<int> uncoveredPaths;
    for(int i = 0; i < info.size(); i++){
        if(info[i] == 0){
            uncoveredPaths.push_back(i);
        }
    }
    TestGenerator testGenerator(newSrcFile, functionName, uncoveredPaths);
    testGenerator.run();
    // 再运行一遍新的测试用例
    TestSuite newTestSuite2;
    cleanUselessFiles();
    return 0;
}