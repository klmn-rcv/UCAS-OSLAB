#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define MAX_PAGES   2048
#define PAGE_SIZE   0x1000
#define ROUNDS      2

static char blank[] = {"                                             "};

uintptr_t addrs[MAX_PAGES];
unsigned long expect[MAX_PAGES];

void sys_move_cursor(int x, int y);

int main(int argc, char *argv[])
{
    uintptr_t base = (uintptr_t)atol(argv[2]);
    int np = atoi(argv[3]);


    for (int i = 0; i < np; ++i) {
        sys_move_cursor(0, 0);
        printf("DEBUG 1: i is %d\n", i);
        addrs[i] = base + (uintptr_t)i * PAGE_SIZE;
    }

    for (int i = 0; i < np; ++i) {
        sys_move_cursor(0, 1);
        printf("DEBUG 2: i is %d\n", i);

        if(i == 2112) {
            asm volatile("nop");
        }

        expect[i] = ((unsigned long)i << 32) ^ 0xf1919810UL;
        *(volatile unsigned long *)addrs[i] = expect[i];
    }

    for (int r = 0; r < ROUNDS; ++r) {
        for (int i = 0; i < np; ++i) {
            unsigned long val = *(volatile unsigned long *)addrs[i];
            if (val != expect[i]) {
                sys_move_cursor(0, 2);
                printf("> [SWAPTEST] ERROR at page %d addr=0x%lx (expect=0x%lx, got=0x%lx)\n", i, (unsigned long)addrs[i], expect[i], val);
                return -1;
            }
            unsigned long newv = expect[i] + (unsigned long)(r + i + 1);
            *(volatile unsigned long *)addrs[i] = newv;
            expect[i] = newv;
        }
        sys_move_cursor(0, 0);
        printf("> [SWAPTEST] round %d / %d done.\n", r + 1, ROUNDS);
    }
    sys_move_cursor(0, 1);
    printf("> [SWAPTEST] SUCCESS.\n");

    return 0;
}
