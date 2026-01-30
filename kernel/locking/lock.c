#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <os/mm.h>
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
    // printl("Enter do_mbox_send...\n");
    mailbox_t *mbox = &mboxes[mbox_idx];

    clear_wait_queue(&mbox->recv_wait_queue);

    // lock
    while(mbox->write_lock.lock.status == LOCKED) {
        // printl("do_mbox_send blocked due to write_lock\n");
        do_block(&current_running->list, &mbox->write_lock.block_queue);
    }
    mbox->write_lock.using_pid = current_running->pid;
    mbox->write_lock.lock.status = LOCKED;

    clear_wait_queue(&mbox->recv_wait_queue);

    // clear_wait_queue(&mbox->recv_wait_queue);
    // clear_wait_queue(&mbox->send_wait_queue);

    int sent = 0;
    char *msg_str = (char *)msg;
    // int block_cnt = 0;

    for(int i = 0; i < msg_length; i++) {
        mbox->tail %= (MAX_MBOX_LENGTH + 1);
        while((mbox->tail + 1) % (MAX_MBOX_LENGTH + 1) == mbox->head % (MAX_MBOX_LENGTH + 1)) {
            // block_cnt++;
            LIST_APPEND(&current_running->list, &mbox->send_wait_queue);
            current_running->status = TASK_BLOCKED;
            // printl("do_mbox_send blocked due to fill box\n");
            do_scheduler();
        }
        mbox->message[mbox->tail++] = msg_str[i];
        sent++;
        clear_wait_queue(&mbox->recv_wait_queue);
    }


    mbox->tail %= (MAX_MBOX_LENGTH + 1);

    // unlock
    mbox->write_lock.using_pid = 0;
    mbox->write_lock.lock.status = UNLOCKED;
    if(!LIST_EMPTY(&mbox->write_lock.block_queue)) {
        list_node_t *first_blocked_node = LIST_FIRST(&mbox->write_lock.block_queue);
        do_unblock(first_blocked_node);
    }

    // printl("Exit do_mbox_send...\n");
    // return block_cnt;
    return sent;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length) {
    // printl("Enter do_mbox_recv...\n");
    mailbox_t *mbox = &mboxes[mbox_idx];

    clear_wait_queue(&mbox->send_wait_queue);

    // lock
    while(mbox->read_lock.lock.status == LOCKED) {
        // printl("do_mbox_recv blocked due to read_lock\n");
        do_block(&current_running->list, &mbox->read_lock.block_queue);
    }
    mbox->read_lock.using_pid = current_running->pid;
    mbox->read_lock.lock.status = LOCKED;

    clear_wait_queue(&mbox->send_wait_queue);

    // clear_wait_queue(&mbox->send_wait_queue);
    // clear_wait_queue(&mbox->recv_wait_queue);

    int recv = 0;
    char *msg_str = (char *)msg;
    // int block_cnt = 0;

    for(int i = 0; i < msg_length; i++) {
        mbox->head %= (MAX_MBOX_LENGTH + 1);
        while(mbox->tail % (MAX_MBOX_LENGTH + 1) == mbox->head % (MAX_MBOX_LENGTH + 1)) {
            // block_cnt++;
            LIST_APPEND(&current_running->list, &mbox->recv_wait_queue);
            current_running->status = TASK_BLOCKED;
            // printl("do_mbox_recv blocked due to empty box\n");
            do_scheduler();
        }
        msg_str[i] = mbox->message[mbox->head++];
        recv++;
        clear_wait_queue(&mbox->send_wait_queue);
    }

    mbox->head %= (MAX_MBOX_LENGTH + 1);

    // unlock
    mbox->read_lock.using_pid = 0;
    mbox->read_lock.lock.status = UNLOCKED;
    if(!LIST_EMPTY(&mbox->read_lock.block_queue)) {
        list_node_t *first_blocked_node = LIST_FIRST(&mbox->read_lock.block_queue);
        do_unblock(first_blocked_node);
    }

    // printl("Exit do_mbox_recv...\n");
    // return block_cnt;
    return recv;
}





pipe_t pipes[PIPE_NUM];

void init_pipe() {
    for(int i = 0; i < PIPE_NUM; i++) {
        pipes[i].in_use = 0;
        pipes[i].fill = 0;
        memset(pipes[i].name, 0, 32);
        pipes[i].pte = 0;
        pipes[i].unswapable = -1;
        LIST_INIT_HEAD(&pipes[i].give_wait_queue);
        LIST_INIT_HEAD(&pipes[i].take_wait_queue);
    }
}

int do_pipe_open(const char *name) {
    for(int i = 0; i < PIPE_NUM; i++) {
        if(strcmp(pipes[i].name, name) == 0) {
            pipes[i].in_use = 1;
            return i;
        }
    }

    for(int i = 0; i < PIPE_NUM; i++) {
        if(pipes[i].in_use == 0) {
            strcpy(pipes[i].name, name);
            pipes[i].in_use = 1;
            return i;
        }
    }

    return -1;
}

