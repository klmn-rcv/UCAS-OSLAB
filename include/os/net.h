#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

#include <os/list.h>
#include <type.h>

#define PKT_NUM 32

#define ETH_ALEN 6u                 // Length of MAC address
#define ETH_P_IP 0x0800u            // IP protocol
// Ethernet header
struct ethhdr {
    uint8_t ether_dmac[ETH_ALEN];   // destination mac address
    uint8_t ether_smac[ETH_ALEN];   // source mac address
    uint16_t ether_type;            // protocol format
};

extern list_head send_block_queue;
extern list_head recv_block_queue;

#define RTP_OFFSET 54
#define RTP_MAGIC 0x45
#define RTP_HEADER_SIZE 8
#define RTP_FLAGS_DAT 0x01
#define RTP_FLAGS_RSD 0x02
#define RTP_FLAGS_ACK 0x04
#define RETRANS_TIMEOUT 1000000

// 静态节点池
#define MAX_RTP_NODES 64  // 可以适当增大，因为现在只处理RTP包

// 无序RTP协议包结构
typedef struct rtp_packet_node {
    list_node_t list;           // 链表节点
    uint32_t seq;               // 序号
    uint16_t len;               // 数据长度
    uint8_t data[2048];  // 数据缓冲区
} rtp_packet_node_t;

struct rtp_proto_header {
    uint8_t magic;      // Magic number to identify RTP protocol
    uint8_t flags;      // Flags
    uint16_t len;      // Length of the data
    uint32_t seq;      // Sequence number
};

void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);
int do_net_recv_stream(void *buffer, int *nbytes);

void init_free_rtp_nodes();

#endif  // __INCLUDE_NET_H__
