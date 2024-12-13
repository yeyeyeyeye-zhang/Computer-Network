#ifndef DATAGRAM_H
#define DATAGRAM_H
#include <cstdint>
#include <cstring>

#define BUFFER_SIZE 1024
#define SERVER_PORT 9991
#define CLIENT_PORT 9990
#define ROUTER_PORT 9992
#define TIMEOUT 100
#define WINDOW_SIZE 8
// 伪首部用于UDP校验和计算－重新看看
struct PseudoHeader {
    uint32_t srcAddr;//源IP地址（32位）
    uint32_t destAddr;//目的IP地址（32位）
    uint8_t zero;//0（8位）
    uint8_t protocol;//协议号，UDP为17（8位）
    uint16_t udpLength;//UDP数据报长度（16位）
};


//数据报结构体
struct Datagram
{
    uint16_t src_port;//源端口号
    uint16_t dst_port;//目的端口号
    uint16_t seq;//序列号
    uint16_t ack;//确认号
    uint16_t dataSize;//当前数据大小
    uint16_t checksum;//校验和
    uint16_t flag;           // 标志位：0-数据，1-SYN，2-SYN-ACK，3-ACK，4-FIN，5-FIN-ACK
    char data[BUFFER_SIZE];//数据
    Datagram(int src_port_,int dst_port_);
    Datagram();//默认构造从客户端发往路由器
    //计算校验和-此处先简单求和,后续再升级修改
    int calChecksum(uint32_t srcAddr, uint32_t destAddr);
    bool validateChecksum(uint32_t srcAddr, uint32_t destAddr);
};

#endif // DATAGRAM_H
