/*
 * VibeOS TextEdit
 *
 * Simple text editor in a window. No modes, just type.
 * Usage: textedit [filename]
 */

#include "../lib/vibe.h"

static kapi_t *api;
static int window_id = -1;
static uint32_t *win_buffer;
static int win_w, win_h;

// Editor dimensions
#define WINDOW_W 500
#define WINDOW_H 350
#define TITLE_BAR_H 18
#define CONTENT_X 4
#define CONTENT_Y 4
#define CHAR_W 8
#define CHAR_H 16

// Text buffer
#define MAX_LINES 256
#define MAX_LINE_LEN 256
#define MAX_TEXT_SIZE (MAX_LINES * MAX_LINE_LEN)

static char text_buffer[MAX_TEXT_SIZE];
static int text_len = 0;
static int cursor_pos = 0;
static int scroll_offset = 0;  // First visible line

// Current file
static char current_file[256];
static int modified = 0;

// Save As modal state
static int save_as_mode = 0;
static char save_as_buf[256];
static int save_as_len = 0;

// Visible area
static int visible_cols;
static int visible_rows;

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

// ============ Text Buffer Helpers ============

// Get line number and column from cursor position
static void cursor_to_line_col(int pos, int *line, int *col) {
    *line = 0;
    *col = 0;
    for (int i = 0; i < pos && i < text_len; i++) {
        if (text_buffer[i] == '\n') {
            (*line)++;
            *col = 0;
        } else {
            (*col)++;
        }
    }
}

// Get cursor position from line and column
static int line_col_to_cursor(int line, int col) {
    int current_line = 0;
    int current_col = 0;
    int i;

    for (i = 0; i < text_len; i++) {
        if (current_line == line && current_col == col) {
            return i;
        }
        if (text_buffer[i] == '\n') {
            if (current_line == line) {
                // Requested column is past end of line
                return i;
            }
            current_line++;
            current_col = 0;
        } else {
            current_col++;
        }
    }

    // End of buffer
    return i;
}

// Get start of line containing pos
static int line_start(int pos) {
    while (pos > 0 && text_buffer[pos - 1] != '\n') {
        pos--;
    }
    return pos;
}

// Get end of line containing pos
static int line_end(int pos) {
    while (pos < text_len && text_buffer[pos] != '\n') {
        pos++;
    }
    return pos;
}

// Count total lines
static int count_lines(void) {
    int lines = 1;
    for (int i = 0; i < text_len; i++) {
        if (text_buffer[i] == '\n') lines++;
    }
    return lines;
}

// Insert character at cursor
static void insert_char(char c) {
    if (text_len >= MAX_TEXT_SIZE - 1) return;

    // Shift everything after cursor
    for (int i = text_len; i > cursor_pos; i--) {
        text_buffer[i] = text_buffer[i - 1];
    }
    text_buffer[cursor_pos] = c;
    text_len++;
    cursor_pos++;
    modified = 1;
}

// Delete character before cursor (backspace)
static void delete_char_before(void) {
    if (cursor_pos == 0) return;

    // Shift everything after cursor back
    for (int i = cursor_pos - 1; i < text_len - 1; i++) {
        text_buffer[i] = text_buffer[i + 1];
    }
    text_len--;
    cursor_pos--;
    modified = 1;
}

// Delete character at cursor (delete key)
static void delete_char_at(void) {
    if (cursor_pos >= text_len) return;

    for (int i = cursor_pos; i < text_len - 1; i++) {
        text_buffer[i] = text_buffer[i + 1];
    }
    text_len--;
    modified = 1;
}

// ============ File Operations ============

static void load_file(const char *path) {
    void *file = api->open(path);
    if (!file) {
        text_len = 0;
        cursor_pos = 0;
        return;
    }

    if (api->is_dir(file)) {
        text_len = 0;
        cursor_pos = 0;
        return;
    }

    int bytes = api->read(file, text_buffer, MAX_TEXT_SIZE - 1, 0);
    if (bytes > 0) {
        text_len = bytes;
        text_buffer[text_len] = '\0';
    } else {
        text_len = 0;
    }
    cursor_pos = 0;
    modified = 0;
}

