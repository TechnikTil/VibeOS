### Session 50
- **USB Hub Fix - Full-Speed Device Behind High-Speed Hub**
- **Problem:** FS keyboard on HS hub port caused infinite NYET loop during enumeration
- **Symptoms:**
  - Hub detected as HS, enumerated fine
  - Device on hub port 4 detected as FS
  - Split transactions enabled (required for FS behind HS hub)
  - Start-split got ACK (hub TT accepted transaction)
  - Complete-split got NYET forever (TT never completed downstream transaction)
- **Investigation:**
  1. First suspected timing - USB msleep() was using ARM generic timer (`cntfrq_el0`/`cntpct_el0`) which wasn't working
  2. Fixed by enabling interrupts before USB init, using kernel's `sleep_ms()` instead
  3. Timing was now correct (~10ms delays) but NYET loop persisted
  4. Split transaction state machine looked correct per USB spec
  5. TT just never completed the FS transaction to downstream device
- **Solution:** Force Full-Speed only mode with `HCFG_FSLSUPP`
  - Set `HCFG = HCFG_FSLSPCLKSEL_30_60 | HCFG_FSLSUPP`
  - Hub now connects at FS instead of HS
  - No split transactions needed - direct FS communication
  - Trade-off: 12 Mbps instead of 480 Mbps (irrelevant for keyboard/mouse)
- **Other fixes along the way:**
  - `kernel/kernel.c` - Enable interrupts before USB init (was too late)
  - `kernel/hal/pizero2w/usb/dwc2_core.c` - msleep() now uses kernel's sleep_ms()
  - `kernel/hal/pizero2w/usb/usb_hid.c` - Added NYET retry limits and frame-based waiting for IRQ path
- **Files modified:**
  - `kernel/kernel.c` - hal_irq_enable() before hal_usb_init()
  - `kernel/hal/pizero2w/usb/dwc2_core.c` - HCFG_FSLSUPP, fixed msleep/usleep
  - `kernel/hal/pizero2w/usb/usb_hid.c` - split transaction state tracking
  - `kernel/hal/pizero2w/usb/usb_hid.h` - added kbd_nyet_count to stats
- **Lesson learned:** For HID devices, FS is perfectly adequate. HS split transactions add complexity with little benefit for low-bandwidth devices.

### Session 51
- **USB MOUSE WORKING ON RASPBERRY PI!**
- **Goal:** Get USB mouse working on Pi to enable desktop GUI on real hardware
- **USB HID Mouse Driver (`kernel/hal/pizero2w/usb/`):**
  1. **State tracking** (`usb_types.h`)
     - Added `mouse_addr`, `mouse_ep`, `mouse_mps`, `mouse_interval` to usb_state_t
  2. **Enumeration** (`usb_enum.c`)
     - Fixed interface/endpoint parsing for combo devices (keyboard+keyboard+mouse)
     - Track `current_iface_type` to correctly associate endpoints with their interface
     - Only captures first keyboard and first mouse (skips duplicates)
     - Sends SET_PROTOCOL(Boot Protocol) and SET_IDLE to mouse interface
  3. **Interrupt transfers** (`usb_hid.c`)
     - Channel 2 for mouse (channel 1 is keyboard)
     - Mouse ring buffer (32 reports) for ISR→main loop communication
     - Full split transaction support (same as keyboard)
     - Mouse stats: `mouse_irq_count`, `mouse_data_count`, `mouse_nak_count`, `mouse_error_count`
  4. **Init** (`usb.c`)
     - Enable channel 2 interrupts if mouse detected
     - Call `usb_start_mouse_transfer()` after enumeration
- **HAL Mouse Integration:**
  1. **Pi HAL driver** (`kernel/hal/pizero2w/mouse.c`) - NEW FILE
     - Implements `hal_mouse_init()`, `hal_mouse_get_state()`
     - Polls USB HID reports via `hal_usb_mouse_poll()`
     - Converts relative deltas to absolute screen position
     - Mouse sensitivity scaling (2x multiplier)
  2. **Virtio fallback** (`kernel/mouse.c`)
     - Added HAL fallback when virtio-tablet not found
     - `mouse_init()` calls `hal_mouse_init()` if no virtio
     - `mouse_get_screen_pos()` / `mouse_get_buttons()` check `mouse_base` and use HAL
  3. **QEMU stubs** (`kernel/hal/qemu/platform.c`)
     - Added `hal_mouse_*` stubs for linking (never called, virtio used)
- **Test program** (`user/bin/mousetest.c`)
  - Simple graphical test showing mouse position, cursor, button clicks
  - Crosshair cursor follows mouse movement
  - Left (green) and right (red) click indicators
