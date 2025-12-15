  Sessions 33-35 (Various improvements)

  - Browser enhancements:
    - Back button with history stack - navigate to previous pages
    - Link following - click <a href> links to navigate
    - Fixed inline text rendering bug (links were breaking to new lines)
    - Ordered list (<ol>) support
  - HTTPS/TLS 1.2 - COMPLETE!
    - Ported TLSe library (added to vendor/)
    - Created kernel libc stubs (kernel/libc/) to satisfy TLSe dependencies:
        - arpa/inet.h, assert.h, ctype.h, errno.h, limits.h, signal.h
      - stdio.h, stdlib.h, string.h, time.h, unistd.h, wchar.h
    - Built kernel/tls.c wrapper (~420 lines) exposing TLS to userspace
    - Added tls_connect, tls_send, tls_recv, tls_close to kapi
    - Updated /bin/fetch - now supports https:// URLs
    - Updated /bin/browser - HTTPS sites work!
    - Added memcmp, memmove to kernel string.c
    - Achievement: Can browse google.com, real HTTPS sites!
  - Resolution upgrade: 800x600 → 1024x768
  - Window resize: Draggable window edges for resizing
  - System Monitor expanded:
    - Shows used/unused heap
    - Process list
    - Sound playing status
  - MP3 loading improvements: Single-pass decode with loading state indicators

### Session 36
- **Fixed critical memory corruption bug in TTF rendering:**
  - Browser was consuming ALL available RAM (tested 512MB and 8GB - both exhausted!)
  - sysmon showed only 73 allocations, ~11MB heap "used", but 0MB "free" - contradiction
  - Root cause identified with help from Gemini: **buffer overflow in `apply_italic()`**
  - Bug: `apply_italic` was calculating `extra_w` again on top of already-expanded buffer
    - Called with `alloc_w` (already includes italic space)
    - Internally calculated `out_w = w + extra_w` (doubling the expansion)
    - `memcpy(bitmap, temp_bitmap, out_w * h)` wrote past buffer end
  - This corrupted heap metadata, breaking the free list traversal
  - Fixed by passing original glyph width separately from stride
- **Enhanced sysmon with memory debugging:**
  - Added `get_heap_start()`, `get_heap_end()`, `get_stack_ptr()`, `get_alloc_count()` to kapi
  - sysmon now shows: heap bounds, heap size, stack pointer, allocation count
  - Helped diagnose the memory corruption (heap looked full because free list was broken)
- **Cache eviction fix:**
  - TTF glyph cache eviction now frees bitmap allocations before resetting
  - Previously leaked all 128 bitmaps on each cache wrap
- **Expanded font size caches:**
  - Added sizes 14, 18, 20, 28 to match browser heading sizes
  - Prevents cache misses for common sizes
- **Known issue:** Italic rendering is still broken (garbled output)
  - The buffer overflow is fixed, but the shear logic needs more work
  - Bold works fine, italic produces garbage
- **Files changed:**
  - `kernel/ttf.c` - Fixed apply_italic buffer overflow, cache eviction
  - `kernel/memory.c` - Added debug functions
  - `kernel/memory.h` - Declared debug functions
  - `kernel/kapi.c`, `kernel/kapi.h` - Added memory debug to kapi
  - `user/lib/vibe.h` - Added memory debug functions
  - `user/bin/sysmon.c` - Added Memory Debug section

