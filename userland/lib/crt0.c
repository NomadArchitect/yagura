#include "errno.h"
#include "panic.h"
#include "private.h"
#include "stdlib.h"
#include "string.h"
#include "sys/auxv.h"
#include "unistd.h"
#include <asm/ldt.h>
#include <elf.h>
#include <extra.h>

static uint32_t auxv[32];

unsigned long getauxval(unsigned long type) {
    if (type >= ARRAY_SIZE(auxv)) {
        errno = ENOENT;
        return 0;
    }
    return auxv[type];
}

static const Elf32_Phdr* tls_phdr;

static const Elf32_Phdr* find_tls_phdr(void) {
    Elf32_Phdr* phdr = (Elf32_Phdr*)getauxval(AT_PHDR);
    size_t n = getauxval(AT_PHNUM);
    for (size_t i = 0; i < n; ++i) {
        if (phdr[i].p_type == PT_TLS)
            return &phdr[i];
    }
    return NULL;
}

size_t __tls_size;

struct pthread* __init_tls(void* tls) {
    memset(tls, 0, __tls_size);

    uintptr_t p = (uintptr_t)tls + __tls_size - sizeof(struct pthread);
    p = ROUND_DOWN(p, tls_phdr->p_align);
    memcpy((unsigned char*)p - tls_phdr->p_memsz, (void*)tls_phdr->p_vaddr,
           tls_phdr->p_filesz);

    struct pthread* pth = (struct pthread*)p;
    pth->self = pth;
    return pth;
}

static void set_thread_area(void* addr) {
    struct user_desc tls_desc = {
        .entry_number = -1,
        .base_addr = (unsigned)addr,
        .limit = 0xfffff,
        .seg_32bit = 1,
        .limit_in_pages = 1,
    };
    ASSERT_OK(SYSCALL1(set_thread_area, &tls_desc));
    uint16_t gs = (tls_desc.entry_number * 8) | 3;
    __asm__ volatile("movw %0, %%gs" ::"r"(gs));
}

int main(int argc, char* const argv[], char* const envp[]);

void __start(unsigned long* args) {
    int argc = args[0];
    char** argv = (char**)(args + 1);
    environ = argv + argc + 1;

    // Initialize auxv
    char** p = environ;
    while (*p++)
        ;
    for (Elf32_auxv_t* aux = (Elf32_auxv_t*)p; aux->a_type != AT_NULL; ++aux) {
        if (aux->a_type < ARRAY_SIZE(auxv))
            auxv[aux->a_type] = aux->a_un.a_val;
    }

    // Initialize TLS
    tls_phdr = find_tls_phdr();
    ASSERT(tls_phdr);
    __tls_size = tls_phdr->p_memsz + tls_phdr->p_align + sizeof(struct pthread);

    // Initialize TLS for the main thread
    unsigned char* tls = malloc(__tls_size);
    ASSERT(tls);
    set_thread_area(__init_tls(tls));

    exit(main(argc, argv, environ));
}