long do_pipe_give_pages(int pipe_idx, void *src, size_t length) {
    if (pipe_idx < 0 || pipe_idx >= PIPE_NUM || !pipes[pipe_idx].in_use) {
        return -1;
    }

    // printl("Enter giver...\n");

    pipe_t *pipe = &pipes[pipe_idx];

    clear_wait_queue(&pipe->take_wait_queue);

    uint64_t pages = length / PAGE_SIZE;
    uint64_t i;
    for(i = 0; i < pages; i++) {
        while(pipe->fill == 1) {
            LIST_APPEND(&current_running->list, &pipe->give_wait_queue);
            current_running->status = TASK_BLOCKED;
            // printl("giver blocked due to fill pipe\n");
            do_scheduler();
        }
        uintptr_t va = (uintptr_t)src + i * PAGE_SIZE;
        PTE *pte = va2pte(va, current_running->pgdir);
        assert(pte);
        assert(*pte);   // must not fail!!!

        if(*pte & _PAGE_PRESENT) {  // 如果有物理页框
            int id = (pa2kva(get_pa(*pte)) - FREEMEM_KERNEL) / PAGE_SIZE;
            // printl("give: id: %d, owner_pid: %d, pte_ptr: %lx, unswapable: %d\n", id, pageframes[id].owner_pid, pageframes[id].pte_ptr, pageframes[id].unswapable);
            assert(pageframes[id].alloc_record == 1);
            assert(pageframes[id].owner_pid == current_running->pid);
            pageframes[id].owner_pid = -1;
            assert(pipe->unswapable == -1);
            pipe->unswapable = pageframes[id].unswapable;
            pageframes[id].unswapable = 1;
            pageframes[id].pte_ptr = NULL;
            LIST_DELETE(&pageframes[id].list);
        }

        pipe->pte = *pte;
        *pte = 0;
        pipe->fill = 1;
        local_flush_tlb_page(va);
        clear_wait_queue(&pipe->take_wait_queue);
    }

    // printl("Exit giver...\n");
    return i * PAGE_SIZE;
}

long do_pipe_take_pages(int pipe_idx, void *dst, size_t length) {
    if (pipe_idx < 0 || pipe_idx >= PIPE_NUM || !pipes[pipe_idx].in_use) {
        return -1;
    }

    // printl("Enter taker...\n");

    pipe_t *pipe = &pipes[pipe_idx];

    clear_wait_queue(&pipe->give_wait_queue);

    uint64_t pages = length / PAGE_SIZE;
    uint64_t i;
    for(i = 0; i < pages; i++) {
        while(pipe->fill == 0) {
            LIST_APPEND(&current_running->list, &pipe->take_wait_queue);
            current_running->status = TASK_BLOCKED;
            // printl("taker blocked due to empty pipe\n");
            do_scheduler();
        }
        uintptr_t va = (uintptr_t)dst + i * PAGE_SIZE;
        PTE *pte = va2pte(va, current_running->pgdir);

        if(*pte != 0) {
            if(*pte & _PAGE_PRESENT) {
                int id = (pa2kva(get_pa(*pte)) - FREEMEM_KERNEL) / PAGE_SIZE;
                // printl("take: id: %d, owner_pid: %d, pte_ptr: %lx, unswapable: %d\n", id, pageframes[id].owner_pid, pageframes[id].pte_ptr, pageframes[id].unswapable);
                assert(pageframes[id].alloc_record == 1);
                assert(pageframes[id].owner_pid == current_running->pid);
                pageframes[id].alloc_record = 0;
                pageframes[id].owner_pid = -1;
                pageframes[id].unswapable = 0;
                // assert(pipe->unswapable != -1);
                // assert(pageframes[id].unswapable == 1);
                // pageframes[id].unswapable = pipe->unswapable;
                // pipe->unswapable = -1;
                // assert(pageframes[id].pte_ptr == NULL);
                // pageframes[id].pte_ptr = pipe->pte;
                LIST_DELETE(&pageframes[id].list);
            }
            else {
                uint64_t begin_sector = (get_pa(*pte) >> NORMAL_PAGE_SHIFT) - 1;
                free_sd_sector(begin_sector);
            }
        }

        *pte = pipe->pte;
        assert(pte);

        if(*pte & _PAGE_PRESENT) {  // 如果有物理页框
            int id = (pa2kva(get_pa(*pte)) - FREEMEM_KERNEL) / PAGE_SIZE;
            assert(pageframes[id].alloc_record == 1);
            assert(pageframes[id].owner_pid == -1);
            assert(pipe->unswapable != -1);
            assert(pageframes[id].unswapable == 1);
            assert(pageframes[id].pte_ptr == NULL);
            
            pageframes[id].owner_pid = current_running->pid;
            pageframes[id].unswapable = pipe->unswapable;
            pipe->unswapable = -1;
            pageframes[id].pte_ptr = pte;
            if(pageframes[id].unswapable == 0) {
                LIST_APPEND(&pageframes[id].list, &pageframe_queue);
            }
        }

        pipe->fill = 0;
        local_flush_tlb_page(va);
        clear_wait_queue(&pipe->give_wait_queue);
    }

    // printl("Exit taker...\n");
    return i * PAGE_SIZE;
}