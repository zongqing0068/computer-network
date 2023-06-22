#include "ethernet.h"
#include "utils.h"
#include "driver.h"
#include "arp.h"
#include "ip.h"
/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 */
void ethernet_in(buf_t *buf)
{
    // TO-DO
    // Step1 ：首先判断数据长度，如果数据长度小于以太网头部长度，则认为数据包不完整，丢弃不处理。
    if(buf->len < sizeof(ether_hdr_t)) return;

    // Step2 ：调用buf_remove_header()函数移除以太网包头。(在移除以太网包头前获取包的数据起始地址)
    ether_hdr_t *hdr = (ether_hdr_t *)buf->data;
    buf_remove_header(buf, sizeof(ether_hdr_t));

    // Step3 ：填写协议类型 protocol（大小端转换），调用net_in()函数向上层传递数据包。
    net_protocol_t protocol = swap16(hdr->protocol16);
    net_in(buf, protocol, hdr->src);

}
/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的数据包
 * @param mac 目标MAC地址
 * @param protocol 上层协议
 */
void ethernet_out(buf_t *buf, const uint8_t *mac, net_protocol_t protocol)
{
    // TO-DO
    // Step1 ：首先判断数据长度，如果不足46则显式填充0，填充可以调用buf_add_padding()函数来实现。
    if(buf->len < ETHERNET_MIN_TRANSPORT_UNIT)
        buf_add_padding(buf, 46 - buf->len);
    
    // Step2 ：调用buf_add_header()函数添加以太网包头。
    buf_add_header(buf, sizeof(ether_hdr_t));
    ether_hdr_t *hdr = (ether_hdr_t *)buf->data;

    // Step3 ：填写目的MAC地址。
    memcpy(hdr->dst, mac, NET_MAC_LEN);

    // Step4 ：填写源MAC地址，即本机的MAC地址。
    memcpy(hdr->src, net_if_mac, NET_MAC_LEN);

    // Step5 ：填写协议类型 protocol。（大小端转换）
    hdr->protocol16 = swap16(protocol);

    // Step6 ：调用驱动层封装好的driver_send()发送函数，将添加了以太网包头的数据帧发送到驱动层。
    driver_send(buf);

}
/**
 * @brief 初始化以太网协议
 * 
 */
void ethernet_init()
{
    buf_init(&rxbuf, ETHERNET_MAX_TRANSPORT_UNIT + sizeof(ether_hdr_t));
}

/**
 * @brief 一次以太网轮询
 * 
 */
void ethernet_poll()
{
    if (driver_recv(&rxbuf) > 0)
        ethernet_in(&rxbuf);
}
