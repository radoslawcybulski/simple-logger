cmake_minimum_required(VERSION 3.10)

project(simple-logger)

include(CTest)
enable_testing()

set(CMAKE_GENERATOR_PLATFORM x64)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_CXX_STANDARD 20)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

set(BOOST_INCLUDE_LIBRARIES program_options)

if(WIN32)
add_definitions(-D_WIN32_WINNT=0x0601)
endif()

add_subdirectory(googletest)

file(GLOB SRC simple_logger/*.cpp)
file(GLOB HDR simple_logger/*.h)
add_library(simple_logger STATIC ${SRC} ${HDR})
target_include_directories(simple_logger PUBLIC ${CMAKE_SOURCE_DIR})

file(GLOB SRC tests/*.cpp)
file(GLOB HDR tests/*.h)
add_executable(simple_logger_test  ${SRC} ${HDR})
target_include_directories(simple_logger_test PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(simple_logger_test PUBLIC simple_logger gtest gtest_main)

add_test(NAME test COMMAND simple_logger_test)
