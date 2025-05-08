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
#include <cerrno>
#include <cstring>
#include <iostream>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <memory>
#include <string_view>
#include <iomanip>
#include <fcntl.h>           // For O_* constants
#include <sys/stat.h>        // For mode constants
#include <mqueue.h>          // For POSIX message queue

#include <boost/program_options.hpp>
#include "../TinyFrame.h"
#include "rf_device.h"
#include "common.h"

/* 消息队列名称定义 */
#define SERVER_TO_CLIENT_MQ "/tf_server_to_client"
#define CLIENT_TO_SERVER_MQ "/tf_client_to_server"

// RF设备上下文类
class RFContext {
public:
    RFContext() : mq_tx((mqd_t)-1), mq_rx((mqd_t)-1), test_mode(false) {
        attr.mq_flags = 0;
        attr.mq_maxmsg = MAX_MSG_COUNT;
        attr.mq_msgsize = MAX_MSG_SIZE;
        attr.mq_curmsgs = 0;
    }
    
    ~RFContext() { cleanup(); }

    bool init(bool is_test) {
        test_mode = is_test;
        try {
#if BOARD_ID == BOARD_SERVER_ID
            return test_mode ? connect_server() : create_server();
#else
            return test_mode ? connect_client() : create_client();
#endif
        } catch (const std::exception& e) {
            std::cerr << "[RF] 初始化失败: " << e.what() << std::endl;
            cleanup();
            return false;
        }
    }

    int send(const uint8_t *data, size_t len) {
        if (mq_tx == (mqd_t)-1) return -1;
        
        if (mq_send(mq_tx, (const char*)data, len, 0) == -1) {
            std::cerr << "[RF] 发送失败: " << strerror(errno) << std::endl;
            return -1;
        }
        
        return static_cast<int>(len);
    }

    int receive(uint8_t *buffer, size_t max_len) {
        if (mq_rx == (mqd_t)-1) return -1;
        
        ssize_t recv_len = mq_receive(mq_rx, (char*)buffer, max_len, nullptr);
        if (recv_len == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;  // 没有消息可读
            }
            std::cerr << "[RF] 接收失败: " << strerror(errno) << std::endl;
            return -1;
        }
        
        return static_cast<int>(recv_len);
    }

