.set DEST, 0x50200000
.global main
main:
    li s0, 1
    li s1, 201
    la s2, DEST
    addi s2, s2, 1
loop_2:
    bge s0, s1, end

    add a0, s0, zero
    jal ra, is_prime
    sb a0, 0(s2)

    addi s0, s0, 1
    addi s2, s2, 1

    jal zero, loop_2
    
end:
    addi zero, zero, 0
    jal zero, end

is_prime:
    li t0, 1
    beq a0, t0, end_not_prime
    li t0, 2

loop_1:
    bge t0, a0, end_prime

    rem t1, a0, t0
    beq t1, zero, end_not_prime

    addi t0, t0, 1
    jal zero, loop_1

end_not_prime:
    li a0, 0
    jal zero, end_func
end_prime:
    li a0, 1
end_func:
    jalr x0, ra, 0
