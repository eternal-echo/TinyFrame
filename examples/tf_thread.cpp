/**
 * capsule.cpp - Electronic capsule application
 * 
 * Reads sensor data from message queues and sends via TinyFrame
 * Receives control commands from transceiver
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <mqueue.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#include "rf_device.h"
#include "common.h"

#define TF_THREADSTACKSIZE (4*1024) 

/*---------- global variables ----------*/
static rf_device_t *rf_dev_handle = NULL;

static mqd_t rf_tx_queue = (mqd_t)-1, rf_rx_queue = (mqd_t)-1;

static TinyFrame *tf_ctx;


/* 【TF】*/
static TF_Result tf_default_listener(TinyFrame *tf, TF_Msg *msg) {
    LOG_INFO("Default listener: Received response with ID %u", msg->frame_id);
    return TF_STAY;
}

#if (BOARD_ID == BOARD_CAPSULE_ID) // Capsule board
// Handle control command from transceiver
static TF_Result tf_control_command_listener(TinyFrame* tf, TF_Msg* msg) {
    control_command_t control_cmd = {0};
    if (msg->len != sizeof(control_command_t)) {
        LOG_ERROR("Invalid control command size: %u", msg->len);
        return TF_STAY;
    }
    memcpy(&control_cmd, msg->data, sizeof(control_command_t));
    LOG_INFO("Received control command: %u", control_cmd.command);

    switch (control_cmd.command) {
        case TF_CMD_START:
            LOG_INFO("Control command: START");
            break;
        case TF_CMD_STOP:
            LOG_INFO("Control command: STOP");
            break;
        default:
            LOG_ERROR("Unknown control command: %u", control_cmd.command);
            break;
    }

    // 发送到消息队列
    if (mq_send(rf_rx_queue, (const char*)&control_cmd, sizeof(control_command_t), 0) == -1) {
        LOG_ERROR("Failed to send control command to queue: %s", strerror(errno));
    } else {
        LOG_DEBUG("Control command sent to queue");
    }
    return TF_STAY;
}
#else // Transceiver board
/** 
 * @brief 接收「胶囊」发送到「收发器」的压力数据
 * @param dev RF设备句柄
 * @param data 压力数据
 * @return 成功返回true，失败返回false
 */
static TF_Result tf_pressure_data_listener(TinyFrame* tf, TF_Msg* msg) {
    tf_message_t pressure_msg = {
        .id = msg->frame_id,
        .type = msg.type,
        .len = msg.len,
    };
    memcpy(&pressure_msg.data, msg->data, sizeof(pressure_msg.data));
    LOG_INFO("Received pressure data: %f", msg->data.pressure_data.pressure_hpa);
    // 发送到消息队列
    if (mq_send(rf_tx_queue, (const char*)&pressure_msg, sizeof(tf_message_t), 0) == -1) {
        LOG_ERROR("Failed to send pressure data to queue: %s", strerror(errno));
    } else {
        LOG_DEBUG("Pressure data sent to queue");
    }
    return TF_STAY;
}

/**
 * @brief 接收「胶囊」发送到「收发器」的IMU数据
 * @param dev RF设备句柄
 * @param data IMU数据
 * @return 成功返回true，失败返回false
 */
static TF_Result tf_imu_data_listener(TinyFrame* tf, TF_Msg* msg) {
    if (msg->len != sizeof(tf_message_t)) {
        LOG_ERROR("Invalid IMU data size: %u", msg->len);
        return TF_STAY;
    }

    LOG_INFO("Received IMU data: Accel(%.2f, %.2f, %.2f) Gyro(%.2f, %.2f, %.2f) Mag(%.2f, %.2f, %.2f)",
             msg->data.imu_data.accel[0], msg->data.imu_data.accel[1], msg->data.imu_data.accel[2],
             msg->data.imu_data.gyro[0], msg->data.imu_data.gyro[1], msg->data.imu_data.gyro[2],
             msg->data.imu_data.mag[0], msg->data.imu_data.mag[1], msg->data.imu_data.mag[2]);
    // 发送到消息队列
    if (mq_send(rf_tx_queue, (const char*)&msg, sizeof(tf_message_t), 0) == -1) {
        LOG_ERROR("Failed to send IMU data to queue: %s", strerror(errno));
    } else {
        LOG_DEBUG("IMU data sent to queue");
    }
    return TF_STAY;
}

