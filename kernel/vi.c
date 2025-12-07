/*
 * VibeOS vi Editor
 *
 * A minimal vi-like text editor for VibeOS.
 *
 * Modes:
 *   - NORMAL: Navigate and issue commands
 *   - INSERT: Type text
 *   - COMMAND: Type ex commands (after :)
 *
 * Normal mode commands:
 *   h, j, k, l  - Move cursor
 *   0           - Go to start of line
 *   $           - Go to end of line
 *   gg          - Go to first line
 *   G           - Go to last line
 *   i           - Insert before cursor
 *   a           - Insert after cursor
 *   o           - Open new line below
 *   O           - Open new line above
 *   x           - Delete character under cursor
 *   dd          - Delete current line
 *   :           - Enter command mode
 *
 * Command mode:
 *   :w          - Write file
 *   :q          - Quit (fails if modified)
 *   :q!         - Force quit
 *   :wq         - Write and quit
 */

#include "vi.h"
#include "console.h"
#include "keyboard.h"
#include "vfs.h"
#include "memory.h"
#include "string.h"
#include "fb.h"
#include <stdint.h>
#include <stddef.h>

// Editor limits
#define VI_MAX_LINES    1000
#define VI_MAX_LINE_LEN 256
#define VI_CMD_LEN      64

// Modes
typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND
} vi_mode_t;

// Editor state
static struct {
    char *lines[VI_MAX_LINES];  // Array of line pointers
    int line_count;             // Number of lines
    int cursor_row;             // Current line (0-based)
    int cursor_col;             // Current column (0-based)
    int scroll_offset;          // First visible line
    vi_mode_t mode;             // Current mode
    int modified;               // File has been modified
    char filename[VFS_MAX_PATH]; // Current filename
    char cmd_buffer[VI_CMD_LEN]; // Command mode buffer
    int cmd_pos;                // Command buffer position
    char status_msg[80];        // Status message
    int screen_rows;            // Available rows for text
    int screen_cols;            // Screen columns
    int pending_g;              // Waiting for second 'g' in 'gg'
    int pending_d;              // Waiting for second 'd' in 'dd'
    int initialized;            // Track if we've run before
} editor;

// Forward declarations
static void vi_draw_screen(void);
static void vi_draw_status(void);
static void vi_set_status(const char *msg);
static int vi_load_file(const char *filename);
static int vi_save_file(void);
static void vi_insert_char(char c);
static void vi_delete_char(void);
static void vi_delete_line(void);
static void vi_new_line_below(void);
static void vi_new_line_above(void);
static void vi_free_lines(void);
static int vi_line_len(int row);
static void vi_ensure_cursor_bounds(void);
static void vi_scroll_to_cursor(void);

// Initialize editor state
static void vi_init(void) {
    editor.line_count = 0;
    editor.cursor_row = 0;
    editor.cursor_col = 0;
    editor.scroll_offset = 0;
    editor.mode = MODE_NORMAL;
    editor.modified = 0;
    editor.filename[0] = '\0';
    editor.cmd_buffer[0] = '\0';
    editor.cmd_pos = 0;
    editor.status_msg[0] = '\0';
    editor.screen_rows = console_rows() - 2;  // Reserve 2 lines for status
    editor.screen_cols = console_cols();

    // Safety: ensure reasonable screen dimensions
    if (editor.screen_rows < 1) editor.screen_rows = 10;
    if (editor.screen_cols < 1) editor.screen_cols = 40;
}

// Get length of a line (excluding null terminator)
static int vi_line_len(int row) {
    if (row < 0 || row >= editor.line_count) return 0;
    if (!editor.lines[row]) return 0;
    return strlen(editor.lines[row]);
}

// Ensure cursor is within valid bounds
static void vi_ensure_cursor_bounds(void) {
    if (editor.cursor_row < 0) editor.cursor_row = 0;
    if (editor.cursor_row >= editor.line_count) {
        editor.cursor_row = editor.line_count > 0 ? editor.line_count - 1 : 0;
    }

    int len = vi_line_len(editor.cursor_row);
    if (editor.cursor_col < 0) editor.cursor_col = 0;

    // In normal mode, cursor can't be past last char (except on empty line)
    if (editor.mode == MODE_NORMAL) {
        if (len > 0 && editor.cursor_col >= len) {
            editor.cursor_col = len - 1;
        } else if (len == 0) {
            editor.cursor_col = 0;
        }
    } else {
        // In insert mode, cursor can be at end of line
        if (editor.cursor_col > len) {
            editor.cursor_col = len;
        }
    }
}

