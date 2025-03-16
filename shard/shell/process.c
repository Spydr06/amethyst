#define _POSIX_C_SOURCE 200809L

#include "shell.h"
    
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/types.h>

#include <string.h>

int shell_process(size_t argc, char** argv, enum shell_process_flags flags) {
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
        int pid_status;
        waitpid(pid, &pid_status, 0);

        if(WIFEXITED(pid_status)) {
            int exit_code = WEXITSTATUS(pid_status);
            if(exit_code == 255) {
                errorf("%s: command not found", argv[0]);
            }
            return exit_code;
        }
        else if(WIFSIGNALED(pid_status)) {
            int sig = WTERMSIG(pid_status);
            errorf("[%d] %s", pid, strsignal(sig));
            return 128 + sig;
        }
        else if(WIFSTOPPED(pid_status)) {
            int sig = WSTOPSIG(pid_status);
            errorf("[%d] %s", pid, strsignal(sig));
            return 128 + sig;
        }
        
    }
    else {
        printf("+ [%d]\n", pid);
    }

    return -err;
}
