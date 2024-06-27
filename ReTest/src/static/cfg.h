#ifndef PCTRT_CFG_H
#define PCTRT_CFG_H

#include <llvm/IR/CFG.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <memory>
#include <vector>
#include <map>
#include <stack>

#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include "utils/common.h"

namespace PCTRT {
    struct src_loc {
        unsigned int line_num_;
        unsigned int col_num_;

        bool operator==(const src_loc &loc) const {
            return line_num_ == loc.line_num_ && col_num_ == loc.col_num_;
        }

        bool operator<(const src_loc &loc) const {
            return line_num_ < loc.line_num_ || (line_num_ == loc.line_num_ && col_num_ < loc.col_num_);
        }
    };
}

// 哈希函数特化
namespace std {
    template <>
    struct ::std::hash<PCTRT::src_loc> {
        size_t operator()(const PCTRT::src_loc &pos) const {
            // 使用合适的哈希算法，这里简单地将 line_num 和 col_num 组合起来
            return hash<uint32_t>()(pos.line_num_) ^ hash<uint32_t>()(pos.col_num_);
        }
    };
}

namespace PCTRT
{
using json = nlohmann::json;
enum class NODE_TYPE{
    NODE_NORMAL,
    NODE_BRANCH,
    NODE_LOOP,
    NODE_ENTRY,
    NODE_EXIT,
};

// map NODE_TYPE values to JSON as strings
NLOHMANN_JSON_SERIALIZE_ENUM( NODE_TYPE, {
    {NODE_TYPE::NODE_NORMAL, "node_normal"},
    {NODE_TYPE::NODE_BRANCH, "node_branch"},
    {NODE_TYPE::NODE_LOOP, "node_loop"},
    {NODE_TYPE::NODE_ENTRY, "node_entry"},
    {NODE_TYPE::NODE_EXIT, "node_exit"},
})

/**
 * Node: 对llvm::BasicBlock进行一层封装，使之能持有额外的信息并可持久化
 */
class Node
{
private:
    static int count_;
    int id;
    NODE_TYPE node_type;
    std::vector<int> successors;
    std::vector<std::string> ops;
    std::string instructions;
    std::string src;
    int selectNum {0};  // 0为默认，1为true，2为false，其他按分支顺序
public:
    Node() : id(count_++), node_type(NODE_TYPE::NODE_NORMAL) {};
    ~Node() = default;

    explicit Node(const llvm::BasicBlock* block, NODE_TYPE nodeType = NODE_TYPE::NODE_NORMAL){
        id = count_++;
        node_type = nodeType;
        printBasicBlockToNodeStr(block, instructions);
        getInstructionsType(block);
    }

    void setType(NODE_TYPE nodeType){
        node_type = nodeType;
    }

    [[nodiscard]] NODE_TYPE getType() const {
        return node_type;
    }

    void setSelectNum(int num){
        selectNum = num;
    }

    [[nodiscard]] int getSelectNum() const {
        return selectNum;
    }

    [[nodiscard]] int getId() const {
        return id;
    }

    std::vector<int>& getSuccessors(){
        return successors;
    }

    void setSrcInfo(const std::string& srcStr){
        src = srcStr;
    }

    [[nodiscard]] const std::string& getSrcInfo() const {
        return src;
    }

    static void printBasicBlockToNodeStr(const llvm::BasicBlock* block, std::string& str){
        llvm::raw_string_ostream rso(str);
        for(auto& instruction : *block){
            instruction.print(rso);
        }
    }

    void getInstructionsType(const llvm::BasicBlock* block){
        for(auto& instruction : *block){
            ops.emplace_back(instruction.getOpcodeName());
        }
    }

    [[nodiscard]] const std::vector<std::string>& getOps() const {
        return ops;
    }

    static void resetCount(){
        count_ = 0;
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Node, id, node_type, selectNum, instructions, ops, src, successors)
}; // class Node

int Node::count_ = 0;

/**
 * Path: CFG中的静态路径
 */
class Path
{

private:
    static int count_;
    int id_;
    std::vector<const Node*> nodes;
    int total_nodes;

public:
    explicit Path(int total_nodes, const std::vector<const Node*>& nodes) :
        id_(count_++), total_nodes(total_nodes), mask(std::make_unique<pathMask>(total_nodes)) {
            this->nodes = nodes;
            for(auto node : nodes){
                int node_id = node->getId();
                PCTRT_ASSERT(node_id >= 0 && node_id < total_nodes, "Node id is out of range.");
                mask->setBit(node_id);
            }
    }

    [[nodiscard]] int getId() const {
        return id_;
    }