private:
    static const size_t MAX_MSG_SIZE = 256;
    static const size_t MAX_MSG_COUNT = 10;

    mqd_t mq_tx;
    mqd_t mq_rx;
    bool test_mode;
    struct mq_attr attr;

    bool create_server() {
        mq_unlink(SERVER_TO_CLIENT_MQ);
        mq_unlink(CLIENT_TO_SERVER_MQ);
        
        mq_tx = mq_open(SERVER_TO_CLIENT_MQ, O_CREAT | O_WRONLY, 0666, &attr);
        if (mq_tx == (mqd_t)-1) {
            throw std::runtime_error(std::string("创建发送队列失败: ") + strerror(errno));
        }
        
        mq_rx = mq_open(CLIENT_TO_SERVER_MQ, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
        if (mq_rx == (mqd_t)-1) {
            cleanup();
            throw std::runtime_error(std::string("创建接收队列失败: ") + strerror(errno));
        }
        return true;
    }

    bool create_client() {
        mq_unlink(SERVER_TO_CLIENT_MQ);
        mq_unlink(CLIENT_TO_SERVER_MQ);
        
        mq_tx = mq_open(CLIENT_TO_SERVER_MQ, O_CREAT | O_WRONLY, 0666, &attr);
        if (mq_tx == (mqd_t)-1) {
            throw std::runtime_error(std::string("创建发送队列失败: ") + strerror(errno));
        }
        
        mq_rx = mq_open(SERVER_TO_CLIENT_MQ, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
        if (mq_rx == (mqd_t)-1) {
            cleanup();
            throw std::runtime_error(std::string("创建接收队列失败: ") + strerror(errno));
        }
        return true;
    }

    bool connect_server() {
        mq_tx = mq_open(SERVER_TO_CLIENT_MQ, O_WRONLY | O_NONBLOCK);
        if (mq_tx == (mqd_t)-1) {
            throw std::runtime_error(std::string("打开发送队列失败: ") + strerror(errno));
        }

        mq_rx = mq_open(CLIENT_TO_SERVER_MQ, O_RDONLY | O_NONBLOCK);
        if (mq_rx == (mqd_t)-1) {
            cleanup();
            throw std::runtime_error(std::string("打开接收队列失败: ") + strerror(errno));
        }
        return true;
    }

    bool connect_client() {
        mq_tx = mq_open(CLIENT_TO_SERVER_MQ, O_WRONLY | O_NONBLOCK);
        if (mq_tx == (mqd_t)-1) {
            throw std::runtime_error(std::string("打开发送队列失败: ") + strerror(errno));
        }

        mq_rx = mq_open(SERVER_TO_CLIENT_MQ, O_RDONLY | O_NONBLOCK);
        if (mq_rx == (mqd_t)-1) {
            cleanup();
            throw std::runtime_error(std::string("打开接收队列失败: ") + strerror(errno));
        }
        return true;
    }

    void cleanup() {
        if (mq_tx != (mqd_t)-1) {
            mq_close(mq_tx);
            mq_tx = (mqd_t)-1;
        }
        if (mq_rx != (mqd_t)-1) {
            mq_close(mq_rx);
            mq_rx = (mqd_t)-1;
        }
        if (!test_mode) {
#if BOARD_ID == BOARD_SERVER_ID
            mq_unlink(SERVER_TO_CLIENT_MQ);
            mq_unlink(CLIENT_TO_SERVER_MQ);
#endif
        }
    }
};

extern void tf_board_init(void *arg);
extern void tf_board_cleanup();
extern void tf_thread_start();

namespace po = boost::program_options;

extern TinyFrame *tf_ctx;

static volatile bool running = true;
static volatile bool rf_test_mode = false;

// RF设备的实现
static bool rf_dev_init(rf_device_t *dev, rf_callback_t callback) {
    std::cout << "[RF] 初始化设备" << std::endl;
    try {
        auto *ctx = new RFContext();
        dev->priv = ctx;
        if (!ctx->init(rf_test_mode)) {
            delete ctx;
            dev->priv = nullptr;
            return false;
        }
        std::cout << "[RF] 设备在" << (rf_test_mode ? "测试模式" : "正常模式") << "下初始化成功" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[RF] 初始化失败: " << e.what() << std::endl;
        return false;
    }
}

static bool rf_dev_deinit(rf_device_t *dev) {
    std::cout << "[RF] 关闭设备" << std::endl;
    if (dev->priv) {
        delete static_cast<RFContext*>(dev->priv);
        dev->priv = nullptr;
    }
    return true;
}

static int rf_dev_transmit(rf_device_t *dev, const uint8_t *data, size_t len) {
    auto *ctx = static_cast<RFContext*>(dev->priv);
    if (!ctx) return -1;
    
    int ret = ctx->send(data, len);
    if (ret > 0) {
        std::cout << "[RF] 发送 " << len << " 字节数据: ";
        for (size_t i = 0; i < len; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                    << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }
    return ret;
}

static int rf_dev_receive(rf_device_t *dev, uint8_t *buffer, size_t max_len) {
    auto *ctx = static_cast<RFContext*>(dev->priv);
    if (!ctx) return -1;
    
    int ret = ctx->receive(buffer, max_len);
    if (ret > 0) {
        std::cout << "[RF] 接收 " << ret << " 字节数据" << std::endl;
    }
    return ret;
}

// 信号处理函数
void signal_handler(int sig) {
    std::cout << "\n接收到信号 " << sig << "，准备退出..." << std::endl;
    running = false;
}

// 添加数据打包函数
static std::vector<uint8_t> pack_imu_data(float ax, float ay, float az, 
                                         float gx, float gy, float gz,
                                         float mx, float my, float mz) {
    tf_data_t data = {0};
    data.data.imu_data.accel[0] = ax;
    data.data.imu_data.accel[1] = ay;
    data.data.imu_data.accel[2] = az;
    data.data.imu_data.gyro[0] = gx;
    data.data.imu_data.gyro[1] = gy;
    data.data.imu_data.gyro[2] = gz;
    data.data.imu_data.mag[0] = mx;
    data.data.imu_data.mag[1] = my;
    data.data.imu_data.mag[2] = mz;
    data.timestamp = static_cast<uint32_t>(time(nullptr));
    
    return std::vector<uint8_t>(
        reinterpret_cast<uint8_t*>(&data),
        reinterpret_cast<uint8_t*>(&data) + sizeof(tf_data_t)
    );
}

static std::vector<uint8_t> pack_pressure_data(float pressure) {
    tf_data_t data = {0};
    data.data.pressure_data.pressure_hpa = pressure;
    data.timestamp = static_cast<uint32_t>(time(nullptr));
    
    return std::vector<uint8_t>(
        reinterpret_cast<uint8_t*>(&data),
        reinterpret_cast<uint8_t*>(&data) + sizeof(tf_data_t)
    );
}

static std::vector<uint8_t> pack_command(uint8_t cmd) {
    tf_data_t data = {0};
    data.data.cmd.command = cmd;
    data.timestamp = static_cast<uint32_t>(time(nullptr));
    
    return std::vector<uint8_t>(
        reinterpret_cast<uint8_t*>(&data),
        reinterpret_cast<uint8_t*>(&data) + sizeof(tf_data_t)
    );
}

int main(int argc, char *argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 命令行参数处理
    po::options_description desc("允许的选项");
    desc.add_options()
        ("help,h", "显示帮助信息")
#if BOARD_ID == BOARD_SERVER_ID
        ("transmit,t", po::value<std::string>(), 
            "发送数据。支持以下格式：\n"
            "  控制命令: -t cmd:start 或 cmd:stop")
#else
        ("transmit,t", po::value<std::string>(), 
            "发送数据。支持以下格式：\n"
            "  IMU数据:  -t imu:ax,ay,az,gx,gy,gz,mx,my,mz\n"
            "  压力数据: -t pressure:1013.25")
#endif
        ("repeat,r", po::value<int>()->default_value(1), "重复发送次数") // 新增重复发送参数
    ;
    
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

        
    // 初始化板载设备（正常模式）
    tf_board_init(&rf_dev);  // 初始化TinyFrame和RF设备

    if (vm.count("transmit")) {
        // 测试模式
        std::string input = vm["transmit"].as<std::string>();
        int repeat_count = vm["repeat"].as<int>();
        
        // 初始化TinyFrame实例
#if BOARD_ID == BOARD_SERVER_ID
        tf_ctx = TF_Init(TF_MASTER);
#else
        tf_ctx = TF_Init(TF_SLAVE);
#endif
        if (tf_ctx == NULL) {
            std::cerr << "TinyFrame初始化失败" << std::endl;
            return 1;
        }

        // 初始化RF设备
        rf_test_mode = true;  // 测试模式
        if (!rf_dev.ops.init(&rf_dev, nullptr)) {  // 测试模式通过nullptr标识
            std::cerr << "RF设备初始化失败" << std::endl;
            TF_DeInit(tf_ctx);
            return 1;
        }

        // 解析命令格式 type:data
        size_t pos = input.find(':');
        if (pos == std::string::npos) {
            std::cerr << "无效的命令格式，请使用 type:data 格式" << std::endl;
            std::cout << desc << std::endl;
            return 1;
        }
        
        std::string type = input.substr(0, pos);
        std::string data_str = input.substr(pos + 1);
        std::vector<uint8_t> data;
        
        try {
            // 创建TinyFrame消息
            TF_Msg msg;
            TF_ClearMsg(&msg);
            
#if BOARD_ID == BOARD_SERVER_ID
            // 服务端只处理控制命令
            if (type == "cmd") {
                uint8_t cmd;
                if (data_str == "start") {
                    cmd = TF_CMD_START;
                } else if (data_str == "stop") {
                    cmd = TF_CMD_STOP;
                } else {
                    throw std::runtime_error("无效的控制命令");
                }
                data = pack_command(cmd);
                msg.type = TF_TYPE_CMD;
                msg.data = data.data();
                msg.len = data.size();
            } else {
                throw std::runtime_error("服务端只支持控制命令(cmd:start/stop)");
            }
#else
            // 客户端只处理传感器数据
            if (type == "imu") {
                std::vector<float> values;
                std::stringstream ss(data_str);
                std::string value_str;
                while (std::getline(ss, value_str, ',')) {
                    values.push_back(std::stof(value_str));
                }
                
                if (values.size() != 9) {
                    throw std::runtime_error("IMU数据需要9个参数");
                }
                
                data = pack_imu_data(
                    values[0], values[1], values[2],
                    values[3], values[4], values[5],
                    values[6], values[7], values[8]
                );
                msg.type = TF_TYPE_SENSOR_IMU;
                msg.data = data.data();
                msg.len = data.size();
            }
            else if (type == "pressure") {
                float pressure = std::stof(data_str);
                data = pack_pressure_data(pressure);
                msg.type = TF_TYPE_SENSOR_PRESSURE;
                msg.data = data.data();
                msg.len = data.size();
            }
            else {
                throw std::runtime_error("客户端只支持IMU和压力传感器数据");
            }
#endif
            
            // 发送数据到另一端
            for (int i = 0; i < repeat_count; i++) {
                bool sent = TF_Send(tf_ctx, &msg);
                if (sent) {
                    std::cout << "消息发送成功 (" << (i + 1) << "/" << repeat_count << ")" << std::endl;
                    if (i < repeat_count - 1) {
                        // 在重复发送之间添加短暂延迟
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                } else {
                    std::cerr << "消息发送失败" << std::endl;
                    break;
                }
            }
            
            // 清理资源
            rf_dev.ops.deinit(&rf_dev);
            TF_DeInit(tf_ctx);
            
        } catch (const std::exception& e) {
            std::cerr << "错误: " << e.what() << std::endl;
            std::cout << desc << std::endl;
            rf_dev.ops.deinit(&rf_dev);
            if (tf_ctx) TF_DeInit(tf_ctx);
            return 1;
        }
        
        return 0;
    } else {
        // 正常运行模式
        std::cout << "启动TinyFrame处理线程，按Ctrl+C退出" << std::endl;

        rf_test_mode = false;  // 正常模式
        
        // 启动TinyFrame处理线程
        tf_thread_start();
        
        // 主线程等待退出信号
        while (running) {
            sleep(1);
        }
        
        // 清理资源
        rf_dev.ops.deinit(&rf_dev);
    }
    
    return 0;
}