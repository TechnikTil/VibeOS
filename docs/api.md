# VibeOS Programming Guide

How to write programs for VibeOS. Win3.1-style: no syscalls, just function pointers.

## Hello World

```c
#include "../lib/vibe.h"

int main(kapi_t *k, int argc, char **argv) {
    vibe_puts(k, "Hello, VibeOS!\n");
    return 0;
}
```

Save as `user/bin/hello.c`, run `make`, then `/bin/hello` in the shell.

## The kapi_t Structure

Every program receives a pointer to `kapi_t` - the kernel API. This struct contains function pointers for everything: I/O, memory, files, windows, networking, sound, etc.

```c
int main(kapi_t *k, int argc, char **argv) {
    // k-> everything
}
```

## Console I/O (Smart Helpers)

These helpers automatically use stdio hooks when running in the terminal emulator, or fall back to console when running directly. **Always use these instead of raw k->puts/putc.**

```c
vibe_puts(k, "text");        // Print string (smart - works in terminal and console)
vibe_putc(k, 'x');           // Print char
int c = vibe_getc(k);        // Read char (blocks)
int has = vibe_has_key(k);   // Check if key available
vibe_print_int(k, 42);       // Print integer
vibe_print_uint(k, 123);     // Print unsigned integer
vibe_print_hex(k, 0xDEAD);   // Print hex (no 0x prefix)
vibe_print_size(k, 1048576); // Print human-readable size (1 MB)
```

## Raw Console I/O

Only use these for special cases (direct console access):

```c
k->puts("text");             // Direct console print
k->putc('x');                // Direct console char
k->clear();                  // Clear console
k->set_color(fg, bg);        // Set text colors (RGB: 0x00RRGGBB)
```

## Memory

```c
void *ptr = k->malloc(size);     // Allocate
k->free(ptr);                     // Free
```

## Filesystem

```c
// Open file or directory
void *file = k->open("/path/to/file");
if (!file) { /* not found */ }

// Check if directory
if (k->is_dir(file)) { /* it's a dir */ }

// Get file size
int size = k->file_size(file);

// Read file (returns bytes read)
char buf[1024];
int n = k->read(file, buf, sizeof(buf), 0);  // last arg is offset

// Create and write file
void *f = k->create("/path/to/newfile");
k->write(f, "content", 7);

// Create directory
k->mkdir("/path/to/newdir");

// Delete file or directory
k->delete("/path/to/file");
k->delete_dir("/path/to/emptydir");

// List directory
char name[64];
uint8_t type;  // 1=file, 2=directory
int idx = 0;
while (k->readdir(dir, idx, name, sizeof(name), &type) == 0) {
    k->puts(name);
    k->puts("\n");
    idx++;
}

// Working directory
char cwd[256];
k->get_cwd(cwd, sizeof(cwd));
k->set_cwd("/new/path");
```

## Process Control

```c
k->exit(0);                           // Exit program
k->yield();                           // Give CPU to other processes
k->exec("/bin/program");              // Run program (waits)
k->spawn("/bin/program");             // Run program (returns immediately)

// With arguments
char *argv[] = {"/bin/prog", "arg1", "arg2"};
k->exec_args("/bin/prog", 3, argv);
k->spawn_args("/bin/prog", 3, argv);
```

## Timing

```c
k->sleep_ms(1000);                    // Sleep 1 second
unsigned long ticks = k->get_uptime_ticks();  // 100 ticks/sec
k->wfi();                             // Wait for interrupt (low power)
```

## Date/Time

```c
uint32_t unix_ts = k->get_timestamp();

int year, month, day, hour, min, sec, weekday;
k->get_datetime(&year, &month, &day, &hour, &min, &sec, &weekday);
```

---

# GUI Programming

## Creating a Window

```c
#include "../lib/vibe.h"
#include "../lib/gfx.h"

static kapi_t *api;
static int window_id;
static uint32_t *buffer;
static int win_w, win_h;
static gfx_ctx_t gfx;

int main(kapi_t *kapi, int argc, char **argv) {
    api = kapi;

    // Check window API available
    if (!k->window_create) {
        k->puts("No window API (run from desktop)\n");
        return 1;
    }

    // Create window: x, y, width, height, title
    window_id = k->window_create(100, 100, 400, 300, "My App");
    if (window_id < 0) {
        k->puts("Failed to create window\n");
        return 1;
    }

    // Get pixel buffer
    buffer = k->window_get_buffer(window_id, &win_w, &win_h);

    // Initialize graphics context
    gfx_init(&gfx, buffer, win_w, win_h, k->font_data);

    // ... draw and handle events ...

    k->window_destroy(window_id);
    return 0;
}
```

## Event Loop

```c
int running = 1;
while (running) {
    int event_type, data1, data2, data3;

    while (k->window_poll_event(window_id, &event_type, &data1, &data2, &data3)) {
        switch (event_type) {
            case WIN_EVENT_CLOSE:
                running = 0;
                break;

            case WIN_EVENT_KEY:
                // data1 = key code
                handle_key(data1);
                break;

            case WIN_EVENT_MOUSE_DOWN:
                // data1 = x, data2 = y, data3 = buttons
                handle_click(data1, data2, data3);
                break;

            case WIN_EVENT_MOUSE_MOVE:
                // data1 = x, data2 = y
                break;

            case WIN_EVENT_MOUSE_UP:
                break;

            case WIN_EVENT_RESIZE:
                // Re-fetch buffer
                buffer = k->window_get_buffer(window_id, &win_w, &win_h);
                gfx_init(&gfx, buffer, win_w, win_h, k->font_data);
                break;
        }
    }

    draw_everything();
    k->window_invalidate(window_id);  // Tell desktop to redraw
    k->yield();  // Let other processes run
}
```

