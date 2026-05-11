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

#define GNUOS_AT_NULL 0UL
#define GNUOS_AT_PHDR 3UL
#define GNUOS_AT_PHENT 4UL
#define GNUOS_AT_PHNUM 5UL

#define GNUOS_PT_PHDR 6U

static char **g_startup_envp;
static unsigned long g_tls_bootstrap_area[64];
static unsigned long g_tls_base_addr;
static int g_errno_value;

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
#define GNUOS_PATH_MAX 256U

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
static char g_cwd[GNUOS_PATH_MAX] = "/";

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

static char *gnuos_fail_str(int error)
{
    g_errno_value = error;
    return 0;
}

void gnuos_libc_stub_touch(void)
{
}

static void gnuos_tls_bootstrap_init(void)
{
    if (g_tls_base_addr != 0UL) {
        return;
    }

    g_tls_bootstrap_area[0] = (unsigned long)&g_tls_bootstrap_area[0];
    g_tls_base_addr = (unsigned long)&g_tls_bootstrap_area[0];
}

void __gnuos_store_startup(unsigned long argc, char **argv, char **envp)
{
    (void)argc;
    (void)argv;
    g_startup_envp = envp;
    gnuos_tls_bootstrap_init();
}

static const unsigned long *gnuos_startup_auxv(void)
{
    char **envp = g_startup_envp;

    if (!envp) {
        return 0;
    }

    while (*envp) {
        envp++;
    }

    return (const unsigned long *)(envp + 1);
}

static unsigned long gnuos_auxv_lookup(unsigned long key)
{
    const unsigned long *auxv = gnuos_startup_auxv();

    if (!auxv) {
        return 0UL;
    }

    while (auxv[0] != GNUOS_AT_NULL) {
        if (auxv[0] == key) {
            return auxv[1];
        }
        auxv += 2;
    }

    return 0UL;
}

static unsigned long gnuos_compute_dlpi_addr(const Elf64_Phdr *phdr, unsigned long phnum, unsigned long at_phdr)
{
    unsigned long index;

    if (!phdr || at_phdr == 0UL) {
        return 0UL;
    }

    for (index = 0UL; index < phnum; index++) {
        if (phdr[index].p_type == GNUOS_PT_PHDR && at_phdr >= phdr[index].p_vaddr) {
            return at_phdr - phdr[index].p_vaddr;
        }
    }

    return 0UL;
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data)
{
    struct dl_phdr_info info;
    unsigned long at_phdr;
    unsigned long at_phnum;
    unsigned long at_phent;
    const Elf64_Phdr *phdr;

    if (!callback) {
        return -1;
    }

    at_phdr = gnuos_auxv_lookup(GNUOS_AT_PHDR);
    at_phnum = gnuos_auxv_lookup(GNUOS_AT_PHNUM);
    at_phent = gnuos_auxv_lookup(GNUOS_AT_PHENT);
    if (at_phdr == 0UL || at_phnum == 0UL || at_phent != sizeof(Elf64_Phdr)) {
        return -1;
    }

    phdr = (const Elf64_Phdr *)(unsigned long)at_phdr;
    info.dlpi_addr = gnuos_compute_dlpi_addr(phdr, at_phnum, at_phdr);
    info.dlpi_name = "";
    info.dlpi_phdr = phdr;
    info.dlpi_phnum = (Elf64_Half)at_phnum;

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
        unsigned long frame_addr;
        unsigned long next_addr;

        return_address = frame[1];
        if (!return_address) {
            break;
        }
        buffer[count++] = return_address;

        next_frame = (void **)frame[0];
        if (!next_frame) {
            break;
        }

        frame_addr = (unsigned long)frame;
        next_addr = (unsigned long)next_frame;
        if (next_addr <= frame_addr) {
            break;
        }
        if ((next_addr - frame_addr) > (1UL << 20)) {
            break;
        }

        frame = next_frame;
    }

    return count;
}

int __gnuos_set_tls_base(void *base)
{
    gnuos_tls_bootstrap_init();
    g_tls_base_addr = (unsigned long)base;
    return 0;
}

void *__gnuos_get_tls_base(void)
{
    gnuos_tls_bootstrap_init();
    return (void *)g_tls_base_addr;
}

void *__tls_get_addr(gnuos_tls_index_t *index)
{
    unsigned long base;

    if (!index) {
        return 0;
    }

    gnuos_tls_bootstrap_init();
    base = g_tls_base_addr;
    return (void *)(base + index->ti_offset);
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

pid_t getpid(void)
{
    return 1;
}

pid_t getppid(void)
{
    return 0;
}

uid_t getuid(void)
{
    return 0U;
}

uid_t geteuid(void)
{
    return 0U;
}

gid_t getgid(void)
{
    return 0U;
}

gid_t getegid(void)
{
    return 0U;
}

int chdir(const char *path)
{
    size_t length = 0U;
    size_t index;

    if (!path || path[0] == '\0') {
        return gnuos_fail_int(GNUOS_PTHREAD_EINVAL);
    }
    if (path[0] != '/' && path[0] != '.') {
        return gnuos_fail_int(GNUOS_FILE_ENOENT);
    }

    while (path[length] != '\0') {
        if (length >= (GNUOS_PATH_MAX - 1U)) {
            return gnuos_fail_int(ERANGE);
        }
        length++;
    }

    for (index = 0U; index <= length; index++) {
        g_cwd[index] = path[index];
    }
    return 0;
}

char *getcwd(char *buf, size_t size)
{
    size_t length = 0U;
    size_t index;

    if (!buf || size == 0U) {
        return gnuos_fail_str(GNUOS_PTHREAD_EINVAL);
    }

    while (g_cwd[length] != '\0') {
        length++;
    }
    if (size <= length) {
        return gnuos_fail_str(ERANGE);
    }

    for (index = 0U; index <= length; index++) {
        buf[index] = g_cwd[index];
    }
    return buf;
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

__attribute__((noreturn)) void _exit(int status)
{
    (void)status;
    for (;;) {
        __asm__ volatile("pause");
    }
}
