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
 
 #include "../TinyFrame.h"
 #include "common.h"

 
// 接收模式：
#define RF_RX_MODE_ASYNC
 
 // Global variables
 TinyFrame* tf = NULL;
 static volatile bool running = true;
 static volatile bool data_collection_active = false;
 static mqd_t pressure_queue = (mqd_t)-1;
 static mqd_t imu_queue = (mqd_t)-1;
 static mqd_t control_queue = (mqd_t)-1;
 
 // External function from tf_transport.cpp
 extern void read_and_process_data(void);
 
 // Forward declarations
 static TF_Result handle_control_command(TinyFrame* tf, TF_Msg* msg);
 static bool send_pressure_data(TinyFrame* tf, PressureData* data);
 static bool send_imu_data(TinyFrame* tf, IMUData* data);
 
 // Handle control command from transceiver
 static TF_Result handle_control_command(TinyFrame* tf, TF_Msg* msg) {
     if (msg->len != sizeof(ControlCommand)) {
         LOG_ERROR("Invalid control command size: %u", msg->len);
         return TF_STAY;
     }
     
     ControlCommand cmd;
     memcpy(&cmd, msg->data, sizeof(ControlCommand));
     
     switch (cmd) {
         case CONTROL_START:
             LOG_INFO("Received START command");
             data_collection_active = true;
             break;
             
         case CONTROL_STOP:
             LOG_INFO("Received STOP command");
             data_collection_active = false;
             break;
             
         default:
             LOG_ERROR("Unknown control command: %d", cmd);
             break;
     }
     
     return TF_STAY;
 }
 
 // Send pressure data
 static bool send_pressure_data(TinyFrame* tf, PressureData* data) {
     TF_Msg msg;
     TF_ClearMsg(&msg);
     
     msg.type = MSG_TYPE_PRESSURE;
     msg.data = (const uint8_t*)data;
     msg.len = sizeof(PressureData);
     
     LOG_DEBUG("Sending pressure data: %.2f hPa", data->pressure_hpa);
     
     return TF_Send(tf, &msg);
 }
 
 // Send IMU data
 static bool send_imu_data(TinyFrame* tf, IMUData* data) {
     TF_Msg msg;
     TF_ClearMsg(&msg);
     
     msg.type = MSG_TYPE_IMU;
     msg.data = (const uint8_t*)data;
     msg.len = sizeof(IMUData);
     
     LOG_DEBUG("Sending IMU data (acc_x: %.2f, gyro_x: %.2f, mag_x: %.2f)", 
              data->acc_x, data->gyro_x, data->mag_x);
     
     return TF_Send(tf, &msg);
 }
 
 // Initialize capsule resources
 void capsule_board_init(void) {
     LOG_INFO("Initializing capsule board...");
     
     // Open message queues
     struct mq_attr attr;
     attr.mq_flags = 0;
     attr.mq_maxmsg = 10;
     
     attr.mq_msgsize = sizeof(PressureData);
     pressure_queue = mq_open(PRESSURE_QUEUE_NAME, O_RDONLY | O_NONBLOCK, 0644, &attr);
     if (pressure_queue == (mqd_t)-1) {
         LOG_ERROR("Failed to open pressure queue: %s", strerror(errno));
         exit(EXIT_FAILURE);
     }
     
     attr.mq_msgsize = sizeof(IMUData);
     imu_queue = mq_open(IMU_QUEUE_NAME, O_RDONLY | O_NONBLOCK, 0644, &attr);
     if (imu_queue == (mqd_t)-1) {
         LOG_ERROR("Failed to open IMU queue: %s", strerror(errno));
         exit(EXIT_FAILURE);
     }
     
     attr.mq_msgsize = sizeof(ControlCommand);
     control_queue = mq_open(CONTROL_QUEUE_NAME, O_WRONLY, 0644, &attr);
     if (control_queue == (mqd_t)-1) {
         LOG_ERROR("Failed to open control queue: %s", strerror(errno));
         exit(EXIT_FAILURE);
     }
     
     LOG_INFO("Capsule board initialization complete");
 }
 
 // Start capsule processing
 bool capsule_board_start(void) {
     LOG_INFO("Starting capsule board...");
     
     // Initialize TinyFrame
     tf = TF_Init(TF_SLAVE);
     if (tf == NULL) {
         LOG_ERROR("Failed to initialize TinyFrame");
         return false;
     }
     
     // Register message listeners
     TF_AddTypeListener(tf, MSG_TYPE_CONTROL, handle_control_command);
     
     LOG_INFO("Capsule board started successfully");
     return true;
 }
 
 // Capsule thread function
 void* capsule_thread(void* arg) {
     LOG_INFO("Capsule thread started");
     
     // Main processing loop
     while (running) {
         // Process incoming control commands from transport layer
         read_and_process_data();
         
         // Only read and send sensor data if collection is active
         if (data_collection_active) {
             // Try to read pressure data
             PressureData pressure_data;
             ssize_t bytes_read = mq_receive(pressure_queue, (char*)&pressure_data, 
                                            sizeof(PressureData), NULL);
             
             if (bytes_read > 0) {
                 if (!send_pressure_data(tf, &pressure_data)) {
                     LOG_ERROR("Failed to send pressure data");
                 }
             } else if (errno != EAGAIN) {
                 // Report error only if it's not "no message available"
                 LOG_ERROR("Error reading from pressure queue: %s", strerror(errno));
             }
             
             // Try to read IMU data
             IMUData imu_data;
             bytes_read = mq_receive(imu_queue, (char*)&imu_data, sizeof(IMUData), NULL);
             
             if (bytes_read > 0) {
                 if (!send_imu_data(tf, &imu_data)) {
                     LOG_ERROR("Failed to send IMU data");
                 }
             } else if (errno != EAGAIN) {
                 // Report error only if it's not "no message available"
                 LOG_ERROR("Error reading from IMU queue: %s", strerror(errno));
             }
         }
         
         // Tick TinyFrame to handle timeouts
         TF_Tick(tf);
         
         // Sleep a bit to avoid CPU hogging
         usleep(10000); // 10ms
     }
     
     LOG_INFO("Capsule thread terminated");
     return NULL;
 }
 
 // Cleanup capsule resources
 void capsule_board_cleanup(void) {
     // Close message queues
     if (pressure_queue != (mqd_t)-1) {
         mq_close(pressure_queue);
     }
     
     if (imu_queue != (mqd_t)-1) {
         mq_close(imu_queue);
     }
     
     if (control_queue != (mqd_t)-1) {
         mq_close(control_queue);
     }
     
     // Free TinyFrame instance
     if (tf != NULL) {
         TF_DeInit(tf);
         tf = NULL;
     }
     
     LOG_INFO("Capsule board cleanup complete");
 }