#include <os/mm.h>
#include <os/string.h>
#include <assert.h>
#include <os/sched.h>
#include <os/kernel.h>
#include <os/smp.h>

extern uint32_t last_nonempty_sector;



pageframe_t pageframes[PAGE_MAX_NUM];

uint8_t sd_record[SD_SWAP_SECTOR_NUM];

list_head pageframe_queue;

// int allocated_pageframe_count = 0;

// int page_total_num = PAGE_MAX_NUM;

int alloc_sd_sector(void) { // 返回分配的8个sector中第一个的下标
    // for(int i = begin; i < begin + PAGE_SIZE / SECTOR_SIZE; i++) {
    //     assert(sd_record[i] == 0);
    //     sd_record[i] = 1;
    // }


    int start = -1;
    int count = 0;
    
    // 寻找连续的8个空闲sector
    for (int i = last_nonempty_sector; i < SD_SWAP_SECTOR_NUM; i++) {
        if (sd_record[i] == 0) {
            if (count == 0) 
                start = i;
            count++;
            if (count == PAGE_SIZE / SECTOR_SIZE) {
                // 标记为已分配
                for (int j = start; j < start + PAGE_SIZE / SECTOR_SIZE; j++) {
                    sd_record[j] = 1;
                }
                // ptr_t page_addr = FREEMEM_KERNEL + start * PAGE_SIZE;
                // memset((uint8_t *)page_addr, 0, numPage * PAGE_SIZE);
                return start;
            }
        } 
        else {
            count = 0;
            start = -1;
        }
    }

    assert(0);
    return 0;  // 分配失败
}

void free_sd_sector(int begin) {
    for(int i = begin; i < begin + PAGE_SIZE / SECTOR_SIZE; i++) {
        // printl("free_sd_sector: free sector %d\n", i);
        assert(i >= 0 && i < SD_SWAP_SECTOR_NUM);
        assert(sd_record[i] == 1);
        sd_record[i] = 0;
    }
}

inline int getPageId(uintptr_t kva) {
    return (kva - FREEMEM_KERNEL) / PAGE_SIZE;
}

void init_pageframe_manager() {
    LIST_INIT_HEAD(&pageframe_queue);
    for (int i = 0; i < PAGE_MAX_NUM; i++) {
        pageframes[i].id = i;
        pageframes[i].alloc_record = 0;
        pageframes[i].owner_pid = -1;
        pageframes[i].pte_ptr = NULL;
        pageframes[i].unswapable = 0;
        LIST_INIT_HEAD(&pageframes[i].list);
    }
}

void enqueue_pageframe(pageframe_t *pageframe) {
    // printl("Enter enqueue_pageframe...\n");
    assert(pageframe);
    assert(0 <= pageframe->id && pageframe->id < PAGE_MAX_NUM);
    assert(pageframe->alloc_record == 0);
    assert(pageframe->unswapable == 0);

    LIST_APPEND(&pageframe->list, &pageframe_queue);
    // pageframe->alloc_record = 1;
    // allocated_pageframe_count++;
    // printl("Exit enqueue_pageframe...\n");
}

