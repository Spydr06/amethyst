#include <unistd.h>
#include <stdarg.h>

#include <sys/syscall.h>

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    return syscall(SYS_execve, pathname, argv, envp);
}

int execv(const char* pathname, char *const argv[]) {
    return execve(pathname, argv, environ);
}

int execl(const char *pathname, const char *arg0, ...) {
    va_list ap;
    va_start(ap, arg0);

    int argc = 1;
    while(va_arg(ap, const char*))
        argc++;

    va_end(ap);

    char *argv[argc + 1];
    argv[0] = (char*) arg0;
    va_start(ap, arg0);
    
    for(int i = 0; i < argc; i++)
        argv[i] = va_arg(ap, char*);
    argv[argc] = NULL;

    va_end(ap);
    return execv(pathname, argv);
}

int execvp(const char *file, char *const argv[]);
int execlp(const char *file, const char *arg, ...);
int execle(const char *pathname, const char* arg, ... /*, char *const envp[] */);
int execvpe(const char *file, char *const argv[], char *const envp[]);
