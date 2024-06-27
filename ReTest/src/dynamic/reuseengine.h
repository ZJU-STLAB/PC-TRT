#ifndef PCTRT_REUSEENGINE_H
#define PCTRT_REUSEENGINE_H

#include <string>
#include <iostream>

#include "utils/common.h"
#include "static/cfg.h"
#include "dynamic/testengine.h"

namespace PCTRT {

// 利用基本块序列的指令类型来做相似度计算
class SimilarityStrategy {
private:
    std::unordered_map<int, std::unordered_map<int, double>> sim_cache;

public:
    double calculate(const Path& path1, const Path& path2) {
        size_t m = path1.size();
        size_t n = path2.size();
        double dp[m + 1][n + 1];
        for(int i = 0; i <= m; ++i){
            dp[i][0] = i;
        }
        for(int j = 0; j <= n; ++j){
            dp[0][j] = j;
        }
        for (int i = 1; i <= m; ++i) {
            for(int j = 1; j <= n; ++j){
                const Node* node1 = path1.getNode(i - 1);
                const Node* node2 = path2.getNode(j - 1);
                double similarity = getNodeSimilarity(node1, node2);
                if(similarity == 1.0){
                    dp[i][j] = dp[i - 1][j - 1];
                }else{
                    double cost = 1 - similarity;
                    dp[i][j] = std::min(std::min(dp[i - 1][j] + 1, dp[i][j - 1] + 1), dp[i - 1][j - 1] + cost);
                }
            }
        }
        return 1.0 - dp[m][n] / static_cast<double>(std::max(m, n));
    }

    double getNodeSimilarity(const Node* node1, const Node* node2) {
        PCTRT_ASSERT(node1 != nullptr && node2 != nullptr, "node1 or node2 is nullptr");
        if(sim_cache.count(node1->getId()) > 0 && sim_cache[node1->getId()].count(node2->getId()) > 0){
            return sim_cache[node1->getId()][node2->getId()];
        }
        double sim = 0;
        if(node1->getType() != node2->getType()){
            sim_cache[node1->getId()][node2->getId()] = sim;
            return 0;
        }
        if(node1->getSelectNum() != node2->getSelectNum()) {
            sim_cache[node1->getId()][node2->getId()] = sim;
            return 0;
        }
        const auto& ops1 = node1->getOps();
        const auto& ops2 = node2->getOps();
        size_t m = ops1.size();
        size_t n = ops2.size();
        int dp[m + 1][n + 1];
        for(int i = 0; i <= m; ++i){
            dp[i][0] = i;
        }
        for(int j = 0; j <= n; ++j){
            dp[0][j] = j;
        }
        for(int i = 1; i <= m; ++i){
            for(int j = 1; j <= n; ++j){
                if(ops1[i - 1] == ops2[j - 1]){
                    dp[i][j] = dp[i - 1][j - 1];
                }else{
                    dp[i][j] = std::min(std::min(dp[i - 1][j], dp[i][j - 1]), dp[i - 1][j - 1]) + 1;
                }
            }
        }
        int lcs = dp[m][n];
        sim = 1.0 - (double)lcs / static_cast<double>(std::max(m, n));
        sim_cache[node1->getId()][node2->getId()] = sim;
        return sim;
    }
};

// 计算两个CFG的路径的相似度
class SimilarityCalculator {
private:
    std::shared_ptr<CFG> cfg_old;   // 旧版函数的CFG
    std::shared_ptr<CFG> cfg_new;   // 新版函数的CFG
    std::string funcName;           // 函数名
    std::unique_ptr<SimilarityStrategy> strategy;   // 相似度计算策略

public:
    SimilarityCalculator() = default;

    SimilarityCalculator(std::shared_ptr<CFG> cfg_old, std::shared_ptr<CFG> cfg_new,
                         std::string funcName)
        : cfg_old(std::move(cfg_old))
        , cfg_new(std::move(cfg_new))
        , funcName(std::move(funcName))
        , strategy(std::make_unique<SimilarityStrategy>())
        {}

    ~SimilarityCalculator() = default;

    void run(std::unordered_map<int, std::pair<int, double>>& path_map){
        const auto& new_paths = cfg_new->getPaths();
        for(const auto& path : new_paths){
            auto [path_id, sim] = findMostSimilarPath(path);
            path_map[path.getId()] = {path_id, sim};
        }
    }

private:

    std::pair<int, double> findMostSimilarPath(const Path& path){
        const auto& old_paths = cfg_old->getPaths();
        int most_similar_path_id = INVALID_PATH_ID;
        double max_sim = 0;
        for (const auto& old_path : old_paths) {
            PCTRT_ASSERT(strategy != nullptr, "calculate strategy is nullptr");
            double sim = strategy->calculate(path, old_path);
            if(sim > max_sim){
                max_sim = sim;
                most_similar_path_id = old_path.getId();
            }
        }
        return {most_similar_path_id, max_sim};
    }
};

class ReuseEngine {
private:
    std::string oldSrcFile;
    std::string newSrcFile;

