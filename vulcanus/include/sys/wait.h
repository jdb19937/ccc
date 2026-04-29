#ifndef VULCANUS_SYS_WAIT_H
#define VULCANUS_SYS_WAIT_H
#include <sys/types.h>
#define WIFEXITED(s)    (((s)&0x7f)==0)
#define WEXITSTATUS(s)  (((s)>>8)&0xff)
#define WIFSIGNALED(s)  (((s)&0x7f)!=0 && ((s)&0x7f)!=0x7f)
#define WTERMSIG(s)     ((s)&0x7f)
pid_t wait(int *s);
pid_t waitpid(pid_t p, int *s, int opt);
#endif
