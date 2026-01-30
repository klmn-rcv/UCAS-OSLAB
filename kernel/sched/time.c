#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    if(!LIST_EMPTY(&sleep_queue)) {
        uint64_t current_time = get_timer();
        list_node_t *node, *next_node;
        for(node = LIST_FIRST(&sleep_queue); node != &sleep_queue; node = next_node) {
            pcb_t *node_pcb = LIST_ENTRY(node, pcb_t, list);
            next_node = node->next;
            if(node_pcb->wakeup_time <= current_time) {
                LIST_DELETE(node);
                node_pcb->status = TASK_READY;
                LIST_APPEND(node, &ready_queue);
            }
        }
    }
}