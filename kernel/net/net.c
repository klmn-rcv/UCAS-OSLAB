#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>

LIST_HEAD(send_block_queue);
LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full

    int transmit_len;
    while(1){
        transmit_len = e1000_transmit(txpacket, length);
        if(transmit_len == 0){
            do_block(&current_running->list, &send_block_queue);
        }
        else {
            break;
        }
    }
    
    return transmit_len;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when there is no packet on the way
    
    int total_bytes = 0;
    for (int i = 0; i < pkt_num; i++) {
        int received_len;
        while (1) {
            received_len = e1000_poll((char *)rxbuffer + total_bytes);
            if (received_len == 0) {
                do_block(&current_running->list, &recv_block_queue);
            } 
            else {
                pkt_lens[i] = received_len;
                total_bytes += received_len;
                break;
            }
        }
    }

    return total_bytes;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
}