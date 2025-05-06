/**
 * TF_Config.h - TinyFrame configuration
 */

 #ifndef TF_CONFIG_H
 #define TF_CONFIG_H
 
 #include <stdint.h>
 
 // Message format configuration
 #define TF_ID_BYTES     1
 #define TF_LEN_BYTES    2
 #define TF_TYPE_BYTES   1
 #define TF_CKSUM_TYPE   TF_CKSUM_XOR
 #define TF_USE_SOF_BYTE 1
 #define TF_SOF_BYTE     0x01
 
 // Platform compatibility
 typedef uint16_t TF_TICKS;
 typedef uint8_t TF_COUNT;
 
 // Parameters
 #define TF_MAX_PAYLOAD_RX 512
 #define TF_SENDBUF_LEN    128
 #define TF_MAX_ID_LST     5
 #define TF_MAX_TYPE_LST   5
 #define TF_MAX_GEN_LST    2
 #define TF_PARSER_TIMEOUT_TICKS 10
 #define TF_USE_MUTEX      1
 
 // Error reporting with our logging system
 #define TF_Error(format, ...) LOG_ERROR(format, ##__VA_ARGS__)
 
 #endif // TF_CONFIG_H