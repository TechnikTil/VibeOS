# Session Log - Part 1 (Sessions 1-10)

Early Development: Bootloader, Kernel, Shell, VFS, FAT32, GUI Foundations

---

### Session 1
- Created project structure
- Wrote bootloader, minimal kernel, linker script, Makefile
- Successfully booted in QEMU, UART output works
- Decided on retro Mac aesthetic
- Human confirmed: terminal-first approach, take it slow
- Added memory management (heap allocator) - working
- Added string functions and printf - working after fixing va_list issue

### Session 2
- Fixed virtio keyboard (was using legacy mode, switched to modern with force-legacy=false)
- Built framebuffer driver using ramfb
- Added bitmap font and text console with colors
- Built in-kernel shell with commands
- Attempted interrupts (GIC, timer, exception vectors) - breaks virtio keyboard
- Debugged extensively, even asked Gemini - couldn't find root cause
- Decided to skip interrupts (cooperative multitasking doesn't need them)
- Built in-memory VFS filesystem
- Added coreutils: ls, cd, pwd, mkdir, touch, cat
- Added echo with > redirect for writing files
- Added shift key support for keyboard (uppercase, symbols like >)
- Everything working! Shell, filesystem, keyboard all functional

### Session 3
- Attempted proper userspace with syscalls (SVC instruction)
- SVC never triggered exception handler - extensive debugging failed
- Pivoted to Win3.1 style: programs run in kernel space, call kapi directly
- Built ELF loader and process execution
- Created kapi (kernel API) - struct of function pointers
- Fixed multiple bugs: ELF load address (0x40200000), crt0 return address, color types
- Attempted to move shell commands to /bin as separate programs
- Hit weird linker bug: 5 embedded programs work, 6 breaks boot
- Extensive debugging: not size, not specific program, just "6th binary breaks it"
- Stack was in BSS and getting zeroed - fixed by putting in .stack section
- Still couldn't fix the 6-binary limit
- **DECISION**: Monolith kernel. All commands stay in shell. Fuck it, it's VibeOS.
- Final kernel: 28KB, all features working

### Session 6
- Revisited the external binaries problem - decided to use persistent FAT32 filesystem instead
- Built virtio-blk driver for block device access
- Implemented FAT32 filesystem driver (read-only)
- Integrated FAT32 with VFS - now `/` is backed by the disk image
- Updated Makefile to create and format 64MB FAT32 disk image
- Fixed multiple bugs:
  - Virtio-blk polling logic (was using stale `last_used_idx`)
  - Packed struct access on ARM (unaligned access faults) - read bytes manually
  - FAT32 minimum size requirement (increased from 32MB to 64MB)
- Disk image is mountable on macOS with `hdiutil attach disk.img`
- Can now put binaries on disk and load them at runtime (solves the 6-binary limit!)
- **Achievement**: Persistent filesystem working! Files survive reboots!

### Session 7
- Made FAT32 filesystem writable!
- Added FAT table write support (fat_set_cluster) - updates both FAT copies
- Added cluster allocation (fat_alloc_cluster) - finds free clusters
- Added cluster chain freeing (fat_free_chain)
- Implemented fat32_create_file() - create empty files
- Implemented fat32_mkdir() - create directories with . and .. entries
- Implemented fat32_write_file() - write data to files, handles cluster allocation
- Implemented fat32_delete() - delete files and free their clusters
- Updated VFS layer to use FAT32 write functions
- Now mkdir, touch, echo > file all persist to disk!
- Files created in VibeOS are visible when mounting disk.img on macOS
- **Achievement**: Full read/write persistent filesystem!

### Session 8
- Moved snake and tetris from kernel to userspace (/bin/snake, /bin/tetris)
- Extended kapi with framebuffer access (fb_base, width, height, drawing functions)
- Extended kapi with mouse input (position, buttons, poll)
- Added uart_puts to kapi for direct UART debug output
- Built virtio-tablet mouse driver (kernel/mouse.c)
- Fixed keyboard detection to not conflict with tablet (both are virtio-input)
- Created /bin/desktop - window manager with:
  - Classic Mac System 7 aesthetic (gray desktop, striped title bars)
  - Draggable windows by title bar
  - Close boxes that work
  - Menu bar (File, Edit, View, Special)
  - Mouse cursor with save/restore
  - Double buffering to reduce flicker
- Fixed heap/program memory overlap bug:
  - Backbuffer allocation (~2MB) was overlapping program load address
  - Moved program load address from 0x40200000 to 0x40400000
- **Achievement**: GUI desktop environment working!

### Session 9
- Implemented dynamic program loading - no more hardcoded addresses!
- Converted userspace programs to PIE (Position Independent Executables)
  - Updated user/linker.ld to base at 0x0
  - Added `-fPIE` and `-pie` compiler/linker flags
- Enhanced ELF loader:
  - elf_load_at() loads PIE binaries at any address
  - elf_calc_size() calculates memory requirements
  - Supports both ET_EXEC and ET_DYN types
- Built cooperative multitasking infrastructure:
  - Process table (MAX_PROCESSES = 16)
  - Process states: FREE, READY, RUNNING, BLOCKED, ZOMBIE
  - Round-robin scheduler
  - Context switching in assembly (kernel/context.S)
  - yield() and spawn() added to kapi
- Programs now load at 0x41000000+ with kernel picking addresses dynamically
- Tested: desktop→snake→tetris→desktop all load at different addresses
- **Achievement**: Dynamic loading and multitasking foundation complete!

### Session 10
- Added Apple menu with dropdown (About VibeOS..., Quit Desktop)
- Removed Q keyboard shortcut - quit only via Apple menu now
- Added font_data pointer to kapi for userspace text rendering
- Fixed window rendering - all drawing now goes to backbuffer:
  - Added bb_draw_char/bb_draw_string for backbuffer text
  - Windows properly occlude each other (no text bleed-through)
  - Title bars, content areas, and borders all render correctly
- Created LONGTERM.md with roadmap
