#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <os/task.h>

// uint64_t load_task_img(int taskid);
uint64_t load_task_img(char *taskname, task_info_t tasks[], uint16_t tasknum);

#endif