static int save_failed = 0;  // Show error in status bar

static void open_save_as(void) {
    save_as_mode = 1;
    save_as_len = 0;
    save_as_buf[0] = '\0';
    // Pre-fill with current filename if exists
    if (current_file[0]) {
        for (int i = 0; current_file[i] && i < 255; i++) {
            save_as_buf[i] = current_file[i];
            save_as_len++;
        }
        save_as_buf[save_as_len] = '\0';
    }
}

static void do_save(const char *path) {
    void *file = api->open(path);
    if (!file) {
        file = api->create(path);
    }
    if (!file) {
        save_failed = 1;
        return;
    }

    api->write(file, text_buffer, text_len);

    // Update current filename
    int i;
    for (i = 0; path[i] && i < 255; i++) {
        current_file[i] = path[i];
    }
    current_file[i] = '\0';

    // Update window title
    api->window_set_title(window_id, current_file);

    modified = 0;
    save_failed = 0;
}

static void save_file(void) {
    if (current_file[0] == '\0') {
        // No filename - open Save As dialog
        open_save_as();
        return;
    }

    do_save(current_file);
}

// ============ Drawing ============

static void draw_save_as_modal(void) {
    // Modal dimensions
    int modal_w = 300;
    int modal_h = 80;
    int modal_x = (win_w - modal_w) / 2;
    int modal_y = (win_h - modal_h) / 2;

    // Draw shadow
    buf_fill_rect(modal_x + 3, modal_y + 3, modal_w, modal_h, 0x00888888);

    // Draw modal background
    buf_fill_rect(modal_x, modal_y, modal_w, modal_h, COLOR_WHITE);

    // Draw border
    buf_fill_rect(modal_x, modal_y, modal_w, 1, COLOR_BLACK);
    buf_fill_rect(modal_x, modal_y + modal_h - 1, modal_w, 1, COLOR_BLACK);
    buf_fill_rect(modal_x, modal_y, 1, modal_h, COLOR_BLACK);
    buf_fill_rect(modal_x + modal_w - 1, modal_y, 1, modal_h, COLOR_BLACK);

    // Draw title
    buf_draw_string(modal_x + 8, modal_y + 8, "Save As:", COLOR_BLACK, COLOR_WHITE);

    // Draw text input box
    int input_x = modal_x + 8;
    int input_y = modal_y + 28;
    int input_w = modal_w - 16;
    int input_h = 20;

    buf_fill_rect(input_x, input_y, input_w, input_h, COLOR_WHITE);
    buf_fill_rect(input_x, input_y, input_w, 1, COLOR_BLACK);
    buf_fill_rect(input_x, input_y + input_h - 1, input_w, 1, COLOR_BLACK);
    buf_fill_rect(input_x, input_y, 1, input_h, COLOR_BLACK);
    buf_fill_rect(input_x + input_w - 1, input_y, 1, input_h, COLOR_BLACK);

    // Draw filename text
    buf_draw_string(input_x + 4, input_y + 2, save_as_buf, COLOR_BLACK, COLOR_WHITE);

    // Draw cursor
    int cursor_x = input_x + 4 + save_as_len * CHAR_W;
    buf_fill_rect(cursor_x, input_y + 2, CHAR_W, CHAR_H, COLOR_BLACK);

    // Draw hint
    buf_draw_string(modal_x + 8, modal_y + 56, "Enter=Save  Esc=Cancel", COLOR_BLACK, COLOR_WHITE);
}

