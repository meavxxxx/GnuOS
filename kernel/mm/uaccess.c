#include <stddef.h>
#include <stdint.h>

#include <gnuos/mm.h>
#include <gnuos/uaccess.h>
#include <gnuos/vmm.h>
#include <gnuos/x86_64_hardening.h>

#define UACCESS_USER_MAX_VA 0x00007FFFFFFFFFFFULL

static int uaccess_validate_range(uint64_t user_addr, uint64_t size, uint8_t require_write)
{
    uint64_t end_addr;
    uint64_t page_addr;
    uint64_t last_page;

    if (size == 0U) {
        return 1;
    }

    if (user_addr == 0U || user_addr > UACCESS_USER_MAX_VA) {
        return 0;
    }

    end_addr = user_addr + size - 1U;
    if (end_addr < user_addr || end_addr > UACCESS_USER_MAX_VA) {
        return 0;
    }

    page_addr = user_addr & ~(MM_PAGE_SIZE - 1U);
    last_page = end_addr & ~(MM_PAGE_SIZE - 1U);

    for (;;) {
        uint64_t phys_addr = 0U;
        uint64_t flags = 0U;

        if (!vmm_query_mapping(page_addr, &phys_addr, &flags)) {
            return 0;
        }
        (void)phys_addr;
        if ((flags & VMM_MAP_USER) == 0U) {
            return 0;
        }
        if (require_write && (flags & VMM_MAP_WRITABLE) == 0U) {
            return 0;
        }

        if (page_addr == last_page) {
            break;
        }
        page_addr += MM_PAGE_SIZE;
    }

    return 1;
}

int uaccess_validate_read(uint64_t user_addr, uint64_t size)
{
    return uaccess_validate_range(user_addr, size, 0U);
}

int uaccess_validate_write(uint64_t user_addr, uint64_t size)
{
    return uaccess_validate_range(user_addr, size, 1U);
}

int uaccess_copy_from_user(void *dst, uint64_t user_src, uint64_t size)
{
    uint8_t *dst_bytes = (uint8_t *)dst;
    const uint8_t *src_bytes = (const uint8_t *)(uintptr_t)user_src;

    if (!dst) {
        return -1;
    }
    if (!uaccess_validate_read(user_src, size)) {
        return -1;
    }

    x86_64_uaccess_begin();
    for (uint64_t index = 0U; index < size; index++) {
        dst_bytes[index] = src_bytes[index];
    }
    x86_64_uaccess_end();

    return 0;
}

int uaccess_copy_to_user(uint64_t user_dst, const void *src, uint64_t size)
{
    uint8_t *dst_bytes = (uint8_t *)(uintptr_t)user_dst;
    const uint8_t *src_bytes = (const uint8_t *)src;

    if (!src) {
        return -1;
    }
    if (!uaccess_validate_write(user_dst, size)) {
        return -1;
    }

    x86_64_uaccess_begin();
    for (uint64_t index = 0U; index < size; index++) {
        dst_bytes[index] = src_bytes[index];
    }
    x86_64_uaccess_end();

    return 0;
}
