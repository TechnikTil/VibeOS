// Stubs for MicroPython on VibeOS

#include <stddef.h>
#include "py/mphal.h"
#include "py/obj.h"

// strchr needs to be a real function (not just inline) because objstr.c uses it
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : (void *)0;
}

// VT100 stubs - VibeOS console doesn't support escape codes
void mp_hal_move_cursor_back(unsigned int pos) {
    while (pos--) {
        mp_hal_stdout_tx_strn("\b", 1);
    }
}

void mp_hal_erase_line_from_cursor(unsigned int n_chars) {
    for (unsigned int i = 0; i < n_chars; i++) {
        mp_hal_stdout_tx_strn(" ", 1);
    }
    mp_hal_move_cursor_back(n_chars);
}

// Math function stubs (for float support)
double nan(const char *s) { (void)s; return 0.0 / 0.0; }
double atan2(double y, double x) { (void)y; (void)x; return 0.0; }  // TODO: real impl
double copysign(double x, double y) {
    // Copy sign of y to x using bit manipulation
    union { double d; unsigned long long i; } ux = {x}, uy = {y};
    ux.i = (ux.i & 0x7FFFFFFFFFFFFFFFULL) | (uy.i & 0x8000000000000000ULL);
    return ux.d;
}
double nearbyint(double x) {
    // Round to nearest integer
    return (double)(long long)(x + (x < 0 ? -0.5 : 0.5));
}

// sys.stdin/stdout/stderr stubs (we use our own I/O)
const mp_obj_t mp_sys_stdin_obj = MP_OBJ_NULL;
const mp_obj_t mp_sys_stdout_obj = MP_OBJ_NULL;
const mp_obj_t mp_sys_stderr_obj = MP_OBJ_NULL;
