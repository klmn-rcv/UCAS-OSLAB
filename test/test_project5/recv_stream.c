#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define RECV_BUFFER_SIZE 1024

static uint16_t fletcher16(uint16_t cur, const uint8_t *data, int n) {
    uint16_t sum1 = (uint16_t)(cur & 0xFF);
    uint16_t sum2 = (uint16_t)((cur >> 8) & 0xFF);
    
    for (int i = 0; i < n; ++i) {
        sum1 = (uint16_t)((sum1 + data[i]) % 0xFF);
        sum2 = (uint16_t)((sum2 + sum1) % 0xFF);
    }
    return (uint16_t)((sum2 << 8) | sum1);
}

int main() {
    sys_move_cursor(0, 0);
    printf("[recv stream] started");
    
    uint8_t buffer[RECV_BUFFER_SIZE];
    
    uint8_t size_buf[4];
    int nbytes = 4;
    
    sys_net_recv_stream(size_buf, &nbytes);
    
    if (nbytes != 4) {
        sys_move_cursor(0, 0);
        printf("[recv stream] error: failed to receive file size");
        return -1;
    }
    
    uint32_t total_size;
    memcpy((uint8_t *)&total_size, size_buf, 4);
    uint32_t file_content_size = total_size - 4;
    
    sys_move_cursor(0, 1);
    printf("[recv stream] file size: %u bytes", file_content_size);
    
    if (file_content_size == 0) {
        sys_move_cursor(0, 3);
        printf("[recv stream] finally received 0 bytes, Fletcher16 checksum = 0x0000");
        return 0;
    }
    
    uint32_t received = 0;
    uint16_t checksum = 0;
    int iteration = 0;
    
    while (received < file_content_size) {
        iteration++;
        
        uint32_t remaining = file_content_size - received;
        nbytes = (remaining > RECV_BUFFER_SIZE) ? RECV_BUFFER_SIZE : remaining;
        
        int result = sys_net_recv_stream(buffer, &nbytes);
        
        if (nbytes <= 0) {
            sys_move_cursor(0, 2);
            printf("[recv stream] error: failed to receive data at offset %u", received);
            return -1;
        }
        
        checksum = fletcher16(checksum, buffer, nbytes);
        
        received += nbytes;
        
        sys_move_cursor(0, 2);
        printf("[recv stream] received %u bytes (iteration %d), checksum = 0x%04X", 
               received, iteration, checksum);
    }
    
    sys_move_cursor(0, 3);
    printf("[recv stream] finally received %u bytes, Fletcher16 checksum = 0x%04X", 
           received, checksum);
    
    return 0;
}