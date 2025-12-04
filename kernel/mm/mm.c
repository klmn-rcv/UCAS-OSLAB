#include <os/mm.h>
#include <os/string.h>
#include <assert.h>

// NOTE: A/C-core
// static ptr_t kernMemCurr = FREEMEM_KERNEL;

int8_t alloc_record[PAGE_TOTAL_NUM];

// 分配物理页框
// ptr_t allocPage(int numPage)
// {
//     // align PAGE_SIZE
//     // ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
//     // kernMemCurr = ret + numPage * PAGE_SIZE;
//     // return ret;
//     for(int i = 0; i < PAGE_TOTAL_NUM; i++) {
//         if(alloc_record[i] == 0) {
//             alloc_record[i] = 1;
//             return FREEMEM_KERNEL + i * PAGE_SIZE;
//         }
//     }
// }

ptr_t allocPage(int numPage) {
    int start = -1;
    int count = 0;
    
    // 寻找连续的空闲页
    for (int i = 0; i < PAGE_TOTAL_NUM; i++) {
        if (alloc_record[i] == 0) {
            if (count == 0) 
                start = i;
            count++;
            if (count == numPage) {
                // 标记为已分配
                for (int j = start; j < start + numPage; j++) {
                    alloc_record[j] = 1;
                }
                ptr_t page_addr = FREEMEM_KERNEL + start * PAGE_SIZE;
                memset((uint8_t *)page_addr, 0, numPage * PAGE_SIZE);
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
    // TODO [P4-task1] (design you 'freePage' here if you need):
    int id = (baseAddr - FREEMEM_KERNEL) / PAGE_SIZE;
    // printl("id: %d\n", id);
    assert(id >= 0 && id < PAGE_TOTAL_NUM);
    alloc_record[id] = 0;
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
    memcpy((uint8_t *)dest_pgdir, (uint8_t *)src_pgdir, PAGE_SIZE);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, PTE *pte, int *already_exist)
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
        set_pfn(&pgd[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgd[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pgd[vpn2])));
    }

    PTE *pmd = (PTE *)pa2kva(get_pa(pgd[vpn2]));
    if (pmd[vpn1] == 0) {
        set_pfn(&pmd[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd[vpn1])));
    }

    PTE *pt = (PTE *)pa2kva(get_pa(pmd[vpn1]));

    // printl("DEBUG: pt[511] is: %lx, &pt[511] is: %lx\n", pt[511], &pt[511]);
    
    if(pt[vpn0] == 0) {
        *already_exist = 0;
        set_pfn(&pt[vpn0], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
    }
    else {
        *already_exist = 1;
    }
    
    set_attribute(
        &pt[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);

    *pte = pt[vpn0];
    return pa2kva(get_pa(pt[vpn0]));
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}
