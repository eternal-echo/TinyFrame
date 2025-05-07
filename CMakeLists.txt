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

# 添加示例可执行文件
add_executable(capsule_comm
    examples/main.cpp
    examples/tf_thread.cpp
)

# 链接依赖项
target_link_libraries(capsule_comm PRIVATE
    tinyframe
    Boost::program_options
    Threads::Threads
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(capsule_comm PRIVATE rt) # 对于 POSIX 消息队列
endif()

# 包含目录
target_include_directories(capsule_comm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/examples
    ${Boost_INCLUDE_DIRS}
)

# 在 Windows 上设置模拟 POSIX 函数
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(capsule_comm PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        MQ_IMPL_WIN=1
    )
endif()

# 安装规则
install(TARGETS capsule_comm
    RUNTIME DESTINATION bin
)

# 启用调试信息
set(CMAKE_BUILD_TYPE Debug)
add_compile_options(-g)

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")