#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <atomic.h>
#include <printk.h>
#include <assert.h>

spin_lock_t kernel_lock;
extern void clear_wait_queue(list_head *queue);

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++) {
        mlocks[i].lock.status = UNLOCKED;
        LIST_INIT_HEAD(&mlocks[i].block_queue);
        mlocks[i].key = -1;
        mlocks[i].pnum = 0;
        memset(mlocks[i].pid, 0, sizeof(mlocks[i].pid));
        mlocks[i].using_pid = 0;
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while(atomic_swap((uint32_t)LOCKED, (ptr_t)&lock->status))
        ;
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    for (int i = 0; i < LOCK_NUM; i++) {
        if (mlocks[i].key == key) {
            if(mlocks[i].pid[current_running->pid] == 0) {
                mlocks[i].pid[current_running->pid] = 1;
                mlocks[i].pnum++;
            }
            return i;
        }
    }

    for (int i = 0; i < LOCK_NUM; i++) {
        if (mlocks[i].key == -1) {
            mlocks[i].key = key;
            mlocks[i].pid[current_running->pid] = 1;
            mlocks[i].pnum = 1;
            return i;
        }
    }
    
    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    while(mlocks[mlock_idx].lock.status == LOCKED) {
        do_block(&current_running->list, &mlocks[mlock_idx].block_queue);
    }
    mlocks[mlock_idx].using_pid = current_running->pid;
    mlocks[mlock_idx].lock.status = LOCKED;
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    mlocks[mlock_idx].using_pid = 0;
    mlocks[mlock_idx].lock.status = UNLOCKED;
    if(!LIST_EMPTY(&mlocks[mlock_idx].block_queue)) {
        list_node_t *first_blocked_node = LIST_FIRST(&mlocks[mlock_idx].block_queue);
        do_unblock(first_blocked_node);
    }
}

void do_mutex_lock_free(pid_t pid) {
    for(int i = 0; i < LOCK_NUM; i++) {
        if(mlocks[i].using_pid == pid) {
            do_mutex_lock_release(i);
        }
    }
    for(int i = 0; i < LOCK_NUM; i++) {
        if(mlocks[i].pid[pid]) {
            mlocks[i].pnum--;
            mlocks[i].pid[pid] = 0;
        }
        if(mlocks[i].pnum == 0) {
            mlocks[i].key = -1;
        }
    }
}

barrier_t bars[BARRIER_NUM];

void init_barriers() {
    for(int i = 0; i < BARRIER_NUM; i++) {
        bars[i].key = -1;
        bars[i].total_count = 0;
        bars[i].current_count = 0;
        LIST_INIT_HEAD(&bars[i].wait_queue);
    }
}

int do_barrier_init(int key, int goal) {
    for(int i = 0; i < BARRIER_NUM; i++) {
        if(bars[i].key == key) {
            bars[i].total_count = goal;
            return i;
        }
    }

    for(int i = 0; i < BARRIER_NUM; i++) {
        if(bars[i].key == -1) {
            bars[i].key = key;
            bars[i].total_count = goal;
            return i;
        }
    }

    return -1;
}

void do_barrier_wait(int bar_idx) {
    bars[bar_idx].current_count++;
    if(bars[bar_idx].current_count < bars[bar_idx].total_count) {
        LIST_APPEND(&current_running->list, &bars[bar_idx].wait_queue);
        current_running->status = TASK_BLOCKED;
        do_scheduler();
    }
    else {
        if(!LIST_EMPTY(&bars[bar_idx].wait_queue)) {
            // list_node_t *node, *next_node;
            // for(node = LIST_FIRST(&bars[bar_idx].wait_queue); node != &bars[bar_idx].wait_queue; node = next_node) {
            //     next_node = node->next;
            //     pcb_t *node_pcb = LIST_ENTRY(node, pcb_t, list);
            //     LIST_DELETE(node);
            //     LIST_APPEND(node, &ready_queue);
            //     node_pcb->status = TASK_READY;
            // }
            clear_wait_queue(&bars[bar_idx].wait_queue);
            bars[bar_idx].current_count = 0;
        }
    }
}

