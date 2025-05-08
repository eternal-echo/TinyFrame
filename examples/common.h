/**
 * common.h - Shared definitions for capsule communication
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <time.h>
#include <stdio.h>
#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif

/* Logging macros */
#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_DEBUG 2

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define LOG_ERROR(fmt, ...) \
    if (LOG_LEVEL >= LOG_LEVEL_ERROR) { \
        fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); \
    }

#define LOG_INFO(fmt, ...) \
    if (LOG_LEVEL >= LOG_LEVEL_INFO) { \
        fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__); \
    }

#define LOG_DEBUG(fmt, ...) \
    if (LOG_LEVEL >= LOG_LEVEL_DEBUG) { \
        fprintf(stdout, "[DEBUG] " fmt "\n", ##__VA_ARGS__); \
    }


#pragma pack(push, 1)  // 禁用字节对齐填充

/* TinyFrame message types */
#define TF_TYPE_CMD     0
#define TF_TYPE_SENSOR_IMU  1
#define TF_TYPE_SENSOR_PRESSURE 2

#define TF_CMD_START  0x01
#define TF_CMD_STOP   0x02

/* TinyFrame message structure */
struct imu_data {
    float accel[3];  // 加速度计数据
    float gyro[3];   // 陀螺仪数据
    float mag[3];    // 磁力计数据
};
typedef struct imu_data imu_data_t;

struct pressure_data {
    float pressure_hpa;  // 压力数据（hPa）
};
typedef struct pressure_data pressure_data_t;

struct control_command {
    uint8_t command;  // 控制命令
};
typedef struct control_command control_command_t;

struct tf_data {
    union {
        imu_data_t imu_data;         // IMU数据
        pressure_data_t pressure_data; // 压力数据
        control_command_t cmd;       // 控制命令
    } data;  // 数据
    uint32_t timestamp;  // 时间戳
};  // 确保结构体按字节对齐
typedef struct tf_data tf_data_t;

#pragma pack(pop)  // 恢复字节对齐填充

/* 设备类型定义 */
#define BOARD_SERVER_ID  1   // 服务端设备
#define BOARD_CLIENT_ID  2   // 客户端设备

// 如果没有定义BOARD_ID，默认为服务端
#ifndef BOARD_ID
#define BOARD_ID BOARD_SERVER_ID
#endif


#endif /* COMMON_H */