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


static void signal_handler(int sig) {
    (void)sig; // unused
    struct timespec ts = {0, 5000L};
    nanosleep(&ts, NULL); // Simulate some work
    unilog_write(&g_log, UNILOG_LEVEL_WARN, 999999, "Signal handler message");
    atomic_fetch_add(&g_signal_count, 1);
}

static void *signal_writer_thread(void *arg) {
    (void)arg; // unused
    
    while (atomic_load(&g_running)) {
        unilog_result_t res = unilog_write(&g_log, UNILOG_LEVEL_INFO, 123456,
                                            "Writer thread message");
        if (res == UNILOG_OK) {
            atomic_fetch_add(&g_write_count, 1);
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
    }
    
    int writes = atomic_load(&g_write_count);
    int reads = atomic_load(&g_read_count);
    int signals = atomic_load(&g_signal_count);
    
    printf("✓ test_signal_interrupt_reader passed (wrote: %d, read: %d, signals: %d)\n", 
           writes, reads, signals);
    
    assert(signals == 1000); // Signal handler should have been called 1000 times
    assert(reads > 0);
    assert(writes > 0);
    assert(reads <= writes);
}

static void test_signal_interrupt_writer(void) {
    uint8_t buffer[16384];
    struct sigaction sa;
    
    atomic_store(&g_write_count, 0);
    atomic_store(&g_read_count, 0);
    atomic_store(&g_running, 1);
    atomic_store(&g_signal_count, 0);
    
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
    }
    
    int writes = atomic_load(&g_write_count);
    int reads = atomic_load(&g_read_count);
    int signals = atomic_load(&g_signal_count);
    
    printf("✓ test_signal_interrupt_writer passed (wrote: %d, read: %d, signals: %d)\n", 
           writes, reads, signals);
    
    assert(signals == 1000); // Signal handler should have been called 1000 times
    assert(reads > 0);
    assert(writes > 0);
    assert(reads <= writes);
}

int main(void) {
    printf("Running signal safety tests...\n\n");
    
    test_signal_interrupt_reader();
    test_signal_interrupt_writer();
    
    printf("\n✓ All signal safety tests passed!\n");
    return 0;
}