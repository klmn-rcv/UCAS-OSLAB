/* RISC-V kernel boot stage */
#include <pgtable.h>
#include <asm.h>

#define ARRTIBUTE_BOOTKERNEL __attribute__((section(".bootkernel")))

typedef void (*kernel_entry_t)(unsigned long);

/********* setup memory mapping ***********/
static uintptr_t ARRTIBUTE_BOOTKERNEL alloc_page()
{
    // 用于分配内核二级页表
    // 这里pg_base是静态变量，因此每次分配的页表基址是递增的
    static uintptr_t pg_base = PGDIR_PA;
    pg_base += 0x1000;  // 每次分配4KB空间作为二级页表
    return pg_base;
    // 注意PGDIR_PA（0x51000000，物理）地址往下的空间结构：
    // 0x51000000 ~ 0x51000fff：内核一级页表（4KB）
    // 0x51001000 ~ 0x51001fff：内核二级页表1（4KB，对应0xffffffc0开头的地址映射）
    // 0x51002000 ~ 0x51002fff：内核二级页表2（4KB，对应地址恒等映射）
    // 一共只分配了两个二级页表
}

// using 2MB large page
// va，pa分别是要建立映射的虚拟/物理地址，pgdir是一级页表（根目录）的物理地址
static void ARRTIBUTE_BOOTKERNEL map_page(uint64_t va, uint64_t pa, PTE *pgdir)
{
    // 这个函数使用时，还没有enable_vm，因此直接使用物理地址

    // 步骤1：确保虚拟地址有效（只使用39位有效位）
    va &= VA_MASK;  // VA_MASK是0x7fffffffff（39位掩码）
    // 0xffffffc0_50000000 => 0x40_50000000（39位，最高位为1）

    // 步骤2：提取VPN[2]（一级页表索引）
    // Sv39地址分解：[VPN[2]:9位][VPN[1]:9位][VPN[0]:9位][Offset:12位]
    // NORMAL_PAGE_SHIFT = 12（页内偏移位数）
    // PPN_BITS = 9（每个PPN字段9位）
    // 所以：va >> (12 + 9 + 9) = va >> 30，取[38:30]位（一级页表索引）
    // 这里vpn2算出来是定值，十六进制0x101，十进制257（没考虑地址恒等映射，考虑的话总共就有两个取值，257和1）
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    
    // 步骤3：提取VPN[1]（二级页表索引）
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));

    // 步骤4：检查一级页表项是否存在，不存在则创建
    // 因为vpn2是定值，所以实际上这个if只会进去1次，alloc_page也只会调用一次，分配一个二级页表（同样没考虑地址恒等映射，考虑的话就分配了两个二级页表）
    if (pgdir[vpn2] == 0) {
        // alloc a new second-level page directory
        // 分配页，并在PTE中设置PPN
        set_pfn(&pgdir[vpn2], alloc_page() >> NORMAL_PAGE_SHIFT);
        // 在PTE中设置标志位（设置V=1，其余标志位全为0，表明是非叶子节点）
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
        // get_pa(pgdir[vpn2])得到二级页表的物理地址，clear_pgdir清空这个二级页表
        clear_pgdir(get_pa(pgdir[vpn2]));
    }
    // 得到二级页表的物理地址
    PTE *pmd = (PTE *)get_pa(pgdir[vpn2]);
    // pa >> NORMAL_PAGE_SHIFT得到物理页号，直接设置到二级页表项pmd[vpn1]中
    // 因为这里用的是2MB大页，所以只有两级页表，第二级页表的PTE中就存真实的物理页号
    set_pfn(&pmd[vpn1], pa >> NORMAL_PAGE_SHIFT);
    // 叶子节点，要设置很多标志位
    // V置为1
    // R，W，X均置为1
    // U置为0（內核页表当然不能让用户访问）
    // G置为0
    // A，D均置为1
    set_attribute(
        &pmd[vpn1], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);

    // 判别叶子节点：V=1，且R/W/X不全为0
}

static void ARRTIBUTE_BOOTKERNEL enable_vm()
{
    // write satp to enable paging
    set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
}

/* Sv-39 mode
 * 0x0000_0000_0000_0000-0x0000_003f_ffff_ffff is for user mode
 * 0xffff_ffc0_0000_0000-0xffff_ffff_ffff_ffff is for kernel mode
 */
static void ARRTIBUTE_BOOTKERNEL setup_vm()
{
    // 清除内核一级页表中的各项
    clear_pgdir(PGDIR_PA);
    // map kernel virtual address(kva) to kernel physical
    // address(kpa) kva = kpa + 0xffff_ffc0_0000_0000 use 2MB page,
    // map all physical memory
    // 把0xffffffc0开头的内核虚拟地址全部映射到相应的物理地址（建立这个页表结构）
    PTE *early_pgdir = (PTE *)PGDIR_PA;
    for (uint64_t kva = 0xffffffc050000000lu;
         kva < 0xffffffc060000000lu; kva += 0x200000lu) {

        // if(kva == 0xffffffc052000000lu) {
        //     asm volatile("nop");
        // }

        map_page(kva, kva2pa(kva), early_pgdir);
    }
    // map boot address
    // 物理地址也会被映射到自身
    // 这是因为，当set_satp() 执行后，CPU会立即开始使用虚拟地址翻译
    // 但当前程序计数器（PC）还是物理地址，所以下一条指令的取指会出问题
    // 也就是执行local_flush_tlb_all()这个函数时，需要把物理地址当作虚拟地址来用，因此需要建立相应映射（恒等映射）
    // 之后就会执行((kernel_entry_t)pa2kva(_start))(mhartid)，跳转到_start，这一步跳转后PC就变成0xffffffc0开头的真正的虚拟地址了
    for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu;
         pa += 0x200000lu) {
        map_page(pa, pa, early_pgdir);
    }
    enable_vm();
}

extern uintptr_t _start[];

/*********** start here **************/
int ARRTIBUTE_BOOTKERNEL boot_kernel(unsigned long mhartid)
{
    if (mhartid == 0) {
        setup_vm();
    } else {
        enable_vm();
    }

    /* enter kernel */
    ((kernel_entry_t)pa2kva((unsigned long)_start))(mhartid);

    return 0;
}
