cmake_minimum_required(VERSION 3.26)
set(CMAKE_C_COMPILER "clang-13")
set(CMAKE_CXX_COMPILER "clang++-13")
set(CMAKE_CXX_STANDARD 17)
project(ReTest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
message("output dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

find_package(nlohmann_json REQUIRED)
find_package(LLVM REQUIRED)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIR})
llvm_map_components_to_libnames(llvm_libs support core irreader native)
message("using llvm libs: ${llvm_libs}")

include_directories("src")
add_executable(retest "src/main.cpp")
target_link_libraries(retest ${llvm_libs} pthread)