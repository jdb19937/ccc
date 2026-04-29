#ifndef VULCANUS_SIGNAL_H
#define VULCANUS_SIGNAL_H

#include <sys/types.h>

typedef int sig_atomic_t;
typedef unsigned long sigset_t;

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGABRT   6
#define SIGFPE    8
#define SIGKILL   9
#define SIGSEGV  11
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGCHLD  20
#define SIGUSR1  30
#define SIGUSR2  31

#define SIG_DFL ((void(*)(int))0)
#define SIG_IGN ((void(*)(int))1)
#define SIG_ERR ((void(*)(int))-1)

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int      sa_flags;
};

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact);
int sigemptyset(sigset_t *s);
int sigfillset(sigset_t *s);
int sigaddset(sigset_t *s, int sig);
int sigdelset(sigset_t *s, int sig);
int sigprocmask(int how, const sigset_t *set, sigset_t *oset);
int sigsuspend(const sigset_t *mask);
int kill(pid_t pid, int sig);
int raise(int sig);
void (*signal(int sig, void (*func)(int)))(int);
unsigned alarm(unsigned s);
int      ualarm(unsigned useconds, unsigned interval);

#endif
