#include <stdint.h>

#include <arpa/inet.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <gnuos/tls.h>
#include <link.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ldso_dlfcn.h"
#include "ldso_elf.h"

#define LDSO_AUXV_SCAN_LIMIT 256U
#define LDSO_STACK_SCAN_LIMIT 4096U

#define LDSO_AT_NULL 0U
#define LDSO_AT_PHDR 3U
#define LDSO_AT_PHENT 4U
#define LDSO_AT_PHNUM 5U
#define LDSO_AT_BASE 7U
#define LDSO_AT_ENTRY 9U
#define LDSO_LD_PRELOAD_KEY "LD_PRELOAD="
#define LDSO_LD_PRELOAD_KEY_LEN 11U
#define LDSO_PRELOAD_TOKEN_MAX 128U
#define LDSO_STAGE0_BUILTIN_SYMBOL_COUNT 69U

#define GNUOS_PTHREAD_ENOSYS 38
#define GNUOS_PTHREAD_EINVAL 22
#define GNUOS_PTHREAD_MAIN_THREAD ((pthread_t)1UL)
#define GNUOS_PTHREAD_STACK_MIN 16384UL
#define GNUOS_PTHREAD_STACK_DEFAULT (2UL * 1024UL * 1024UL)
#define GNUOS_SEM_EAGAIN 11
#define GNUOS_SEM_EOVERFLOW 75
#define GNUOS_SEM_VALUE_MAX 0x7FFFFFFFU
#define GNUOS_SIGNAL_NSIG NSIG
#define GNUOS_SOCKET_EMFILE 24
#define GNUOS_SOCKET_EPROTONOSUPPORT 93
#define GNUOS_SOCKET_ENOTSOCK 88
#define GNUOS_SOCKET_EAFNOSUPPORT 97
#define GNUOS_SOCKET_EOPNOTSUPP 95
#define GNUOS_SOCKET_FD_BASE 3
#define GNUOS_SOCKET_MAX 32
#define GNUOS_FILE_ENOENT 2
#define GNUOS_FILE_EBADF 9
#define GNUOS_FILE_ENFILE 23
#define GNUOS_FILE_FD_BASE 64
#define GNUOS_FILE_MAX 64
#define GNUOS_MMAN_POOL_SIZE (2UL * 1024UL * 1024UL)

typedef struct {
    uint64_t a_type;
    uint64_t a_val;
} ldso_auxv_entry_t;

typedef struct {
    uint64_t ready;
    uint64_t at_base;
    uint64_t at_entry;
    uint64_t at_phdr;
    uint64_t at_phent;
    uint64_t at_phnum;
    uint64_t load_bias;
    uint64_t load_count;
    uint64_t has_dynamic_segment;
    uint64_t has_gnu_relro;
    uint64_t dynamic_ready;
    uint64_t relocations_attempted;
    uint64_t relocations_applied;
    uint64_t relocations_unresolved;
    uint64_t relocations_unsupported;
    uint64_t init_sequence_attempted;
    uint64_t init_sequence_completed;
    uint64_t dlfcn_ready;
    uint64_t builtin_object_registered;
    uint64_t ld_preload_seen;
    uint64_t ld_preload_attempted;
    uint64_t ld_preload_loaded;
    uint64_t ld_preload_failed;
} ldso_stage0_state_t;

volatile ldso_stage0_state_t g_ldso_stage0_state;
static ldso_dlfcn_builtin_symbol_t g_ldso_stage0_builtin_symbols[LDSO_STAGE0_BUILTIN_SYMBOL_COUNT];
static uintptr_t g_ldso_stage0_tls_storage[64];
static uintptr_t g_ldso_stage0_tls_base;
static int g_errno_value;
static sigset_t g_signal_mask;
static struct sigaction g_signal_actions[GNUOS_SIGNAL_NSIG];
typedef struct {
    int in_use;
    int domain;
    int type;
    int protocol;
    int connected;
    int listening;
    int shutdown_how;
} gnuos_socket_entry_t;
static gnuos_socket_entry_t g_socket_table[GNUOS_SOCKET_MAX];
typedef struct {
    int in_use;
    int flags;
    mode_t mode;
    off_t offset;
    struct stat st;
} gnuos_file_entry_t;
static gnuos_file_entry_t g_file_table[GNUOS_FILE_MAX];
static mode_t g_file_umask = 0;
static unsigned char g_mmap_pool[GNUOS_MMAN_POOL_SIZE];
static size_t g_mmap_pool_next = 0;

int *__errno_location(void)
{
    return &g_errno_value;
}

static int gnuos_fail_int(int error)
{
    g_errno_value = error;
    return -1;
}

static ssize_t gnuos_fail_ssize(int error)
{
    g_errno_value = error;
    return (ssize_t)-1;
}

static off_t gnuos_fail_off(int error)
{
    g_errno_value = error;
    return (off_t)-1;
}

static void *gnuos_fail_ptr(int error)
{
    g_errno_value = error;
    return MAP_FAILED;
}

static const char *gnuos_fail_cstr(int error)
{
    g_errno_value = error;
    return 0;
}

static const uint64_t *ldso_stack_skip_argv(const uint64_t *cursor, uint64_t argc)
{
    uint64_t index;

    if (!cursor) {
        return 0;
    }

    for (index = 0U; index < argc; index++) {
        if (*cursor == 0U) {
            return 0;
        }
        cursor++;
    }

    if (*cursor != 0U) {
        return 0;
    }

    return cursor + 1U;
}

static const uint64_t *ldso_stack_skip_envp(const uint64_t *cursor)
{
    uint64_t scanned = 0U;

    if (!cursor) {
        return 0;
    }

    while (scanned < LDSO_STACK_SCAN_LIMIT) {
        if (*cursor == 0U) {
            return cursor + 1U;
        }

        cursor++;
        scanned++;
    }

    return 0;
}

static const ldso_auxv_entry_t *ldso_find_auxv(const uint64_t *initial_stack)
{
    uint64_t argc;
    const uint64_t *cursor;

    if (!initial_stack) {
        return 0;
    }

    argc = initial_stack[0];
    cursor = ldso_stack_skip_argv(initial_stack + 1U, argc);
    if (!cursor) {
        return 0;
    }

    cursor = ldso_stack_skip_envp(cursor);
    if (!cursor) {
        return 0;
    }

    return (const ldso_auxv_entry_t *)cursor;
}

