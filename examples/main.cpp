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
 
 #include "common.h"
 
 // External functions from capsule.cpp
 extern void capsule_board_init(void);
 extern bool capsule_board_start(void);
 extern void* capsule_thread(void* arg);
 extern void capsule_board_cleanup(void);
 
 // External functions from transceiver.cpp
 extern void transceiver_board_init(void);
 extern bool transceiver_board_start(void);
 extern void* transceiver_thread(void* arg);
 extern void transceiver_board_cleanup(void);
 
 // Global control flags
 static volatile bool running = true;
 static pthread_t capsule_thread_id;
 static pthread_t transceiver_thread_id;
 
 // Signal handler
 static void signal_handler(int signo) {
     LOG_INFO("Received signal %d, shutting down...", signo);
     running = false;
 }
 
 // Cleanup function
 static void cleanup(void) {
     LOG_INFO("Application cleanup started");
     
     // Wait for threads to terminate
     if (capsule_thread_id) {
         pthread_join(capsule_thread_id, NULL);
     }
     
     if (transceiver_thread_id) {
         pthread_join(transceiver_thread_id, NULL);
     }
     
     // Cleanup components
     capsule_board_cleanup();
     transceiver_board_cleanup();
     
     LOG_INFO("Application cleanup complete");
 }
 
 int main() {
     // Setup signal handling
     signal(SIGINT, signal_handler);
     signal(SIGTERM, signal_handler);
     
     // Register cleanup function
     atexit(cleanup);
     
     LOG_INFO("Starting capsule communication application");
     
     // Initialize components
     transceiver_board_init();
     capsule_board_init();
     
     // Start components
     if (!transceiver_board_start() || !capsule_board_start()) {
         LOG_ERROR("Failed to start one or more components");
         return EXIT_FAILURE;
     }
     
     // Create threads
     if (pthread_create(&transceiver_thread_id, NULL, transceiver_thread, NULL) != 0) {
         LOG_ERROR("Failed to create transceiver thread: %s", strerror(errno));
         return EXIT_FAILURE;
     }
     
     if (pthread_create(&capsule_thread_id, NULL, capsule_thread, NULL) != 0) {
         LOG_ERROR("Failed to create capsule thread: %s", strerror(errno));
         return EXIT_FAILURE;
     }
     
     LOG_INFO("Application started successfully");
     
     // Main thread can now wait for signals
     while (running) {
         sleep(1);
     }
     
     LOG_INFO("Application shutting down...");
     return EXIT_SUCCESS;
 }