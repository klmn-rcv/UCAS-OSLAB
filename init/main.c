#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

#define TASK_NUM 1

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];
static uint16_t tasknum;
static uint32_t task_info_offset;

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    jmptab[REFLUSH]         = (long (*)())screen_reflush;

    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(/*uint16_t tasknum, uint32_t task_info_offset*/)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    uint32_t task_info_size = sizeof(task_info_t) * tasknum;
    unsigned begin_sector = NBYTES2SEC(task_info_offset) - 1;
    unsigned end_sector = NBYTES2SEC(task_info_offset + task_info_size) - 1;
    unsigned num_sector = end_sector - begin_sector + 1;
    unsigned mem_address = TASK_MEM_BASE + TASK_SIZE * tasknum;
    bios_sd_read(mem_address, num_sector, begin_sector);
    memcpy((uint8_t *)mem_address, (uint8_t *)(mem_address + (task_info_offset % SECTOR_SIZE)), task_info_size);
    memcpy((uint8_t *)tasks, (uint8_t *)mem_address, task_info_size);
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    for(int i = 0; i < 32; i++) {
        pt_regs->regs[i] = 0;
    }

    // pt_regs->regs[1] = 0;                       // ra
    pt_regs->regs[2] = (reg_t)user_stack;       // sp
    pt_regs->regs[4] = (reg_t)pcb;              // tp

    pt_regs->sstatus = SR_SIE;
    pt_regs->sepc = (reg_t)entry_point;
    pt_regs->stval = 0;
    pt_regs->scause = 0;


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

    pt_switchto->regs[0] = (reg_t)ret_from_exception;  // ra
    pt_switchto->regs[1] = (reg_t)pt_switchto;   // sp

    for (int i = 2; i < 14; i++) {
        pt_switchto->regs[i] = 0;
    }

    pcb->kernel_sp = (reg_t)pt_switchto;

}

int create_task(char *taskname) {
    ptr_t entry_point = load_task_img(taskname, tasks, tasknum);
    if(entry_point == 0) {  // task not found
        return 0;
    }

    int pid = 0;
    for(int i = 1; i < NUM_MAX_TASK; i++) {
        if(pcb[i].status == TASK_EXITED) {
            pid = i;
            break;
        }
    }
    if(pid == 0) {
        return 0;
    }
    
    pcb[pid].kernel_sp = (reg_t)(pid * 3 * PAGE_SIZE + ROUND(FREEMEM_KERNEL, PAGE_SIZE));
    pcb[pid].user_sp = (reg_t)(pid * 3 * PAGE_SIZE + ROUND(FREEMEM_USER, PAGE_SIZE));
    pcb[pid].kernel_stack_base = (reg_t)(pid + ROUND(FREEMEM_KERNEL, PAGE_SIZE));
    pcb[pid].user_stack_base = (reg_t)(pid + ROUND(FREEMEM_USER, PAGE_SIZE));
    LIST_INIT_HEAD(&pcb[pid].wait_list);
    pcb[pid].pid = pid;
    pcb[pid].cursor_x = 0;
    pcb[pid].cursor_y = 0;
    pcb[pid].wakeup_time = 0;
    pcb[pid].status = TASK_READY;
    // LIST_APPEND(&pcb[process_id].list, &ready_queue);
    init_pcb_stack(pcb[pid].kernel_sp, pcb[pid].user_sp, entry_point, &pcb[pid]);
    return pid;
}

static void init_pcb(/*uint16_t tasknum*/)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    pcb[0] = pid0_pcb;
    
    for(int i = 1; i < NUM_MAX_TASK; i++) {
        pcb[i].kernel_sp = 0;
        pcb[i].user_sp = 0;
        pcb[i].kernel_stack_base = 0;
        pcb[i].user_stack_base = 0;
        LIST_INIT_HEAD(&pcb[i].wait_list);
        pcb[i].pid = i;
        pcb[i].status = TASK_EXITED;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = 0;
        pcb[i].wakeup_time = 0;
    }

    char *tasknames[TASK_NUM] = {"shell"};
    
    for(int i = 0; i < TASK_NUM; i++) {
        int pid = create_task(tasknames[i]);
        assert(pid);
        LIST_APPEND(&pcb[pid].list, &ready_queue);
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    pcb[0].status = TASK_RUNNING;
    current_running = &pcb[0];
    asm volatile("mv tp, %0" : : "r"(current_running));
}


static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_EXEC]              = (long (*)(long,long,long,long,long))do_exec;
    syscall[SYSCALL_EXIT]              = (long (*)(long,long,long,long,long))do_exit;
    syscall[SYSCALL_SLEEP]             = (long (*)(long,long,long,long,long))do_sleep;
    syscall[SYSCALL_KILL]              = (long (*)(long,long,long,long,long))do_kill;
    syscall[SYSCALL_WAITPID]           = (long (*)(long,long,long,long,long))do_waitpid;
    syscall[SYSCALL_PS]                = (long (*)(long,long,long,long,long))do_process_show;
    syscall[SYSCALL_GETPID]            = (long (*)(long,long,long,long,long))do_getpid;
    syscall[SYSCALL_YIELD]             = (long (*)(long,long,long,long,long))do_scheduler;
    syscall[SYSCALL_WRITE]             = (long (*)(long,long,long,long,long))screen_write;
    syscall[SYSCALL_READCH]            = (long (*)(long,long,long,long,long))bios_getchar;
    syscall[SYSCALL_CURSOR]            = (long (*)(long,long,long,long,long))screen_move_cursor;
    syscall[SYSCALL_REFLUSH]           = (long (*)(long,long,long,long,long))screen_reflush;
    syscall[SYSCALL_CLEAR]             = (long (*)(long,long,long,long,long))screen_clear;
    syscall[SYSCALL_GET_TIMEBASE]      = (long (*)(long,long,long,long,long))get_time_base;
    syscall[SYSCALL_GET_TICK]          = (long (*)(long,long,long,long,long))get_ticks;
    syscall[SYSCALL_LOCK_INIT]         = (long (*)(long,long,long,long,long))do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]          = (long (*)(long,long,long,long,long))do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]      = (long (*)(long,long,long,long,long))do_mutex_lock_release;
}
/************************************************************/



int main(uint16_t tasknum_arg, uint32_t task_info_offset_arg)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    tasknum = tasknum_arg;
    task_info_offset = task_info_offset_arg;
    init_task_info(/*tasknum, task_info_offset*/);

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb(/*tasknum*/);
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    uint64_t current_time = get_ticks();
    bios_set_timer(current_time + TIMER_INTERVAL);

    // // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
