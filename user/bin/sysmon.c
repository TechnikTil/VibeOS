/*
 * VibeOS System Monitor
 *
 * Classic Mac-style system monitor showing uptime and memory usage.
 * Runs in a desktop window.
 */

#include "../lib/vibe.h"

static kapi_t *api;
static int window_id = -1;
static uint32_t *win_buffer;
static int win_w, win_h;

// Window content dimensions
#define CONTENT_W 200
#define CONTENT_H 120

// ============ Drawing Helpers ============

static void buf_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int py = y; py < y + h && py < win_h; py++) {
        if (py < 0) continue;
        for (int px = x; px < x + w && px < win_w; px++) {
            if (px < 0) continue;
            win_buffer[py * win_w + px] = color;
        }
    }
}

static void buf_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = &api->font_data[(unsigned char)c * 16];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            uint32_t color = (glyph[row] & (0x80 >> col)) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < win_w && py >= 0 && py < win_h) {
                win_buffer[py * win_w + px] = color;
            }
        }
    }
}

static void buf_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    while (*s) {
        buf_draw_char(x, y, *s, fg, bg);
        x += 8;
        s++;
    }
}

static void buf_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) {
        if (x + i >= 0 && x + i < win_w) {
            if (y >= 0 && y < win_h) win_buffer[y * win_w + x + i] = color;
            if (y + h - 1 >= 0 && y + h - 1 < win_h) win_buffer[(y + h - 1) * win_w + x + i] = color;
        }
    }
    for (int i = 0; i < h; i++) {
        if (y + i >= 0 && y + i < win_h) {
            if (x >= 0 && x < win_w) win_buffer[(y + i) * win_w + x] = color;
            if (x + w - 1 >= 0 && x + w - 1 < win_w) win_buffer[(y + i) * win_w + x + w - 1] = color;
        }
    }
}

// ============ Formatting Helpers ============

static void format_num(char *buf, unsigned long n) {
    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[20];
    int i = 0;
    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

static void format_size(char *buf, unsigned long bytes) {
    // Format as MB with one decimal
    unsigned long mb = bytes / (1024 * 1024);
    unsigned long remainder = (bytes % (1024 * 1024)) * 10 / (1024 * 1024);

    format_num(buf, mb);
    int len = strlen(buf);
    buf[len] = '.';
    buf[len+1] = '0' + remainder;
    buf[len+2] = ' ';
    buf[len+3] = 'M';
    buf[len+4] = 'B';
    buf[len+5] = '\0';
}

static void format_uptime(char *buf, unsigned long ticks) {
    unsigned long total_seconds = ticks / 100;
    unsigned long hours = total_seconds / 3600;
    unsigned long minutes = (total_seconds % 3600) / 60;
    unsigned long seconds = total_seconds % 60;

    char tmp[8];
    int pos = 0;

    if (hours > 0) {
        format_num(tmp, hours);
        for (int i = 0; tmp[i]; i++) buf[pos++] = tmp[i];
        buf[pos++] = 'h';
        buf[pos++] = ' ';
    }

    format_num(tmp, minutes);
    for (int i = 0; tmp[i]; i++) buf[pos++] = tmp[i];
    buf[pos++] = 'm';
    buf[pos++] = ' ';

    format_num(tmp, seconds);
    for (int i = 0; tmp[i]; i++) buf[pos++] = tmp[i];
    buf[pos++] = 's';
    buf[pos] = '\0';
}

// ============ Drawing ============

static void draw_progress_bar(int x, int y, int w, int h, int percent) {
    // Background
    buf_fill_rect(x, y, w, h, COLOR_WHITE);
    buf_draw_rect(x, y, w, h, COLOR_BLACK);

    // Fill
    int fill_w = (w - 2) * percent / 100;
    if (fill_w > 0) {
        // Classic Mac diagonal stripes pattern
        for (int py = y + 1; py < y + h - 1; py++) {
            for (int px = x + 1; px < x + 1 + fill_w; px++) {
                if ((px + py) % 2 == 0) {
                    win_buffer[py * win_w + px] = COLOR_BLACK;
                }
            }
        }
    }
}

static void draw_all(void) {
    // Clear background
    buf_fill_rect(0, 0, win_w, win_h, COLOR_WHITE);

    // Get system info
    unsigned long ticks = api->get_uptime_ticks();
    unsigned long mem_used = api->get_mem_used();
    unsigned long mem_free = api->get_mem_free();
    unsigned long mem_total = mem_used + mem_free;
    int mem_percent = (int)((mem_used * 100) / mem_total);

    char buf[64];
    int y = 8;

    // Uptime section
    buf_draw_string(8, y, "Uptime:", COLOR_BLACK, COLOR_WHITE);
    y += 18;
    format_uptime(buf, ticks);
    buf_draw_string(16, y, buf, COLOR_BLACK, COLOR_WHITE);
    y += 24;

    // Memory section
    buf_draw_string(8, y, "Memory:", COLOR_BLACK, COLOR_WHITE);
    y += 18;

    // Progress bar
    draw_progress_bar(16, y, CONTENT_W - 32, 14, mem_percent);
    y += 18;

    // Used/Total
    format_size(buf, mem_used);
    buf_draw_string(16, y, "Used: ", COLOR_BLACK, COLOR_WHITE);
    buf_draw_string(16 + 6*8, y, buf, COLOR_BLACK, COLOR_WHITE);
    y += 16;

    format_size(buf, mem_free);
    buf_draw_string(16, y, "Free: ", COLOR_BLACK, COLOR_WHITE);
    buf_draw_string(16 + 6*8, y, buf, COLOR_BLACK, COLOR_WHITE);

    api->window_invalidate(window_id);
}

// ============ Main ============

int main(kapi_t *kapi, int argc, char **argv) {
    (void)argc;
    (void)argv;

    api = kapi;

    // Wait for window API to be available
    if (!api->window_create) {
        api->puts("sysmon: window API not available (desktop not running?)\n");
        return 1;
    }

    // Create window
    window_id = api->window_create(300, 150, CONTENT_W, CONTENT_H + 18, "System Monitor");
    if (window_id < 0) {
        api->puts("sysmon: failed to create window\n");
        return 1;
    }

    // Get buffer
    win_buffer = api->window_get_buffer(window_id, &win_w, &win_h);
    if (!win_buffer) {
        api->puts("sysmon: failed to get window buffer\n");
        api->window_destroy(window_id);
        return 1;
    }

    // Initial draw
    draw_all();

    // Event loop with periodic refresh
    int running = 1;
    int refresh_counter = 0;

    while (running) {
        int event_type, data1, data2, data3;
        while (api->window_poll_event(window_id, &event_type, &data1, &data2, &data3)) {
            switch (event_type) {
                case WIN_EVENT_CLOSE:
                    running = 0;
                    break;
                case WIN_EVENT_KEY:
                    if (data1 == 'q' || data1 == 'Q') {
                        running = 0;
                    }
                    break;
            }
        }

        // Refresh display every ~500ms (30 frames * 16ms ≈ 480ms)
        refresh_counter++;
        if (refresh_counter >= 30) {
            refresh_counter = 0;
            draw_all();
        }

        // Yield to other processes
        api->yield();
    }

    api->window_destroy(window_id);
    return 0;
}
