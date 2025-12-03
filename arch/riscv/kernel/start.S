/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

/* maybe you need another stack for booting? */
.equ boot_stack_top_base_master, 0x50201800
.equ boot_stack_top_base_slave, 0x50202000

.section ".bootkernel_entry","ax"
ENTRY(_boot)
  /*
   * this is the entrypoint of the kernel. 
   * Don't clear BSS here
   */

  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  csrr t0, CSR_MHARTID
  bne t0, zero, slave_core

  la sp, boot_stack_top_base_master
  jal zero, master_core
slave_core:
  la sp, boot_stack_top_base_slave
master_core:
  csrr a0, CSR_MHARTID 

  // this will never return
  call boot_kernel


loop:
  wfi
  j loop

END(_boot)
