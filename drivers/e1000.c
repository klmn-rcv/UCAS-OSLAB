#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <os/net.h>
#include <os/sched.h>
#include <assert.h>
#include <pgtable.h>

extern void clear_wait_queue(list_head *queue);

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static volatile struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    // 清空发送描述符
    memset(tx_desc_array, 0, sizeof(tx_desc_array));
    for (int i = 0; i < TXDESCS; i++) {
        // 设置缓冲区物理地址（注意：DMA需要物理地址）
        // 这里先设置虚拟地址，稍后需要转换为物理地址
        assert(&tx_pkt_buffer[i][0] >= 0xffffffc050000000lu && &tx_pkt_buffer[i][0] <= 0xffffffc060000000lu);
        tx_desc_array[i].addr = (uint64_t)kva2pa((uintptr_t)&tx_pkt_buffer[i][0]);
        
        // 其他字段初始化为0，在发送时设置
        tx_desc_array[i].length = 0;
        tx_desc_array[i].cso = 0;
        tx_desc_array[i].cmd = E1000_TXD_CMD_RS;
        tx_desc_array[i].status = E1000_TXD_STAT_DD;
        tx_desc_array[i].css = 0;
        tx_desc_array[i].special = 0;
    }

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    // 获取描述符数组的物理地址
    uint64_t tx_desc_phys = kva2pa((uintptr_t)tx_desc_array);
    // 设置低32位地址
    e1000_write_reg(e1000, E1000_TDBAL, (uint32_t)(tx_desc_phys & 0xFFFFFFFF));
    // 设置高32位地址（对于64位系统）
    e1000_write_reg(e1000, E1000_TDBAH, (uint32_t)((tx_desc_phys >> 32) & 0xFFFFFFFF));

    uint32_t tx_desc_len = TXDESCS * sizeof(struct e1000_tx_desc);
    e1000_write_reg(e1000, E1000_TDLEN, tx_desc_len);

	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);  // 头指针指向第一个描述符
    e1000_write_reg(e1000, E1000_TDT, 0);  // 尾指针也指向第一个描述符

    /* TODO: [p5-task1] Program the Transmit Control Register */
    uint32_t tctl = 0;
    tctl |= E1000_TCTL_EN;
    tctl |= E1000_TCTL_PSP;
    tctl |= (E1000_TCTL_CT & (0x10 << 4));
    tctl |= (E1000_TCTL_COLD & (0x40 << 12));
    e1000_write_reg(e1000, E1000_TCTL, tctl);

    local_flush_dcache();
    
    // printk("[E1000] TX configuration completed. Descriptors: %d, Buffer size: %d bytes\n", 
    //        TXDESCS, TX_PKT_SIZE);
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    uint32_t rar_low = enetaddr[0] | (enetaddr[1] << 8) | (enetaddr[2] << 16) | (enetaddr[3] << 24);
    uint32_t rar_high = enetaddr[4] | (enetaddr[5] << 8);
    e1000_write_reg_array(e1000, E1000_RA, 0, rar_low);
    e1000_write_reg_array(e1000, E1000_RA, 1, rar_high | E1000_RAH_AV); // 设置有效 bit

    /* TODO: [p5-task2] Initialize rx descriptors */
    // 清空接收描述符
    memset(rx_desc_array, 0, sizeof(rx_desc_array));
    for (int i = 0; i < RXDESCS; i++) {
        // 设置缓冲区物理地址（注意：DMA需要物理地址）
        assert(&rx_pkt_buffer[i][0] >= 0xffffffc050000000lu && &rx_pkt_buffer[i][0] <= 0xffffffc060000000lu);
        rx_desc_array[i].addr = (uint64_t)kva2pa((uintptr_t)&rx_pkt_buffer[i][0]);
        rx_desc_array[i].length = 0;
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].status = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].special = 0;
    }

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    // 获取描述符数组的物理地址
    uint64_t rx_desc_phys = kva2pa((uintptr_t)rx_desc_array);
    // 设置低32位地址
    e1000_write_reg(e1000, E1000_RDBAL, (uint32_t)(rx_desc_phys & 0xFFFFFFFF));
    // 设置高32位地址（对于64位系统）
    e1000_write_reg(e1000, E1000_RDBAH, (uint32_t)(rx_desc_phys >> 32));

    uint32_t rx_desc_len = RXDESCS * sizeof(struct e1000_rx_desc);
    e1000_write_reg(e1000, E1000_RDLEN, rx_desc_len);

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);  // 头指针指向第一个描述符
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);  // 尾指针指向最后一个描述符

    /* TODO: [p5-task2] Program the Receive Control Register */
    uint32_t rctl = 0;
    rctl |= E1000_RCTL_EN;  // 启用接收
    rctl |= E1000_RCTL_BAM; // 启用广播接收
    rctl |= E1000_RCTL_SZ_2048; // 设置接收缓冲区大小为2048字节
    e1000_write_reg(e1000, E1000_RCTL, rctl);

    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */

    
    local_flush_dcache();
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();

    // e1000_write_reg(e1000, E1000_RDTR, 0);     // 配置包定时器
	// e1000_write_reg(e1000, E1000_RADV, 0);     // 配置绝对定时器
    // e1000_write_reg(e1000, E1000_RDTR, 0x12345678);
    // e1000_write_reg(e1000, E1000_RADV, 0x87654321);
    // printl("write RDTR and RADV finish\n");
    // uint32_t rdtr = e1000_read_reg(e1000, E1000_RDTR);
    // uint32_t radv = e1000_read_reg(e1000, E1000_RADV);
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE | E1000_IMS_RXDMT0 | E1000_IMS_RXT0);
    local_flush_dcache();
    // printl("After init: RDTR = %u, RADV = %u\n", rdtr, radv);
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    local_flush_dcache();

    if (!txpacket || length <= 0) {
        assert(0);
    }

    uint32_t tail = e1000_read_reg(e1000, E1000_TDT);

    struct e1000_tx_desc *tx_desc_tail;

    // struct e1000_tx_desc *tx_desc_tail = &tx_desc_array[tail];

    // assert(tx_desc_tail->cmd & E1000_TXD_CMD_RS);

    // while (!(tx_desc_tail->status & E1000_TXD_STAT_DD)) {
    //     do_block(&current_running->list, &send_block_queue);
    // }

    int total_len = 0;

    while(1) {
        tx_desc_tail = &tx_desc_array[tail];
        assert(tx_desc_tail->cmd & E1000_TXD_CMD_RS);
        while (!(tx_desc_tail->status & E1000_TXD_STAT_DD)) {
            do_block(&current_running->list, &send_block_queue);
        }
        
        tx_desc_tail->length = (length > TX_PKT_SIZE ? TX_PKT_SIZE : length);
        local_flush_dcache();
        memcpy((uint8_t *)tx_pkt_buffer[tail], (uint8_t *)txpacket, tx_desc_tail->length);
        tx_desc_tail->status = 0;
        

        tx_desc_tail->cmd |= E1000_TXD_CMD_RS;
        if(tx_desc_tail->length == length) {
            tx_desc_tail->cmd |= E1000_TXD_CMD_EOP;
            e1000_write_reg(e1000, E1000_TDT, (tail + 1) % TXDESCS);
            total_len += tx_desc_tail->length;
            break;
        }
        
        e1000_write_reg(e1000, E1000_TDT, (tail + 1) % TXDESCS);
        total_len += tx_desc_tail->length;
        tail = (tail + 1) % TXDESCS;
        length -= tx_desc_tail->length;
        txpacket = (void *)((uintptr_t)txpacket + tx_desc_tail->length);
        local_flush_dcache();
    }    
    local_flush_dcache();
    return total_len;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    local_flush_dcache();
    if (!rxbuffer) {
        assert(0);
    }
    uint32_t next_tail = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;
    struct e1000_rx_desc *rx_desc_tail;
    // struct e1000_rx_desc *rx_desc_tail = &rx_desc_array[next_tail];

    // // printl("rx_desc_tail->status: %d\n", rx_desc_tail->status);

    // while (!(rx_desc_tail->status & E1000_RXD_STAT_DD)) {
    //     do_block(&current_running->list, &recv_block_queue);
    // }
    int total_len = 0;

    while(1) {
        rx_desc_tail = &rx_desc_array[next_tail];
        while (!(rx_desc_tail->status & E1000_RXD_STAT_DD)) {
            do_block(&current_running->list, &recv_block_queue);
        }

        uint16_t pkt_len = rx_desc_tail->length;
        local_flush_dcache();
        memcpy((uint8_t *)rxbuffer, (uint8_t *)rx_pkt_buffer[next_tail], pkt_len);
        total_len += pkt_len;

        if(rx_desc_tail->status & E1000_RXD_STAT_EOP) {
            rx_desc_tail->status = 0;
            e1000_write_reg(e1000, E1000_RDT, next_tail);
            break;
        }

        rxbuffer = (void *)((uintptr_t)rxbuffer + pkt_len);
        rx_desc_tail->status = 0;
        e1000_write_reg(e1000, E1000_RDT, next_tail);
        next_tail = (next_tail + 1) % RXDESCS;
        local_flush_dcache();
    }

    // uint16_t pkt_len = rx_desc_tail->length;
    // memcpy((uint8_t *)rxbuffer, (uint8_t *)rx_pkt_buffer[next_tail], pkt_len);
    // rx_desc_tail->status = 0;
    // e1000_write_reg(e1000, E1000_RDT, next_tail);

    local_flush_dcache();
    return total_len;
}

