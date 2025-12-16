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
#include <os/smp.h>
#include <os/ioremap.h>
#include <os/net.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <plic.h>

#define TASK_NUM 1

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];
static uint16_t tasknum;
static uint32_t task_info_offset;

uint32_t last_nonempty_sector;

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

    
    ////////////////////////////////////////////////////
    // task_info_offset是物理地址！！！！！
    ////////////////////////////////////////////////////

    uint32_t task_info_size = sizeof(task_info_t) * tasknum;
    unsigned begin_sector = NBYTES2SEC(task_info_offset) - 1;
    unsigned end_sector = NBYTES2SEC(task_info_offset + task_info_size) - 1;

    last_nonempty_sector = end_sector + 8; // 加8防止意外覆盖

    unsigned num_sector = end_sector - begin_sector + 1;
    uintptr_t mem_address = TASK_MEM_BASE + TASK_SIZE * tasknum;

    assert(num_sector <= 16);

    unsigned long buf_pa = kva2pa((uintptr_t)&buf);
    bios_sd_read(buf_pa, num_sector, begin_sector);
    memcpy((uint8_t *)mem_address, (uint8_t *)((unsigned long)&buf + (task_info_offset % SECTOR_SIZE)), task_info_size);
    memcpy((uint8_t *)tasks, (uint8_t *)mem_address, task_info_size);
}

/************************************************************/
void init_pcb_stack(
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

    pt_regs->sstatus = SR_SPIE | SR_SUM;
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
    // ptr_t entry_point = load_task_img(taskname, tasks, tasknum);
    // if(entry_point == 0) {  // task not found
    //     return 0;
    // }

    int pid = 0;
    for(int i = 2; i < NUM_MAX_TASK; i++) {
        if(pcb[i].status == TASK_EXITED) {
            pid = i;
            break;
        }
    }
    if(pid == 0) {
        assert(0);
    }

    ptr_t entry_point = map_and_load_task_img(taskname, pid, pcb[pid].pgdir, tasks, tasknum);
    if(entry_point == 0) { // task not found
        return 0;
    }
    
    // pcb[pid].kernel_sp = (reg_t)(pid * 2 * PAGE_SIZE + ROUND(FREEMEM_KERNEL, PAGE_SIZE));
    // pcb[pid].user_sp = (reg_t)(pid * 2 * PAGE_SIZE + ROUND(FREEMEM_USER, PAGE_SIZE));
    // pcb[pid].kernel_stack_base = (reg_t)(pid * 2 * PAGE_SIZE + ROUND(FREEMEM_KERNEL, PAGE_SIZE));
    // pcb[pid].user_stack_base = (reg_t)(pid * 2 * PAGE_SIZE + ROUND(FREEMEM_USER, PAGE_SIZE));

    pcb[pid].kernel_sp = allocKernelPage(KERNEL_STACK_PAGE_NUM, pid) + KERNEL_STACK_PAGE_NUM * PAGE_SIZE;

    pcb[pid].kernel_stack_start_page = pcb[pid].kernel_sp - KERNEL_STACK_PAGE_NUM * PAGE_SIZE;

    // pcb[pid].user_sp = allocUserPage(1) + PAGE_SIZE;
    pcb[pid].user_sp = USER_STACK_ADDR;

    for(int i = 1; i <= USER_STACK_PAGE_NUM; i++) {
        // printl("main.c: va is: %lx\n", USER_STACK_ADDR - i * PAGE_SIZE);
        // PTE pte;
        // printl("alloc_page_helper 1, va is: %lx\n", USER_STACK_ADDR - i * PAGE_SIZE);
        alloc_page_helper(USER_STACK_ADDR - i * PAGE_SIZE, pid, pcb[pid].pgdir);
    }

    LIST_INIT_HEAD(&pcb[pid].wait_list);
    pcb[pid].pid = pid;
    // pcb[pid].is_thread = 0;
    // pcb[pid].tid = -1;
    pcb[pid].cursor_x = 0;
    pcb[pid].cursor_y = 0;
    pcb[pid].wakeup_time = 0;
    pcb[pid].status = TASK_READY;
    //pcb[pid].run_core_mask = current_running->run_core_mask;
    pcb[pid].run_core_mask = 0x3;
    pcb[pid].killed = 0;
    
    // LIST_APPEND(&pcb[process_id].list, &ready_queue);
    init_pcb_stack(pcb[pid].kernel_sp, pcb[pid].user_sp, entry_point, &pcb[pid]);
    return pid;
}