/***
 * @brief 发送控制命令到胶囊
 * @param tf TinyFrame实例
 * @param cmd 控制命令
 * @return 成功返回true，失败返回false
 */
static bool tf_send_control_command(TinyFrame* tf, ControlCommand cmd) {
    TF_Msg msg;
    TF_ClearMsg(&msg);
    
    msg.type = MSG_TYPE_CONTROL;
    msg.data = (const uint8_t*)&cmd;
    msg.len = sizeof(ControlCommand);
    
    LOG_INFO("Sending control command: %s", 
             (cmd == CONTROL_START) ? "START" : "STOP");
    
    return TF_Send(tf, &msg);
}
#endif // BOARD_ID == BOARD_CAPSULE_ID


static bool tf_init() {
    // 初始化TinyFrame
#if (BOARD_ID == BOARD_CAPSULE_ID)
    tf_ctx = TF_Init(TF_SLAVE);
#else
    tf_ctx = TF_Init(TF_MASTER);
#endif
    if (tf_ctx == NULL) {
        LOG_ERROR("Failed to initialize TinyFrame");
        return false;
    }
    
    // 添加监听器
    TF_AddGenericListener(tf_ctx, tf_default_listener);
#ifdef BOARD_CAPSULE_ID
    TF_AddTypeListener(tf_ctx, TF_TYPE_CMD, tf_control_command_listener);
#else
    TF_AddTypeListener(tf_ctx, TF_TYPE_SENSOR, tf_pressure_data_listener);
    TF_AddTypeListener(tf_ctx, TF_TYPE_SENSOR, tf_imu_data_listener);
#endif // BOARD_ID == BOARD_CAPSULE_ID

    return true;
}

/* 【RF】 */
#ifdef RF_RX_MODE_ASYNC
static void _rf_callback(rf_device_t *dev, const uint8_t *data, size_t len) {
    // 处理接收到的数据
    if (data != NULL && len > 0) {
        LOG_INFO("RF Callback: Received data of length %zu", len);
        // 传入TinyFrame进行处理
        TF_Accept(tf_ctx, data, len);
    } else {
        LOG_ERROR("RF Callback: Received invalid data");
    }
}
#endif // RF_RX_MODE_ASYNC
/**
 * @brief RF设备初始化：设置全局变量rf_dev_handle，初始化RF设备，设置接收回调函数
 * @param dev RF设备句柄
 * @param callback 接收回调函数
 * @return 成功返回true，失败返回false
 */
static bool rf_init(rf_device_t *dev, rf_callback_t callback) {
    // 初始化RF设备，设置接收回调函数
    if (dev->ops.init != NULL) {
        if (!dev->ops.init(dev, callback)) {
            LOG_ERROR("Failed to initialize RF device");
            return false;
        }
    }

    // 设置handle
    rf_dev_handle = dev;
    LOG_INFO("RF device initialized successfully");

    return true;
}

/**
 * @brief RF设备反初始化：关闭RF设备
 * @param dev RF设备句柄
 * @return 成功返回true，失败返回false
 */
static bool rf_deinit(rf_device_t *dev) {
    // 关闭RF设备
    if (dev->ops.deinit != NULL) {
        if (!dev->ops.deinit(dev)) {
            LOG_ERROR("Failed to deinitialize RF device");
            return false;
        }
    }

    // 清除handle
    rf_dev_handle = NULL;
    LOG_INFO("RF device deinitialized successfully");
    return true;
}

/**
 * @brief 接收数据
 * @param dev RF设备句柄
 * @return 成功返回true，失败返回false
 */
static bool rf_receive(rf_device_t *dev) {
    TF_Msg tf_msg;
    // 接收数据
#ifdef RF_RX_MODE_ASYNC
    if (dev->ops.receive_async != NULL) {
        if (!dev->ops.receive_async(dev)) {
            LOG_ERROR("Failed to start async receive");
            return false;
        }
    } else {
        LOG_ERROR("RF device receive_async function not implemented");
        return false;
    }
#else    
    if (dev->ops.receive != NULL) {
        if (!dev->ops.receive(dev, (uint8_t*)&tf_msg, sizeof(tf_msg))) {
            LOG_ERROR("Failed to receive data via RF device");
            return false;
        } else {
            LOG_INFO("Data received successfully via RF device");
            // 将数据传递给TinyFrame进行处理
            TF_Accept(tf_ctx, tf_msg.data, tf_msg.len);
        }
    } else {
        LOG_ERROR("RF device receive function not implemented");
        return false;
    }
#endif // RF_RX_MODE_ASYNC
    
    return true;
}

