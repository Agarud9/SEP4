cmake_minimum_required(VERSION 3.14)
project(iot_base_test)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_VERBOSE_MAKEFILE TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)


include_directories(
  ./include
  ../target/drivers/include
  ../target/src/include
  )
  
  
  add_executable(
    ${PROJECT_NAME}
    ""
  )

add_subdirectory(src)

target_link_libraries(
  ${PROJECT_NAME}
  GTest::gtest_main
)

gtest_discover_tests(  ${PROJECT_NAME})

add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running tests"
)
