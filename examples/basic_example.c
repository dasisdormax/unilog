/**
 * @file basic_example.c
 * @brief Basic example of using unilog library
 */

#include <unilog/unilog.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Simulated timestamp function */
static uint32_t get_timestamp(void) {
    return (uint32_t)time(NULL);
}

int main(void) {
    /* Allocate buffer on stack (no dynamic allocation) */
    uint8_t buffer[1024];
    unilog_t log;
    
    /* Initialize the logger */
    unilog_result_t result = unilog_init(&log, buffer, sizeof(buffer));
    if (result != UNILOG_OK) {
        fprintf(stderr, "Failed to initialize unilog\n");
        return 1;
    }
    
    printf("Unilog initialized with %zu byte buffer\n\n", sizeof(buffer));
    
    /* Set minimum log level */
    unilog_set_level(&log, UNILOG_LEVEL_DEBUG);
    
    /* Write some log messages */
    printf("Writing log messages...\n");
    unilog_format(&log, UNILOG_LEVEL_INFO, get_timestamp(), "System initialized");
    unilog_format(&log, UNILOG_LEVEL_DEBUG, get_timestamp(), "Debug value: %d", 42);
    unilog_format(&log, UNILOG_LEVEL_WARN, get_timestamp(), "Warning: value out of range");
    unilog_format(&log, UNILOG_LEVEL_ERROR, get_timestamp(), "Error code: 0x%X", 0xDEADBEEF);
    unilog_format(&log, UNILOG_LEVEL_TRACE, get_timestamp(), "This trace won't be logged (below threshold)");
    
    /* Write a raw message */
    const char *raw_msg = "Raw message without formatting";
    unilog_write_raw(&log, UNILOG_LEVEL_INFO, get_timestamp(), raw_msg, strlen(raw_msg));
    
    printf("\nReading log messages...\n");
    printf("----------------------------------------\n");
    
    /* Read and display log messages */
    char read_buffer[256];
    unilog_level_t level;
    uint32_t timestamp;
    int read_len;
    
    while ((read_len = unilog_read(&log, &level, &timestamp, read_buffer, sizeof(read_buffer))) > 0) {
        printf("[%u] %s: %s\n", timestamp, unilog_level_name(level), read_buffer);
    }
    
    printf("----------------------------------------\n");
    printf("\nExample completed successfully\n");
    
    return 0;
}
