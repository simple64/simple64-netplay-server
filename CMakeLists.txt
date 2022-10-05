cmake_minimum_required(VERSION 3.20)

project(simple64-netplay-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# check for INTERPROCEDURAL_OPTIMIZATION support
if((${CMAKE_VERSION} VERSION_EQUAL 3.9) OR (${CMAKE_VERSION} VERSION_GREATER 3.9))
    cmake_policy(SET CMP0069 NEW)

    include(CheckIPOSupported)
    check_ipo_supported(RESULT ENABLE_IPO)
    if(ENABLE_IPO)
        message("Interprocedural optimizations enabled")
    endif(ENABLE_IPO)
endif()

if(ENABLE_IPO AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x0602ff)

find_package(Qt6 REQUIRED COMPONENTS Network WebSockets)
qt_standard_project_setup()

add_executable(simple64-netplay-server
    main.cpp
    udpServer.cpp
    tcpServer.cpp
    serverThread.cpp
    socketServer.cpp
)

target_compile_options(simple64-netplay-server PRIVATE -march=x86-64-v3)
target_link_options(simple64-netplay-server PRIVATE -march=x86-64-v3)
target_link_libraries(simple64-netplay-server PRIVATE Qt6::Network Qt6::WebSockets)