pageframe_t *dequeue_pageframe() {
    if (!LIST_EMPTY(&pageframe_queue)) {

        list_node_t *node, *next_node;
        pageframe_t *pageframe = NULL;

        long loop_count = 0;
        while(1) {
            for(node = LIST_FIRST(&pageframe_queue); node != &pageframe_queue; node = next_node) {
                next_node = node->next;
                pageframe_t *pageframe_now = LIST_ENTRY(node, pageframe_t, list);
    
                assert(pageframe_now);
                assert(0 <= pageframe_now->id && pageframe_now->id < PAGE_MAX_NUM);
                assert(pageframe_now->alloc_record == 1);
                assert(pageframe_now->unswapable == 0);
    
                if((pcb[pageframe_now->owner_pid].status != TASK_RUNNING) || (pcb[pageframe_now->owner_pid].running_core_id == cpuid)) {
                    pageframe = pageframe_now;
                    break;
                }
            }

            if(pageframe != NULL)
                break;

            // loop_count++;
            // if(loop_count % 1000 == 0) {
            //     printl("%ld\n", loop_count);
            // }

            unlock_kernel();
            lock_kernel();
        }

        assert(pageframe);

        // pageframe_t *pageframe = LIST_ENTRY(LIST_FIRST(&pageframe_queue), pageframe_t, list);

        // printl("dequeue_pageframe: pageframe->id: %d\n");
        
        // assert(pageframe);
        // assert(0 <= pageframe->id && pageframe->id < PAGE_MAX_NUM);
        // assert(pageframe->alloc_record == 1);
        // assert(pageframe->unswapable == 0);

        swap_out(pageframe);
        // LIST_DELETE(&pageframe->list);
        // pageframe->alloc_record = 0;
        // pageframe->owner_pid = -1;
        // allocated_pageframe_count--;
        
        // printl("Exit dequeue_pageframe...\n");
        return pageframe;
    }

    assert(0);
    return NULL;
}

// NOTE: A/C-core
// static ptr_t kernMemCurr = FREEMEM_KERNEL;

// int8_t alloc_record[PAGE_MAX_NUM];

// 分配物理页框
// ptr_t allocPage(int numPage)
// {
//     // align PAGE_SIZE
//     // ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
//     // kernMemCurr = ret + numPage * PAGE_SIZE;
//     // return ret;
//     for(int i = 0; i < PAGE_MAX_NUM; i++) {
//         if(alloc_record[i] == 0) {
//             alloc_record[i] = 1;
//             return FREEMEM_KERNEL + i * PAGE_SIZE;
//         }
//     }
// }


// 因为内核栈需要连续的空间，并且内核映射已经建立，因此需要单独实现分配函数
ptr_t allocKernelPage(int numPage, pid_t pid) {
    int start = -1;
    int count = 0;
    
    // 寻找连续的空闲页
    for (int i = 0; i < PAGE_MAX_NUM; i++) {
        if (pageframes[i].alloc_record == 0) {
            if (count == 0) 
                start = i;
            count++;
            if (count == numPage) {
                // 标记为已分配
                for (int j = start; j < start + numPage; j++) {
                    pageframes[j].alloc_record = 1;
                    pageframes[j].owner_pid = pid;
                    pageframes[j].pte_ptr = NULL;   // 不需要PTE信息（不是没有）
                    pageframes[j].unswapable = 1;
                    // allocated_pageframe_count++;
                    // page_total_num--;
                }
                ptr_t page_addr = FREEMEM_KERNEL + start * PAGE_SIZE;
                memset((uint8_t *)page_addr, 0, numPage * PAGE_SIZE);

                // printl("Exit allocKernelPage 1, page_addr is %lx\n", page_addr);

                return page_addr;
            }
        }
        else {
            count = 0;
            start = -1;
        }
    }

    for(int i = 0; i < PAGE_MAX_NUM; i++) {
        if((pageframes[i].alloc_record == 0) || 
           (pageframes[i].alloc_record == 1 && pageframes[i].unswapable == 0 && ((pcb[pageframes[i].owner_pid].status != TASK_RUNNING) || (pcb[pageframes[i].owner_pid].running_core_id == cpuid)))) {
            if (count == 0) {
                start = i;
            }
            count++;
            if (count == numPage) {
                for (int j = start; j < start + numPage; j++) {
                    if(pageframes[j].alloc_record == 0) {
                        pageframes[j].alloc_record = 1;
                        pageframes[j].owner_pid = pid;
                        pageframes[j].pte_ptr = NULL;   // 不需要PTE信息（不是没有）
                        pageframes[j].unswapable = 1;
                    }
                    else  {
                        assert(pageframes[j].unswapable == 0);
                        assert((pcb[pageframes[j].owner_pid].status != TASK_RUNNING) || (pcb[pageframes[j].owner_pid].running_core_id == cpuid));
                        swap_out(&pageframes[j]);
                        pageframes[j].alloc_record = 1;
                        pageframes[j].owner_pid = pid;
                        pageframes[j].pte_ptr = NULL;   // 不需要PTE信息（不是没有）
                        pageframes[j].unswapable = 1;
                    }
                }
                ptr_t page_addr = FREEMEM_KERNEL + start * PAGE_SIZE;
                memset((uint8_t *)page_addr, 0, numPage * PAGE_SIZE);

                // printl("Exit allocKernelPage 2, page_addr is %lx\n", page_addr);
                return page_addr;
            }
        }
        else {
            count = 0;
            start = -1;
        }
    }

    assert(0);
    return 0;  // 分配失败
}