    [[nodiscard]] size_t size() const {
        return nodes.size();
    }

    [[nodiscard]] const Node* getNode(int idx) const {
        PCTRT_ASSERT(idx >= 0 && idx < nodes.size(), "Index is out of range.");
        return nodes[idx];
    }

    [[nodiscard]] std::string to_string() const {
        return mask->to_string();
    }

    [[nodiscard]] std::string to_string_with_nodes() const {
        std::string ret;
        for(int i = 0; i < nodes.size(); ++i){
            ret += std::to_string(nodes[i]->getId());
            if(i != nodes.size() - 1){
                ret += " -> ";
            }
        }
        return ret;
    }

    [[nodiscard]] std::vector<int> to_vector_of_nodes() const {
        std::vector<int> ret;
        for(auto node : nodes){
            ret.push_back(node->getId());
        }
        return ret;
    }

    ~Path()= default;

    static void resetCount(){
        count_ = 0;
    }

    friend void to_json(json& j, const Path& path);
    friend void from_json(const json& j, Path& path);

    struct pathMask {
        int numNodes;
        std::unique_ptr<bool[]> nodeMask;
        explicit pathMask(int num) : numNodes(num) {
            nodeMask = std::make_unique<bool[]>(numNodes);
            for(int i = 0; i < numNodes; ++i){
                nodeMask[i] = false;
            }
        }

        explicit pathMask(const std::string& str) : numNodes(static_cast<int>(str.size())) {
            nodeMask = std::make_unique<bool[]>(numNodes);
            for(int i = 0; i < numNodes; ++i){
                nodeMask[i] = str[i] == '1';
            }
        }

        void setBit(int index) const {
            PCTRT_ASSERT(index >= 0 && index < numNodes, "Index is out of range.");
            nodeMask[index] = true;
        }

        [[nodiscard]] bool isCover(const pathMask& other) const {
//            PCTRT_ASSERT(numNodes == other.numNodes, "The number of nodes is not equal.");
            if(numNodes != other.numNodes){
                return false;
            }
            for(int i = 0; i < numNodes; ++i){
                if(other.nodeMask[i] && !nodeMask[i]){
                    return false;
                }
            }
            return true;
        }

        void clearBits() const {
            for(int i = 0; i < numNodes; ++i){
                nodeMask[i] = false;
            }
        }

        [[nodiscard]] std::string to_string() const{
            std::string ret(numNodes, '0');
            for (int i = 0; i < numNodes; ++i) {
                ret[i] = nodeMask[i] ? '1' : '0';
            }
            return ret;
        }
    };

    std::shared_ptr<pathMask> mask {nullptr};
};

int Path::count_ = 0;

void to_json(json& j, const Path& path){
    j = json{
        {"id", path.id_},
        {"mask", path.mask->to_string()},
        {"nodesStr", path.to_string_with_nodes()}
    };
}

void from_json(const json& j, Path& path){
    j.at("id").get_to(path.id_);
    path.mask = std::make_shared<Path::pathMask>(j.at("mask").get<std::string>());
}

/**
 * CFG: 对llvm::Function进行的简单封装
 */
class CFG
{
private:
    static int count_;
    int id;
    size_t size{};
    std::vector<Node> nodes;
    std::unordered_map<const llvm::BasicBlock*, int> node_map;
    std::vector<std::vector<int>> edges;    // 节点之间的边
    std::vector<Path> paths;                // 静态路径

    // 静态分析相关
    std::unique_ptr<llvm::DominatorTree> DT;
    std::unique_ptr<llvm::LoopInfo> loopInfo;
    std::unordered_map<const llvm::BasicBlock*, const llvm::Loop*> loop_map;
    llvm::Function* func {nullptr};
    std::unordered_map<std::string, int> pathIdMap;
    std::unordered_map<int, bool> nodeSelectMap;

    // 源代码相关
    std::vector<std::string> srcLines;      // 对应的源代码
    std::map<src_loc, int> srcLocMap;       // 源代码位置到节点的映射
    std::vector<std::vector<src_loc>> srcLocs;  // 每个节点对应的源代码位置

    // 动态执行相关
    std::unordered_map<int, int> pathTestCntMap;  // 路径对应的测试用例执行次数

public:
    CFG() : id(count_++), size(0) {
        Node::resetCount();
        Path::resetCount();
    }

    size_t getSize() const {
        return size;
    }

    std::vector<Path>& getPaths(){
        return paths;
    }

    std::vector<std::vector<int>> getPathNodes(){
        std::vector<std::vector<int>> ret;
        for(auto& path : paths){
            std::vector<int> pathNodes;
            for(int i = 0; i < path.size(); ++i){
                pathNodes.push_back(path.getNode(i)->getId());
            }
            ret.push_back(pathNodes);
        }
        return ret;
    }

