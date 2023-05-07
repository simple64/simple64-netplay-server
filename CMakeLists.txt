cmake_minimum_required(VERSION 3.20)

project(simple64-netplay-server LANGUAGES CXX)

include(CheckIPOSupported)
check_ipo_supported(RESULT ENABLE_IPO)

if(ENABLE_IPO AND (NOT CMAKE_BUILD_TYPE STREQUAL "Debug"))
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    message("Interprocedural optimizations enabled")
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

set_property(TARGET simple64-netplay-server PROPERTY C_VISIBILITY_PRESET hidden)
set_property(TARGET simple64-netplay-server PROPERTY CXX_VISIBILITY_PRESET hidden)
set_property(TARGET simple64-netplay-server PROPERTY VISIBILITY_INLINES_HIDDEN ON)
target_compile_options(simple64-netplay-server PRIVATE -march=x86-64-v3 -Ofast)
target_link_options(simple64-netplay-server PRIVATE -march=x86-64-v3 -Ofast -Wl,-Bsymbolic -Wl,-Bsymbolic-functions)
target_link_libraries(simple64-netplay-server PRIVATE Qt6::Network Qt6::WebSockets)
