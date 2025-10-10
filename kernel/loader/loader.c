#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    uint64_t entrypoint = (taskid - 1) * 0x10000 + 0x52000000;
    unsigned int block_id = 15 * taskid + 1;
    bios_sd_read(entrypoint, 15, block_id);

    return entrypoint;
}