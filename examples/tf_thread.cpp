#include <stdio.h>
#include <stdarg.h>
/* POSIX Header files */
#include <pthread.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>

#include "rf_device.h"
#include "common.h"

// 接收模式：
// #define RF_RX_MODE_ASYNC


// 配置参数
#define TF_THREADSTACKSIZE      1024
#define TF_PROCESS_INTERVAL     3     // 处理间隔(ms)

/*---------- global variables ----------*/
rf_device_t *rf_dev_handle = NULL;  // RF设备句柄
TinyFrame *tf_ctx = NULL;    // TinyFrame上下文
pthread_mutex_t tf_mutex;  // TinyFrame互斥锁

#ifdef RF_RX_MODE_ASYNC
/**
 * @brief RF接收回调函数
 * 
 * 当RF设备接收到数据时调用此函数，将数据传递给TinyFrame处理
 */
static void rf_rx_callback(rf_device_t *dev, const uint8_t *data, size_t len) {
    if (data == NULL || len == 0 || tf_ctx == NULL) return;
    
    LOG_INFO("Processing received data, len=%zu", len);
    
    // 交由TinyFrame处理
    TF_Accept(tf_ctx, data, len);
}
#endif // RF_RX_MODE_ASYNC

/**
 * @brief TinyFrame默认帧监听器
 */
static TF_Result default_listener(TinyFrame *tf, TF_Msg *msg) {
    if (msg == NULL || msg->data == NULL || msg->len == 0) return TF_NEXT;
    
    // 处理接收到的数据
    LOG_INFO("Received data, id=%u, type=%u, len=%u", msg->frame_id, msg->type, (unsigned int)msg->len);
    
    switch (msg->type) {
        case TF_TYPE_SENSOR_IMU: {
            if (msg->len == sizeof(tf_data_t)) {
                tf_data_t *data = (tf_data_t *)msg->data;
                LOG_INFO("Sensor data received, timestamp: %u", data->timestamp);
                // 可以根据需要处理不同类型的传感器数据
            }
            break;
        }
        
        case TF_TYPE_CMD: {
            if (msg->len == sizeof(tf_data_t)) {
                tf_data_t *data = (tf_data_t *)msg->data;
                LOG_INFO("Command received: %d, timestamp: %u", 
                        data->data.cmd.command, data->timestamp);
            }
            break;
        }
        
        default:
            LOG_ERROR("Unknown message type: %d", msg->type);
            break;
    }
    
    return TF_NEXT;
}

#if (BOARD_ID == BOARD_SERVER_ID)
// imu_listener
static TF_Result imu_listener(TinyFrame *tf, TF_Msg *msg) {
    if (msg == NULL || msg->data == NULL || msg->len == 0) return TF_STAY;
    
    // 处理接收到的IMU数据
    LOG_INFO("IMU data received, id=%u, type=%u, len=%u", msg->frame_id, msg->type, (unsigned int)msg->len);
    
    if (msg->len == sizeof(tf_data_t)) {
        tf_data_t *data = (tf_data_t *)msg->data;
        LOG_INFO("IMU data received, timestamp: %u", data->timestamp);
        // 可以根据需要处理IMU数据
        LOG_INFO("IMU data: accel=[%f, %f, %f], gyro=[%f, %f, %f], mag=[%f, %f, %f]",
                data->data.imu_data.accel[0], data->data.imu_data.accel[1], data->data.imu_data.accel[2],
                data->data.imu_data.gyro[0], data->data.imu_data.gyro[1], data->data.imu_data.gyro[2],
                data->data.imu_data.mag[0], data->data.imu_data.mag[1], data->data.imu_data.mag[2]);
    } else {
        LOG_ERROR("IMU data length mismatch: expected %zu, got %u", sizeof(tf_data_t), msg->len);
    }
    
    return TF_STAY;
}

