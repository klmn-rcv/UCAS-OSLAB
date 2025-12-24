#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/smp.h>
#include <os/mm.h>
#include <os/net.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>
#include <csr.h>
#include <plic.h>

#define SCAUSE_IRQ_FLAG   (1UL << 63)
#define LENGTH 60

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    cpuid = get_current_cpu_id();
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`

    // if((regs->sstatus & SR_SPP) && (scause == EXCC_INST_PAGE_FAULT || scause == EXCC_LOAD_PAGE_FAULT || scause == EXCC_STORE_PAGE_FAULT)) {
    //     printl("Kernel page fault detected! stval: %lx, scause: %lx, sepc: %lx, pid: %d, cpuid: %d\n", stval, scause, regs->sepc, current_running->pid, cpuid);
    // }

    if(scause & SCAUSE_IRQ_FLAG) {
        uint64_t irq_cause = scause & ~SCAUSE_IRQ_FLAG;
        if (irq_cause < IRQC_COUNT && irq_table[irq_cause])
            irq_table[irq_cause](regs, stval, scause);
        else assert(0);
    }
    else {
        if(scause < EXCC_COUNT && exc_table[scause])
            exc_table[scause](regs, stval, scause);
        else assert(0);
    }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint64_t current_time = get_ticks();
    bios_set_timer(current_time + TIMER_INTERVAL);
    do_scheduler();
}

void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause) {

    // printl("handle_page_fault: stval is %lx, scause is %lx, sepc is %lx\n", stval, scause, regs->sepc);

    uintptr_t va = stval & VA_MASK;  // 触发缺页的虚拟地址
    uintptr_t pgdir = current_running->pgdir;
    // PTE* pte = page_walk(pgdir, va);  // 查找页表项
    // PTE pte;
    // printl("alloc_page_helper 2, va is: %lx\n", va);
    uintptr_t physic_page_kva = alloc_page_helper(va, current_running->pid, pgdir);
    
    // 刷新TLB
    local_flush_tlb_page(va);
    local_flush_icache_all();
}

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task4] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...
    uint32_t ir_src_id = plic_claim();
    if(ir_src_id == PLIC_E1000_QEMU_IRQ || ir_src_id == PLIC_E1000_PYNQ_IRQ) {
        net_handle_irq();
    }
    plic_complete(ir_src_id);
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    exc_table[EXCC_INST_MISALIGNED]  = handle_other;
    exc_table[EXCC_INST_ACCESS]      = handle_other;
    exc_table[EXCC_BREAKPOINT]       = handle_other;
    exc_table[EXCC_LOAD_ACCESS]      = handle_other;
    exc_table[EXCC_STORE_ACCESS]     = handle_other;
    exc_table[EXCC_SYSCALL]          = handle_syscall;
    exc_table[EXCC_INST_PAGE_FAULT]  = handle_page_fault;
    exc_table[EXCC_LOAD_PAGE_FAULT]  = handle_page_fault;
    exc_table[EXCC_STORE_PAGE_FAULT] = handle_page_fault;


    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    irq_table[IRQC_U_SOFT]  = handle_other;
    irq_table[IRQC_S_SOFT]  = handle_other;
    irq_table[IRQC_M_SOFT]  = handle_other;
    irq_table[IRQC_U_TIMER] = handle_other;
    irq_table[IRQC_S_TIMER] = handle_irq_timer;
    irq_table[IRQC_M_TIMER] = handle_other;
    irq_table[IRQC_U_EXT]   = handle_other;
    irq_table[IRQC_S_EXT]   = handle_irq_ext;
    irq_table[IRQC_M_EXT]   = handle_other;

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    // setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx stval: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->stval, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
