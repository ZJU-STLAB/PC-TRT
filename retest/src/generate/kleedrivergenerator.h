#ifndef RETEST_KLEEDRIVERGENERATOR_H
#define RETEST_KLEEDRIVERGENERATOR_H

#include <utility>
#include <sstream>
#include "utils/common.h"

namespace retest {

class KleeDriverGenerator {
private:
    std::string srcName;
    std::string functionName;

public:
    explicit KleeDriverGenerator(std::string  src, std::string  func)
      : srcName(std::move(src)), functionName(std::move(func)) {}

    bool generateDriver(){
        // 修改源文件中的main函数
        if(!modifyMainFunction(srcName)){
            return false;
        }
        // 输出文件
        std::string driverFile = getDirPath(srcName) + getBaseName(srcName) + "_klee_driver.c";
        if(fileExists(driverFile.c_str())) {
            std::cout << "Klee driver file already exists: " << driverFile << std::endl;
            return true;
        }
        std::ofstream outputFile(driverFile);
        if(!outputFile.is_open()){
            std::cout << "Cannot open driver file: " << driverFile << std::endl;
            return false;
        }
        // include待测源文件
        outputFile << KLEE_INCLUDE_STRING;
        outputFile << "#include \"" << getBaseName(srcName) << ".c\"\n" << std::endl;
        outputFile << KLEE_MAIN_STRING;
        auto functionDecl = getFunctionDeclaration(srcName, functionName);
        outputFile << getFunctionCallString(functionDecl, functionName, 5);
        outputFile << KLEE_END_STRING << std::endl;
        outputFile.close();
        return true;
    }

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

    static std::string getFunctionCallString(const std::string& functionDecl, const std::string& functionName, int arraySize = 5) {
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
                if (type == "int") {
                    ss << TEMPLATE_BLANK_STRING << "int " << name << ";\n";
                    ss << TEMPLATE_BLANK_STRING << "klee_make_symbolic(&" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
                } else if (type == "uint32_t") {
                    ss << TEMPLATE_BLANK_STRING << "uint32_t " << name << ";\n";
                    ss << TEMPLATE_BLANK_STRING << "klee_make_symbolic(&" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
                } else if (type == "int*") {
                    ss << TEMPLATE_BLANK_STRING << "int " << name << "[" << arraySize << "];\n";
                    ss << TEMPLATE_BLANK_STRING << "klee_make_symbolic(" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
                } else if (type == "char*") {
                    ss << TEMPLATE_BLANK_STRING << "char " << name << "[" << arraySize << "];\n";
                    ss << TEMPLATE_BLANK_STRING << "klee_make_symbolic(" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
                } else if (type == "char") {
                    ss << TEMPLATE_BLANK_STRING << "char " << name << ";\n";
                    ss << TEMPLATE_BLANK_STRING << "klee_make_symbolic(&" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
                } else {
                    ss << TEMPLATE_BLANK_STRING << type << " " << name << ";\n";
                    ss << TEMPLATE_BLANK_STRING << "klee_make_symbolic(&" << name << ", sizeof(" << name << "), \"" << name << "\");\n";
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
        // 如果返回值是指针类型，需要释放内存
        if(returnType.find('*') != std::string::npos){
            ss << TEMPLATE_BLANK_STRING << "free(retVal);\n";
        }
        return ss.str();
    }
};

}

#endif //RETEST_KLEEDRIVERGENERATOR_H