// pressure_listener
static TF_Result pressure_listener(TinyFrame *tf, TF_Msg *msg) {
    if (msg == NULL || msg->data == NULL || msg->len == 0) return TF_STAY;
    
    // 处理接收到的压力数据
    LOG_INFO("Pressure data received, id=%u, type=%u, len=%u", msg->frame_id, msg->type, (unsigned int)msg->len);
    
    if (msg->len == sizeof(tf_data_t)) {
        tf_data_t *data = (tf_data_t *)msg->data;
        LOG_INFO("Pressure data received, timestamp: %u", data->timestamp);
        // 可以根据需要处理压力数据
        LOG_INFO("Pressure data: pressure_hpa=%f", data->data.pressure_data.pressure_hpa);
    } else {
        LOG_ERROR("Pressure data length mismatch: expected %zu, got %u", sizeof(tf_data_t), msg->len);
    }
    
    return TF_STAY;
}

#else // CLIENT_ID
static TF_Result cmd_listener(TinyFrame *tf, TF_Msg *msg) {
    if (msg == NULL || msg->data == NULL || msg->len == 0) return TF_STAY;
    
    // 处理接收到的控制命令
    LOG_INFO("Command data received, id=%u, type=%u, len=%u", msg->frame_id, msg->type, (unsigned int)msg->len);
    
    if (msg->len == sizeof(tf_data_t)) {
        tf_data_t *data = (tf_data_t *)msg->data;
        LOG_INFO("Command data received, timestamp: %u", data->timestamp);
        // 可以根据需要处理控制命令
        LOG_INFO("Command data: command=%d", data->data.cmd.command);
    } else {
        LOG_ERROR("Command data length mismatch: expected %zu, got %u", sizeof(tf_data_t), msg->len);
    }
    
    return TF_STAY;
}
#endif // BOARD_ID

/**
 * @brief 初始化TinyFrame
 */
static bool tf_init() {
#if (BOARD_ID == BOARD_SERVER_ID)
    // 创建TinyFrame实例
    tf_ctx = TF_Init(TF_MASTER);
#else
    // 创建TinyFrame实例
    tf_ctx = TF_Init(TF_SLAVE);
#endif
    // 检查TinyFrame实例是否创建成功
    if (tf_ctx == NULL) {
        LOG_ERROR("TF_Init failed");
        return false;
    }
    
    // 添加默认监听器
    TF_AddGenericListener(tf_ctx, default_listener);
#if (BOARD_ID == BOARD_SERVER_ID)
    // 添加IMU和压力数据监听器
    TF_AddTypeListener(tf_ctx, TF_TYPE_SENSOR_IMU, imu_listener);
    TF_AddTypeListener(tf_ctx, TF_TYPE_SENSOR_PRESSURE, pressure_listener);
#else
    // 添加控制命令监听器
    TF_AddTypeListener(tf_ctx, TF_TYPE_CMD, cmd_listener);
#endif // BOARD_ID
    
    return true;
}

/**
 * @brief TinyFrame线程主函数
 */
void *tf_thread(void *arg) {
    
    // 初始化RF设备
#ifdef RF_RX_MODE_ASYNC    
    if (!rf_dev_handle->ops.init(rf_dev_handle, rf_rx_callback)) {
        LOG_ERROR("RF device init failed");
        return NULL;
    }
#else
    if (!rf_dev_handle->ops.init(rf_dev_handle, NULL)) {
        LOG_ERROR("RF device init failed");
        return NULL;
    }
#endif // RF_RX_MODE_ASYNC
    
    // 初始化TinyFrame
    if (!tf_init()) {
        LOG_ERROR("TinyFrame init failed");
        return NULL;
    }
    
    // 主循环
    while (1) {
#ifdef RF_RX_MODE_ASYNC
        // 启动异步接收
        if (!rf_dev_handle->ops.receive_async(rf_dev_handle)) {
            LOG_ERROR("Failed to start async receive");
            break;
        }
#else
        // 启动同步接收
        static uint8_t buffer[TF_MAX_PAYLOAD_RX];
        size_t len = rf_dev_handle->ops.receive(rf_dev_handle, buffer, TF_MAX_PAYLOAD_RX);
        if (len > 0) {
            // 交由TinyFrame处理
            TF_Accept(tf_ctx, buffer, len);
        }
#endif // RF_RX_MODE_ASYNC
        
        // 定期调用TF_Tick以支持超时功能
        TF_Tick(tf_ctx);
        
        // 休眠
        usleep(TF_PROCESS_INTERVAL * 1000);
    }
    
    return NULL;
}

