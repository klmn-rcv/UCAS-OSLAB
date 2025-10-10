#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
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
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(uint16_t tasknum, uint32_t task_info_offset)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info(tasknum, task_info_offset);

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    char input_buf[1024];
    int input_p = 0;

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    bios_putstr("Please input task ID...\n\r");

    while (1) {
        int ch = bios_getchar();
        if (ch != -1) {
            bios_putchar(ch);
            if (ch == '\n' || ch == '\r') {

                input_buf[input_p] = '\0';
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
                    bios_putstr("File does not exist. Please input correct file name...\n\r");
                }
                else {
                    bios_putstr("Please input file name...\n\r");
                }
                
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
        asm volatile("wfi");
    }

    return 0;
}
