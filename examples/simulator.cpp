/**
 * simulator.cpp - Simulates sensor data and provides control interface
 */

 #include <iostream>
 #include <thread>
 #include <chrono>
 #include <random>
 #include <string>
 #include <atomic>
 #include <mqueue.h>
 #include <cstring>
 #include <csignal>
 #include <ctime>
 
 #include "common.h"
 
 // Global control
 static std::atomic<bool> running{true};
 
 // Random number generation
 static std::random_device rd;
 static std::mt19937 gen(rd());
 
 // Signal handler
 void signal_handler(int signo) {
     std::cout << "Simulator received signal " << signo << ", shutting down..." << std::endl;
     running = false;
 }
 
 // Generate random sensor data
 void generate_sensor_data() {
     // Distribution for pressure (900-1100 hPa)
     std::uniform_real_distribution<> pressure_dist(900.0, 1100.0);
     
     // Distribution for IMU (-10 to 10)
     std::uniform_real_distribution<> imu_dist(-10.0, 10.0);
     
     // Open message queues
     mqd_t pressure_queue = mq_open(PRESSURE_QUEUE_NAME, O_WRONLY);
     mqd_t imu_queue = mq_open(IMU_QUEUE_NAME, O_WRONLY);
     
     if (pressure_queue == (mqd_t)-1 || imu_queue == (mqd_t)-1) {
         std::cerr << "Failed to open message queues: " << strerror(errno) << std::endl;
         return;
     }
     
     std::cout << "Sensor data generator started" << std::endl;
     
     // Generate data
     while (running) {
         // Create pressure data
         PressureData pressure;
         pressure.pressure_hpa = pressure_dist(gen);
         pressure.timestamp = time(NULL);
         
         // Send to queue
         if (mq_send(pressure_queue, (const char*)&pressure, sizeof(PressureData), 0) == -1) {
             std::cerr << "Failed to send pressure data: " << strerror(errno) << std::endl;
         } else {
             std::cout << "Generated pressure: " << pressure.pressure_hpa << " hPa" << std::endl;
         }
         
         // Create IMU data
         IMUData imu;
         imu.acc_x = imu_dist(gen);
         imu.acc_y = imu_dist(gen);
         imu.acc_z = imu_dist(gen);
         imu.gyro_x = imu_dist(gen);
         imu.gyro_y = imu_dist(gen);
         imu.gyro_z = imu_dist(gen);
         imu.mag_x = imu_dist(gen);
         imu.mag_y = imu_dist(gen);
         imu.mag_z = imu_dist(gen);
         imu.timestamp = time(NULL);
         
         // Send to queue
         if (mq_send(imu_queue, (const char*)&imu, sizeof(IMUData), 0) == -1) {
             std::cerr << "Failed to send IMU data: " << strerror(errno) << std::endl;
         } else {
             std::cout << "Generated IMU data" << std::endl;
         }
         
         // Wait before next generation
         std::this_thread::sleep_for(std::chrono::milliseconds(500));
     }
     
     // Cleanup
     mq_close(pressure_queue);
     mq_close(imu_queue);
     
     std::cout << "Sensor data generator stopped" << std::endl;
 }
 
 // Control command interface
 void control_interface() {
     // Open control queue
     mqd_t control_queue = mq_open(CONTROL_QUEUE_NAME, O_WRONLY);
     
     if (control_queue == (mqd_t)-1) {
         std::cerr << "Failed to open control queue: " << strerror(errno) << std::endl;
         return;
     }
     
     std::cout << "Control interface started" << std::endl;
     std::cout << "Commands: 's' to start data collection, 'p' to stop, 'q' to quit" << std::endl;
     
     char input;
     while (running) {
         std::cin >> input;
         
         ControlCommand cmd;
         bool send_cmd = true;
         
         switch (input) {
             case 's':
             case 'S':
                 cmd = CONTROL_START;
                 std::cout << "Sending START command..." << std::endl;
                 break;
                 
             case 'p':
             case 'P':
                 cmd = CONTROL_STOP;
                 std::cout << "Sending STOP command..." << std::endl;
                 break;
                 
             case 'q':
             case 'Q':
                 running = false;
                 send_cmd = false;
                 break;
                 
             default:
                 std::cout << "Unknown command" << std::endl;
                 send_cmd = false;
                 break;
         }
         
         if (send_cmd) {
             if (mq_send(control_queue, (const char*)&cmd, sizeof(ControlCommand), 0) == -1) {
                 std::cerr << "Failed to send control command: " << strerror(errno) << std::endl;
             }
         }
     }
     
     // Cleanup
     mq_close(control_queue);
     
     std::cout << "Control interface stopped" << std::endl;
 }
 
 int main() {
     // Setup signal handling
     signal(SIGINT, signal_handler);
     signal(SIGTERM, signal_handler);
     
     std::cout << "Starting simulator..." << std::endl;
     
     // Start threads
     std::thread sensor_thread(generate_sensor_data);
     std::thread control_thread(control_interface);
     
     // Wait for threads to finish
     if (sensor_thread.joinable()) {
         sensor_thread.join();
     }
     
     if (control_thread.joinable()) {
         control_thread.join();
     }
     
     std::cout << "Simulator shutdown complete" << std::endl;
     return 0;
 }