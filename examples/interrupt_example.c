/**
 * @file interrupt_example.c
 * @brief Example demonstrating interrupt-safe logging with threads
 */

#define _POSIX_C_SOURCE 199309L

#include <unilog/unilog.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* Shared logger instance */
static unilog_t g_log;

/* Simulated timestamp function */
static uint32_t get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Simulated interrupt/producer thread */
static void *producer_thread(void *arg) {
    int thread_id = *(int *)arg;
    struct timespec ts = {0, 1000000L};
    
    for (int i = 0; i < 10; i++) {
        unilog_format(&g_log, UNILOG_LEVEL_INFO, get_timestamp(),
                     "Thread %d: message %d", thread_id, i);
        nanosleep(&ts, NULL);
    }
    
    return NULL;
}

int main(void) {
    /* Allocate buffer on stack (no dynamic allocation) */
    uint8_t buffer[4096];
    
    /* Initialize the logger */
    unilog_result_t result = unilog_init(&g_log, buffer, sizeof(buffer));
    if (result != UNILOG_OK) {
        fprintf(stderr, "Failed to initialize unilog\n");
        return 1;
    }
    
    printf("Interrupt-safe logging example\n");
    printf("Creating multiple producer threads...\n\n");
    
    /* Create multiple producer threads (simulating interrupts) */
    const int num_threads = 4;
    pthread_t threads[num_threads];
    int thread_ids[num_threads];
    
    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, producer_thread, &thread_ids[i]);
    }
    
    /* Consumer thread: read and display messages */
    printf("Reading messages from buffer:\n");
    printf("----------------------------------------\n");
    
    char read_buffer[256];
    unilog_level_t level;
    uint32_t timestamp;
    int messages_read = 0;
    int empty_count = 0;
    const int max_empty_checks = 100;  /* Number of empty checks before stopping */
    struct timespec ts = {0, 1000000L};
    
    /* Keep reading until all threads are done and buffer is empty */
    while (empty_count < max_empty_checks) {
        int read_len = unilog_read(&g_log, &level, &timestamp, read_buffer, sizeof(read_buffer));
        
        if (read_len > 0) {
            printf("[%u] %s: %s\n", timestamp, unilog_level_name(level), read_buffer);
            messages_read++;
            empty_count = 0;
        } else {
            empty_count++;
            nanosleep(&ts, NULL);
        }
    }
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Read any remaining messages */
    int read_len;
    while ((read_len = unilog_read(&g_log, &level, &timestamp, read_buffer, sizeof(read_buffer))) > 0) {
        printf("[%u] %s: %s\n", timestamp, unilog_level_name(level), read_buffer);
        messages_read++;
    }
    
    printf("----------------------------------------\n");
    printf("\nTotal messages read: %d\n", messages_read);
    printf("Expected messages: %d\n", num_threads * 10);
    printf("\nExample completed successfully\n");
    
    return 0;
}