// Scroll view to keep cursor visible
static void vi_scroll_to_cursor(void) {
    if (editor.cursor_row < editor.scroll_offset) {
        editor.scroll_offset = editor.cursor_row;
    }
    if (editor.cursor_row >= editor.scroll_offset + editor.screen_rows) {
        editor.scroll_offset = editor.cursor_row - editor.screen_rows + 1;
    }
}

// Draw a single line of the file
static void vi_draw_line(int screen_row, int file_row) {
    console_set_cursor(screen_row, 0);

    if (file_row >= editor.line_count) {
        // Empty line beyond file - draw tilde
        console_set_color(COLOR_CYAN, COLOR_BLACK);
        console_putc('~');
        console_set_color(COLOR_WHITE, COLOR_BLACK);
        // Clear rest of line
        for (int i = 1; i < editor.screen_cols; i++) {
            console_putc(' ');
        }
    } else {
        // Draw line content
        console_set_color(COLOR_GREEN, COLOR_BLACK);
        char *line = editor.lines[file_row];
        int len = line ? strlen(line) : 0;

        int col = 0;
        for (col = 0; col < len && col < editor.screen_cols; col++) {
            console_putc(line[col]);
        }

        // Clear rest of line
        for (; col < editor.screen_cols; col++) {
            console_putc(' ');
        }
    }
}

// Draw the entire screen
static void vi_draw_screen(void) {
    console_set_color(COLOR_WHITE, COLOR_BLACK);

    // Draw all visible lines
    for (int i = 0; i < editor.screen_rows; i++) {
        vi_draw_line(i, editor.scroll_offset + i);
    }

    // Draw status bar and command line
    vi_draw_status();

    // Position cursor
    int screen_row = editor.cursor_row - editor.scroll_offset;
    console_set_cursor(screen_row, editor.cursor_col);
}

// Draw status bar - simple version
static void vi_draw_status(void) {
    // Status bar on second to last line
    console_set_cursor(editor.screen_rows, 0);
    console_set_color(COLOR_BLACK, COLOR_WHITE);

    // Just filename and mode, no fancy padding
    console_puts(" ");
    console_puts(editor.filename[0] ? editor.filename : "[New]");
    if (editor.modified) console_puts(" *");
    console_puts(" ");

    // Command line on last line - clear it first
    console_set_cursor(editor.screen_rows + 1, 0);
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts("                    ");  // Clear 20 chars
    console_set_cursor(editor.screen_rows + 1, 0);

    if (editor.mode == MODE_COMMAND) {
        console_putc(':');
        console_puts(editor.cmd_buffer);
    } else if (editor.mode == MODE_INSERT) {
        console_puts("-- INSERT --");
    }
}

// Set status message
static void vi_set_status(const char *msg) {
    int i = 0;
    while (msg[i] && i < 79) {
        editor.status_msg[i] = msg[i];
        i++;
    }
    editor.status_msg[i] = '\0';
}

// Load a file into the editor
static int vi_load_file(const char *filename) {
    // Copy filename
    int i = 0;
    while (filename[i] && i < VFS_MAX_PATH - 1) {
        editor.filename[i] = filename[i];
        i++;
    }
    editor.filename[i] = '\0';

    // Try to open file
    vfs_node_t *file = vfs_lookup(filename);

    if (!file) {
        // New file - start with one empty line
        editor.lines[0] = malloc(VI_MAX_LINE_LEN);
        if (!editor.lines[0]) return -1;
        editor.lines[0][0] = '\0';
        editor.line_count = 1;
        vi_set_status("New file");
        return 0;
    }

    if (vfs_is_dir(file)) {
        vi_set_status("Cannot edit a directory");
        return -1;
    }

    // Read file content
    size_t file_size = file->size;
    char *content = malloc(file_size + 1);
    if (!content) return -1;

    int bytes = vfs_read(file, content, file_size, 0);
    if (bytes < 0) {
        free(content);
        return -1;
    }
    content[bytes] = '\0';

    // Parse into lines
    char *p = content;
    editor.line_count = 0;

    while (*p && editor.line_count < VI_MAX_LINES) {
        // Find end of line
        char *start = p;
        while (*p && *p != '\n') {
            p++;
        }

        // Allocate and copy line
        int len = p - start;
        if (len >= VI_MAX_LINE_LEN) len = VI_MAX_LINE_LEN - 1;

        editor.lines[editor.line_count] = malloc(VI_MAX_LINE_LEN);
        if (!editor.lines[editor.line_count]) {
            free(content);
            return -1;
        }

        for (int j = 0; j < len; j++) {
            editor.lines[editor.line_count][j] = start[j];
        }
        editor.lines[editor.line_count][len] = '\0';
        editor.line_count++;

        // Skip newline
        if (*p == '\n') p++;
    }

    free(content);

    // Ensure at least one line
    if (editor.line_count == 0) {
        editor.lines[0] = malloc(VI_MAX_LINE_LEN);
        if (!editor.lines[0]) return -1;
        editor.lines[0][0] = '\0';
        editor.line_count = 1;
    }

    return 0;
}