### Session 37
- **CSS Engine - Full CSS support for the browser!**
- **New files:**
  - `user/bin/browser/css.h` (~950 lines) - Complete CSS parser and style engine:
    - CSS value types (length units: px, em, rem, %)
    - Display modes (block, inline, inline-block, none, table, flex, list-item)
    - Float, position, visibility properties
    - Box model (width, height, margin, padding)
    - Text properties (color, background-color, font-size, font-weight, font-style, text-align, text-decoration, white-space, vertical-align)
    - CSS selector parsing (tag, .class, #id, [attr], *, combinators: >, +, ~, descendant)
    - Specificity calculation for cascade
    - `<style>` block parsing
    - Inline style attribute parsing
    - Pseudo-selector skipping (:hover, :focus, :not(), etc.)
  - `user/bin/browser/dom.h` (~400 lines) - Proper DOM tree structure:
    - DOM nodes (element and text types)
    - Tree structure (parent, children, siblings)
    - Attribute storage (id, class, style, href)
    - User-agent default styles for HTML elements
    - Style inheritance (color, font-size, etc. inherit from parent)
    - Computed style calculation (cascade: UA → stylesheet → inline)
    - Selector matching against DOM nodes
- **Updated files:**
  - `user/bin/browser/html.h` - Rewrote HTML parser to build DOM tree:
    - Creates proper tree structure instead of flat list
    - Extracts `<style>` blocks and parses them
    - Stores id, class, style, href attributes on nodes
    - Computes styles after parsing
    - `dom_to_blocks()` converts DOM to flat render list (legacy compatibility)
    - Fixed: href attribute now stored properly (was placeholder)
    - Fixed: Duplicate newline prevention (tracks last_was_newline)
  - `user/bin/browser/main.c` - Updated renderer:
    - Uses CSS colors from text blocks
    - Uses CSS font-size for TTF rendering
    - Uses CSS margin-left for indentation
    - Skips blocks with `display:none` (is_hidden flag)
    - Fixed TTF word wrapping (was going off-screen):
      - Estimates char width based on font size (not fixed 8px)
      - Renders word-by-word with wrap check
      - Wraps to next line if word would exceed right edge
  - `user/lib/crt0.S` - Added memcpy, memmove, memset implementations:
    - GCC generates calls to these for struct copies with -O3
    - Assembly implementations for AArch64
- **Bug fixes:**
  - CSS parser infinite loop on Wikipedia - selectors starting with `:` caused hang
  - Added pseudo-selector skipping (`:hover`, `:not()`, `::before`, etc.)
  - Added safety: skip unknown characters, handle malformed rules gracefully
  - Links were untappable - href wasn't being stored/passed through
- **Wikipedia now renders!** (was hanging before)
- **Text wraps properly** (was going off-screen)
- **Reduced whitespace** (was adding double newlines everywhere)
- **Links clickable again** (href properly stored in DOM and passed to renderer)
- **Achievement**: CSS engine complete! Wikipedia is usable!

### Session 38
- **Image Viewer (`/bin/viewer`) - View images from the file manager!**
  - Added in previous commit (e739b6b)
  - Supports PNG, JPG, BMP via stb_image
  - Center image in window, scale if too large
  - Arrow keys or click to navigate prev/next in same directory
- **Music Player - Single file mode!**
  - Can now open individual audio files: `music /path/to/song.mp3`
  - Supports both MP3 and WAV formats
  - When opened with file argument:
    - Enters compact "Now Playing" mode (350x200 window)
    - Shows filename centered with playback controls
    - Just Play/Pause button (no prev/next)
    - Auto-plays immediately on launch
  - When opened without arguments: Album browser mode (existing behavior)
  - New `play_file()` function handles both MP3 and WAV decoding
  - WAV support: Parses RIFF header, extracts PCM, handles mono→stereo conversion
  - `ends_with()` helper for case-insensitive extension checking
  - Prep work for file manager associations (double-click .mp3/.wav opens Music player)
- **Achievement**: Music player ready for file associations!

- **File Manager - Default App Associations!**
  - Double-click files to open them with the appropriate app
  - Smart file type detection:
    - Image extensions (`.png`, `.jpg`, `.jpeg`, `.bmp`, `.gif`) → `/bin/viewer`
    - Audio extensions (`.mp3`, `.wav`) → `/bin/music`
    - Everything else: content-based text detection
  - `is_text_file()` - reads first 512 bytes, checks for null bytes and control chars
    - Null byte = binary file
    - >10% non-printable chars = binary file
    - Otherwise = text file → `/bin/textedit`
  - No giant extension list needed - just 9 media extensions, rest is auto-detected
  - Binary files silently ignored (no app association)
- **Added `spawn_args()` to kernel API:**
  - Like `spawn()` but with argc/argv support
  - Non-blocking: parent continues immediately
  - Files app now opens files concurrently (can open multiple at once)
  - `kernel/kapi.c`, `kernel/kapi.h`, `user/lib/vibe.h` updated
- **Achievement**: File associations complete! Double-click any file to open it!

- **TextEdit - Unsaved Changes Warning:**
  - Shows dialog when closing with unsaved changes
  - "You have unsaved changes. Save before closing?"
  - Three buttons: [Save] [Don't Save] [Cancel]
  - Keyboard shortcuts: S/Enter=Save, D/N=Don't Save, Esc=Cancel
  - Mouse hover highlighting on buttons
  - Smart flow: if no filename, opens Save As first, then closes after save
  - `pending_close` state tracks waiting for save to complete
- **Achievement**: TextEdit warns before losing work!

### Session 39
- **V1 COMPLETE - Raspberry Pi Zero 2W Port Begins!**
- **Hardware Abstraction Layer (HAL) created:**
  - `kernel/hal/hal.h` - Common interface for platform-specific hardware
  - `kernel/hal/qemu/` - QEMU virt machine implementations
    - `fb.c` - ramfb via fw_cfg
    - `serial.c` - PL011 UART
    - `platform.c` - Platform info
  - `kernel/hal/pizero2w/` - Raspberry Pi Zero 2W implementations
    - `fb.c` - VideoCore mailbox framebuffer
    - `serial.c` - Mini UART (GPIO 14/15)
    - `platform.c` - Platform info
- **Updated core kernel to use HAL:**
  - `kernel/fb.c` - Now calls `hal_fb_init()` and `hal_fb_get_info()`
  - `kernel/kernel.c` - UART functions now wrap HAL serial calls
- **Pi-specific boot code:**
  - `boot/boot-pi.S` - Entry point for Pi (loads at 0x80000, drops from EL2→EL1)
  - `linker-pi.ld` - Memory layout for Pi (no separate flash region)
- **Build system updated:**
  - `TARGET=qemu` (default) - Builds `build/vibeos.bin`
  - `TARGET=pi` or `make pi` - Builds `build/kernel8.img`
  - HAL files automatically selected based on target
- **SD card installer script:**
  - `scripts/install-pi.sh` - Downloads Pi firmware, formats SD, installs VibeOS
  - `make install-pi DISK=disk5s2` - One-command install
  - Downloads bootcode.bin, start.elf, fixup.dat from official Pi firmware repo
  - Creates config.txt with arm_64bit=1
- **FIRST BOOT ON REAL HARDWARE - IT WORKS!**
  - Pi Zero 2W boots to VibeOS splash screen!
  - VideoCore mailbox framebuffer working
  - Console text rendering working
  - Full boot sequence completes
  - Drops to recovery shell (expected - no disk/keyboard drivers yet)
- **What works on Pi:**
  - Boot sequence (EL2→EL1 transition)
  - FPU initialization
  - Exception vectors
  - Framebuffer via VideoCore mailbox
  - Console and font rendering
  - Memory allocator
  - Printf/kernel init
- **What's missing (expected - need new drivers):**
  - Block device (need SDHCI driver for SD card)
  - Keyboard/Mouse (need USB HID stack)
  - Sound/Network (skip for v1 Pi port)
- **Achievement**: VibeOS boots on real hardware! First try!

### Session 40
- **USB Host Driver for Raspberry Pi Zero 2W!**
- **DWC2 (DesignWare USB 2.0) controller driver (`kernel/hal/pizero2w/usb.c`):**
  - ~1100 lines of bare-metal USB host implementation
  - Based on Linux dwc2 driver documentation (generated via Gemini analysis)
  - Full register definitions for Global, Host, and Channel registers
  - Slave mode (no DMA) - CPU handles FIFO read/write
- **USB initialization sequence:**
  - Power on USB controller via VideoCore mailbox (device ID 3)
  - Core soft reset (wait for AHBIDLE, trigger CSFTRST, wait for completion)
  - PHY configuration (UTMI+ 8-bit interface, no PHYSEL for Pi's HS PHY)
  - Force host mode via GUSBCFG
  - FIFO sizing (RxFIFO 256 words, TxFIFO 256 words each)
  - Frame interval configuration (60MHz PHY clock, HFIR=60000)
- **Port control:**
  - VBUS power on via HPRT0.PRTPWR
  - Device connection detection via HPRT0.PRTCONNSTS
  - Port reset (50ms assert, then deassert)
  - Speed detection from HPRT0.PRTSPD (Full Speed detected)
- **Control transfers (SETUP/DATA/STATUS):**
  - Channel-based transfers using HCCHAR, HCTSIZ, HCINT
  - Proper NAK retry handling with channel re-enable
  - Multi-packet IN transfers (re-enable channel after each packet)
  - FIFO read/write with GRXSTSP status parsing
- **USB enumeration working:**
  - GET_DESCRIPTOR (device) - VID/PID and max packet size
  - SET_ADDRESS - assign device address 1
  - GET_DESCRIPTOR (configuration) - full config with interfaces
  - SET_CONFIGURATION - activate device
  - HID keyboard detected!
- **Key debugging insights (with help from Gemini):**
  - **Babble error cause #1:** MPS=8 for Full Speed devices - should be 64!
    - FS devices send up to 64 bytes per packet, 8 causes babble on byte 9
  - **Babble error cause #2:** Wrong PHY clock setting
    - Pi uses UTMI+ PHY at 60MHz, not dedicated 48MHz FS PHY
    - FSLSPCLKSEL must be 0 (30/60MHz), not 1 (48MHz)
  - **Multi-packet timeout:** Need to re-enable channel after each IN packet
    - Slave mode requires explicit channel re-enable for each packet
    - Fixed by re-enabling on ACK and IN_COMPLETE events
- **HAL integration:**
  - Added `hal_usb_init()` and `hal_usb_keyboard_poll()` to hal.h
  - QEMU stub returns -1 (uses virtio input instead)
  - Pi calls USB init during kernel startup
- **What works:**
  - Device detection and enumeration
  - Reading device/config descriptors
  - HID keyboard identification
- **What's next:**
  - Interrupt transfers for HID reports (keyboard input)
  - HID report parsing
  - Wire up to HAL keyboard interface
- **New files:**
  - `kernel/hal/pizero2w/usb.c` - DWC2 USB host driver
  - `docs/rpi_usb.md` - Comprehensive USB implementation documentation
- **Achievement**: USB enumeration works on real Pi hardware! Keyboard detected!

### Session 41
- **INTERRUPTS WORK ON REAL PI HARDWARE!**
- **Two-level interrupt controller architecture implemented:**
  - **ARM Local Controller (0x40000000):** Root controller for BCM2836/2837
    - Per-core interrupt routing
    - LOCAL_CONTROL, LOCAL_PRESCALER for timer config
    - LOCAL_TIMER_INT_CTRL0 for enabling timer IRQs
    - LOCAL_IRQ_PENDING0 for reading pending interrupts
    - Bit 1 = CNTPNSIRQ (ARM Generic Timer)
    - Bit 8 = GPU interrupt (from BCM2835 IC)
  - **BCM2835 IC (0x3F00B200):** GPU peripheral interrupts
    - Three banks: Bank 0 (ARM local), Bank 1 (GPU 0-31), Bank 2 (GPU 32-63)
    - Shortcut logic: high-priority IRQs (bits 10-20) bypass bank summary
    - USB is Bank 1 IRQ 9, shortcut to bit 11
    - ENABLE_1/DISABLE_1 for Bank 1 interrupts
- **HAL properly refactored:**
  - `kernel/hal/qemu/irq.c` - GIC-400 driver with handle_irq()
  - `kernel/hal/pizero2w/irq.c` - BCM2836+BCM2835 driver with handle_irq()
  - `kernel/irq.c` - Thin wrappers + shared exception handlers
  - No more #ifdef spaghetti - clean platform separation
- **ARM Generic Timer working on Pi:**
  - Same timer as QEMU (CNTPNSIRQ)
  - Routes through ARM Local Controller bit 1
  - 100ms interval, prints "Interrupt!" every second (10 ticks)
- **GPIO driver added** (`kernel/hal/pizero2w/gpio.c`):
  - GPIO 47 (ACT LED) configured as output
  - LED toggle on each interrupt (didn't visibly blink but code is correct)
- **Clean-room implementation:**
  - BCM2835 IC spec generated from Linux irq-bcm2835.c
  - ARM Local Controller spec generated from Linux irq-bcm2836.c
  - No GPL code copied - just hardware register documentation
- **What this enables:**
  - USB interrupts for HID keyboard (IRQ 17 = Bank 1 bit 9)
  - Proper interrupt-driven USB driver (no polling!)
  - Foundation for DMA-based USB transfers
- **Files created:**
  - `kernel/hal/pizero2w/irq.c` - Pi interrupt controller driver
  - `kernel/hal/pizero2w/gpio.c` - Pi GPIO driver
  - `kernel/hal/qemu/irq.c` - QEMU GIC driver (moved from kernel/irq.c)
- **Achievement**: Interrupts fire on real Pi hardware! Timer ticks, handler runs!

### Session 42
- **QEMU raspi3b debugging infrastructure for USB**
- **Problem:** USB keyboard works partially on real Pi (enumerates, finds keyboard, no input) but very hard to debug with only serial output
- **Solution:** Run Pi build in QEMU raspi3b with same DWC2 controller for easier debugging
- **Pi serial driver rewritten:**
  - Switched from Mini UART (0x3F215000) to PL011 (0x3F201000)
  - PL011 works on both real Pi (with serial cable) and QEMU raspi3b
  - QEMU's `-serial stdio` connects to PL011
- **Printf output control:**
  - Added `PRINTF_UART` compile flag
  - `PRINTF=uart` (default) sends printf to UART
  - `PRINTF=screen` sends printf to framebuffer console
  - Same default for both targets (not target-dependent)
- **Fixed QEMU hang during USB init:**
  - Original `usleep()`/`msleep()` used nop loops calibrated for 1GHz Pi
  - M2 laptop runs QEMU much faster - "100ms" delay was microseconds
  - QEMU couldn't keep up with rapid mailbox/register polling
  - **Fix:** Use ARM generic timer (`cntpct_el0`, `cntfrq_el0`) for real delays
- **Added `make run-pi` target:**
  - Builds with `TARGET=pi`
  - Runs in QEMU raspi3b with USB keyboard attached
  - One command to test USB changes
- **Disabled noisy interrupt logging** (was printing every 100ms)
- **USB debugging findings:**
  - QEMU raspi3b boots, initializes USB, detects device as Full Speed
  - Fails at first GET_DESCRIPTOR with: `usb_generic_handle_packet: ctrl buffer too small (43551 > 4096)`
  - 43551 = 0xAA1F = garbage in wLength field
  - QEMU is stricter than real Pi - catches malformed packets that real hardware tolerates
  - Real Pi: enumeration succeeds but keyboard input doesn't work
  - Different failure points, possibly related root cause
- **Files modified:**
  - `kernel/hal/pizero2w/serial.c` - PL011 instead of Mini UART
  - `kernel/hal/pizero2w/usb.c` - Timer-based delays, debug prints
  - `kernel/hal/pizero2w/irq.c` - Disabled interrupt spam
  - `kernel/printf.c` - PRINTF_UART flag support
  - `Makefile` - PRINTF option, `run-pi` target
- **Achievement**: Can now debug USB driver in QEMU with full serial output!

### Session 43
- **USB KEYBOARD WORKING IN QEMU!**
- **Root cause found:** QEMU's DWC2 emulation only supports DMA mode, not slave mode
  - Slave mode = CPU manually reads/writes FIFOs
  - DMA mode = Controller reads/writes memory directly
  - Our original driver used slave mode, QEMU ignored FIFO operations
- **DMA mode implementation:**
  - Enabled `GAHBCFG_DMA_EN` (bit 5) in AHB configuration
  - Added 32-byte aligned DMA buffers for transfers
  - Set `HCDMA(ch)` register to bus address instead of writing to `FIFO(ch)`
  - Removed all FIFO read/write code
  - Simplified wait logic (no more RXFLVL polling)
- **USB hub support added:**
  - QEMU raspi3b has virtual 8-port root hub
  - Keyboard connected behind hub, not directly to root
  - Added hub descriptor fetching
  - Added port power on, status check, reset sequence
  - Recursive enumeration through hub ports
  - Found keyboard on hub port 1, address 2
- **HID keyboard interrupt transfers:**
  - Implemented interrupt IN transfers using DMA
  - 8-byte HID boot keyboard reports working
  - Data toggle (DATA0/DATA1) alternation between transfers
  - NAK handling (no data available, not an error)
- **HID report parsing:**
  - Created `kernel/hal/pizero2w/keyboard.c`
  - USB HID scancodes to ASCII conversion
  - Modifier key support (Shift for uppercase/symbols)
  - Ctrl+key combinations (Ctrl+A = 1, etc.)
  - Arrow keys and special keys
- **Keyboard integration:**
  - Modified `kernel/keyboard.c` to fall back to HAL when no virtio keyboard
  - `keyboard_getc()` calls `hal_keyboard_getc()` when `kbd_base == NULL`
  - Works seamlessly - shell accepts keyboard input!
- **Debug output improvements:**
  - Added debug levels (0=errors, 1=key events, 2=verbose)
  - `usb_info()` for important status, `usb_debug()` for verbose
  - Much cleaner output for real hardware testing
- **Real Pi status:**
  - USB enumeration works (hub found, keyboard found)
  - Interrupt transfers return 0 bytes (NAK)
  - Likely cache coherency issue - QEMU doesn't emulate caches
  - DMA buffer might need uncached memory or cache flush/invalidate
  - Deferred to future session
- **Files changed:**
  - `kernel/hal/pizero2w/usb.c` - DMA mode, hub support, interrupt transfers
  - `kernel/hal/pizero2w/keyboard.c` - New file, USB HID to ASCII
  - `kernel/keyboard.c` - HAL fallback for Pi
- **Achievement**: USB keyboard fully working in QEMU raspi3b! Type in shell!

### Session 44
- **Framebuffer performance optimization**
- **Problem:** Pi Zero 2W was painfully slow - every pixel operation was byte-by-byte
- **Root cause:** `memcpy`, `memmove`, `memset` all used byte loops
  - Console scroll = 6+ million byte operations per scroll
  - Screen clear = 3+ million byte operations
- **Optimizations implemented:**
  - `memcpy` - 64-bit copies when 8-byte aligned (8x faster)
  - `memset` - 64-bit stores when aligned (8x faster)
  - `memmove` - uses fast memcpy path when no overlap, 64-bit backward copy
  - Added `memset32` - fills with 32-bit pattern using 64-bit stores (2 pixels/op)
  - `fb_clear` - now uses `memset32`
  - `fb_fill_rect` - uses `memset32` per row instead of pixel loops
  - `fb_draw_char` - unrolled 8-pixel rows, removed per-pixel bounds checks
  - `scroll_up` - uses `memmove` + `memset32`
- **Result:** ~8-12x faster framebuffer operations on Pi
- **Files changed:**
  - `kernel/string.c` - optimized memcpy/memmove/memset, added memset32
  - `kernel/string.h` - added memset32 declaration
  - `kernel/fb.c` - use memset32, optimized fb_draw_char
  - `kernel/console.c` - use memmove/memset32 for scroll
- **TODO:** Apply same optimizations to userspace `gfx.h` for desktop/GUI apps
Session 44: USB Keyboard Working on Real Pi Hardware!

  Goal: Fix USB keyboard on Raspberry Pi Zero 2W (worked in QEMU, not on real hardware)

  Issues Fixed:

  1. Port Power Being Disabled (HPRT0 bug)
    - Port interrupt handler was writing only W1C bits, clearing PRTPWR
    - Fix: Preserve RW bits when clearing W1C status bits, mask out PRTENA
  2. Interrupt Storm / Too Fast Polling
    - SOF interrupt firing 1000x/sec was overwhelming the Pi
    - Removed SOF-based polling, switched to timer-tick based (every 10ms)
    - Reduced channel interrupt mask to just CHHLTD + errors
  3. CPU Cache Coherency (THE BIG ONE)
    - QEMU doesn't emulate caches, so DMA "just works"
    - Real Pi: CPU writes sit in L1 cache, DMA reads stale RAM = garbage
    - Added clean_data_cache_range() before OUT/SETUP transfers
    - Added invalidate_data_cache_range() before/after IN transfers
    - Increased DMA buffer alignment from 32 to 64 bytes (Cortex-A53 cache line)
  4. Missing SET_PROTOCOL
    - HID keyboards default to Report Protocol (complex reports)
    - Added SET_PROTOCOL(0) to switch to Boot Protocol (simple 8-byte reports)
    - Added SET_IDLE(0) to only report on key state changes

  Key Learnings:

  - Cache coherency is critical for DMA on real ARM hardware
  - dc cvac cleans cache to point of coherency (flush to RAM)
  - dc ivac invalidates cache (force re-read from RAM)
  - Always clean before DMA reads from buffer, invalidate before CPU reads DMA results

  Files Modified:

  - kernel/hal/pizero2w/usb.c - Cache maintenance, SET_PROTOCOL, interrupt handling
  - kernel/hal/pizero2w/irq.c - Timer-based keyboard polling
  - kernel/hal/hal.h - Added hal_usb_keyboard_tick()

  USB keyboard now works on real Raspberry Pi Zero 2W hardware!

### Session 45
- **USB Driver Refactor** - Split 2256-line monolithic driver into maintainable modules
- **Problem:** USB driver was too large and had reliability issues
  - Printf calls in ISR causing timing problems
  - Single keyboard buffer could drop fast keypresses
  - No recovery from stuck transfers
- **Refactored into `kernel/hal/pizero2w/usb/` directory:**
  - `dwc2_regs.h` (~200 lines) - Register definitions
  - `usb_types.h` (~180 lines) - USB descriptors and structs
  - `dwc2_core.c/h` (~400 lines) - PHY, reset, cache ops, mailbox, port control
  - `usb_transfer.c/h` (~290 lines) - Control transfers, DMA handling
  - `usb_enum.c/h` (~380 lines) - Device enumeration, hub support
  - `usb_hid.c/h` (~340 lines) - Keyboard ISR, polling, ring buffer
  - `usb.c` (~200 lines) - Init wrapper
- **New reliability features:**
  1. **Ring buffer** (16 reports) - Fast typing won't drop keys
  2. **Debug counters** - No printf in ISR, safe atomic counters
  3. **Watchdog** - Recovers stuck transfers after 50ms timeout
  4. **`usbstats` command** - Shows IRQ/transfer/error statistics
- **Files changed:**
  - `kernel/hal/pizero2w/usb/` - New directory with split USB driver
  - `kernel/shell.c` - Added `usbstats` recovery command
  - `Makefile` - Added USB subdirectory compilation rules
- **Stats output:** `[USB-STATS] IRQ=X KBD=X data=X NAK=X err=X restart=X port=X watchdog=X`

### Session 46
- **SD Card & FAT32 Performance Optimization**
- **Problem:** SD card operations on Pi were painfully slow
  - Single-sector commands: Each 512-byte read = separate CMD17 command
  - No FAT caching: Every cluster lookup read a sector from disk
  - Low clock speed: Running at 25MHz instead of 50MHz
- **Optimizations implemented:**
  1. **Multi-block SD commands** (`kernel/hal/pizero2w/emmc.c`)
     - Added `read_data_blocks()` / `write_data_blocks()` for multi-sector transfers
     - CMD18 (READ_MULTIPLE_BLOCK) with auto-CMD12 for reads
     - CMD25 (WRITE_MULTIPLE_BLOCK) with auto-CMD12 for writes
     - Single-sector still uses CMD17/CMD24 for compatibility
  2. **High Speed mode** (`kernel/hal/pizero2w/emmc.c`)
     - Added CMD6 (SWITCH_FUNC) to enable High Speed mode after init
     - Clock increased from 25MHz to 50MHz (2x improvement)
  3. **FAT sector cache** (`kernel/fat32.c`)
     - 8-entry LRU cache for FAT table sectors
     - `fat_read_sector_cached()` - returns cached data or reads from disk
     - `fat_next_cluster()` now uses cached reads
     - `fat_cache_invalidate()` called on FAT writes to maintain coherency
- **Performance improvement:**
  - Reading 8 sectors: 8 commands → 1 command
  - FAT chain traversal (10 clusters): 10 disk reads → 1-2 reads (cache hits)
  - Raw transfer speed: 2x (50MHz vs 25MHz)
- **Files changed:**
  - `kernel/hal/pizero2w/emmc.c` - Multi-block commands, High Speed mode
  - `kernel/fat32.c` - FAT sector cache with LRU eviction
- **Result:** File operations dramatically faster on real Pi hardware

  ### Session 46
  - **MASSIVE COREUTILS EXPANSION - 27 new commands!**
  - **File Operations (11):**
    - `cp` - copy files/directories with `-r` recursive support
    - `mv` - move/rename (uses rename for same-dir, copy+delete for cross-dir)
    - `head` / `tail` - first/last N lines (`-n` flag)
    - `wc` - word/line/char count (`-lwc` flags)
    - `grep` - simple substring search (`-i` case insensitive, `-n` line numbers, `-v` invert)
    - `find` - find files by name pattern (`-name`, `-type f|d`)
    - `stat` - file size and type
    - `du` - disk usage with `-h` human-readable, `-s` summary
    - `df` - filesystem free space
    - `hexdump` - hex dump with `-C` canonical format
  - **System Info (7):**
    - `ps` - process list showing PID, state, name
    - `kill` - terminate process by PID (real implementation!)
    - `free` - memory usage (`-h`, `-m`, `-k` flags)
    - `uname` - system info (`-a`, `-s`, `-n`, `-r`, `-m`)
    - `hostname` - print hostname
    - `lscpu` - CPU info (model, frequency, cores, RAM)
    - `lsusb` - USB device list (Pi shows real devices, QEMU shows none)
  - **Misc (9):**
    - `sleep` - sleep N seconds
    - `seq` - print number sequences
    - `which` - find command in /bin
    - `whoami` - print current user ("user")
    - `yes` - repeat string forever
    - `clear` - clear screen
    - `basename` / `dirname` - path manipulation
  - **Kernel changes for kill/lscpu/lsusb:**
    - Added `process_kill(int pid)` to kernel/process.c
    - Added HAL functions: `hal_get_cpu_name()`, `hal_get_cpu_freq_mhz()`, `hal_get_cpu_cores()`
    - Added HAL functions: `hal_usb_get_device_count()`, `hal_usb_get_device_info()`
    - QEMU: returns Cortex-A72 @ 1500MHz, 1 core, no USB devices
    - Pi: returns Cortex-A53 @ 1000MHz, 4 cores, real USB device list
  - **Files created:** 27 new files in user/bin/
  - **Files modified:** Makefile, kernel/process.c, kernel/process.h, kernel/kapi.c, kernel/kapi.h, user/lib/vibe.h, kernel/hal/hal.h, kernel/hal/qemu/platform.c, kernel/hal/pizero2w/platform.c
  - **Achievement**: VibeOS now has a proper Unix-like coreutils suite!

### Session 47
- **Preemptive Multitasking Implementation**
- **Goal:** Switch from cooperative (apps must call yield()) to preemptive (timer forces context switches)
- **Initial approach (WRONG):** Make yield() a no-op, rely entirely on timer preemption
- **Problem discovered:** With 10 apps running, system crawled to a halt
  - Each process got 50ms timeslice
  - Full round-robin = 10 × 50ms = 500ms per cycle
  - Apps spinning in event loops burned their full slice doing nothing
  - With cooperative, apps yielded immediately when waiting for input
- **Key insight:** Real OSes (Linux) use BOTH mechanisms:
  1. Voluntary yield - apps yield when waiting (fast path, primary)
  2. Preemptive backup - timer forces switch for CPU hogs (safety net)
- **Implementation details:**
  - Expanded `cpu_context_t` to save ALL registers (x0-x30, sp, pc, pstate, FPU)
  - Modified `vectors.S` IRQ handler to save/restore full context
  - Added `CONTEXT_OFFSET` (0x50) - cpu_context_t offset within process_t
  - Changed `context.S` to use `eret` instead of `ret` (restores PSTATE/IRQ state)
  - Timer fires at 100Hz (10ms) for audio/responsiveness
  - Preemption check every 5 ticks (50ms timeslice)
  - `process_schedule_from_irq()` called from timer - updates current_process
- **Bugs fixed along the way:**
  - Kernel panic: process slot with invalid context (sp=0) - added safety check
  - Process exits immediately: pstate missing EL1h mode bits - save as `DAIF | 0x5`
  - IRQs disabled after voluntary switch: `ret` doesn't restore PSTATE - use `eret`
  - Context corruption: vectors.S writing to offset 0 instead of 0x50
- **Final fix:** Restored yield() to actually switch processes
  - `kapi.yield = process_yield` (not noop)
  - Apps that yield() switch immediately
  - Apps that don't yield get preempted after 50ms
- **Files modified:**
  - `kernel/process.c` - process_schedule_from_irq(), kernel_context global
  - `kernel/process.h` - expanded cpu_context_t
  - `kernel/vectors.S` - full context save/restore, CONTEXT_OFFSET
  - `kernel/context.S` - eret instead of ret, proper pstate save
  - `kernel/hal/qemu/irq.c` - timer calls scheduler every 5 ticks
  - `kernel/hal/pizero2w/irq.c` - same for Pi
  - `kernel/kapi.c` - yield = process_yield (not noop)
- **Result:** Preemptive multitasking with cooperative fast path - best of both worlds!

### Session 48
- **Display Performance Optimizations for Raspberry Pi**
- **Problem:** vim and other text apps were painfully slow on Pi, but snake/tetris were fine
- **Root cause analysis:**
  - Snake/Tetris: only update ~200 changed pixels per frame
  - Vim: redraws entire screen (~480,000 pixels) on every keystroke
  - Each `putc(' ')` for clearing = 128 pixel writes
- **Optimizations implemented:**
  1. **Fast console clearing functions** (`kernel/console.c`)
     - `console_clear_to_eol()` - uses `fb_fill_rect` instead of putc loop
     - `console_clear_region(row, col, w, h)` - fast rectangular clear
     - Exposed via kapi for userspace programs
  2. **Pi GPU hardware scroll** (`kernel/hal/pizero2w/fb.c`)
     - Virtual framebuffer 2x physical height (1200 vs 600 pixels)
     - `hal_fb_set_scroll_offset(y)` - instant GPU register update
     - Circular buffer approach: scroll ~37 lines before needing to copy
     - Wrap copies visible portion back to top, resets offset
  3. **Vim optimizations** (`user/bin/vim.c`)
     - `redraw_screen()` clears all rows with one `clear_region` call
     - `draw_line()` uses `clear_line()` before drawing text
     - Status/command line use fast clear instead of putc loops
     - Fixed scroll bug: trailing space on status line triggered unwanted scroll
- **Hardware scroll bugs fixed:**
  - "Sideways scrolling" - memmove used `fb_base + scroll_offset` instead of `fb_base + scroll_offset * fb_width`
  - The offset is Y pixels, must multiply by width to get linear pixel index
- **Framebuffer bounds fix** (`kernel/fb.c`)
  - Added `fb_buffer_height` for virtual buffer size
  - Drawing functions now clip to buffer height, not just visible height
  - Allows drawing in scroll area above/below visible region
- **Files modified:**
  - `kernel/hal/hal.h` - added `hal_fb_set_scroll_offset()`, `hal_fb_get_virtual_height()`
  - `kernel/hal/pizero2w/fb.c` - 2x virtual height, hardware scroll via mailbox
  - `kernel/hal/qemu/fb.c` - stub returns -1 (no hardware scroll)
  - `kernel/fb.c` - fb_buffer_height for bounds checking
  - `kernel/console.c` - hardware scroll, fast clear functions
  - `kernel/console.h` - new function declarations
  - `kernel/kapi.c/h` - exposed clear functions
  - `user/lib/vibe.h` - kapi struct updated
  - `user/bin/vim.c` - use fast clear, fix scroll bugs
- **Performance improvement:**
  - Console scroll: memmove every line → GPU offset update (37x fewer copies)
  - Vim clear: ~100 putc per line → 1 fb_fill_rect call
  - Overall: text apps significantly faster on Pi

### Session 49
- **Kernel Ring Buffer and dmesg**
- **Goal:** Add a kernel logging system like Linux dmesg
- **Implementation:**
  1. **Kernel ring buffer** (`kernel/klog.c`, `kernel/klog.h`)
     - 64KB static circular buffer
     - `klog_putc()` writes one char, wraps around when full
     - `klog_read()` reads from any offset, handles wrap-around
     - `klog_size()` returns current log size
  2. **Printf integration** (`kernel/printf.c`)
     - Added `klog_putc(c)` to `printf_putchar()` - always logs
     - Happens in addition to UART/screen output, not instead of
     - No compile flags needed, fully automatic
  3. **Early initialization** (`kernel/kernel.c`)
     - `klog_init()` called before `memory_init()`
     - Uses static buffer, no malloc needed
     - Captures all boot messages from first printf onward
  4. **kapi exposure** (`kernel/kapi.c`, `kernel/kapi.h`, `user/lib/vibe.h`)
     - `klog_read(buf, offset, size)` - read log data
     - `klog_size()` - get total logged bytes
  5. **dmesg program** (`user/bin/dmesg.c`)
     - Interactive scrollable viewer (default)
     - `-n` flag for non-interactive dump
     - Controls: j/k arrows scroll, g/G start/end, u/d page, q quit
     - Status bar shows position (e.g., "45-68/120")
- **Files created:**
  - `kernel/klog.h` - ring buffer header
  - `kernel/klog.c` - ring buffer implementation
  - `user/bin/dmesg.c` - log viewer program
- **Files modified:**
  - `kernel/printf.c` - added klog_putc() call
  - `kernel/kernel.c` - klog_init() early in boot
  - `kernel/kapi.c/h` - exposed klog functions
  - `user/lib/vibe.h` - kapi struct updated
  - `Makefile` - added dmesg to USER_PROGS
