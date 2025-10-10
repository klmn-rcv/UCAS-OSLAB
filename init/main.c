#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50
#define APP_NUM_LOC 0x502001fa

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

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

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
                int input_valid = 1;
                int id = 0;
                for(int i = 0; i < input_p; i++) {
                    if('0' > input_buf[i] || '9' < input_buf[i]) {
                        input_valid = 0;
                        bios_putstr("Invalid input. Please input task ID...\n\r");
                        break;
                    }
                    id = id * 10 + (input_buf[i] - '0');
                    if(id < 0 || id > 0xffff) {
                        input_valid = 0;
                        bios_putstr("Invalid ID. Please input correct task ID...\n\r");
                        break;
                    }
                }
                if(input_valid) {
                    if(id > 0 && id <= *((uint16_t *)APP_NUM_LOC)) {
                        uint64_t entrypoint = load_task_img(id);
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
                        bios_putstr("ID out of range. Please input correct task ID...\n\r");
                    }
                    else {
                        bios_putstr("Please input task ID...\n\r");
                    }
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