- **USB Boot Mouse Protocol:**
  - Byte 0: Buttons (bit 0=left, bit 1=right, bit 2=middle)
  - Byte 1: X displacement (signed 8-bit)
  - Byte 2: Y displacement (signed 8-bit)
- **Files created:**
  - `kernel/hal/pizero2w/mouse.c` - Pi USB mouse HAL driver
  - `user/bin/mousetest.c` - Mouse test program
- **Files modified:**
  - `kernel/hal/pizero2w/usb/usb_types.h` - mouse state fields
  - `kernel/hal/pizero2w/usb/usb_enum.c` - mouse enumeration, interface tracking
  - `kernel/hal/pizero2w/usb/usb_hid.c` - mouse transfers, ring buffer, stats
  - `kernel/hal/pizero2w/usb/usb_hid.h` - mouse stats struct, function declarations
  - `kernel/hal/pizero2w/usb/usb.c` - mouse init, channel 2 IRQ enable
  - `kernel/mouse.c` - HAL fallback for Pi
  - `kernel/hal/qemu/platform.c` - HAL mouse stubs
  - `Makefile` - added mousetest to USER_PROGS
- **Achievement**: Full mouse support on Raspberry Pi! Desktop GUI now possible on real hardware!

---

## Session 41 - Desktop Performance Optimization

- **MASSIVE DESKTOP PERFORMANCE IMPROVEMENTS FOR PI**
- **Problem:** Desktop was unusably slow on Pi - redrawing entire 800x600 screen every frame with pixel-by-pixel loops
- **Root Causes Found:**
  1. Full redraw every frame (even when nothing changed)
  2. Pixel-by-pixel nested loops everywhere
  3. 480KB memcpy every flip (even when nothing changed)
  4. Pi has hardware scroll but it was unused

### Phase 1: Skip Static Frames
- Added `needs_redraw` flag - desktop only redraws when something actually changed
- Added `request_redraw()` helper called by window management, menus, etc.
- No work when idle

### Phase 2: 64-bit Graphics Primitives (`user/lib/vibe.h`, `user/lib/gfx.h`)
- Added `memset32_fast()` - fills 2 pixels per 64-bit store
- Added `memcpy64()` - copies 8 bytes per operation
- Optimized `gfx_fill_rect()`, `gfx_draw_hline()`, `gfx_fill_pattern()`
- Optimized window content copy (row-based memcpy64 instead of pixel loop)
- Optimized title bar stripes drawing

### Phase 3: Hardware Double Buffering (Pi Only)
- Added kernel API: `fb_has_hw_double_buffer()`, `fb_flip()`, `fb_get_backbuffer()`
- Pi's 2x virtual framebuffer now used for zero-copy flipping
- `flip_buffer()` on Pi just changes GPU scroll offset (instant)
- QEMU falls back to fast 64-bit memcpy

### Phase 4: Smarter Redraw
- Tracks dock hover state - only redraws when highlighted icon changes
- Menu/dialog hover triggers redraws only when open
- Avoids full redraw for hover changes outside interactive areas

### Phase 5: Cursor-Only Updates
- When ONLY cursor moved (no other changes):
  - Saves/restores 16x16 cursor background
  - Updates cursor directly on visible buffer
  - Skips full redraw (~512 pixels vs ~480,000)
- `get_visible_buffer()` handles hardware double buffer correctly

### Files Modified:
- `user/bin/desktop.c` - needs_redraw, cursor-only updates, hw double buffer support
- `user/lib/gfx.h` - optimized drawing primitives with 64-bit ops
- `user/lib/vibe.h` - added memset32_fast, memcpy64, new kapi fields
- `kernel/fb.c` / `kernel/fb.h` - hardware double buffer functions
- `kernel/kapi.c` / `kernel/kapi.h` - new fb_flip API

---

## Session 42 - DMA Support for Pi Framebuffer

- **Added DMA (Direct Memory Access) support for Raspberry Pi**
- **Goal:** Offload memory copies from CPU to hardware DMA controller for faster framebuffer operations

### DMA Driver (`kernel/hal/pizero2w/dma.c`)
- BCM2837 DMA controller driver using channel 0 (supports full 2D mode)
- Control blocks are 32-byte aligned, use bus addresses (physical | 0xC0000000)
- Three copy operations:
  - `hal_dma_copy()` - 1D linear memory-to-memory transfer
  - `hal_dma_copy_2d()` - 2D rectangular blit with stride support
  - `hal_dma_fb_copy()` - Full framebuffer copy