    Path& getPath(int idx){
        PCTRT_ASSERT(idx >= 0 && idx < paths.size(), "Index is out of range.");
        return paths[idx];
    }

    std::string getPathString(int pathId){
        if(pathId < 0 || pathId >= paths.size()){
            return "Invalid path, runtime error.";
        }
        return paths[pathId].to_string_with_nodes();
    }

    std::vector<int> getBlockSuccessors(int blockId){
        PCTRT_ASSERT(blockId >= 0 && blockId < size, "Block id is out of range.");
        return edges[blockId];
    }

    void initGraphFromFunction(llvm::Function* function){
        PCTRT_ASSERT(function != nullptr && !function->empty(), "Function cannot be empty!");
        func = function;
        size = function->size();
        nodes.reserve(size);
        edges.resize(size);
        DT = std::make_unique<llvm::DominatorTree>(*function);
        loopInfo = std::make_unique<llvm::LoopInfo>();
        loopInfo->analyze(*DT);

        int bbId = 0;
        for (auto& block : *function) {
            nodes.emplace_back(&block);
            node_map[&block] = bbId;
            PCTRT_ASSERT(bbId == nodes.back().getId(), "Node id doesn't match!");
            bbId++;
        }
        auto& entry = function->getEntryBlock();
        int entry_id = node_map[&entry];
        nodes[entry_id].setType(NODE_TYPE::NODE_ENTRY);

        // 处理循环
        for(auto topLoop : *loopInfo){
            handleLoopsInFunction(topLoop);
        }
        buildGraph();
        analyzingPaths();
    }

    ~CFG() {
        nodes.clear();
    }

    void buildGraph() {
        for (auto& [block, node_id] : node_map) {
            auto& node = nodes[node_id];
            auto& successors = node.getSuccessors();
            for(auto it = succ_begin(block); it != succ_end(block); ++it){
                successors.push_back(node_map[*it]);
            }
            if(successors.empty()){
                node.setType(NODE_TYPE::NODE_EXIT);
            }else if (successors.size() > 1) {
                if (node.getType() == NODE_TYPE::NODE_NORMAL) {
                    node.setType(NODE_TYPE::NODE_BRANCH);
                }
                size_t num = block->getTerminator()->getNumSuccessors();
                for(size_t i = 0; i < num; ++i) {
                    llvm::BasicBlock *succBlock = block->getTerminator()->getSuccessor(i);
                    int succ_node_id = node_map[succBlock];
                    nodes[node_map[block->getTerminator()->getSuccessor(i)]].setSelectNum(i + 1);
                }
            }
            edges[node_id] = node.getSuccessors();
        }
    }

    void handleLoopsInFunction(llvm::Loop* loop){
        if(loop == nullptr){
            return;
        }
        auto header = loop->getHeader();
        loop_map[header] = loop;
        nodes[node_map[header]].setType(NODE_TYPE::NODE_LOOP);
        for(auto subLoop : loop->getSubLoops()){
            handleLoopsInFunction(subLoop);
        }
    }

    void analyzingPaths(){
        std::vector<std::vector<int>> idPaths;
        if(loopInfo->empty()){
            idPaths = dfsWithoutLoops();
        }else{
            idPaths = dfsWithLoops();
        }
        for(const auto& idPath : idPaths) {
            std::vector<const Node*> nodePath;
            nodePath.reserve(idPath.size());
            for(auto node_id : idPath){
                nodePath.push_back(&nodes[node_id]);
            }
            paths.emplace_back(size, nodePath);
            pathIdMap[paths.back().to_string()] = paths.back().getId();
        }
    }

    // 深度优先遍历，不考虑循环
    std::vector<std::vector<int>> dfsWithoutLoops(){
        std::vector<std::vector<int>> allPaths;
        std::stack<std::vector<int>> stack;
        stack.push({0});
        while (!stack.empty()) {
            std::vector<int> currentPath = stack.top();
            stack.pop();
            int currentNode = currentPath.back();
            for (int neighbor : edges[currentNode]) {
                std::vector<int> newPath = currentPath;
                newPath.push_back(neighbor);
                // If the neighbor is the destination, add the path to the result
                if (edges[neighbor].empty()) {
                    allPaths.push_back(newPath);
                } else {
                    stack.push(newPath);
                }
            }
        }
        return allPaths;
    }

