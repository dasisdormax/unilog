/**
 * @file unilog.h
 * @brief Universal embedded logging library with lock-free MPSC buffer
 * 
 * A C11 library to support logging on embedded devices in any context,
 * including interrupts. Uses an atomic, lock-free MPSC buffer.
 * No dynamic allocation - works on provided buffers.
 */

#ifndef UNILOG_H
#define UNILOG_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log levels supported by unilog
 */
typedef enum {
    UNILOG_LEVEL_TRACE = 0,
    UNILOG_LEVEL_DEBUG = 1,
    UNILOG_LEVEL_INFO = 2,
    UNILOG_LEVEL_WARN = 3,
    UNILOG_LEVEL_ERROR = 4,
    UNILOG_LEVEL_FATAL = 5,
    UNILOG_LEVEL_NONE = 6
} unilog_level_t;

/**
 * @brief Return codes for unilog operations
 */
typedef enum {
    UNILOG_OK = 0,
    UNILOG_ERR_FULL = -1,
    UNILOG_ERR_INVALID = -2,
    UNILOG_ERR_EMPTY = -3,
    UNILOG_ERR_BUSY = -4
} unilog_result_t;

/**
 * @brief Lock-free MPSC ring buffer for log entries
 * 
 * This structure uses atomic operations for thread-safe and interrupt-safe
 * access from multiple producers and a single consumer.
 */
typedef struct {
    _Atomic(uint32_t) write_pos;  /**< Write position (producer) */
    _Atomic(uint32_t) read_pos;   /**< Read position (consumer) */
    uint32_t capacity;             /**< Buffer capacity in bytes */
    uint8_t *buffer;               /**< Pointer to buffer storage */
} unilog_buffer_t;

/**
 * @brief Log entry header stored in the ring buffer
 * 
 * Note: Structure is aligned to 4 bytes. Producers reserve space 
 * atomically to prevent conflicting writes, and write the length last
 * so consumers will only read complete entries.
 */
typedef struct {
    uint32_t length;        /**< Total length including header and message */
    unilog_level_t level;   /**< Log level */
    uint32_t timestamp;     /**< Timestamp (implementation-defined units) */
} unilog_entry_header_t;

/**
 * @brief Main unilog context structure
 */
typedef struct {
    unilog_buffer_t buffer;         /**< Lock-free ring buffer */
    _Atomic(unilog_level_t) min_level;  /**< Minimum log level to record */
} unilog_t;

/**
 * @brief Initialize a unilog buffer with provided memory
 * 
 * @param log Pointer to unilog context
 * @param buffer Pointer to buffer memory (must remain valid)
 * @param capacity Buffer capacity in bytes (must be power of 2)
 * @return UNILOG_OK on success, error code otherwise
 */
unilog_result_t unilog_init(unilog_t *log, void *buffer, uint32_t capacity);

/**
 * @brief Set the minimum log level
 * 
 * @param log Pointer to unilog context
 * @param level Minimum level to log
 */
void unilog_set_level(unilog_t *log, unilog_level_t level);

/**
 * @brief Get the current minimum log level
 * 
 * @param log Pointer to unilog context
 * @return Current minimum log level
 */
unilog_level_t unilog_get_level(const unilog_t *log);

/**
 * @brief Write a formatted log message
 * 
 * This function is thread-safe and lock-free, but NOT
 * interrupt-safe due to use of variable arguments.
 * Uses snprintf internally for formatting.
 * 
 * @param log Pointer to unilog context
 * @param level Log level
 * @param timestamp Timestamp value (implementation-defined)
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * @return UNILOG_OK on success, error code otherwise
 */
unilog_result_t unilog_format(unilog_t *log, unilog_level_t level, 
                              uint32_t timestamp, const char *format, ...);

/**
 * @brief Write a raw message without formatting
 * 
 * This function is interrupt-safe and lock-free.
 * Uses memcpy internally.
 * 
 * @param log Pointer to unilog context
 * @param level Log level
 * @param timestamp Timestamp value (implementation-defined)
 * @param message Raw message string
 * @param length Message length (without null terminator)
 * @return UNILOG_OK on success, error code otherwise
 */
unilog_result_t unilog_write_raw(unilog_t *log, unilog_level_t level,
                                  uint32_t timestamp, const char *message,
                                  size_t length);

/**
 * @brief Write a null-terminated message
 * 
 * This function is interrupt-safe and lock-free.
 * Determines message length using strlen.
 * 
 * @param log Pointer to unilog context
 * @param level Log level
 * @param timestamp Timestamp value (implementation-defined)
 * @param message Null-terminated message string
 * @return UNILOG_OK on success, error code otherwise
 */
unilog_result_t unilog_write(unilog_t *log, unilog_level_t level,
                             uint32_t timestamp, const char *message);

/**
 * @brief Read the next log entry from the buffer
 * 
 * This function should only be called from the consumer thread.
 * 
 * @param log Pointer to unilog context
 * @param level Output pointer for log level
 * @param timestamp Output pointer for timestamp
 * @param buffer Output buffer for message
 * @param buffer_size Size of output buffer
 * @return Number of bytes read on success, negative error code otherwise
 */
int unilog_read(unilog_t *log, unilog_level_t *level, uint32_t *timestamp,
                char *buffer, size_t buffer_size);

/**
 * @brief Get the number of bytes available to read
 * 
 * @param log Pointer to unilog context
 * @return Number of bytes available
 */
uint32_t unilog_available(const unilog_t *log);

/**
 * @brief Check if the buffer is empty
 * 
 * @param log Pointer to unilog context
 * @return true if empty, false otherwise
 */
bool unilog_is_empty(const unilog_t *log);

/**
 * @brief Get level name as string
 * 
 * @param level Log level
 * @return Level name string
 */
const char *unilog_level_name(unilog_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* UNILOG_H */