    std::string funcName;

    std::shared_ptr<CFG> new_cfg;
    std::shared_ptr<CFG> old_cfg;
    TestSuite old_suite;
    std::unordered_map<int, std::vector<int>> path_test_map; // 旧版本路径对应的测试用例id

    std::unique_ptr<SimilarityCalculator> calculator;
    std::unordered_map<int, std::pair<int, double>> path_map;

    std::unique_ptr<TestEngine> tester;

    std::vector<int> executedOldPaths;
    std::vector<int> executedNewPaths;

public:
    ReuseEngine() = default;
    ~ReuseEngine() = default;

    void init() {
        // 1. 编译旧版本的源文件
        auto oldIrFile = getDirPath(oldSrcFile) + getBaseName(oldSrcFile) + ".ll";
        compileSrcToIR(oldSrcFile, oldIrFile);
        // 2. 编译新版本的源文件
        auto newIrFile = getDirPath(newSrcFile) + getBaseName(newSrcFile) + ".ll";
        compileSrcToIR(newSrcFile, newIrFile);
    }

    bool initCFG(){
        // 1. 获取旧版本的CFG
        old_cfg = std::make_shared<CFG>();
        std::string old_ir_file = getDirPath(oldSrcFile) + getBaseName(oldSrcFile) + ".ll";
        // 从旧版本IR解析llvm::Module
        llvm::LLVMContext ctx;
        llvm::SMDiagnostic err;
        auto ptr = llvm::parseIRFile(old_ir_file, err, ctx);
        if(!ptr){
            std::cout << "Parse IR file" << old_ir_file << " failed" << std::endl;
            return false;
        }
        old_cfg->initGraphFromFunction(ptr->getFunction(funcName));
        old_cfg->getInfoFromSrcFile(oldSrcFile);
        // 2. 获取新版本的CFG
        new_cfg = std::make_shared<CFG>();
        std::string new_ir_file = getDirPath(newSrcFile) + getBaseName(newSrcFile) + ".ll";
        // 从新版本IR解析llvm::Module
        ptr = llvm::parseIRFile(new_ir_file, err, ctx);
        if(!ptr){
            std::cout << "Parse IR file" << new_ir_file << " failed" << std::endl;
            return false;
        }
        new_cfg->initGraphFromFunction(ptr->getFunction(funcName));
        new_cfg->getInfoFromSrcFile(newSrcFile);
        return true;
    }

    void setSrcAndFunction(const std::string& oldSrc, const std::string& newSrc, const std::string& func){
        this->oldSrcFile = oldSrc;
        this->newSrcFile = newSrc;
        this->funcName = func;
        init();
        initCFG();
        calculator = std::make_unique<SimilarityCalculator>(old_cfg, new_cfg, funcName);
        calculator->run(path_map);  // 计算新版本CFG的路径与旧版本CFG路径的相似度存放到path_map中
    }

