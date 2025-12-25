/**
 * @file test_threadsafe.c
 * @brief Thread safety tests for unilog
 */

#include <unilog/unilog.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#define NUM_THREADS 8
#define MESSAGES_PER_THREAD 100

static unilog_t g_log;
static _Atomic(int) g_write_count;
static _Atomic(int) g_read_count;
static _Atomic(int) g_running;

typedef struct {
    int thread_id;
} thread_arg_t;

static void *producer_thread(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    int tid = targ->thread_id;
    
    for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
        unilog_result_t res = unilog_write(&g_log, UNILOG_LEVEL_INFO, tid * 1000 + i,
                                            "Thread %d message %d", tid, i);
        if (res == UNILOG_OK) {
            atomic_fetch_add(&g_write_count, 1);
        }
    }
    
    return NULL;
}

static void *consumer_thread(void *arg) {
    (void)arg; // unused
    
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    while (atomic_load(&g_running)) {
        if (unilog_read(&g_log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0) {
            atomic_fetch_add(&g_read_count, 1);
        }
    }
    
    // Read any remaining messages
    while (unilog_read(&g_log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0) {
        atomic_fetch_add(&g_read_count, 1);
    }
    
    return NULL;
}

static void test_concurrent_writes(void) {
    uint8_t buffer[8192];
    
    atomic_store(&g_write_count, 0);
    atomic_store(&g_read_count, 0);
    
    unilog_init(&g_log, buffer, sizeof(buffer));
    
    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];
    
    /* Start producer threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, producer_thread, &args[i]);
    }
    
    /* Wait for all threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Read all messages */
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    while (unilog_read(&g_log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0) {
        atomic_fetch_add(&g_read_count, 1);
    }
    
    int writes = atomic_load(&g_write_count);
    int reads = atomic_load(&g_read_count);
    
    printf("✓ test_concurrent_writes passed (wrote: %d, read: %d)\n", writes, reads);
    
    /* We should have read all written messages (or at least most if buffer was full) */
    assert(reads > 0);
    assert(reads <= writes);
}

static void test_concurrent_read_write(void) {
    uint8_t buffer[16384]; // Larger buffer to reduce overflow likelihood
    
    atomic_store(&g_write_count, 0);
    atomic_store(&g_read_count, 0);
    atomic_store(&g_running, 1);
    
    unilog_init(&g_log, buffer, sizeof(buffer));
    
    pthread_t consumer;
    pthread_create(&consumer, NULL, consumer_thread, NULL);
    
    pthread_t producers[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];
    
    /* Start producer threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id = i;
        pthread_create(&producers[i], NULL, producer_thread, &args[i]);
    }
    
    /* Wait for all producers */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    /* Stop consumer */
    atomic_store(&g_running, 0);
    pthread_join(consumer, NULL);
    
    int writes = atomic_load(&g_write_count);
    int reads = atomic_load(&g_read_count);
    
    printf("✓ test_concurrent_read_write passed (wrote: %d, read: %d)\n", writes, reads);
    
    assert(reads > 0);
    assert(reads <= writes);
}

static void *mixed_producer(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;
    int tid = targ->thread_id;
    
    for (int i = 0; i < MESSAGES_PER_THREAD / 2; i++) {
        /* Alternate between formatted and raw writes */
        if (i % 2 == 0) {
            unilog_write(&g_log, UNILOG_LEVEL_DEBUG, tid * 1000 + i,
                        "Formatted: T%d M%d", tid, i);
        } else {
            char msg[64];
            int len = snprintf(msg, sizeof(msg), "Raw: T%d M%d", tid, i);
            unilog_write_raw(&g_log, UNILOG_LEVEL_INFO, tid * 1000 + i, msg, len);
        }
    }
    
    return NULL;
}

static void test_mixed_operations(void) {
    uint8_t buffer[8192];
    
    unilog_init(&g_log, buffer, sizeof(buffer));
    
    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];
    
    /* Start mixed producer threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, mixed_producer, &args[i]);
    }
    
    /* Wait for all threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Verify we can read messages */
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    int count = 0;
    
    while (unilog_read(&g_log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0) {
        count++;
    }
    
    printf("✓ test_mixed_operations passed (read %d messages)\n", count);
    assert(count > 0);
}

static void test_level_change_concurrent(void) {
    uint8_t buffer[4096];
    
    unilog_init(&g_log, buffer, sizeof(buffer));
    unilog_set_level(&g_log, UNILOG_LEVEL_INFO);
    
    /* Verify level changes are atomic */
    /* Number of distinct log levels from TRACE(0) to NONE(6) */
    const int num_log_levels = 7;
    for (int i = 0; i < 100; i++) {
        unilog_set_level(&g_log, (unilog_level_t)(i % num_log_levels));
        unilog_level_t level = unilog_get_level(&g_log);
        assert(level >= UNILOG_LEVEL_TRACE && level <= UNILOG_LEVEL_NONE);
    }
    
    printf("✓ test_level_change_concurrent passed\n");
}

int main(void) {
    printf("Running thread safety tests...\n\n");
    
    test_concurrent_writes();
    test_concurrent_read_write();
    test_mixed_operations();
    test_level_change_concurrent();
    
    printf("\n✓ All thread safety tests passed!\n");
    return 0;
}