void do_barrier_destroy(int bar_idx) {
    bars[bar_idx].key = -1;
    bars[bar_idx].current_count = 0;
    bars[bar_idx].total_count = 0;
    //if(!LIST_EMPTY(&bars[bar_idx].wait_queue)) {
        // list_node_t *node, *next_node;
        // for(node = LIST_FIRST(&bars[bar_idx].wait_queue); node != &bars[bar_idx].wait_queue; node = next_node) {
        //     next_node = node->next;
        //     pcb_t *node_pcb = LIST_ENTRY(node, pcb_t, list);
        //     LIST_DELETE(node);
        //     LIST_APPEND(node, &ready_queue);
        //     node_pcb->status = TASK_READY;
        // }
    clear_wait_queue(&bars[bar_idx].wait_queue);
    //}
}

condition_t conds[CONDITION_NUM];

void init_conditions() {
    for(int i = 0; i < CONDITION_NUM; i++) {
        conds[i].key = -1;
        LIST_INIT_HEAD(&conds[i].wait_queue);
    }
}

int do_condition_init(int key) {
    for(int i = 0; i < CONDITION_NUM; i++) {
        if(conds[i].key == key) {
            return i;
        }
    }

    for(int i = 0; i < CONDITION_NUM; i++) {
        if(conds[i].key == -1) {
            conds[i].key = key;
            return i;
        }
    }

    return -1;
}

void do_condition_wait(int cond_idx, int mutex_idx) {
    LIST_APPEND(&current_running->list, &conds[cond_idx].wait_queue);
    current_running->status = TASK_BLOCKED;
    do_mutex_lock_release(mutex_idx);
    do_scheduler();
    do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx) {
    if(!LIST_EMPTY(&conds[cond_idx].wait_queue)) {
        list_node_t *wakeup_node = LIST_FIRST(&conds[cond_idx].wait_queue);
        LIST_DELETE(wakeup_node);
        LIST_APPEND(wakeup_node, &ready_queue);
    }
}

void do_condition_broadcast(int cond_idx) {
    //if(!LIST_EMPTY(&conds[cond_idx].wait_queue)) {
        // list_node_t *wakeup_node, *next_node;
        // for(wakeup_node = LIST_FIRST(&conds[cond_idx].wait_queue); wakeup_node != &conds[cond_idx].wait_queue; wakeup_node = next_node) {
        //     next_node = wakeup_node->next;
        //     pcb_t *wakeup_node_pcb = LIST_ENTRY(wakeup_node, pcb_t, list);
        //     LIST_DELETE(wakeup_node);
        //     LIST_APPEND(wakeup_node, &ready_queue);
        //     wakeup_node_pcb->status = TASK_READY;
        // }
    clear_wait_queue(&conds[cond_idx].wait_queue);
    //}
    assert(LIST_EMPTY(&conds[cond_idx].wait_queue));
}

void do_condition_destroy(int cond_idx) {
    // do_condition_broadcast(cond_idx);
    assert(LIST_EMPTY(&conds[cond_idx].wait_queue));
    conds[cond_idx].key = -1;
}

mailbox_t mboxes[MBOX_NUM];

void init_mbox() {
    for(int i = 0; i < MBOX_NUM; i++) {
        mboxes[i].using_num = 0;
        mboxes[i].head = 0;
        mboxes[i].tail = 0;
        memset(mboxes[i].name, 0, 32);
        memset(mboxes[i].message, 0, MAX_MBOX_LENGTH + 1);
        LIST_INIT_HEAD(&mboxes[i].send_wait_queue);
        LIST_INIT_HEAD(&mboxes[i].recv_wait_queue);
        
        mboxes[i].write_lock.lock.status = UNLOCKED;
        LIST_INIT_HEAD(&mboxes[i].write_lock.block_queue);
        mboxes[i].write_lock.key = -1;
        mboxes[i].write_lock.pnum = 0;
        memset(mboxes[i].write_lock.pid, 0, sizeof(mboxes[i].write_lock.pid));
        mboxes[i].write_lock.using_pid = 0;

        mboxes[i].read_lock.lock.status = UNLOCKED;
        LIST_INIT_HEAD(&mboxes[i].read_lock.block_queue);
        mboxes[i].read_lock.key = -1;
        mboxes[i].read_lock.pnum = 0;
        memset(mboxes[i].read_lock.pid, 0, sizeof(mboxes[i].read_lock.pid));
        mboxes[i].read_lock.using_pid = 0;
    }
}

int do_mbox_open(char *name) {
    for(int i = 0; i < MBOX_NUM; i++) {
        if(strcmp(mboxes[i].name, name) == 0) {
            mboxes[i].using_num++;
            return i;
        }
    }

    for(int i = 0; i < MBOX_NUM; i++) {
        if(mboxes[i].using_num == 0) {
            strcpy(mboxes[i].name, name);
            mboxes[i].using_num = 1;
            return i;
        }
    }

    return -1;
}

