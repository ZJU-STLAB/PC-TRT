cmake_minimum_required(VERSION 3.26)
project(ReTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
message("output dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

find_package(GTest REQUIRED)

find_package(nlohmann_json REQUIRED)

find_package(LLVM REQUIRED)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIR})
llvm_map_components_to_libnames(llvm_libs support core irreader native)
message("using llvm libs: ${llvm_libs}")
message("LLVM DIR: ${LLVM_INCLUDE_DIR}")

include_directories("src")

# enable_testing()

# add_executable(
#   hello_test
#   "test/test.cpp"
# )
# target_link_libraries(
#   hello_test
#   GTest::gtest_main
# )

# include(GoogleTest)
# gtest_discover_tests(hello_test)

add_executable(retest "src/main.cpp")
target_link_libraries(retest ${llvm_libs} pthread)

add_executable(generate_test "test/test.cpp")
target_link_libraries(generate_test ${llvm_libs} pthread)