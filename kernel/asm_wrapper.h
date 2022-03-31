#pragma once

#include <stdint.h>
#include <stdnoreturn.h>

uint32_t read_eip(void);

static inline uint32_t read_eflags(void) {
    uint32_t eflags;
    __asm__ volatile("pushf;\n"
                     "popl %0"
                     : "=r"(eflags));
    return eflags;
}

static inline void sti(void) { __asm__ volatile("sti"); }

static inline void cli(void) { __asm__ volatile("cli"); }

static inline uint32_t read_cr0(void) {
    uint32_t cr0;
    __asm__("mov %%cr0, %%eax" : "=a"(cr0));
    return cr0;
}

static inline void write_cr0(uint32_t value) {
    __asm__ volatile("mov %%eax, %%cr0" ::"a"(value));
}

static inline uint32_t read_cr2(void) {
    uint32_t cr2;
    __asm__("mov %%cr2, %%eax" : "=a"(cr2));
    return cr2;
}

static inline uint32_t read_cr3(void) {
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %%eax" : "=a"(cr3));
    return cr3;
}

static inline void write_cr3(uint32_t cr3) {
    __asm__ volatile("mov %%eax, %%cr3" ::"a"(cr3) : "memory");
}

static inline uint32_t read_cr4(void) {
    uint32_t cr4;
    __asm__("mov %%cr4, %%eax" : "=a"(cr4));
    return cr4;
}

static inline void write_cr4(uint32_t value) {
    __asm__ volatile("mov %%eax, %%cr4" ::"a"(value));
}

static inline void flush_tlb(void) { write_cr3(read_cr3()); }

static inline void flush_tlb_single(uintptr_t vaddr) {
    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

static inline uint8_t in8(uint16_t port) {
    uint8_t rv;
    __asm__ volatile("inb %1, %0" : "=a"(rv) : "dN"(port));
    return rv;
}

static inline uint16_t in16(uint16_t port) {
    uint16_t rv;
    __asm__ volatile("inw %1, %0" : "=a"(rv) : "dN"(port));
    return rv;
}

static inline uint32_t in32(uint16_t port) {
    uint32_t rv;
    __asm__ volatile("inl %1, %0" : "=a"(rv) : "dN"(port));
    return rv;
}

static inline void out8(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %1, %0" : : "dN"(port), "a"(data));
}

static inline void out16(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %1, %0" : : "dN"(port), "a"(data));
}

static inline void out32(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %1, %0" : : "dN"(port), "a"(data));
}

static inline void delay(uint32_t usec) {
    uint8_t dummy;
    while (usec--) {
        __asm__ volatile("inb $0x80, %0" : "=a"(dummy));
    }
}

static inline void hlt(void) { __asm__ volatile("hlt"); }

static inline noreturn void ud2(void) {
    __asm__ volatile("ud2");
    __builtin_unreachable();
}

static inline void pause(void) { __asm__ volatile("pause"); }