- DMA waits for completion synchronously (simpler, avoids race conditions)

### HAL Interface (`kernel/hal/hal.h`)
- Added DMA function declarations
- QEMU stub (`kernel/hal/qemu/dma.c`) uses CPU memcpy as fallback

### Kernel API (`kernel/kapi.h`, `kernel/kapi.c`)
- Exposed DMA functions to userspace: `dma_available()`, `dma_copy()`, `dma_copy_2d()`, `dma_fb_copy()`
- DMA initialized in `kernel.c` after framebuffer init

### Desktop Integration (`user/bin/desktop.c`)
- `flip_buffer()` uses DMA for software fallback path (when HW double buffering unavailable)
- Window content blitting uses DMA 2D copy for fully-visible windows
- Falls back to CPU copy when window is clipped or DMA unavailable

### Technical Details
- DMA base address: 0x3F007000 (Pi Zero 2W / BCM2837)
- Channel spacing: 0x100 bytes per channel
- Uses uncached bus address alias (0xC0000000) for coherent DMA
- Control block structure: TI, SOURCE_AD, DEST_AD, TXFR_LEN, STRIDE, NEXTCONBK
- 2D mode: TXFR_LEN = (YLENGTH << 16) | XLENGTH, STRIDE = (D_STRIDE << 16) | S_STRIDE

### Files Created
- `kernel/hal/pizero2w/dma.c` - Pi DMA controller driver
- `kernel/hal/qemu/dma.c` - QEMU CPU fallback stub

### Files Modified
- `kernel/hal/hal.h` - DMA function declarations
- `kernel/kernel.c` - DMA init call
- `kernel/kapi.h` - DMA kapi struct fields
- `kernel/kapi.c` - DMA function wiring
- `user/lib/vibe.h` - DMA function pointers in userspace kapi
- `user/bin/desktop.c` - DMA integration in flip_buffer and window blit

## Session 41: D-Cache Coherency Fixes for Raspberry Pi

  **Problem**: Pi boot broken after adding MMU with D-cache enabled. System crashed during USB enumeration.

  **Root Causes & Fixes**:

  1. **GPU Mailbox Coherency** - Mailbox buffers shared with GPU lacked cache maintenance:
     - EMMC `prop_buf`: Added `cache_clean()` before send, `cache_invalidate()` after receive
     - Framebuffer `mailbox_buffer`: Same fix
     - USB `mbox_buf`: Same fix

  2. **DMA Control Block** - `dma_cb` in dma.c needed `cache_clean_range()` before DMA starts

  3. **Unsafe Cache Invalidate** - Changed all `dc ivac` to `dc civac` (clean-and-invalidate) because `dc ivac` on dirty lines has undefined behavior on ARM

  4. **DMA Receive Buffer Bug** - `memset()` + `invalidate()` discarded the zeros. Fixed to `memset()` + `clean()` so zeros are flushed to RAM before DMA writes

  5. **USB Polling Timing** - With D-cache, CPU runs faster. Fixed `usleep()` to use DSB barriers for reliable system timer reads. Increased poll delay to 50μs.

  **Result**: Pi boots with D-cache enabled, ~100x faster memory access, USB keyboard/mouse working.

## Session 52: GPIO Driver Expansion & LED Control

**Goal**: Make GPIO driver more robust and general-purpose.

### GPIO Driver Refactor (`kernel/hal/pizero2w/gpio.c`)
- Expanded from LED-only to full GPIO API:
  - `gpio_set_function(pin, func)` - Set any pin to INPUT/OUTPUT/ALT0-5
  - `gpio_get_function(pin)` - Read current function
  - `gpio_set(pin, high)` - Set output level
  - `gpio_get(pin)` - Read input level
  - `gpio_set_pull(pin, pull)` - Set pull-up/down/none for single pin
  - `gpio_set_pull_mask(mask, bank, pull)` - Set pull for multiple pins efficiently
- LED functions now use the general API internally
- Added edge detect register definitions (for future interrupt support)

### Code Deduplication
- EMMC driver (`emmc.c`) now uses shared GPIO API instead of local register access
- `setup_sd_gpio()` reduced from 20 lines to 6 lines

### HAL Updates
- Added GPIO constants to `hal.h`: GPIO_INPUT, GPIO_OUTPUT, GPIO_ALT0-5, GPIO_PULL_*
- Added QEMU stubs in `platform.c` (no-ops since QEMU virt has no GPIO)
- Added `hal_led_status()` to query LED state

