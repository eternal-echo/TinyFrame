cmake_minimum_required(VERSION 3.10)
project(TinyFrame VERSION 1.0 LANGUAGES C CXX)

# 设置 C/C++ 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 检查操作系统
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Compiling on Windows")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    message(STATUS "Compiling on ${CMAKE_SYSTEM_NAME}")
endif()

# 查找依赖项
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(Threads REQUIRED)

# 添加 TinyFrame 库
add_library(tinyframe STATIC
    TinyFrame.c
)

target_include_directories(tinyframe PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/examples
)

# 添加通用编译标志函数
function(add_tf_executable target_name board_id)
    add_executable(${target_name}
        examples/main.cpp
        examples/tf_thread.cpp
    )

    target_compile_definitions(${target_name} PRIVATE
        BOARD_ID=${board_id}
    )

    target_link_libraries(${target_name} PRIVATE
        tinyframe
        Boost::program_options
        Threads::Threads
    )

    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_link_libraries(${target_name} PRIVATE rt)
    endif()

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        ${Boost_INCLUDE_DIRS}
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_compile_definitions(${target_name} PRIVATE 
            _CRT_SECURE_NO_WARNINGS
            MQ_IMPL_WIN=1
        )
    endif()

    # 安装规则
    install(TARGETS ${target_name}
        RUNTIME DESTINATION bin
    )
endfunction()

# 添加服务端和客户端可执行文件
add_tf_executable(tf_server BOARD_SERVER_ID)
add_tf_executable(tf_client BOARD_CLIENT_ID)

# 启用调试信息
set(CMAKE_BUILD_TYPE Debug)
add_compile_options(-g)

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")