## Drawing with gfx.h

```c
#include "../lib/gfx.h"

// Fill rectangle
gfx_fill_rect(&gfx, x, y, width, height, COLOR_WHITE);

// Draw rectangle outline
gfx_draw_rect(&gfx, x, y, width, height, COLOR_BLACK);

// Draw lines
gfx_draw_hline(&gfx, x, y, width, COLOR_BLACK);
gfx_draw_vline(&gfx, x, y, height, COLOR_BLACK);

// Draw text (8x16 bitmap font)
gfx_draw_string(&gfx, x, y, "Hello", COLOR_BLACK, COLOR_WHITE);
gfx_draw_char(&gfx, x, y, 'A', COLOR_BLACK, COLOR_WHITE);

// Clipped text (max width in pixels)
gfx_draw_string_clip(&gfx, x, y, "Long text...", fg, bg, 100);

// Pattern fill (checkerboard)
gfx_fill_pattern(&gfx, x, y, w, h, COLOR_BLACK, COLOR_WHITE);
```

## Colors

```c
#define COLOR_BLACK   0x00000000
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_RED     0x00FF0000
#define COLOR_GREEN   0x0000FF00
#define COLOR_BLUE    0x000000FF

// Custom: 0x00RRGGBB
uint32_t purple = 0x00800080;
```

## Key Codes

```c
// Printable: ASCII value (32-126)
// Special keys:
#define KEY_UP     0x100
#define KEY_DOWN   0x101
#define KEY_LEFT   0x102
#define KEY_RIGHT  0x103
#define KEY_HOME   0x104
#define KEY_END    0x105
#define KEY_DELETE 0x106
#define KEY_PGUP   0x107
#define KEY_PGDN   0x108

// Ctrl+key: key & 0x1F (e.g., Ctrl+S = 19)
// Enter: '\r' or '\n'
// Backspace: 8
// Escape: 0x1B (27)
// Tab: '\t'
```

## Mouse Buttons

```c
#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

// In mouse event:
if (data3 & MOUSE_BTN_LEFT) { /* left click */ }
if (data3 & MOUSE_BTN_RIGHT) { /* right click */ }
```

---

# Networking

## TCP Client

```c
// Resolve hostname
uint32_t ip = k->dns_resolve("example.com");
if (ip == 0) { /* failed */ }

// Or use IP directly
uint32_t ip = MAKE_IP(93,184,216,34);

// Connect
int sock = k->tcp_connect(ip, 80);
if (sock < 0) { /* failed */ }

// Send
const char *req = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
k->tcp_send(sock, req, strlen(req));

// Receive
char buf[4096];
int n;
while ((n = k->tcp_recv(sock, buf, sizeof(buf))) > 0) {
    // process buf[0..n-1]
}

// Close
k->tcp_close(sock);
```

## HTTPS (TLS)

```c
int sock = k->tls_connect(ip, 443, "example.com");
k->tls_send(sock, request, len);
int n = k->tls_recv(sock, buf, sizeof(buf));
k->tls_close(sock);
```

## Ping

```c
uint32_t ip = MAKE_IP(1,1,1,1);
if (k->net_ping(ip, 1, 1000) == 0) {
    k->puts("Pong!\n");
}
```

---

# Sound

```c
// Play WAV file from memory
k->sound_play_wav(wav_data, wav_size);

// Play raw PCM (S16LE)
k->sound_play_pcm(samples, num_samples, channels, sample_rate);
k->sound_play_pcm_async(samples, num_samples, channels, sample_rate);

// Control
k->sound_pause();
k->sound_resume();
k->sound_stop();

// Status
if (k->sound_is_playing()) { ... }
if (k->sound_is_paused()) { ... }
```

---

# System Info

```c
// Memory
size_t used = k->get_mem_used();
size_t free = k->get_mem_free();
size_t total = k->get_ram_total();

// Disk
int total_kb = k->get_disk_total();
int free_kb = k->get_disk_free();

// CPU
const char *cpu = k->get_cpu_name();
uint32_t mhz = k->get_cpu_freq_mhz();
int cores = k->get_cpu_cores();

// Processes
int count = k->get_process_count();
char name[64];
int state;
k->get_process_info(0, name, sizeof(name), &state);
```

---

# Building Your Program

1. Create `user/bin/myprogram.c`
2. Run `make` (builds everything)
3. Binary appears at `/bin/myprogram` in the disk image
4. Run with `/bin/myprogram` in the shell or from desktop

## Program Structure

```c
#include "../lib/vibe.h"

int main(kapi_t *api, int argc, char **argv) {
    // Your code here
    return 0;
}
```

That's it. No libc, no headers to include except vibe.h (and gfx.h for GUI).

---

# Tips

- **Always yield()**: In loops, call `k->yield()` so other processes run
- **Check window API**: Console programs should check `k->window_create != NULL`
- **Redraw efficiently**: Only call `window_invalidate()` when something changed
- **String functions**: `strlen`, `strcmp`, `strcpy`, `memset`, `memcpy` are in vibe.h
- **No printf**: Use `k->puts()` and build strings manually, or use the kernel's `print_int()`/`print_hex()`
