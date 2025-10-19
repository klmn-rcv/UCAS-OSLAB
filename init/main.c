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

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];


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

    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(uint16_t tasknum, uint32_t task_info_offset)
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


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */


    /* TODO: [p2-task1] remember to initialize 'current_running' */

}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

void command_ls(uint16_t tasknum) {
    for(uint16_t i = 0; i < tasknum; i++) {
        bios_putstr(tasks[i].taskname);
        bios_putstr("\t");
    }
    bios_putstr("\n\r");
}

void command_batchwrite(int *strpos, char *argv, uint16_t tasknum, uint32_t task_info_offset) {
    char name_order[512];
    name_order[0] = '\0';
    char *name;
    for(uint16_t i = 1; i <= tasknum; i++) {
        name = argv + strpos[i];
        int task_exist = 0;
        for(uint16_t j = 0; j < tasknum; j++) {
            if(strcmp(tasks[j].taskname, name) == 0) {
                task_exist = 1;
                break;
            }
        }
        if(task_exist) {
            strcat(name_order, name);
            if(i != tasknum) strcat(name_order, " ");
        }
        else {
            bios_putstr("batchwrite: incorrect arguments\n\r");
            return;
        }
    }
    bios_sd_write((unsigned)name_order, 1, NBYTES2SEC(task_info_offset + sizeof(task_info_t) * tasknum));
}

void command_batchrun(uint16_t tasknum, uint32_t task_info_offset) {
    char name_in_order[512];
    char *name = name_in_order;
    bios_sd_read((unsigned)name_in_order, 1, NBYTES2SEC(task_info_offset + sizeof(task_info_t) * tasknum));
    int len_name_in_order = strlen(name_in_order);
    for(int i = 0; i < len_name_in_order; i++) {
        if(name_in_order[i] == ' ') {
            name_in_order[i] = '\0';
        }
    }

    int temp_result = 0;    // place to store temporary results of apps
    for(uint16_t i = 0; i < tasknum; i++) {
        uint64_t entrypoint = load_task_img(name, tasks, tasknum);
        asm volatile(
            "addi sp, sp, -24\n\t"
            "sd s1, 16(sp)\n\t"
            "sd ra, 8(sp)\n\t"
            "sd fp, 0(sp)\n\t"
            "add fp, sp, zero\n\t"
            "lw a0, 0(%1)\n\t"
            "add s1, %1, zero\n\t"
            "jalr ra, %0\n\t"
            "sw a0, 0(s1)\n\t"
            "ld fp, 0(sp)\n\t"
            "ld ra, 8(sp)\n\t"
            "ld s1, 16(sp)\n\t"
            "addi sp, sp, 24\n\t"
            : 
            : "r" (entrypoint), "r"(&temp_result)
            : "memory", "a0"
        );
        name = name + strlen(name) + 1;
    }
}

int check_command(char *input_buf, int input_p, uint16_t tasknum, uint32_t task_info_offset) {
    int argc = 0;
    int strpos[64];
    char argv[1024];    // not char *argv[] here, for convenience
    strcpy(argv, input_buf);

    int encounter_space = 1;
    for(int i = 0; i < input_p; i++) {
        if(argv[i] == ' ') {
            argv[i] = '\0';
            encounter_space = 1;
        }
        else if(encounter_space == 1) {
            encounter_space = 0;
            strpos[argc++] = i;
        }
    }

    if(strcmp(argv, "ls") == 0) {
        if(argc == 1) {
            command_ls(tasknum);
        }
        else {
            bios_putstr("ls: do not need any argument\n\r");
        }
        return 1;
    }
    else if(strcmp(argv, "batchwrite") == 0) {
        if(argc == ((int)tasknum + 1)) {
            int success;
            command_batchwrite(strpos, argv, tasknum, task_info_offset);
        }
        else {
            bios_putstr("batchwrite: incorrect arguments\n\r");
        }
        return 1;
    }
    else if(strcmp(argv, "batchrun") == 0) {
        if(argc == 1) {
            command_batchrun(tasknum, task_info_offset);
        }
        else {
            bios_putstr("batchrun: do not need any argument\n\r");
        }
        return 1;
    }
    return 0;
}

void load_and_execute_app(char *input_buf, int input_p, uint16_t tasknum) {
    int file_exist = 0;
    for(uint16_t i = 0; i < tasknum; i++) {
        if(strcmp(input_buf, tasks[i].taskname) == 0)
            file_exist = 1;
    }

    if(file_exist) {
        uint64_t entrypoint = load_task_img(input_buf, tasks, tasknum);
        asm volatile(
            "addi sp, sp, -16\n\t"
            "sd ra, 8(sp)\n\t"
            "sd fp, 0(sp)\n\t"
            "add fp, sp, zero\n\t"
            "jalr ra, %0\n\t"
            "ld fp, 0(sp)\n\t"
            "ld ra, 8(sp)\n\t"
            "addi sp, sp, 16\n\t"
            : 
            : "r" (entrypoint)
            : "memory"
        );
    }
    else if(input_p > 0) {
        bios_putstr("File or command does not exist\n\r");
    }
    else {
        bios_putstr("Please input file name or batch processing command...\n\r");
    }
}

int main(uint16_t tasknum, uint32_t task_info_offset)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info(tasknum, task_info_offset);

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
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
    


    char input_buf[1024];
    int input_p = 0;

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    while (1) {
        int ch = bios_getchar();
        if (ch != -1) {
            if(ch != '\r')
                bios_putchar(ch);
            else
                bios_putstr("\n\r");
            if (ch == '\n' || ch == '\r') {
                input_buf[input_p] = '\0';

                if(check_command(input_buf, input_p, tasknum, task_info_offset) == 0)
                    load_and_execute_app(input_buf, input_p, tasknum);
                
                input_p = 0;
                continue;
            }
            if (ch >= 32 && ch <= 126)
                input_buf[input_p++] = ch;
        }
    }


    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}