/* -------------- public functions -------------- */
/**
 * @brief 初始化TinyFrame和消息队列
 * @return 无
 */
void tf_board_init(void) {
    /* 设置rf设备 */
    // TODO: 初始化RF设备接口（用boost的mqueue模拟进程间通信）
    LOG_INFO("Initializing capsule board...");
    
    /* 设置消息队列 */
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 5;
    attr.mq_msgsize = sizeof(tf_message_t);

    // 创建tf,->rf之间的消息队列
    rf_tx_queue = mq_open(TF_TX_QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
    rf_rx_queue = mq_open(TF_RX_QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
    
    if (rf_tx_queue == (mqd_t)-1 || rf_rx_queue == (mqd_t)-1) {
        LOG_ERROR("Error: Failed to create message queues");
    }
    LOG_INFO("Capsule board initialization complete");
}

void* tf_thread(void* arg) {
    rf_device_t *rf_dev = (rf_device_t *)arg;
    rf_dev_handle = rf_dev;

    LOG_INFO("tinyframe thread started");

    // 初始化TinyFrame
    tf_init();
    // 初始化rf设备
#ifdef RF_RX_MODE_ASYNC
    rf_init(rf_dev_handle, _rf_callback);
#else
    rf_init(rf_dev_handle, NULL);
#endif // RF_RX_MODE_ASYNC

    
    while (true) {
        tf_message_t tx_msg;
        /* 接收数据 */
        rf_receive(rf_dev_handle);

        /* 发送数据 */
        // 读取rf_tx_queue中的数据
        ssize_t bytes_read = mq_receive(rf_tx_queue, (char*)&tx_msg, sizeof(tx_msg), NULL);
        // 如果读取成功，通过TF_Send()发送数据
        if (bytes_read > 0) {
            TF_Msg tf_msg;
            tf_msg.type = tx_msg.type;
            tf_msg.data = (const uint8_t*)&tx_msg.data;
            tf_msg.len = tx_msg.len;
            if (!TF_Send(tf_ctx, &tf_msg)) {
                LOG_ERROR("Failed to send data via TinyFrame");
            } else {
                LOG_INFO("Data sent successfully via TinyFrame");
            }
        } else if (bytes_read == -1) {
            LOG_ERROR("Failed to read from rf_tx_queue: %s", strerror(errno));
        }

        
        // Tick TinyFrame to handle timeouts
        TF_Tick(tf_ctx);
        
        // Sleep a bit to avoid CPU hogging
        usleep(10000); // 10ms
    }
    
    LOG_INFO("Capsule thread terminated");
    return NULL;
}

void tf_board_cleanup(void) {
    // 关闭消息队列
    if (rf_tx_queue != (mqd_t)-1) {
        mq_close(rf_tx_queue);
        mq_unlink(TF_TX_QUEUE_NAME);
    }
    if (rf_rx_queue != (mqd_t)-1) {
        mq_close(rf_rx_queue);
        mq_unlink(TF_RX_QUEUE_NAME);
    }

    // 反初始化RF设备
    rf_deinit(rf_dev_handle);

    // 反初始化TinyFrame
    if (tf_ctx != NULL) {
        TF_DeInit(tf_ctx);
        tf_ctx = NULL;
    }

    LOG_INFO("Capsule board cleanup complete");
}

/***
 * @brief 启动TinyFrame线程
 * @return 无
 */
void tf_thread_start(void *arg) {
    pthread_t tf_tid;
    pthread_attr_t attrs;
    struct sched_param pri_param;
    int retc;

    // 初始化线程属性
    pthread_attr_init(&attrs);
    pri_param.sched_priority = 1; // 优先级设置为1
    retc = pthread_attr_setschedparam(&attrs, &pri_param);
    retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    retc |= pthread_attr_setstacksize(&attrs, TF_THREADSTACKSIZE);
    if (retc != 0) {
        LOG_ERROR("Failed to set thread attributes: %s", strerror(retc));
    }

    // 创建线程
    retc = pthread_create(&tf_tid, &attrs, tf_thread, arg);
    if (retc != 0) {
        LOG_ERROR("Failed to create thread: %s", strerror(retc));
    }
    pthread_attr_destroy(&attrs); // 销毁线程属性对象

    LOG_INFO("Thread created successfully");
}