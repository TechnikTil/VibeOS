/*
 * VibeOS Context Switch
 *
 * Saves callee-saved registers and switches stacks between processes.
 * AArch64 calling convention: x19-x28 are callee-saved, x29 is FP, x30 is LR
 * Also saves/restores all FPU state (q0-q31, fpcr, fpsr)
 */

.global context_switch

/*
 * void context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx)
 *
 * old_ctx: x0 - where to save current context
 * new_ctx: x1 - context to restore
 *
 * cpu_context_t layout:
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
 *   0x68: fpcr
 *   0x70: fpsr
 *   0x78: _pad (alignment padding)
 *   0x80: fp_regs[0-63] (q0-q31, 512 bytes) - 16-byte aligned!
 */

context_switch:
    // Save current context to old_ctx (x0)
    // If old_ctx is NULL, skip saving
    cbz     x0, 1f

    // Save general purpose registers
    stp     x19, x20, [x0, #0x00]
    stp     x21, x22, [x0, #0x10]
    stp     x23, x24, [x0, #0x20]
    stp     x25, x26, [x0, #0x30]
    stp     x27, x28, [x0, #0x40]
    stp     x29, x30, [x0, #0x50]
    mov     x2, sp
    str     x2, [x0, #0x60]

    // Save FPU control/status registers
    mrs     x2, fpcr
    str     x2, [x0, #0x68]
    mrs     x2, fpsr
    str     x2, [x0, #0x70]

    // Save FP registers q0-q31 (each is 128-bit)
    add     x2, x0, #0x80
    stp     q0,  q1,  [x2, #0x00]
    stp     q2,  q3,  [x2, #0x20]
    stp     q4,  q5,  [x2, #0x40]
    stp     q6,  q7,  [x2, #0x60]
    stp     q8,  q9,  [x2, #0x80]
    stp     q10, q11, [x2, #0xa0]
    stp     q12, q13, [x2, #0xc0]
    stp     q14, q15, [x2, #0xe0]
    stp     q16, q17, [x2, #0x100]
    stp     q18, q19, [x2, #0x120]
    stp     q20, q21, [x2, #0x140]
    stp     q22, q23, [x2, #0x160]
    stp     q24, q25, [x2, #0x180]
    stp     q26, q27, [x2, #0x1a0]
    stp     q28, q29, [x2, #0x1c0]
    stp     q30, q31, [x2, #0x1e0]

1:
    // Restore context from new_ctx (x1)

    // Restore FP registers first (use x2 as temp)
    add     x2, x1, #0x80
    ldp     q0,  q1,  [x2, #0x00]
    ldp     q2,  q3,  [x2, #0x20]
    ldp     q4,  q5,  [x2, #0x40]
    ldp     q6,  q7,  [x2, #0x60]
    ldp     q8,  q9,  [x2, #0x80]
    ldp     q10, q11, [x2, #0xa0]
    ldp     q12, q13, [x2, #0xc0]
    ldp     q14, q15, [x2, #0xe0]
    ldp     q16, q17, [x2, #0x100]
    ldp     q18, q19, [x2, #0x120]
    ldp     q20, q21, [x2, #0x140]
    ldp     q22, q23, [x2, #0x160]
    ldp     q24, q25, [x2, #0x180]
    ldp     q26, q27, [x2, #0x1a0]
    ldp     q28, q29, [x2, #0x1c0]
    ldp     q30, q31, [x2, #0x1e0]

    // Restore FPU control/status registers
    ldr     x2, [x1, #0x68]
    msr     fpcr, x2
    ldr     x2, [x1, #0x70]
    msr     fpsr, x2

    // Restore general purpose registers
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