// Save file
static int vi_save_file(void) {
    if (!editor.filename[0]) {
        vi_set_status("No filename");
        return -1;
    }

    // Create or truncate file
    vfs_node_t *file = vfs_create(editor.filename);
    if (!file) {
        vi_set_status("Cannot save file");
        return -1;
    }

    // Build content
    // First calculate total size
    size_t total_size = 0;
    for (int i = 0; i < editor.line_count; i++) {
        total_size += strlen(editor.lines[i]) + 1;  // +1 for newline
    }

    char *content = malloc(total_size + 1);
    if (!content) {
        vi_set_status("Out of memory");
        return -1;
    }

    // Copy lines
    size_t pos = 0;
    for (int i = 0; i < editor.line_count; i++) {
        int len = strlen(editor.lines[i]);
        for (int j = 0; j < len; j++) {
            content[pos++] = editor.lines[i][j];
        }
        content[pos++] = '\n';
    }
    content[pos] = '\0';

    // Write to file
    int result = vfs_write(file, content, pos);
    free(content);

    if (result < 0) {
        vi_set_status("Write error");
        return -1;
    }

    editor.modified = 0;
    vi_set_status("Written");
    return 0;
}

// Free all line buffers
static void vi_free_lines(void) {
    for (int i = 0; i < editor.line_count; i++) {
        if (editor.lines[i]) {
            free(editor.lines[i]);
            editor.lines[i] = NULL;
        }
    }
    editor.line_count = 0;
}

// Insert a character at cursor position
static void vi_insert_char(char c) {
    if (editor.cursor_row >= editor.line_count) return;

    char *line = editor.lines[editor.cursor_row];
    if (!line) return;
    int len = strlen(line);

    if (len >= VI_MAX_LINE_LEN - 1) return;  // Line too long

    // Shift characters right
    for (int i = len; i >= editor.cursor_col; i--) {
        line[i + 1] = line[i];
    }

    // Insert character
    line[editor.cursor_col] = c;
    editor.cursor_col++;
    editor.modified = 1;
}

// Handle Enter key in insert mode
static void vi_insert_newline(void) {
    if (editor.line_count >= VI_MAX_LINES) return;

    char *line = editor.lines[editor.cursor_row];
    int len = strlen(line);

    // Allocate new line for text after cursor
    char *new_line = malloc(VI_MAX_LINE_LEN);
    if (!new_line) return;

    // Copy text after cursor to new line
    int new_len = len - editor.cursor_col;
    for (int i = 0; i < new_len; i++) {
        new_line[i] = line[editor.cursor_col + i];
    }
    new_line[new_len] = '\0';

    // Truncate current line at cursor
    line[editor.cursor_col] = '\0';

    // Shift lines down
    for (int i = editor.line_count; i > editor.cursor_row + 1; i--) {
        editor.lines[i] = editor.lines[i - 1];
    }

    // Insert new line
    editor.lines[editor.cursor_row + 1] = new_line;
    editor.line_count++;

    // Move cursor to start of new line
    editor.cursor_row++;
    editor.cursor_col = 0;
    editor.modified = 1;
}

// Delete character under cursor (x command)
static void vi_delete_char(void) {
    if (editor.cursor_row >= editor.line_count) return;

    char *line = editor.lines[editor.cursor_row];
    int len = strlen(line);

    if (len == 0 || editor.cursor_col >= len) return;

    // Shift characters left
    for (int i = editor.cursor_col; i < len; i++) {
        line[i] = line[i + 1];
    }

    editor.modified = 1;
    vi_ensure_cursor_bounds();
}

