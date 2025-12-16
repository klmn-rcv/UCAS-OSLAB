#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/net.h>
#include <os/time.h>
#include <e1000.h>
#include <assert.h>

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
    printl("Enter net_handle_irq...\n");
    uint32_t icr = e1000_read_reg(e1000, E1000_ICR);
    if (icr & E1000_ICR_TXQE) {
        printl("net_handle_irq: send\n");
        e1000_handle_txqe();
    }
    // else if (icr & E1000_ICR_RXDMT0) {
    //     e1000_handle_rxdmt0();        
    // }
    // else {
    //     assert(0);
    // }
    else {
        printl("net_handle_irq: recv\n");
        e1000_handle_rxdmt0();
    }
}

uint64_t timer_start = 0;
int timer_active = 0;
uint32_t next_expected_seq = 0;
int ever_received = 0;
uint8_t tpc_ip_head_buffer[RTP_OFFSET];
// 定义队列头
LIST_HEAD(ooo_rtp_queue);
LIST_HEAD(free_rtp_nodes);     // 空闲RTP节点队列
rtp_packet_node_t rtp_node_pool[MAX_RTP_NODES];

void init_free_rtp_nodes() {
    // 初始化RTP节点池，将所有节点加入空闲队列
    for (int i = 0; i < MAX_RTP_NODES; i++) {
        LIST_INIT_HEAD(&rtp_node_pool[i].list);
        LIST_APPEND(&rtp_node_pool[i].list, &free_rtp_nodes);
    }
}

// 16位网络字节序转主机字节序
static uint16_t ntohs(uint16_t n) {
    return ((n & 0xFF) << 8) | ((n >> 8) & 0xFF);
}

// 32位网络字节序转主机字节序  
static uint32_t ntohl(uint32_t n) {
    return ((n & 0xFF) << 24) | 
           ((n & 0xFF00) << 8) | 
           ((n >> 8) & 0xFF00) | 
           ((n >> 24) & 0xFF);
}

// 16位主机字节序转网络字节序
static uint16_t htons(uint16_t h) {
    return ((h & 0xFF) << 8) | ((h >> 8) & 0xFF);
}

// 32位主机字节序转网络字节序
static uint32_t htonl(uint32_t h) {
    return ((h & 0xFF) << 24) | 
           ((h & 0xFF00) << 8) | 
           ((h >> 8) & 0xFF00) | 
           ((h >> 24) & 0xFF);
}

static void reset_timer(void) {
    timer_start = get_ticks();
    timer_active = 1;
}

static int is_timeout(void) {
    if (!timer_active) return 0;
    uint64_t current_time = get_ticks();
    return (current_time - timer_start) > RETRANS_TIMEOUT;
}

// 从空闲队列分配一个RTP节点
static rtp_packet_node_t* alloc_rtp_node(void) {
    if (LIST_EMPTY(&free_rtp_nodes)) {
        printl("[NET] Warning: No free RTP nodes available!\n");
        return NULL;  // 没有空闲节点
    }
    
    list_node_t *node = LIST_FIRST(&free_rtp_nodes);
    LIST_DELETE(node);
    
    rtp_packet_node_t *rtp_node = LIST_ENTRY(node, rtp_packet_node_t, list);
    // LIST_INIT_HEAD(&rtp_node->list);  // 重置链表指针
    
    return rtp_node;
}

// 释放RTP节点到空闲队列
static void free_rtp_node(rtp_packet_node_t *node) {
    if (node) {
        LIST_APPEND(&node->list, &free_rtp_nodes);
    }
}

// 在无序RTP队列中查找指定seq的包
static rtp_packet_node_t* find_in_ooo_queue(uint32_t seq) {
    list_node_t *node;
    if(!LIST_EMPTY(&ooo_rtp_queue)) {
        for (node = ooo_rtp_queue.next; node != &ooo_rtp_queue; node = node->next) {
            rtp_packet_node_t *rtp_node = LIST_ENTRY(node, rtp_packet_node_t, list);
            if (rtp_node->seq == seq) {
                LIST_DELETE(node);
                return rtp_node;
            }
        }
    }
    return NULL;
}

// 添加包到无序RTP队列
static void add_to_ooo_queue(uint32_t seq, uint8_t *data, uint16_t len) {
    rtp_packet_node_t *node = alloc_rtp_node();
    if (!node) {
        // 没有空闲节点，丢弃这个包
        printl("[NET] Dropped packet seq=%u (no free nodes)\n", seq);
        return;
    }
    
    node->seq = seq;
    node->len = len;
    if (len > 0 && data) {
        memcpy(node->data, data, len);
    }
    
    LIST_APPEND(&node->list, &ooo_rtp_queue);
    printl("[NET] Added seq=%u to OOO queue (len=%u)\n", seq, len);
}