static uint64_t ldso_auxv_lookup(const ldso_auxv_entry_t *auxv, uint64_t key)
{
    uint64_t index;

    if (!auxv) {
        return 0U;
    }

    for (index = 0U; index < LDSO_AUXV_SCAN_LIMIT; index++) {
        const ldso_auxv_entry_t *entry = &auxv[index];
        if (entry->a_type == LDSO_AT_NULL) {
            break;
        }
        if (entry->a_type == key) {
            return entry->a_val;
        }
    }

    return 0U;
}

static int ldso_str_starts_with(const char *value, const char *prefix, uint64_t prefix_len)
{
    uint64_t index;

    if (!value || !prefix) {
        return 0;
    }

    for (index = 0U; index < prefix_len; index++) {
        if (value[index] != prefix[index]) {
            return 0;
        }
    }

    return 1;
}

static int ldso_is_preload_delimiter(char c)
{
    return c == ' ' || c == ':';
}

static const char *const *ldso_find_envp(const uint64_t *initial_stack)
{
    uint64_t argc;
    const uint64_t *cursor;

    if (!initial_stack) {
        return 0;
    }

    argc = initial_stack[0];
    cursor = ldso_stack_skip_argv(initial_stack + 1U, argc);
    if (!cursor) {
        return 0;
    }

    return (const char *const *)cursor;
}

static const char *ldso_find_env_value(const uint64_t *initial_stack, const char *key, uint64_t key_len)
{
    uint64_t index = 0U;
    const char *const *envp = ldso_find_envp(initial_stack);

    if (!envp || !key || key_len == 0U) {
        return 0;
    }

    while (envp[index]) {
        const char *entry = envp[index];
        if (ldso_str_starts_with(entry, key, key_len)) {
            return entry + key_len;
        }
        index++;
    }

    return 0;
}

static void ldso_builtin_touch(void)
{
}

static __attribute__((noreturn)) void ldso_builtin_exit(void)
{
    for (;;) {
        __asm__ volatile("pause");
    }
}

int __gnuos_set_tls_base(void *base)
{
    g_ldso_stage0_tls_base = (uintptr_t)base;
    return 0;
}

void *__gnuos_get_tls_base(void)
{
    return (void *)g_ldso_stage0_tls_base;
}

void *__tls_get_addr(gnuos_tls_index_t *index)
{
    if (!index) {
        return 0;
    }

    return (void *)(g_ldso_stage0_tls_base + index->ti_offset);
}

int pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg)
{
    (void)attr;
    (void)start_routine;
    (void)arg;

    if (thread) {
        *thread = 0UL;
    }

    return GNUOS_PTHREAD_ENOSYS;
}

pthread_t pthread_self(void)
{
    return GNUOS_PTHREAD_MAIN_THREAD;
}

int pthread_equal(pthread_t thread1, pthread_t thread2)
{
    return thread1 == thread2;
}

int pthread_join(pthread_t thread, void **retval)
{
    (void)thread;
    if (retval) {
        *retval = 0;
    }

    return GNUOS_PTHREAD_ENOSYS;
}

int pthread_detach(pthread_t thread)
{
    (void)thread;
    return GNUOS_PTHREAD_ENOSYS;
}

int pthread_attr_init(pthread_attr_t *attr)
{
    if (!attr) {
        return GNUOS_PTHREAD_EINVAL;
    }

    attr->__opaque = 0UL;
    attr->__stack_size = GNUOS_PTHREAD_STACK_DEFAULT;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    if (!attr) {
        return GNUOS_PTHREAD_EINVAL;
    }

    attr->__opaque = 0UL;
    attr->__stack_size = 0UL;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    if (!attr || stacksize < GNUOS_PTHREAD_STACK_MIN) {
        return GNUOS_PTHREAD_EINVAL;
    }

    attr->__stack_size = stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    if (!attr || !stacksize) {
        return GNUOS_PTHREAD_EINVAL;
    }

    *stacksize = attr->__stack_size;
    return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (!sem) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
    if (pshared != 0) {
        return gnuos_fail_int(GNUOS_PTHREAD_ENOSYS);
    }

    sem->__value = value;
    return 0;
}

int sem_destroy(sem_t *sem)
{
    if (!sem) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    sem->__value = 0U;
    return 0;
}

int sem_wait(sem_t *sem)
{
    if (!sem) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
    if (sem->__value == 0U) {
        return gnuos_fail_int(GNUOS_SEM_EAGAIN);
    }

    sem->__value--;
    return 0;
}

int sem_trywait(sem_t *sem)
{
    return sem_wait(sem);
}

int sem_post(sem_t *sem)
{
    if (!sem) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
    if (sem->__value == GNUOS_SEM_VALUE_MAX) {
        return gnuos_fail_int(GNUOS_SEM_EOVERFLOW);
    }

    sem->__value++;
    return 0;
}

int sem_getvalue(sem_t *sem, int *sval)
{
    if (!sem || !sval) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    *sval = (int)sem->__value;
    return 0;
}

static int gnuos_socket_supported_domain(int domain)
{
    return domain == AF_UNSPEC || domain == AF_UNIX || domain == AF_INET || domain == AF_INET6;
}

static int gnuos_socket_supported_type(int type)
{
    int base_type = type & 0x0F;
    return base_type == SOCK_STREAM || base_type == SOCK_DGRAM || base_type == SOCK_RAW;
}

static gnuos_socket_entry_t *gnuos_socket_get(int sockfd)
{
    int index = sockfd - GNUOS_SOCKET_FD_BASE;

    if (index < 0 || index >= GNUOS_SOCKET_MAX) {
        return 0;
    }
    if (!g_socket_table[index].in_use) {
        return 0;
    }

    return &g_socket_table[index];
}

