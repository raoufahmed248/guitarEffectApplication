cmake_minimum_required(VERSION 3.14)
project(audioEffectsLib_test)

# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(
  audioEffectsLibTest
  audioEffectsLibTest.cc
)
target_link_libraries(
  audioEffectsLibTest
  audioEffectsLibSrc
  gtest_main
)

# an executable for the sample test
gtest_discover_tests(audioEffectsLibTest)
