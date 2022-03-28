#pragma once

#include <stdint.h>

typedef struct registers {
    uint32_t ss, gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t num, err_code;
    uint32_t eip, cs, eflags, user_esp, user_ss;
} __attribute__((packed)) registers;

void dump_registers(const registers*);

void gdt_init(void);
void pit_init(uint32_t freq);
void syscall_init(void);

void gdt_set_kernel_stack(uintptr_t stack_top);
