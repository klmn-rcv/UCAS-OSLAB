#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <printk.h>
#include <os/mm.h>

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
    int success = 0;
    for(uint16_t i = 0; i < tasknum; i++) {
        if(strcmp(tasks[i].taskname, taskname) == 0) {
            task_info = tasks[i];
            success = 1;
        }
    }
    if(!success) return 0;
    
    uint64_t entrypoint = task_info.taskid * 0x10000 + 0x52000000;
    unsigned begin_sector = NBYTES2SEC(task_info.offset) - 1;
    unsigned end_sector = NBYTES2SEC(task_info.offset + task_info.filesz) - 1;
    unsigned num_sector = end_sector - begin_sector + 1;
    bios_sd_read_wrapper((unsigned)entrypoint, num_sector, begin_sector);
    memcpy((uint8_t *)entrypoint, (uint8_t *)(entrypoint + (task_info.offset % SECTOR_SIZE)), task_info.filesz);

    return entrypoint;
}

static void load_from_sd(uintptr_t dest_mem, uint32_t begin_off, uint32_t end_off) {
    unsigned long dest_mem_pa = kva2pa(dest_mem);
    if(begin_off != end_off) {
        unsigned begin_sector = NBYTES2SEC(begin_off) - 1;
        unsigned end_sector = NBYTES2SEC(end_off) - 1;
        unsigned num_sector = end_sector - begin_sector + 1;
        bios_sd_read_wrapper(dest_mem_pa, num_sector, begin_sector);
        memcpy((uint8_t *)dest_mem, (uint8_t *)(dest_mem + (begin_off % SECTOR_SIZE)), end_off - begin_off); 
    }
}

uint64_t map_and_load_task_img(char *taskname, pid_t pid, uintptr_t pgdir, task_info_t tasks[], uint16_t tasknum)
{
    task_info_t task_info;
    int success = 0;
    for(uint16_t i = 0; i < tasknum; i++) {
        if(strcmp(tasks[i].taskname, taskname) == 0) {
            task_info = tasks[i];
            success = 1;
            break;
        }
    }
    if(!success) return 0;

    // unsigned begin_sector = NBYTES2SEC(task_info.offset) - 1;
    // unsigned end_sector = NBYTES2SEC(task_info.offset + task_info.filesz) - 1;
    // unsigned num_sector = end_sector - begin_sector + 1;

    uint32_t begin_off = task_info.offset;
    uint32_t end_off;

    clear_pgdir(pgdir);
    share_pgtable(pgdir, pa2kva(PGDIR_PA));
    uintptr_t va = USER_ENTRYPOINT;
    uintptr_t va_file_end = USER_ENTRYPOINT + task_info.filesz;
    uintptr_t va_mem_end = USER_ENTRYPOINT + task_info.memsz;
    
    for(; va < va_mem_end; va += PAGE_SIZE) {
        // printl("loader.c: va: %lx\n", va);
        // PTE pte;
        int already_exist = 0;
        uintptr_t load_dest = alloc_page_helper(va, pid, pgdir, &already_exist);
        uint64_t copy_size = (va_mem_end - va > PAGE_SIZE) ? PAGE_SIZE : (va_mem_end - va);
        uint64_t remain_filesz = (va_file_end - va > 0) ? (va_file_end - va) : 0;
        
        uint64_t copy_file_size = (copy_size < remain_filesz) ? copy_size : remain_filesz;
        uint64_t set_zero_size = copy_size - copy_file_size;

        begin_off = task_info.offset + task_info.filesz - remain_filesz;
        end_off = begin_off + copy_file_size;

        load_from_sd(load_dest, begin_off, end_off);
        memset((uint8_t *)(load_dest + copy_file_size), 0, set_zero_size);
    }
    
    // bios_sd_read_wrapper((unsigned)load_dest, num_sector, begin_sector);
    // memcpy((uint8_t *)load_dest, (uint8_t *)(load_dest + (task_info.offset % SECTOR_SIZE)), task_info.filesz);
    // // bss清零
    // memset((uint8_t *)(load_dest + task_info.filesz), 0, task_info.memsz - task_info.filesz);

    return USER_ENTRYPOINT;
}