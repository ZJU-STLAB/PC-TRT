#ifndef RETEST_DRIVERGENERATOR_H
#define RETEST_DRIVERGENERATOR_H

#include <string>
#include "utils/common.h"

namespace retest {

enum class DRIVER_TYPE {
    DRIVER_EXECUTABLE,
    DRIVER_KLEE_SE,
};

class DriverGenerator {
private:
    std::string srcFileName;

public:
    explicit DriverGenerator(std::string srcFile) : srcFileName(std::move(srcFile)) {
        // 要保证源文件存在
        if (!fileExists(srcFileName)) {
            std::cerr << "Source file " << srcFileName << " does not exist." << std::endl;
            std::abort();
        }
    }

    bool generate(std::string functionName, DRIVER_TYPE driverType){
        // 修改源文件中的main函数
        if(!modifyMainFunction(srcFileName)){
            return false;
        }
        if(driverType == DRIVER_TYPE::DRIVER_EXECUTABLE){
            return generateExecutableDriver(std::move(functionName));
        } else if(driverType == DRIVER_TYPE::DRIVER_KLEE_SE){
            return generateKleeDriver(std::move(functionName));
        }
        return true;
    }

    std::string getFunctionDeclaration(const std::string& srcFileName, const std::string& functionName){
        std::ifstream input(srcFileName);
        if(!input.is_open()){
            std::cout << "Cannot open file: " << srcFileName << std::endl;
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

    std::vector<std::pair<std::string, std::string>> getFunctionParameters(const std::string& functionDeclaration, const std::string functionName){
        std::vector<std::pair<std::string, std::string>> parameters;
        if(functionDeclaration.empty()){
            return parameters;
        }
        auto start = functionDeclaration.find(functionName);
        auto paraStart = functionDeclaration.find('(', start);
        auto paraEnd = functionDeclaration.find(')', paraStart);
        RETEST_ASSERT(paraStart != std::string::npos && paraEnd != std::string::npos, "Cannot find function parameters");
        if(paraStart != paraEnd - 1) {
            //用逗号分隔所有参数
            std::vector<int> commaPos;
            for (auto i = paraStart; i < paraEnd; ++i) {
                if (functionDeclaration[i] == ',') {
                    commaPos.push_back(static_cast<int>(i));
                }
            }
            commaPos.push_back(static_cast<int>(paraEnd));
            int lastPos = static_cast<int>(paraStart) + 1;
            for (int idx: commaPos) {
                int endPos = idx;
                while (functionDeclaration[lastPos] == ' ') {
                    lastPos++;
                }
                while (functionDeclaration[endPos] == ' ' || functionDeclaration[endPos] == ',' || functionDeclaration[endPos] == ')') {
                    endPos--;
                }
                int blankPos = endPos;
                while (functionDeclaration[blankPos] != ' ' && functionDeclaration[blankPos] != '*') {
                    blankPos--;
                }
                std::string type, name;
                for (int i = lastPos; i <= blankPos; ++i) {
                    if (functionDeclaration[i] != ' ') {
                        type += functionDeclaration[i];
                    }
                }
                for (int i = blankPos + 1; i <= endPos; ++i) {
                    if (functionDeclaration[i] != ' ') {
                        name += functionDeclaration[i];
                    }
                }
                parameters.emplace_back(type, name);
                lastPos = idx + 1;
            }
        }
        return std::move(parameters);
    }
    
    bool modifyMainFunction(const std::string& fileName) {
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
    
    bool generateExecutableDriver(std::string functionName){
        auto functionDeclaration = getFunctionDeclaration(srcFileName, functionName);
        if(functionDeclaration.empty()){
            std::cerr << "Cannot find function declaration for " << functionName << std::endl;
            return false;
        }
        auto returnType = functionDeclaration.substr(0, functionDeclaration.find(functionName));
        auto parameters = getFunctionParameters(functionDeclaration, functionName);
        // 检查参数类型
        for(const auto& [type, name] : parameters){
            if (type != "int" && type != "uint32_t" && type != "int*" &&
                type != "char*" && type != "char") {
                std::cerr << "Unsupported parameter type: " << type << " " << name << std::endl;
                return false;
            }
        }
        // 生成driver文件
        std::string driverFileName = getDirPath(srcFileName) + getBaseName(srcFileName) + "_driver.c";
        std::ofstream outputFile(driverFileName);
        if (!outputFile.is_open()) {
            std::cerr << "Cannot open driver file: " << driverFileName << std::endl;
            return false;
        }
        outputFile << "#include \"" << getBaseName(srcFileName) << ".c\"\n" << std::endl;
        outputFile << TEMPLATE_PARSER_STRING << "\n";
        outputFile << TEMPLATE_MAIN_STRING;
        int idx = 1;
        for (const auto &[type, name]: parameters) {
            outputFile << TEMPLATE_BLANK_STRING << type << " " << name << " = ";
            if (type == "int") {
                outputFile << "atoi(argv[" << idx++ << "]);\n";
            } else if (type == "uint32_t") {
                outputFile << "atou(argv[" << idx++ << "]);\n";
            } else if (type == "int*") {
                outputFile << "parse_string_to_array(argv[" << idx++ << "]);\n";
            } else if (type == "char*") {
                outputFile << "copy(argv[" << idx++ << "]);\n";
            } else if (type == "char") {
                outputFile << "argv[" << idx++ << "][0];\n";
            } else {
                outputFile << "argv[" << idx++ << "];\n";
            }
        }
        // 调用函数
        outputFile << TEMPLATE_BLANK_STRING << returnType << "retVal = " << functionName << "(";
        for(int i = 0; i < parameters.size(); ++i){
            outputFile << parameters[i].second;
            if(i != parameters.size() - 1){
                outputFile << ", ";
            }
        }
        outputFile << ");\n";
        // 如果参数中有指针类型，需要释放内存
        for(const auto& [type, name] : parameters){
            if(type.find('*') != std::string::npos){
                outputFile << TEMPLATE_BLANK_STRING << "free(" << name << ");\n";
            }
        }
        // 如果返回值是指针类型，需要释放内存
        if(returnType.find('*') != std::string::npos){
            outputFile << TEMPLATE_BLANK_STRING << "free(retVal);\n";
        }
        outputFile << TEMPLATE_END_STRING;
        outputFile.close();
        return true;
    }

    bool generateKleeDriver(std::string functionName) {
        auto functionDeclaration = getFunctionDeclaration(srcFileName, functionName);
        if(functionDeclaration.empty()){
            std::cerr << "Cannot find function declaration for " << functionName << std::endl;
            return false;
        }
        auto returnType = functionDeclaration.substr(0, functionDeclaration.find(functionName));
        auto parameters = getFunctionParameters(functionDeclaration, functionName);
        // 检查参数类型
        for(const auto& [type, name] : parameters){
            if (type != "int" && type != "uint32_t" && type != "int*" &&
                type != "char*" && type != "char") {
                std::cerr << "Unsupported parameter type: " << type << " " << name << std::endl;
                return false;
            }
        }
        // 生成driver文件
        std::string driverFileName = getDirPath(srcFileName) + getBaseName(srcFileName) + "_klee_driver.c";
        std::ofstream outputFile(driverFileName);
        if (!outputFile.is_open()) {
            std::cerr << "Cannot open driver file: " << driverFileName << std::endl;
            return false;
        }
        outputFile << KLEE_INCLUDE_STRING;
        outputFile << "#include \"" << getBaseName(srcFileName) << ".c\"\n" << std::endl;
        outputFile << KLEE_MAIN_STRING;
        for (const auto &[type, name]: parameters) {
            if (type == "int") {
                outputFile << TEMPLATE_BLANK_STRING << "int " << name << ";\n";
                outputFile << TEMPLATE_BLANK_STRING << "klee_make_symbolic(&" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
            } else if (type == "uint32_t") {
                outputFile << TEMPLATE_BLANK_STRING << "uint32_t " << name << ";\n";
                outputFile << TEMPLATE_BLANK_STRING << "klee_make_symbolic(&" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
            } else if (type == "int*") {
                outputFile << TEMPLATE_BLANK_STRING << "int " << name << "[" << KLEE_ARRAY_SIZE << "];\n";
                outputFile << TEMPLATE_BLANK_STRING << "klee_make_symbolic(" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
            } else if (type == "char*") {
                outputFile << TEMPLATE_BLANK_STRING << "char " << name << "[" << KLEE_ARRAY_SIZE << "];\n";
                outputFile << TEMPLATE_BLANK_STRING << "klee_make_symbolic(" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
            } else if (type == "char") {
                outputFile << TEMPLATE_BLANK_STRING << "char " << name << ";\n";
                outputFile << TEMPLATE_BLANK_STRING << "klee_make_symbolic(&" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
            } else {
                outputFile << TEMPLATE_BLANK_STRING << type << " " << name << ";\n";
                outputFile << TEMPLATE_BLANK_STRING << "klee_make_symbolic(&" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
            }
        }
        // 调用函数
        outputFile << TEMPLATE_BLANK_STRING << returnType << "retVal = " << functionName << "(";
        for(int i = 0; i < parameters.size(); ++i){
            outputFile << parameters[i].second;
            if(i != parameters.size() - 1){
                outputFile << ", ";
            }
        }
        outputFile << ");\n";
        // 如果返回值是指针类型，需要释放内存
        if(returnType.find('*') != std::string::npos){
            outputFile << TEMPLATE_BLANK_STRING << "free(retVal);\n";
        }
        outputFile << KLEE_END_STRING;
        outputFile.close();
        return true;
    }
};

}

#endif