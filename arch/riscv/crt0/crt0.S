#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    addi sp, sp, -16
    sd ra, 8(sp)
    sd fp, 0(sp)
    add fp, sp, zero

    la t0, __bss_start
    la t1, __BSS_END__
bss_clear_loop:
    beq t0, t1, bss_clear_end
    sb zero, 0(t0)
    addi t0, t0, 1
    jal zero, bss_clear_loop
bss_clear_end:
    /* TODO: [p1-task3] enter main function */
    jal ra, main

    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    add sp, fp, zero
    ld fp, 0(sp)
    ld ra, 8(sp)
    addi sp, sp, 16
    //jalr zero, 0(ra)


    /************************************************************/
    /* TODO: [P3] call sys_exit */
    /************************************************************/
    call sys_exit

// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
