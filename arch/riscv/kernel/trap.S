#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0
  li t0, 3
  csrc stvec, t0

  jr ra

  /* TODO: [p2-task4] enable interrupts globally */

ENDPROC(setup_exception)
