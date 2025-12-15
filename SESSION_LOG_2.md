# Session Log - Part 2 (Sessions 11-20)

Desktop Apps, PIE Relocations, Terminal, Interrupts Working

---

### Session 11
- Added dock bar at bottom of screen with app icons
- Built Calculator app (first desktop app!):
  - Calculator icon in dock (32x32 pixel art)
  - Click dock icon to open calculator window
  - Working integer arithmetic (+, -, *, /)
  - 3D button styling with proper hit detection
  - Shows pending operation in display
  - Fixed PIE string literal issue (2D char* array → flat char[][] array)
- Improved menu bar layout and spacing
- Windows can't be dragged below the dock

### Session 12
- Built File Explorer app:
  - Folder icon in dock
  - Navigate directories (click to select, double-click to enter)
  - Path bar showing current location
  - ".." to go up a directory
  - Right-click context menu with: New File, New Folder, Rename, Delete
  - Inline rename with keyboard input (Enter to confirm, Escape to cancel)
- Added rm command to shell
- Added vfs_delete() and vfs_rename() to VFS layer
- Added fat32_rename() - modifies directory entry name in place
- Extended kapi with delete, rename, readdir, set_cwd, get_cwd
- Fixed macOS junk files (._, .DS_Store, .fseventsd) in Makefile:
  - Disk mounts with -nobrowse to prevent Finder indexing
  - Cleanup commands run after every mount/install

### Session 13
- Added snake and tetris game launchers to dock:
  - Snake icon (32x32 pixel art green snake)
  - Tetris icon (32x32 colorful blocks)
  - Click to launch games from desktop
- Fixed cooperative multitasking to actually work:
  - Added yield() calls to desktop, snake, and tetris main loops
  - Fixed process_yield() to work from kernel context (was returning early)
  - Made process_exec() create real process entries (was direct function call)
  - Added kernel_context to save/restore when switching between kernel and processes
- Games now use exec() instead of spawn():
  - Desktop blocks while game runs, resumes when game exits
  - Clean handoff - no screen fighting between processes
- **Achievement**: Can launch games from dock and return to desktop!

### Session 14
- Built userspace shell `/bin/vibesh`:
  - Boots directly into vibesh (kernel shell is now just a bootstrap)
  - Parses commands, handles builtins (cd, exit, help)
  - Executes external programs from /bin with argument passing
- Built userspace coreutils:
  - `/bin/echo` - with output redirection support (echo foo > file)
  - `/bin/ls` - uses proper readdir API
  - `/bin/cat` - supports multiple files
  - `/bin/pwd` - print working directory
  - `/bin/mkdir`, `/bin/touch`, `/bin/rm` - file operations
- Added `exec_args` to kapi for passing argc/argv to programs
- Added `console_rows`/`console_cols` to kapi
- Fixed process exit bug:
  - process_exit() was returning when no other processes existed
  - Now context switches directly back to kernel_context
- Attempted userspace vi but hit PIE/BSS issues:
  - Static variables in PIE binaries don't work reliably
  - Even heap-allocated state with function pointer calls hangs
  - Abandoned - will make GUI editor instead
- **Achievement**: Userspace shell and coreutils working!

### Session 15
- **MAJOR BREAKTHROUGH: Fixed PIE relocations!**
  - Userspace C code now works like normal C - pointers, string literals, everything
  - Problem: Static initializers with pointers (e.g., `const char *label = "hello"`) were broken
  - Root cause: With `-O2`, GCC generates PC-relative code but puts struct in BSS (zeroed), so pointers are NULL
  - Solution 1: Use `-O0` for userspace so GCC generates proper relocations
  - Solution 2: ELF loader now processes `.rela.dyn` section with `R_AARCH64_RELATIVE` entries
  - Added `Elf64_Dyn`, `Elf64_Rela` structs and `elf_process_relocations()` to kernel/elf.c
  - Formula: `*(load_base + offset) = load_base + addend`
- Rebuilt desktop from scratch (old one was a mess with everything embedded):
  - Clean window manager architecture
  - Desktop just manages windows, dock, menu bar, cursor
  - Apps are separate binaries that use window API via kapi
  - Fullscreen apps (snake, tetris) use exec() - desktop waits
  - Windowed apps (calc) use spawn() + window API
- Built Calculator app (`/bin/calc`):
  - First proper windowed app using the new window API
  - Creates window, gets buffer, draws buttons, receives events
  - Working integer arithmetic
- Window API in kapi:
  - `window_create()`, `window_destroy()`, `window_get_buffer()`
  - `window_poll_event()`, `window_invalidate()`, `window_set_title()`
  - Desktop registers these functions at startup
- **Achievement**: Can now write normal C code in userspace! This unlocks everything.

### Session 16
- **Visual refresh - True Mac System 7 aesthetic!**
  - Pure 1-bit black & white color scheme
  - Classic Mac diagonal checkerboard desktop pattern
  - Apple logo (16x16 bitmap) in menu bar
  - Beautiful 32x32 pixel art dock icons: Snake, Tetris, Calculator, Files, Terminal
  - System 7 window chrome: horizontal stripes on focused title bars, drop shadows, double-line borders
  - Close box with inner square when focused
  - Clock in menu bar (decorative)
- **Built Terminal Emulator (`/bin/term`)!**
  - 80x24 character window with monospace font
  - Spawns vibesh shell inside the window
  - Stdio hooks mechanism: `stdio_putc`, `stdio_puts`, `stdio_getc`, `stdio_has_key`
  - Shell and all coreutils use hooks when available, fall back to console otherwise
  - Keyboard input via window events → ring buffer → shell reads
  - Inverse block cursor
