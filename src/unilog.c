/**
 * @file unilog.c
 * @brief Implementation of unilog lock-free logging library
 */

#include "unilog/unilog.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Internal helper to check if value is power of 2 */
static inline bool is_power_of_2(uint32_t x) {
    return x > 0 && (x & (x - 1)) == 0;
}

/* Internal helper to align size to 4-byte boundary */
static inline uint32_t align_up(uint32_t size) {
    return (size + 3) & ~3;
}

unilog_result_t unilog_init(unilog_t *log, void *buffer, uint32_t capacity) {
    if (!log || !buffer || !is_power_of_2(capacity)) {
        return UNILOG_ERR_INVALID;
    }
    
    /* Initialize buffer structure */
    atomic_init(&log->buffer.write_pos, 0);
    atomic_init(&log->buffer.read_pos, 0);
    log->buffer.capacity = capacity;
    log->buffer.buffer = (uint8_t *)buffer;
    
    /* Initialize minimum log level */
    atomic_init(&log->min_level, UNILOG_LEVEL_TRACE);
    
    /* Clear the buffer */
    memset(buffer, 0, capacity);
    
    return UNILOG_OK;
}

void unilog_set_level(unilog_t *log, unilog_level_t level) {
    if (!log) {
        return;
    }
    atomic_store(&log->min_level, level);
}

unilog_level_t unilog_get_level(const unilog_t *log) {
    if (!log) {
        return UNILOG_LEVEL_NONE;
    }
    return atomic_load(&log->min_level);
}

static unilog_result_t unilog_write_internal(unilog_t *log, unilog_level_t level,
                                               uint32_t timestamp, const char *message,
                                               size_t msg_len) {
    if (!log || !message) {
        return UNILOG_ERR_INVALID;
    }
    
    /* Check if this level should be logged */
    unilog_level_t min_level = atomic_load(&log->min_level);
    if (level < min_level) {
        return UNILOG_OK;  /* Silently ignore */
    }
    
    /* Calculate total entry size (aligned) */
    uint32_t header_size = sizeof(unilog_entry_header_t);
    uint32_t total_size = header_size + msg_len;
    uint32_t advance_by = align_up(total_size);
    
    /* Check if entry is too large */
    if (total_size > log->buffer.capacity / 2) {
        /* TODO: Check if we really need this restriction */
        return UNILOG_ERR_INVALID;
    }
    
    /* Get current positions atomically */
    uint32_t capacity = log->buffer.capacity;
    uint32_t mask = capacity - 1;
    
    /* Try to reserve space using atomic compare-exchange */
    uint32_t write_pos, new_write_pos;
    do {
        write_pos = atomic_load_explicit(&log->buffer.write_pos, memory_order_acquire);
        uint32_t read_pos = atomic_load_explicit(&log->buffer.read_pos, memory_order_acquire);
        
        /* Calculate available space */
        uint32_t used = (write_pos - read_pos) & mask;
        uint32_t available = capacity - used - 1;  /* -1 to distinguish full from empty */
        
        if (advance_by > available) {
            return UNILOG_ERR_FULL;
        }
        
        new_write_pos = (write_pos + advance_by) & mask;
    } while (!atomic_compare_exchange_weak_explicit(&log->buffer.write_pos, &write_pos, 
                                                      new_write_pos, memory_order_release, 
                                                      memory_order_acquire));
    
    /* Now we have exclusive access to [write_pos, new_write_pos) */
    uint8_t *buffer = log->buffer.buffer;
    
    /* Write header */
    unilog_entry_header_t header;
    header.length = total_size;
    header.level = level;
    header.timestamp = timestamp;
    
    uint32_t pos = (write_pos + sizeof(header.length)) & mask;
    
    /* Copy header, excluding length */
    for (size_t i = sizeof(header.length); i < sizeof(header); i++) {
        buffer[pos] = ((uint8_t *)&header)[i];
        pos = (pos + 1) & mask;
    }
    
    /* Copy message */
    for (size_t i = 0; i < msg_len; i++) {
        buffer[pos] = message[i];
        pos = (pos + 1) & mask;
    }
    
    /* Pad to alignment */
    while (pos != new_write_pos) {
        buffer[pos] = 0;
        pos = (pos + 1) & mask;
    }

    /* Mark entry as complete by writing length last (atomic release) */
    atomic_store_explicit((_Atomic uint32_t *)&buffer[write_pos],
            header.length, memory_order_release);
    
    return UNILOG_OK;
}

unilog_result_t unilog_format(unilog_t *log, unilog_level_t level,
                              uint32_t timestamp, const char *format, ...) {
    if (!log || !format) {
        return UNILOG_ERR_INVALID;
    }
    
    /* Format message into a temporary buffer */
    char temp_buffer[256];  /* Stack-allocated, no dynamic memory */
    va_list args;
    va_start(args, format);
    int len = vsnprintf(temp_buffer, sizeof(temp_buffer), format, args);
    va_end(args);
    
    if (len < 0) {
        return UNILOG_ERR_INVALID;
    }
    
    /* Truncate if necessary */
    if (len >= (int)sizeof(temp_buffer)) {
        len = sizeof(temp_buffer) - 1;
    }
    
    return unilog_write_internal(log, level, timestamp, temp_buffer, len);
}

