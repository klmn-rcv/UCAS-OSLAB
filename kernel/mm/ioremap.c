#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    // unsigned long aligned_phys = phys_addr & ~(NORMAL_PAGE_SIZE - 1);
    // unsigned long aligned_size = (size + NORMAL_PAGE_SIZE - 1) & ~(NORMAL_PAGE_SIZE - 1);
    unsigned long num_pages = size / NORMAL_PAGE_SIZE;

    uintptr_t kernel_pgdir = pa2kva(PGDIR_PA);

    for (unsigned long i = 0; i < num_pages; ++i) {
        uintptr_t va = io_base + i * NORMAL_PAGE_SIZE;
        uintptr_t pa = phys_addr + i * NORMAL_PAGE_SIZE;
        create_mapping(va, pa, kernel_pgdir, 1);
    }

    io_base += size;
    local_flush_tlb_all();
    return (void *)(io_base - size);
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
