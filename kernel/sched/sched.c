#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <os/task.h>
#include <os/string.h>
#include <os/smp.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

#define LENGTH 60

extern int create_task(char *taskname);
extern void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, pcb_t *pcb);
//extern void sys_thread_exit();

pcb_t pcb[NUM_MAX_TASK];
pcb_t tcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .run_core_mask = 0x1,
    .running_core_id = 0
};
const ptr_t pid1_stack = INIT_KERNEL_STACK + 2 * PAGE_SIZE;
pcb_t pid1_pcb = {
    .pid = 1,
    .kernel_sp = (ptr_t)pid1_stack,
    .user_sp = (ptr_t)pid1_stack,
    .run_core_mask = 0x2,
    .running_core_id = 1
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

static list_node_t *find_next_node() {
    list_node_t *node, *next_node, *result_node = NULL;
    for(node = LIST_FIRST(&ready_queue); node != &ready_queue; node = next_node) {
        next_node = node->next;
        pcb_t *node_pcb = LIST_ENTRY(node, pcb_t, list);
        uint32_t cpu_mask = (1U << cpuid);
        if(node_pcb->run_core_mask & cpu_mask) {
            result_node = node;
            break;
        }
    }
    return result_node;
}

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
        // list_node_t *next_node = LIST_FIRST(&ready_queue);

        list_node_t *next_node = find_next_node();

        if(next_node == NULL) {
            next_node = &current_running->list;
        }

        pcb_t *next_pcb = LIST_ENTRY(next_node, pcb_t, list);

        LIST_DELETE(next_node);

        next_pcb->status = TASK_RUNNING;
        next_pcb->running_core_id = cpuid;
        current_running = next_pcb;

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
    LIST_DELETE(pcb_node);
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

void clear_wait_queue(list_head *queue) {
    if(!LIST_EMPTY(queue)) {
        list_node_t *node, *next_node;
        for(node = LIST_FIRST(queue); node != queue; node = next_node) {
            next_node = node->next;
            // pcb_t *node_pcb = LIST_ENTRY(node, pcb_t, list);
            // LIST_DELETE(node);
            // node_pcb->status = TASK_READY;
            // LIST_APPEND(node, &ready_queue);
            do_unblock(node);
        }
    }
}

pid_t do_exec(char *name, int argc, char *argv[]) {
    int pid = create_task(name);
    if(pid == 0) return 0;  // task not found

    pcb[pid].run_core_mask = current_running->run_core_mask;
    // pcb[pid].user_sp -= (argc * sizeof(char *));

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
            node_pcb->status = TASK_READY;
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
    return 1;
}

void do_process_show(void) {
    char *status[3] = {"BLOCKED", "RUNNING", "READY"};
    printk("[Process Table]\n");
    for(int i = 2; i < NUM_MAX_TASK; i++) {
        if(pcb[i].status != TASK_EXITED) {
            printk("[%d] PID : %d  STATUS : %s mask: %x", i - 2, pcb[i].pid, status[pcb[i].status], pcb[i].run_core_mask);
            if(pcb[i].status == TASK_RUNNING) {
                printk(" Running on core %d\n", pcb[i].running_core_id);
            } else {
                printk("\n");
            }
        }
    }
}

pid_t do_getpid(void) {
    return (pid_t)current_running->pid;
}

pid_t do_taskset(uint32_t mask, char *taskname) {
    char *argv[1] = {taskname};
    pid_t pid = do_exec(taskname, 1, argv);
    if(pid == 0) {
        return 0;
    }
    pcb[pid].run_core_mask = mask;
}

int do_taskset_p(uint32_t mask, pid_t pid) {
    if(pid <= 0 || pid >= NUM_MAX_TASK || pcb[pid].status == TASK_EXITED) {
        return 0;
    }

    pcb[pid].run_core_mask = mask;
    return 1;
}

tid_t do_thread_create(void *func, void *arg) {
    pcb_t *new_thread = NULL;
    int i;
    for (i = 0; i < 16; i++) {
        if (tcb[i].status == TASK_EXITED) {
            new_thread = &tcb[i];
            break;
        }
    }
    
    assert(new_thread);
    
    tid_t tid = i;
    new_thread->pid = current_running->pid;
    new_thread->is_thread = 1;
    new_thread->tid = tid;

    new_thread->kernel_sp = (reg_t)((tid + NUM_MAX_TASK) * 2 * PAGE_SIZE + ROUND(FREEMEM_KERNEL, PAGE_SIZE));
    new_thread->user_sp = (reg_t)((tid + NUM_MAX_TASK) * 2 * PAGE_SIZE + ROUND(FREEMEM_USER, PAGE_SIZE));
    new_thread->kernel_stack_base = (reg_t)((tid + NUM_MAX_TASK) * 2 * PAGE_SIZE + ROUND(FREEMEM_KERNEL, PAGE_SIZE));
    new_thread->user_stack_base = (reg_t)((tid + NUM_MAX_TASK) * 2 * PAGE_SIZE + ROUND(FREEMEM_USER, PAGE_SIZE));

    new_thread->run_core_mask = current_running->run_core_mask;

    init_pcb_stack(new_thread->kernel_sp, new_thread->user_sp, (ptr_t)func, new_thread);
    ((regs_context_t *)(new_thread->kernel_sp + sizeof(switchto_context_t)))->regs[10] = (reg_t)arg;

    new_thread->status = TASK_READY;
    LIST_APPEND(&new_thread->list, &ready_queue);

    return tid;
}

void do_thread_join(tid_t tid) {
    assert(0 <= tid && tid < NUM_MAX_TASK);
    
    if(tcb[tid].status != TASK_EXITED) {
        do_block(&current_running->list, &tcb[tid].wait_list);
    }
}

void do_thread_exit() {
    assert(current_running->is_thread);
    assert(current_running->tid != -1);
    current_running->status = TASK_EXITED;
    current_running->pid = 0;
    current_running->running_core_id = -1;
    clear_wait_queue(&current_running->wait_list);
    assert(LIST_EMPTY(&current_running->wait_list));
    do_scheduler();
}