unilog_result_t unilog_write_raw(unilog_t *log, unilog_level_t level,
                                  uint32_t timestamp, const char *message,
                                  size_t length) {
    return unilog_write_internal(log, level, timestamp, message, length);
}

unilog_result_t unilog_write(unilog_t *log, unilog_level_t level,
                             uint32_t timestamp, const char *message) {
    if (!message) {
        return UNILOG_ERR_INVALID;
    }
    return unilog_write_internal(log, level, timestamp, message, strlen(message));
}

int unilog_read(unilog_t *log, unilog_level_t *level, uint32_t *timestamp,
                char *buffer, size_t buffer_size) {
    if (!log || !level || !timestamp || !buffer || buffer_size == 0) {
        return UNILOG_ERR_INVALID;
    }
    
    uint32_t capacity = log->buffer.capacity;
    uint32_t mask = capacity - 1;
    
    /* Get current read position */
    uint32_t read_pos = atomic_load_explicit(&log->buffer.read_pos, memory_order_acquire);
    uint32_t write_pos = atomic_load_explicit(&log->buffer.write_pos, memory_order_acquire);
    
    /* Check if buffer is empty */
    if (read_pos == write_pos) {
        return UNILOG_ERR_EMPTY;
    }
    
    uint8_t *buf = log->buffer.buffer;
    
    /* Check if message was written completely (load length with acquire) */
    uint32_t total_size = atomic_load_explicit((_Atomic uint32_t *)&buf[read_pos], memory_order_acquire);
    if (total_size == 0) {
        return UNILOG_ERR_BUSY;  /* Message not yet complete */
    }

    if (total_size > capacity / 2) {
        return UNILOG_ERR_INVALID;
    }

    atomic_store_explicit((_Atomic uint32_t *)&buf[read_pos], 0, memory_order_relaxed);

    /* Read header */
    unilog_entry_header_t header;
    header.length = total_size;

    uint32_t pos = (read_pos + sizeof(header.length)) & mask;
    for (size_t i = sizeof(header.length); i < sizeof(header); i++) {
        ((uint8_t *)&header)[i] = buf[pos];
        buf[pos] = 0;
        pos = (pos + 1) & mask;
    }
    
    *level = header.level;
    *timestamp = header.timestamp;
    
    /* Calculate message length */
    uint32_t msg_len = total_size - sizeof(header);
    uint32_t copy_len = msg_len < buffer_size ? msg_len : buffer_size - 1;
    
    /* Read message */
    for (size_t i = 0; i < copy_len; i++) {
        buffer[i] = buf[pos];
        buf[pos] = 0;
        pos = (pos + 1) & mask;
    }
    buffer[copy_len] = '\0';
    
    /* Update read position with release semantics */
    uint32_t advance_by = align_up(header.length);
    uint32_t new_read_pos = (read_pos + advance_by) & mask;
    atomic_store_explicit(&log->buffer.read_pos, new_read_pos, memory_order_release);

    /* Clear padding */
    while (pos != new_read_pos) {
        buf[pos] = 0;
        pos = (pos + 1) & mask;
    }
    
    return (int)copy_len;
}

uint32_t unilog_available(const unilog_t *log) {
    if (!log) {
        return 0;
    }
    
    uint32_t read_pos = atomic_load_explicit(&log->buffer.read_pos, memory_order_acquire);
    uint32_t write_pos = atomic_load_explicit(&log->buffer.write_pos, memory_order_acquire);
    uint32_t mask = log->buffer.capacity - 1;
    
    return (write_pos - read_pos) & mask;
}

bool unilog_is_empty(const unilog_t *log) {
    if (!log) {
        return true;
    }
    
    uint32_t read_pos = atomic_load_explicit(&log->buffer.read_pos, memory_order_acquire);
    uint32_t write_pos = atomic_load_explicit(&log->buffer.write_pos, memory_order_acquire);
    
    return write_pos == read_pos;
}

const char *unilog_level_name(unilog_level_t level) {
    switch (level) {
        case UNILOG_LEVEL_TRACE: return "TRACE";
        case UNILOG_LEVEL_DEBUG: return "DEBUG";
        case UNILOG_LEVEL_INFO:  return "INFO";
        case UNILOG_LEVEL_WARN:  return "WARN";
        case UNILOG_LEVEL_ERROR: return "ERROR";
        case UNILOG_LEVEL_FATAL: return "FATAL";
        case UNILOG_LEVEL_NONE:  return "NONE";
        default: return "UNKNOWN";
    }
}
