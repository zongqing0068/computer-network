#include "net.h"
#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac)
{
    // TO-DO
    // Step1 ：如果数据包的长度小于IP头部长度，丢弃不处理。
    if(buf->len < sizeof(ip_hdr_t)) return;

    // Step2 ：接下来做报头检测，如果不符合这些要求，则丢弃不处理。
    ip_hdr_t* ip_head = (ip_hdr_t*)buf->data;
    // 检查IP头部的版本号是否为IPv4
    if(ip_head->version != IP_VERSION_4) return;
    // 检查总长度字段小于或等于收到的包的长度
    uint16_t total_len16 = swap16(ip_head->total_len16);
    if(total_len16 > buf->len) return;

    // Step3 ：先把IP头部的头部校验和字段用其他变量保存起来，
    // 接着将该头部校验和字段置0，然后调用checksum16函数来计算头部校验和，
    // 如果与IP头部的首部校验和字段不一致，丢弃不处理，
    // 如果一致，则再将该头部校验和字段恢复成原来的值。
    uint16_t old_checksum16 = ip_head->hdr_checksum16;
    ip_head->hdr_checksum16 = 0;
    uint16_t new_checksum16 = checksum16((uint16_t *)ip_head, sizeof(ip_hdr_t));
    if(new_checksum16 != old_checksum16) return;
    ip_head->hdr_checksum16 = old_checksum16;

    // Step4 ：对比目的IP地址是否为本机的IP地址，如果不是，则丢弃不处理。
    if(memcmp(ip_head->dst_ip,net_if_ip,4) != 0) return;

    // Step5 ：如果接收到的数据包的长度大于IP头部的总长度字段，则说明该数据包有填充字段，
    // 可调用buf_remove_padding()函数去除填充字段。
    if(buf->len > total_len16)
        buf_remove_padding(buf, buf->len - total_len16);

    // Step6 ：调用buf_remove_header()函数去掉IP报头。
    buf_remove_header(buf, sizeof(ip_hdr_t));

    // Step7 ：调用net_in()函数向上层传递数据包。如果是不能识别的协议类型，
    // 即调用icmp_unreachable()返回ICMP协议不可达信息。
    if(net_in(buf, ip_head->protocol, ip_head->src_ip) == -1){
        buf_add_header(buf, sizeof(ip_hdr_t));
        memcpy(buf->data, ip_head, sizeof(ip_hdr_t));
        icmp_unreachable(buf, ip_head->src_ip, ICMP_CODE_PROTOCOL_UNREACH);
    }
    return;
}

/**
 * @brief 处理一个要发送的ip分片
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TO-DO
    // Step1 ：调用buf_add_header()增加IP数据报头部缓存空间。
    buf_add_header(buf, sizeof(ip_hdr_t));

    // Step2 ：填写IP数据报头部字段。
    ip_hdr_t* ip_head = (ip_hdr_t*) buf->data;
    ip_head->hdr_len = 5;
    ip_head->version = IP_VERSION_4;
    ip_head->tos = 0;
    ip_head->total_len16 = swap16((uint16_t)buf->len);
    ip_head->id16 = swap16((uint16_t)id);
    ip_head->flags_fragment16 = swap16(((uint16_t)mf << 13) | offset);
    ip_head->ttl = 64;
    ip_head->protocol = protocol;
    memcpy(ip_head->dst_ip, ip, NET_IP_LEN);
    memcpy(ip_head->src_ip, net_if_ip, NET_IP_LEN);

    // Step3 ：先把IP头部的首部校验和字段填0，
    // 再调用checksum16函数计算校验和，然后把计算出来的校验和填入首部校验和字段。
    ip_head->hdr_checksum16 = 0;
    ip_head->hdr_checksum16 = checksum16((uint16_t*)ip_head, sizeof(ip_hdr_t));

    // Step4 ：调用arp_out函数()将封装后的IP头部和数据发送出去。
    arp_out(buf, ip);
    return;
}

/**
 * @brief 处理一个要发送的ip数据包
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
static int id = 0;
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TO-DO
    // Step1 ：首先检查从上层传递下来的数据报包长是否大于
    // IP协议最大负载包长（1500字节（MTU） 减去IP首部长度）。
    int max_data_len = (ETHERNET_MAX_TRANSPORT_UNIT - sizeof(ip_hdr_t)) / 8 * 8;
    int offset_unit = max_data_len / 8;
    if(buf->len > max_data_len){
        // Step2 ：如果超过IP协议最大负载包长，则需要分片发送。
        // 计算分片数量（向上取整）
        size_t n = (buf->len - 1) / max_data_len + 1;
        // 计算最后一个分片的数据包长度
        size_t last_data_len = buf->len % max_data_len;
        if(last_data_len == 0) last_data_len = max_data_len;

        buf_t ip_buf;
        for(int i = 0; i < n - 1; i++){
            // 首先调用buf_init()初始化一个ip_buf,将数据报包长截断
            buf_init(&ip_buf, max_data_len);
            memcpy(ip_buf.data, buf->data + i * max_data_len, max_data_len);
            // 调用ip_fragment_out()函数发送出去
            ip_fragment_out(&ip_buf, ip, protocol, id, i * offset_unit, 1);
        }
        // 最后一个分片
        // 调用buf_init()初始化一个ip_buf，大小等于该分片大小
        buf_init(&ip_buf, last_data_len);
        memcpy(ip_buf.data, buf->data + (n - 1) * max_data_len, last_data_len);
        // 调用ip_fragment_out()函数发送出去，最后一个分片的MF = 0
        ip_fragment_out(&ip_buf, ip, protocol, id, (n - 1) * offset_unit, 0);
    }
    
    // Step3 ：如果没有超过IP协议最大负载包长，则直接调用ip_fragment_out()函数发送出去。
    else{
        ip_fragment_out(buf, ip, protocol, id, 0, 0);
    }

    id ++;
    return;
}

/**
 * @brief 初始化ip协议
 * 
 */
void ip_init()
{
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}