/**
 * @file test_buffer.c
 * @brief Buffer management and edge case tests for unilog
 */

#include <unilog/unilog.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_buffer_wrap(void) {
    uint8_t buffer[256];
    unilog_t log;
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Fill buffer with messages */
    for (int i = 0; i < 10; i++) {
        unilog_result_t res = unilog_format(&log, UNILOG_LEVEL_INFO, i, "Message %d", i);
        if (res == UNILOG_ERR_FULL) {
            break;
        }
        assert(res == UNILOG_OK);
    }
    
    /* Read some messages */
    for (int i = 0; i < 5; i++) {
        int len = unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf));
        assert(len > 0);
    }
    
    /* Write more messages (should wrap around) */
    for (int i = 10; i < 15; i++) {
        unilog_result_t res = unilog_format(&log, UNILOG_LEVEL_INFO, i, "Message %d", i);
        if (res != UNILOG_OK && res != UNILOG_ERR_FULL) {
            assert(0);  /* Unexpected error */
        }
    }
    
    printf("✓ test_buffer_wrap passed\n");
}

static void test_buffer_full(void) {
    uint8_t buffer[256];
    unilog_t log;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Fill buffer until full */
    int count = 0;
    for (int i = 0; i < 100; i++) {
        unilog_result_t res = unilog_format(&log, UNILOG_LEVEL_INFO, i, "Test message %d", i);
        if (res == UNILOG_ERR_FULL) {
            break;
        }
        assert(res == UNILOG_OK);
        count++;
    }
    
    assert(count > 0);  /* Should have written at least one message */
    assert(count < 100);  /* Should have hit full condition */
    
    printf("✓ test_buffer_full passed (wrote %d messages before full)\n", count);
}

static void test_empty_read(void) {
    uint8_t buffer[1024];
    unilog_t log;
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Try to read from empty buffer */
    int len = unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf));
    assert(len == UNILOG_ERR_EMPTY);
    
    printf("✓ test_empty_read passed\n");
}

static void test_large_message(void) {
    uint8_t buffer[1024];
    unilog_t log;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Try to write a message that's too large */
    char large_msg[600];
    memset(large_msg, 'A', sizeof(large_msg) - 1);
    large_msg[sizeof(large_msg) - 1] = '\0';
    
    /* This should return an error */
    unilog_result_t res = unilog_write(&log, UNILOG_LEVEL_INFO, 0, large_msg);
    assert(res == UNILOG_ERR_INVALID);
    
    printf("✓ test_large_message passed\n");
}

static void test_truncated_read(void) {
    uint8_t buffer[1024];
    unilog_t log;
    char small_buf[10];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Write a long message */
    unilog_write(&log, UNILOG_LEVEL_INFO, 1, "This is a very long message");
    
    /* Read into small buffer */
    int len = unilog_read(&log, &level, &timestamp, small_buf, sizeof(small_buf));
    assert(len > 0);
    assert(len < (int)sizeof(small_buf));  /* Should be truncated */
    assert(small_buf[len] == '\0');  /* Should be null-terminated */
    
    printf("✓ test_truncated_read passed\n");
}

static void test_alternating_write_read(void) {
    uint8_t buffer[1024];
    unilog_t log;
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Alternate between writing and reading */
    for (int i = 0; i < 20; i++) {
        assert(unilog_format(&log, UNILOG_LEVEL_INFO, i, "Message %d", i) == UNILOG_OK);
        
        int len = unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf));
        assert(len > 0);
        assert((int)timestamp == i);
        
        char expected[256];
        snprintf(expected, sizeof(expected), "Message %d", i);
        assert(strcmp(read_buf, expected) == 0);
    }
    
    assert(unilog_is_empty(&log));
    
    printf("✓ test_alternating_write_read passed\n");
}

int main(void) {
    printf("Running buffer management tests...\n\n");
    
    test_buffer_wrap();
    test_buffer_full();
    test_empty_read();
    test_large_message();
    test_truncated_read();
    test_alternating_write_read();
    
    printf("\n✓ All buffer tests passed!\n");
    return 0;
}
