#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    long ret;

    asm volatile(
        "mv a7, %1\n\t"
        "mv a0, %2\n\t"
        "mv a1, %3\n\t"
        "mv a2, %4\n\t"
        "mv a3, %5\n\t"
        "mv a4, %6\n\t"
        "ecall\n\t"
        "mv %0, a0\n\t"
        : "=r"(ret)
        : "r"(sysno), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4) 
        : "memory", "a0", "a1", "a2", "a3", "a4", "a7"
    );

    return ret;
}

void sys_yield(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_yield */
    // call_jmptab(YIELD, 0, 0, 0, 0, 0);

    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_move_cursor */
    // call_jmptab(MOVE_CURSOR, (long)x, (long)y, 0, 0, 0);

    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall(SYSCALL_CURSOR, (long)x, (long)y, 0, 0, 0);
}

void sys_write(char *buff)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_write */
    // call_jmptab(PRINT, (long)buff, 0, 0, 0, 0);

    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall(SYSCALL_WRITE, (long)buff, 0, 0, 0, 0);
}

void sys_reflush(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
    // call_jmptab(REFLUSH, 0, 0, 0, 0, 0);

    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall(SYSCALL_REFLUSH, 0, 0, 0, 0, 0);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
    // return call_jmptab(MUTEX_INIT, (long)key, 0, 0, 0, 0);

    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    int ret = invoke_syscall(SYSCALL_LOCK_INIT, (long)key, 0, 0, 0, 0);
    return ret;
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    // call_jmptab(MUTEX_ACQ, (long)mutex_idx, 0, 0, 0, 0);

    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(SYSCALL_LOCK_ACQ, (long)mutex_idx, 0, 0, 0, 0);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    // call_jmptab(MUTEX_RELEASE, (long)mutex_idx, 0, 0, 0, 0);

    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_LOCK_RELEASE, (long)mutex_idx, 0, 0, 0, 0);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    long timebase = invoke_syscall(SYSCALL_GET_TIMEBASE, 0, 0, 0, 0, 0);
    return timebase;
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    long tick = invoke_syscall(SYSCALL_GET_TICK, 0, 0, 0, 0, 0);
    return tick;
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP, (long)time, 0, 0, 0, 0);
}

/************************************************************/
#ifdef S_CORE
pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}    
#else
pid_t  sys_exec(char *name, int argc, char **argv)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
    pid_t pid = invoke_syscall(SYSCALL_EXEC, (long)name, (long)argc, (long)argv, 0, 0);
    return pid;
}
#endif

void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
    invoke_syscall(SYSCALL_EXIT, 0, 0, 0, 0, 0);
}

int  sys_kill(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
    int success = invoke_syscall(SYSCALL_KILL, (long)pid, 0, 0, 0, 0);
    return success;
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
    int success = invoke_syscall(SYSCALL_WAITPID, (long)pid, 0, 0, 0, 0);
    return success;
}


void sys_ps(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
    invoke_syscall(SYSCALL_PS, 0, 0, 0, 0, 0);
}

pid_t sys_getpid()
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
    pid_t pid = invoke_syscall(SYSCALL_GETPID, 0, 0, 0, 0, 0);
    return pid;
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
    int ch = invoke_syscall(SYSCALL_READCH, 0, 0, 0, 0, 0);
    return ch;
}

void sys_clear(void)
{
    invoke_syscall(SYSCALL_CLEAR, 0, 0, 0, 0, 0);
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
    int bar_idx = invoke_syscall(SYSCALL_BARR_INIT, (long)key, (long)goal, 0, 0, 0);
    return bar_idx;
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
    invoke_syscall(SYSCALL_BARR_WAIT, (long)bar_idx, 0, 0, 0, 0);
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
    invoke_syscall(SYSCALL_BARR_DESTROY, (long)bar_idx, 0, 0, 0, 0);
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
    int cond_idx = invoke_syscall(SYSCALL_COND_INIT, (long)key, 0, 0, 0, 0);
    return cond_idx;
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
    invoke_syscall(SYSCALL_COND_WAIT, (long)cond_idx, (long)mutex_idx, 0, 0, 0);
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
    invoke_syscall(SYSCALL_COND_SIGNAL, (long)cond_idx, 0, 0, 0, 0);
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
    invoke_syscall(SYSCALL_COND_BROADCAST, (long)cond_idx, 0, 0, 0, 0);
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
    invoke_syscall(SYSCALL_COND_DESTROY, (long)cond_idx, 0, 0, 0, 0);
}

int sys_semaphore_init(int key, int init)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_init */
}

void sys_semaphore_up(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_up */
}

void sys_semaphore_down(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_down */
}

void sys_semaphore_destroy(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_destroy */
}