void tf_board_init(void *arg) {
    // 获取RF设备
    rf_dev_handle = (rf_device_t *)arg;
    if (rf_dev_handle == NULL) {
        LOG_ERROR("Failed to get RF device");
        return;
    }
}

/**
 * @brief 启动TF线程
 */
void tf_thread_start() {
    // 初始化互斥锁
    pthread_mutex_init(&tf_mutex, NULL);

    // 创建线程
    pthread_t thread;
    pthread_attr_t attrs;
    struct sched_param priParam;
    
    // 设置线程属性
    pthread_attr_init(&attrs);
    priParam.sched_priority = 1;
    pthread_attr_setschedparam(&attrs, &priParam);
    pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attrs, TF_THREADSTACKSIZE);
    
    // 创建线程
    if (pthread_create(&thread, &attrs, tf_thread, NULL) != 0) {
        LOG_ERROR("Failed to create TF thread");
    } else {
        LOG_INFO("TF thread started successfully");
    }
}

/**
 * @brief 清理TF线程资源
 */
void tf_board_cleanup() {
    
    // 销毁互斥锁
    pthread_mutex_destroy(&tf_mutex);
}



/* 【TF 依赖】 */

extern "C" 
{
    // --------- Mutex callbacks ----------
    // TF_USE_MUTEX=1 时需要的互斥锁实现
    
    /** Claim the TX interface before composing and sending a frame */
    bool TF_ClaimTx(TinyFrame *tf)
    {
        int result = pthread_mutex_lock(&tf_mutex);
        if (result != 0) {
            LOG_ERROR("Failed to lock mutex: %s", strerror(result));
            return false;
        }
        return true; // 成功获取锁
    }
    
    /** Free the TX interface after composing and sending a frame */
    void TF_ReleaseTx(TinyFrame *tf)
    {
        int result = pthread_mutex_unlock(&tf_mutex);
        if (result != 0) {
            LOG_ERROR("Failed to unlock mutex: %s", strerror(result));
        }
    }
    
    // --------- TinyFrame底层写入实现 ---------
    /** 
     * 将数据从TinyFrame写入到RF设备
     * 注意：这个函数必须实现，由TinyFrame内部调用
     */
    void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
    {
        if (rf_dev_handle == NULL || rf_dev_handle->ops.transmit == NULL) {
            LOG_ERROR("Cannot write to RF device: device not initialized");
            return;
        }
        
        int sent = rf_dev_handle->ops.transmit(rf_dev_handle, buff, len);
        if (sent <= 0) {
            LOG_ERROR("Failed to transmit data via RF device");
        } else {
            LOG_DEBUG("TF transmitted %d bytes via RF device", sent);
        }
    }
    
    // 如果使用自定义校验和，保留这些函数；如果使用内置校验和类型，可以删除
    /** Initialize a checksum */
    TF_CKSUM TF_CksumStart(void)
    {
        return 0;
    }
    
    /** Update a checksum with a byte */
    TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte)
    {
        return cksum ^ byte;
    }
    
    /** Finalize the checksum calculation */
    TF_CKSUM TF_CksumEnd(TF_CKSUM cksum)
    {
        return (TF_CKSUM) ~cksum; // 取反，符合XOR校验和规则
    }
}