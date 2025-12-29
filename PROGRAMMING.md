# Programming for VibeOS

There are three ways to write programs for VibeOS:

1. **TCC** - Compile C programs directly on VibeOS
2. **MicroPython** - Write Python scripts with kernel API access
3. **Cross-compile** - Build on your host machine with `aarch64-elf-gcc`

## Option 1: TCC (On-Device C Compiler)

### Hello World

Create `/home/user/hello.c`:
```c
#include <vibe.h>

int main(kapi_t *api, int argc, char **argv) {
    api->puts("Hello from TCC!\n");
    return 0;
}
```

Compile and run:
```bash
cd /home/user
tcc hello.c -o hello
./hello
```

### Program Structure

Every program receives a `kapi_t` pointer as its first argument:

```c
#include <vibe.h>

int main(kapi_t *api, int argc, char **argv) {
    // api-> gives you access to all kernel functions
    api->puts("Hello\n");
    api->sleep_ms(1000);
    return 0;
}
```

The `<vibe.h>` header (at `/lib/tcc/include/vibe.h`) defines the `kapi_t` struct with all available functions.

### Available Headers

TCC includes standard C headers:
- `stdio.h` - printf, sprintf (limited)
- `stdlib.h` - malloc, free, atoi
- `string.h` - strlen, strcpy, memcpy
- `vibe.h` - VibeOS kernel API

### Limitations

- No floating point (use MicroPython or cross-compile)
- No threads
- Limited libc (what's implemented in TCC's runtime)

## Option 2: MicroPython

### Interactive REPL

```bash
mpy
```

```python
>>> import vibe
>>> vibe.puts("Hello!")
>>> vibe.fill_rect(100, 100, 50, 50, vibe.RED)
```

### Running Scripts

```bash
mpy /path/to/script.py
```

### The vibe Module

All kernel functionality is exposed through the `vibe` module:

```python
import vibe

# Console
vibe.clear()
vibe.puts("Hello")
vibe.set_color(vibe.GREEN, vibe.BLACK)

# Timing
vibe.sleep_ms(1000)
uptime = vibe.uptime_ms()

# Input
if vibe.has_key():
    c = vibe.getc()

# Graphics
w, h = vibe.screen_size()
vibe.put_pixel(100, 100, vibe.WHITE)
vibe.fill_rect(50, 50, 100, 100, vibe.BLUE)
vibe.draw_string(10, 10, "Text", vibe.WHITE, vibe.BLACK)

# Mouse
x, y = vibe.mouse_pos()
buttons = vibe.mouse_buttons()

# Files
files = vibe.listdir("/bin")
data = vibe.read("/etc/motd", 0, 1024)
vibe.write("/tmp/test.txt", "hello")
vibe.mkdir("/tmp/mydir")
vibe.delete("/tmp/test.txt")

# Processes
vibe.spawn("/bin/calc")
vibe.exec("/bin/snake")  # waits for completion
procs = vibe.ps()  # [(pid, name, state), ...]

# System info
free = vibe.mem_free()
used = vibe.mem_used()
total = vibe.ram_total()

# Networking
ip = vibe.dns_resolve("example.com")
sock = vibe.tcp_connect(ip, 80)
vibe.tcp_send(sock, b"GET / HTTP/1.0\r\n\r\n")
data = vibe.tcp_recv(sock, 4096)
vibe.tcp_close(sock)

# TLS
sock = vibe.tls_connect(ip, 443, "example.com")
vibe.tls_send(sock, b"GET / HTTP/1.0\r\n\r\n")
data = vibe.tls_recv(sock, 4096)
vibe.tls_close(sock)

# Sound
vibe.sound_play("/music/song.wav")
vibe.sound_pause()
vibe.sound_resume()
vibe.sound_stop()
```

### Color Constants

```python
vibe.BLACK, vibe.WHITE, vibe.RED, vibe.GREEN,
vibe.BLUE, vibe.YELLOW, vibe.CYAN, vibe.MAGENTA, vibe.AMBER
```

### Window Events (for GUI apps)

