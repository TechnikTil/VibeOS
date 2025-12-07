/*
 * VibeOS Context Switch
 *
 * Saves callee-saved registers and switches stacks between processes.
 * AArch64 calling convention: x19-x28 are callee-saved, x29 is FP, x30 is LR
 */

.global context_switch

/*
 * void context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx)
 *
 * old_ctx: x0 - where to save current context
 * new_ctx: x1 - context to restore
 *
 * cpu_context_t layout (13 * 8 = 104 bytes):
 *   0x00: x19
 *   0x08: x20
 *   0x10: x21
 *   0x18: x22
 *   0x20: x23
 *   0x28: x24
 *   0x30: x25
 *   0x38: x26
 *   0x40: x27
 *   0x48: x28
 *   0x50: x29 (FP)
 *   0x58: x30 (LR - return address)
 *   0x60: sp
 */

context_switch:
    // Save current context to old_ctx (x0)
    // If old_ctx is NULL, skip saving
    cbz     x0, 1f

    stp     x19, x20, [x0, #0x00]
    stp     x21, x22, [x0, #0x10]
    stp     x23, x24, [x0, #0x20]
    stp     x25, x26, [x0, #0x30]
    stp     x27, x28, [x0, #0x40]
    stp     x29, x30, [x0, #0x50]
    mov     x2, sp
    str     x2, [x0, #0x60]

1:
    // Restore context from new_ctx (x1)
    ldp     x19, x20, [x1, #0x00]
    ldp     x21, x22, [x1, #0x10]
    ldp     x23, x24, [x1, #0x20]
    ldp     x25, x26, [x1, #0x30]
    ldp     x27, x28, [x1, #0x40]
    ldp     x29, x30, [x1, #0x50]
    ldr     x2, [x1, #0x60]
    mov     sp, x2

    // Return to new process (x30 has return address)
    ret
