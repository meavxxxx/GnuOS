#include <gnuos/target.h>

#ifdef GNUOS_DYNAMIC_SMOKE
#include <execinfo.h>
#include <fcntl.h>
#include <gnuos/tls.h>
#include <link.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

extern void gnuos_libc_stub_touch(void);

__attribute__((constructor)) static void gnuos_dynamic_ctor(void)
{
    gnuos_libc_stub_touch();
}

static int gnuos_dl_phdr_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)info;
    (void)size;
    (void)data;
    return 0;
}

static void *gnuos_pthread_noop(void *arg)
{
    return arg;
}
#endif

int main(int argc, char **argv, char **envp)
{
    (void)GNUOS_TARGET_TRIPLET;
    (void)argc;
    (void)argv;
    (void)envp;
#ifdef GNUOS_DYNAMIC_SMOKE
    void *frames[8];
    gnuos_tls_index_t tls_index;
    void *tls_base;
    pthread_t thread;
    void *thread_result = 0;
    pthread_attr_t attr;
    size_t stack_size = 0;
    sem_t sem;
    int sem_value = 0;
    sigset_t sigset;
    sigset_t sigold;
    struct sigaction sigact;
    struct sigaction sigoldact;
    int sig_member = 0;
    int sockfd = -1;
    struct sockaddr loop_addr;
    char socket_buf[8] = {0};
    in_addr_t loop_addr_be = 0;
    in_port_t loop_port_be = 0;
    char inet_text[16] = "127.0.0.1";
    char inet_out[64];
    int file_fd = -1;
    struct stat file_stat;
    mode_t old_umask = 0;

    gnuos_libc_stub_touch();
    tls_base = __gnuos_get_tls_base();
    (void)__gnuos_set_tls_base(tls_base);
    tls_index.ti_module = 1UL;
    tls_index.ti_offset = 0UL;
    (void)__tls_get_addr(&tls_index);
    (void)backtrace(frames, 8);
    (void)dl_iterate_phdr(gnuos_dl_phdr_cb, 0);
    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setstacksize(&attr, 1UL << 20);
    (void)pthread_attr_getstacksize(&attr, &stack_size);
    (void)stack_size;
    (void)pthread_self();
    (void)pthread_equal(pthread_self(), pthread_self());
    (void)pthread_create(&thread, &attr, gnuos_pthread_noop, 0);
    (void)pthread_join(thread, &thread_result);
    (void)pthread_detach(thread);
    (void)pthread_attr_destroy(&attr);
    (void)sem_init(&sem, 0, 1U);
    (void)sem_wait(&sem);
    (void)sem_post(&sem);
    (void)sem_trywait(&sem);
    (void)sem_getvalue(&sem, &sem_value);
    (void)sem_value;
    (void)sem_destroy(&sem);
    (void)sigemptyset(&sigset);
    (void)sigaddset(&sigset, SIGINT);
    (void)sigdelset(&sigset, SIGINT);
    (void)sigfillset(&sigset);
    sig_member = sigismember(&sigset, SIGTERM);
    (void)sig_member;
    (void)sigprocmask(SIG_BLOCK, &sigset, &sigold);
    (void)sigprocmask(SIG_SETMASK, &sigold, 0);
    sigact.sa_handler = SIG_IGN;
    (void)sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    (void)sigaction(SIGUSR1, &sigact, &sigoldact);
    (void)signal(SIGUSR2, SIG_DFL);
    (void)raise(SIGUSR1);
    (void)kill(0, SIGTERM);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd >= 0) {
        loop_port_be = htons(1234U);
        loop_port_be = ntohs(loop_port_be);
        loop_addr_be = htonl(INADDR_LOOPBACK);
        loop_addr_be = ntohl(loop_addr_be);
        (void)inet_pton(AF_INET, inet_text, &loop_addr_be);
        (void)inet_ntop(AF_INET, &loop_addr_be, inet_out, sizeof(inet_out));
        (void)loop_port_be;
        loop_addr.sa_family = AF_INET;
        loop_addr.sa_data[0] = 0;
        loop_addr.sa_data[1] = 0;
        (void)bind(sockfd, &loop_addr, sizeof(loop_addr));
        (void)listen(sockfd, 1);
        (void)connect(sockfd, &loop_addr, sizeof(loop_addr));
        (void)send(sockfd, socket_buf, sizeof(socket_buf), 0);
        (void)recv(sockfd, socket_buf, sizeof(socket_buf), 0);
        (void)shutdown(sockfd, SHUT_RDWR);
        (void)close(sockfd);
    }
    file_fd = open("./dummy", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (file_fd >= 0) {
        old_umask = umask(022);
        (void)old_umask;
        (void)write(file_fd, socket_buf, sizeof(socket_buf));
        (void)lseek(file_fd, 0, SEEK_SET);
        (void)read(file_fd, socket_buf, sizeof(socket_buf));
        (void)fstat(file_fd, &file_stat);
        (void)chmod("./dummy", 0644);
        (void)fchmod(file_fd, 0644);
        (void)access("./dummy", F_OK);
        (void)close(file_fd);
        (void)umask(old_umask);
    }
    (void)stat("./dummy", &file_stat);
    (void)mkdir("./tmp", 0755);
#endif
    return 0;
}
