/**
 * @file test_signal.c
 * @brief Signal safety tests for unilog
 */

#include <unilog/unilog.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <time.h>

static unilog_t g_log;
static _Atomic(int) g_write_count;
static _Atomic(int) g_read_count;
static _Atomic(int) g_running;
static _Atomic(int) g_signal_count;


static _Atomic(long long) g_write_sum;
static _Atomic(long long) g_read_sum;


static void signal_handler(int sig) {
    (void)sig; // unused
    struct timespec ts = {0, 5000L};
    nanosleep(&ts, NULL); // Simulate some work
    int len = strlen("Signal handler message");
    unilog_result_t res = unilog_write(&g_log, UNILOG_LEVEL_WARN, 999999, "Signal handler message");
    if (res == UNILOG_OK) {
        atomic_fetch_add(&g_write_count, 1);
        atomic_fetch_add(&g_write_sum, len);
    }
    atomic_fetch_add(&g_signal_count, 1);
}

static void *signal_writer_thread(void *arg) {
    (void)arg; // unused
    
    while (atomic_load(&g_running)) {
        int len = strlen("Writer thread message");
        unilog_result_t res = unilog_write(&g_log, UNILOG_LEVEL_INFO, 123456,
                                            "Writer thread message");
        if (res == UNILOG_OK) {
            atomic_fetch_add(&g_write_count, 1);
            atomic_fetch_add(&g_write_sum, len);
        }
    }
    
    return NULL;
}

static void *signal_reader_thread(void *arg) {
    (void)arg; // unused
    
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    
    while (atomic_load(&g_running)) {
        if (unilog_read(&g_log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0) {
            atomic_fetch_add(&g_read_count, 1);
            int len = strnlen(read_buf, sizeof(read_buf));
            atomic_fetch_add(&g_read_sum, len);
        }
    }
    
    return NULL;
}

static void test_signal_interrupt_reader(void) {
    uint8_t buffer[16384];
    struct sigaction sa;
    
    atomic_store(&g_write_count, 0);
    atomic_store(&g_read_count, 0);
    atomic_store(&g_running, 1);
    atomic_store(&g_signal_count, 0);
    atomic_store(&g_write_sum, 0);
    atomic_store(&g_read_sum, 0);
    
    unilog_init(&g_log, buffer, sizeof(buffer));
    
    /* Set up signal handler */
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    /* Start writer and reader threads */
    pthread_t writer, reader;
    pthread_create(&writer, NULL, signal_writer_thread, NULL);
    pthread_create(&reader, NULL, signal_reader_thread, NULL);
    
    /* Send signals to reader thread until 1000 delivered */
    for (int signal_count = 1; signal_count <= 1000; signal_count++) {
        pthread_kill(reader, SIGUSR1);
        while(atomic_load(&g_signal_count) < signal_count) {
            struct timespec ts = {0, 50000L}; // 50us
            nanosleep(&ts, NULL);
        }
    }
    
    /* Stop threads */
    atomic_store(&g_running, 0);
    pthread_join(writer, NULL);
    pthread_join(reader, NULL);
    
    /* Read any remaining messages */
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    while (unilog_read(&g_log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0) {
        atomic_fetch_add(&g_read_count, 1);
        int len = strnlen(read_buf, sizeof(read_buf));
        atomic_fetch_add(&g_read_sum, len);
    }
    
    int writes = atomic_load(&g_write_count);
    int reads = atomic_load(&g_read_count);
    int signals = atomic_load(&g_signal_count);
    long long write_sum = atomic_load(&g_write_sum);
    long long read_sum = atomic_load(&g_read_sum);
    
    printf("✓ test_signal_interrupt_reader passed (wrote: %d, read: %d, signals: %d, write_sum: %lld, read_sum: %lld)\n", 
           writes, reads, signals, write_sum, read_sum);
    
    assert(signals == 1000); // Signal handler should have been called 1000 times
    assert(reads > 0);
    assert(writes > 0);
    assert(reads <= writes);
    assert(unilog_is_empty(&g_log));
    assert(write_sum == read_sum);
}

static void test_signal_interrupt_writer(void) {
    uint8_t buffer[16384];
    struct sigaction sa;
    
    atomic_store(&g_write_count, 0);
    atomic_store(&g_read_count, 0);
    atomic_store(&g_running, 1);
    atomic_store(&g_signal_count, 0);
    atomic_store(&g_write_sum, 0);
    atomic_store(&g_read_sum, 0);
    
    unilog_init(&g_log, buffer, sizeof(buffer));
    
    /* Set up signal handler */
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);
    
    /* Start writer and reader threads */
    pthread_t writer, reader;
    pthread_create(&writer, NULL, signal_writer_thread, NULL);
    pthread_create(&reader, NULL, signal_reader_thread, NULL);
    
    /* Send signals to writer thread until 1000 delivered */
    for (int signal_count = 1; signal_count <= 1000; signal_count++) {
        pthread_kill(writer, SIGUSR2);
        while(atomic_load(&g_signal_count) < signal_count) {
            struct timespec ts = {0, 50000L}; // 50us
            nanosleep(&ts, NULL);
        }
    }
    
    /* Stop threads */
    atomic_store(&g_running, 0);
    pthread_join(writer, NULL);
    pthread_join(reader, NULL);
    
    /* Read any remaining messages */
    char read_buf[256];
    unilog_level_t level;
    uint32_t timestamp;
    while (unilog_read(&g_log, &level, &timestamp, read_buf, sizeof(read_buf)) > 0) {
        atomic_fetch_add(&g_read_count, 1);
        int len = strnlen(read_buf, sizeof(read_buf));
        atomic_fetch_add(&g_read_sum, len);
    }
    
    int writes = atomic_load(&g_write_count);
    int reads = atomic_load(&g_read_count);
    int signals = atomic_load(&g_signal_count);
    long long write_sum = atomic_load(&g_write_sum);
    long long read_sum = atomic_load(&g_read_sum);
    
    printf("✓ test_signal_interrupt_writer passed (wrote: %d, read: %d, signals: %d, write_sum: %lld, read_sum: %lld)\n", 
           writes, reads, signals, write_sum, read_sum);
    
    assert(signals == 1000); // Signal handler should have been called 1000 times
    assert(reads > 0);
    assert(writes > 0);
    assert(reads <= writes);
    assert(unilog_is_empty(&g_log));
    assert(write_sum == read_sum);
}

int main(void) {
    printf("Running signal safety tests...\n\n");
    
    test_signal_interrupt_reader();
    test_signal_interrupt_writer();
    
    printf("\n✓ All signal safety tests passed!\n");
    return 0;
}