int e1000_poll_for_stream(void *rxbuffer)
{
    local_flush_dcache();
    if (!rxbuffer) {
        assert(0);
    }
    uint32_t next_tail = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;
    struct e1000_rx_desc *rx_desc_tail;
    int total_len = 0;
    while(1) {
        rx_desc_tail = &rx_desc_array[next_tail];
        if (!(rx_desc_tail->status & E1000_RXD_STAT_DD)) {
            return 0;   // no packet available now
        }

        uint16_t pkt_len = rx_desc_tail->length;
        local_flush_dcache();
        memcpy((uint8_t *)rxbuffer, (uint8_t *)rx_pkt_buffer[next_tail], pkt_len);
        total_len += pkt_len;

        if(rx_desc_tail->status & E1000_RXD_STAT_EOP) {
            rx_desc_tail->status = 0;
            e1000_write_reg(e1000, E1000_RDT, next_tail);
            break;
        }

        rxbuffer = (void *)((uintptr_t)rxbuffer + pkt_len);
        rx_desc_tail->status = 0;
        e1000_write_reg(e1000, E1000_RDT, next_tail);
        next_tail = (next_tail + 1) % RXDESCS;
        local_flush_dcache();
    }

    local_flush_dcache();
    return total_len;
}

void e1000_handle_txqe() {
    clear_wait_queue(&send_block_queue);
}

void e1000_handle_rxdmt0() {
    clear_wait_queue(&recv_block_queue);
    clear_wait_queue(&recv_stream_block_queue);
}