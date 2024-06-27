# PC-TRT-a-test-case-reuse-and-generate-tool
PC-TRT (Path Coverage Test Reuse Tool) is a test case reuse and generation tool for unit testing in the C language.
## Introduction
Test cases are crucial in software testing, but their cost can be very high. To reduce costs, improve testing efficiency, and enhance the reliability of testing, we have developed a test case reuse and generation tool called PC-TRT. The primary function of this tool is to target program paths, reuse test cases from historical versions of the program, and generate test cases for uncovered program paths. PC-TRT has two significant advantages: First, by reusing old test cases, especially the expected value parts of the test cases, it can save the cost of confirming test cases for the program under test. Second, by generating test cases for uncovered paths, it can greatly improve path coverage, thereby enhancing the reliability of the tests.

## Functions
**function 1: Static Analysis**  
This function aims to obtain the set of program paths. The process involves first obtaining the control flow graph of the program through the compiler, then searching for all paths in the program, and finally assigning a unique identifier to each path to form the path set.  
**function 2: Dynamic Analysis**  
This function aims to obtain the mapping relationship between test cases and program paths. The process involves generating or configuring the driver program for the program under test, then using the compiler to obtain the program's IR (Intermediate Representation) code, and finally performing automatic instrumentation and running the instrumented driver program. This execution gathers the mapping relationship between each test case and the program paths.  
**function 3: Reusing Test Cases**  
This function selects reusable test cases from the test case set of historical versions. The process involves first traversing the path set of the program under test. For each path p1, the most similar path p2 is identified from the historical program's path set. Then, the similarity between p1 and p2 is calculated. If the similarity exceeds a certain threshold, the test cases mapped to p2 are reused, including both input values and expected values. If the similarity does not exceed the threshold, only the input values of the test cases mapped to p2 are reused.  
**function 4: Generating Test Cases for Uncovered Paths**  
This function generates test cases that can cover currently uncovered paths. Initially, the reused test cases, including those with only input values, are run on the program under test to obtain path coverage information. Subsequently, test cases are generated for all uncovered paths. PC-TRT is optimized based on the path-focused test case generation tool KLEE, enabling it to generate input variables specifically for uncovered paths rather than for all paths.  
**Special function Notes**
- With specific input settings, PC-TRT can generate a test case set that covers all paths of the entire function under test.
- With specific parameter settings, PC-TRT can choose to reuse or not reuse all expected values of test cases from historical versions.


## Install
### Environment and Dependencies
This tool needs to be used in Linux system  

**Install llvm-13 and clang-13, as well as necessary development software**
```bash
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
sudo add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-13 main"
sudo apt-get update
sudo apt-get install clang-13 llvm-13-dev libclang-13-dev build-essential git
# compile and install
sudo apt-get install build-essential cmake curl file g++-multilib gcc-multilib git libcap-dev libgoogle-perftools-dev libncurses5-dev libsqlite3-dev libtcmalloc-minimal4 python3-pip unzip graphviz doxygen
```

**Install klee and patch it**
```bash
wget https://github.com/klee/klee/archive/refs/tags/v3.1.zip
unzip v3.1.zip
cd klee-3.1
git init
git config user.email "temp@example.com"
git config user.name "temp"
git add .
git commit -m "Initial commit"
git am your/location/of/0001-add-klee_path_trigger-and-klee_path_conditional_exit.patch

# install STP solver
sudo apt-get install cmake bison flex libboost-all-dev python perl zlib1g-dev minisat
wget https://github.com/stp/stp/archive/refs/tags/2.3.3.zip
unzip 2.3.3.zip
cd stp-2.3.3
mkdir build
cd build
cmake ..
make
sudo make install
# klee
cd klee-3.1
mkdir build
cd build
cmake -DENABLE_SOLVER_STP=ON ..
```

**install json**
```bash
#install json
wget https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
unzip v3.11.3.zip
cd json-3.11.3
mkdir build
cd build
cmake ..
make
sudo make install
```

## PC-TRT Usage

1. **Download the project to your local machine**

2. **Modify the KLEE path** (If you installed KLEE according to the default instructions, you do not need to modify the path):
    - In the project directory, find the file `scripts/klee_ir.py`.
    - Modify the KLEE path on line 12 to the location where KLEE was installed in the previous step.

3. **Commands and Parameters**
    - **Configuration**
        - In the configuration file `src/utils/config.h`, you can set the value of `SIMILARITY_THRESHOLD`. This variable is the threshold for path similarity. When the similarity between path `a` and path `b` is greater than or equal to the threshold, the test cases covering path `b` are reused, including input values and expected values. If the similarity is less than the threshold, only the input values are reused. 
        - Note: 
            - When `SIMILARITY_THRESHOLD` is set to 0, the tool reuses the expected values of every test case, but these expected values have low credibility and require manual verification.
            - When `SIMILARITY_THRESHOLD` is set to 1, the tool only reuses the expected values of exactly matching paths, which have high credibility and generally do not require manual verification.
        - **Help**
            - In the `bin` directory, enter the command `retest --help` to get the usage instructions for the tool.
   图

    - **Commands**
        - Go to the `bin` directory and execute the following command to perform the PC-TRT test case reuse and generate test cases for uncovered paths:
          ```bash
          ../bin/retest --old=./reverse_old.c --new=./reverse.c --func=reverse --test=./test_suite.json --cfg=1
          ```

4. **Input Settings**
    - **Input the current program under test**
    - **Input the historical version of the program**
    - **Input the test case set of the historical version in JSON format**. Refer to `test/test_suite.json` for the input format. If the input test case set is empty, the tool will generate test cases for all paths within the function under test.


## Example：
Compile the project and run it. Run the program according to the instructions to get the results. The following figures are：
- history version of reverse.c
- the reverse.c under test
- PC-TRT is running
- the input test suite, the reused test cases and the generated test cases. 
图1234



## license
This project is licensed under the MIT License.
