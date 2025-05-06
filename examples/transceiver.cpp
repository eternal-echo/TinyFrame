/**
 * transceiver.cpp - External transceiver application
 * 
 * Receives sensor data from capsule via TinyFrame and writes to message queues
 * Reads control commands from message queue and sends to capsule
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
 
 // Global variables
 TinyFrame* tf = NULL;
 static volatile bool running = true;
 static mqd_t pressure_queue = (mqd_t)-1;
 static mqd_t imu_queue = (mqd_t)-1;
 static mqd_t control_queue = (mqd_t)-1;
 
 // External function from tf_transport.cpp
 extern void read_and_process_data(void);
 
 // Forward declarations
 static TF_Result handle_pressure_data(TinyFrame* tf, TF_Msg* msg);
 static TF_Result handle_imu_data(TinyFrame* tf, TF_Msg* msg);
 static bool send_control_command(TinyFrame* tf, ControlCommand cmd);
 
 // Handle received pressure data from capsule
 static TF_Result handle_pressure_data(TinyFrame* tf, TF_Msg* msg) {
     if (msg->len != sizeof(PressureData)) {
         LOG_ERROR("Invalid pressure data size: %u", msg->len);
         return TF_STAY;
     }
     
     PressureData data;
     memcpy(&data, msg->data, sizeof(PressureData));
     
     // Print received data for debugging
     LOG_INFO("Received pressure data: %.2f hPa, timestamp: %lu", 
            data.pressure_hpa, (unsigned long)data.timestamp);
     
     // Send to message queue
     if (mq_send(pressure_queue, (const char*)&data, sizeof(PressureData), 0) == -1) {
         LOG_ERROR("Failed to send pressure data to queue: %s", strerror(errno));
     } else {
         LOG_DEBUG("Pressure data sent to queue");
     }
     
     return TF_STAY;
 }
 
 // Handle received IMU data from capsule
 static TF_Result handle_imu_data(TinyFrame* tf, TF_Msg* msg) {
     if (msg->len != sizeof(IMUData)) {
         LOG_ERROR("Invalid IMU data size: %u", msg->len);
         return TF_STAY;
     }
     
     IMUData data;
     memcpy(&data, msg->data, sizeof(IMUData));
     
     // Print received data for debugging
     LOG_DEBUG("Received IMU data: Accel(%.2f, %.2f, %.2f) Gyro(%.2f, %.2f, %.2f) Mag(%.2f, %.2f, %.2f)",
               data.acc_x, data.acc_y, data.acc_z,
               data.gyro_x, data.gyro_y, data.gyro_z,
               data.mag_x, data.mag_y, data.mag_z);
     
     // Send to message queue
     if (mq_send(imu_queue, (const char*)&data, sizeof(IMUData), 0) == -1) {
         LOG_ERROR("Failed to send IMU data to queue: %s", strerror(errno));
     } else {
         LOG_DEBUG("IMU data sent to queue");
     }
     
     return TF_STAY;
 }
 
 // Send control command to capsule
 static bool send_control_command(TinyFrame* tf, ControlCommand cmd) {
     TF_Msg msg;
     TF_ClearMsg(&msg);
     
     msg.type = MSG_TYPE_CONTROL;
     msg.data = (const uint8_t*)&cmd;
     msg.len = sizeof(ControlCommand);
     
     LOG_INFO("Sending control command: %s", 
            (cmd == CONTROL_START) ? "START" : "STOP");
     
     return TF_Send(tf, &msg);
 }
 
 // Initialize transceiver resources
 void transceiver_board_init(void) {
     LOG_INFO("Initializing transceiver board...");
     
     // Open message queues
     struct mq_attr attr;
     attr.mq_flags = 0;
     attr.mq_maxmsg = 10;
     attr.mq_msgsize = sizeof(PressureData);
     
     // Unlink queues first in case they already exist
     mq_unlink(PRESSURE_QUEUE_NAME);
     mq_unlink(IMU_QUEUE_NAME);
     mq_unlink(CONTROL_QUEUE_NAME);
     
     pressure_queue = mq_open(PRESSURE_QUEUE_NAME, O_WRONLY | O_CREAT, 0644, &attr);
     if (pressure_queue == (mqd_t)-1) {
         LOG_ERROR("Failed to open pressure queue: %s", strerror(errno));
         exit(EXIT_FAILURE);
     }
     
     attr.mq_msgsize = sizeof(IMUData);
     imu_queue = mq_open(IMU_QUEUE_NAME, O_WRONLY | O_CREAT, 0644, &attr);
     if (imu_queue == (mqd_t)-1) {
         LOG_ERROR("Failed to open IMU queue: %s", strerror(errno));
         exit(EXIT_FAILURE);
     }
     
     attr.mq_msgsize = sizeof(ControlCommand);
     control_queue = mq_open(CONTROL_QUEUE_NAME, O_RDONLY | O_CREAT, 0644, &attr);
     if (control_queue == (mqd_t)-1) {
         LOG_ERROR("Failed to open control queue: %s", strerror(errno));
         exit(EXIT_FAILURE);
     }
     
     LOG_INFO("Transceiver board initialization complete");
 }
 
 // Start transceiver processing
 bool transceiver_board_start(void) {
     LOG_INFO("Starting transceiver board...");
     
     // Initialize TinyFrame
     tf = TF_Init(TF_MASTER);
     if (tf == NULL) {
         LOG_ERROR("Failed to initialize TinyFrame");
         return false;
     }
     
     // Register message listeners
     TF_AddTypeListener(tf, MSG_TYPE_PRESSURE, handle_pressure_data);
     TF_AddTypeListener(tf, MSG_TYPE_IMU, handle_imu_data);
     
     LOG_INFO("Transceiver board started successfully");
     return true;
 }
 
 // Transceiver thread function
 void* transceiver_thread(void* arg) {
     LOG_INFO("Transceiver thread started");
     
     // Main processing loop
     while (running) {
         // Read control commands from queue
         ControlCommand cmd;
         struct timespec timeout;
         clock_gettime(CLOCK_REALTIME, &timeout);
         timeout.tv_nsec += 100000000; // Add 100ms
         if (timeout.tv_nsec >= 1000000000) {
             timeout.tv_sec++;
             timeout.tv_nsec -= 1000000000;
         }
         
         ssize_t bytes_read = mq_timedreceive(control_queue, (char*)&cmd, 
                                            sizeof(ControlCommand), NULL, &timeout);
         
         if (bytes_read > 0) {
             // Send command to capsule
             if (!send_control_command(tf, cmd)) {
                 LOG_ERROR("Failed to send control command");
             }
         } else if (errno != ETIMEDOUT) {
             // Report error only if it's not a timeout
             LOG_ERROR("Error reading from control queue: %s", strerror(errno));
         }
         
         // Process incoming data from transport layer
         read_and_process_data();
         
         // Tick TinyFrame to handle timeouts
         TF_Tick(tf);
         
         // Sleep a bit to avoid CPU hogging
         usleep(10000); // 10ms
     }
     
     LOG_INFO("Transceiver thread terminated");
     return NULL;
 }
 
 // Cleanup transceiver resources
 void transceiver_board_cleanup(void) {
     // Close message queues
     if (pressure_queue != (mqd_t)-1) {
         mq_close(pressure_queue);
         mq_unlink(PRESSURE_QUEUE_NAME);
     }
     
     if (imu_queue != (mqd_t)-1) {
         mq_close(imu_queue);
         mq_unlink(IMU_QUEUE_NAME);
     }
     
     if (control_queue != (mqd_t)-1) {
         mq_close(control_queue);
         mq_unlink(CONTROL_QUEUE_NAME);
     }
     
     // Free TinyFrame instance
     if (tf != NULL) {
         TF_DeInit(tf);
         tf = NULL;
     }
     
     LOG_INFO("Transceiver board cleanup complete");
 }