```python
wid = vibe.window_create(100, 100, 400, 300, "My Window")
buf, w, h = vibe.window_size(wid)

while True:
    event = vibe.window_poll(wid)
    if event:
        etype, d1, d2, d3 = event
        if etype == vibe.WIN_EVENT_CLOSE:
            break
        elif etype == vibe.WIN_EVENT_KEY:
            key = d1
        elif etype == vibe.WIN_EVENT_MOUSE_DOWN:
            x, y, button = d1, d2, d3

    # Draw to buffer...
    vibe.window_invalidate(wid)
    vibe.sched_yield()

vibe.window_destroy(wid)
```

### Included Modules

MicroPython includes:
- `json` - JSON parsing/serialization
- `re` - Regular expressions
- `math` - Math functions (sin, cos, sqrt, etc.)
- `random` - Random number generation
- `heapq` - Priority queues

## Option 3: Cross-Compile

For complex programs or when you need more control.

### Setup

You need the cross-compiler:
```bash
brew install aarch64-elf-gcc  # macOS
```

### Creating a Program

Add your source file to `user/bin/`:

```c
// user/bin/myapp.c
#include "../lib/vibe.h"

int main(kapi_t *api, int argc, char **argv) {
    api->puts("Hello from cross-compiled program!\n");
    return 0;
}
```

The Makefile automatically detects new `.c` files in `user/bin/` and builds them.

### Build and Deploy

```bash
make        # Builds kernel and all userspace
make run    # Syncs to disk and runs QEMU
```

Your program will be at `/bin/myapp`.

### Using Graphics (gfx.h)

For GUI programs, include the graphics helpers:

```c
#include "../lib/vibe.h"
#include "../lib/gfx.h"

int main(kapi_t *api, int argc, char **argv) {
    gfx_ctx_t ctx = {
        .buf = api->fb_base,
        .width = api->fb_width,
        .height = api->fb_height,
        .stride = api->fb_width
    };

    gfx_fill_rect(&ctx, 100, 100, 200, 150, 0x0000FF);  // Blue rect
    gfx_draw_string(&ctx, 110, 120, "Hello!", 0xFFFFFF, 0x0000FF, api->font_data);

    while (1) {
        api->yield();
    }

    return 0;
}
```

### Multi-File Programs

For programs with multiple source files, create a directory:

```
user/bin/myapp/
├── main.c
├── helper.h
└── helper.c
```

Then add a Makefile or update the main Makefile to handle it (see `user/bin/doom/` for an example).

## Kernel API Reference (kapi_t)

The `kapi_t` struct is passed to every program. Here's the complete API:

### Console I/O

```c
void putc(char c);                           // Print character
void puts(const char *s);                    // Print string
void uart_puts(const char *s);               // Print to UART (debug)
int  getc(void);                             // Read character (blocking)
int  has_key(void);                          // Check if key available
void set_color(uint32_t fg, uint32_t bg);    // Set text colors
void clear(void);                            // Clear screen
void set_cursor(int row, int col);           // Move cursor
void set_cursor_enabled(int enabled);        // Show/hide cursor
void clear_to_eol(void);                     // Clear to end of line
void clear_region(int row, int col, int w, int h);  // Clear rectangle
int  console_rows(void);                     // Get console height
int  console_cols(void);                     // Get console width
```

### Memory

```c
void *malloc(size_t size);                   // Allocate memory
void  free(void *ptr);                       // Free memory
```

### Filesystem

```c
void   *open(const char *path);              // Open file/directory
void    close(void *handle);                 // Close handle
size_t  read(void *f, char *buf, size_t size, size_t offset);
size_t  write(void *f, const char *buf, size_t size);
int     is_dir(void *node);                  // Check if directory
size_t  file_size(void *node);               // Get file size
int     create(const char *path);            // Create file
int     mkdir(const char *path);             // Create directory
int     delete(const char *path);            // Delete file
int     delete_dir(const char *path);        // Delete empty directory
int     delete_recursive(const char *path);  // Delete recursively
int     rename(const char *old, const char *new);
int     readdir(void *dir, int index, char *name, size_t size, uint8_t *type);
void    set_cwd(const char *path);           // Change directory
void    get_cwd(char *buf, size_t size);     // Get current directory
```