### New Userspace Tool (`user/bin/led.c`)
- Replaced old `blink.c` with simpler `led` command
- Usage: `led on`, `led off`, `led status`
- Controls Pi ACT LED (GPIO 29, active-low)

### Debugging Adventure
- Timer tick was toggling LED every 10ms, overriding user commands
- Fixed: GPIO 29 confirmed correct, active-low polarity
- Heartbeat LED now toggles every 50ms (10Hz) - visible but not distracting

### Files Modified
- `kernel/hal/pizero2w/gpio.c` - Full GPIO API
- `kernel/hal/pizero2w/emmc.c` - Use shared GPIO API
- `kernel/hal/pizero2w/irq.c` - 10Hz heartbeat LED
- `kernel/hal/qemu/platform.c` - GPIO stubs
- `kernel/hal/hal.h` - GPIO API declarations
- `kernel/kapi.h`, `kernel/kapi.c` - Added `led_status`
- `user/lib/vibe.h` - Added `led_status` to userspace kapi
- `user/bin/led.c` - New LED control tool (replaced blink.c)
- `Makefile` - Build led instead of blink

## Session 53: DMA Fill Implementation & Optimization Audit Update

**Goal**: Add DMA fill function for fast memory fills, update optimization audit.

### DMA Fill (`kernel/hal/pizero2w/dma.c`)
- Added `hal_dma_fill(dst, value, len)` - fills memory with 32-bit value using DMA
- Technique: Source buffer with fill value, SRC_INC=0 (no increment), DEST_INC=1
- Added `cache_invalidate_range()` for cache coherency after DMA writes
- QEMU fallback uses CPU loop

### Kernel API
- Added `dma_fill` to kapi.h, kapi.c, vibe.h

### Desktop Integration (`user/bin/desktop.c`)
- Window creation: Uses DMA fill to clear new window buffer
- Window resize: Uses DMA fill to clear resized buffer
- Exit cleanup: Uses DMA fill to clear screen

### Known Issue: DMA Fill Unreliable in Some Contexts
- Works: Desktop window creation/resize/exit
- Broken: term.c buffer clears (squiggly lines artifact)
- Broken: console.c scroll clears (reverted to CPU)
- Theory: SRC_INC=0 technique unreliable on BCM2837, or cache coherency timing issues
- Decision: Keep DMA fill for desktop (works), use CPU loops in term.c

### Optimization Audit Update (`docs/optimization_audit.md`)
- Marked fixed: Infinite loops (timeouts added), vsync, desktop dirty tracking
- Updated priority matrix and attack plan
- DMA fill now exists but noted as partially working

### Files Modified
- `kernel/hal/pizero2w/dma.c` - Added hal_dma_fill, cache_invalidate_range
- `kernel/hal/qemu/dma.c` - CPU fallback for dma_fill
- `kernel/hal/hal.h` - dma_fill declaration
- `kernel/kapi.h`, `kernel/kapi.c` - dma_fill API
- `user/lib/vibe.h` - dma_fill in userspace kapi
- `user/bin/desktop.c` - Use dma_fill for buffer clears
- `docs/optimization_audit.md` - Updated status

## Session 54: SD Card DMA & VFS Partial Read Fix

**Goal**: Optimize SD card reads with DMA, fix major VFS performance bug.

### VFS Partial Read Fix (THE BIG WIN)
- **Bug**: `vfs_read()` was reading ENTIRE file for every partial read
  - 64KB chunk from 64MB file = read 64MB, copy 64KB, free
  - 1024 chunks = 65GB disk I/O for 64MB file!
- **Fix**: Added `fat32_read_file_offset()` with proper offset/size handling
  - Skips clusters to reach offset, reads only needed data
- **Result**: 10+ minutes → 0.170s for 1MB file (>3500x improvement)

### SD Card DMA (Multi-block reads)
- Added DMA support using BCM2837 DMA controller (channel 4)
- DREQ pacing from EMMC peripheral (DREQ 11)
- Tight polling (removed delay_us busy waits)
- Single-block reads still use FIFO (DMA overhead not worth it)
- Note: DMA enabled but ~same speed - bottleneck is SD card itself

### Other Improvements
- FAT cache increased: 8 → 64 sectors
- `time` command added to vibesh (measures command execution)
- `readtest` benchmark tool (reads file, discards data)
- Disk activity LED: blinks at 20Hz during I/O
- Heartbeat LED: 1Hz when idle

