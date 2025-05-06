/**
 * tf_transport.cpp - POSIX pipe implementation for TinyFrame transport
 */

 #include <iostream>
 #include <vector>
 #include <thread>
 #include <mutex>
 #include <unistd.h>
 #include <fcntl.h>
 #include <sys/stat.h>
 #include <string>
 #include <cstring>
 #include <cerrno>
 #include <atomic>
 
 #include "../TinyFrame.h"
 #include "common.h"
 
 // Pipe names
 const char* CAPSULE_TO_TRANSCEIVER = "/tmp/capsule_to_transceiver";
 const char* TRANSCEIVER_TO_CAPSULE = "/tmp/transceiver_to_capsule";
 
 // File descriptors
 static int capsule_read_fd = -1;
 static int capsule_write_fd = -1;
 static int transceiver_read_fd = -1;
 static int transceiver_write_fd = -1;
 
 // Thread control
 static std::atomic<bool> read_thread_running{false};
 static std::thread read_thread;
 static std::mutex write_mutex;
 
 // Buffer for read operations
 static std::vector<uint8_t> read_buffer(256);
 
 // Initialize transport for the capsule side
 bool init_transport_capsule() {
     // Create named pipes if they don't exist
     if (mkfifo(CAPSULE_TO_TRANSCEIVER, 0666) == -1 && errno != EEXIST) {
         LOG_ERROR("Failed to create capsule to transceiver pipe: %s", strerror(errno));
         return false;
     }
     
     if (mkfifo(TRANSCEIVER_TO_CAPSULE, 0666) == -1 && errno != EEXIST) {
         LOG_ERROR("Failed to create transceiver to capsule pipe: %s", strerror(errno));
         return false;
     }
     
     // Open pipes
     capsule_write_fd = open(CAPSULE_TO_TRANSCEIVER, O_WRONLY);
     if (capsule_write_fd == -1) {
         LOG_ERROR("Failed to open capsule write pipe: %s", strerror(errno));
         return false;
     }
     
     capsule_read_fd = open(TRANSCEIVER_TO_CAPSULE, O_RDONLY | O_NONBLOCK);
     if (capsule_read_fd == -1) {
         LOG_ERROR("Failed to open capsule read pipe: %s", strerror(errno));
         return false;
     }
     
     LOG_INFO("Capsule transport initialized");
     return true;
 }
 
 // Initialize transport for the transceiver side
 bool init_transport_transceiver() {
     // Create named pipes if they don't exist
     if (mkfifo(CAPSULE_TO_TRANSCEIVER, 0666) == -1 && errno != EEXIST) {
         LOG_ERROR("Failed to create capsule to transceiver pipe: %s", strerror(errno));
         return false;
     }
     
     if (mkfifo(TRANSCEIVER_TO_CAPSULE, 0666) == -1 && errno != EEXIST) {
         LOG_ERROR("Failed to create transceiver to capsule pipe: %s", strerror(errno));
         return false;
     }
     
     // Open pipes
     transceiver_read_fd = open(CAPSULE_TO_TRANSCEIVER, O_RDONLY | O_NONBLOCK);
     if (transceiver_read_fd == -1) {
         LOG_ERROR("Failed to open transceiver read pipe: %s", strerror(errno));
         return false;
     }
     
     transceiver_write_fd = open(TRANSCEIVER_TO_CAPSULE, O_WRONLY);
     if (transceiver_write_fd == -1) {
         LOG_ERROR("Failed to open transceiver write pipe: %s", strerror(errno));
         return false;
     }
     
     LOG_INFO("Transceiver transport initialized");
     return true;
 }
 
 // Clean up transport resources
 void cleanup_transport() {
     // Stop read thread if running
     if (read_thread_running) {
         read_thread_running = false;
         if (read_thread.joinable()) {
             read_thread.join();
         }
     }
     
     // Close file descriptors
     if (capsule_read_fd != -1) close(capsule_read_fd);
     if (capsule_write_fd != -1) close(capsule_write_fd);
     if (transceiver_read_fd != -1) close(transceiver_read_fd);
     if (transceiver_write_fd != -1) close(transceiver_write_fd);
     
     // Remove named pipes
     unlink(CAPSULE_TO_TRANSCEIVER);
     unlink(TRANSCEIVER_TO_CAPSULE);
     
     LOG_INFO("Transport resources cleaned up");
 }
 
 // Process received data - called from main loops
 void read_and_process_data() {
     // Check if we're in capsule or transceiver mode
     int read_fd = (capsule_read_fd != -1) ? capsule_read_fd : transceiver_read_fd;
     
     if (read_fd == -1) {
         return;
     }
     
     // Try to read data
     ssize_t bytes_read = read(read_fd, read_buffer.data(), read_buffer.size());
     
     if (bytes_read > 0) {
         LOG_DEBUG("Received %zd bytes of data", bytes_read);
         // Process data with TinyFrame
         TF_Accept(tf, read_buffer.data(), bytes_read);
     } else if (bytes_read == -1 && errno != EAGAIN) {
         LOG_ERROR("Error reading from pipe: %s", strerror(errno));
     }
 }
 
 // TinyFrame implementation for writing data
 void TF_WriteImpl(TinyFrame* tf, const uint8_t* buff, uint32_t len) {
     std::lock_guard<std::mutex> lock(write_mutex);
     
     int write_fd = -1;
     
     // Determine which pipe to write to based on the context
     if (capsule_write_fd != -1) {
         write_fd = capsule_write_fd;
     } else if (transceiver_write_fd != -1) {
         write_fd = transceiver_write_fd;
     } else {
         LOG_ERROR("No write pipe available");
         return;
     }
     
     // Write data to pipe
     ssize_t bytes_written = write(write_fd, buff, len);
     
     if (bytes_written == -1) {
         LOG_ERROR("Failed to write to pipe: %s", strerror(errno));
     } else if (bytes_written != len) {
         LOG_ERROR("Partial write to pipe: %zd of %u bytes", bytes_written, len);
     } else {
         LOG_DEBUG("Successfully wrote %u bytes to pipe", len);
     }
 }
 
 // TinyFrame mutex implementation
 bool TF_ClaimTx(TinyFrame* tf) {
     write_mutex.lock();
     return true;
 }
 
 void TF_ReleaseTx(TinyFrame* tf) {
     write_mutex.unlock();
 }