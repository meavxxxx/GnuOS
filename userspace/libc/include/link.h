#ifndef _LINK_H
#define _LINK_H

#include <sys/cdefs.h>
#include <sys/types.h>

typedef unsigned short Elf64_Half;
typedef unsigned int Elf64_Word;
typedef long Elf64_Sword;
typedef unsigned long Elf64_Xword;
typedef long Elf64_Sxword;
typedef unsigned long Elf64_Addr;
typedef unsigned long Elf64_Off;

typedef struct {
    Elf64_Word p_type;
    Elf64_Word p_flags;
    Elf64_Off p_offset;
    Elf64_Addr p_vaddr;
    Elf64_Addr p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} Elf64_Phdr;

struct dl_phdr_info {
    Elf64_Addr dlpi_addr;
    const char *dlpi_name;
    const Elf64_Phdr *dlpi_phdr;
    Elf64_Half dlpi_phnum;
};

__BEGIN_DECLS

int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info *info, size_t size, void *data),
    void *data);

__END_DECLS

#endif
