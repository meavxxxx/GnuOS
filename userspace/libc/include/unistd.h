#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <features.h>
#include <sys/types.h>

__BEGIN_DECLS

ssize_t read(int fd, void *buf, size_t count) __THROW;
ssize_t write(int fd, const void *buf, size_t count) __THROW;
int close(int fd) __THROW;
off_t lseek(int fd, off_t offset, int whence) __THROW;
int access(const char *pathname, int mode) __THROW;
pid_t getpid(void) __THROW;
pid_t getppid(void) __THROW;
uid_t getuid(void) __THROW;
uid_t geteuid(void) __THROW;
gid_t getgid(void) __THROW;
gid_t getegid(void) __THROW;
int chdir(const char *path) __THROW;
char *getcwd(char *buf, size_t size) __THROW;
void _exit(int status) __THROW __attribute__((noreturn));

__END_DECLS

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
