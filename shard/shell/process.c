#define _POSIX_C_SOURCE 200809L

#include "shell.h"
    
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <string.h>

/*int shell_process(size_t argc, char** argv, enum shell_process_flags flags) {
    pid_t pid = fork();
    
    assert(argc > 0);

    int err = 0;
    if(pid == 0) {
        execvp(argv[0], argv);
        exit(255);
    }
    else if(pid < 0) {
        err = -pid;
    }
    else if(flags & SH_PROC_WAIT) {
        int status;
        waitpid(pid, &status, 0);

        
    }
    else {
        printf("+ [%d]\n", pid);
    }

    return -err;
}*/

static void sigint_handler(int sig) {
    if(!shell.fg_pid)
        return;

    signal(sig, SIG_IGN);

    if(kill(shell.fg_pid, SIGINT) < 0)
        errorf("failed interrupting process `%d`: %s", shell.fg_pid, strerror(errno));

    if(shell.jmp_on_sigint)
        longjmp(shell.sigint_jmp, sig);
}

void install_signal_handlers(void) {
    signal(SIGINT, sigint_handler);
}

int shell_create_process(int argc, char** argv) {
    assert(argc > 0);

    pid_t pid = fork();
    
    if(pid == 0) {
        if(raise(SIGSTOP))
            errorf("%s: failed stopping", argv[0]);

        execvp(argv[0], argv);

        errorf("%s: command not found", argv[0]);
        exit(255);
    }
    else if(pid < 0)
        return pid;

    int status;
    waitpid(pid, &status, WUNTRACED);

    if(!WIFSTOPPED(status)) {
        assert(false && "???");
    }

    return pid;
}

int shell_pipe(int64_t src_pid, int64_t dst_pid, enum shell_iostream ios) {
    assert(!"unimplemented");
}

int shell_redirect(int64_t src_pid, const char* restrict dst_path, enum shell_iostream ios) {
    assert(!"unimplemented");
}

int shell_waitpid(int64_t pid) {
    assert(shell.fg_pid == 0);

    int err = shell_resume(pid);
    if(err)
        return err;

    shell.fg_pid = pid;

    int status;
    if(waitpid(pid, &status, 0) < 0)
        return errno;

    if(WIFEXITED(status)) {
        shell.fg_pid = 0;
        return WEXITSTATUS(status);
    }
    else if(WIFSIGNALED(status)) {
        shell.fg_pid = 0;
        int sig = WTERMSIG(status);
        errorf("[%ld] %s", pid, strsignal(sig));
        return 128 + sig;
    }
    else if(WIFSTOPPED(status)) {
        shell.fg_pid = 0;
        int sig = WSTOPSIG(status);
        errorf("[%ld] %s", pid, strsignal(sig));
        return 128 + sig;
    }

    assert(!"unreachable");
}

int shell_resume(int64_t pid) {
    return kill(pid, SIGCONT) < 0 ? errno : 0;
}

int shell_suspend(int64_t pid) {
    return kill(pid, SIGSTOP) < 0 ? errno : 0;
}

