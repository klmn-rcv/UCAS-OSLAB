#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <os/task.h>
#include <os/string.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

#define LENGTH 60

extern int create_task(char *taskname);

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.

    if (current_running->status == TASK_RUNNING) {
        current_running->status = TASK_READY;
        LIST_APPEND(&current_running->list, &ready_queue);
    }

    if(!LIST_EMPTY(&ready_queue)) {
        pcb_t *prev_pcb = current_running;
        list_node_t *next_node = LIST_FIRST(&ready_queue);
        pcb_t *next_pcb = LIST_ENTRY(next_node, pcb_t, list);

        LIST_DELETE(next_node);

        next_pcb->status = TASK_RUNNING;
        current_running = next_pcb;
        asm volatile("mv tp, %0" : : "r"(current_running));

    // TODO: [p2-task1] switch_to current_running

        switch_to(prev_pcb, next_pcb);
    }
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running->status = TASK_BLOCKED;
    LIST_APPEND(&current_running->list, &sleep_queue);
    current_running->wakeup_time = get_timer() + (uint64_t)sleep_time;
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    pcb_t *pcb = LIST_ENTRY(pcb_node, pcb_t, list);
    pcb->status = TASK_BLOCKED;
    LIST_APPEND(pcb_node, queue);
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    pcb_t *pcb = LIST_ENTRY(pcb_node, pcb_t, list);
    pcb->status = TASK_READY;
    LIST_DELETE(pcb_node);
    LIST_APPEND(pcb_node, &ready_queue);
}

pid_t do_exec(char *name, int argc, char *argv[]) {
    int pid = create_task(name);
    if(pid == 0) return 0;  // task not found
    
    pcb[pid].user_sp -= (argc * sizeof(char *));

    regs_context_t *pt_regs = (regs_context_t *)(pcb[pid].kernel_sp + sizeof(switchto_context_t));
    pt_regs->regs[10] = (reg_t)argc;
    pt_regs->regs[11] = (reg_t)pcb[pid].user_sp;

    char **argv_to_user = (char **)pcb[pid].user_sp;

    for(int i = 0; i < argc; i++) {
        int len = strlen(argv[i]);
        pcb[pid].user_sp -= (len + 1);
        argv_to_user[i] = (char *)pcb[pid].user_sp;
        
        for(int j = 0; j < len; j++) {
            *((char *)pcb[pid].user_sp + j) = argv[i][j];
        }
        *((char *)pcb[pid].user_sp + len) = '\0';
    }

    pcb[pid].user_sp &= 0xFFFFFFFFFFFFFFF0;

    pt_regs->regs[2] = pcb[pid].user_sp;  // store sp back to user context
    

    LIST_APPEND(&pcb[pid].list, &ready_queue);

    return pid;
}

static void clear_wait_list(pid_t pid) {
    list_node_t *node, *next_node;
    list_head *head = &pcb[pid].wait_list;
    if(!LIST_EMPTY(head)) {
        for(node = LIST_FIRST(head); node != head; node = next_node) {
            pcb_t *node_pcb = LIST_ENTRY(node, pcb_t, list);
            next_node = node->next;
            LIST_DELETE(node);
            LIST_APPEND(node, &ready_queue);
        }
    }
}

void do_exit(void) {
    current_running->status = TASK_EXITED;
    do_mutex_lock_free(current_running->pid);
    clear_wait_list(current_running->pid);
    do_scheduler();
}

int do_kill(pid_t pid) {
    if(pid <= 0 || pid >= NUM_MAX_TASK || pcb[pid].status == TASK_EXITED) {
        return 0;
    }

    pcb[pid].status = TASK_EXITED;
    do_mutex_lock_free(pid);
    clear_wait_list(pid);
    LIST_DELETE(&pcb[pid].list);
    if(current_running->pid == pid) {
        do_scheduler();
    }
    return 1;
}

int do_waitpid(pid_t pid) {
    if(pid <= 0 || pid >= NUM_MAX_TASK || pcb[pid].status == TASK_EXITED) {
        return 0;
    }
    current_running->status = TASK_BLOCKED;
    LIST_APPEND(&current_running->list, &pcb[pid].wait_list);
    do_scheduler();
}

void do_process_show(void) {
    char *status[3] = {"BLOCKED", "RUNNING", "READY"};
    printk("[Process Table]\n");
    for(int i = 1; i < NUM_MAX_TASK; i++) {
        if(pcb[i].status != TASK_EXITED) {
            printk("[%d] PID : %d  STATUS : %s\n", i - 1, pcb[i].pid, status[pcb[i].status]);
        }
    }
}

pid_t do_getpid(void) {
    return (pid_t)current_running->pid;
}