static int gnuos_socket_alloc_fd(void)
{
    int index;

    for (index = 0; index < GNUOS_SOCKET_MAX; index++) {
        if (!g_socket_table[index].in_use) {
            g_socket_table[index].in_use = 1;
            g_socket_table[index].domain = AF_UNSPEC;
            g_socket_table[index].type = 0;
            g_socket_table[index].protocol = 0;
            g_socket_table[index].connected = 0;
            g_socket_table[index].listening = 0;
            g_socket_table[index].shutdown_how = -1;
            return GNUOS_SOCKET_FD_BASE + index;
        }
    }

    return -1;
}

in_port_t htons(in_port_t hostshort)
{
    return (in_port_t)((hostshort >> 8) | (hostshort << 8));
}

in_port_t ntohs(in_port_t netshort)
{
    return htons(netshort);
}

in_addr_t htonl(in_addr_t hostlong)
{
    return ((hostlong & 0x000000FFU) << 24) |
        ((hostlong & 0x0000FF00U) << 8) |
        ((hostlong & 0x00FF0000U) >> 8) |
        ((hostlong & 0xFF000000U) >> 24);
}

in_addr_t ntohl(in_addr_t netlong)
{
    return htonl(netlong);
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    (void)af;
    (void)src;
    (void)dst;
    (void)size;
    return gnuos_fail_cstr(GNUOS_PTHREAD_ENOSYS);
}

int inet_pton(int af, const char *src, void *dst)
{
    (void)af;
    (void)src;
    (void)dst;
    return gnuos_fail_int(GNUOS_PTHREAD_ENOSYS);
}

int socket(int domain, int type, int protocol)
{
    int sockfd;
    gnuos_socket_entry_t *entry;

    if (!gnuos_socket_supported_domain(domain)) {
        return gnuos_fail_int(GNUOS_SOCKET_EAFNOSUPPORT);
    }
    if (!gnuos_socket_supported_type(type)) {
        return gnuos_fail_int(GNUOS_SOCKET_EPROTONOSUPPORT);
    }

    sockfd = gnuos_socket_alloc_fd();
    if (sockfd < 0) {
        return gnuos_fail_int(GNUOS_SOCKET_EMFILE);
    }

    entry = gnuos_socket_get(sockfd);
    entry->domain = domain;
    entry->type = type;
    entry->protocol = protocol;
    return sockfd;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    gnuos_socket_entry_t *entry = gnuos_socket_get(sockfd);
    (void)addr;
    (void)addrlen;

    if (!entry) {
        return gnuos_fail_int(GNUOS_SOCKET_ENOTSOCK);
    }

    return 0;
}

int listen(int sockfd, int backlog)
{
    gnuos_socket_entry_t *entry = gnuos_socket_get(sockfd);
    (void)backlog;

    if (!entry) {
        return gnuos_fail_int(GNUOS_SOCKET_ENOTSOCK);
    }

    entry->listening = 1;
    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    gnuos_socket_entry_t *entry = gnuos_socket_get(sockfd);
    (void)addr;
    (void)addrlen;

    if (!entry) {
        return gnuos_fail_int(GNUOS_SOCKET_ENOTSOCK);
    }
    if (!entry->listening) {
        return gnuos_fail_int(GNUOS_SOCKET_EOPNOTSUPP);
    }

    return gnuos_fail_int(GNUOS_PTHREAD_ENOSYS);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    gnuos_socket_entry_t *entry = gnuos_socket_get(sockfd);
    (void)addr;
    (void)addrlen;

    if (!entry) {
        return gnuos_fail_int(GNUOS_SOCKET_ENOTSOCK);
    }

    entry->connected = 1;
    return 0;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    gnuos_socket_entry_t *entry = gnuos_socket_get(sockfd);
    (void)buf;
    (void)flags;

    if (!entry) {
        return gnuos_fail_ssize(GNUOS_SOCKET_ENOTSOCK);
    }
    if (!entry->connected) {
        return gnuos_fail_ssize(GNUOS_SOCKET_EOPNOTSUPP);
    }

    return (ssize_t)len;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    gnuos_socket_entry_t *entry = gnuos_socket_get(sockfd);
    (void)buf;
    (void)len;
    (void)flags;

    if (!entry) {
        return gnuos_fail_ssize(GNUOS_SOCKET_ENOTSOCK);
    }
    if (!entry->connected) {
        return gnuos_fail_ssize(GNUOS_SOCKET_EOPNOTSUPP);
    }

    return 0;
}

ssize_t sendto(
    int sockfd,
    const void *buf,
    size_t len,
    int flags,
    const struct sockaddr *dest_addr,
    socklen_t addrlen)
{
    (void)dest_addr;
    (void)addrlen;
    return send(sockfd, buf, len, flags);
}

ssize_t recvfrom(
    int sockfd,
    void *buf,
    size_t len,
    int flags,
    struct sockaddr *src_addr,
    socklen_t *addrlen)
{
    (void)src_addr;
    (void)addrlen;
    return recv(sockfd, buf, len, flags);
}

int shutdown(int sockfd, int how)
{
    gnuos_socket_entry_t *entry = gnuos_socket_get(sockfd);

    if (!entry) {
        return gnuos_fail_int(GNUOS_SOCKET_ENOTSOCK);
    }
    if (how < SHUT_RD || how > SHUT_RDWR) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    entry->shutdown_how = how;
    return 0;
}

static gnuos_file_entry_t *gnuos_file_get(int fd)
{
    int index = fd - GNUOS_FILE_FD_BASE;

    if (index < 0 || index >= GNUOS_FILE_MAX) {
        return 0;
    }
    if (!g_file_table[index].in_use) {
        return 0;
    }

    return &g_file_table[index];
}

static int gnuos_file_alloc_fd(void)
{
    int index;

    for (index = 0; index < GNUOS_FILE_MAX; index++) {
        if (!g_file_table[index].in_use) {
            g_file_table[index].in_use = 1;
            g_file_table[index].flags = O_RDONLY;
            g_file_table[index].mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            g_file_table[index].offset = 0;
            g_file_table[index].st.st_mode = g_file_table[index].mode;
            g_file_table[index].st.st_size = 0;
            g_file_table[index].st.st_nlink = 1;
            g_file_table[index].st.st_blksize = 4096;
            g_file_table[index].st.st_blocks = 0;
            return GNUOS_FILE_FD_BASE + index;
        }
    }

    return -1;
}

