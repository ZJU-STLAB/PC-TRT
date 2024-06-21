#ifndef RETEST_COMMON_H
#define RETEST_COMMON_H

#include <cassert>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Module.h>

#include "utils/config.h"

namespace retest
{

#define RETEST_ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "Assertion failed: (" << #condition << "), function " << __FUNCTION__ \
                  << ", file " << __FILE__ << ", line " << __LINE__ << ".\n" \
                  << "Message: " << (message) << std::endl; \
        std::abort(); \
    }

#define TEMPLATE_FILE "./utils.h"
#define TEMPLATE_INCLUDE_STRING "#include <stdio.h>\n#include <stdlib.h>\n#include <fcntl.h>\n#include <unistd.h>\n"
#define TEMPLATE_MAIN_STRING "int main(int argc, char** argv){\n    int stdout_fd = dup(1);\n    close(1);\n"
#define TEMPLATE_END_STRING "    fflush(stdout);\n    dup2(stdout_fd, 1);\n    return 0;\n}\n"
#define TEMPLATE_BLANK_STRING "    "
#define TEMPLATE_PARSER_STRING \
"#include <stdio.h>\n" \
"#include <stdlib.h>\n" \
"#include <stdint.h>\n" \
"#include <assert.h>\n" \
"#include <unistd.h>\n" \
"#include <limits.h>\n" \
"\n" \
"typedef unsigned int uint32_t;\n" \
"\n" \
"int get_length(const char *str) {\n" \
"    int length = 0;\n" \
"    while (str[length] != '\\0') {\n" \
"        length++;\n" \
"    }\n" \
"    return length;\n" \
"}\n" \
"\n" \
"int count_numbers(const char *str) {\n" \
"    int len = get_length(str);\n" \
"    assert(len >= 2);\n" \
"    if(str[0] == '[' && str[1] == ']') {\n" \
"        return 0;\n" \
"    }\n" \
"    int count = 0;\n" \
"    for(int i = 0; i < len; i++) {\n" \
"        if(str[i] == ',') {\n" \
"            count++;\n" \
"        }\n" \
"    }\n" \
"    return count + 1;\n" \
"}\n" \
"\n" \
"int* parse_string_to_array(const char *str) {\n" \
"    int count = count_numbers(str);\n" \
"    int *array = (int *)malloc(count * sizeof(int));\n" \
"    int number = 0;\n" \
"    int index = 0;\n" \
"    int isNegative = 0;\n" \
"    int stop = 0;\n" \
"    int len = get_length(str);\n" \
"    for (int i = 0; i <= len; i++) {\n" \
"        if (str[i] >= '0' && str[i] <= '9' && !stop) {\n" \
"            if(isNegative){\n" \
"                if (number < INT_MIN / 10 || (number == INT_MIN / 10 && str[i] - '0' > 8)) {\n" \
"                    number = INT_MIN;\n" \
"                    stop = 1;\n" \
"                }else{\n" \
"                    number = number * 10 - (str[i] - '0');\n" \
"                }\n" \
"            }else{\n" \
"                if (number > INT_MAX / 10 || (number == INT_MAX / 10 && str[i] - '0' > 7)) {\n" \
"                    number = INT_MAX;\n" \
"                    stop = 1;\n" \
"                }else{\n" \
"                    number = number * 10 + (str[i] - '0');\n" \
"                }\n" \
"            }\n" \
"        } else if (str[i] == '-') {\n" \
"            isNegative = 1;\n" \
"        } else if (str[i] == ',' || str[i] == ']') {\n" \
"            array[index++] = number;\n" \
"            number = 0;\n" \
"            isNegative = 0;\n" \
"            stop = 0;\n" \
"        }\n" \
"    }\n" \
"    return array;\n" \
"}\n" \
"\n" \
"uint32_t atou(const char *str) {\n" \
"    uint32_t number = 0;\n" \
"    int len = get_length(str);\n" \
"    for (int i = 0; i < len; i++) {\n" \
"        if(str[i] >= '0' && str[i] <= '9'){\n" \
"            if(number > UINT32_MAX / 10 || (number == UINT32_MAX / 10 && str[i] - '0' > 5)){\n" \
"                number = UINT32_MAX;\n" \
"                break;\n" \
"            }\n" \
"            number = number * 10 + (str[i] - '0');\n" \
"        }else{\n" \
"            break;\n" \
"        }\n" \
"    }\n" \
"    return number;\n" \
"}\n" \
"\n" \
"char* copy(const char *str) {\n" \
"    int len = get_length(str);\n" \
"    char *ret = (char*)malloc((len + 1) * sizeof(char));\n" \
"    for (int i = 0; i < len; i++) {\n" \
"        ret[i] = str[i];\n" \
"    }\n" \
"    ret[len] = '\\0';\n" \
"    return ret;\n" \
"}\n"

#define KLEE_INCLUDE_STRING "#include <klee/klee.h>\n"
#define KLEE_MAIN_STRING "int main(){\n"
#define KLEE_END_STRING "    return 0;\n}\n"

bool fileExists(const std::string& fileStr){
    std::filesystem::path filepath(fileStr);
    return std::filesystem::exists(filepath);
}

bool fileExists(const char* fileStr){
    std::filesystem::path filepath(fileStr);
    return std::filesystem::exists(filepath);
}

std::string getBaseName(const std::string& filepath){
    std::filesystem::path path(filepath);
    return path.stem().string();
}

std::string getNakedName(const std::string& filepath){
    auto baseName = getBaseName(filepath);
    return baseName.substr(0, baseName.find('_'));
}

std::string getDirPath(const std::string& filepath){
    std::filesystem::path path(filepath);
    return path.parent_path().string() + "/";
}

bool readLinesFromFile(const std::string& filepath, std::vector<std::string>& lines, bool isAppend){
    // 打开文件
    std::ifstream file_stream(filepath);
    // 检查文件是否成功打开
    if (!file_stream.is_open()) {
        std::cout << "Unable to open the file: " << filepath << std::endl;
        return false; // 返回错误码
    }
    if(!isAppend){
        lines.clear();
    }
    // 逐行读取文件内容
    std::string line;
    while (std::getline(file_stream, line)) {
        // 将每一行添加到vector中
        lines.push_back(line);
    }
    // 关闭文件
    file_stream.close();
    return true;
}

bool compileSrcToIR(std::string srcFile, std::string irFile){
    std::string cmd = COMPILER + IR_COMPILE_OPTIONS + srcFile + " -o " + irFile;
    int ret = system(cmd.c_str());
    return ret == 0;
}

bool compileIRToExec(std::string irFile, std::string execFile){
    std::string cmd = COMPILER + EXE_COMPILE_OPTIONS + irFile + " -o " + execFile;
    int ret = system(cmd.c_str());
    return ret == 0;
}

bool cleanUselessFiles(){
    std::string cmd = CLEAN_SCRIPT;
    int ret = system(cmd.c_str());
    return ret == 0;
}

bool changeName(std::string IRFile, std::string functionName){
    // 解析IRfile
    llvm::SMDiagnostic err;
    llvm::LLVMContext ctx;
    auto module = llvm::parseIRFile(IRFile, err, ctx);
    if(!module || !module->getFunction(functionName)){
        return false;
    }
    auto function = module->getFunction(functionName);
    int bbID = 0;
    for(auto& bb : *function){
        bb.setName(std::to_string(bbID++));
    }
    std::error_code EC;
    llvm::raw_fd_ostream file(IRFile, EC);
    module->print(file, nullptr);
    return true;
}

} // namespace retest

#endif //RETEST_COMMON_H
