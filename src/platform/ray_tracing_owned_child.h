#ifndef RAY_TRACING_OWNED_CHILD_H
#define RAY_TRACING_OWNED_CHILD_H

#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Private local helper groups only. The launcher must create a group whose ID
 * is the direct child's PID before executing any helper code. Never use this
 * for a durable independently owned render or a borrowed PID. */
static inline bool RayTracingOwnedChild_Stop(pid_t child, int *status) {
    if (child <= 0) return false;
    siginfo_t info = {0};
    int observed;
    do { observed = waitid(P_PID, (id_t)child, &info, WEXITED | WNOHANG | WNOWAIT); }
    while (observed < 0 && errno == EINTR);
    if (observed < 0) return false;
    /* The unreaped child reserves the process/group identity even after EOF. */
    (void)kill(-child, SIGTERM);
    (void)kill(child, SIGTERM);
    for (unsigned attempt = 0; attempt < 10 && !info.si_pid; ++attempt) {
        struct timespec pause = {0, 10000000};
        (void)nanosleep(&pause, NULL);
        do { observed = waitid(P_PID, (id_t)child, &info, WEXITED | WNOHANG | WNOWAIT); }
        while (observed < 0 && errno == EINTR);
        if (observed < 0) return false;
    }
    (void)kill(-child, SIGKILL);
    (void)kill(child, SIGKILL);
    pid_t result;
    do { result = waitpid(child, status, 0); } while (result < 0 && errno == EINTR);
    return result == child;
}

/* 0: still running; 1: descendants stopped and direct child reaped; -1: owner
 * no longer has a waitable child. Status is meaningful only for result 1. */
static inline int RayTracingOwnedChild_Poll(pid_t child, int *status) {
    if (child <= 0) return -1;
    siginfo_t info = {0};
    int result;
    do { result = waitid(P_PID, (id_t)child, &info, WEXITED | WNOHANG | WNOWAIT); }
    while (result < 0 && errno == EINTR);
    if (result < 0) return -1;
    if (!info.si_pid) return 0;
    return RayTracingOwnedChild_Stop(child, status) ? 1 : -1;
}
#endif