int sys_mbox_open(char * name)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
    int mbox_idx = invoke_syscall(SYSCALL_MBOX_OPEN, (long)name, 0, 0, 0, 0);
    return mbox_idx;
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
    invoke_syscall(SYSCALL_MBOX_CLOSE, (long)mbox_id, 0, 0, 0, 0);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
    int ret = invoke_syscall(SYSCALL_MBOX_SEND, (long)mbox_idx, (long)msg, (long)msg_length, 0, 0);
    return ret;
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
    int ret = invoke_syscall(SYSCALL_MBOX_RECV, (long)mbox_idx, (long)msg, (long)msg_length, 0, 0);
    return ret;
}

pid_t sys_taskset(uint32_t mask, char *taskname)
{
    pid_t pid = invoke_syscall(SYSCALL_TASKSET, (long)mask, (long)taskname, 0, 0, 0);
    return pid;
}

int sys_taskset_p(uint32_t mask, pid_t pid)
{
    int success = invoke_syscall(SYSCALL_TASKSET_P, (long)mask, (long)pid, 0, 0, 0);
    return success;
}

size_t sys_free_mem(void) {
    size_t free_mem = invoke_syscall(SYSCALL_FREE_MEM, 0, 0, 0, 0, 0);
    return free_mem;
}

/* TODO: [P4 task5] pipe*/
int sys_pipe_open(const char *name) {
    int pipe_idx = invoke_syscall(SYSCALL_PIPE_OPEN, (long)name, 0, 0, 0, 0);
    return pipe_idx;
}

long sys_pipe_give_pages(int pipe_idx, void *src, size_t length) {
    long send_bytes = invoke_syscall(SYSCALL_PIPE_GIVE, (long)pipe_idx, (long)src, (long)length, 0, 0);
    return send_bytes;
}

long sys_pipe_take_pages(int pipe_idx, void *dst, size_t length) {
    long recv_bytes = invoke_syscall(SYSCALL_PIPE_TAKE, (long)pipe_idx, (long)dst, (long)length, 0, 0);
    return recv_bytes;
}

// tid_t sys_thread_create(void *func, void *arg) {
//     tid_t tid = invoke_syscall(SYSCALL_THREAD_CREATE, (long)func, (long)arg, 0, 0, 0);
//     return tid;
// }

// void sys_thread_join(tid_t tid) {
//     invoke_syscall(SYSCALL_THREAD_JOIN, (long)tid, 0, 0, 0, 0);
// }

// void sys_thread_exit() {
//     invoke_syscall(SYSCALL_THREAD_EXIT, 0, 0, 0, 0, 0);
// }
/************************************************************/

int sys_net_send(void *txpacket, int length)
{
    /* TODO: [p5-task1] call invoke_syscall to implement sys_net_send */
    int transmit_len = invoke_syscall(SYSCALL_NET_SEND, (long)txpacket, (long)length, 0, 0, 0);
    return transmit_len;
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    /* TODO: [p5-task2] call invoke_syscall to implement sys_net_recv */
    int received_len = invoke_syscall(SYSCALL_NET_RECV, (long)rxbuffer, (long)pkt_num, (long)pkt_lens, 0, 0);
    return received_len;
}

int sys_net_recv_stream(void *buffer, int *nbytes) {
    int received_len = invoke_syscall(SYSCALL_NET_RECV_STREAM, (long)buffer, (long)nbytes, 0, 0, 0);
    return received_len;
}

int sys_mkfs(void)
{
    // TODO [P6-task1]: Implement sys_mkfs
    return 0;  // sys_mkfs succeeds
}

int sys_statfs(void)
{
    // TODO [P6-task1]: Implement sys_statfs
    return 0;  // sys_statfs succeeds
}

int sys_cd(char *path)
{
    // TODO [P6-task1]: Implement sys_cd
    return 0;  // sys_cd succeeds
}

int sys_mkdir(char *path)
{
    // TODO [P6-task1]: Implement sys_mkdir
    return 0;  // sys_mkdir succeeds
}

int sys_rmdir(char *path)
{
    // TODO [P6-task1]: Implement sys_rmdir
    return 0;  // sys_rmdir succeeds
}

int sys_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement sys_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    return 0;  // sys_ls succeeds
}

int sys_open(char *path, int mode)
{
    // TODO [P6-task2]: Implement sys_open
    return 0;  // return the id of file descriptor
}

int sys_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_fread
    return 0;  // return the length of trully read data
}

int sys_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement sys_fwrite
    return 0;  // return the length of trully written data
}

int sys_close(int fd)
{
    // TODO [P6-task2]: Implement sys_close
    return 0;  // sys_close succeeds
}

int sys_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement sys_ln
    return 0;  // sys_ln succeeds 
}

int sys_rm(char *path)
{
    // TODO [P6-task2]: Implement sys_rm
    return 0;  // sys_rm succeeds 
}

int sys_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement sys_lseek
    return 0;  // the resulting offset location from the beginning of the file
}
/************************************************************/