void freeKernelPage(uintptr_t start_page, int numPage) {
    int id = getPageId(start_page);
    assert(id >= 0 && id < PAGE_MAX_NUM);
    for (int i = id; i < id + numPage; i++) {
        assert(pageframes[i].unswapable == 1);
        pageframes[i].alloc_record = 0;
        pageframes[i].owner_pid = -1;
        pageframes[i].pte_ptr = NULL;
        pageframes[i].unswapable = 0;
        // allocated_pageframe_count--;
    }
}

// 分配物理页框
ptr_t allocPage(pid_t pid, PTE *pte_ptr, int unswapable) {

    // printl("Enter allocPage...\n");
    for (int i = 0; i < PAGE_MAX_NUM; i++) {
        if(pageframes[i].alloc_record == 0) {
            assert(pageframes[i].unswapable == 0);

            if(!unswapable) {
                enqueue_pageframe(&pageframes[i]);
            }
            
            pageframes[i].alloc_record = 1;
            pageframes[i].owner_pid = pid;
            pageframes[i].pte_ptr = pte_ptr;
            pageframes[i].unswapable = unswapable;
            ptr_t page_addr = FREEMEM_KERNEL + i * PAGE_SIZE;
            memset((uint8_t *)page_addr, 0, PAGE_SIZE);

            // if(pte_ptr == NULL) {   // 在内核页表中
            //     printl("Here 2\n");
            //     pageframes[i].pte_ptr = kva2pte(page_addr);
            //     if(i == 0) {
            //         printl("allocPage: when i = 0, pid == %u, page_addr == %lx, pageframes[i].pte_ptr == %lx\n", pid, page_addr, pageframes[i].pte_ptr);
            //         printl("allocPage: *pageframes[i].pte_ptr == %lx, get_pa(*pageframes[i].pte_ptr) is %lx\n", *(pageframes[i].pte_ptr), get_pa(*(pageframes[i].pte_ptr)));
            //     }
            // }

            //printl("Exit allocPage 1, page_addr is %lx, pageframes[i].pte_ptr is %lx\n", page_addr, pageframes[i].pte_ptr);

            return page_addr;
        }
    }

    // 页替换机制实现
    assert(!LIST_EMPTY(&pageframe_queue));

    // printl("Here 1\n");
    pageframe_t *pageframe = dequeue_pageframe();
    
    assert(pageframe);
    assert(0 <= pageframe->id && pageframe->id < PAGE_MAX_NUM);
    assert(pageframes[pageframe->id].alloc_record == 0);
    assert(pageframe->unswapable == 0);

    if(!unswapable) {
        enqueue_pageframe(pageframe);
    }

    pageframe->alloc_record = 1;
    pageframe->owner_pid = pid;
    pageframe->pte_ptr = pte_ptr;
    pageframe->unswapable = unswapable;
    ptr_t page_addr = pageframe->id * PAGE_SIZE + FREEMEM_KERNEL;
    memset((uint8_t *)page_addr, 0, PAGE_SIZE);

    // if(pte_ptr == NULL) {
    //     printl("Here 3\n");
    //     pageframe->pte_ptr = kva2pte(page_addr);
    // }
    
    // printl("Exit allocPage 2, page_addr is %lx, pageframe->pte_ptr is %lx\n", page_addr, pageframe->pte_ptr);
    return page_addr;
}

