#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

#define LENGTH 60

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

list_node_t *get_last_by_progress(list_head *head) { //descending
    if(current_running->pid != 0) {
        uint64_t sum_length = current_running->sum_length;
        uint64_t times = sum_length / 60;
        int remain_length = 60 - (sum_length % 60);
        int check_point = current_running->check_point;
        int this_process;
        if (remain_length > check_point) {
            this_process = 6000 - (6000 * (remain_length - check_point)) / (LENGTH - check_point);
        } else {
            this_process = 10000 - (4000 * remain_length) / check_point;
        }
        current_running->progress = 10000 * times + this_process;
    }

    if (LIST_EMPTY(head)) {
        return NULL;
    }
    else if(head->next->next == head) {
        return head->next;
    }

    list_node_t *node, *next_node;

    uint64_t min_progress = 0xFFFFFFFFFFFFFFFF;
    list_node_t *min_progress_node = NULL;
    for(node = LIST_FIRST(head); node != head; node = next_node) {
        pcb_t *node_pcb = LIST_ENTRY(node, pcb_t, list);
        next_node = node->next;
        if(node_pcb->pid != 0 && node_pcb->progress < min_progress) {
            min_progress = node_pcb->progress;
            min_progress_node = node;
        }
    }

    return min_progress_node;
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
        list_node_t *next_node = get_last_by_progress(&ready_queue);
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