    // 深度优先遍历，考虑循环
    // 伪代码算法流程：
    // 输入：函数func，循环信息loop_map，节点映射node_map，边信息edges
    // 输出：所有路径的集合allPaths
    // 初始化：空路径集合allPaths，空块路径集合blockPaths
    // 创建初始路径path，起点为函数入口块&func->getEntryBlock()
    // 调用dfsHelper函数，传入起点块&func->getEntryBlock()、循环头部为空、初始路径path、块路径集合blockPaths、终点块集合{&func->back()}
    // 遍历块路径集合blockPaths中的每个块路径blockPath
        // 创建空的节点路径idPath
        // 遍历块路径blockPath中的每个块block
            // 将块block的节点映射值node_map[block]添加到节点路径idPath中
        // 将节点路径idPath添加到所有路径集合allPaths中
    // 返回所有路径集合allPaths

    std::vector<std::vector<int>> dfsWithLoops(){
        std::vector<std::vector<int>> allPaths;
        std::vector<std::vector<llvm::BasicBlock*>> blockPaths;
        std::vector<llvm::BasicBlock*> path = {&func->getEntryBlock()};
        dfsHelper(&func->getEntryBlock(), nullptr, path, blockPaths, {&func->back()});
        for(auto& blockPath : blockPaths){
            std::vector<int> idPath;
            idPath.reserve(blockPath.size());
            for(auto block : blockPath){
                idPath.push_back(node_map[block]);
            }
            allPaths.push_back(idPath);
        }
        return allPaths;
    }

    // 从循环的Header开始，找到所有到达Exit的路径
    std::vector<std::vector<llvm::BasicBlock*>> getLoopPathsFromHeader(llvm::BasicBlock* header){
        std::vector<std::vector<llvm::BasicBlock*>> allPaths;
        std::vector<llvm::BasicBlock*> path = {header};
        llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocks;
        loop_map[header]->getExitBlocks(exitBlocks);
        std::unordered_set<llvm::BasicBlock*> exitBlockSet(exitBlocks.begin(), exitBlocks.end());
        dfsHelper(header, header, path, allPaths, exitBlockSet);
        return allPaths;
    }

    // 从起点开始深度优先遍历，找到所有到达终点exitBlocks的路径
    void dfsHelper(llvm::BasicBlock* block, llvm::BasicBlock* header,
                   std::vector<llvm::BasicBlock*> path,
                   std::vector<std::vector<llvm::BasicBlock*>>& allPaths,
                   const std::unordered_set<llvm::BasicBlock*>& exitBlockSet){
        // 碰到exitBlock，就将当前路径加入到结果中
        if(exitBlockSet.count(block) > 0){
            allPaths.push_back(path);
            return;
        }
        // 如果当前块是子循环的Header，那么就要找到所有到达Exit的子路径
        if(block != header && loop_map.count(block) > 0) {
            auto subPaths = getLoopPathsFromHeader(block);
            for(auto& subPath : subPaths){
                std::vector<llvm::BasicBlock*> newPath = path;
                for(auto it = subPath.begin() + 1; it != subPath.end(); ++it){
                    newPath.push_back(*it);
                }
                dfsHelper(newPath.back(), header, newPath, allPaths, exitBlockSet);
            }
        } else {
            for(auto next : successors(block)){
                if(next == header){
                    path.push_back(next);
                    auto& set = loop_map[next]->getBlocksSet();
                    for(auto n_next : successors(next)) {
                        if(exitBlockSet.count(n_next) > 0 && set.count(n_next) == 0) {
                            path.push_back(n_next);
                            dfsHelper(n_next, header, path, allPaths, exitBlockSet);
                        }
                    }
                }else{
                    std::vector<llvm::BasicBlock*> newPath = path;
                    newPath.push_back(next);
                    dfsHelper(next, header, newPath, allPaths, exitBlockSet);
                }
            }
        }
    }

    bool getInfoFromSrcFile(const std::string& srcFile){
        bool ok = readLinesFromFile(srcFile, srcLines, false);
        if(!ok){
            return false;
        }
        srcLocs.resize(size);
        for (auto [node, node_id] : node_map) {
            for(auto& instruction : *node){
                if(const auto& debugLoc = instruction.getDebugLoc()){
                    if(debugLoc.getLine() == 0){
                        continue;
                    }
                    unsigned line = debugLoc.getLine();
                    unsigned column = debugLoc.getCol();
                    src_loc loc{line, column};
                    srcLocs[node_id].push_back(loc);
                    srcLocMap[loc] = node_id;
                }
            }
            // 排序
            std::sort(srcLocs[node_id].begin(), srcLocs[node_id].end());
            // 去重
            auto last = std::unique(srcLocs[node_id].begin(), srcLocs[node_id].end());
            srcLocs[node_id].erase(last, srcLocs[node_id].end());
        }
         addSrcInfoToNodes();
        return true;
    }