// Delete character before cursor (backspace in insert mode)
static void vi_delete_char_before(void) {
    if (editor.cursor_col > 0) {
        // Delete char before cursor on same line
        editor.cursor_col--;
        vi_delete_char();
    } else if (editor.cursor_row > 0) {
        // Join with previous line
        char *prev_line = editor.lines[editor.cursor_row - 1];
        char *curr_line = editor.lines[editor.cursor_row];
        int prev_len = strlen(prev_line);
        int curr_len = strlen(curr_line);

        if (prev_len + curr_len < VI_MAX_LINE_LEN) {
            // Append current line to previous
            for (int i = 0; i <= curr_len; i++) {
                prev_line[prev_len + i] = curr_line[i];
            }

            // Free current line and shift lines up
            free(curr_line);
            for (int i = editor.cursor_row; i < editor.line_count - 1; i++) {
                editor.lines[i] = editor.lines[i + 1];
            }
            editor.lines[editor.line_count - 1] = NULL;
            editor.line_count--;

            // Position cursor at join point
            editor.cursor_row--;
            editor.cursor_col = prev_len;
            editor.modified = 1;
        }
    }
}

// Delete current line (dd command)
static void vi_delete_line(void) {
    if (editor.line_count <= 1) {
        // Can't delete last line, just clear it
        editor.lines[0][0] = '\0';
        editor.cursor_col = 0;
        editor.modified = 1;
        return;
    }

    // Free the line
    free(editor.lines[editor.cursor_row]);

    // Shift lines up
    for (int i = editor.cursor_row; i < editor.line_count - 1; i++) {
        editor.lines[i] = editor.lines[i + 1];
    }
    editor.lines[editor.line_count - 1] = NULL;
    editor.line_count--;

    vi_ensure_cursor_bounds();
    editor.modified = 1;
}

// Open new line below cursor (o command)
static void vi_new_line_below(void) {
    if (editor.line_count >= VI_MAX_LINES) return;

    // Shift lines down
    for (int i = editor.line_count; i > editor.cursor_row + 1; i--) {
        editor.lines[i] = editor.lines[i - 1];
    }

    // Create new empty line
    editor.lines[editor.cursor_row + 1] = malloc(VI_MAX_LINE_LEN);
    if (!editor.lines[editor.cursor_row + 1]) return;
    editor.lines[editor.cursor_row + 1][0] = '\0';
    editor.line_count++;

    // Move cursor to new line
    editor.cursor_row++;
    editor.cursor_col = 0;
    editor.mode = MODE_INSERT;
    editor.modified = 1;
}

// Open new line above cursor (O command)
static void vi_new_line_above(void) {
    if (editor.line_count >= VI_MAX_LINES) return;

    // Shift lines down
    for (int i = editor.line_count; i > editor.cursor_row; i--) {
        editor.lines[i] = editor.lines[i - 1];
    }

    // Create new empty line at cursor position
    editor.lines[editor.cursor_row] = malloc(VI_MAX_LINE_LEN);
    if (!editor.lines[editor.cursor_row]) return;
    editor.lines[editor.cursor_row][0] = '\0';
    editor.line_count++;

    // Cursor stays on new line
    editor.cursor_col = 0;
    editor.mode = MODE_INSERT;
    editor.modified = 1;
}

// Process command mode input
static int vi_process_command(void) {
    char *cmd = editor.cmd_buffer;

    // :q - quit
    if (strcmp(cmd, "q") == 0) {
        if (editor.modified) {
            vi_set_status("No write since last change (use :q! to override)");
            return 0;
        }
        return 1;  // Exit
    }

    // :q! - force quit
    if (strcmp(cmd, "q!") == 0) {
        return 1;  // Exit
    }

    // :w - write
    if (strcmp(cmd, "w") == 0) {
        vi_save_file();
        return 0;
    }

    // :wq - write and quit
    if (strcmp(cmd, "wq") == 0) {
        if (vi_save_file() == 0) {
            return 1;  // Exit
        }
        return 0;
    }

    // :x - same as :wq
    if (strcmp(cmd, "x") == 0) {
        if (editor.modified) {
            if (vi_save_file() != 0) {
                return 0;
            }
        }
        return 1;  // Exit
    }

    vi_set_status("Unknown command");
    return 0;
}

