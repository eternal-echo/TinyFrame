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
 
 /* Message queue names */
 #define PRESSURE_QUEUE_NAME "/pressure_data_queue"
 #define IMU_QUEUE_NAME "/imu_data_queue"
 #define CONTROL_QUEUE_NAME "/control_cmd_queue"
 
 /* TinyFrame message types */
 #define MSG_TYPE_PRESSURE 0x01
 #define MSG_TYPE_IMU      0x02
 #define MSG_TYPE_CONTROL  0x03
 
 /* Data structures */
 typedef struct {
     double pressure_hpa;  /* Pressure in hPa */
     time_t timestamp;     /* Timestamp in seconds */
 } PressureData;
 
 typedef struct {
     /* Accelerometer data (m/s²) */
     float acc_x;
     float acc_y;
     float acc_z;
     
     /* Gyroscope data (rad/s) */
     float gyro_x;
     float gyro_y;
     float gyro_z;
     
     /* Magnetometer data (μT) */
     float mag_x;
     float mag_y;
     float mag_z;
     
     time_t timestamp;  /* Timestamp in seconds */
 } IMUData;
 
 /* Control commands */
 typedef enum {
     CONTROL_STOP = 0,
     CONTROL_START = 1
 } ControlCommand;
 
 #endif /* COMMON_H */