#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <printk.h>

uint64_t load_task_img(char *taskname, task_info_t tasks[], uint16_t tasknum)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    //// p1-task3:
    // uint64_t entrypoint = (taskid - 1) * 0x10000 + 0x52000000;
    // unsigned int block_id = 15 * taskid + 1;
    // bios_sd_read(entrypoint, 15, block_id);

    // p1-task4:
    task_info_t task_info;
    for(uint16_t i = 0; i < tasknum; i++) {
        if(strcmp(tasks[i].taskname, taskname) == 0) {
            task_info = tasks[i];
        }
    }
    
    uint64_t entrypoint = task_info.taskid * 0x10000 + 0x52000000;
    unsigned begin_sector = NBYTES2SEC(task_info.offset) - 1;
    unsigned end_sector = NBYTES2SEC(task_info.offset + task_info.filesz) - 1;
    unsigned num_sector = end_sector - begin_sector + 1;
    bios_sd_read((unsigned)entrypoint, num_sector, begin_sector);
    memcpy((uint8_t *)entrypoint, (uint8_t *)(entrypoint + (task_info.offset % SECTOR_SIZE)), task_info.filesz);

    return entrypoint;
}