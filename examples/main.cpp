/**
 * main.cpp - Application entry point
 * 
 * Initializes and starts both capsule and transceiver components
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <cerrno>   // For errno
#include <cstring>  // For strerror
#include <iostream>
#include <vector>
#include <sstream>

#include <algorithm>
#include <memory>
#include <string_view>
#include <iomanip>

// Boost库
// #include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/program_options.hpp>
#include "../TinyFrame.h"
#include "rf_device.h"
#include "common.h"

// 引用tf_thread.cpp中的外部函数
extern void tf_board_init(void);
extern void tf_thread_start(void *arg);
extern void tf_board_cleanup(void);
extern void* tf_thread(void* arg);


// namespace bip = boost::interprocess;
namespace po = boost::program_options;


// 全局标志，用于控制程序退出
static volatile bool running = true;


// RF设备的简化实现（只打印日志）
static bool rf_dev_init(rf_device_t *dev, rf_callback_t callback) {
    std::cout << "[RF] 初始化设备" << std::endl;
    
    // 保存回调函数指针到私有数据区
    dev->priv = reinterpret_cast<void*>(callback);
    
    std::cout << "[RF] 设备初始化成功" << std::endl;
    return true;
}

static bool rf_dev_deinit(rf_device_t *dev) {
    std::cout << "[RF] 关闭设备" << std::endl;
    dev->priv = nullptr;
    return true;
}

static int rf_dev_transmit(rf_device_t *dev, const uint8_t *data, size_t len) {
    // 打印发送的数据
    std::cout << "[RF] 发送 " << len << " 字节数据: ";
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // 模拟调用回调函数，将发送的数据传递给接收端
    if (dev->priv) {
        rf_callback_t callback = reinterpret_cast<rf_callback_t>(dev->priv);
        callback(dev, data, len);
    }
    
    return static_cast<int>(len);
}

static int rf_dev_receive(rf_device_t *dev, uint8_t *buffer, size_t max_len) {
    // 模拟没有接收到数据的情况
    return 0;
}

// 信号处理函数
void signal_handler(int sig) {
    std::cout << "\n接收到信号 " << sig << "，准备退出..." << std::endl;
    running = false;
}

int main(int argc, char *argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 命令行参数处理
    po::options_description desc("允许的选项");
    desc.add_options()
        ("help,h", "显示帮助信息")
        ("transmit,t", po::value<std::string>(), "发送一条十六进制消息，例如: --transmit=\"01 02 03\"");
    
    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (std::exception &e) {
        std::cerr << "命令行参数错误: " << e.what() << std::endl;
        return 1;
    }
    
    // 显示帮助
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }
    
    // 初始化RF设备
    rf_device_t rf_dev = {0};
    rf_dev.ops.init = rf_dev_init;
    rf_dev.ops.deinit = rf_dev_deinit;
    rf_dev.ops.transmit = rf_dev_transmit;
    rf_dev.ops.receive = rf_dev_receive;
    
    // 初始化TinyFrame板级支持
    tf_board_init();
    
    // 发送模式
    if (vm.count("transmit")) {
        std::string hex_str = vm["transmit"].as<std::string>();
        std::vector<uint8_t> data;
        
        // 解析十六进制字符串
        std::stringstream ss(hex_str);
        std::string byte_str;
        while (ss >> byte_str) {
            try {
                data.push_back(std::stoi(byte_str, nullptr, 16));
            } catch (...) {
                std::cerr << "无效的十六进制数: " << byte_str << std::endl;
                tf_board_cleanup();
                return 1;
            }
        }
        
        if (data.empty()) {
            std::cerr << "没有提供有效的十六进制数据" << std::endl;
            tf_board_cleanup();
            return 1;
        }
        
        // 初始化RF设备
        if (!rf_dev.ops.init(&rf_dev, nullptr)) {
            std::cerr << "RF设备初始化失败" << std::endl;
            tf_board_cleanup();
            return 1;
        }
        
        // 发送消息
        int sent = rf_dev.ops.transmit(&rf_dev, data.data(), data.size());
        if (sent > 0) {
            std::cout << "成功发送 " << sent << " 字节: ";
            for (auto byte : data) {
                printf("%02X ", byte);
            }
            std::cout << std::endl;
        } else {
            std::cerr << "发送失败" << std::endl;
        }
        
        // 清理资源
        rf_dev.ops.deinit(&rf_dev);
        tf_board_cleanup();
        return 0;
    }
    
    // 接收模式（默认）
    std::cout << "启动TinyFrame处理线程，按Ctrl+C退出" << std::endl;
    
    // 初始化RF设备
    if (!rf_dev.ops.init(&rf_dev, nullptr)) {
        std::cerr << "RF设备初始化失败" << std::endl;
        tf_board_cleanup();
        return 1;
    }
    
    // 启动TinyFrame处理线程
    tf_thread_start(&rf_dev);
    
    // 主线程等待退出信号
    while (running) {
        sleep(1);
    }
    
    // 清理资源
    rf_dev.ops.deinit(&rf_dev);
    tf_board_cleanup();
    
    return 0;
}