// static ptr_t usrMemCurr = FREEMEM_USER;

// ptr_t allocPage(int numPage)
// {
//     // align PAGE_SIZE
//     ptr_t ret = ROUND(usrMemCurr, PAGE_SIZE);
//     usrMemCurr = ret + numPage * PAGE_SIZE;
//     return ret;
// }

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

// 回收物理页框
void freePage(ptr_t baseAddr)
{
    // printl("Enter freePage...\n");
    // TODO [P4-task1] (design you 'freePage' here if you need):
    int id = (baseAddr - FREEMEM_KERNEL) / PAGE_SIZE;
    // assert(pageframes[id].unswapable == 0);

    // if(!(id >= 0 && id < PAGE_MAX_NUM)) {
    //     printl("freePage: id: %d\n", id);
    // }

    // printl("freePage: id is %d, baseAddr is 0x%lx\n", id, baseAddr);
    assert(id >= 0 && id < PAGE_MAX_NUM);
    // pageframes[id].alloc_record = 0;
    // dequeue_pageframe();
    pageframes[id].alloc_record = 0;
    pageframes[id].owner_pid = -1;
    pageframes[id].unswapable = 0;
    pageframes[id].pte_ptr = NULL;
    if(!pageframes[id].unswapable) {
        LIST_DELETE(&pageframes[id].list);
    }
    // allocated_pageframe_count--;

    // printl("Exit freePage...\n");
}

void swap_out(pageframe_t *pageframe)
{
    assert(pageframe);
    assert(pageframe->alloc_record == 1);
    assert(pageframe->owner_pid >= 0 && pageframe->owner_pid < 16);
    assert(pageframe->pte_ptr != NULL);
    assert(pageframe->unswapable == 0);

    // if (pageframe->pte_ptr != NULL) {
    //     *(pageframe->pte_ptr) = 0;
    // }

    // uintptr_t page_pa = get_pa(*(pageframe->pte_ptr));

    // printl("mm.c: page_pa: %lx, pa: %lx\n", page_pa, kva2pa(FREEMEM_KERNEL + pageframe->id * PAGE_SIZE));

    // assert(page_pa == kva2pa(FREEMEM_KERNEL + pageframe->id * PAGE_SIZE));

    // uint32_t start_block = last_nonempty_sector + 8 + num_sector * pageframe->id;

    int begin_sector = alloc_sd_sector();

    uintptr_t page_pa = kva2pa(FREEMEM_KERNEL + pageframe->id * PAGE_SIZE);

    bios_sd_write(page_pa, PAGE_SIZE / SECTOR_SIZE, begin_sector);

    // begin_sector + 1的目的：防止begin_sector全为0，导致无法区分该页框是在SD中还是未建立映射

    // if(pageframe->pte_ptr == 0xffffffc052031000) {
    //     printl("DEBUG 1!!! write in pfn: %lx\n", (uint64_t)(begin_sector + 1));
    // }

    // printl("swap_out: pte_ptr: %lx, write in pfn: %lx\n", pageframe->pte_ptr, (uint64_t)(begin_sector + 1));

    set_pfn(pageframe->pte_ptr, (uint64_t)(begin_sector + 1));

    *(pageframe->pte_ptr) &= ~((uint64_t)_PAGE_PRESENT);
    // pcb[pageframe->owner_pid]

    LIST_DELETE(&pageframe->list);
    pageframe->alloc_record = 0;
    pageframe->owner_pid = -1;
    pageframe->pte_ptr = NULL;

    // printl("Exit swap_out...\n");
}