static void draw_all(void) {
    // Clear background (white)
    buf_fill_rect(0, 0, win_w, win_h, COLOR_WHITE);

    // Draw border
    buf_fill_rect(0, 0, win_w, 1, COLOR_BLACK);
    buf_fill_rect(0, win_h - 1, win_w, 1, COLOR_BLACK);
    buf_fill_rect(0, 0, 1, win_h, COLOR_BLACK);
    buf_fill_rect(win_w - 1, 0, 1, win_h, COLOR_BLACK);

    // Get cursor line/col for scroll adjustment
    int cursor_line, cursor_col;
    cursor_to_line_col(cursor_pos, &cursor_line, &cursor_col);

    // Adjust scroll to keep cursor visible
    if (cursor_line < scroll_offset) {
        scroll_offset = cursor_line;
    } else if (cursor_line >= scroll_offset + visible_rows) {
        scroll_offset = cursor_line - visible_rows + 1;
    }

    // Draw text
    int x = CONTENT_X;
    int y = CONTENT_Y;
    int current_line = 0;
    int col = 0;

    for (int i = 0; i <= text_len; i++) {
        // Draw cursor
        if (i == cursor_pos && current_line >= scroll_offset && current_line < scroll_offset + visible_rows) {
            int cy = CONTENT_Y + (current_line - scroll_offset) * CHAR_H;
            int cx = CONTENT_X + col * CHAR_W;
            // Inverse block cursor
            buf_fill_rect(cx, cy, CHAR_W, CHAR_H, COLOR_BLACK);
            if (i < text_len && text_buffer[i] != '\n') {
                buf_draw_char(cx, cy, text_buffer[i], COLOR_WHITE, COLOR_BLACK);
            }
        }

        if (i >= text_len) break;

        char c = text_buffer[i];

        if (c == '\n') {
            current_line++;
            col = 0;
        } else {
            // Only draw if in visible area and not at cursor (cursor already drawn)
            if (current_line >= scroll_offset && current_line < scroll_offset + visible_rows) {
                if (i != cursor_pos) {
                    int cy = CONTENT_Y + (current_line - scroll_offset) * CHAR_H;
                    int cx = CONTENT_X + col * CHAR_W;
                    if (cx + CHAR_W <= win_w - CONTENT_X) {
                        buf_draw_char(cx, cy, c, COLOR_BLACK, COLOR_WHITE);
                    }
                }
            }
            col++;
        }
    }

    // Draw status bar at bottom
    int status_y = win_h - CHAR_H - 2;
    buf_fill_rect(1, status_y - 1, win_w - 2, CHAR_H + 2, 0x00DDDDDD);

    // Status text: filename and position
    char status[64];
    int si = 0;

    // Save failed indicator
    if (save_failed) {
        const char *err = "[No filename] ";
        for (int i = 0; err[i]; i++) status[si++] = err[i];
    }

    // Modified indicator
    if (modified) {
        status[si++] = '*';
    }

    // Filename
    const char *fname = current_file[0] ? current_file : "untitled";
    for (int i = 0; fname[i] && si < 40; i++) {
        status[si++] = fname[i];
    }

    status[si++] = ' ';
    status[si++] = '-';
    status[si++] = ' ';
    status[si++] = 'L';

    // Line number
    char num[8];
    int n = cursor_line + 1;
    int ni = 0;
    if (n == 0) {
        num[ni++] = '0';
    } else {
        char tmp[8];
        int ti = 0;
        while (n > 0) {
            tmp[ti++] = '0' + (n % 10);
            n /= 10;
        }
        while (ti > 0) {
            num[ni++] = tmp[--ti];
        }
    }
    for (int i = 0; i < ni && si < 60; i++) {
        status[si++] = num[i];
    }

    status[si++] = ':';

    // Column number
    n = cursor_col + 1;
    ni = 0;
    if (n == 0) {
        num[ni++] = '0';
    } else {
        char tmp[8];
        int ti = 0;
        while (n > 0) {
            tmp[ti++] = '0' + (n % 10);
            n /= 10;
        }
        while (ti > 0) {
            num[ni++] = tmp[--ti];
        }
    }
    for (int i = 0; i < ni && si < 63; i++) {
        status[si++] = num[i];
    }

    status[si] = '\0';

    buf_draw_string(4, status_y, status, COLOR_BLACK, 0x00DDDDDD);

    // Draw Save As modal if active
    if (save_as_mode) {
        draw_save_as_modal();
    }

    api->window_invalidate(window_id);
}

// ============ Input Handling ============