static void init_pcb(/*uint16_t tasknum*/)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    pcb[0] = pid0_pcb;
    pcb[1] = pid1_pcb;
    pcb[0].status = TASK_RUNNING;
    pcb[1].status = TASK_RUNNING;
    current_running = &pcb[0];
    
    for(int i = 2; i < NUM_MAX_TASK; i++) {
        pcb[i].kernel_sp = 0;
        pcb[i].user_sp = 0;
        pcb[i].kernel_stack_start_page = 0;
        // pcb[i].kernel_stack_base = 0;
        // pcb[i].user_stack_base = 0;
        LIST_INIT_HEAD(&pcb[i].wait_list);
        pcb[i].pid = i;
        // pcb[i].is_thread = 0;
        // pcb[i].tid = -1;
        pcb[i].status = TASK_EXITED;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = 0;
        pcb[i].wakeup_time = 0;
        pcb[i].run_core_mask = 0;
        pcb[i].running_core_id = -1;
        // printl("Here!!!\n");
        pcb[i].pgdir = allocPage(i, NULL, 1);
        pcb[i].killed = 0;
    }

    char *tasknames[TASK_NUM] = {"shell"};
    
    for(int i = 0; i < TASK_NUM; i++) {
        int pid = create_task(tasknames[i]);
        assert(pid);
        LIST_APPEND(&pcb[pid].list, &ready_queue);
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
}


// static void init_tcb() {
//     for(int i = 0; i < NUM_MAX_TASK; i++) {
//         tcb[i].kernel_sp = 0;
//         tcb[i].user_sp = 0;
//         // tcb[i].kernel_stack_base = 0;
//         // tcb[i].user_stack_base = 0;
//         LIST_INIT_HEAD(&tcb[i].wait_list);
//         tcb[i].pid = 0;
//         tcb[i].is_thread = 1;
//         tcb[i].tid = i;
//         tcb[i].status = TASK_EXITED;
//         tcb[i].cursor_x = 0;
//         tcb[i].cursor_y = 0;
//         tcb[i].wakeup_time = 0;
//         tcb[i].run_core_mask = 0;
//         tcb[i].running_core_id = -1;
//     }
// }


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
    syscall[SYSCALL_BARR_INIT]         = (long (*)(long,long,long,long,long))do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]         = (long (*)(long,long,long,long,long))do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]      = (long (*)(long,long,long,long,long))do_barrier_destroy;
    syscall[SYSCALL_COND_INIT]         = (long (*)(long,long,long,long,long))do_condition_init;
    syscall[SYSCALL_COND_WAIT]         = (long (*)(long,long,long,long,long))do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL]       = (long (*)(long,long,long,long,long))do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST]    = (long (*)(long,long,long,long,long))do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY]      = (long (*)(long,long,long,long,long))do_condition_destroy;
    syscall[SYSCALL_MBOX_OPEN]         = (long (*)(long,long,long,long,long))do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]        = (long (*)(long,long,long,long,long))do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]         = (long (*)(long,long,long,long,long))do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]         = (long (*)(long,long,long,long,long))do_mbox_recv;

    syscall[SYSCALL_FREE_MEM]          = (long (*)(long,long,long,long,long))get_free_memory;
    syscall[SYSCALL_PIPE_OPEN]         = (long (*)(long,long,long,long,long))do_pipe_open;
    syscall[SYSCALL_PIPE_GIVE]         = (long (*)(long,long,long,long,long))do_pipe_give_pages;
    syscall[SYSCALL_PIPE_TAKE]         = (long (*)(long,long,long,long,long))do_pipe_take_pages;

    syscall[SYSCALL_TASKSET]           = (long (*)(long,long,long,long,long))do_taskset;
    syscall[SYSCALL_TASKSET_P]         = (long (*)(long,long,long,long,long))do_taskset_p;

    syscall[SYSCALL_NET_SEND]          = (long (*)(long,long,long,long,long))do_net_send;
    syscall[SYSCALL_NET_RECV]          = (long (*)(long,long,long,long,long))do_net_recv;
    syscall[SYSCALL_NET_RECV_STREAM]   = (long (*)(long,long,long,long,long))do_net_recv_stream;

    // syscall[SYSCALL_THREAD_CREATE]     = (long (*)(long,long,long,long,long))do_thread_create;
    // syscall[SYSCALL_THREAD_JOIN]       = (long (*)(long,long,long,long,long))do_thread_join;
    // syscall[SYSCALL_THREAD_EXIT]       = (long (*)(long,long,long,long,long))do_thread_exit;
}
/************************************************************/

