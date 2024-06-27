#ifndef PCTRT_CONFIG_H
#define PCTRT_CONFIG_H

#include <string>
// 定义编译器返回值
#define INVALID_PATH_ID -1
#define SIMILARITY_THRESHOLD 0.35
#define KLEE_ARRAY_SIZE 5

const std::string COMPILER = "clang-13 ";
const std::string IR_COMPILE_OPTIONS = " -S -emit-llvm -g ";

const std::string KLEE_SCRIPT = "../scripts/klee_ir.py ";
const std::string IR2PNG_SCRIPT = "../scripts/ir2png.py ";
const std::string CLEAN_SCRIPT = "../scripts/clean.py ";


#endif