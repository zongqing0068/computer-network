#include "udp.h"
#include "ip.h"
#include "icmp.h"

/**
 * @brief udp处理程序表
 * 
 */
map_t udp_table;

/**
 * @brief udp伪校验和计算
 * 
 * @param buf 要计算的包
 * @param src_ip 源ip地址
 * @param dst_ip 目的ip地址
 * @return uint16_t 伪校验和
 */
static uint16_t udp_checksum(buf_t *buf, uint8_t *src_ip, uint8_t *dst_ip)
{
    // TO-DO
    // Step1 ：首先调用buf_add_header()函数增加UDP伪头部。
    udp_hdr_t *udp_head = (udp_hdr_t *)buf->data;
    buf_add_header(buf, sizeof(udp_peso_hdr_t));

    // Step2 ：将被UDP伪头部覆盖的IP头部拷贝出来，暂存IP头部，以免被覆盖。
    udp_peso_hdr_t* udp_peso_head = (udp_peso_hdr_t*) buf->data;
    udp_peso_hdr_t udp_peso_head_temp = *udp_peso_head;

    // Step3 ：填写UDP伪头部的12字节字段。
    memcpy(udp_peso_head->src_ip, src_ip, NET_IP_LEN);
    memcpy(udp_peso_head->dst_ip, dst_ip, NET_IP_LEN);
    udp_peso_head->placeholder = 0;
    udp_peso_head->protocol = NET_PROTOCOL_UDP;
    udp_peso_head->total_len16 = udp_head->total_len16;
    int pad = 0;
    if(buf->len % 2) {
        //若数据长度为奇数，则需要填充一个字节
        pad = 1;
        buf_add_padding(buf, 1);
    }

    // Step4 ：计算UDP校验和。
    uint16_t checksum = checksum16((uint16_t *)buf->data, buf->len);
    // 算完校验和后再将pad去掉
    if(pad) buf_remove_padding(buf, 1);

    // Step5 ：再将 Step2 中暂存的IP头部拷贝回来。
    *udp_peso_head = udp_peso_head_temp;

    // Step6 ：调用buf_remove_header()函数去掉UDP伪头部。
    buf_remove_header(buf, sizeof(udp_peso_hdr_t));

    // Step7 ：返回计算出来的校验和值。
    return checksum;
}

/**
 * @brief 处理一个收到的udp数据包
 * 
 * @param buf 要处理的包
 * @param src_ip 源ip地址
 */
void udp_in(buf_t *buf, uint8_t *src_ip)
{
    // TO-DO
    // Step1 ：首先做包检查，检测该数据报的长度是否小于UDP首部长度
    if(buf->len < sizeof(udp_hdr_t)) return;
    // 接收到的包长度是否小于UDP首部长度字段给出的长度
    udp_hdr_t* udp_head = (udp_hdr_t*) buf->data;
    if(swap16(udp_head->total_len16) < sizeof(udp_hdr_t)) return;

    // Step2 ：接着重新计算校验和，先把首部的校验和字段保存起来，
    // 然后把该字段填充0，调用udp_checksum()函数计算出校验和，
    // 如果该值与接收到的UDP数据报的校验和不一致，则丢弃不处理。
    uint16_t old_checksum = udp_head->checksum16;
    udp_head->checksum16 = 0;
    uint16_t new_checksum = udp_checksum(buf, src_ip, net_if_ip);
    if(new_checksum != old_checksum) return;
    udp_head->checksum16 = old_checksum;

    // Step3 ：调用map_get()函数查询udp_table是否有
    // 该目的端口号对应的处理函数（回调函数）。
    uint16_t dst_port16 = swap16(udp_head->dst_port16);
    udp_handler_t* handler = map_get(&udp_table, &dst_port16);

    if(!handler) {
        // Step4 ：如果没有找到，则调用buf_add_header()函数增加IPv4数据报头部，
        // 再调用icmp_unreachable()函数发送一个端口不可达的ICMP差错报文。
        buf_add_header(buf, sizeof(ip_hdr_t));
        icmp_unreachable(buf, src_ip, ICMP_CODE_PORT_UNREACH);
    } else {
        // Step5 ：如果能找到，则去掉UDP报头，调用处理函数来做相应处理。
        buf_remove_header(buf, sizeof(udp_hdr_t));
        (*handler)(buf->data, buf->len, src_ip, dst_port16);
    }
}

/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的包
 * @param src_port 源端口号
 * @param dst_ip 目的ip地址
 * @param dst_port 目的端口号
 */
void udp_out(buf_t *buf, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port)
{
    // TO-DO
    // Step1 ：首先调用buf_add_header()函数添加UDP报头。
    buf_add_header(buf, sizeof(udp_hdr_t));

    // Step2 ：接着，填充UDP首部字段。
    udp_hdr_t *udp_head = (udp_hdr_t *)buf->data;
    udp_head->src_port16 = swap16(src_port);
    udp_head->dst_port16 = swap16(dst_port);
    udp_head->total_len16 = swap16(buf->len);

    // Step3 ：先将校验和字段填充0，然后调用udp_checksum()函数计算出校验和，
    // 再将计算出来的校验和结果填入校验和字段。
    udp_head->checksum16 = 0;
    udp_head->checksum16 = udp_checksum(buf, net_if_ip, dst_ip);

    // Step4 ：调用ip_out()函数发送UDP数据报。
    ip_out(buf, dst_ip, NET_PROTOCOL_UDP);
}

/**
 * @brief 初始化udp协议
 * 
 */
void udp_init()
{
    map_init(&udp_table, sizeof(uint16_t), sizeof(udp_handler_t), 0, 0, NULL);
    net_add_protocol(NET_PROTOCOL_UDP, udp_in);
}

/**
 * @brief 打开一个udp端口并注册处理程序
 * 
 * @param port 端口号
 * @param handler 处理程序
 * @return int 成功为0，失败为-1
 */
int udp_open(uint16_t port, udp_handler_t handler)
{
    return map_set(&udp_table, &port, &handler);
}

/**
 * @brief 关闭一个udp端口
 * 
 * @param port 端口号
 */
void udp_close(uint16_t port)
{
    map_delete(&udp_table, &port);
}

/**
 * @brief 发送一个udp包
 * 
 * @param data 要发送的数据
 * @param len 数据长度
 * @param src_port 源端口号
 * @param dst_ip 目的ip地址
 * @param dst_port 目的端口号
 */
void udp_send(uint8_t *data, uint16_t len, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port)
{
    buf_init(&txbuf, len);
    memcpy(txbuf.data, data, len);
    udp_out(&txbuf, src_port, dst_ip, dst_port);
}