// Handle normal mode key
static int vi_handle_normal(int c) {
    // Clear status on any key
    editor.status_msg[0] = '\0';

    // Handle pending 'g' for 'gg'
    if (editor.pending_g) {
        editor.pending_g = 0;
        if (c == 'g') {
            // gg - go to first line
            editor.cursor_row = 0;
            editor.cursor_col = 0;
            vi_ensure_cursor_bounds();
            return 0;
        }
        // Not 'g', ignore the pending g
    }

    // Handle pending 'd' for 'dd'
    if (editor.pending_d) {
        editor.pending_d = 0;
        if (c == 'd') {
            // dd - delete line
            vi_delete_line();
            return 0;
        }
        // Not 'd', ignore the pending d
    }

    switch (c) {
        // Movement
        case 'h':
            if (editor.cursor_col > 0) editor.cursor_col--;
            break;
        case 'j':
            if (editor.cursor_row < editor.line_count - 1) {
                editor.cursor_row++;
                vi_ensure_cursor_bounds();
            }
            break;
        case 'k':
            if (editor.cursor_row > 0) {
                editor.cursor_row--;
                vi_ensure_cursor_bounds();
            }
            break;
        case 'l':
            if (editor.cursor_col < vi_line_len(editor.cursor_row) - 1) {
                editor.cursor_col++;
            }
            break;

        // Line navigation
        case '0':
            editor.cursor_col = 0;
            break;
        case '$':
            editor.cursor_col = vi_line_len(editor.cursor_row);
            if (editor.cursor_col > 0) editor.cursor_col--;
            break;

        // File navigation
        case 'g':
            editor.pending_g = 1;
            break;
        case 'G':
            editor.cursor_row = editor.line_count - 1;
            editor.cursor_col = 0;
            vi_ensure_cursor_bounds();
            break;

        // Insert mode
        case 'i':
            editor.mode = MODE_INSERT;
            break;
        case 'a':
            editor.mode = MODE_INSERT;
            if (vi_line_len(editor.cursor_row) > 0) {
                editor.cursor_col++;
            }
            break;
        case 'o':
            vi_new_line_below();
            break;
        case 'O':
            vi_new_line_above();
            break;

        // Deletion
        case 'x':
            vi_delete_char();
            break;
        case 'd':
            editor.pending_d = 1;
            break;

        // Command mode
        case ':':
            editor.mode = MODE_COMMAND;
            editor.cmd_buffer[0] = '\0';
            editor.cmd_pos = 0;
            break;
    }

    return 0;
}

// Handle insert mode key
static void vi_handle_insert(int c) {
    if (c == 27) {  // Escape
        editor.mode = MODE_NORMAL;
        if (editor.cursor_col > 0) editor.cursor_col--;
        vi_ensure_cursor_bounds();
    } else if (c == '\r' || c == '\n') {
        vi_insert_newline();
    } else if (c == '\b' || c == 127) {
        vi_delete_char_before();
    } else if (c >= 32 && c < 127) {
        vi_insert_char((char)c);
    }
}

// Handle command mode key
static int vi_handle_command(int c) {
    if (c == 27) {  // Escape
        editor.mode = MODE_NORMAL;
        editor.cmd_buffer[0] = '\0';
        editor.cmd_pos = 0;
    } else if (c == '\r' || c == '\n') {
        editor.mode = MODE_NORMAL;
        int result = vi_process_command();
        editor.cmd_buffer[0] = '\0';
        editor.cmd_pos = 0;
        return result;
    } else if (c == '\b' || c == 127) {
        if (editor.cmd_pos > 0) {
            editor.cmd_pos--;
            editor.cmd_buffer[editor.cmd_pos] = '\0';
        }
    } else if (c >= 32 && c < 127) {
        if (editor.cmd_pos < VI_CMD_LEN - 1) {
            editor.cmd_buffer[editor.cmd_pos++] = (char)c;
            editor.cmd_buffer[editor.cmd_pos] = '\0';
        }
    }
    return 0;
}

// Main editor function
int vi_edit(const char *filename) {
    // Initialize
    vi_init();

    // Load file
    if (vi_load_file(filename) < 0) {
        return -1;
    }

    // Clear screen and draw
    console_clear();
    vi_draw_screen();

    // Main loop
    int quit = 0;
    while (!quit) {
        int c = keyboard_getc();

        if (c < 0) {
            continue;  // No key, keep polling
        }

        // Handle key based on mode
        switch (editor.mode) {
            case MODE_NORMAL:
                quit = vi_handle_normal(c);
                break;
            case MODE_INSERT:
                vi_handle_insert(c);
                break;
            case MODE_COMMAND:
                quit = vi_handle_command(c);
                break;
        }

        // Update scroll and cursor bounds
        vi_ensure_cursor_bounds();
        vi_scroll_to_cursor();

        // Redraw
        vi_draw_screen();
    }

    // Clean up
    vi_free_lines();
    console_clear();

    return 0;
}