static void create_tcp_ip_header(uint8_t *packet) {
    memcpy(packet, tpc_ip_head_buffer, RTP_OFFSET);

    // Swap Ethernet MACs
    struct ethhdr *eth = (struct ethhdr *)packet;
    uint8_t tmp_mac[6];
    memcpy(tmp_mac, eth->ether_dmac, 6);
    memcpy(eth->ether_dmac, eth->ether_smac, 6);
    memcpy(eth->ether_smac, tmp_mac, 6);

    // Swap IP Addresses (IPv4)
    // IP Header starts at 14
    // Src IP at 14+12=26, Dst IP at 14+16=30
    uint32_t *src_ip = (uint32_t *)(packet + 26);
    uint32_t *dst_ip = (uint32_t *)(packet + 30);
    uint32_t tmp_ip = *src_ip;
    *src_ip = *dst_ip;
    *dst_ip = tmp_ip;

    // Swap UDP Ports
    // UDP Header starts at 14+20=34
    // Src Port at 34, Dst Port at 36
    uint16_t *src_port = (uint16_t *)(packet + 34);
    uint16_t *dst_port = (uint16_t *)(packet + 36);
    uint16_t tmp_port = *src_port;
    *src_port = *dst_port;
    *dst_port = tmp_port;

}

// 发送RSD包
static void send_rsd_packet(uint32_t seq) {
    uint8_t rsd_packet[128];
    create_tcp_ip_header(rsd_packet);

    struct rtp_proto_header *header = (struct rtp_proto_header *)(rsd_packet + RTP_OFFSET);
    
    header->magic = RTP_MAGIC;
    header->flags = RTP_FLAGS_RSD;
    header->len = htons(0);  // RSD包没有数据
    header->seq = htonl(seq);
    
    do_net_send(rsd_packet, RTP_OFFSET + RTP_HEADER_SIZE);
    printl("[NET] Sent RSD for seq=%u\n", seq);
}

// 发送ACK包
static void send_ack_packet(uint32_t seq) {
    uint8_t ack_packet[128];
    create_tcp_ip_header(ack_packet);

    struct rtp_proto_header *header = (struct rtp_proto_header *)(ack_packet + RTP_OFFSET);
    
    header->magic = RTP_MAGIC;
    header->flags = RTP_FLAGS_ACK;
    header->len = htons(0);  // ACK包没有数据
    header->seq = htonl(seq);
    
    do_net_send(ack_packet, RTP_OFFSET + RTP_HEADER_SIZE);
    printl("[NET] Sent ACK for seq=%u\n", seq);
}

static int process_expected_packet(uint32_t seq, uint8_t *data, uint16_t len, 
                                    uint8_t **user_buffer, uint32_t *delivered_len,
                                    uint32_t requested_len) {
    // 计算可以复制的数据量
    uint32_t copy_len = len;
    if (*delivered_len + copy_len > requested_len) {
        copy_len = requested_len - *delivered_len;
    }

    // 复制数据到用户缓冲区
    memcpy(*user_buffer + *delivered_len, data, copy_len);
    *delivered_len += copy_len;

    // 发送ACK确认
    send_ack_packet(seq + copy_len);  // 确认到这个包之后的位置

    // 更新期待的seq
    next_expected_seq = seq + copy_len;

    // 重置计时器
    reset_timer();

    // 返回已处理的数据量
    return copy_len;
}