    void addSrcInfoToNodes(){
        for(auto& node : nodes){
            std::string nodeSrcStr;
            auto& nodeLocs = srcLocs[node.getId()];
            if(nodeLocs.empty()){
                continue;
            }
            for(const auto& loc : nodeLocs){
                nodeSrcStr += getSrcWithLoc(loc);
            }
            node.setSrcInfo(nodeSrcStr);
            // std::cout << "node_id: " << node.getId() << " src: " << nodeSrcStr << "\n";
        }
    }

    std::string getSrcWithLoc(const src_loc& loc){
        PCTRT_ASSERT(srcLocMap.count(loc) > 0, "Cannot find the location in source file.");
        std::string ret;
        auto it = srcLocMap.find(loc);
        auto next = std::next(it);
        if(next != srcLocMap.end() && next->first.line_num_ == loc.line_num_){
            ret = srcLines[loc.line_num_ - 1].substr(loc.col_num_ - 1, next->first.col_num_ - loc.col_num_);
            return addEscapeChar(ret, false);
        }
        auto line_size = srcLines[loc.line_num_ - 1].size();
        ret = srcLines[loc.line_num_ - 1].substr(loc.col_num_ - 1, line_size - loc.col_num_);
        return addEscapeChar(ret, true);
    }

    static std::string addEscapeChar(const std::string& origin, bool isEndOfLine){
        std::string ret;
        for(auto c : origin){
            if(c == '"' || c == '\\' || c == '\n') {
                ret += "\\";
                ret += c;
            }else{
                ret += c;
            }
        }
        if(isEndOfLine){
            ret += "\\n";
        }
        return ret;
    }

    int matchPathId(const std::string& pathMask) {
        if(pathIdMap.count(pathMask) > 0){
            return pathIdMap[pathMask];
        }
        return INVALID_PATH_ID;
    }

    std::vector<int> matchPathIds(const std::string& pathMask) {
        std::vector<int> ret;
        Path::pathMask mask(pathMask);
        for(auto& path : paths){
            Path::pathMask pm(path.to_string());
            if(mask.isCover(pm)){
                ret.push_back(path.getId());
            }
        }
        return std::move(ret);
    }

    int matchBestPathId(const std::string& pathMask) {
        int pathId = matchPathId(pathMask);
        if(pathId != INVALID_PATH_ID && pathTestCntMap.count(pathId) == 0){
            pathTestCntMap[pathId]++;
            return pathId;
        }
        auto pathIds = matchPathIds(pathMask);
        if(pathIds.empty()){
            return INVALID_PATH_ID;
        }
        int minCnt = INT32_MAX;
        for(int pid : pathIds){
            if(pathTestCntMap.count(pid) == 0){
                pathTestCntMap[pid]++;
                return pid;
            }else if(pathTestCntMap[pid] < minCnt){
                minCnt = pathTestCntMap[pid];
                pathId = pid;
            }
        }
        pathTestCntMap[pathId]++;
        return pathId;
    }

    std::string dumpToDotGraph (){
        std::string dotString;
        dotString += "digraph G {\n";
        dotString += "\tlabel=\"CFG for " + func->getName().str() + " function\";\n";
        for(auto& node : nodes){
            dotString += "\t" + std::to_string(node.getId());
            dotString +=" [label=\"" + std::to_string(node.getId()) + "\\n" + node.getSrcInfo() + "\"];\n";
        }

        for(int i = 0; i < size; ++i){
            for(auto& next : edges[i]){
                dotString += "\t" + std::to_string(i) + " -> " + std::to_string(next) + ";\n";
            }
        }
        dotString += "}\n";
        return dotString;
    }

    friend void to_json(json& j, const CFG& cfg);
    friend void from_json(const json& j, CFG& cfg);
}; // class CFG

int CFG::count_ = 0;

/**
 * Json序列化: CFG
 */
void to_json(json& j, const CFG& cfg){
    j = json{
        {"id", cfg.id},
        {"size", cfg.size},
        {"nodes", cfg.nodes},
        {"edges", cfg.edges},
        {"paths", cfg.paths}
    };
}

void from_json(const json &j, CFG &cfg) {
    j.at("id").get_to(cfg.id);
    j.at("size").get_to(cfg.size);
    j.at("nodes").get_to(cfg.nodes);
    j.at("edges").get_to(cfg.edges);
//    j.at("paths").get_to(cfg.paths);
}


} //namespace PCTRT

#endif //PCTRT_CFG_H