int open(const char *pathname, int flags, ...)
{
    int fd;
    gnuos_file_entry_t *entry;

    if (!pathname || pathname[0] == '\0') {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
    if (pathname[0] != '/' && pathname[0] != '.') {
        return gnuos_fail_int(GNUOS_FILE_ENOENT);
    }

    fd = gnuos_file_alloc_fd();
    if (fd < 0) {
        return gnuos_fail_int(GNUOS_FILE_ENFILE);
    }

    entry = gnuos_file_get(fd);
    entry->flags = flags;
    if (flags & O_TRUNC) {
        entry->st.st_size = 0;
    }

    return fd;
}

int close(int fd)
{
    gnuos_socket_entry_t *sock_entry = gnuos_socket_get(fd);
    gnuos_file_entry_t *file_entry = gnuos_file_get(fd);

    if (sock_entry) {
        sock_entry->in_use = 0;
        sock_entry->connected = 0;
        sock_entry->listening = 0;
        return 0;
    }

    if (file_entry) {
        file_entry->in_use = 0;
        file_entry->offset = 0;
        return 0;
    }

    return gnuos_fail_int(GNUOS_FILE_EBADF);
}

ssize_t read(int fd, void *buf, size_t count)
{
    gnuos_socket_entry_t *sock_entry = gnuos_socket_get(fd);
    gnuos_file_entry_t *file_entry = gnuos_file_get(fd);
    size_t i;
    unsigned char *out = (unsigned char *)buf;

    if (sock_entry) {
        return recv(fd, buf, count, 0);
    }
    if (!file_entry) {
        return gnuos_fail_ssize(GNUOS_FILE_EBADF);
    }
    if (!buf && count != 0U) {
        return gnuos_fail_ssize(GNUOS_PTHREAD_EINVAL);
    }

    for (i = 0; i < count; i++) {
        out[i] = 0;
    }
    file_entry->offset += (off_t)count;
    return (ssize_t)count;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    gnuos_socket_entry_t *sock_entry = gnuos_socket_get(fd);
    gnuos_file_entry_t *file_entry = gnuos_file_get(fd);

    (void)buf;
    if (sock_entry) {
        return send(fd, buf, count, 0);
    }
    if (!file_entry) {
        return gnuos_fail_ssize(GNUOS_FILE_EBADF);
    }

    file_entry->offset += (off_t)count;
    if (file_entry->offset > file_entry->st.st_size) {
        file_entry->st.st_size = file_entry->offset;
    }
    return (ssize_t)count;
}

off_t lseek(int fd, off_t offset, int whence)
{
    gnuos_file_entry_t *entry = gnuos_file_get(fd);
    off_t base;
    off_t new_offset;

    if (!entry) {
        return gnuos_fail_off(GNUOS_FILE_EBADF);
    }

    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = entry->offset;
        break;
    case SEEK_END:
        base = entry->st.st_size;
        break;
    default:
        return gnuos_fail_off(GNUOS_PTHREAD_EINVAL);
    }

    new_offset = base + offset;
    if (new_offset < 0) {
        return gnuos_fail_off(GNUOS_PTHREAD_EINVAL);
    }

    entry->offset = new_offset;
    return new_offset;
}

int fstat(int fd, struct stat *buf)
{
    gnuos_file_entry_t *entry = gnuos_file_get(fd);

    if (!buf) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
    if (!entry) {
        return gnuos_fail_int(GNUOS_FILE_EBADF);
    }

    *buf = entry->st;
    return 0;
}

int stat(const char *path, struct stat *buf)
{
    if (!path || !buf) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
    if (path[0] != '/' && path[0] != '.') {
        return gnuos_fail_int(GNUOS_FILE_ENOENT);
    }

    buf->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    buf->st_size = 0;
    buf->st_nlink = 1;
    buf->st_blksize = 4096;
    buf->st_blocks = 0;
    return 0;
}