    std::vector<bool> reuseTestSuite(const std::string& testSuiteJsonFile, TestSuite& new_suite){
        old_suite = getTestSuiteFromFile(testSuiteJsonFile.c_str());
        if(!old_suite.isExecuted()){
            //如果没执行过，就执行一遍
            tester = std::make_unique<TestEngine>(oldSrcFile, funcName);
            tester->setDriverFile();
            std::vector<std::string> test_results;
            tester->run(old_suite, test_results);
            for(int i = 0; i < test_results.size(); ++i){
                int old_path_id = old_cfg->matchBestPathId(test_results[i]);
                if(old_path_id != INVALID_PATH_ID){
                    path_test_map[old_path_id].push_back(i);
                }
            }
        }else{
            int old_suite_size = static_cast<int>(old_suite.getTestCases().size());
            for(int i = 0; i < old_suite_size; ++i){
                int old_path_id = old_suite.getTestCase(i).getPathId();
                path_test_map[old_path_id].push_back(i);
            }
        }
        // 打印新版程序的CFG路径
        int new_path_size = new_cfg->getPaths().size();
        for(int i = 0; i < new_path_size; ++i){
            std::cout << "path " << i << " : " << new_cfg->getPathString(i) << std::endl;
        }
        // 从旧版本的测试用例中挑选出新版本的测试用例
        // 测试用例：如果路径相似度超过某个阈值，就复用测试用例的全部内容；否则只复用输入
        new_suite.setDescription("new test suite reused from old test suite for function " + funcName);
        new_suite.setFuncName(funcName);
        new_suite.setSrcFile(newSrcFile);
        std::unordered_set<int> reused_test_ids, reused_without_exep_test_ids;
        for(const auto& [new_path_id, old_path] : path_map) {
            const auto& [old_path_id, sim] = old_path;
            std::cout << "new path: " << new_path_id << ", old path: " << old_path_id << ", similarity: " << sim << "\n";
            const auto& test_ids = path_test_map[old_path_id];
            // 如果sim大于SIMILARITY_THRESHOLD，就复用测试用例的全部内容
            if(sim > SIMILARITY_THRESHOLD){
                for(const auto& test_id : test_ids){
                    reused_test_ids.insert(test_id);
                }
            }else{
                // 否则只复用除了期望值之外的部分
                for(const auto& test_id : test_ids){
                    if(reused_test_ids.count(test_id) == 0){
                        reused_without_exep_test_ids.insert(test_id);
                    }
                }
            }
        }
        for(const auto& test_id : reused_test_ids){
            new_suite.addTestCase(old_suite.getTestCase(test_id));
        }
        for(const auto& test_id : reused_without_exep_test_ids){
            new_suite.addTestCaseWithoutExpectation(old_suite.getTestCase(test_id));
        }
        std::string newTestSuiteJsonFile = getDirPath(testSuiteJsonFile) + getNakedName(testSuiteJsonFile) + "_reused.json";
        executeNewTestsuite(new_suite, newTestSuiteJsonFile);

        // 统计路径覆盖情况
        std::vector<bool> oldPathCoverInfo, newPathCoverInfo;
        newPathCoverInfo.resize(new_cfg->getPaths().size(), false);

        const auto &newTestCases = new_suite.getTestCases();
        for(const auto& tc : newTestCases){
            if(tc.pathId >= 0){
                newPathCoverInfo[tc.pathId] = true;
            }
        }
        // dumpReuseReport(oldPathCoverInfo, newPathCoverInfo, path_map, getDirPath(testSuiteJsonFile) + "reuse_report.json");
        return newPathCoverInfo;
    }

    bool compileDriverAndInstrument(std::string driverFile, std::string functionName){
        // 将驱动文件编译为IR文件
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
        std::string exeFile = getDirPath(driverFile) + getBaseName(driverFile) + "_instrumented";
        if(!compileIRToExec(irInstrumentedFile, exeFile)){
            return false;
        }
        return true;
    }

    void executeNewTestsuite(TestSuite& newSuite, const std::string& newTestSuiteJsonFile){
        tester = std::make_unique<TestEngine>(newSrcFile, funcName);
        tester->setDriverFile();
        std::vector<std::string> test_results;
        tester->run(newSuite, test_results);
        dumpTestSuiteToFile(newSuite, newTestSuiteJsonFile);
    }

    // 输出测试用例复用的报告
    void dumpReuseReport(const std::vector<bool>& oldCover, const std::vector<bool>& newCover,
                         const std::unordered_map<int, std::pair<int, double>>& pathMap, const std::string& reportFile){
        nlohmann::ordered_json j;
        j["old_src_file"] = oldSrcFile;
        j["new_src_file"] = newSrcFile;
        j["function_name"] = funcName;
        json old_j;
        old_j["cfg_dot"]  = old_cfg->dumpToDotGraph();
        old_j["paths"] = old_cfg->getPaths();
        old_j["coverInfo"] = oldCover;
        j["old_info"] = old_j;
        json new_j;
        new_j["cfg_dot"] = new_cfg->dumpToDotGraph();
        new_j["paths"] = new_cfg->getPaths();
        new_j["coverInfo"] = newCover;
        j["new_info"] = new_j;
        j["pathSimilarity"] = pathMap;
        std::ofstream fs(reportFile);
        fs << j.dump(2);
        fs.close();
    }

    void printCFG(){
        std::cout << "old cfg: \n";
        json j1 = *old_cfg;
        std::cout << j1.dump(4) << std::endl;
        std::cout << old_cfg->dumpToDotGraph() << std::endl;
        std::cout << "\nnew cfg: \n";
        json j2 = *new_cfg;
        std::cout << j2.dump(4) << std::endl;
        std::cout << new_cfg->dumpToDotGraph() << std::endl;
    }

    void drawNewCFG(){
        auto newIRFile = getDirPath(newSrcFile) + getBaseName(newSrcFile) + ".ll";
        changeName(newIRFile, funcName);
        std::string cmd = IR2PNG_SCRIPT;
        cmd += newIRFile + " " + funcName + " > /dev/null";
        system(cmd.c_str());
    }
};

} // namespace PCTRT

#endif //PCTRT_REUSEENGINE_H