#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <os/task.h>

#define PAGE_SIZE 4096

extern char buf[2 * PAGE_SIZE];

// uint64_t load_task_img(int taskid);
// uint64_t load_task_img(char *taskname, task_info_t tasks[], uint16_t tasknum);
uint64_t map_and_load_task_img(char *taskname, pid_t pid, uintptr_t pgdir, task_info_t tasks[], uint16_t tasknum);

#endif