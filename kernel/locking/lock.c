#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <atomic.h>
#include <printk.h>

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
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
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