# VibeOS Session Log 6 - MicroPython Port

## Session 56: MicroPython Interpreter Port

**Goal**: Port MicroPython to VibeOS as a userspace program with access to kernel API.

### Why MicroPython?
- Python REPL running on bare metal
- Scripting language for VibeOS applications
- Interactive development/testing
- MIT licensed, minimal dependencies

### Initial Approach (Abandoned)
- Tried putting MicroPython in `user/bin/micropython/`
- MicroPython's build system requires qstring generation step
- Copying sources and building with our Makefile didn't work
- Qstrings are auto-generated from source scanning

### Working Approach
- Created proper port in `micropython/ports/vibeos/`
- Uses MicroPython's own build system (py.mk, mkrules.mk)
- Build system handles qstring generation automatically

### Port Files Created
1. **`mpconfigport.h`** - Configuration:
   - `MICROPY_CONFIG_ROM_LEVEL_MINIMUM` - Minimal feature set
   - No floats (avoids libgcc soft-float dependencies)
   - No external modules (json, re, random disabled)
   - Minimal sys module (needed for REPL prompts)
   - 2MB heap for garbage collector
   - `MICROPY_HAL_HAS_VT100 = 0` - VibeOS console doesn't support escape codes

2. **`mphalport.c`** - Hardware Abstraction Layer:
   - `mp_hal_stdin_rx_chr()` - Keyboard input via kapi
   - `mp_hal_stdout_tx_strn()` - Console output via kapi
   - `mp_hal_ticks_ms()` - Uptime from kernel
   - `mp_hal_delay_ms()` - Sleep via kapi
   - Key translation: `'\n'` → `'\r'` for Enter (readline expects CR)
   - Special key conversion: Arrow keys → VT100 escape sequences

3. **`modvibe.c`** - Python `vibe` Module:
   - Console: `clear()`, `puts()`, `set_color(fg, bg)`
   - Input: `has_key()`, `getc()`
   - Timing: `sleep_ms()`, `uptime_ms()`, `yield()`
   - Graphics: `put_pixel()`, `fill_rect()`, `draw_string()`, `screen_size()`
   - Mouse: `mouse_pos()`, `mouse_buttons()`
   - Memory: `mem_free()`, `mem_used()`
   - Color constants: `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`, `CYAN`, `MAGENTA`

4. **`main.c`** - Entry point:
   - Receives kapi pointer from kernel
   - Initializes MicroPython runtime and GC
   - Runs `pyexec_friendly_repl()`

5. **`setjmp.S`** - AArch64 setjmp/longjmp:
   - Required for GC register scanning
   - Saves/restores x19-x30, sp

6. **`stubs.c`** - Missing function stubs:
   - `strchr()` - Non-inline version for address-taking
   - `mp_sched_keyboard_interrupt()` - Ctrl+C handler (no-op)
   - `mp_hal_move_cursor_back()` - Backspace handling (uses `\b`)
   - `mp_hal_erase_line_from_cursor()` - Line clear (spaces + backspace)

### Kernel libc Extensions
Added headers for freestanding MicroPython build:
- `setjmp.h` - jmp_buf typedef, setjmp/longjmp declarations
- `stddef.h` - size_t, ssize_t, ptrdiff_t, NULL, offsetof
- `stdint.h` - int8_t through int64_t, limits
- `string.h` - Inline implementations of memcpy, strlen, strchr, etc.
- `errno.h` - Full set of POSIX error codes
- `stdio.h` - Added SEEK_SET/CUR/END
- `math.h` - Added NAN, INFINITY, isnan, isinf
- `assert.h` - Added NDEBUG guard

### Build Issues Solved
1. **NDEBUG redefinition** - MicroPython's `-DNDEBUG` conflicted with assert.h
2. **Missing math macros** - Added NAN, INFINITY, HUGE_VAL to math.h
3. **Missing ssize_t** - Added to stddef.h
4. **strchr implicit declaration** - Added `-include string.h` to CFLAGS
5. **Missing errno codes** - Expanded errno.h
6. **extmod dependencies** - Created stub headers (virtpin.h, vfs.h, modplatform.h)
7. **VT100 escape codes** - Disabled, implemented simple backspace handling

### MicroPython Source Cleanup
Trimmed from ~50MB to ~3.5MB:
- **Kept**: `py/`, `shared/`, `ports/vibeos/`, `LICENSE`
- **Deleted**: All other ports, drivers, lib, extmod, tools, docs, tests, examples
- Created minimal `extmod/` stubs for headers still referenced by py/

### Python Demo Scripts (`python/`)
1. **`hello.py`** - Hello world with colors
2. **`graphics.py`** - Draws colored rectangles
3. **`mouse.py`** - Paint program with mouse
4. **`bounce.py`** - Bouncing ball animation at 60fps
5. **`sysinfo.py`** - System info display

### Binary Size
- Text: 130KB
- Data: 12KB
- BSS: 2MB (GC heap)

### What Works
- Full Python REPL with line editing
- `import vibe` module
- All vibe functions (console, graphics, input, timing)
- `print()` (uses our HAL)
- Backspace, Enter, arrow keys
- Arbitrary precision integers (`2 ** 100`)

### What Doesn't Work Yet
- Running .py files from disk (needs `mp_lexer_new_from_file()`)
- Floats (disabled to avoid libgcc)
- External modules (json, re, random)

### Files Created
- `micropython/ports/vibeos/*` - Port implementation
- `micropython/extmod/*.h` - Stub headers
- `kernel/libc/setjmp.h`, `stddef.h`, `stdint.h` - New headers
- `python/*.py` - Demo scripts

### Files Modified
- `Makefile` - MicroPython build integration, python/ install
- `kernel/libc/*.h` - Extended for freestanding build

### Licensing
- MicroPython core is MIT licensed
- All non-MIT code (drivers, lib, other ports) was deleted
- Only need to keep `micropython/LICENSE` file

### Lessons Learned
1. MicroPython's build system is complex but handles qstring generation automatically
2. Creating a proper port in `ports/` is easier than fighting the build system
3. VT100 escape codes assumed by readline - need stubs if terminal doesn't support them
4. Key codes differ: VibeOS uses `'\n'` for Enter, readline expects `'\r'`
5. Trimming MicroPython source is safe - just keep py/, shared/, and your port