/*
 * Once a CPU core calls this function,
 * it will stop executing!
 */
static void kernel_brake(void)
{
    disable_interrupt();
    while (1)
        __asm__ volatile("wfi");
}

static void delete_temp_map(void) {
    PTE *pgdir = (PTE *)pa2kva(PGDIR_PA);
    uint64_t vpn2_preserve = 0;
    for (uint64_t va = 0x50000000lu; va < 0x51000000lu; 
         va += 0x200000lu) {
        va &= VA_MASK;
        uint64_t vpn2 =
            va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                        (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        PTE *pmd = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
        pmd[vpn1] = 0;

        if(vpn2 != vpn2_preserve) {
            pgdir[vpn2_preserve] = 0;
        }
        vpn2_preserve = vpn2;
    }
    pgdir[vpn2_preserve] = 0;
    local_flush_tlb_all();
}

static void ioremap_init() {
    time_base = bios_read_fdt(TIMEBASE);
    e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR); // 从设备树中获取MAC内部寄存器的起始地址
    uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
    uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
    // printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);

    // IOremap
    plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
    e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
    // printk("> [INIT] IOremap initialization succeeded.\n");

    // TODO: [p5-task4] Init plic
    plic_init(plic_addr, nr_irqs);
    // printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);
}

int main(uint16_t tasknum_arg, uint32_t task_info_offset_arg)
{
    cpuid = get_current_cpu_id();

    if(cpuid == 0) {
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        tasknum = tasknum_arg;
        task_info_offset = task_info_offset_arg;

        // printk("tasknum is: %d\n", tasknum);
        // printk("task_info_offset is: %d\n", task_info_offset);

        init_task_info(/*tasknum, task_info_offset*/);

        init_pageframe_manager();

        ioremap_init();
        init_free_rtp_nodes();

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb(/*tasknum*/);
        // init_tcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // // Read CPU frequency (｡•ᴗ-)_
        // time_base = bios_read_fdt(TIMEBASE);


        // Init lock mechanism o(´^｀)o
        init_locks();
        init_barriers();
        init_conditions();
        init_mbox();
        init_pipe();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");


        // Init network device
        e1000_init();
        printk("> [INIT] E1000 device initialized successfully.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        // printl("Here 1, cpuid: %d\n", cpuid);


        // delete_temp_map();


        /*
         * Just start kernel with VM and print this string
         * in the first part of task 1 of project 4.
         * NOTE: if you use SMP, then every CPU core should call
         *  `kernel_brake()` to stop executing!
         */
        // screen_move_cursor(0, get_current_cpu_id() + 1);
        // printk("> [INIT] CPU #%u has entered kernel with VM!\n",
        // (unsigned int)get_current_cpu_id());
        // TODO: [p4-task1 cont.] remove the brake and continue to start user processes.

        wakeup_other_hart();

        // kernel_brake();
    }
    else {
        // printl("Here 2, cpuid: %d\n", cpuid);
        current_running = &pcb[1];
        current_running->status = TASK_RUNNING;

        // 因为从核也要用temp map，所以必须在从核里删
        // 但这样不兼容单核的情况！！
        delete_temp_map();
    
        /*
         * Just start kernel with VM and print this string
         * in the first part of task 1 of project 4.
         * NOTE: if you use SMP, then every CPU core should call
         *  `kernel_brake()` to stop executing!
         */
        // screen_move_cursor(0, get_current_cpu_id() + 1);
        // printk("> [INIT] CPU #%u has entered kernel with VM!\n",
        //     (unsigned int)get_current_cpu_id());
        // TODO: [p4-task1 cont.] remove the brake and continue to start user processes.
        // kernel_brake();
    }

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    setup_exception();
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
