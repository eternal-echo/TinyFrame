#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../TinyFrame.h"

/* -- Global variables -- */

// - 接收模式：
// #define RF_RX_MODE_ASYNC

/* ---------- types ----------*/

struct rf_device;
typedef struct rf_device rf_device_t;
typedef void (*rf_callback_t)(rf_device_t *dev, const uint8_t *data, size_t len);


/*---------- structures ----------*/
// RF回调函数

struct rf_device_ops {
    bool (*init)(rf_device_t *dev, rf_callback_t callback); // 初始化RF设备并设置回调函数
    bool (*deinit)(rf_device_t *dev);
    int (*transmit)(rf_device_t *dev, const uint8_t *data, size_t len);
#ifdef RF_RX_MODE_ASYNC
    bool (*receive_async)(rf_device_t *dev);
#else
    int (*receive)(rf_device_t *dev, uint8_t *buffer, size_t max_len);
#endif
};
typedef struct rf_device_ops rf_device_ops_t;


// RF设备结构体
struct rf_device {
    rf_device_ops_t ops;
    void* priv;
};
