cmake_minimum_required(VERSION 3.11)

project(rlbox_api-cpp17 VERSION 0.1
    DESCRIPTION "LP64 to LP32 converter in C++17")

# Add magic_get
include(FetchContent)
FetchContent_Declare(magic_get
    GIT_REPOSITORY https://github.com/apolukhin/magic_get.git
    GIT_TAG 82658caf7c283fdf843f815904e32f8dc93c9d89
)

FetchContent_GetProperties(magic_get)
if (NOT magic_get_POLULATED)
    FetchContent_Populate(magic_get)
    add_library(magic_get INTERFACE)
    target_include_directories(magic_get INTERFACE ${magic_get_SOURCE_DIR}/include)
    target_compile_features(magic_get INTERFACE cxx_std_14)
endif()

add_library(rlbox-cpp17 INTERFACE)
target_include_directories(rlbox-cpp17 INTERFACE include/)
target_compile_features(rlbox-cpp17 INTERFACE cxx_std_17)
target_link_libraries(rlbox-cpp17 INTERFACE magic_get)

enable_testing()

# Compile tests
include(cmake/test-compile.cmake)

add_library(rlbox-cpp17-test-base INTERFACE)
target_include_directories(rlbox-cpp17-test-base INTERFACE test/include/)
target_compile_features(rlbox-cpp17-test-base INTERFACE cxx_std_17)

add_executable(test-a test/compile-test/a_pass.cpp)
target_link_libraries(test-a rlbox-cpp17 rlbox-cpp17-test-base)
target_compile_test(test-a TRUE)

add_executable(test-b test/compile-test/b_fail.cpp)
target_link_libraries(test-b rlbox-cpp17 rlbox-cpp17-test-base)
target_compile_test(test-b FALSE)
