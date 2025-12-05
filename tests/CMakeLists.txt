cmake_minimum_required(VERSION 3.14)
project(MyTests)

enable_testing()

# Try to find local GTest installation first, fallback to FetchContent
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "GTest not found locally, fetching from GitHub...")
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
else()
  message(STATUS "Using locally installed GTest")
endif()

include_directories(${CMAKE_SOURCE_DIR})

# Build tests by linking against the library target `webserv_lib` so we don't recompile sources
add_executable(runTests test_main.cpp ../src/utils/utils_test.cpp ../src/utils/file_utils_test.cpp ../src/config/Config_test.cpp ../src/http/Uri_test.cpp)
target_link_libraries(runTests PRIVATE GTest::gtest_main webserv_core webserv_http webserv_config webserv_handlers webserv_utils pthread)

# Tests must be compiled with C++11 to support GoogleTest
target_compile_features(runTests PRIVATE cxx_std_11)
target_compile_options(runTests PRIVATE -Wall -Wextra -Werror)

# Allow tests to see private headers next to sources
target_include_directories(runTests PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

add_test(NAME MyTest COMMAND runTests)
