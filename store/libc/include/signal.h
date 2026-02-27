#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <bits/alltypes.h>
#include <amethyst/signal.h>

int kill(pid_t pid, int sig);
int raise(int sig);

#endif /* _SIGNAL_H */

