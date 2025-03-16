cmake_minimum_required(VERSION 3.10)

project(simple-logger)

option(SIMPLE_LOGGER_TESTING "Enable testing" OFF)

if(SIMPLE_LOGGER_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(googletest)
endif()

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

if(SIMPLE_LOGGER_TESTING)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

file(GLOB SRC simple_logger/*.cpp)
file(GLOB HDR simple_logger/*.h)
add_library(simple_logger STATIC ${SRC} ${HDR})
target_include_directories(simple_logger PUBLIC ${CMAKE_SOURCE_DIR})

file(GLOB SRC grep/*.cpp)
file(GLOB HDR grep/*.h)
add_executable(grep  ${SRC} ${HDR})
target_include_directories(grep PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(grep PUBLIC simple_logger)

if(SIMPLE_LOGGER_TESTING)
    file(GLOB SRC tests/*.cpp)
    file(GLOB HDR tests/*.h)
    add_executable(simple_logger_test  ${SRC} ${HDR})
    target_include_directories(simple_logger_test PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(simple_logger_test PUBLIC simple_logger gtest gtest_main)
    add_test(NAME test COMMAND simple_logger_test)
endif()
