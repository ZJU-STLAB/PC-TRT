# PC-TRT(path coverage test reuse tool) ：一个面向c语言单元测试的测试用例重用与用例生成工具

## 简介

测试用例在软件测试中十分重要，但测试用例的成本是非常昂贵的。为了节约成本，提高测试效率并提升测试的可靠性，我们提出了一个测试用例复用与用例生成工具PC-TRT。该工具的主要功能是以程序路径为目标，复用历史版本程序的测试用例，并且对未被覆盖的程序路径生成测试用例。PC-TRT有两个显而易见的优势：第一，复用旧版测试用例，尤其是测试用例的期望值部分，可以节省为被测程序确认测试用例的成本；第二，为未覆盖路径生成测试用例，可以大幅提升测试的路径覆盖率，从而提高测试的可靠性。

## 功能
- 功能1：静态分析以获取程序的路径集。该功能实现的流程是，首先通过编译器获取程序的控制流图，然后从中搜索出程序的所有路径，最后为每条路径赋予唯一编号，得到路径集。
- 功能2：动态分析以获取测试用例与程序路径的映射关系。该功能的流程是，生成或配置被测程序的驱动程序, 而后用编译器获取程序的IR中间码，最后对它进行自动插桩并运行插桩好的驱动程序。这会在程序上执行测试用例，获取每个测试用例与路径的映射关系。
- 功能3：重用测试用例
以从历史版本的测试用例集中选取可以复用的测试用例。该功能的流程是，首先遍历被测程序的路径集，对每条路径put，从历史程序的路径集中找到最相似的一条路径ph。然后计算put与ph的相似度，如果相似度超过阈值，则复用映射到ph的测试用例，包括输入值和期望值。如果其相似度未超过阈值，则复用映射到ph的测试用例的输入值。
- 功能4：生成未覆盖路径的测试用例
以生成可以覆盖目前未覆盖的路径的测试用例。首先，在被测程序上运行当前已复用的测试用例，包括仅有输入值的测试用例，从而获取被测程序的路径覆盖信息。然后为所有未覆盖路径生成测试用例。PC-TRT在面向路径生成测试用例集的工具KLEE上进行了优化，可以单独为未覆盖的路径生成测试用例的输入变量，而不是生成覆盖所有路径的测试用例。

特殊功能说明：
在特殊的输入设置下，PC-TRT可以为整个被测函数的所有路径生成满足路径覆盖的测试用例集
在特殊的参数设置下，PC-TRT可以复用或不复用历史版本测试用例的所有期望值。

## 安装与使用
### 安装步骤
安装llvm-13和clang-13，以及必要的开发软件
```bash
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
sudo add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-13 main"
sudo apt-get update
sudo apt-get install clang-13 llvm-13-dev libclang-13-dev build-essential git
```
### 安装klee并打补丁
```bash
sudo apt-get install build-essential cmake curl file g++-multilib gcc-multilib git libcap-dev libgoogle-perftools-dev libncurses5-dev libsqlite3-dev libtcmalloc-minimal4 python3-pip unzip graphviz doxygen

wget https://github.com/klee/klee/archive/refs/tags/v3.1.zip
unzip v3.1.zip
cd klee-3.1
git init
git config user.email "temp@example.com"
git config user.name "temp"
git add .
git commit -m "Initial commit"
git am your/location/of/0001-add-klee_path_trigger-and-klee_path_conditional_exit.patch

# 安装STP求解器
sudo apt-get install cmake bison flex libboost-all-dev python perl zlib1g-dev minisat
wget https://github.com/stp/stp/archive/refs/tags/2.3.3.zip
unzip 2.3.3.zip
cd stp-2.3.3
mkdir build
cd build
cmake ..
sudo make && make install
# 编译安装klee
cd klee-3.1
mkdir build
cd build
cmake -DENABLE_SOLVER_STP=ON ..
安装json：
#安装json
wget https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
unzip v3.11.3.zip
cd json-3.11.3
mkdir build
cd build
cmake ..
make
sudo make install
```
### PC-TRT使用
1. 下载项目至本地
2. 修改klee位置(若是按以上指令默认安装的，则无需修改位置)：在项目目录下，找到scripts/klee_ir.py文件，在第12行修改上一步骤中安装的klee的位置。
3. 指令与参数 
   - a. configuration
在配置文件src/utils/config.h中，可以设置SIMILARITY_THRESHOLD的值。该变量是路径相似速度的阈值，当路径a与路径b的相似度大于等于阈值时，复用覆盖路径b的测试用例，包括输入值与期望值，若小于阈值，则仅复用输入值。值得注意的是，当SIMILARITY_THRESHOLD设置为0时，工具将复用每一个被复用的测试用例的期望值，此时的期望值可信度不高，需要人工检验。而当SIMILARITY_THRESHOLD设置为1时，工具仅会复用完全相同的路径的期望值，此时的期望值可信度高，基本不需要人工检验。
    ⅰ. help
在bin目录下，输入指令 retest --help，可获得该工具的使用说明。

   - b. 指令
回到bin目录下执行以下指令可以完成PC-TRT的用例复用+未覆盖路径用例生成任务。
```bash
../bin/retest --old=./reverse_old.c --new=./reverse.c --func=reverse --test=./test_suite.json --cfg=1
```
4. 输入设置 
   - a. 输入当前被测程序
   - b. 输入历史版本程序
   - c. 以json格式输入历史版本程序的测试用例集，输入格式请参考test/test_suite.json。当输入的测试用例集为空时，本工具将会为被测函数内所有路径生成测试用例。
使用示例：
编译项目后运行，依据指令运行程序得到结果。下图分别是 history version of reverse.c ，the reverse.c under test， PC-TRT is running and the input test suite, the reused test cases and the generated test cases. 