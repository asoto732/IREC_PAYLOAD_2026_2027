cmake_minimum_required(VERSION 3.10)
project(payload_software CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

include_directories(include)

add_library(payload_core
    src/DataLogger.cpp
)

find_package(Threads REQUIRED)

add_executable(payload_collector
    src/main.cpp
)
target_link_libraries(payload_collector PRIVATE payload_core Threads::Threads)

enable_testing()
add_subdirectory(tests)
