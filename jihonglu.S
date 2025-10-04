
.global main
main:

// TODO: [p0-task1] Add your code here!

.set DEST, 0x50200000

    li t0, 0
    li t1, 1
    li t2, 49

loop:
    blt t2, zero, end
    add t0, t0, t1
    addi t1, t1, 1
    addi t2, t2, -1
    jal zero, loop

end:
    la t4, DEST
    sw t0, 0(t4)
