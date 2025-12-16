#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define BUF_SIZE   8912

uint8_t recv_buffer[BUF_SIZE];

int main(int argc, char *argv[])
{
    int print_location = 1;
    sys_move_cursor(0, print_location);
    printf("recv_stream_loop: start");
    int i = 0;
    while (1) {
        i++;
        int n = BUF_SIZE;
        sys_move_cursor(0, print_location);
        long ret = sys_net_recv_stream(recv_buffer, &n);
        printf("recv_stream_loop: want %d, got %d (%d)\n", BUF_SIZE, ret, i);
    }
    return 0;
}