### Processes

```c
void exit(int status);                       // Exit process
int  exec(const char *path);                 // Execute and wait
int  exec_args(const char *path, int argc, char **argv);
void yield(void);                            // Yield to scheduler
int  spawn(const char *path);                // Execute async
int  spawn_args(const char *path, int argc, char **argv);
int  kill_process(int pid);                  // Kill process
int  get_process_count(void);                // Number of processes
int  get_process_info(int idx, char *name, int size, int *state);
```

### Graphics

```c
uint32_t *fb_base;                           // Framebuffer pointer
int       fb_width;                          // Screen width
int       fb_height;                         // Screen height
uint8_t  *font_data;                         // 8x16 bitmap font

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg);
```

### Mouse

```c
void    mouse_get_pos(int *x, int *y);       // Get position
uint8_t mouse_get_buttons(void);             // Get button state
void    mouse_poll(void);                    // Update state
void    mouse_set_pos(int x, int y);         // Set position
void    mouse_get_delta(int *dx, int *dy);   // Get movement delta
```

### Windows (for GUI apps running under desktop)

```c
int   window_create(int x, int y, int w, int h, const char *title);
void  window_destroy(int wid);
void *window_get_buffer(int wid, int *w, int *h);
int   window_poll_event(int wid, int *type, int *d1, int *d2, int *d3);
void  window_invalidate(int wid);            // Request redraw
void  window_set_title(int wid, const char *title);
```

Window event types:
- `WIN_EVENT_NONE`, `WIN_EVENT_MOUSE_DOWN`, `WIN_EVENT_MOUSE_UP`
- `WIN_EVENT_MOUSE_MOVE`, `WIN_EVENT_KEY`, `WIN_EVENT_CLOSE`
- `WIN_EVENT_FOCUS`, `WIN_EVENT_UNFOCUS`, `WIN_EVENT_RESIZE`

### Timing

```c
uint64_t get_uptime_ticks(void);             // Ticks since boot (100Hz)
void     wfi(void);                          // Wait for interrupt
void     sleep_ms(uint32_t ms);              // Sleep milliseconds
```

### RTC

```c
uint32_t get_timestamp(void);                // Unix timestamp
void     get_datetime(int *year, int *month, int *day,
                      int *hour, int *minute, int *second, int *weekday);
```

### Sound

```c
void sound_play_wav(const void *data, uint32_t size);
void sound_stop(void);
int  sound_is_playing(void);
void sound_play_pcm(const void *data, uint32_t samples, uint8_t channels, uint32_t rate);
void sound_play_pcm_async(const void *data, uint32_t samples, uint8_t channels, uint32_t rate);
void sound_pause(void);
void sound_resume(void);
int  sound_is_paused(void);
```

### Networking

```c
int      net_ping(uint32_t ip, uint16_t seq, uint32_t timeout_ms);
void     net_poll(void);
uint32_t net_get_ip(void);
void     net_get_mac(uint8_t *mac);
uint32_t dns_resolve(const char *hostname);

// TCP
int      tcp_connect(uint32_t ip, uint16_t port);
int      tcp_send(int sock, const void *data, uint32_t len);
int      tcp_recv(int sock, void *buf, uint32_t maxlen);
void     tcp_close(int sock);
int      tcp_is_connected(int sock);

// TLS
int      tls_connect(uint32_t ip, uint16_t port, const char *hostname);
int      tls_send(int sock, const void *data, uint32_t len);
int      tls_recv(int sock, void *buf, uint32_t maxlen);
void     tls_close(int sock);
int      tls_is_connected(int sock);
```

### TrueType Fonts

```c
void *ttf_get_glyph(int codepoint, int size, int style);
int   ttf_get_advance(int codepoint, int size);
int   ttf_get_kerning(int cp1, int cp2, int size);
void  ttf_get_metrics(int size, int *ascent, int *descent, int *line_gap);
int   ttf_is_ready(void);
```

### System Info

