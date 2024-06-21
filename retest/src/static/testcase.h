#ifndef RETEST_TESTCASE_H
#define RETEST_TESTCASE_H

#include <utility>
#include <nlohmann/json.hpp>
#include "utils/common.h"

namespace retest
{

using json = nlohmann::json;
struct InputVar{
    std::string name;   // 名称
    std::string type;   // 类型
    std::string data;   // 数值
};

void to_json(json& j, const InputVar& arg){
    j = json{{"name", arg.name}, {"type", arg.type}, {"data", arg.data}};
}

void from_json(const json& j, InputVar& arg){
    j.at("name").get_to(arg.name);
    j.at("type").get_to(arg.type);
    j.at("data").get_to(arg.data);
}

struct OutputVar{
    std::string name;   // 名称
    std::string type;   // 类型
    std::string expectation; // 期望值
};

void to_json(json& j, const OutputVar& arg){
    j = json{{"name", arg.name}, {"type", arg.type}, {"expectation", arg.expectation}};
}

void from_json(const json& j, OutputVar& arg){
    j.at("name").get_to(arg.name);
    j.at("type").get_to(arg.type);
    j.at("expectation").get_to(arg.expectation);
}

class TestCase {
public:
    std::vector<InputVar> inputs;   // 输入变量
    std::vector<OutputVar> outputs; // 输出变量
    std::string description;        // 描述
    int pathId {INVALID_PATH_ID};
    std::string result;

    TestCase() = default;
    explicit TestCase(std::vector<InputVar> inputs, std::string description)
        : inputs(std::move(inputs))
        , description(std::move(description))
        {}

    void setPathId(int id){
        this->pathId = id;
    }

    [[nodiscard]] int getPathId() const {
        return pathId;
    }

    void setResult(std::string res){
        this->result = std::move(res);
    }

    [[nodiscard]] std::string getResult() const {
        return result;
    }

    [[nodiscard]] std::string toString() const {
        std::string str = "[";
        for(int i = 0; i < inputs.size(); ++i){
            str += inputs[i].data;
            if(i != inputs.size() - 1){
                str += ", ";
            }
        }
        str += "]";
        return str;
    }

    ~TestCase() = default;
};

void to_json(json& j, const TestCase& tc){
    j = json{
        {"inputs", tc.inputs},
        {"outputs", tc.outputs},
        {"description", tc.description},
        {"pathId", tc.pathId}
    };
}

void from_json(const json& j, TestCase& tc){
    j.at("inputs").get_to(tc.inputs);
    j.at("outputs").get_to(tc.outputs);
    j.at("description").get_to(tc.description);
    j.at("pathId").get_to(tc.pathId);
}

class TestSuite {
private:
    std::string srcFile;
    std::string funcName;
    std::string description;
    bool executed {false};
    double coverage {0.0};

public:
    std::vector<TestCase> testCases;
    TestSuite() = default;
    ~TestSuite() = default;

    explicit TestSuite (
            std::string  srcFile, std::string  funcName,
            std::string  description, const std::vector<TestCase>& testCases)
        : srcFile(std::move(srcFile))
        , funcName(std::move(funcName))
        , description(std::move(description))
        , testCases(testCases)
        {}

    void setSrcFile(std::string src){
        this->srcFile = std::move(src);
    }

    void setFuncName(std::string func){
        this->funcName = std::move(func);
    }

    void setDescription(std::string dsp){
        this->description = std::move(dsp);
    }

    void setTestCases(const std::vector<TestCase>& tcs){
        this->testCases = tcs;
    }

    void setCoverage(double cvg){
        this->coverage = cvg;
    }

    void setExecuted(bool exe){
        this->executed = exe;
    }

    [[nodiscard]] bool isExecuted() const {
        return executed;
    }

    [[nodiscard]] std::string getSrcFile() const {
        return srcFile;
    }

    [[nodiscard]] std::string getFuncName() const {
        return funcName;
    }

    [[nodiscard]] std::string getDescription() const {
        return description;
    }

    [[nodiscard]] std::vector<TestCase>& getTestCases() {
        return testCases;
    }

    [[nodiscard]] const TestCase& getTestCase(int idx) const {
        RETEST_ASSERT(idx >= 0 && idx < testCases.size(), "TestCase index out of range.");
        return testCases[idx];
    }

    [[nodiscard]] double getPathCoverage() const {
        return coverage;
    }

    void addTestCase(const TestCase& tc){
        testCases.push_back(tc);
    }

    void addTestCaseWithoutExpectation(const TestCase& tc){
        TestCase newTc = tc;
        for(auto& output : newTc.outputs){
            output.expectation = "";
        }
        testCases.push_back(newTc);
    }

    friend void to_json(json& j, const TestSuite& ts);
    friend void from_json(const json& j, TestSuite& ts);
};

void to_json(json& j, const TestSuite& ts){
    j = json{
        {"executed", ts.executed},
        {"srcFile", ts.srcFile},
        {"funcName", ts.funcName},
        {"description", ts.description},
        {"coverage", ts.coverage},
        {"testCases", ts.testCases}
    };
}

void from_json(const json& j, TestSuite& ts){
    j.at("srcFile").get_to(ts.srcFile);
    j.at("funcName").get_to(ts.funcName);
    j.at("description").get_to(ts.description);
    j.at("coverage").get_to(ts.coverage);
    j.at("testCases").get_to(ts.testCases);
}

TestSuite getTestSuiteFromFile(const char* filename){
    if(!fileExists(filename)){
        std::cout << "File " << filename << " does not exist." << std::endl;
        return {};
    }
    std::ifstream fin(filename);
    json j;
    fin >> j;
    fin.close();
    TestSuite ts = j;
    return ts;
}

void dumpTestSuiteToFile(const TestSuite& ts, const char* filename){
    std::ofstream fout(filename);
    json j = ts;
    fout << j.dump(4);
    fout.close();
}

void dumpTestSuiteToFile(const TestSuite& ts, const std::string& filename){
    std::ofstream fout(filename);
    json j = ts;
    fout << j.dump(4);
    fout.close();
}

}

#endif //RETEST_TESTCASE_H