- **Updated all coreutils for terminal support:**
  - ls, cat, echo, pwd, mkdir, touch, rm all use `out_puts`/`out_putc` helpers
  - Check for stdio hooks, use them if set, otherwise use console I/O
  - Works in both kernel console AND terminal window
- **Achievement**: Shell running inside a GUI window! Can run commands, see output, everything works!

### Session 17
- **INTERRUPTS FINALLY WORKING!**
  - Root cause found: GIC security groups. Running in Non-Secure EL1 but trying to configure Group 0 registers was a no-op.
  - Solution: Boot at EL3 (Secure) using `-bios` instead of `-kernel`, stay in Secure world
  - Changed QEMU flags: `-M virt,secure=on -bios vibeos.bin`
  - Updated linker script: code at 0x0 (flash), data/BSS in RAM (0x40000000)
  - Updated boot.S: EL3→EL1 transition (skip EL2), copy .data from flash to RAM
  - GIC configured for Group 0 (Secure) interrupts
- **New files:**
  - `kernel/irq.c` / `kernel/irq.h` - GIC-400 driver, timer support
  - `kernel/vectors.S` - Exception vector table for AArch64
- **Keyboard now interrupt-driven:**
  - Registered IRQ handler for virtio keyboard (IRQ 78)
  - No more polling needed for keyboard input
- **Fixed FAT32 bug:**
  - `resolve_path()` was dereferencing NULL `rest` pointer from `strtok_r`
  - Added NULL check: `if (rest && *rest && ...)`
- **Timer ready but disabled:**
  - Timer IRQ handler exists, can enable preemptive multitasking later
  - Currently disabled to keep cooperative model stable
- **Achievement**: Full interrupt support! GIC mystery finally solved after Sessions 2-3 failures!

### Session 18
- **Enabled timer for uptime tracking:**
  - Timer fires at 100Hz (10ms intervals)
  - `timer_get_ticks()` returns tick count since boot
  - Keeping cooperative multitasking (no preemption)
- **Added uptime command (`/bin/uptime`):**
  - Shows hours/minutes/seconds since boot
  - Also shows raw tick count
  - Fixed crash: uptime wasn't in USER_PROGS list, old binary had wrong kapi struct
- **Added memory stats to kapi:**
  - `get_mem_used()` and `get_mem_free()`
  - Heap is ~16MB (between BSS end and program load area at 0x41000000)
- **Built System Monitor GUI app (`/bin/sysmon`):**
  - Classic Mac-style windowed app
  - Shows uptime (updates live)
  - Shows memory usage with progress bar (diagonal stripes pattern)
  - Shows used/free memory in MB
  - Auto-refreshes every ~500ms
- **Gotcha: USER_PROGS list**
  - New userspace programs MUST be added to USER_PROGS in Makefile
  - Old binaries on disk with outdated kapi struct will crash!
- **Achievement**: Timer working! System monitoring! The vibes are immaculate.

- **Built TextEdit (`/bin/textedit`):**
  - Simple GUI text editor - no modes, just type
  - Arrow keys, Home, End, Delete all work
  - Ctrl+S to save
  - Save As modal dialog when no filename set
  - Status bar shows filename, line:col, modified indicator
  - Scrolling for long files
  - Usage: `textedit` or `textedit /path/to/file`
- **Enhanced keyboard driver:**
  - Arrow keys now return special codes (0x100-0x106)
  - Ctrl modifier support (Ctrl+A = 1, Ctrl+S = 19, etc.)
  - Key buffer changed from `char` to `int` to support extended codes
  - Added Home, End, Delete key support
- **TextEdit enhancements:**
  - Line numbers in gray gutter on the left
  - Tab key inserts 4 spaces
  - Auto-close brackets and quotes: `()`, `[]`, `{}`, `""`, `''`
  - C syntax highlighting (detects .c and .h files):
    - Keywords in dark blue (if, else, for, while, return, etc.)
    - Comments in dark green (// and /* */)
    - String literals in dark red
    - Numbers in purple
  - Colors only appear in apps where it makes sense (desktop stays 1-bit B&W)
- **Achievement**: Real text editor with IDE-lite features!

### Session 19
- **Built File Explorer (`/bin/files`):**
  - Windowed GUI file browser
  - Navigate directories (click to select, double-click to enter)
  - Path bar showing current location
  - ".." to go up a directory
  - Right-click context menu with:
    - New File / New Folder (with inline rename UI)
    - Rename (inline text editing)
    - Delete (recursive - works on non-empty directories!)
    - Open with TextEdit
    - Open Terminal Here
  - Keyboard navigation (arrow keys, Enter, Backspace)
- **Added right-click support to desktop:**
  - Desktop now detects right mouse button clicks
  - Passes button info to windowed apps via event data3
- **Implemented recursive directory deletion:**
  - `fat32_delete_recursive()` - deletes files and directories recursively
  - Walks directory tree, deletes children first, then parent
  - Exposed via `vfs_delete_recursive()` and kapi
- **Improved create file/folder UX:**
  - "New File" / "New Folder" now shows inline rename field immediately
  - Type the name and press Enter to actually create
  - Press Escape to cancel
  - No more "untitled" / "newfolder" placeholder names
- **Achievement**: Full-featured file explorer!