```c
size_t   get_mem_used(void);
size_t   get_mem_free(void);
uint64_t get_ram_total(void);
uint32_t get_disk_total(void);               // KB
uint32_t get_disk_free(void);                // KB
char    *get_cpu_name(void);
int      get_cpu_freq_mhz(void);
int      get_cpu_cores(void);
int      usb_device_count(void);
int      usb_device_info(int idx, uint16_t *vid, uint16_t *pid, char *name, int len);
size_t   klog_read(char *buf, size_t offset, size_t size);
size_t   klog_size(void);
```

### GPIO (Pi only)

```c
void led_on(void);
void led_off(void);
void led_toggle(void);
int  led_status(void);
```

### DMA (Pi only)

```c
int  dma_available(void);
void dma_copy(void *dst, const void *src, uint32_t len);
void dma_copy_2d(void *dst, uint32_t dst_pitch, const void *src, uint32_t src_pitch, uint32_t w, uint32_t h);
void dma_fb_copy(uint32_t *dst, const uint32_t *src, uint32_t w, uint32_t h);
void dma_fill(void *dst, uint32_t value, uint32_t len);
```

### Hardware Double Buffering (Pi only)

```c
int   fb_has_hw_double_buffer(void);
void  fb_flip(int buffer);
void *fb_get_backbuffer(void);
```

## Helper Functions (vibe.h)

The `vibe.h` header includes inline helpers that use stdio hooks when available:

```c
vibe_putc(api, 'x');              // Uses terminal if in term, console otherwise
vibe_puts(api, "hello");
vibe_getc(api);
vibe_has_key(api);
vibe_print_int(api, 42);
vibe_print_hex(api, 0xDEAD);
vibe_print_size(api, bytes);      // Human-readable (KB, MB, GB)
```

## Colors

Predefined color constants (RGB):

```c
COLOR_BLACK   0x000000
COLOR_WHITE   0xFFFFFF
COLOR_RED     0xFF0000
COLOR_GREEN   0x00FF00
COLOR_BLUE    0x0000FF
COLOR_CYAN    0x00FFFF
COLOR_MAGENTA 0xFF00FF
COLOR_YELLOW  0xFFFF00
COLOR_AMBER   0xFFBF00
```

## Example: Simple GUI Program

```c
#include "../lib/vibe.h"
#include "../lib/gfx.h"

int main(kapi_t *api, int argc, char **argv) {
    int wid = api->window_create(100, 100, 300, 200, "My App");
    if (wid < 0) {
        api->puts("Failed to create window\n");
        return 1;
    }

    int w, h;
    uint32_t *buf = api->window_get_buffer(wid, &w, &h);

    gfx_ctx_t ctx = { .buf = buf, .width = w, .height = h, .stride = w };

    int running = 1;
    while (running) {
        int type, d1, d2, d3;
        while (api->window_poll_event(wid, &type, &d1, &d2, &d3)) {
            if (type == WIN_EVENT_CLOSE) {
                running = 0;
            } else if (type == WIN_EVENT_KEY) {
                if (d1 == 'q') running = 0;
            }
        }

        // Draw
        gfx_fill_rect(&ctx, 0, 0, w, h, 0xFFFFFF);  // White background
        gfx_draw_string(&ctx, 10, 10, "Hello!", 0x000000, 0xFFFFFF, api->font_data);

        api->window_invalidate(wid);
        api->yield();
    }

    api->window_destroy(wid);
    return 0;
}
```

## Tips

1. **Always call `yield()`** in your main loop. Without it, other processes won't run.

2. **Use `printf()` for debugging** - it goes to screen if compiled with PRINTF=screen and serial if compiled with PRINTF=uart. also you will see the prints in dmesg. which exists as both /bin/dmesg to call from vibesh and dmesg command in recovery shell.

3. **Check return values** - file operations return 0 or negative on error.

4. **Colors are 32-bit RGB** - 0xRRGGBB format.

5. **The framebuffer is shared** - if you're a fullscreen app, you own it. If you're a windowed app, draw to your window buffer.

6. **Ctrl+C in terminal** sends character 3 (ETX), not a signal.

7. **No floating point in TCC** - use MicroPython or cross-compile if you need floats.
