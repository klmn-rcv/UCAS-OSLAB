/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <pgtable.h>
#include <os/list.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK+2*PAGE_SIZE) // 前两页分别作为了主核和从核的kernel stack
#define FREEMEM_KERNEL_END 0xffffffc060000000
// #define PAGE_MAX_NUM ((FREEMEM_KERNEL_END - FREEMEM_KERNEL) / PAGE_SIZE)
#define PAGE_MAX_NUM 500

#define SD_SWAP_SECTOR_NUM 80000

#define KERNEL_STACK_PAGE_NUM 2
#define USER_STACK_PAGE_NUM 20

////////////////////////////////////////////////////////////////
// #define INIT_USER_STACK 0x52500000      // 要改！！！！！
// #define FREEMEM_USER INIT_USER_STACK    // 要改！！！！！
////////////////////////////////////////////////////////////////

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

typedef struct {
    list_node_t list;       /* 链表节点，用于管理页框 */
    PTE *pte_ptr;           /* 指向对应 PTE 的内核地址（如果存在） */
    pid_t owner_pid;        /* 页所属进程的 PID */
    uint16_t id;            /* 页框编号 */
    int8_t alloc_record;    /* 页框分配记录，0 表示空闲，1 表示已分配 */
    int8_t unswapable;      /* 页框被用作内核栈或页表，不参与换入换出机制 */
} pageframe_t;

extern pageframe_t pageframes[PAGE_MAX_NUM];
extern list_head pageframe_queue;

int getPageId(uintptr_t kva);
extern void init_pageframe_manager();

ptr_t allocKernelPage(int numPage, pid_t pid);
void freeKernelPage(uintptr_t start_page, int numPage);

extern ptr_t allocPage(pid_t pid, PTE *pte_ptr, int unswapable);
// extern ptr_t allocUserPage(int numPage);
// TODO [P4-task1] */
void freePage(ptr_t baseAddr);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

// TODO [P4-task1] */
extern void* kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, pid_t pid, uintptr_t pgdir, /*PTE *pte, */int *already_exist);
void swap_out(pageframe_t *pageframe);
void swap_in(PTE *pte_ptr, pid_t pid, int unswapable);
int alloc_sd_sector(void);
void free_sd_sector(int begin);
uintptr_t va2kva(uintptr_t va, uintptr_t pgdir, pid_t pid, int *success);
PTE *va2pte(uintptr_t va, uintptr_t pgdir);
size_t get_free_memory(void);

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);



#endif /* MM_H */
