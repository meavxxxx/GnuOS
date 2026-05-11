#ifndef GNUOS_UACCESS_H
#define GNUOS_UACCESS_H

#include <stdint.h>

int uaccess_validate_read(uint64_t user_addr, uint64_t size);
int uaccess_validate_write(uint64_t user_addr, uint64_t size);
int uaccess_copy_from_user(void *dst, uint64_t user_src, uint64_t size);
int uaccess_copy_to_user(uint64_t user_dst, const void *src, uint64_t size);

#endif