void do_mbox_close(int mbox_idx) {
    mboxes[mbox_idx].using_num--;
    if(mboxes[mbox_idx].using_num == 0) {
        mboxes[mbox_idx].head = 0;
        mboxes[mbox_idx].tail = 0;
        memset(mboxes[mbox_idx].name, 0, 32);
        memset(mboxes[mbox_idx].message, 0, MAX_MBOX_LENGTH + 1);
        clear_wait_queue(&mboxes[mbox_idx].send_wait_queue);
        clear_wait_queue(&mboxes[mbox_idx].recv_wait_queue);
        clear_wait_queue(&mboxes[mbox_idx].write_lock.block_queue);
        clear_wait_queue(&mboxes[mbox_idx].read_lock.block_queue);
        mboxes[mbox_idx].write_lock.lock.status = UNLOCKED;
        mboxes[mbox_idx].write_lock.using_pid = 0;
        mboxes[mbox_idx].read_lock.lock.status = UNLOCKED;
        mboxes[mbox_idx].read_lock.using_pid = 0;
    }
}

int do_mbox_send(int mbox_idx, void * msg, int msg_length) {
    mailbox_t *mbox = &mboxes[mbox_idx];

    clear_wait_queue(&mbox->recv_wait_queue);

    // lock
    while(mbox->write_lock.lock.status == LOCKED) {
        do_block(&current_running->list, &mbox->write_lock.block_queue);
    }
    mbox->write_lock.using_pid = current_running->pid;
    mbox->write_lock.lock.status = LOCKED;

    // clear_wait_queue(&mbox->recv_wait_queue);
    // clear_wait_queue(&mbox->send_wait_queue);

    char *msg_str = (char *)msg;
    int block_cnt = 0;

    for(int i = 0; i < msg_length; i++) {
        mbox->tail %= (MAX_MBOX_LENGTH + 1);
        while((mbox->tail + 1) % (MAX_MBOX_LENGTH + 1) == mbox->head % (MAX_MBOX_LENGTH + 1)) {
            block_cnt++;
            LIST_APPEND(&current_running->list, &mbox->send_wait_queue);
            current_running->status = TASK_BLOCKED;
            do_scheduler();
        }
        mbox->message[mbox->tail++] = msg_str[i];
    }


    mbox->tail %= (MAX_MBOX_LENGTH + 1);

    // unlock
    mbox->write_lock.using_pid = 0;
    mbox->write_lock.lock.status = UNLOCKED;
    if(!LIST_EMPTY(&mbox->write_lock.block_queue)) {
        list_node_t *first_blocked_node = LIST_FIRST(&mbox->write_lock.block_queue);
        do_unblock(first_blocked_node);
    }


    return block_cnt;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length) {
    mailbox_t *mbox = &mboxes[mbox_idx];

    clear_wait_queue(&mbox->send_wait_queue);

    // lock
    while(mbox->read_lock.lock.status == LOCKED) {
        do_block(&current_running->list, &mbox->read_lock.block_queue);
    }
    mbox->read_lock.using_pid = current_running->pid;
    mbox->read_lock.lock.status = LOCKED;

    // clear_wait_queue(&mbox->send_wait_queue);
    // clear_wait_queue(&mbox->recv_wait_queue);

    char *msg_str = (char *)msg;
    int block_cnt = 0;

    for(int i = 0; i < msg_length; i++) {
        mbox->head %= (MAX_MBOX_LENGTH + 1);
        while(mbox->tail % (MAX_MBOX_LENGTH + 1) == mbox->head % (MAX_MBOX_LENGTH + 1)) {
            block_cnt++;
            LIST_APPEND(&current_running->list, &mbox->recv_wait_queue);
            current_running->status = TASK_BLOCKED;
            do_scheduler();
        }
        msg_str[i] = mbox->message[mbox->head++];
    }

    mbox->head %= (MAX_MBOX_LENGTH + 1);

    // unlock
    mbox->read_lock.using_pid = 0;
    mbox->read_lock.lock.status = UNLOCKED;
    if(!LIST_EMPTY(&mbox->read_lock.block_queue)) {
        list_node_t *first_blocked_node = LIST_FIRST(&mbox->read_lock.block_queue);
        do_unblock(first_blocked_node);
    }

    return block_cnt;
}