static void handle_save_as_key(int key) {
    switch (key) {
        case '\r':
        case '\n':
            // Confirm save
            if (save_as_len > 0) {
                save_as_buf[save_as_len] = '\0';
                do_save(save_as_buf);
                save_as_mode = 0;
            }
            break;

        case 0x1B: // Escape - cancel
            save_as_mode = 0;
            break;

        case 8: // Backspace
            if (save_as_len > 0) {
                save_as_len--;
                save_as_buf[save_as_len] = '\0';
            }
            break;

        default:
            // Add printable characters
            if (key >= 32 && key < 127 && save_as_len < 250) {
                save_as_buf[save_as_len++] = (char)key;
                save_as_buf[save_as_len] = '\0';
            }
            break;
    }
}

static void handle_key(int key) {
    // If Save As modal is open, handle those keys
    if (save_as_mode) {
        handle_save_as_key(key);
        return;
    }

    int line, col;

    switch (key) {
        case '\r':
        case '\n':
            insert_char('\n');
            break;

        case 8:   // Backspace
            delete_char_before();
            break;

        case 0x106: // Delete key
            delete_char_at();
            break;

        case 0x1B: // Escape - could use for menu later
            break;

        // Arrow keys (special codes from keyboard driver)
        case 0x100: // Up
            cursor_to_line_col(cursor_pos, &line, &col);
            if (line > 0) {
                cursor_pos = line_col_to_cursor(line - 1, col);
            }
            break;

        case 0x101: // Down
            cursor_to_line_col(cursor_pos, &line, &col);
            cursor_pos = line_col_to_cursor(line + 1, col);
            if (cursor_pos > text_len) cursor_pos = text_len;
            break;

        case 0x102: // Left
            if (cursor_pos > 0) cursor_pos--;
            break;

        case 0x103: // Right
            if (cursor_pos < text_len) cursor_pos++;
            break;

        case 0x104: // Home
            cursor_pos = line_start(cursor_pos);
            break;

        case 0x105: // End
            cursor_pos = line_end(cursor_pos);
            break;

        case 19: // Ctrl+S
            save_file();
            break;

        default:
            if (key >= 32 && key < 127) {
                insert_char((char)key);
            }
            break;
    }
}

// ============ Main ============

int main(kapi_t *kapi, int argc, char **argv) {
    api = kapi;

    // Initialize
    text_len = 0;
    cursor_pos = 0;
    scroll_offset = 0;
    modified = 0;
    current_file[0] = '\0';

    // Load file if specified
    if (argc > 1) {
        // Copy filename
        int i;
        for (i = 0; argv[1][i] && i < 255; i++) {
            current_file[i] = argv[1][i];
        }
        current_file[i] = '\0';
        load_file(current_file);
    }

    // Check for window API
    if (!api->window_create) {
        api->puts("textedit: window API not available (run from desktop)\n");
        return 1;
    }

    // Create window
    const char *title = current_file[0] ? current_file : "TextEdit";
    window_id = api->window_create(50, 50, WINDOW_W, WINDOW_H + TITLE_BAR_H, title);
    if (window_id < 0) {
        api->puts("textedit: failed to create window\n");
        return 1;
    }

    // Get buffer
    win_buffer = api->window_get_buffer(window_id, &win_w, &win_h);
    if (!win_buffer) {
        api->puts("textedit: failed to get window buffer\n");
        api->window_destroy(window_id);
        return 1;
    }

    // Calculate visible area
    visible_cols = (win_w - CONTENT_X * 2) / CHAR_W;
    visible_rows = (win_h - CONTENT_Y * 2 - CHAR_H - 4) / CHAR_H;  // Account for status bar

    // Initial draw
    draw_all();

    // Event loop
    int running = 1;
    while (running) {
        int event_type, data1, data2, data3;
        while (api->window_poll_event(window_id, &event_type, &data1, &data2, &data3)) {
            switch (event_type) {
                case WIN_EVENT_CLOSE:
                    running = 0;
                    break;

                case WIN_EVENT_KEY:
                    handle_key(data1);
                    draw_all();
                    break;
            }
        }

        api->yield();
    }

    api->window_destroy(window_id);
    return 0;
}