void swap_in(PTE *pte_ptr, pid_t pid, int unswapable)
{
    assert(*pte_ptr);
    assert((*pte_ptr & _PAGE_PRESENT) == 0);

    ptr_t page_kva = allocPage(pid, pte_ptr, unswapable);

    int id = getPageId(page_kva);

    pageframe_t *pageframe = &pageframes[id];

    // printl()
    assert(pageframe);
    assert(pageframe->alloc_record == 1);
    assert(pageframe->owner_pid == pid);
    assert(pageframe->unswapable == unswapable);

    uint64_t begin_sector = (get_pa(*(pte_ptr)) >> NORMAL_PAGE_SHIFT) - 1;

    ptr_t page_pa = kva2pa(page_kva);

    bios_sd_read(page_pa, PAGE_SIZE / SECTOR_SIZE, begin_sector);

    free_sd_sector(begin_sector);

    set_pfn(pte_ptr, page_pa >> NORMAL_PAGE_SHIFT);
    set_attribute(pte_ptr, _PAGE_PRESENT);

    // printl("Exit swap_in...\n");
}

uintptr_t va2kva(uintptr_t va, uintptr_t pgdir, pid_t pid, int *success) {
    // printl("Enter va2kva, va is %lx\n", va);
    PTE *pgd = (PTE *)pgdir;
    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> NORMAL_PAGE_SHIFT);
    uint64_t offset = va & ((1 << NORMAL_PAGE_SHIFT) - 1);
    if(pgd[vpn2] == 0) {
        *success = 0;
        // printl("Exit va2kva (1)\n");
        return 0;
    }
    else if((pgd[vpn2] & _PAGE_PRESENT) == 0) {
        assert(0);
    }
    if(isLeaf(pgd[vpn2])) {
        *success = 1;
        // printl("Exit va2kva (2)\n");
        // printl("DEBUG 1!!!!!!!! vpn2 is: %lx, vpn1 is %lx, vpn0 is %lx\n", vpn2, vpn1, vpn0);
        assert(0);
        return pa2kva(get_pa(pgd[vpn2]) | (vpn1 << (NORMAL_PAGE_SHIFT + PPN_BITS)) | (vpn0 << NORMAL_PAGE_SHIFT) | offset);
    }

    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    if(pmd[vpn1] == 0) {
        *success = 0;
        // printl("Exit va2kva (3)\n");
        return 0;
    }
    else if((pmd[vpn1] & _PAGE_PRESENT) == 0) {
        assert(0);
    }
    if(isLeaf(pmd[vpn1])) {
        *success = 1;
        // printl("Exit va2kva (4)\n");
        // printl("DEBUG 2!!!!!!!! vpn2 is: %lx, vpn1 is %lx, vpn0 is %lx\n", vpn2, vpn1, vpn0);
        assert(0);
        return pa2kva(get_pa(pmd[vpn1]) | (vpn0 << NORMAL_PAGE_SHIFT) | offset);
    }
    
    PTE *pt = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    if(pt[vpn0] == 0) {
        *success = 0;
        // printl("Exit va2kva (5)\n");
        return 0;
    }
    else if((pt[vpn0] & _PAGE_PRESENT) == 0) {
        swap_in(&pt[vpn0], pid, 0);
    }
    if(isLeaf(pt[vpn0])) {
        *success = 1;
        uintptr_t kva = pa2kva(get_pa(pt[vpn0]) | offset);
        // printl("Exit va2kva (6), kva is %lx\n", kva);
        return kva;
    }
    else
        assert(0);
}

