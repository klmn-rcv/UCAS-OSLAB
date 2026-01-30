#include <sys/syscall.h>

#include <printk.h>

long (*syscall[NUM_SYSCALLS])(long,long,long,long,long);

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */

    regs->sepc += 4;

    unsigned long sysno = regs->regs[17];
    unsigned long arg0 = regs->regs[10];
    unsigned long arg1 = regs->regs[11];
    unsigned long arg2 = regs->regs[12];
    unsigned long arg3 = regs->regs[13];
    unsigned long arg4 = regs->regs[14];

    regs->regs[10] = syscall[sysno](arg0, arg1, arg2, arg3, arg4);  // a0
}
