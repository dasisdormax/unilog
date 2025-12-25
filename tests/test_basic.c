/**
 * @file test_basic.c
 * @brief Basic functionality tests for unilog
 */

#include <unilog/unilog.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_init(void) {
    uint8_t buffer[1024];
    unilog_t log;
    
    /* Test valid initialization */
    assert(unilog_init(&log, buffer, sizeof(buffer)) == UNILOG_OK);
    
    /* Test invalid parameters */
    assert(unilog_init(NULL, buffer, sizeof(buffer)) == UNILOG_ERR_INVALID);
    assert(unilog_init(&log, NULL, sizeof(buffer)) == UNILOG_ERR_INVALID);
    assert(unilog_init(&log, buffer, 1023) == UNILOG_ERR_INVALID);  /* Not power of 2 */
    
    printf("✓ test_init passed\n");
}

static void test_level(void) {
    uint8_t buffer[1024];
    unilog_t log;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Test default level */
    assert(unilog_get_level(&log) == UNILOG_LEVEL_TRACE);
    
    /* Test setting level */
    unilog_set_level(&log, UNILOG_LEVEL_WARN);
    assert(unilog_get_level(&log) == UNILOG_LEVEL_WARN);
    
    printf("✓ test_level passed\n");
}

static void test_write_read(void) {
    uint8_t buffer[1024];
    unilog_t log;
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Write a message */
    assert(unilog_write(&log, UNILOG_LEVEL_INFO, 12345, "Test message") == UNILOG_OK);
    
    /* Check buffer is not empty */
    assert(!unilog_is_empty(&log));
    assert(unilog_available(&log) > 0);
    
    /* Read the message */
    int len = unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf));
    assert(len > 0);
    assert(level == UNILOG_LEVEL_INFO);
    assert(timestamp == 12345);
    assert(strcmp(read_buf, "Test message") == 0);
    
    /* Check buffer is now empty */
    assert(unilog_is_empty(&log));
    assert(unilog_available(&log) == 0);
    
    printf("✓ test_write_read passed\n");
}

static void test_formatted_write(void) {
    uint8_t buffer[1024];
    unilog_t log;
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Write formatted message */
    assert(unilog_write(&log, UNILOG_LEVEL_DEBUG, 100, "Value: %d, Hex: 0x%X", 42, 0xABCD) == UNILOG_OK);
    
    /* Read and verify */
    int len = unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf));
    assert(len > 0);
    assert(strcmp(read_buf, "Value: 42, Hex: 0xABCD") == 0);
    
    printf("✓ test_formatted_write passed\n");
}

static void test_raw_write(void) {
    uint8_t buffer[1024];
    unilog_t log;
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Write raw message */
    const char *msg = "Raw message test";
    assert(unilog_write_raw(&log, UNILOG_LEVEL_ERROR, 200, msg, strlen(msg)) == UNILOG_OK);
    
    /* Read and verify */
    int len = unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf));
    assert(len > 0);
    assert(level == UNILOG_LEVEL_ERROR);
    assert(timestamp == 200);
    assert(strcmp(read_buf, msg) == 0);
    
    printf("✓ test_raw_write passed\n");
}

static void test_multiple_messages(void) {
    uint8_t buffer[1024];
    unilog_t log;
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    
    /* Write multiple messages */
    assert(unilog_write(&log, UNILOG_LEVEL_INFO, 1, "Message 1") == UNILOG_OK);
    assert(unilog_write(&log, UNILOG_LEVEL_WARN, 2, "Message 2") == UNILOG_OK);
    assert(unilog_write(&log, UNILOG_LEVEL_ERROR, 3, "Message 3") == UNILOG_OK);
    
    /* Read and verify in order */
    assert(unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0);
    assert(timestamp == 1);
    assert(strcmp(read_buf, "Message 1") == 0);
    
    assert(unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0);
    assert(timestamp == 2);
    assert(strcmp(read_buf, "Message 2") == 0);
    
    assert(unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0);
    assert(timestamp == 3);
    assert(strcmp(read_buf, "Message 3") == 0);
    
    assert(unilog_is_empty(&log));
    
    printf("✓ test_multiple_messages passed\n");
}

static void test_level_filtering(void) {
    uint8_t buffer[1024];
    unilog_t log;
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    unilog_init(&log, buffer, sizeof(buffer));
    unilog_set_level(&log, UNILOG_LEVEL_WARN);
    
    /* Write messages at different levels */
    assert(unilog_write(&log, UNILOG_LEVEL_DEBUG, 1, "Debug") == UNILOG_OK);  /* Filtered */
    assert(unilog_write(&log, UNILOG_LEVEL_INFO, 2, "Info") == UNILOG_OK);    /* Filtered */
    assert(unilog_write(&log, UNILOG_LEVEL_WARN, 3, "Warning") == UNILOG_OK); /* Logged */
    assert(unilog_write(&log, UNILOG_LEVEL_ERROR, 4, "Error") == UNILOG_OK);  /* Logged */
    
    /* Should only read WARN and ERROR */
    assert(unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0);
    assert(level == UNILOG_LEVEL_WARN);
    
    assert(unilog_read(&log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0);
    assert(level == UNILOG_LEVEL_ERROR);
    
    assert(unilog_is_empty(&log));
    
    printf("✓ test_level_filtering passed\n");
}

static void test_level_names(void) {
    assert(strcmp(unilog_level_name(UNILOG_LEVEL_TRACE), "TRACE") == 0);
    assert(strcmp(unilog_level_name(UNILOG_LEVEL_DEBUG), "DEBUG") == 0);
    assert(strcmp(unilog_level_name(UNILOG_LEVEL_INFO), "INFO") == 0);
    assert(strcmp(unilog_level_name(UNILOG_LEVEL_WARN), "WARN") == 0);
    assert(strcmp(unilog_level_name(UNILOG_LEVEL_ERROR), "ERROR") == 0);
    assert(strcmp(unilog_level_name(UNILOG_LEVEL_FATAL), "FATAL") == 0);
    
    printf("✓ test_level_names passed\n");
}

int main(void) {
    printf("Running basic tests...\n\n");
    
    test_init();
    test_level();
    test_write_read();
    test_formatted_write();
    test_raw_write();
    test_multiple_messages();
    test_level_filtering();
    test_level_names();
    
    printf("\n✓ All basic tests passed!\n");
    return 0;
}