// 用户页表PTE查找
PTE *va2pte(uintptr_t va, uintptr_t pgdir) {
    PTE *pgd = (PTE *)pgdir;
    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> NORMAL_PAGE_SHIFT);
    
    if(pgd[vpn2] == 0) {
        // printl("va2pte: pgd[vpn2] == 0: va is %lx, current_running->pid is %d\n", va, current_running->pid);
        // assert(0);
        uint64_t pfn = kva2pa(allocPage(current_running->pid, &pgd[vpn2], 1)) >> NORMAL_PAGE_SHIFT;

        set_pfn(&pgd[vpn2], pfn);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    
    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    if(pmd[vpn1] == 0) {
        // printl("va2pte: pmd[vpn1] == 0: va is %lx, current_running->pid is %d\n", va, current_running->pid);
        // assert(0);
        uint64_t pfn = kva2pa(allocPage(current_running->pid, &pmd[vpn1], 1)) >> NORMAL_PAGE_SHIFT;

        set_pfn(&pmd[vpn1], pfn);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }

    PTE *pt = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    // assert(pt[vpn0]);

    // printl("va2pte: va is %lx, pgd[vpn2] is %lx, pmd[vpn1] is %lx, pt[vpn0] is %lx\n", va, pgd[vpn2], pmd[vpn1], pt[vpn0]);

    return &pt[vpn0];
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    // 直接拷贝内核一级页表
    // 不用拷贝内核二级页表，内核二级页表由所有进程共用。因为一级页表的PTE本身就指向那里
    // 将内核一级页表先拷贝给用户一级页表，此时用户页表还没建立。这一步拷贝相当于初始化
    // 不用担心后续用户页表的建立和分配会覆盖拷贝的内核一级页表
    uint64_t vpn2 = (0xffffffc050000000 & VA_MASK) >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    assert(vpn2 == 257);
    memcpy((uint8_t *)(&((PTE *)dest_pgdir)[vpn2]), (uint8_t *)(&((PTE *)src_pgdir)[vpn2]), sizeof(PTE));
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, pid_t pid, uintptr_t pgdir)
{

    // printl("pgdir: %lx\n", pgdir);
    // printl("va: %lx\n", va);

    // TODO [P4-task1] alloc_page_helper:
    PTE *pgd = (PTE *)pgdir;
    va &= VA_MASK;
    // printl("mm.c: va is: %lx\n", va);
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> NORMAL_PAGE_SHIFT);

    // printl("mm.c: pgdir is: %lx, va is: %lx, vpn2 is: %lx, vpn1 is: %lx, vpn0 is: %lx\n", pgdir, va, vpn2, vpn1, vpn0);

    if (pgd[vpn2] == 0) {
        // 注意：set_pfn要先转换成物理地址

        // printl("Here 2!!!\n");
        uint64_t pfn = kva2pa(allocPage(pid, &pgd[vpn2], 1)) >> NORMAL_PAGE_SHIFT;

        // uint64_t pfn = kva2pa(allocKernelPage(1, pid)) >> NORMAL_PAGE_SHIFT;

        // if(&pgd[vpn2] == 0xffffffc052031000) {
        //     printl("DEBUG 3!!! write in pfn: %lx\n", pfn);
        // }

        set_pfn(&pgd[vpn2], pfn);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    else if((pgd[vpn2] & _PAGE_PRESENT) == 0) {
        // swap in
        // pageframe_t *pageframe = dequeue_pageframe();
        // assert(pageframe != NULL);
        // printl("Here 2\n");
        // printl("swap_in 2\n");
        swap_in(&pgd[vpn2], pid, 1);
        // pageframe->owner_pid = pid;
    }


    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    if (pmd[vpn1] == 0) {

        // printl("Here 3!!!\n");
        uint64_t pfn = kva2pa(allocPage(pid, &pmd[vpn1], 1)) >> NORMAL_PAGE_SHIFT;

        // uint64_t pfn = kva2pa(allocKernelPage(1, pid)) >> NORMAL_PAGE_SHIFT;

        // if(&pmd[vpn1] == 0xffffffc052031000) {
        //     printl("DEBUG 4!!! write in pfn: %lx\n", pfn);
        // }

        set_pfn(&pmd[vpn1], pfn);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    else if((pmd[vpn1] & _PAGE_PRESENT) == 0) {
        // swap in
        // pageframe_t *pageframe = dequeue_pageframe();
        // assert(pageframe != NULL);
        // printl("Here 3\n");
        // printl("swap_in 3\n");
        swap_in(&pmd[vpn1], pid, 1);
        // pageframe->owner_pid = pid;
    }

    PTE *pt = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    // printl("DEBUG: pt[511] is: %lx, &pt[511] is: %lx\n", pt[511], &pt[511]);
    
    // if(&pt[vpn0] == 0xffffffc052011000lu) {
    //     printl("1, pt[vpn0]: %lx\n", pt[vpn0]);
    // }

    
    if(pt[vpn0] == 0) {

        // printl("Here 4!!! va is %lx\n", va);
        uint64_t pfn = kva2pa(allocPage(pid, &pt[vpn0], 0)) >> NORMAL_PAGE_SHIFT;

        // if(&pt[vpn0] == 0xffffffc052031000) {
        //     printl("DEBUG 5!!! write in pfn: %lx\n", pfn);
        // }

        set_pfn(&pt[vpn0], pfn);
        set_attribute(
            &pt[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                            _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
    }
    else if((pt[vpn0] & _PAGE_PRESENT) == 0) {
        // swap in
        // pageframe_t *pageframe = dequeue_pageframe();
        // assert(pageframe != NULL);
        // printl("Here 4\n");
        // printl("swap_in 4\n");
        // if(&pt[vpn0] == 0xffffffc052011000lu) {
        //     printl("2, pt[vpn0]: %lx\n", pt[vpn0]);
        // }

        swap_in(&pt[vpn0], pid, 0);
        // pageframe->owner_pid = pid;
    }

    // *pte = pt[vpn0];
    return pa2kva(get_pa(pt[vpn0]));
}

void create_mapping(uintptr_t va, uintptr_t pa, uintptr_t pgdir, int kernel) {
    PTE *pgd = (PTE *)pgdir;
    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (PPN_BITS + PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> NORMAL_PAGE_SHIFT);

    if (pgd[vpn2] == 0) {

        // printl("va is: %lx\n", va);

        uint64_t pfn = kva2pa(allocPage(current_running->pid, &pgd[vpn2], 1)) >> NORMAL_PAGE_SHIFT;
        set_pfn(&pgd[vpn2], pfn);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }
    else if((pgd[vpn2] & _PAGE_PRESENT) == 0){
        // printl("va is: %lx, pgd[vpn2]: %lx\n", va, pgd[vpn2]);
        assert(0);
    }

    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    if (pmd[vpn1] == 0) {
        uint64_t pfn = kva2pa(allocPage(current_running->pid, &pmd[vpn1], 1)) >> NORMAL_PAGE_SHIFT;
        set_pfn(&pmd[vpn1], pfn);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }
    else if((pmd[vpn1] & _PAGE_PRESENT) == 0) {
        assert(0);
    }

    PTE *pt = (PTE *)pa2kva(get_pa(pmd[vpn1]));
    
    if(pt[vpn0] == 0) {
        uint64_t pfn = pa >> NORMAL_PAGE_SHIFT;

        set_pfn(&pt[vpn0], pfn);
        if(kernel) {
            set_attribute(
                &pt[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);
        }
        else {
            set_attribute(
                &pt[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                                _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);
        }
    }
    else if((pt[vpn0] & _PAGE_PRESENT) == 0) {
        assert(0);
    }
}

size_t get_free_memory(void) {
    size_t free_mem = 0;
    for(int i = 0; i < PAGE_MAX_NUM; i++) {
        if(pageframes[i].alloc_record == 0) {
            free_mem += PAGE_SIZE;
        }
    }
    
    return free_mem;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}