### Files Modified
- `kernel/fat32.c`, `kernel/fat32.h` - Added `fat32_read_file_offset()`
- `kernel/vfs.c` - Use offset-aware read
- `kernel/hal/pizero2w/emmc.c` - DMA read support, disk activity LED
- `kernel/hal/pizero2w/irq.c` - 1Hz heartbeat
- `user/bin/vibesh.c` - `time` builtin command
- `user/bin/readtest.c` - New benchmark tool
- `Makefile` - Added readtest

## Session 55: Console Text Rendering Optimization

**Goal**: Speed up console text output (hexdump was taking 47s, framebuffer-bound).

### The Problem
- Framebuffer is mapped as **non-cacheable device memory** for GPU coherency
- Every pixel write goes directly to slow RAM (~100 cycles vs ~2 for cached)
- `fb_draw_char()` does 128 pixel writes per character
- For hexdump outputting thousands of lines, millions of slow uncached writes

### Solution: Line Buffer with DMA
- Added line buffer in cached RAM (800 × 16 pixels = 51KB)
- Characters drawn to cached buffer (fast L1/L2 cache writes)
- Track min/max columns actually drawn per line
- On newline/flush: single DMA 2D copy of only the drawn region to framebuffer

### Key Implementation Details
- `line_buffer[]` - Cached RAM buffer for one text line
- `line_buf_row`, `line_buf_min_col`, `line_buf_max_col` - Tracking state
- `line_buf_flush()` - Uses `hal_dma_copy_2d()` for strided copy
- `draw_char_at()` - Writes to line buffer, tracks dirty region
- Flush points: newline, scroll, cursor draw, explicit (end of puts)

### What Didn't Work
1. **First attempt**: memset32 entire buffer on each new line = 51KB overhead per line, made things 10x slower!
2. **DMA for scroll operations**: Cache coherency issues with GPU framebuffer, caused visual artifacts (squiggly lines)

### Results
- hexdump /bin/ls: **47s → 38s** (~20% improvement)
- Bottleneck shifted from character drawing to scroll operations
- Further DMA optimization for scroll caused cache issues, reverted

### Files Modified
- `kernel/console.c` - Line buffering implementation

## Session 56: Modern macOS-Inspired UI Refresh

**Goal**: Transform the retro System 7 aesthetic into a modern macOS Aqua-inspired look with proper window management.

### Desktop Overhaul
- **Pure white background** - Clean, minimal desktop
- **Translucent menu bar** - Light gray with subtle shadow
- **Frosted glass dock** - Solid light gray (#F0F0F0) with rounded corners
- **Window shadows** - Changed from heavy black blur to subtle light gray
- **Consistent color matching** - Fixed dock icon/text backgrounds to match dock

### Window Management (Traffic Lights)
- **Minimize (yellow)** - Window minimizes to dock as thumbnail preview
- **Maximize (green)** - Toggles between maximized (fills screen between menu bar and dock) and restored position
- **Restore from dock** - Click minimized window thumbnail to restore
- **Right-click context menu** - Right-click dock icons for "New Window" option

### App Modernization
Updated all GUI apps to match new aesthetic:
- **calc.c** - Light theme with rounded buttons, subtle shadows
- **sysmon.c** - Clean white background, modern progress bars
- **files.c** - Light file browser with rounded selection highlights
- **music.c** - Modern player controls with alpha-blended elements
- **term.c** - Light terminal with modern styling
- **textedit.c** - Clean text editor with updated chrome

### New Graphics Primitives (gfx.h)
- `gfx_fill_rect_alpha()` - Alpha-blended rectangle fill
- `gfx_fill_rounded_rect_alpha()` - Alpha-blended rounded rectangles
- `gfx_box_shadow()` - Drop shadows for rectangles
- `gfx_box_shadow_rounded()` - Drop shadows for rounded rects
- `gfx_gradient_v()` - Vertical gradient fills
- `gfx_lerp_color()` - Color interpolation helper

### UI Constants Changed
- `SHADOW_BLUR`: 8 → 4 (subtler)
- `COLOR_SHADOW`: Black → #AAAAAA (light gray)
- `DOCK_PADDING`: 12 → 32 (more spacing for labels)
- `COLOR_DESKTOP`: Gradient → Pure white (#FFFFFF)
- Dock: Alpha-blended → Solid (for consistent backgrounds)

### Files Modified
- `user/bin/desktop.c` - Complete UI overhaul, window management
- `user/bin/calc.c` - Modern calculator styling
- `user/bin/sysmon.c` - Light system monitor theme
- `user/bin/files.c` - Updated file browser
- `user/bin/music.c` - Modern music player
- `user/bin/term.c` - Light terminal theme
- `user/bin/textedit.c` - Clean text editor
- `user/lib/gfx.h` - New alpha/shadow/gradient primitives
