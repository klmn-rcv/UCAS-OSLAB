/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000
#define PAGE_SIZE       4096

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr t0, mhartid
  bne t0, zero, slave_core
  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la t0, __bss_start
  la t1, __BSS_END__

bss_clear_loop:
  beq t0, t1, bss_clear_end
  sb zero, 0(t0)
  addi t0, t0, 1
  jal zero, bss_clear_loop

bss_clear_end:

  /* TODO: [p1-task2] setup C environment */
  la sp, KERNEL_STACK + PAGE_SIZE
  jal zero, master_core
slave_core:
  la sp, KERNEL_STACK + 2 * PAGE_SIZE
master_core:
  jal ra, main


loop:
  wfi
  j loop

END(_start)