int do_net_recv_stream(void *buffer, int *nbytes) {

    printl("Enter do_net_recv_stream...\n");

    if (!buffer || !nbytes || *nbytes <= 0) {
        if (nbytes) *nbytes = 0;
        assert(0);
        return -1;
    }
    
    // 初始化计时器（如果是第一次调用或需要重置）
    if (!timer_active) {
        reset_timer();
    }
    
    uint32_t requested_len = *nbytes;
    uint32_t delivered_len = 0;
    uint8_t *user_buffer = (uint8_t *)buffer;
    
    int i = 0;

    while (delivered_len < requested_len) {
        i++;
        printl("loop %d\n", i);

        // 1. 先查无序RTP队列，看是否有我们期待的seq包
        rtp_packet_node_t *found = find_in_ooo_queue(next_expected_seq);

        printl("Here 1, next_expected_seq is %d, found->seq is %d, found->len is %d\n", next_expected_seq, found ? found->seq : -1, found ? found->len : -1);

        if (found) {
            // 处理找到的包
            int processed_len = process_expected_packet(found->seq, found->data, 
                                                         found->len, &user_buffer, 
                                                         &delivered_len, requested_len);
            int fully_processed = (processed_len == found->len);

            // 如果包没有完全处理（用户缓冲区满了）
            if (!fully_processed) {
                // 更新包中的数据（移除已处理的部分）
                uint32_t remaining_len = found->len - processed_len;

                printl("Before: found->len is %d, process_len is %d\n", found->len, processed_len);
                
                // 将剩余的部分放回队列
                rtp_packet_node_t *remaining = alloc_rtp_node();
                if (remaining) {
                    remaining->seq = next_expected_seq;
                    remaining->len = remaining_len;
                    memcpy(remaining->data, found->data + processed_len, remaining_len);
                    LIST_APPEND(&remaining->list, &ooo_rtp_queue);
                }
                
                free_rtp_node(found);
                break;  // 用户缓冲区已满，退出
            }
            
            free_rtp_node(found);
            continue;  // 继续处理下一个包
        }
        
        // 2. 尝试从接收缓冲区读取包
        uint8_t temp_buffer[RX_PKT_SIZE];
        int pkt_len;
        
        while (1) {
            pkt_len = e1000_poll(temp_buffer);
            if (pkt_len == 0) {
                // 没有数据包，检查是否超时
                if (ever_received && is_timeout()) {
                    printl("Timeout (1)\n");
                    // 发送RSD请求重传
                    send_rsd_packet(next_expected_seq);
                    reset_timer();  // 重置计时器
                }
                
                // 阻塞等待
                do_block(&current_running->list, &recv_block_queue);
                continue;  // 被唤醒后继续尝试接收
            }

            // 3. 只处理RTP协议包
            if (pkt_len < RTP_OFFSET + RTP_HEADER_SIZE) {
                // 包太短，不可能是RTP协议包，丢弃
                printl("Not RTP protocol packet (1)\n");
                continue;
            }
            
            struct rtp_proto_header *header = (struct rtp_proto_header *)(temp_buffer + RTP_OFFSET);

            // 检查是否是RTP协议包
            if (header->magic != RTP_MAGIC) {
                // 不是RTP协议包，丢弃
                printl("Not RTP protocol packet (2), header->magic is %u\n", header->magic);
                continue;
            }

            printl("Got RTP protocol packet!\n");
            ever_received = 1;
            memcpy(tpc_ip_head_buffer, temp_buffer, RTP_OFFSET);
            break;  // 收到RTP包了
        }

        struct rtp_proto_header *header = (struct rtp_proto_header *)(temp_buffer + RTP_OFFSET);
        
        // 解析RTP协议包
        uint16_t data_len = ntohs(header->len);
        uint32_t seq = ntohl(header->seq);
        
        // 检查数据长度是否合理
        if (RTP_OFFSET + RTP_HEADER_SIZE + data_len > pkt_len) {
            // 数据长度不合理，丢弃
            printl("Data length not correct!!!\n");
            continue;
        }
        
        uint8_t *packet_data = temp_buffer + RTP_OFFSET + RTP_HEADER_SIZE;
        
        // 4. 检查包的seq
        if (seq == next_expected_seq) {
            // 这就是我们期待的包！
            int processed_len = process_expected_packet(seq, packet_data, data_len,
                                                         &user_buffer, &delivered_len,
                                                         requested_len);
            int fully_processed = (processed_len == data_len);

            printl("fully_processed: %d\n", fully_processed);
            
            if (!fully_processed) {
                // 包没有完全处理，将剩余部分放入队列
                uint32_t remaining_len = data_len - processed_len;
                
                if (remaining_len > 0) {
                    add_to_ooo_queue(next_expected_seq, 
                                   packet_data + processed_len, 
                                   remaining_len);
                }
                break;  // 用户缓冲区已满
            }
        } 
        else {
            // 不是我们期待的包，放入无序队列
            add_to_ooo_queue(seq, packet_data, data_len);
        }
        
        // 5. 检查超时
        if (is_timeout()) {
            printl("Timeout (2)\n");
            send_rsd_packet(next_expected_seq);
            reset_timer();
        }
    }
    
    // 返回实际接收的字节数
    *nbytes = delivered_len;
    
    // 注意：这里不清理队列，因为可能有后续调用需要队列中的数据

    printl("Exit do_net_recv_stream, delivered_len is %d\n", delivered_len);

    return delivered_len;
}