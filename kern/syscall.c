#include <syscall.h>
// #include <unistd.h>

#include "trap.h"
#include "console.h"
#include "proc.h"
#include "debug.h"

void
debug_tf(struct trapframe *tf)
{
    debug("elr: 0x%llx", tf->elr);
    debug("sp0: 0x%llx", tf->sp);
    for (int i = 0; i < 32; i++) {
        debug("x[%d] = 0x%llx(%lld)", i, tf->x[i], tf->x[i]);
    }
}

// For debug
void
sys_ptrace()
{
    struct trapframe *tf = thisproc()->tf;
    // debug("ptrace '%s': '%s'", thisproc()->name, tf->x[0]);
    // debug_tf(tf);
}

int
syscall1(struct trapframe *tf)
{
    thisproc()->tf = tf;
    int sysno = tf->x[8];
    switch (sysno) {
    // FIXME: fake ptrace for debug.
    case SYS_ptrace:
        sys_ptrace();
        return 0;

    // FIXME: use pid instead of tid since we don't have threads :)
    case SYS_set_tid_address:
        trace("set_tid_address: name '%s'", thisproc()->name);
        return thisproc()->pid;
    case SYS_gettid:
        trace("gettid: name '%s'", thisproc()->name);
        return thisproc()->pid;

    // FIXME: Hack TIOCGWINSZ(get window size)
    case SYS_ioctl:
        trace("ioctl: name '%s'", thisproc()->name);
        if (tf->x[1] == 0x5413) return 0;
        else panic("ioctl unimplemented. ");

    // FIXME: always return 0 since we don't have signals  :)
    case SYS_rt_sigprocmask:
        trace("rt_sigprocmask: name '%s' how 0x%x", thisproc()->name, (int)tf->x[0]);
        return 0;

    case SYS_brk:
        return sys_brk();
    case SYS_mmap:
        return sys_mmap();

    case SYS_execve:
        return execve(tf->x[0], tf->x[1], tf->x[2]);

    case SYS_sched_yield:
        return sys_yield();

    case SYS_clone:
        return sys_clone();

    case SYS_wait4:
        return sys_wait4();

    // FIXME: exit_group should kill every thread in the current thread group.
    case SYS_exit_group:
    case SYS_exit:
        trace("sys_exit: '%s' exit with code %d", thisproc()->name, tf->x[0]);
        exit(tf->x[0]);

    case SYS_dup:
        return sys_dup();

    case SYS_chdir:
        return sys_chdir();

    case SYS_fstat:
        return sys_fstat();

    case SYS_newfstatat:
        return sys_fstatat();

    case SYS_mkdirat:
        return sys_mkdirat();

    case SYS_mknodat:
        return sys_mknodat();
        
    case SYS_openat:
        return sys_openat();

    case SYS_writev:
        return sys_writev();

    case SYS_read:
        return sys_read();

    case SYS_close:
        return sys_close();

    default:
        // FIXME: don't panic.

        debug_reg();
        panic("Unexpected syscall #%d\n", sysno);
        
        return 0;
    }
}