int access(const char *pathname, int mode)
{
    (void)mode;
    if (!pathname || pathname[0] == '\0') {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
    if (pathname[0] != '/' && pathname[0] != '.') {
        return gnuos_fail_int(GNUOS_FILE_ENOENT);
    }

    return 0;
}

int mkdir(const char *path, mode_t mode)
{
    (void)mode;
    if (!path || path[0] == '\0') {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    return gnuos_fail_int(GNUOS_PTHREAD_ENOSYS);
}

int chmod(const char *path, mode_t mode)
{
    (void)mode;
    if (!path || path[0] == '\0') {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    return 0;
}

int fchmod(int fd, mode_t mode)
{
    gnuos_file_entry_t *entry = gnuos_file_get(fd);
    if (!entry) {
        return gnuos_fail_int(GNUOS_FILE_EBADF);
    }

    entry->mode = mode;
    entry->st.st_mode = mode;
    return 0;
}

mode_t umask(mode_t mask)
{
    mode_t old = g_file_umask;
    g_file_umask = mask & 0777U;
    return old;
}

static size_t gnuos_align_up(size_t value, size_t alignment)
{
    size_t mask = alignment - 1U;
    return (value + mask) & ~mask;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    size_t aligned_length;
    size_t next;
    void *mapped;

    (void)prot;
    (void)fd;
    (void)offset;

    if (length == 0U) {
        return gnuos_fail_ptr(GNUOS_PTHREAD_EINVAL);
    }
    if ((flags & MAP_FIXED) != 0 && addr) {
        return addr;
    }

    aligned_length = gnuos_align_up(length, 4096U);
    next = g_mmap_pool_next;
    if (next > GNUOS_MMAN_POOL_SIZE || aligned_length > (GNUOS_MMAN_POOL_SIZE - next)) {
        return gnuos_fail_ptr(ENOMEM);
    }

    mapped = (void *)&g_mmap_pool[next];
    g_mmap_pool_next = next + aligned_length;
    return mapped;
}

int munmap(void *addr, size_t length)
{
    if (!addr || length == 0U) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
    (void)prot;
    if (!addr || len == 0U) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    return 0;
}

static int gnuos_signal_valid(int signo)
{
    return signo > 0 && signo < GNUOS_SIGNAL_NSIG;
}

static sigset_t gnuos_signal_bit(int signo)
{
    return (sigset_t)1UL << (unsigned long)(signo - 1);
}

int sigemptyset(sigset_t *set)
{
    if (!set) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    *set = 0UL;
    return 0;
}

int sigfillset(sigset_t *set)
{
    if (!set) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    *set = ~(sigset_t)0UL;
    return 0;
}

int sigaddset(sigset_t *set, int signo)
{
    if (!set || !gnuos_signal_valid(signo)) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    *set |= gnuos_signal_bit(signo);
    return 0;
}

int sigdelset(sigset_t *set, int signo)
{
    if (!set || !gnuos_signal_valid(signo)) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    *set &= ~(gnuos_signal_bit(signo));
    return 0;
}

int sigismember(const sigset_t *set, int signo)
{
    if (!set || !gnuos_signal_valid(signo)) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    return ((*set & gnuos_signal_bit(signo)) != 0UL) ? 1 : 0;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    if (!gnuos_signal_valid(signum) || signum == SIGKILL || signum == SIGSTOP) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    if (oldact) {
        *oldact = g_signal_actions[signum];
    }

    if (act) {
        g_signal_actions[signum] = *act;
    }

    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (oldset) {
        *oldset = g_signal_mask;
    }

    if (!set) {
        return 0;
    }

    switch (how) {
    case SIG_BLOCK:
        g_signal_mask |= *set;
        return 0;
    case SIG_UNBLOCK:
        g_signal_mask &= ~(*set);
        return 0;
    case SIG_SETMASK:
        g_signal_mask = *set;
        return 0;
    default:
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
}

int kill(pid_t pid, int sig)
{
    (void)pid;
    if (!gnuos_signal_valid(sig)) {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }

    return gnuos_fail_int(GNUOS_PTHREAD_ENOSYS);
}

int raise(int sig)
{
    return kill(0, sig);
}

sighandler_t signal(int signum, sighandler_t handler)
{
    struct sigaction act;
    struct sigaction oldact;
    int result;

    act.sa_handler = handler;
    act.sa_mask = 0UL;
    act.sa_flags = 0;

    result = sigaction(signum, &act, &oldact);
    if (result != 0) {
        return SIG_ERR;
    }

    return oldact.sa_handler;
}

void __gnuos_store_startup(unsigned long argc, char **argv, char **envp)
{
    (void)argc;
    (void)argv;
    (void)envp;
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data)
{
    struct dl_phdr_info info;

    if (!callback) {
        return -1;
    }
    if (g_ldso_stage0_state.at_phdr == 0U ||
        g_ldso_stage0_state.at_phnum == 0U ||
        g_ldso_stage0_state.at_phent != sizeof(ldso_elf_phdr_t)) {
        return -1;
    }

    info.dlpi_addr = g_ldso_stage0_state.load_bias;
    info.dlpi_name = "";
    info.dlpi_phdr = (const Elf64_Phdr *)(uintptr_t)g_ldso_stage0_state.at_phdr;
    info.dlpi_phnum = (Elf64_Half)g_ldso_stage0_state.at_phnum;
    return callback(&info, sizeof(info), data);
}

int backtrace(void **buffer, int size)
{
    void **frame;
    int count = 0;

    if (!buffer || size <= 0) {
        return 0;
    }

    frame = (void **)__builtin_frame_address(0);
    while (frame && count < size) {
        void *return_address;
        void **next_frame;
        uint64_t frame_addr;
        uint64_t next_addr;

        return_address = frame[1];
        if (!return_address) {
            break;
        }
        buffer[count++] = return_address;

        next_frame = (void **)frame[0];
        if (!next_frame) {
            break;
        }

        frame_addr = (uint64_t)(uintptr_t)frame;
        next_addr = (uint64_t)(uintptr_t)next_frame;
        if (next_addr <= frame_addr) {
            break;
        }
        if ((next_addr - frame_addr) > (1ULL << 20U)) {
            break;
        }

        frame = next_frame;
    }

    return count;
}

static int ldso_stage0_resolve_symbol(const char *name, uint64_t *address, void *context)
{
    void *resolved = 0;

    (void)context;

    if (!name || !address) {
        return 0;
    }

    resolved = ldso_dlfcn_resolve_global(name);
    if (!resolved) {
        return 0;
    }

    *address = (uint64_t)(uintptr_t)resolved;
    return 1;
}

static int ldso_stage0_register_builtin_symbols(void)
{
    int registered_primary;
    int registered_alias;

    g_ldso_stage0_builtin_symbols[0].name = "gnuos_libc_stub_touch";
    g_ldso_stage0_builtin_symbols[0].address = (uint64_t)(uintptr_t)ldso_builtin_touch;
    g_ldso_stage0_builtin_symbols[1].name = "_exit";
    g_ldso_stage0_builtin_symbols[1].address = (uint64_t)(uintptr_t)ldso_builtin_exit;
    g_ldso_stage0_builtin_symbols[2].name = "dlopen";
    g_ldso_stage0_builtin_symbols[2].address = (uint64_t)(uintptr_t)ldso_dlopen;
    g_ldso_stage0_builtin_symbols[3].name = "dlsym";
    g_ldso_stage0_builtin_symbols[3].address = (uint64_t)(uintptr_t)ldso_dlsym;
    g_ldso_stage0_builtin_symbols[4].name = "dlclose";
    g_ldso_stage0_builtin_symbols[4].address = (uint64_t)(uintptr_t)ldso_dlclose;
    g_ldso_stage0_builtin_symbols[5].name = "dlerror";
    g_ldso_stage0_builtin_symbols[5].address = (uint64_t)(uintptr_t)ldso_dlerror;
    g_ldso_stage0_builtin_symbols[6].name = "__gnuos_store_startup";
    g_ldso_stage0_builtin_symbols[6].address = (uint64_t)(uintptr_t)__gnuos_store_startup;
    g_ldso_stage0_builtin_symbols[7].name = "dl_iterate_phdr";
    g_ldso_stage0_builtin_symbols[7].address = (uint64_t)(uintptr_t)dl_iterate_phdr;
    g_ldso_stage0_builtin_symbols[8].name = "backtrace";
    g_ldso_stage0_builtin_symbols[8].address = (uint64_t)(uintptr_t)backtrace;
    g_ldso_stage0_builtin_symbols[9].name = "__gnuos_set_tls_base";
    g_ldso_stage0_builtin_symbols[9].address = (uint64_t)(uintptr_t)__gnuos_set_tls_base;
    g_ldso_stage0_builtin_symbols[10].name = "__gnuos_get_tls_base";
    g_ldso_stage0_builtin_symbols[10].address = (uint64_t)(uintptr_t)__gnuos_get_tls_base;
    g_ldso_stage0_builtin_symbols[11].name = "__tls_get_addr";
    g_ldso_stage0_builtin_symbols[11].address = (uint64_t)(uintptr_t)__tls_get_addr;
    g_ldso_stage0_builtin_symbols[12].name = "pthread_create";
    g_ldso_stage0_builtin_symbols[12].address = (uint64_t)(uintptr_t)pthread_create;
    g_ldso_stage0_builtin_symbols[13].name = "pthread_self";
    g_ldso_stage0_builtin_symbols[13].address = (uint64_t)(uintptr_t)pthread_self;
    g_ldso_stage0_builtin_symbols[14].name = "pthread_equal";
    g_ldso_stage0_builtin_symbols[14].address = (uint64_t)(uintptr_t)pthread_equal;
    g_ldso_stage0_builtin_symbols[15].name = "pthread_join";
    g_ldso_stage0_builtin_symbols[15].address = (uint64_t)(uintptr_t)pthread_join;
    g_ldso_stage0_builtin_symbols[16].name = "pthread_detach";
    g_ldso_stage0_builtin_symbols[16].address = (uint64_t)(uintptr_t)pthread_detach;
    g_ldso_stage0_builtin_symbols[17].name = "pthread_attr_init";
    g_ldso_stage0_builtin_symbols[17].address = (uint64_t)(uintptr_t)pthread_attr_init;
    g_ldso_stage0_builtin_symbols[18].name = "pthread_attr_destroy";
    g_ldso_stage0_builtin_symbols[18].address = (uint64_t)(uintptr_t)pthread_attr_destroy;
    g_ldso_stage0_builtin_symbols[19].name = "pthread_attr_setstacksize";
    g_ldso_stage0_builtin_symbols[19].address = (uint64_t)(uintptr_t)pthread_attr_setstacksize;
    g_ldso_stage0_builtin_symbols[20].name = "pthread_attr_getstacksize";
    g_ldso_stage0_builtin_symbols[20].address = (uint64_t)(uintptr_t)pthread_attr_getstacksize;
    g_ldso_stage0_builtin_symbols[21].name = "sem_init";
    g_ldso_stage0_builtin_symbols[21].address = (uint64_t)(uintptr_t)sem_init;
    g_ldso_stage0_builtin_symbols[22].name = "sem_destroy";
    g_ldso_stage0_builtin_symbols[22].address = (uint64_t)(uintptr_t)sem_destroy;
    g_ldso_stage0_builtin_symbols[23].name = "sem_wait";
    g_ldso_stage0_builtin_symbols[23].address = (uint64_t)(uintptr_t)sem_wait;
    g_ldso_stage0_builtin_symbols[24].name = "sem_trywait";
    g_ldso_stage0_builtin_symbols[24].address = (uint64_t)(uintptr_t)sem_trywait;
    g_ldso_stage0_builtin_symbols[25].name = "sem_post";
    g_ldso_stage0_builtin_symbols[25].address = (uint64_t)(uintptr_t)sem_post;
    g_ldso_stage0_builtin_symbols[26].name = "sem_getvalue";
    g_ldso_stage0_builtin_symbols[26].address = (uint64_t)(uintptr_t)sem_getvalue;
    g_ldso_stage0_builtin_symbols[27].name = "sigemptyset";
    g_ldso_stage0_builtin_symbols[27].address = (uint64_t)(uintptr_t)sigemptyset;
    g_ldso_stage0_builtin_symbols[28].name = "sigfillset";
    g_ldso_stage0_builtin_symbols[28].address = (uint64_t)(uintptr_t)sigfillset;
    g_ldso_stage0_builtin_symbols[29].name = "sigaddset";
    g_ldso_stage0_builtin_symbols[29].address = (uint64_t)(uintptr_t)sigaddset;
    g_ldso_stage0_builtin_symbols[30].name = "sigdelset";
    g_ldso_stage0_builtin_symbols[30].address = (uint64_t)(uintptr_t)sigdelset;
    g_ldso_stage0_builtin_symbols[31].name = "sigismember";
    g_ldso_stage0_builtin_symbols[31].address = (uint64_t)(uintptr_t)sigismember;
    g_ldso_stage0_builtin_symbols[32].name = "sigaction";
    g_ldso_stage0_builtin_symbols[32].address = (uint64_t)(uintptr_t)sigaction;
    g_ldso_stage0_builtin_symbols[33].name = "sigprocmask";
    g_ldso_stage0_builtin_symbols[33].address = (uint64_t)(uintptr_t)sigprocmask;
    g_ldso_stage0_builtin_symbols[34].name = "kill";
    g_ldso_stage0_builtin_symbols[34].address = (uint64_t)(uintptr_t)kill;
    g_ldso_stage0_builtin_symbols[35].name = "raise";
    g_ldso_stage0_builtin_symbols[35].address = (uint64_t)(uintptr_t)raise;
    g_ldso_stage0_builtin_symbols[36].name = "signal";
    g_ldso_stage0_builtin_symbols[36].address = (uint64_t)(uintptr_t)signal;
    g_ldso_stage0_builtin_symbols[37].name = "socket";
    g_ldso_stage0_builtin_symbols[37].address = (uint64_t)(uintptr_t)socket;
    g_ldso_stage0_builtin_symbols[38].name = "bind";
    g_ldso_stage0_builtin_symbols[38].address = (uint64_t)(uintptr_t)bind;
    g_ldso_stage0_builtin_symbols[39].name = "listen";
    g_ldso_stage0_builtin_symbols[39].address = (uint64_t)(uintptr_t)listen;
    g_ldso_stage0_builtin_symbols[40].name = "accept";
    g_ldso_stage0_builtin_symbols[40].address = (uint64_t)(uintptr_t)accept;
    g_ldso_stage0_builtin_symbols[41].name = "connect";
    g_ldso_stage0_builtin_symbols[41].address = (uint64_t)(uintptr_t)connect;
    g_ldso_stage0_builtin_symbols[42].name = "send";
    g_ldso_stage0_builtin_symbols[42].address = (uint64_t)(uintptr_t)send;
    g_ldso_stage0_builtin_symbols[43].name = "recv";
    g_ldso_stage0_builtin_symbols[43].address = (uint64_t)(uintptr_t)recv;
    g_ldso_stage0_builtin_symbols[44].name = "sendto";
    g_ldso_stage0_builtin_symbols[44].address = (uint64_t)(uintptr_t)sendto;
    g_ldso_stage0_builtin_symbols[45].name = "recvfrom";
    g_ldso_stage0_builtin_symbols[45].address = (uint64_t)(uintptr_t)recvfrom;
    g_ldso_stage0_builtin_symbols[46].name = "shutdown";
    g_ldso_stage0_builtin_symbols[46].address = (uint64_t)(uintptr_t)shutdown;
    g_ldso_stage0_builtin_symbols[47].name = "htons";
    g_ldso_stage0_builtin_symbols[47].address = (uint64_t)(uintptr_t)htons;
    g_ldso_stage0_builtin_symbols[48].name = "ntohs";
    g_ldso_stage0_builtin_symbols[48].address = (uint64_t)(uintptr_t)ntohs;
    g_ldso_stage0_builtin_symbols[49].name = "htonl";
    g_ldso_stage0_builtin_symbols[49].address = (uint64_t)(uintptr_t)htonl;
    g_ldso_stage0_builtin_symbols[50].name = "ntohl";
    g_ldso_stage0_builtin_symbols[50].address = (uint64_t)(uintptr_t)ntohl;
    g_ldso_stage0_builtin_symbols[51].name = "inet_pton";
    g_ldso_stage0_builtin_symbols[51].address = (uint64_t)(uintptr_t)inet_pton;
    g_ldso_stage0_builtin_symbols[52].name = "inet_ntop";
    g_ldso_stage0_builtin_symbols[52].address = (uint64_t)(uintptr_t)inet_ntop;
    g_ldso_stage0_builtin_symbols[53].name = "open";
    g_ldso_stage0_builtin_symbols[53].address = (uint64_t)(uintptr_t)open;
    g_ldso_stage0_builtin_symbols[54].name = "close";
    g_ldso_stage0_builtin_symbols[54].address = (uint64_t)(uintptr_t)close;
    g_ldso_stage0_builtin_symbols[55].name = "read";
    g_ldso_stage0_builtin_symbols[55].address = (uint64_t)(uintptr_t)read;
    g_ldso_stage0_builtin_symbols[56].name = "write";
    g_ldso_stage0_builtin_symbols[56].address = (uint64_t)(uintptr_t)write;
    g_ldso_stage0_builtin_symbols[57].name = "lseek";
    g_ldso_stage0_builtin_symbols[57].address = (uint64_t)(uintptr_t)lseek;
    g_ldso_stage0_builtin_symbols[58].name = "fstat";
    g_ldso_stage0_builtin_symbols[58].address = (uint64_t)(uintptr_t)fstat;
    g_ldso_stage0_builtin_symbols[59].name = "stat";
    g_ldso_stage0_builtin_symbols[59].address = (uint64_t)(uintptr_t)stat;
    g_ldso_stage0_builtin_symbols[60].name = "access";
    g_ldso_stage0_builtin_symbols[60].address = (uint64_t)(uintptr_t)access;
    g_ldso_stage0_builtin_symbols[61].name = "chmod";
    g_ldso_stage0_builtin_symbols[61].address = (uint64_t)(uintptr_t)chmod;
    g_ldso_stage0_builtin_symbols[62].name = "mkdir";
    g_ldso_stage0_builtin_symbols[62].address = (uint64_t)(uintptr_t)mkdir;
    g_ldso_stage0_builtin_symbols[63].name = "fchmod";
    g_ldso_stage0_builtin_symbols[63].address = (uint64_t)(uintptr_t)fchmod;
    g_ldso_stage0_builtin_symbols[64].name = "umask";
    g_ldso_stage0_builtin_symbols[64].address = (uint64_t)(uintptr_t)umask;
    g_ldso_stage0_builtin_symbols[65].name = "mmap";
    g_ldso_stage0_builtin_symbols[65].address = (uint64_t)(uintptr_t)mmap;
    g_ldso_stage0_builtin_symbols[66].name = "munmap";
    g_ldso_stage0_builtin_symbols[66].address = (uint64_t)(uintptr_t)munmap;
    g_ldso_stage0_builtin_symbols[67].name = "mprotect";
    g_ldso_stage0_builtin_symbols[67].address = (uint64_t)(uintptr_t)mprotect;
    g_ldso_stage0_builtin_symbols[68].name = "__errno_location";
    g_ldso_stage0_builtin_symbols[68].address = (uint64_t)(uintptr_t)__errno_location;

    registered_primary = ldso_dlfcn_register_builtin_object(
        "stage0-builtins",
        g_ldso_stage0_builtin_symbols,
        LDSO_STAGE0_BUILTIN_SYMBOL_COUNT);
    registered_alias = ldso_dlfcn_register_builtin_object(
        "libc.so.6",
        g_ldso_stage0_builtin_symbols,
        LDSO_STAGE0_BUILTIN_SYMBOL_COUNT);

    if (registered_primary != 0 || registered_alias != 0) {
        return -1;
    }

    return 0;
}

static void ldso_stage0_apply_ld_preload(const uint64_t *initial_stack)
{
    const char *value;
    const char *cursor;

    value = ldso_find_env_value(initial_stack, LDSO_LD_PRELOAD_KEY, LDSO_LD_PRELOAD_KEY_LEN);
    if (!value || value[0] == '\0') {
        return;
    }

    g_ldso_stage0_state.ld_preload_seen = 1U;
    cursor = value;
    while (*cursor) {
        char token[LDSO_PRELOAD_TOKEN_MAX];
        uint64_t token_len = 0U;

        while (*cursor && ldso_is_preload_delimiter(*cursor)) {
            cursor++;
        }
        if (!*cursor) {
            break;
        }

        while (*cursor && !ldso_is_preload_delimiter(*cursor)) {
            if (token_len < (LDSO_PRELOAD_TOKEN_MAX - 1U)) {
                token[token_len++] = *cursor;
            }
            cursor++;
        }
        token[token_len] = '\0';
        if (token_len == 0U) {
            continue;
        }

        g_ldso_stage0_state.ld_preload_attempted++;
        if (ldso_dlopen(token, LDSO_RTLD_NOW | LDSO_RTLD_GLOBAL)) {
            g_ldso_stage0_state.ld_preload_loaded++;
        } else {
            g_ldso_stage0_state.ld_preload_failed++;
        }
    }
}

static uint64_t ldso_compute_load_bias(const ldso_elf_layout_t *layout, uint64_t at_phdr)
{
    if (!layout || !layout->phdr_segment) {
        return 0U;
    }

    if (at_phdr < layout->phdr_segment->p_vaddr) {
        return 0U;
    }

    return at_phdr - layout->phdr_segment->p_vaddr;
}

static void ldso_stage0_state_reset(void)
{
    g_ldso_stage0_tls_base = (uintptr_t)&g_ldso_stage0_tls_storage[0];

    g_ldso_stage0_state.ready = 0U;
    g_ldso_stage0_state.at_base = 0U;
    g_ldso_stage0_state.at_entry = 0U;
    g_ldso_stage0_state.at_phdr = 0U;
    g_ldso_stage0_state.at_phent = 0U;
    g_ldso_stage0_state.at_phnum = 0U;
    g_ldso_stage0_state.load_bias = 0U;
    g_ldso_stage0_state.load_count = 0U;
    g_ldso_stage0_state.has_dynamic_segment = 0U;
    g_ldso_stage0_state.has_gnu_relro = 0U;
    g_ldso_stage0_state.dynamic_ready = 0U;
    g_ldso_stage0_state.relocations_attempted = 0U;
    g_ldso_stage0_state.relocations_applied = 0U;
    g_ldso_stage0_state.relocations_unresolved = 0U;
    g_ldso_stage0_state.relocations_unsupported = 0U;
    g_ldso_stage0_state.init_sequence_attempted = 0U;
    g_ldso_stage0_state.init_sequence_completed = 0U;
    g_ldso_stage0_state.dlfcn_ready = 0U;
    g_ldso_stage0_state.builtin_object_registered = 0U;
    g_ldso_stage0_state.ld_preload_seen = 0U;
    g_ldso_stage0_state.ld_preload_attempted = 0U;
    g_ldso_stage0_state.ld_preload_loaded = 0U;
    g_ldso_stage0_state.ld_preload_failed = 0U;
}

void ldso_stage0_bootstrap(const uint64_t *initial_stack)
{
    const ldso_auxv_entry_t *auxv;
    ldso_elf_layout_t layout;
    ldso_elf_dynamic_info_t dynamic_info;
    ldso_elf_reloc_result_t reloc_result;
    uint64_t load_bias;
    int parsed;

    ldso_stage0_state_reset();

    auxv = ldso_find_auxv(initial_stack);
    if (!auxv) {
        return;
    }

    g_ldso_stage0_state.at_base = ldso_auxv_lookup(auxv, LDSO_AT_BASE);
    g_ldso_stage0_state.at_entry = ldso_auxv_lookup(auxv, LDSO_AT_ENTRY);
    g_ldso_stage0_state.at_phdr = ldso_auxv_lookup(auxv, LDSO_AT_PHDR);
    g_ldso_stage0_state.at_phent = ldso_auxv_lookup(auxv, LDSO_AT_PHENT);
    g_ldso_stage0_state.at_phnum = ldso_auxv_lookup(auxv, LDSO_AT_PHNUM);

    if (g_ldso_stage0_state.at_phdr == 0U || g_ldso_stage0_state.at_phnum == 0U) {
        return;
    }

    parsed = ldso_elf_classify_phdrs(
        (const ldso_elf_phdr_t *)(uintptr_t)g_ldso_stage0_state.at_phdr,
        g_ldso_stage0_state.at_phnum,
        g_ldso_stage0_state.at_phent,
        &layout);
    if (parsed != 0) {
        return;
    }

    load_bias = ldso_compute_load_bias(&layout, g_ldso_stage0_state.at_phdr);
    g_ldso_stage0_state.load_bias = load_bias;
    g_ldso_stage0_state.load_count = layout.load_count;
    g_ldso_stage0_state.has_dynamic_segment = layout.dynamic_segment != 0;
    g_ldso_stage0_state.has_gnu_relro = layout.relro_segment != 0;

    if (ldso_elf_parse_dynamic(&layout, load_bias, &dynamic_info) == 0) {
        ldso_dlfcn_init(&dynamic_info, load_bias);
        g_ldso_stage0_state.dlfcn_ready = 1U;
        if (ldso_stage0_register_builtin_symbols() == 0) {
            g_ldso_stage0_state.builtin_object_registered = 1U;
            ldso_stage0_apply_ld_preload(initial_stack);
        }

        g_ldso_stage0_state.dynamic_ready = 1U;
        g_ldso_stage0_state.relocations_attempted = 1U;

        if (ldso_elf_apply_relocations(
                &dynamic_info,
                load_bias,
                ldso_stage0_resolve_symbol,
                0,
                &reloc_result) == 0) {
            g_ldso_stage0_state.relocations_applied = reloc_result.applied_count;
            g_ldso_stage0_state.relocations_unresolved = reloc_result.unresolved_count;
            g_ldso_stage0_state.relocations_unsupported = reloc_result.unsupported_count;

            g_ldso_stage0_state.init_sequence_attempted = 1U;
            if (ldso_elf_run_init_sequence(&dynamic_info) == 0) {
                g_ldso_stage0_state.init_sequence_completed = 1U;
            }
        }
    }

    g_ldso_stage0_state.ready = 1U;
}
